/*
 *  Copyright (C) 2015, OpenSGX team, Georgia Tech & KAIST, All Rights Reserved
 *
 *  This file is part of OpenSGX (https://github.com/sslab-gatech/opensgx).
 *
 *  OpenSGX is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenSGX is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenSGX.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <sgx-shared.h>
#include <stdarg.h>

#include <netinet/in.h>

#ifndef NULL
#define NULL 0
#endif

#define sgx_exit(ptr) {                         \
    asm volatile("movl %0, %%eax\n\t"           \
                 "movq %1, %%rbx\n\t"           \
                 ".byte 0x0F\n\t"               \
                 ".byte 0x01\n\t"               \
                 ".byte 0xd7\n\t"               \
                 :                              \
                 :"a"((uint32_t)ENCLU_EEXIT),   \
                  "b"((uint64_t)ptr));          \
}

#define sgx_report(tgtinfo, rptdata, output) {  \
    asm volatile("movl %0, %%eax\n\t"           \
                 "movq %1, %%rbx\n\t"           \
                 "movq %2, %%rcx\n\t"           \
                 "movq %3, %%rdx\n\t"           \
                 ".byte 0x0F\n\t"               \
                 ".byte 0x01\n\t"               \
                 ".byte 0xd7\n\t"               \
                 :                              \
                 :"a"((uint32_t)ENCLU_EREPORT), \
                  "b"((uint64_t)tgtinfo),       \
                  "c"((uint64_t)rptdata),       \
                  "d"((uint64_t)output));       \
}

#define sgx_getkey(keyreq, output) {            \
    asm volatile("movl %0, %%eax\n\t"           \
                 "movq %1, %%rbx\n\t"           \
                 "movq %2, %%rcx\n\t"           \
                 ".byte 0x0F\n\t"               \
                 ".byte 0x01\n\t"               \
                 ".byte 0xd7\n\t"               \
                 :                              \
                 :"a"((uint32_t)ENCLU_EGETKEY), \
                  "b"((uint64_t)keyreq),        \
                  "c"((uint64_t)output));       \
}

#define sgx_htons(A) ((((uint16_t)(A) & 0xff00) >> 8) | \
                      (((uint16_t)(A) & 0x00ff) << 8))

struct mem_control_block {
    int is_available;
    int size;
};

extern void sgx_puts(char buf[]);
extern void *realloc(void *ptr, size_t size);
extern void *memalign(size_t align, size_t size);
extern time_t time(time_t *t);
extern void *memcpy (void *dest, const void *src, size_t size);
extern void *memmove(void *dest, const void *src, size_t size);

// I/O trampoline functions
extern ssize_t write(int fd, const void *buf, size_t count);
extern ssize_t read(int fd, void *buf, size_t count);
extern int close(int fd);

// Socket trampoline functions
extern int socket(int domain, int type, int protocol);
extern int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern int listen(int sockfd, int backlog);
extern int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern ssize_t send(int fd, const void *buf, size_t len, int flag);
extern ssize_t recv(int fd, void *buf, size_t len, int flag);

extern int sgx_printf(const char *format, ...);
extern void sgx_putchar(char c);
extern void sgx_print_hex(unsigned long addr);
extern void *malloc(size_t numbytes);
extern void free(void *ptr);
extern void sgx_malloc_init();

extern int tolower(int c);
extern int toupper(int c);
extern int islower(int c);
extern int isupper(int c);
extern int isdigit(int c);
extern int isspace(int c);
extern int isalnum(int c);
extern int isxdigit(int c);

extern void *memchr(const void *s, int c, size_t n);
extern char *strchr (const char *s, int c_in);
extern int inet_pton(int af, const char *src, void *dst);
extern void qsort(void *base, size_t num, size_t size, int (*cmp)(const void *, const void *));
extern int strcmp(const char *p1, const char *p2);
extern int strncmp(const char *s1, const char *s2, size_t n);
extern int strcasecmp (const char *s1, const char *s2);
extern int strncasecmp (const char *s1, const char *s2, size_t n);
extern char *strcpy(char *dest, const char *src);
extern char *strncpy(char *s1, const char *s2, size_t n);
extern void *memset (void *ptr, int value, size_t num);
extern size_t strlen(const char *string);
extern int strcmp (const char *str1, const char *str2);
extern int memcmp (const void *ptr1, const void *ptr2, size_t num);
extern size_t strnlen (const char *str, size_t maxlen);
extern char * strcat (char *dest, const char *src);
extern char * strncat (char *s1, const char *s2, size_t n);

extern int sgx_attest_target(struct sockaddr *quote_addr, socklen_t quote_addrlen, struct sockaddr *challenger_addr, socklen_t challenger_addrlen);
extern int sgx_intra_for_quoting(struct sockaddr *server_addr, socklen_t addrlen);
extern int sgx_remote(const struct sockaddr *target_addr, socklen_t addrlen);

extern int sgx_enclave_read(void *buf, int len);
extern int sgx_enclave_write(void *buf, int len);
