#ifndef STM32F407_BOARD_H
#define STM32F407_BOARD_H
#include <stdint.h>
void board_clock_init(void);
void board_init_leds(void);
void board_set_led(unsigned pin, int on);
int board_button_pressed(void);
void board_delay(volatile uint32_t count);
#endif
