#include "iap/config.h"
#include "iap/crc32.h"
#include "iap/image.h"
#include "iap/updater.h"
#include "mock_flash.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static mock_flash_t flash_mem;
static unsigned pass_count=0;
#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);return 1;}}while(0)
static void fixture(uint8_t *image, size_t n){
    uint32_t msp=IAP_SRAM_END, reset=IAP_APP_BASE+0x21u;
    size_t i;
    for(i=0;i<n;++i) image[i]=(uint8_t)((i*73u+11u)&0xFFu);
    memcpy(image,&msp,4u);
    memcpy(image+4,&reset,4u);
    mock_flash_reset(&flash_mem);
}
static int program_image(const uint8_t *image,size_t n, uint32_t expected_crc) {
    iap_updater_t up;size_t p=0;
    iap_updater_init(&up,mock_flash_ops(&flash_mem));
    CHECK(iap_updater_begin(&up,(uint32_t)n,expected_crc,0x010200u)==IAP_OK);
    while(p<n){size_t k=(n-p)<128u?(n-p):128u;
       CHECK(iap_updater_write(&up,(uint32_t)p,image+p,k)==IAP_OK);p+=k;}
    CHECK(iap_updater_finish(&up)==IAP_OK);
    return 0;
}
static int test_empty_boot(void){
    mock_flash_reset(&flash_mem);
    CHECK(iap_boot_select(&(iap_flash_t){0})==IAP_BOOT_RECOVERY);
    iap_flash_t f=mock_flash_ops(&flash_mem);
    CHECK(iap_boot_select(&f)==IAP_BOOT_RECOVERY);
    return 0;
}
static int test_crc_known(void){
    CHECK(iap_crc32("123456789",9u)==0xCBF43926u);
    return 0;
}
static int test_success(void){
    uint8_t image[257];iap_flash_t f; iap_image_record_t rec;
    fixture(image,sizeof(image));
    CHECK(program_image(image,sizeof(image),iap_crc32(image,sizeof(image)))==0);
    f=mock_flash_ops(&flash_mem);
    CHECK(iap_boot_select(&f)==IAP_BOOT_APP);
    CHECK(iap_image_validate(&f,&rec));
    CHECK(rec.image_size==sizeof(image) && rec.version==0x010200u);
    CHECK(flash_mem.erase_calls==2u); /* metadata + first APP sector */
    return 0;
}
static int test_interrupted(void){
    uint8_t image[256];iap_updater_t up; iap_flash_t f;
    fixture(image,sizeof(image));f=mock_flash_ops(&flash_mem);
    iap_updater_init(&up,f);
    CHECK(iap_updater_begin(&up,sizeof(image),iap_crc32(image,sizeof(image)),1u)==IAP_OK);
    CHECK(iap_updater_write(&up,0u,image,128u)==IAP_OK);
    /* Power loss modeled by discarding RAM state, NOT a real electrical test. */
    CHECK(iap_boot_select(&f)==IAP_BOOT_RECOVERY);
    return 0;
}
static int test_crc_mismatch(void){
    uint8_t image[256];iap_updater_t up; iap_flash_t f;
    fixture(image,sizeof(image));f=mock_flash_ops(&flash_mem);
    iap_updater_init(&up,f);
    CHECK(iap_updater_begin(&up,sizeof(image),0x12345678u,1u)==IAP_OK);
    CHECK(iap_updater_write(&up,0u,image,256u)==IAP_OK);
    CHECK(iap_updater_finish(&up)==IAP_ERR_CRC);
    CHECK(iap_boot_select(&f)==IAP_BOOT_RECOVERY);
    return 0;
}
static int test_corrupt_existing_image(void){
    uint8_t image[256];iap_flash_t f;
    fixture(image,sizeof(image));
    CHECK(program_image(image,sizeof(image),iap_crc32(image,sizeof(image)))==0);
    flash_mem.data[IAP_APP_BASE-IAP_FLASH_BASE+100u]^=1u; /* test-only fault injection */
    f=mock_flash_ops(&flash_mem);
    CHECK(iap_boot_select(&f)==IAP_BOOT_RECOVERY);
    return 0;
}
static int test_wrong_offset(void){
    uint8_t image[256];iap_updater_t up;
    fixture(image,sizeof(image));
    iap_updater_init(&up,mock_flash_ops(&flash_mem));
    CHECK(iap_updater_begin(&up,sizeof(image),iap_crc32(image,sizeof(image)),1u)==IAP_OK);
    CHECK(iap_updater_write(&up,4u,image+4u,128u)==IAP_ERR_ORDER);
    CHECK(up.written==0u);
    return 0;
}
static int test_commit_failure(void){
    uint8_t image[256];iap_updater_t up;iap_flash_t f;
    fixture(image,sizeof(image));f=mock_flash_ops(&flash_mem);
    iap_updater_init(&up,f);
    CHECK(iap_updater_begin(&up,sizeof(image),iap_crc32(image,sizeof(image)),1u)==IAP_OK);
    CHECK(iap_updater_write(&up,0u,image,256u)==IAP_OK);
    flash_mem.fail_program_addr=IAP_META_BASE+(uint32_t)offsetof(iap_image_record_t,magic);
    CHECK(iap_updater_finish(&up)==IAP_ERR_FLASH);
    CHECK(iap_boot_select(&f)==IAP_BOOT_RECOVERY);
    return 0;
}
static int test_bad_reset_vector(void){
    uint8_t image[256];iap_flash_t f;uint32_t bad_reset=0x08000201u;
    fixture(image,sizeof(image));memcpy(image+4,&bad_reset,4u);
    CHECK(program_image(image,sizeof(image),iap_crc32(image,sizeof(image)))==0);
    f=mock_flash_ops(&flash_mem);
    CHECK(iap_boot_select(&f)==IAP_BOOT_RECOVERY);
    return 0;
}
static int run(const char *name,int (*test)(void)) {
    if(test()!=0){fprintf(stderr,"TEST FAILED: %s\n",name);return 1;}
    ++pass_count;printf("PASS %-28s\n",name);return 0;
}
int main(void){
    if(run("empty boot => recovery",test_empty_boot) ||
       run("CRC32 known vector",test_crc_known) ||
       run("valid image => app",test_success) ||
       run("interrupted download",test_interrupted) ||
       run("wrong CRC",test_crc_mismatch) ||
       run("corrupted committed image",test_corrupt_existing_image) ||
       run("out-of-order block",test_wrong_offset) ||
       run("interrupted commit marker",test_commit_failure) ||
       run("invalid reset vector",test_bad_reset_vector)) return 1;
    printf("%u/9 tests passed (host model only)\n",pass_count);
    return 0;
}
