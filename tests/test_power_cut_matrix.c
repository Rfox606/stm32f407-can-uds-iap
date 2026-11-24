/* Deterministic interrupted Flash model. NOT a physical power-loss experiment. */
#include "iap/config.h"
#include "iap/crc32.h"
#include "iap/image.h"
#include "iap/updater.h"
#include "mock_flash.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static mock_flash_t mem;
static uint8_t old_image[512], new_image[512];
typedef struct {
    iap_flash_t real;
    unsigned count, cut;
    int partial;
} fault_t;
static int read_fault(void *ctx,uint32_t addr,void *out,size_t n){
    fault_t *f=ctx;return f->real.read(f->real.ctx,addr,out,n);
}
static int erase_fault(void *ctx,unsigned sector){
    fault_t *f=ctx;
    if(++f->count==f->cut && f->partial){
        /* Illustrative partially erased prefix; not a prediction of silicon. */
        size_t start=sector==4u?IAP_META_BASE-IAP_FLASH_BASE:IAP_APP_BASE-IAP_FLASH_BASE;
        size_t n=sector==4u?IAP_META_BYTES:0x20000u;
        memset(mem.data+start,0xFF,n/2);
        return -1;
    }
    int rc=f->real.erase_sector(f->real.ctx,sector);
    return f->count==f->cut?-1:rc;
}
static int program_fault(void *ctx,uint32_t addr,const void *data,size_t n){
    fault_t *f=ctx;
    if(++f->count==f->cut && f->partial){
        /* Model a prefix with only 1->0 changes, including a torn commit word. */
        const uint8_t *p=data;
        for(size_t i=0;i<n/2;i++) mem.data[addr-IAP_FLASH_BASE+i]&=p[i];
        return -1;
    }
    int rc=f->real.program(f->real.ctx,addr,data,n);
    return f->count==f->cut?-1:rc;
}
static void make_image(uint8_t *p,unsigned seed){
    uint32_t msp=0x2001FFC0u,entry=IAP_APP_BASE+0x21u;
    for(size_t i=0;i<512u;i++) p[i]=(uint8_t)(i*73u+seed);
    memcpy(p,&msp,4);memcpy(p+4,&entry,4);
}
static iap_result_t update(iap_flash_t f,const uint8_t *p,uint32_t version){
    iap_updater_t u;
    iap_updater_init(&u,f);
    iap_result_t rc=iap_updater_begin(&u,512u,iap_crc32(p,512u),version);
    if(rc!=IAP_OK)return rc;
    rc=iap_updater_write(&u,0u,p,256u);
    if(rc!=IAP_OK)return rc;
    rc=iap_updater_write(&u,256u,p+256u,256u);
    if(rc!=IAP_OK)return rc;
    return iap_updater_finish(&u);
}
static int run(unsigned cut,int partial){
    mock_flash_reset(&mem);
    memset(mem.data,0xA5,IAP_BOOT_BYTES);
    iap_flash_t clean=mock_flash_ops(&mem);
    CHECK(update(clean,old_image,1u)==IAP_OK);
    fault_t f={clean,0u,cut,partial};
    iap_flash_t hooked={&f,read_fault,erase_fault,program_fault};
    if(cut!=0u) CHECK(update(hooked,new_image,2u)==IAP_ERR_FLASH);
    /* Reboot: discard transaction RAM, evaluate persisted Flash only. */
    iap_image_record_t rec;
    iap_boot_choice_t choice=iap_boot_select(&clean);
    if(cut==0u || (!partial && cut==6u)){
        CHECK(choice==IAP_BOOT_APP);
        CHECK(iap_image_validate(&clean,&rec));
        CHECK(rec.version==(cut==0u?1u:2u));
    }else CHECK(choice==IAP_BOOT_RECOVERY);
    for(size_t i=0;i<IAP_BOOT_BYTES;i++)CHECK(mem.data[i]==0xA5);
    printf("cut=%u %s => %s; boot area intact; ",cut,partial?"mid-operation":"after-operation",
           choice==IAP_BOOT_APP?(cut==0u?"OLD APP":"NEW APP"):"RECOVERY");
    CHECK(update(clean,new_image,2u)==IAP_OK);
    CHECK(iap_image_validate(&clean,&rec) && rec.version==2u);
    puts("reflash PASS");
    return 0;
}
int main(void){
    make_image(old_image,11u);make_image(new_image,23u);
    CHECK(run(0,0)==0);
    for(unsigned i=1;i<=6;i++){CHECK(run(i,0)==0);CHECK(run(i,1)==0);}
    puts("13/13 interrupted Flash scenarios passed (software model only)");
    return 0;
}
