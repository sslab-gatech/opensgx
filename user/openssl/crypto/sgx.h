#pragma once

#include <stddef.h>
#include <time.h>

#define ENCLAVE_OFFSET 0x20004000

extern void *sgx_malloc (int size);
extern void sgx_free (void *ptr);
extern void *sgx_realloc (void *ptr, size_t size);
extern void *sgx_memset (void *ptr, int value, size_t num);
extern void *sgx_memcpy (void *dest, const void *src, size_t size);
extern int sgx_strcmp (const char *str1, const char *str2);
extern size_t sgx_strlen(const char *string);
extern int sgx_memcmp (const void *ptr1, const void *ptr2, size_t num);
extern void sgx_puts (char buf[]);


extern int sgx_time(time_t *time);
extern void *sgx_memchr (const void *s, int c, size_t n);
