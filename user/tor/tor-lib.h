#pragma once

#include <sgx.h>
#include <sgx-user.h>
#include <sgx-kern.h>

extern int sgx_mkdir(char pathname[], size_t size, mode_t mode);
extern int sgx_mknod(char pathname[], size_t size, mode_t mode, dev_t dev);
extern int sgx_open(char pathname[], size_t size, int flag);

// Note that this is not regular snprintf, specialized one for Tor
extern int sgx_snprintf(char dst[], int dst_size, char buf1[], size_t size1, 
                        char buf2[], size_t size2, int arg1);

//extern int sgx_strncasecmp(const char *s1, const char *s2, size_t n);
//extern int sgx_strncmp(const char *s1, const char *s2, size_t n);
//extern void *sgx_memmove(void *dest, const void *src, size_t size);
//extern int sgx_isspace(int c);
//extern int sgx_tolower(int c);
//extern int sgx_isdigit(int c);
extern void sgx_debug(char msg[]);
extern struct tm *sgx_localtime_r(const time_t *timep, struct tm *result);
extern struct tm *sgx_gmtime(const time_t *timep);
extern size_t sgx_strftime(char *s, size_t max, const char *format, const struct tm *tm);
extern time_t sgx_mktime(struct tm *tm);
extern void sgx_print_bytes(char *s, size_t n);
