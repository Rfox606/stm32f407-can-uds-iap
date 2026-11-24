#ifndef IAP_REPLY_GATE_H
#define IAP_REPLY_GATE_H
#include "isotp.h"
#include <stddef.h>
#include <stdint.h>

/* Safety gate for the project's SHORT UDS responses (<=7 data bytes).
 * A deferred Flash operation or APP->Bootloader handoff must not start
 * until its preceding positive/0x78 response has been placed on bxCAN and
 * bxCAN reports TXOK. Mailbox busy means retry; TX error means fail closed.
 * This confirms CAN controller transmission, not receipt by an application.
 */
typedef struct {
    uint8_t payload[7];
    uint8_t size;
    uint8_t pending;
    uint8_t submitted;
    uint8_t acked;
} reply_gate_t;
void reply_gate_init(reply_gate_t *q);
int reply_gate_stage(reply_gate_t *q, const uint8_t *payload, size_t size);
void reply_gate_poll(reply_gate_t *q, isotp_t *tp, int can_tx_status, uint32_t now_ms);
int reply_gate_pending(const reply_gate_t *q);
int reply_gate_take_ack(reply_gate_t *q);
#endif
