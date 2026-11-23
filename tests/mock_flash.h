#ifndef MOCK_FLASH_H
#define MOCK_FLASH_H
#include "iap/flash_if.h"
#include "iap/config.h"
#include <stdint.h>

typedef struct {
    uint8_t data[IAP_FLASH_BYTES];
    uint32_t fail_program_addr; /* 0 = disabled; fail before program at address */
    unsigned erase_calls;
    unsigned program_calls;
} mock_flash_t;
void mock_flash_reset(mock_flash_t *mock);
iap_flash_t mock_flash_ops(mock_flash_t *mock);
#endif
