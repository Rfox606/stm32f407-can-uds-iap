#ifndef IAP_FREESTANDING_STRING_H
#define IAP_FREESTANDING_STRING_H
#include <stddef.h>
/* This header is only on the freestanding ARM build include path.
 * The corresponding definitions live in runtime_mem.c. */
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int byte, size_t n);
int memcmp(const void *a, const void *b, size_t n);
#endif
