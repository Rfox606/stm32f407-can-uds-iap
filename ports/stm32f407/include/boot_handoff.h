#ifndef BOOT_HANDOFF_H
#define BOOT_HANDOFF_H
#include <stdint.h>
/* Retained SRAM mailbox. Both linker scripts reserve the final 64 bytes of
 * ordinary SRAM; the top-of-stack is 0x2001FFC0, mailbox at 0x2001FFF0.
 * This survives SYSRESETREQ with VDD present, NOT power loss.
 */
#define BOOT_HANDOFF_MAGIC 0xB007F407u
#define BOOT_HANDOFF_ADDRESS 0x2001FFF0u
#define BOOT_INITIAL_MSP 0x2001FFC0u
static inline int boot_handoff_marker_valid(uint32_t reset_flags,
                                             uint32_t word, uint32_t inverted) {
    return (reset_flags & (1u<<28)) != 0u &&
           word == BOOT_HANDOFF_MAGIC && inverted == ~BOOT_HANDOFF_MAGIC;
}
/* APP uses this only AFTER sending the positive CAN response. Never returns. */
void boot_handoff_request(void) __attribute__((noreturn));
/* Called at beginning of Bootloader main; clears mailbox and RCC reset flags. */
int boot_handoff_consume(void);
#endif
