#include "board.h"
#include "bxcan.h"
#include "isotp.h"
#include "app_uds.h"
#include "boot_handoff.h"
#include <stdint.h>

typedef struct { uint32_t magic, major, minor, patch; } app_version_t;
__attribute__((used,section(".app_info")))
const app_version_t app_version={0x56524553u,1u,0u,0u};

static isotp_t transport;
static int tx_frame(void *ctx,const uint8_t frame[8]) {
    return bxcan_send_response(ctx,frame);
}
int main(void) {
    uint8_t frame[8],dlc=0u,out[8];
    size_t out_len;
    const uint8_t *req;
    size_t req_len;
    uint32_t last_led=0u;
    int led_on=0;
    board_clock_init();
    board_init_leds();
    bxcan_time_init();
    if(bxcan_init()!=0) {
        /* GPIO still indicates lack of CAN initialization on physical hardware. */
        for (;;) { board_set_led(12u,1);board_delay(90000u);
                   board_set_led(12u,0);board_delay(90000u); }
    }
    isotp_init(&transport,tx_frame,0);
    for (;;) {
        uint32_t now=bxcan_now_ms();
        while(bxcan_read(frame,&dlc)==1) {
            (void)isotp_on_can(&transport,frame,dlc,now);
        }
        isotp_poll(&transport,now);
        if(isotp_peek(&transport,&req,&req_len) &&
           isotp_tx_idle(&transport) && bxcan_tx_complete()==1) {
            if(app_uds_request(req,req_len,out,&out_len)==0 && out_len)
                (void)isotp_send(&transport,out,out_len,now);
            isotp_consume(&transport);
        }
        /* Wait until CAN controller reports the positive UDS response TX OK,
         * then issue a software reset with a retained SRAM handoff marker.
         */
        if(app_uds_handoff_pending() && isotp_tx_idle(&transport) &&
           bxcan_tx_complete()==1) boot_handoff_request();
        if((uint32_t)(now-last_led)>=500u) {
            last_led=now;led_on=!led_on;board_set_led(12u,led_on);
        }
    }
}
