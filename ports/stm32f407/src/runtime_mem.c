/* Freestanding memory primitives, for the M1 no-libc embedded images. */
#include <stddef.h>
void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}
void *memset(void *dst, int byte, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    for (size_t i = 0; i < n; ++i) d[i] = (unsigned char)byte;
    return dst;
}
int memcmp(const void *lhs, const void *rhs, size_t n) {
    const unsigned char *a = (const unsigned char *)lhs;
    const unsigned char *b = (const unsigned char *)rhs;
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    }
    return 0;
}
