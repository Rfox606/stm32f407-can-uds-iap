/* Real C ISO-TP + UDS + updater; byte trace is software-generated, not candump. */
#include "isotp.h"
#include "uds_boot.h"
#include "iap/config.h"
#include "iap/crc32.h"
#include "iap/image.h"
#include "mock_flash.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static mock_flash_t flash;
static isotp_t tp;
static uds_boot_t uds;
static unsigned frames;
static uint32_t clock_ms=0;
static void print_frame(unsigned id,const uint8_t *f){
    printf("SW %03X [8]",id);
    for(unsigned i=0;i<8;i++)printf(" %02X",f[i]);
    putchar('\n');
}
static int tx(void *ctx,const uint8_t f[8]){
    (void)ctx;frames++;print_frame(0x7E8,f);return 0;
}
static int rx(const uint8_t f[8]){
    frames++;print_frame(0x7E0,f);return isotp_on_can(&tp,f,8,++clock_ms);
}
static int send_request(const uint8_t *q,size_t len,uint8_t out[8],size_t *n){
    uint8_t frame[8]={0};
    if(len<=7u){frame[0]=(uint8_t)len;memcpy(frame+1,q,len);CHECK(rx(frame)==0);}
    else{
        frame[0]=(uint8_t)(0x10u|(len>>8));frame[1]=(uint8_t)len;memcpy(frame+2,q,6);
        CHECK(rx(frame)==0);
        size_t off=6u;unsigned sn=1u;
        while(off<len){
            size_t k=len-off>7u?7u:len-off;
            memset(frame,0,8);frame[0]=(uint8_t)(0x20u|(sn&15u));memcpy(frame+1,q+off,k);
            CHECK(rx(frame)==0);off+=k;sn++;
        }
    }
    const uint8_t *p;size_t size;
    CHECK(isotp_peek(&tp,&p,&size)==1 && size==len && memcmp(p,q,len)==0);
    CHECK(uds_boot_request(&uds,p,size,out,n,clock_ms)==0);
    isotp_consume(&tp);
    CHECK(isotp_send(&tp,out,*n,++clock_ms)==0);
    return 0;
}
static void be32(uint8_t *b,uint32_t v){
    b[0]=(uint8_t)(v>>24);b[1]=(uint8_t)(v>>16);b[2]=(uint8_t)(v>>8);b[3]=(uint8_t)v;
}
int main(void){
    uint8_t out[8],block[258]={0x36,1},end[9]={0x37};
    uint8_t request[11]={0x34,0,0x44};
    size_t n;uint32_t sp=0x2001FFC0u,entry=IAP_APP_BASE+0x21u;
    mock_flash_reset(&flash);
    iap_flash_t f=mock_flash_ops(&flash);
    uds_boot_init(&uds,f,0);isotp_init(&tp,tx,0);
    const uint8_t session[]={0x10,2};
    CHECK(send_request(session,2,out,&n)==0 && out[0]==0x50);
    be32(request+3,IAP_APP_BASE);be32(request+7,256u);
    CHECK(send_request(request,11,out,&n)==0 && out[2]==0x78);
    while(uds.busy_erase){
        CHECK(uds_boot_erase_step(&uds,out,&n,++clock_ms)==1);
        CHECK(isotp_send(&tp,out,n,++clock_ms)==0);
    }
    CHECK(out[0]==0x74);
    for(unsigned i=0;i<256;i++)block[2+i]=(uint8_t)(i*13u+9u);
    memcpy(block+2,&sp,4);memcpy(block+6,&entry,4);
    unsigned before=frames;
    CHECK(send_request(block,sizeof(block),out,&n)==0 && out[0]==0x76 && out[1]==1);
    CHECK(frames-before==39u);
    be32(end+1,iap_crc32(block+2,256));be32(end+5,0x010002u);
    CHECK(send_request(end,9,out,&n)==0 && out[2]==0x78);
    CHECK(uds_boot_finish_step(&uds,out,&n,++clock_ms)==1 && out[0]==0x77);
    CHECK(isotp_send(&tp,out,n,++clock_ms)==0);
    const uint8_t verify[]={0x31,1,0xFF,1};
    CHECK(send_request(verify,4,out,&n)==0 && out[2]==0x78);
    CHECK(uds_boot_verify_step(&uds,out,&n,++clock_ms)==1 && out[4]==0);
    CHECK(isotp_send(&tp,out,n,++clock_ms)==0);
    const uint8_t version[]={0x22,0xF1,0x50};
    CHECK(send_request(version,3,out,&n)==0 && n==7 && out[4]==1 && out[6]==2);
    CHECK(iap_boot_select(&f)==IAP_BOOT_APP);
    puts("PASS full software CAN-frame trace; 39 frames for 256B block; image valid");
    return 0;
}
