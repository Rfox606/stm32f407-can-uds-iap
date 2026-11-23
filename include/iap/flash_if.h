#ifndef IAP_FLASH_IF_H
#define IAP_FLASH_IF_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    void *ctx;
    int (*read)(void *ctx, uint32_t addr, void *out, size_t len);
    int (*erase_sector)(void *ctx, unsigned sector);
    /* addr and len MUST be 4-byte aligned; only transitions 1->0 are valid. */
    int (*program)(void *ctx, uint32_t addr, const void *data, size_t len);
} iap_flash_t;

#endif
