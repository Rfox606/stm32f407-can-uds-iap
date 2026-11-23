#include "iap/image.h"
#include "iap/config.h"
#include "iap/crc32.h"
#include <stddef.h>
#include <stdint.h>

static int valid_vector(const iap_flash_t *flash, uint32_t n) {
    uint32_t vec[2];
    uint32_t reset;
    if (flash->read(flash->ctx, IAP_APP_BASE, vec, sizeof(vec)) != 0) return 0;
    if ((vec[0] < IAP_SRAM_BASE) || (vec[0] > IAP_SRAM_END) || (vec[0] & 3u)) return 0;
    if ((vec[1] & 1u) == 0u) return 0; /* Cortex-M Thumb reset handler */
    reset = vec[1] & ~1u;
    if ((reset < IAP_APP_BASE) || ((uint64_t)reset >= ((uint64_t)IAP_APP_BASE + n))) return 0;
    return 1;
}

int iap_image_validate(const iap_flash_t *flash, iap_image_record_t *out_record) {
    iap_image_record_t rec;
    uint8_t buf[128];
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t pos = 0;
    if (!flash || !flash->read) return 0;
    if (flash->read(flash->ctx, IAP_META_BASE, &rec, sizeof(rec)) != 0) return 0;
    if (rec.magic != IAP_IMAGE_MAGIC || rec.image_size < 8u || rec.image_size > IAP_APP_BYTES) return 0;
    if (iap_crc32(&rec, offsetof(iap_image_record_t, record_crc32)) != rec.record_crc32) return 0;
    if (!valid_vector(flash, rec.image_size)) return 0;
    while (pos < rec.image_size) {
        uint32_t remain = rec.image_size - pos;
        size_t chunk = remain < sizeof(buf) ? (size_t)remain : sizeof(buf);
        if (flash->read(flash->ctx, IAP_APP_BASE + pos, buf, chunk) != 0) return 0;
        crc = iap_crc32_update(crc, buf, chunk);
        pos += (uint32_t)chunk;
    }
    if ((crc ^ 0xFFFFFFFFu) != rec.image_crc32) return 0;
    if (out_record) *out_record = rec;
    return 1;
}

iap_boot_choice_t iap_boot_select(const iap_flash_t *flash) {
    return iap_image_validate(flash, NULL) ? IAP_BOOT_APP : IAP_BOOT_RECOVERY;
}
