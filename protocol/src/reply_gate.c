#include "reply_gate.h"
#include <string.h>

void reply_gate_init(reply_gate_t *q) {
    if (q) memset(q, 0, sizeof(*q));
}
int reply_gate_stage(reply_gate_t *q, const uint8_t *payload, size_t size) {
    if (!q || !payload || !size || size > sizeof(q->payload) || q->pending) return -1;
    memcpy(q->payload,payload,size);
    q->size=(uint8_t)size;
    q->pending=1u;
    q->submitted=0u;
    q->acked=0u;
    return 0;
}
void reply_gate_poll(reply_gate_t *q, isotp_t *tp, int can_tx_status, uint32_t now_ms) {
    if (!q || !tp || !q->pending || !isotp_tx_idle(tp)) return;
    if (can_tx_status != 1) return; /* busy or bus error: no state advancement */
    if (!q->submitted) {
        if (isotp_send(tp,q->payload,q->size,now_ms)==0) q->submitted=1u;
        return;
    }
    /* Called in a subsequent poll; bxCAN confirms TXOK of submitted frame. */
    q->pending=0u;
    q->acked=1u;
}
int reply_gate_pending(const reply_gate_t *q) {
    return q && q->pending;
}
int reply_gate_take_ack(reply_gate_t *q) {
    if (!q || !q->acked) return 0;
    q->acked=0u;
    return 1;
}
