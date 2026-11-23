#include "isotp.h"
#include "uds_boot.h"
#include "iap/config.h"
#include "iap/crc32.h"
#include "iap/image.h"
#include "mock_flash.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
static mock_flash_t flash;
static unsigned passed;
static uint8_t sent[16][8];
static unsigned sent_count;
static int tx(void *ctx,const uint8_t frame[8]) {
    (void)ctx;
    if (sent_count==16u) return -1;
    memcpy(sent[sent_count++],frame,8);
    return 0;
}
static void pack(uint8_t *b,uint32_t v) {
    b[0]=(uint8_t)(v>>24);b[1]=(uint8_t)(v>>16);b[2]=(uint8_t)(v>>8);b[3]=(uint8_t)v;
}
static int iso_receive(void) {
    isotp_t t;const uint8_t *data;size_t n;
    uint8_t ff[8]={0x10,0x0F,1,2,3,4,5,6};
    uint8_t cf1[8]={0x21,7,8,9,10,11,12,13};
    uint8_t cf2[8]={0x22,14,15};
    sent_count=0;isotp_init(&t,tx,0);
    CHECK(isotp_on_can(&t,ff,8,100)==0);
    CHECK(sent_count==1 && sent[0][0]==0x30);
    CHECK(isotp_on_can(&t,cf1,8,101)==0);
    CHECK(isotp_on_can(&t,cf2,8,102)==0);
    CHECK(isotp_peek(&t,&data,&n)==1 && n==15u);
    for(unsigned i=0;i<15;i++) CHECK(data[i]==(uint8_t)(i+1));
    isotp_consume(&t);
    CHECK(!t.rx_ready);
    CHECK(isotp_on_can(&t,ff,8,200)==0);
    cf1[0]=0x22;
    CHECK(isotp_on_can(&t,cf1,8,201)==-1 && t.rx_errors!=0u);
    CHECK(isotp_on_can(&t,ff,8,300)==0);
    isotp_poll(&t,1302);
    CHECK(!t.rx_active);
    return 0;
}
static int iso_send(void) {
    isotp_t t;uint8_t data[30]={0},fc[8]={0x30,2,3};
    for(unsigned i=0;i<30;i++) data[i]=(uint8_t)i;
    sent_count=0;isotp_init(&t,tx,0);
    CHECK(isotp_send(&t,data,sizeof(data),50)==0 && sent_count==1 && sent[0][0]==0x10);
    isotp_poll(&t,51);CHECK(sent_count==1);
    CHECK(isotp_on_can(&t,fc,8,52)==0);
    isotp_poll(&t,52);CHECK(sent_count==2 && sent[1][0]==0x21);
    isotp_poll(&t,53);CHECK(sent_count==2);
    isotp_poll(&t,55);CHECK(sent_count==3 && sent[2][0]==0x22);
    isotp_poll(&t,58);CHECK(sent_count==3); /* block size=2 */
    CHECK(isotp_on_can(&t,fc,8,59)==0);
    isotp_poll(&t,59);CHECK(sent_count==4 && sent[3][0]==0x23);
    isotp_poll(&t,62);CHECK(sent_count==5 && sent[4][0]==0x24);
    CHECK(isotp_tx_idle(&t));
    return 0;
}
static void make_image(uint8_t *b,size_t n) {
    uint32_t sp=IAP_SRAM_END,pc=IAP_APP_BASE+0x21u;
    mock_flash_reset(&flash);
    for(size_t i=0;i<n;i++) b[i]=(uint8_t)(i*11u+7u);
    memcpy(b,&sp,4);memcpy(b+4,&pc,4);
}
static int ask(uds_boot_t *u,const uint8_t *q,size_t n,uint8_t *out,size_t *len) {
    return uds_boot_request(u,q,n,out,len,100u);
}
static int ready_download(uds_boot_t *u) {
    uint8_t q[11]={0x34,0,0x44},out[8];size_t n;
    const uint8_t session[2]={0x10,2};
    CHECK(ask(u,session,2,out,&n)==0 && n==6 && out[0]==0x50);
    pack(q+3,IAP_APP_BASE);pack(q+7,256);
    CHECK(ask(u,q,11,out,&n)==0 && out[0]==0x7F && out[2]==0x78);
    CHECK(uds_boot_erase_step(u,out,&n)==1 && out[2]==0x78);
    CHECK(uds_boot_erase_step(u,out,&n)==1 && out[0]==0x74 && n==4);
    return 0;
}
static int uds_download(void) {
    uds_boot_t u;uint8_t image[256],block[258]={0x36,1},end[9]={0x37},out[8],readq[3]={0x22,0xF1,0x90};
    size_t n;unsigned writes;
    make_image(image,sizeof(image));iap_flash_t f=mock_flash_ops(&flash);
    uds_boot_init(&u,f,100);
    CHECK(ready_download(&u)==0);
    memcpy(block+2,image,256);
    CHECK(ask(&u,block,sizeof(block),out,&n)==0 && n==2 && out[0]==0x76);
    writes=flash.program_calls;
    CHECK(ask(&u,block,sizeof(block),out,&n)==0 && n==2 && flash.program_calls==writes);
    block[42]^=1u;
    CHECK(ask(&u,block,sizeof(block),out,&n)==0 && n==3 && out[2]==0x73);
    block[42]^=1u;
    pack(end+1,iap_crc32(image,sizeof(image)));pack(end+5,0x01020304u);
    CHECK(ask(&u,end,sizeof(end),out,&n)==0 && n==1 && out[0]==0x77);
    CHECK(iap_boot_select(&f)==IAP_BOOT_APP);
    CHECK(ask(&u,readq,sizeof(readq),out,&n)==0 && n==7);
    CHECK(out[3]==1u && out[4]==2u && out[5]==3u && out[6]==4u);
    return 0;
}
static int uds_error(void) {
    uds_boot_t u;uint8_t image[256],block[258]={0x36,2},end[9]={0x37},out[8];size_t n;
    make_image(image,sizeof(image));iap_flash_t f=mock_flash_ops(&flash);
    uds_boot_init(&u,f,100);
    CHECK(ready_download(&u)==0);
    memcpy(block+2,image,256);
    CHECK(ask(&u,block,sizeof(block),out,&n)==0 && n==3 && out[2]==0x73);
    block[1]=1;
    CHECK(ask(&u,block,sizeof(block),out,&n)==0 && n==2);
    pack(end+1,0xAABBCCDDu);pack(end+5,1u);
    CHECK(ask(&u,end,sizeof(end),out,&n)==0 && n==3 && out[2]==0x72);
    CHECK(iap_boot_select(&f)==IAP_BOOT_RECOVERY);
    return 0;
}
static int uds_timeout(void) {
    uds_boot_t u;uint8_t q[11]={0x34,0,0x44},out[8];size_t n;
    mock_flash_reset(&flash);uds_boot_init(&u,mock_flash_ops(&flash),100u);
    pack(q+3,IAP_APP_BASE);pack(q+7,256u);
    CHECK(ask(&u,q,sizeof(q),out,&n)==0 && n==3 && out[2]==0x7F);
    const uint8_t sess[2]={0x10,2};
    CHECK(ask(&u,sess,2,out,&n)==0 && out[0]==0x50);
    q[3]=0x09;
    CHECK(ask(&u,q,sizeof(q),out,&n)==0 && out[2]==0x31);
    uds_boot_tick(&u,5300u);
    CHECK(u.programming==0u);
    return 0;
}
static int run(const char *name,int (*fn)(void)) {
    if (fn()) return 1;
    ++passed;printf("PASS %s\n",name);return 0;
}
int main(void) {
    if(run("ISO-TP receive, SN error, timeout",iso_receive) ||
       run("ISO-TP FC / STmin / block size",iso_send) ||
       run("UDS download, idempotence, version",uds_download) ||
       run("UDS sequence and CRC faults",uds_error) ||
       run("UDS session/address/timeout",uds_timeout)) return 1;
    printf("%u/5 M2 protocol scenarios passed\n",passed);
    return 0;
}
