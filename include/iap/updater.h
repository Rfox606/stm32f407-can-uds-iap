#ifndef IAP_UPDATER_H
#define IAP_UPDATER_H
#include "iap/flash_if.h"
#include <stdint.h>
#include <stddef.h>

typedef enum {
    IAP_OK = 0, IAP_MORE = 1, IAP_ERR_ARG = -1, IAP_ERR_STATE = -2,
    IAP_ERR_FLASH = -3, IAP_ERR_ORDER = -4, IAP_ERR_CRC = -5
} iap_result_t;

typedef enum { IAP_UPD_IDLE, IAP_UPD_ERASING, IAP_UPD_RECEIVING, IAP_UPD_FINISHED } iap_update_state_t;

typedef struct {
    iap_flash_t flash;
    iap_update_state_t state;
    uint32_t expected_size;
    uint32_t expected_crc;
    uint32_t version;
    uint32_t written;
    unsigned next_sector;
    unsigned last_sector;
} iap_updater_t;

void iap_updater_init(iap_updater_t *up, iap_flash_t flash);
iap_result_t iap_updater_prepare(iap_updater_t *up, uint32_t image_size,
                                 uint32_t expected_crc, uint32_t version);
/* Erase precisely ONE sector, returning IAP_MORE or IAP_OK when complete. */
iap_result_t iap_updater_erase_step(iap_updater_t *up);
void iap_updater_abort(iap_updater_t *up);
iap_result_t iap_updater_begin(iap_updater_t *up, uint32_t image_size,
                               uint32_t expected_crc, uint32_t version);
iap_result_t iap_updater_write(iap_updater_t *up, uint32_t offset,
                               const uint8_t *payload, size_t len);
iap_result_t iap_updater_finish(iap_updater_t *up);
#endif
