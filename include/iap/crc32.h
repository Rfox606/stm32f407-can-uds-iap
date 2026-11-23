#ifndef IAP_CRC32_H
#define IAP_CRC32_H
#include <stddef.h>
#include <stdint.h>
/* IEEE 802.3 reflected CRC-32, init/xorout 0xFFFFFFFF; matches Python zlib.crc32. */
uint32_t iap_crc32(const void *data, size_t len);
uint32_t iap_crc32_update(uint32_t state, const void *data, size_t len);
#endif
