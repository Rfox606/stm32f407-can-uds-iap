#include "board.h"
#include "boot_jump.h"
#include "iap/image.h"
#include "stm32f407_flash.h"
/* Bootloader: no CAN commands yet; reserved for M2.
 * With a valid APP and button PA0 not pressed, jumps to the APP.
 * With invalid image or PA0 pressed at startup, remains in recovery mode.
 */
int main(void) {
    board_clock_init();
    board_init_leds();
    iap_flash_t f = stm32f407_flash_ops();
    if (!board_button_pressed() && iap_boot_select(&f) == IAP_BOOT_APP) {
        board_set_led(13u, 0);
        boot_jump_to_app();
    }
    while (1) {
        board_set_led(13u, 1);
        board_delay(180000u);
        board_set_led(13u, 0);
        board_delay(180000u);
        /* M2 will poll the UDS/ISO-TP state machine here. */
    }
}
