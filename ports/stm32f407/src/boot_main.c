#include "board.h"
#include "boot_jump.h"
#include "boot_handoff.h"
#include "iap/image.h"
#include "stm32f407_flash.h"
#include "bxcan.h"
#include "isotp.h"
#include "reply_gate.h"
#include "uds_boot.h"

/* One pending short UDS response. A long Flash action may run ONLY after
 * reply_gate_take_ack() confirms the preceding 0x78 was transmitted (TXOK).
 */
static isotp_t transport;
static uds_boot_t diagnostic;
static reply_gate_t response_gate;

static int can_send_frame(void *ctx, const uint8_t payload[8]) {
    return bxcan_send_response(ctx,payload);
}
int main(void) {
    uint8_t can_frame[8], response[8], dlc=0u;
    size_t response_len=0u;
    const uint8_t *request;
    size_t request_len;
    board_clock_init();
    board_init_leds();
    iap_flash_t flash = stm32f407_flash_ops();
    int handoff=boot_handoff_consume();
    if (!handoff && !board_button_pressed() && iap_boot_select(&flash)==IAP_BOOT_APP) {
        board_set_led(13u,0);
        boot_jump_to_app();
    }
    bxcan_time_init();
    if (bxcan_init()!=0) {
        for (;;) {board_set_led(13u,1);board_delay(180000u);
                  board_set_led(13u,0);board_delay(180000u);}
    }
    isotp_init(&transport,can_send_frame,0);
    reply_gate_init(&response_gate);
    uds_boot_init(&diagnostic,flash,bxcan_now_ms());

    for (;;) {
        uint32_t now=bxcan_now_ms();
        while (bxcan_read(can_frame,&dlc)==1)
            (void)isotp_on_can(&transport,can_frame,dlc,now);
        isotp_poll(&transport,now);
        uds_boot_tick(&diagnostic,now);
        reply_gate_poll(&response_gate,&transport,bxcan_tx_complete(),now);

        /* The UDS handler may enter busy_erase/finish/verify, but the actual
         * operation cannot run until its response is confirmed by bxCAN. */
        if (!diagnostic.busy_erase && !diagnostic.busy_finish && !diagnostic.busy_verify &&
            !reply_gate_pending(&response_gate) &&
            isotp_peek(&transport,&request,&request_len)) {
            if (uds_boot_request(&diagnostic,request,request_len,
                                 response,&response_len,now)==0 && response_len)
                (void)reply_gate_stage(&response_gate,response,response_len);
            isotp_consume(&transport);
        }
        if (diagnostic.busy_erase && reply_gate_take_ack(&response_gate)) {
            if (uds_boot_erase_step(&diagnostic,response,&response_len,bxcan_now_ms())>0) {
                diagnostic.last_activity_ms=bxcan_now_ms();
                (void)reply_gate_stage(&response_gate,response,response_len);
            }
        }
        if (diagnostic.busy_finish && reply_gate_take_ack(&response_gate)) {
            if (uds_boot_finish_step(&diagnostic,response,&response_len,bxcan_now_ms())>0) {
                diagnostic.last_activity_ms=bxcan_now_ms();
                (void)reply_gate_stage(&response_gate,response,response_len);
            }
        }
        if (diagnostic.busy_verify && reply_gate_take_ack(&response_gate)) {
            if (uds_boot_verify_step(&diagnostic,response,&response_len,bxcan_now_ms())>0) {
                diagnostic.last_activity_ms=bxcan_now_ms();
                (void)reply_gate_stage(&response_gate,response,response_len);
            }
        }
        /* Never reset before the ECUReset positive response is TXOK. */
        if (diagnostic.reset_requested && reply_gate_take_ack(&response_gate))
            bxcan_system_reset();
    }
}
