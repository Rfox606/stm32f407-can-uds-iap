#include "isotp.h"
#include <string.h>
#define DLY(now, then) ((uint32_t)((now)-(then)))

static int send8(isotp_t *t, uint8_t *data) {
    return (t->can_tx && t->can_tx(t->ctx, data) == 0) ? 0 : -1;
}
static void abort_rx(isotp_t *t) {
    t->rx_active = 0u;
    t->rx_size = 0u;
    t->rx_count = 0u;
    ++t->rx_errors;
}
static void abort_tx(isotp_t *t) {
    t->tx_active = 0u;
    t->tx_wait_fc = 0u;
    ++t->tx_errors;
}
void isotp_init(isotp_t *t, isotp_can_tx_t send_fn, void *ctx) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
    t->can_tx = send_fn;
    t->ctx = ctx;
}
int isotp_peek(const isotp_t *t, const uint8_t **data, size_t *len) {
    if (!t || !data || !len || !t->rx_ready) return 0;
    *data = t->rx;
    *len = t->rx_size;
    return 1;
}
void isotp_consume(isotp_t *t) {
    if (!t) return;
    t->rx_ready = 0u;
    t->rx_size = 0u;
    t->rx_count = 0u;
}
int isotp_tx_idle(const isotp_t *t) {
    return t && !t->tx_active;
}
int isotp_on_can(isotp_t *t, const uint8_t frame[8], uint8_t dlc, uint32_t now_ms) {
    uint8_t pci;
    if (!t || !frame || dlc == 0u || dlc > 8u) return -1;
    pci = frame[0] >> 4;
    if (pci == 3u) { /* Flow Control is for OUR transmitter */
        if (!t->tx_active || !t->tx_wait_fc || dlc < 3u) return -1;
        if ((frame[0] & 0x0Fu) == 1u) { /* WAIT */
            if (++t->tx_wait_count > 3u) { abort_tx(t); return -1; }
            t->tx_last_ms = now_ms;
            return 0;
        }
        if ((frame[0] & 0x0Fu) != 0u || (frame[2] > 0x7Fu && (frame[2] < 0xF1u || frame[2] > 0xF9u))) {
            abort_tx(t);
            return -1;
        }
        t->tx_wait_count=0u;
        t->tx_block_left = frame[1];
        t->tx_stmin_ms = frame[2] > 0x7Fu ? 1u : frame[2]; /* sub-ms STmin conservatively rounded UP */
        t->tx_wait_fc = 0u;
        t->tx_last_ms = now_ms;
        t->tx_next_ms = now_ms;
        return 0;
    }
    if (t->rx_ready) return -1; /* one completed diagnostic request at a time */
    if (pci == 0u) {
        uint8_t n = frame[0] & 0x0Fu;
        if (t->rx_active || n == 0u || n > 7u || dlc < (uint8_t)(n+1u)) return -1;
        memcpy(t->rx, frame+1, n);
        t->rx_size = n;
        t->rx_count = n;
        t->rx_ready = 1u;
        return 0;
    }
    if (pci == 1u) {
        uint16_t total;
        uint8_t fc[8]={0x30u,0u,0u,0u,0u,0u,0u,0u};
        if (t->rx_active || dlc != 8u) return -1;
        total=(uint16_t)(((uint16_t)(frame[0]&0x0Fu)<<8)|frame[1]);
        if (total <= 7u || total > ISOTP_CAPACITY) { ++t->rx_errors; return -1; }
        memcpy(t->rx, frame+2, 6u);
        t->rx_size = total;
        t->rx_count = 6u;
        t->rx_next_sn = 1u;
        t->rx_last_ms = now_ms;
        t->rx_active = 1u;
        if (send8(t,fc) != 0) { abort_rx(t); return -1; }
        return 0;
    }
    if (pci == 2u) {
        uint16_t n;
        if (!t->rx_active || dlc != 8u || (frame[0]&0x0Fu) != t->rx_next_sn ||
            DLY(now_ms,t->rx_last_ms)>ISOTP_TIMEOUT_MS) { abort_rx(t); return -1; }
        n = (uint16_t)(t->rx_size - t->rx_count);
        if (n > 7u) n = 7u;
        memcpy(t->rx+t->rx_count,frame+1,n);
        t->rx_count = (uint16_t)(t->rx_count+n);
        t->rx_next_sn = (uint8_t)((t->rx_next_sn+1u)&0x0Fu);
        t->rx_last_ms=now_ms;
        if (t->rx_count == t->rx_size) { t->rx_active=0u; t->rx_ready=1u; }
        return 0;
    }
    return -1;
}
int isotp_send(isotp_t *t, const uint8_t *data, size_t len, uint32_t now_ms) {
    uint8_t f[8]={0};
    if (!t || !data || !t->can_tx || len == 0u || len > ISOTP_CAPACITY || t->tx_active) return -1;
    if (len <= 7u) {
        f[0] = (uint8_t)len;
        memcpy(f+1,data,len);
        return send8(t,f);
    }
    f[0]=(uint8_t)(0x10u|((len>>8)&0x0Fu));
    f[1]=(uint8_t)len;
    memcpy(f+2,data,6u);
    if (send8(t,f) != 0) return -1;
    memcpy(t->tx,data,len);
    t->tx_size=(uint16_t)len;
    t->tx_count=6u;
    t->tx_next_sn=1u;
    t->tx_active=1u;
    t->tx_wait_fc=1u;
    t->tx_last_ms=now_ms;
    return 0;
}
void isotp_poll(isotp_t *t, uint32_t now_ms) {
    uint8_t f[8]={0};
    uint16_t n;
    if (!t) return;
    if (t->rx_active && DLY(now_ms,t->rx_last_ms)>ISOTP_TIMEOUT_MS) abort_rx(t);
    if (!t->tx_active) return;
    if (t->tx_wait_fc) {
        if (DLY(now_ms,t->tx_last_ms)>ISOTP_TIMEOUT_MS) abort_tx(t);
        return;
    }
    if ((int32_t)(now_ms-t->tx_next_ms)<0) return;
    if (t->tx_count >= t->tx_size) { t->tx_active=0u; return; }
    n=(uint16_t)(t->tx_size - t->tx_count);
    if (n>7u) n=7u;
    f[0]=(uint8_t)(0x20u|t->tx_next_sn);
    memcpy(f+1,t->tx+t->tx_count,n);
    if (send8(t,f) != 0) { /* retry, but abort if mailbox blocked too long */
        if (DLY(now_ms,t->tx_last_ms)>ISOTP_TIMEOUT_MS) abort_tx(t);
        return;
    }
    t->tx_count=(uint16_t)(t->tx_count+n);
    t->tx_next_sn=(uint8_t)((t->tx_next_sn+1u)&0x0Fu);
    t->tx_last_ms=now_ms;
    t->tx_next_ms=now_ms+t->tx_stmin_ms;
    if (t->tx_count>=t->tx_size) t->tx_active=0u;
    else if (t->tx_block_left) {
        --t->tx_block_left;
        if (!t->tx_block_left) t->tx_wait_fc=1u;
    }
}
