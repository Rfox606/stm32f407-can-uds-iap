#include "uds_boot.h"
#include "iap/config.h"
#include "iap/crc32.h"
#include "iap/image.h"
#include <string.h>
/* NRC names follow ISO 14229; only a small programming subset is implemented. */
#define NRC_SERVICE (0x11u)
#define NRC_SUBFUNCTION (0x12u)
#define NRC_LENGTH (0x13u)
#define NRC_CONDITIONS (0x22u)
#define NRC_SEQUENCE (0x24u)
#define NRC_RANGE (0x31u)
#define NRC_PROGRAMMING (0x72u)
#define NRC_BLOCK (0x73u)
#define NRC_PENDING (0x78u)
#define NRC_SESSION (0x7Fu)
static uint32_t get32(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static void put32(uint8_t *p, uint32_t n) {
    p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;
}
static void nrc(uint8_t sid, uint8_t code, uint8_t out[8], size_t *n) {
    out[0]=0x7Fu;out[1]=sid;out[2]=code;*n=3u;
}
static void reset_session(uds_boot_t *b) {
    b->programming=0u;
    b->has_last=0u;
    b->busy_erase=0u;
    b->busy_verify=0u;
    b->busy_finish=0u;
    iap_updater_abort(&b->updater);
}
void uds_boot_init(uds_boot_t *b, iap_flash_t flash, uint32_t now_ms) {
    if (!b) return;
    memset(b,0,sizeof(*b));
    iap_updater_init(&b->updater,flash);
    b->last_activity_ms=now_ms;
    b->next_seq=1u;
}
void uds_boot_tick(uds_boot_t *b, uint32_t now_ms) {
    if (!b || !b->programming || (b->busy_erase || b->busy_verify || b->busy_finish)) return;
    if ((uint32_t)(now_ms-b->last_activity_ms)>UDS_S3_MS) reset_session(b);
}
int uds_boot_request(uds_boot_t *b, const uint8_t *q, size_t n,
                     uint8_t out[8], size_t *o, uint32_t now_ms) {
    uint8_t sid;
    if (!b || !q || !n || !out || !o) return -1;
    sid=q[0];
    *o=0u;
    if (b->busy_erase || b->busy_verify || b->busy_finish) { nrc(sid,NRC_CONDITIONS,out,o);return 0; }
    b->last_activity_ms=now_ms;
    switch(sid) {
    case 0x10u: { /* DiagnosticSessionControl */
        if (n!=2u) { nrc(sid,NRC_LENGTH,out,o);break; }
        if (q[1]!=1u && q[1]!=2u) { nrc(sid,NRC_SUBFUNCTION,out,o);break; }
        if (q[1]==1u) reset_session(b);
        else b->programming=1u;
        out[0]=0x50u;out[1]=q[1];out[2]=0u;out[3]=50u;
        out[4]=0x01u;out[5]=0xF4u;
        *o=6u;break;
    }
    case 0x22u: { /* ReadDataByIdentifier; F190=4B image version, F191=state/progress */
        if (n!=3u) { nrc(sid,NRC_LENGTH,out,o);break; }
        if (q[1]!=0xF1u || (q[2]!=0x90u && q[2]!=0x91u)) { nrc(sid,NRC_RANGE,out,o);break; }
        out[0]=0x62u;out[1]=q[1];out[2]=q[2];
        if (q[2]==0x90u) {
            /* Read ONLY metadata for a quick DID response; 0x31 verifies the entire image. */
            iap_image_record_t rec;
            uint32_t ver=0u;
            if (b->updater.flash.read(b->updater.flash.ctx,IAP_META_BASE,&rec,sizeof(rec))==0 &&
                rec.magic==IAP_IMAGE_MAGIC && rec.image_size>=8u && rec.image_size<=IAP_APP_BYTES &&
                iap_crc32(&rec,offsetof(iap_image_record_t,record_crc32))==rec.record_crc32)
                ver=rec.version;
            put32(out+3,ver);*o=7u;
        } else {
            out[3]=(uint8_t)b->updater.state;
            out[4]=(uint8_t)(b->updater.written>>16);
            out[5]=(uint8_t)(b->updater.written>>8);
            out[6]=(uint8_t)b->updater.written;
            *o=7u;
        }
        break;
    }
    case 0x34u: { /* 34 00 44 + 4B absolute addr + 4B size */
        uint32_t addr,size;
        if (!b->programming) { nrc(sid,NRC_SESSION,out,o);break; }
        if (n!=11u || q[1]!=0u || q[2]!=0x44u) { nrc(sid,NRC_LENGTH,out,o);break; }
        addr=get32(q+3);size=get32(q+7);
        if (addr!=IAP_APP_BASE || size<8u || size>IAP_APP_BYTES) { nrc(sid,NRC_RANGE,out,o);break; }
        if (b->updater.state==IAP_UPD_RECEIVING) iap_updater_abort(&b->updater);
        if (iap_updater_prepare(&b->updater,size,0u,0u)!=IAP_OK) {
            nrc(sid,NRC_CONDITIONS,out,o);break;
        }
        b->busy_erase=1u;b->has_last=0u;b->next_seq=1u;
        nrc(sid,NRC_PENDING,out,o);
        break;
    }
    case 0x36u: { /* TransferData, max 256 payload bytes */
        uint8_t seq;
        if (!b->programming) { nrc(sid,NRC_SESSION,out,o);break; }
        if (b->updater.state!=IAP_UPD_RECEIVING) { nrc(sid,NRC_SEQUENCE,out,o);break; }
        if (n<3u || n>258u) { nrc(sid,NRC_LENGTH,out,o);break; }
        seq=q[1];
        if (b->has_last && seq==b->last_seq) {
            uint8_t old[IAP_MAX_BLOCK];
            if ((size_t)b->last_len!=n-2u ||
                b->updater.flash.read(b->updater.flash.ctx,
                    IAP_APP_BASE+b->last_offset,old,n-2u)!=0 ||
                memcmp(old,q+2,n-2u)!=0) {
                nrc(sid,NRC_BLOCK,out,o);break;
            }
        } else if (seq==b->next_seq) {
            uint32_t off=b->updater.written;
            iap_result_t rc=iap_updater_write(&b->updater,off,q+2,n-2u);
            if (rc!=IAP_OK) { nrc(sid,(rc==IAP_ERR_ARG)?NRC_LENGTH:NRC_PROGRAMMING,out,o);break; }
            b->has_last=1u;b->last_seq=seq;b->last_offset=off;
            b->last_len=(uint16_t)(n-2u);
            b->next_seq=(uint8_t)(seq+1u);
        } else { nrc(sid,NRC_BLOCK,out,o);break; }
        out[0]=0x76u;out[1]=seq;*o=2u;break;
    }
    case 0x37u: { /* TransferExit: defer full CRC and metadata commit */
        if (!b->programming) { nrc(sid,NRC_SESSION,out,o);break; }
        if (n!=9u) { nrc(sid,NRC_LENGTH,out,o);break; }
        if (b->updater.state!=IAP_UPD_RECEIVING ||
            b->updater.written!=b->updater.expected_size) {
            nrc(sid,NRC_SEQUENCE,out,o);break;
        }
        b->updater.expected_crc=get32(q+1);
        b->updater.version=get32(q+5);
        b->busy_finish=1u;
        nrc(sid,NRC_PENDING,out,o);
        break;
    }
    case 0x31u: { /* routine FF01: deferred full-image CRC scan */
        if (n!=4u) { nrc(sid,NRC_LENGTH,out,o);break; }
        if (q[1]!=1u || q[2]!=0xFFu || q[3]!=1u) { nrc(sid,NRC_RANGE,out,o);break; }
        b->busy_verify=1u;
        nrc(sid,NRC_PENDING,out,o);
        break;
    }
    case 0x11u: {
        if (n!=2u) { nrc(sid,NRC_LENGTH,out,o);break; }
        if (q[1]!=1u) { nrc(sid,NRC_SUBFUNCTION,out,o);break; }
        out[0]=0x51u;out[1]=1u;*o=2u;
        b->reset_requested=1u;break;
    }
    case 0x3Eu:
        if (n!=2u || q[1]!=0u) { nrc(sid,NRC_LENGTH,out,o);break; }
        out[0]=0x7Eu;out[1]=0u;*o=2u;break;
    default: nrc(sid,NRC_SERVICE,out,o);break;
    }
    return 0;
}
int uds_boot_erase_step(uds_boot_t *b, uint8_t out[8], size_t *out_len, uint32_t now_ms) {
    iap_result_t rc;
    if (!b || !out || !out_len || !b->busy_erase) return 0;
    rc=iap_updater_erase_step(&b->updater);
    b->last_activity_ms = now_ms; /* S3 starts after lengthy erase, not before it. */
    if (rc==IAP_MORE) nrc(0x34u,NRC_PENDING,out,out_len);
    else {
        b->busy_erase=0u;
        if (rc!=IAP_OK) nrc(0x34u,NRC_PROGRAMMING,out,out_len);
        else {
            /* 0x74, lengthFormatIdentifier=0x20, maxNumberOfBlockLength=258 */
            out[0]=0x74u;out[1]=0x20u;out[2]=1u;out[3]=2u;*out_len=4u;
        }
    }
    return 1;
}

int uds_boot_verify_step(uds_boot_t *b, uint8_t out[8], size_t *out_len, uint32_t now_ms) {
    if (!b || !out || !out_len || !b->busy_verify) return 0;
    /* Read-only operation may be long for large images. It follows a CAN-ACKed
     * ResponsePending frame, just like a Flash sector erase.
     */
    uint8_t valid=(uint8_t)(iap_image_validate(&b->updater.flash,0)?0u:1u);
    b->busy_verify=0u;
    b->last_activity_ms=now_ms;
    out[0]=0x71u;out[1]=1u;out[2]=0xFFu;out[3]=1u;out[4]=valid;
    *out_len=5u;
    return 1;
}

int uds_boot_finish_step(uds_boot_t *b, uint8_t out[8], size_t *out_len, uint32_t now_ms) {
    if (!b || !out || !out_len || !b->busy_finish) return 0;
    iap_result_t rc=iap_updater_finish(&b->updater);
    b->busy_finish=0u;
    b->last_activity_ms=now_ms;
    if (rc!=IAP_OK) {
        iap_updater_abort(&b->updater);
        nrc(0x37u,NRC_PROGRAMMING,out,out_len);
    } else {
        out[0]=0x77u;*out_len=1u;
    }
    return 1;
}
