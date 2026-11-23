#include "stm32f407_flash.h"
#include "stm32f407_min.h"
#include "iap/config.h"
#include <stddef.h>
#include <stdint.h>

/* 3.3V assumed: PSIZE = x32 according to RM0090 (2.7~3.6V).
 * With a lower target voltage, this setting MUST be changed.
 * The limit is a finite polling safeguard, NOT an elapsed-time guarantee.
 */
#define FLASH_POLL_LIMIT (100000000u)
#define FLASH_KEY1 (0x45670123u)
#define FLASH_KEY2 (0xCDEF89ABu)

static int wait_idle(void) {
    uint32_t remaining = FLASH_POLL_LIMIT;
    while ((FLASH_SR & FLASH_SR_BSY) != 0u) {
        if (--remaining == 0u) return -1;
    }
    return (FLASH_SR & FLASH_SR_ERRORS) ? -1 : 0;
}
static void clear_status(void) {
    FLASH_SR = FLASH_SR_ERRORS | FLASH_SR_EOP; /* W1C; never RMW this register */
}
static int unlock_flash(void) {
    if ((FLASH_CR & FLASH_CR_LOCK) != 0u) {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
    return ((FLASH_CR & FLASH_CR_LOCK) == 0u) ? 0 : -1;
}
static void lock_flash(void) { FLASH_CR |= FLASH_CR_LOCK; }
static uint32_t save_and_disable_irq(void) {
    uint32_t state;
    __asm volatile("mrs %0, primask" : "=r"(state) :: "memory");
    stm_irq_disable();
    return state;
}
static void restore_irq(uint32_t state) {
    __asm volatile("msr primask, %0" :: "r"(state) : "memory");
}
static int hw_read(void *ctx, uint32_t addr, void *out, size_t len) {
    (void)ctx;
    if (!out || addr < IAP_FLASH_BASE ||
        (uint64_t)addr + len > (uint64_t)IAP_FLASH_BASE + IAP_FLASH_BYTES) return -1;
    const volatile uint8_t *src = (const volatile uint8_t *)(uintptr_t)addr;
    uint8_t *dst = (uint8_t *)out;
    for (size_t i = 0; i < len; ++i) dst[i] = src[i];
    return 0;
}
static int hw_erase(void *ctx, unsigned sector) {
    stm32_flash_sector_t info;
    (void)ctx;
    if (sector < 4u || stm32f407_sector_info(sector, &info) != 0) return -1;
    (void)info;
    uint32_t primask = save_and_disable_irq();
    if ((FLASH_SR & FLASH_SR_BSY) != 0u || unlock_flash() != 0) {
        restore_irq(primask); return -1;
    }
    clear_status();
    FLASH_CR = (FLASH_CR & ~(FLASH_CR_PSIZE_Msk | FLASH_CR_SNB_Msk | FLASH_CR_PG)) |
               FLASH_CR_PSIZE_X32 | FLASH_CR_SER | ((uint32_t)sector << FLASH_CR_SNB_Pos);
    FLASH_CR |= FLASH_CR_STRT;
    int ok = wait_idle();
    FLASH_CR &= ~(FLASH_CR_SER | FLASH_CR_SNB_Msk);
    if (ok == 0) clear_status();
    lock_flash();
    restore_irq(primask);
    return ok;
}
static int hw_program(void *ctx, uint32_t addr, const void *data, size_t len) {
    (void)ctx;
    if (!data || (addr & 3u) || (len & 3u) || !stm32f407_writable_range(addr,len)) return -1;
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t primask = save_and_disable_irq();
    if ((FLASH_SR & FLASH_SR_BSY) != 0u || unlock_flash() != 0) {
        restore_irq(primask); return -1;
    }
    clear_status();
    FLASH_CR = (FLASH_CR & ~(FLASH_CR_PSIZE_Msk | FLASH_CR_SER | FLASH_CR_SNB_Msk)) |
               FLASH_CR_PSIZE_X32 | FLASH_CR_PG;
    int ok = 0;
    for (size_t i = 0; i < len; i += 4u) {
        uint32_t word = (uint32_t)bytes[i] | ((uint32_t)bytes[i+1u] << 8) |
                        ((uint32_t)bytes[i+2u] << 16) | ((uint32_t)bytes[i+3u] << 24);
        volatile uint32_t *dest = (volatile uint32_t *)(uintptr_t)(addr + (uint32_t)i);
        if ((*dest & word) != word) { ok = -1; break; } /* cannot program 0 -> 1 */
        *dest = word;
        if (wait_idle() != 0 || *dest != word) { ok = -1; break; }
        clear_status();
    }
    FLASH_CR &= ~FLASH_CR_PG;
    lock_flash();
    restore_irq(primask);
    return ok;
}
iap_flash_t stm32f407_flash_ops(void) {
    iap_flash_t ops = { 0, hw_read, hw_erase, hw_program };
    return ops;
}
