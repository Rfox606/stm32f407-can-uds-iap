#include "board.h"
#include <stdint.h>

typedef struct { uint32_t magic, major, minor, patch; } app_version_t;
/* A real, linked APP image includes a discoverable version record. */
__attribute__((used,section(".app_info")))
const app_version_t app_version = { 0x56524553u, 1u, 0u, 0u };

int main(void) {
    board_clock_init();
    board_init_leds();
    while (1) {
        board_set_led(12u, 1);
        board_delay(600000u);
        board_set_led(12u, 0);
        board_delay(600000u);
    }
}
