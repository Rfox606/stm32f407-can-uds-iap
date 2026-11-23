#include "app_uds.h"
#include "iap/config.h"
#include "iap/crc32.h"
#include "iap/image.h"
#include <stdint.h>
#include <stddef.h>

static uint8_t pending_handoff;
static void nrc(uint8_t sid,uint8_t code,uint8_t out[8],size_t *n) {
    out[0]=0x7Fu;out[1]=sid;out[2]=code;*n=3u;
}
static void put32(uint8_t *b,uint32_t v) {
    b[0]=(uint8_t)(v>>24);b[1]=(uint8_t)(v>>16);
    b[2]=(uint8_t)(v>>8);b[3]=(uint8_t)v;
}
int app_uds_handoff_pending(void) { return pending_handoff!=0u; }
int app_uds_request(const uint8_t *q,size_t n,uint8_t out[8],size_t *out_len) {
    if (!q || !n || !out || !out_len) return -1;
    *out_len=0u;
    if (pending_handoff) { nrc(q[0],0x22u,out,out_len);return 0; }
    switch(q[0]) {
    case 0x22u:
        if(n!=3u) { nrc(q[0],0x13u,out,out_len);break; }
        if(q[1]!=0xF1u || (q[2]!=0x90u && q[2]!=0x92u)) {
            nrc(q[0],0x31u,out,out_len);break;
        }
        out[0]=0x62u;out[1]=q[1];out[2]=q[2];
        if(q[2]==0x92u) put32(out+3,0u); /* APP mode */
        else {
            const volatile iap_image_record_t *meta=(const volatile iap_image_record_t *)(uintptr_t)IAP_META_BASE;
            iap_image_record_t snapshot;
            uint8_t *p=(uint8_t *)(void *)&snapshot;
            const volatile uint8_t *src=(const volatile uint8_t *)meta;
            for (size_t i=0;i<sizeof(snapshot);i++) p[i]=src[i];
            uint32_t ver=0u;
            if(snapshot.magic==IAP_IMAGE_MAGIC &&
               snapshot.image_size>=8u && snapshot.image_size<=IAP_APP_BYTES &&
               iap_crc32(&snapshot,offsetof(iap_image_record_t,record_crc32))==snapshot.record_crc32)
                ver=snapshot.version;
            put32(out+3,ver);
        }
        *out_len=7u;break;
    case 0x10u:
        if(n!=2u) { nrc(q[0],0x13u,out,out_len);break; }
        if(q[1]!=2u) { nrc(q[0],0x12u,out,out_len);break; }
        out[0]=0x50u;out[1]=2u;out[2]=0u;out[3]=50u;
        out[4]=1u;out[5]=0xF4u;*out_len=6u;
        pending_handoff=1u;
        break;
    case 0x3Eu:
        if(n!=2u || q[1]!=0u) { nrc(q[0],0x13u,out,out_len);break; }
        out[0]=0x7Eu;out[1]=0u;*out_len=2u;break;
    default:
        nrc(q[0],0x11u,out,out_len);break;
    }
    return 0;
}
