#include "boot_handoff.h"
#include "bxcan.h"
#include "stm32f407_min.h"
#include <stdint.h>

#define RCC_CSR REG32(0x40023874u)
#define RCC_RMVF (1u<<24)
static volatile uint32_t *const mailbox=(volatile uint32_t *)(uintptr_t)BOOT_HANDOFF_ADDRESS;
void boot_handoff_request(void) {
    /* Mailbox is in SRAM explicitly excluded from BOTH linker-managed RAM
     * regions. It will not be zeroed by startup or used by the stack. */
    mailbox[0]=BOOT_HANDOFF_MAGIC;
    mailbox[1]=~BOOT_HANDOFF_MAGIC;
    stm_dsb();
    bxcan_system_reset();
}
int boot_handoff_consume(void) {
    uint32_t flags=RCC_CSR;
    uint32_t word=mailbox[0], inverted=mailbox[1];
    int requested=boot_handoff_marker_valid(flags,word,inverted);
    mailbox[0]=0u;mailbox[1]=0u;  /* once only; clear even invalid content */
    RCC_CSR |= RCC_RMVF;         /* Clear sticky reset-origin flags */
    stm_dsb();
    return requested;
}
