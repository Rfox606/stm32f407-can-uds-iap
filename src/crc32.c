#include "iap/crc32.h"
#include <stdint.h>

uint32_t iap_crc32_update(uint32_t state, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    size_t i;
    while (len--) {
        state ^= (uint32_t)*p++;
        for (i = 0; i < 8u; ++i) {
            state = (state >> 1) ^ ((state & 1u) ? 0xEDB88320u : 0u);
        }
    }
    return state;
}

uint32_t iap_crc32(const void *data, size_t len) {
    return iap_crc32_update(0xFFFFFFFFu, data, len) ^ 0xFFFFFFFFu;
}
