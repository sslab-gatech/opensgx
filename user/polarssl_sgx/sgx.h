#pragma once

#include <stddef.h>

extern void *sgx_malloc (int size);
extern void sgx_free (void *ptr);
extern void *sgx_memset (void *ptr, int value, size_t num);
extern void *sgx_memcpy (void *dest, const void *src, size_t size);
extern size_t sgx_strlen(const char *string);
extern int sgx_memcmp (const void *ptr1, const void *ptr2, size_t num);
extern int sgx_printf(const char *format, ...);

