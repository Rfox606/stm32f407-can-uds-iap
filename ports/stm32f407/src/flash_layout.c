#include "stm32f407_flash.h"
#include "iap/config.h"

/* RM0090: 4x16KiB, 1x64KiB, 7x128KiB. */
static const stm32_flash_sector_t sectors[12] = {
    {0x08000000u,0x4000u}, {0x08004000u,0x4000u},
    {0x08008000u,0x4000u}, {0x0800C000u,0x4000u},
    {0x08010000u,0x10000u}, {0x08020000u,0x20000u},
    {0x08040000u,0x20000u}, {0x08060000u,0x20000u},
    {0x08080000u,0x20000u}, {0x080A0000u,0x20000u},
    {0x080C0000u,0x20000u}, {0x080E0000u,0x20000u}
};
int stm32f407_sector_info(unsigned sector, stm32_flash_sector_t *out) {
    if (sector >= 12u || !out) return -1;
    *out = sectors[sector];
    return 0;
}
int stm32f407_writable_range(uint32_t addr, size_t len) {
    uint64_t end = (uint64_t)addr + (uint64_t)len;
    if (len == 0u || end > (uint64_t)IAP_FLASH_BASE + IAP_FLASH_BYTES) return 0;
    /* Metadata (sector 4) + APP (sector 5..11). Boot sectors 0..3 protected. */
    return addr >= IAP_META_BASE;
}
