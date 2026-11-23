#include "iap/updater.h"
#include "iap/config.h"
#include "iap/crc32.h"
#include "iap/image.h"
#include <stddef.h>
#include <string.h>

void iap_updater_init(iap_updater_t *up, iap_flash_t flash) {
    if (!up) return;
    memset(up, 0, sizeof(*up));
    up->flash = flash;
    up->state = IAP_UPD_IDLE;
}

/* Prepare a transaction without erasing: the caller must send UDS 0x78 and
 * confirm CAN transmission BEFORE executing a potentially long sector erase.
 */
iap_result_t iap_updater_prepare(iap_updater_t *up, uint32_t image_size,
                                 uint32_t expected_crc, uint32_t version) {
    if (!up || !up->flash.erase_sector || !up->flash.program || !up->flash.read ||
        image_size < 8u || image_size > IAP_APP_BYTES ||
        up->state == IAP_UPD_ERASING || up->state == IAP_UPD_RECEIVING)
        return IAP_ERR_ARG;
    up->state = IAP_UPD_ERASING;
    up->expected_size = image_size;
    up->expected_crc = expected_crc;
    up->version = version;
    up->written = 0u;
    up->next_sector = 4u; /* metadata FIRST */
    up->last_sector = 5u + (unsigned)((image_size - 1u) / 0x20000u);
    return IAP_OK;
}

iap_result_t iap_updater_erase_step(iap_updater_t *up) {
    if (!up || up->state != IAP_UPD_ERASING) return IAP_ERR_STATE;
    if (up->flash.erase_sector(up->flash.ctx, up->next_sector) != 0) {
        up->state = IAP_UPD_IDLE;
        return IAP_ERR_FLASH;
    }
    ++up->next_sector;
    if (up->next_sector > up->last_sector) {
        up->state = IAP_UPD_RECEIVING;
        return IAP_OK;
    }
    return IAP_MORE;
}

void iap_updater_abort(iap_updater_t *up) {
    if (!up) return;
    up->state = IAP_UPD_IDLE;
    up->written = 0u;
}

iap_result_t iap_updater_begin(iap_updater_t *up, uint32_t image_size,
                               uint32_t expected_crc, uint32_t version) {
    iap_result_t rc = iap_updater_prepare(up, image_size, expected_crc, version);
    if (rc != IAP_OK) return rc;
    do { rc = iap_updater_erase_step(up); } while (rc == IAP_MORE);
    return rc;
}

iap_result_t iap_updater_write(iap_updater_t *up, uint32_t offset,
                               const uint8_t *payload, size_t len) {
    uint8_t aligned[IAP_MAX_BLOCK];
    uint8_t verify[IAP_MAX_BLOCK];
    size_t padded;
    if (!up || !payload || len == 0u || len > IAP_MAX_BLOCK) return IAP_ERR_ARG;
    if (up->state != IAP_UPD_RECEIVING) return IAP_ERR_STATE;
    if (offset != up->written || (offset & 3u) != 0u) return IAP_ERR_ORDER;
    if (len > up->expected_size - up->written) return IAP_ERR_ARG;
    if ((len & 3u) && (len != up->expected_size - up->written)) return IAP_ERR_ARG;
    padded = (len + 3u) & ~(size_t)3u;
    memset(aligned, 0xFF, padded);
    memcpy(aligned, payload, len);
    if (up->flash.program(up->flash.ctx, IAP_APP_BASE + offset, aligned, padded) != 0) return IAP_ERR_FLASH;
    if (up->flash.read(up->flash.ctx, IAP_APP_BASE + offset, verify, len) != 0 ||
        memcmp(verify, payload, len) != 0) return IAP_ERR_FLASH;
    up->written += (uint32_t)len;
    return IAP_OK;
}

iap_result_t iap_updater_finish(iap_updater_t *up) {
    iap_image_record_t rec;
    uint8_t chunk[128];
    uint32_t pos = 0u;
    uint32_t crc = 0xFFFFFFFFu;
    if (!up || up->state != IAP_UPD_RECEIVING || up->written != up->expected_size)
        return IAP_ERR_STATE;
    while (pos < up->expected_size) {
        uint32_t remain = up->expected_size - pos;
        size_t n = remain < sizeof(chunk) ? (size_t)remain : sizeof(chunk);
        if (up->flash.read(up->flash.ctx, IAP_APP_BASE + pos, chunk, n) != 0)
            return IAP_ERR_FLASH;
        crc = iap_crc32_update(crc, chunk, n);
        pos += (uint32_t)n;
    }
    if ((crc ^ 0xFFFFFFFFu) != up->expected_crc) return IAP_ERR_CRC;
    /* A valid CRC alone does not make an image executable.
     * Reject invalid Cortex-M vector words BEFORE committing metadata. */
    uint32_t vectors[2];
    if (up->flash.read(up->flash.ctx,IAP_APP_BASE,vectors,sizeof(vectors))!=0)
        return IAP_ERR_FLASH;
    uint32_t entry=vectors[1]&~1u;
    if (vectors[0]<IAP_SRAM_BASE || vectors[0]>IAP_SRAM_END ||
        (vectors[0]&3u)!=0u || (vectors[1]&1u)==0u ||
        entry<IAP_APP_BASE || (uint64_t)entry>=(uint64_t)IAP_APP_BASE+up->expected_size)
        return IAP_ERR_ARG;
    rec.image_size = up->expected_size;
    rec.image_crc32 = up->expected_crc;
    rec.version = up->version;
    rec.record_crc32 = iap_crc32(&rec, offsetof(iap_image_record_t, record_crc32));
    rec.magic = IAP_IMAGE_MAGIC;
    /* Record data first, commit marker last. Flash writes require 4-byte alignment. */
    if (up->flash.program(up->flash.ctx, IAP_META_BASE, &rec,
                          offsetof(iap_image_record_t, magic)) != 0)
        return IAP_ERR_FLASH;
    if (up->flash.program(up->flash.ctx, IAP_META_BASE + (uint32_t)offsetof(iap_image_record_t, magic),
                          &rec.magic, sizeof(rec.magic)) != 0)
        return IAP_ERR_FLASH;
    up->state = IAP_UPD_FINISHED;
    return IAP_OK;
}
