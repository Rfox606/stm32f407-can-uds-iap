#include "stm32f407_min.h"
/* M1 deliberately uses the 16MHz HSI reset clock; no PLL assumptions. */
void board_clock_init(void) {
    /* Reset already selected HSI; explicitly set vector base in startup. */
    SCB_CPACR |= (0xFu << 20); /* Enable Cortex-M4F CP10/CP11 before hard-float APP code. */
    stm_dsb();
    stm_isb();
}
