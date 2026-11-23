#ifndef IAP_ISOTP_H
#define IAP_ISOTP_H
#include <stddef.h>
#include <stdint.h>
/* Classic CAN, normal 11-bit addressing, ISO-TP 12-bit length (<=4095).
 * We bound a single diagnostic PDU to 320 bytes to conserve SRAM.
 * Supports SF, FF, CF, FC; BS and millisecond STmin; no extended address,
 * CAN FD, 32-bit FF lengths, or 100-900us STmin scheduling.
 */
#define ISOTP_CAPACITY 320u
#define ISOTP_TIMEOUT_MS 1000u
typedef int (*isotp_can_tx_t)(void *ctx, const uint8_t frame[8]);
typedef struct {
    void *ctx;
    isotp_can_tx_t can_tx;
    uint8_t rx[ISOTP_CAPACITY];
    uint8_t tx[ISOTP_CAPACITY];
    uint16_t rx_size, rx_count, tx_size, tx_count;
    uint32_t rx_last_ms, tx_last_ms, tx_next_ms;
    uint8_t rx_next_sn, tx_next_sn;
    uint8_t tx_block_left, tx_wait_fc, tx_stmin_ms;
    uint8_t rx_active, rx_ready, tx_active;
    uint32_t rx_errors, tx_errors;
} isotp_t;
void isotp_init(isotp_t *t, isotp_can_tx_t send_fn, void *ctx);
/* Feed frames from the expected CAN identifier only; 0 on accepted, -1 on malformed. */
int isotp_on_can(isotp_t *t, const uint8_t frame[8], uint8_t dlc, uint32_t now_ms);
void isotp_poll(isotp_t *t, uint32_t now_ms);
/* 0 accepted, -1 busy/invalid/no CAN mailbox. */
int isotp_send(isotp_t *t, const uint8_t *data, size_t len, uint32_t now_ms);
int isotp_peek(const isotp_t *t, const uint8_t **data, size_t *len);
void isotp_consume(isotp_t *t);
int isotp_tx_idle(const isotp_t *t);
#endif
