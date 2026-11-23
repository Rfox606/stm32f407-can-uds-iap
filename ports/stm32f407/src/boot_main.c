#include "board.h"
#include "boot_jump.h"
#include "iap/image.h"
#include "stm32f407_flash.h"
#include "bxcan.h"
#include "isotp.h"
#include "uds_boot.h"

static isotp_t transport;
static uds_boot_t diagnostic;
static int can_send_frame(void *ctx, const uint8_t payload[8]) {
    return bxcan_send_response(ctx,payload);
}
int main(void) {
    uint8_t can_frame[8], response[8], dlc=0u;
    size_t response_len=0u;
    const uint8_t *request;
    size_t request_len;
    uint32_t now;
    board_clock_init();
    board_init_leds();
    iap_flash_t flash = stm32f407_flash_ops();
    if (!board_button_pressed() && iap_boot_select(&flash) == IAP_BOOT_APP) {
        board_set_led(13u,0);
        boot_jump_to_app();
    }
    bxcan_time_init();
    if (bxcan_init()!=0) {
        /* Preserve a visible error indication if CAN cannot enter normal mode. */
        for (;;) { board_set_led(13u,1); board_delay(180000u);
                    board_set_led(13u,0); board_delay(180000u); }
    }
    isotp_init(&transport,can_send_frame,0);
    uds_boot_init(&diagnostic,flash,bxcan_now_ms());
    for (;;) {
        now=bxcan_now_ms();
        while (bxcan_read(can_frame,&dlc)==1) {
            (void)isotp_on_can(&transport,can_frame,dlc,now);
        }
        isotp_poll(&transport,now);
        uds_boot_tick(&diagnostic,now);
        /* Consume a request only when a response CAN mailbox is available.
         * No Flash work is performed from the CAN receive path. */
        if (!diagnostic.busy_erase && !diagnostic.busy_verify && isotp_peek(&transport,&request,&request_len) &&
            isotp_tx_idle(&transport) && bxcan_tx_complete()==1) {
            if (uds_boot_request(&diagnostic,request,request_len,
                                 response,&response_len,now)==0 && response_len) {
                (void)isotp_send(&transport,response,response_len,now);
            }
            isotp_consume(&transport);
        }
        /* 0x34 is initially answered with 7F 34 78. After that frame has
         * been ACKed by CAN, one Flash sector can be erased; then the next
         * 0x78 or final 0x74 response is sent. The sender must wait for 0x74. */
        if (diagnostic.busy_erase && isotp_tx_idle(&transport) &&
            bxcan_tx_complete()==1) {
            if (uds_boot_erase_step(&diagnostic,response,&response_len,bxcan_now_ms())>0) {
                (void)isotp_send(&transport,response,response_len,bxcan_now_ms());
            }
        }
        if (diagnostic.busy_verify && isotp_tx_idle(&transport) &&
            bxcan_tx_complete()==1) {
            if (uds_boot_verify_step(&diagnostic,response,&response_len,bxcan_now_ms())>0)
                (void)isotp_send(&transport,response,response_len,bxcan_now_ms());
        }
        if (diagnostic.reset_requested && isotp_tx_idle(&transport) &&
            bxcan_tx_complete()==1) {
            bxcan_system_reset();
        }
    }
}
