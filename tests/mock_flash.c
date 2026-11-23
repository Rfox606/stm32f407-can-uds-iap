#include "mock_flash.h"
#include <stddef.h>
#include <string.h>

static const uint32_t starts[12] = {
    0x08000000u,0x08004000u,0x08008000u,0x0800C000u,
    0x08010000u,0x08020000u,0x08040000u,0x08060000u,
    0x08080000u,0x080A0000u,0x080C0000u,0x080E0000u
};
static int good_range(uint32_t addr, size_t len) {
    return addr >= IAP_FLASH_BASE && (uint64_t)addr + len <=
           (uint64_t)IAP_FLASH_BASE + IAP_FLASH_BYTES;
}
static int mem_read(void *ctx, uint32_t addr, void *out, size_t len) {
    mock_flash_t *m = (mock_flash_t *)ctx;
    if (!m || !out || !good_range(addr,len)) return -1;
    memcpy(out, m->data + (addr - IAP_FLASH_BASE), len);
    return 0;
}
static int mem_erase(void *ctx, unsigned sector) {
    mock_flash_t *m = (mock_flash_t *)ctx;
    uint32_t start, end;
    if (!m || sector < 4u || sector > 11u) return -1; /* Protect boot sector 0..3 */
    start = starts[sector];
    end = sector == 11u ? IAP_FLASH_BASE+IAP_FLASH_BYTES : starts[sector+1u];
    memset(m->data+(start-IAP_FLASH_BASE), 0xFF, (size_t)(end-start));
    ++m->erase_calls;
    return 0;
}
static int mem_program(void *ctx, uint32_t addr, const void *data, size_t len) {
    mock_flash_t *m = (mock_flash_t *)ctx;
    const uint8_t *p = (const uint8_t *)data;
    size_t i;
    if (!m || !p || !good_range(addr,len) || (addr&3u) || (len&3u)) return -1;
    if (addr < IAP_META_BASE) return -1;
    if (m->fail_program_addr != 0u && addr == m->fail_program_addr) return -1;
    for (i=0; i<len; ++i) {
        uint8_t old = m->data[addr-IAP_FLASH_BASE+i];
        if ((old & p[i]) != p[i]) return -1;
    }
    for (i=0; i<len; ++i) m->data[addr-IAP_FLASH_BASE+i] &= p[i];
    ++m->program_calls;
    return 0;
}
void mock_flash_reset(mock_flash_t *m) {
    if (!m) return;
    memset(m->data,0xFF,sizeof(m->data));
    m->fail_program_addr=0u;
    m->erase_calls=0u;
    m->program_calls=0u;
}
iap_flash_t mock_flash_ops(mock_flash_t *m) {
    iap_flash_t f={m,mem_read,mem_erase,mem_program};
    return f;
}
