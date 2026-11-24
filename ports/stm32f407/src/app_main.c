#include "board.h"
#include "bxcan.h"
#include "isotp.h"
#include "reply_gate.h"
#include "app_uds.h"
#include "boot_handoff.h"
#include <stdint.h>

typedef struct { uint32_t magic, major, minor, patch; } app_version_t;
__attribute__((used,section(".app_info")))
const app_version_t app_version={0x56524553u,1u,0u,0u};

static isotp_t transport;
static reply_gate_t response_gate;
static int tx_frame(void *ctx,const uint8_t frame[8]) {
    return bxcan_send_response(ctx,frame);
}
int main(void) {
    uint8_t frame[8],dlc=0u,out[8];
    size_t out_len=0u;
    const uint8_t *req;
    size_t req_len;
    uint32_t last_led=0u;
    int led_on=0;
    board_clock_init();
    board_init_leds();
    bxcan_time_init();
    if(bxcan_init()!=0) {
        for (;;) {board_set_led(12u,1);board_delay(90000u);
                  board_set_led(12u,0);board_delay(90000u);}
    }
    isotp_init(&transport,tx_frame,0);
    reply_gate_init(&response_gate);
    for (;;) {
        uint32_t now=bxcan_now_ms();
        while(bxcan_read(frame,&dlc)==1)
            (void)isotp_on_can(&transport,frame,dlc,now);
        isotp_poll(&transport,now);
        reply_gate_poll(&response_gate,&transport,bxcan_tx_complete(),now);
        /* Handoff takes priority over newly arrived diagnostics: otherwise
         * staging a second reply could clear the already-confirmed 0x50 ACK. */
        if(app_uds_handoff_pending() && reply_gate_take_ack(&response_gate))
            boot_handoff_request();
        if(!app_uds_handoff_pending() && !reply_gate_pending(&response_gate) &&
           isotp_peek(&transport,&req,&req_len)) {
            if(app_uds_request(req,req_len,out,&out_len)==0 && out_len)
                (void)reply_gate_stage(&response_gate,out,out_len);
            isotp_consume(&transport);
        }
        if((uint32_t)(now-last_led)>=500u) {
            last_led=now;led_on=!led_on;board_set_led(12u,led_on);
        }
    }
}
