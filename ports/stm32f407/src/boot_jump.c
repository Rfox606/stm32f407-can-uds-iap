#include "boot_jump.h"
#include "stm32f407_min.h"
#include "iap/config.h"
#include <stdint.h>

/* No C stack operations after writing MSP. Naked final transfer avoids
 * a compiler-generated epilogue referring to the just-replaced stack.
 */
__attribute__((naked,noreturn)) static void transfer_msp_reset(uint32_t sp __attribute__((unused)),
                                                               uint32_t reset __attribute__((unused))) {
    __asm volatile(
        "msr msp, r0\n"
        "bx r1\n"
    );
}
void boot_jump_to_app(void) {
    const uint32_t *vectors = (const uint32_t *)(uintptr_t)IAP_APP_BASE;
    uint32_t sp = vectors[0];
    uint32_t reset = vectors[1];
    stm_irq_disable();
    SYST_CSR = 0u; SYST_RVR = 0u; SYST_CVR = 0u;
    /* STM32F407 has fewer than 96 peripheral IRQs. Clear all 8 NVIC words
     * conservatively; no HAL callbacks or active DMA remain in M1. */
    for (unsigned i = 0; i < 8u; ++i) {
        NVIC_ICER(i) = 0xFFFFFFFFu;
        NVIC_ICPR(i) = 0xFFFFFFFFu;
    }
    SCB_ICSR = SCB_ICSR_PENDSTCLR | SCB_ICSR_PENDSVCLR;
    stm_dsb();
    SCB_VTOR = IAP_APP_BASE;
    stm_dsb(); stm_isb();
    /* PRIMASK remains disabled; APP Reset_Handler enables interrupts after init. */
    transfer_msp_reset(sp, reset);
}
