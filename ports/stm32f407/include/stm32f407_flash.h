#ifndef STM32F407_FLASH_H
#define STM32F407_FLASH_H
#include <stddef.h>
#include <stdint.h>
#include "iap/flash_if.h"

typedef struct {
    uint32_t start;
    uint32_t size;
} stm32_flash_sector_t;

/* Pure functions: safe for host-side boundary tests. */
int stm32f407_sector_info(unsigned sector, stm32_flash_sector_t *out);
int stm32f407_writable_range(uint32_t addr, size_t len);

/* STM32F407 registers, only valid when executing on the actual MCU. */
iap_flash_t stm32f407_flash_ops(void);
#endif
