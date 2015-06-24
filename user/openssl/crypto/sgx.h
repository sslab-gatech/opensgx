#pragma once

#define ENCLAVE_OFFSET 0x20004000

extern void *sgx_malloc (int size);
extern void sgx_free (void *ptr);
extern void *sgx_realloc (void *ptr, size_t size);
extern void *sgx_memset (void *ptr, int value, size_t num);
extern void *sgx_memcpy (void *dest, const void *src, size_t size);

extern int sgx_time(time_t *time);
