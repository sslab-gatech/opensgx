#pragma once

#include <stddef.h>
#include <time.h>
#include <unistd.h>

#define ENCLAVE_OFFSET 0x0

int sgx_errno;

extern void *sgx_malloc (size_t numbytes);
extern void sgx_free (void *ptr);
extern void *sgx_realloc (void *ptr, size_t size);
extern void *sgx_memcpy (void *dest, const void *src, size_t size);
extern void *sgx_memmove(void *dest, const void *src, size_t size);

extern void sgx_puts (char buf[]);

extern time_t sgx_time(time_t *time);
extern void *sgx_memchr (const void *s, int c, size_t n);

extern void *sgx_memchr(const void *s, int c, size_t n);
extern char *sgx_strchr (const char *s, int c_in);
extern int sgx_inet_pton(int af, const char *src, void *dst);
extern void sgx_qsort(void *base, size_t num, size_t size, int (*cmp)(const void *, const void *));
extern int sgx_strcmp(const char *p1, const char *p2);
extern int sgx_strncmp(const char *s1, const char *s2, size_t n);
extern int sgx_strcasecmp (const char *s1, const char *s2);
extern int sgx_strncasecmp (const char *s1, const char *s2, size_t n);
extern char *sgx_strcpy(char *dest, const char *src);
extern char *sgx_strncpy(char *s1, const char *s2, size_t n);
extern void *sgx_memset (void *ptr, int value, size_t num);
extern size_t sgx_strlen(const char *string);
extern int sgx_strcmp (const char *str1, const char *str2);
extern int sgx_memcmp (const void *ptr1, const void *ptr2, size_t num);
extern size_t sgx_strnlen (const char *str, size_t maxlen);
extern char * sgx_strcat (char *dest, const char *src);
extern char * sgx_strncat (char *s1, const char *s2, size_t n);

extern int sgx_tolower(int c);
extern int sgx_toupper(int c);
extern int sgx_islower(int c);
extern int sgx_isupper(int c);
extern int sgx_isdigit(int c);
extern int sgx_isspace(int c);
extern int sgx_isalnum(int c);
extern int sgx_isxdigit(int c);

extern void sgx_debug(char msg[]);
extern void sgx_print_bytes(char *s, size_t n);
extern int sgx_printf(const char *format, ...);

extern ssize_t sgx_send(int fd, const void *buf, size_t len, int flag);
extern ssize_t sgx_recv(int fd, void *buf, size_t len, int flag);

extern ssize_t sgx_write(int fd, const void *buf, size_t count);
extern ssize_t sgx_read(int fd, void *buf, size_t count);
extern int sgx_close(int fd);
