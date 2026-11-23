#ifndef IAP_UDS_BOOT_H
#define IAP_UDS_BOOT_H
#include "iap/updater.h"
#include <stddef.h>
#include <stdint.h>
/* Limited UDS programming server, with an explicit OEM TransferExit record.
 * Requests/positive responses are ISO-TP application payloads, NOT CAN frames.
 */
#define UDS_S3_MS 5100u
typedef struct {
    iap_updater_t updater;
    uint32_t last_activity_ms;
    uint32_t last_offset;
    uint16_t last_len;
    uint8_t programming;
    uint8_t last_seq, next_seq, has_last;
    uint8_t busy_erase, busy_verify, reset_requested;
} uds_boot_t;
void uds_boot_init(uds_boot_t *b, iap_flash_t flash, uint32_t now_ms);
/* Always fills response (positive or 7F negative), length <=7; return -1 on bad pointers. */
int uds_boot_request(uds_boot_t *b, const uint8_t *req, size_t len,
                     uint8_t out[8], size_t *out_len, uint32_t now_ms);
/* Called ONLY after previous pending CAN frame has physically completed.
 * Does one Flash sector erase then returns either 7F 34 78 or final 74 response.
 * Returns 1 if a response was produced, 0 if no erase was pending.
 */
int uds_boot_erase_step(uds_boot_t *b, uint8_t out[8], size_t *out_len, uint32_t now_ms);
/* Deferred CRC scan: called only after NRC 7F 31 78 is CAN-ACKed. */
int uds_boot_verify_step(uds_boot_t *b, uint8_t out[8], size_t *out_len, uint32_t now_ms);
void uds_boot_tick(uds_boot_t *b, uint32_t now_ms);
#endif
