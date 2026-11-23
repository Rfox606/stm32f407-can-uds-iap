#include "stm32f407_min.h"
/* STM32F407G-DISC1 board: LEDs PD12/PD13, user button PA0 active high. */
void board_init_leds(void) {
    RCC_AHB1ENR |= (1u << 3); /* GPIOD clock */
    (void)RCC_AHB1ENR;
    GPIOD_MODER = (GPIOD_MODER & ~((3u << 24) | (3u << 26))) |
                    (1u << 24) | (1u << 26);
}
void board_set_led(unsigned pin, int on) {
    if (pin != 12u && pin != 13u) return;
    GPIOD_BSRR = on ? (1u << pin) : (1u << (pin + 16u));
}
int board_button_pressed(void) {
    RCC_AHB1ENR |= (1u << 0); /* GPIOA clock; PA0 reset input */
    (void)RCC_AHB1ENR;
    return (GPIOA_IDR & 1u) != 0u;
}
void board_delay(volatile uint32_t count) {
    while (count--) __asm volatile("nop");
}
