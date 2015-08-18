#pragma once

#include <sgx.h>
#include <sgx-user.h>
#include <sgx-kern.h>

extern void sgx_debug(char msg[]);
extern void sgx_print_bytes(char *s, size_t n);
extern struct tm *sgx_gmtime(const time_t *timep);

//extern int sgx_memcmp(const void *ptr1, const void *ptr2, size_t n);
