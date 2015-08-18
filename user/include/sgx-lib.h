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

#include <sgx.h>
#include <sgx-user.h>
#include <sgx-kern.h>
#include <sgx-trampoline.h>
#include <stdarg.h>

#include <netinet/in.h>

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
/*
static unsigned long cur_heap_ptr = 0x0;
static unsigned long heap_end = 0x0;
static int has_initialized = 0;
static void *managed_memory_start = 0;
static int g_total_chunk = 0;
*/
extern void _enclu(enclu_cmd_t leaf, uint64_t rbx, uint64_t rcx, uint64_t rdx,
           out_regs_t *out_regs);

extern void *sgx_realloc(void *ptr, size_t size);
extern void *sgx_memalign(size_t align, size_t size);
extern void sgx_puts(char buf[]);
extern time_t sgx_time(time_t *t);
extern void *sgx_memcpy (void *dest, const void *src, size_t size);
extern void *sgx_memmove(void *dest, const void *src, size_t size);

// I/O trampoline functions
extern ssize_t sgx_write(int fd, const void *buf, size_t count);
extern ssize_t sgx_read(int fd, void *buf, size_t count);
extern int sgx_close(int fd);

// Socket trampoline functions
extern int sgx_socket(int domain, int type, int protocol);
extern int sgx_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern int sgx_listen(int sockfd, int backlog);
extern int sgx_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int sgx_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern ssize_t sgx_send(int fd, const void *buf, size_t len, int flag);
extern ssize_t sgx_recv(int fd, void *buf, size_t len, int flag);

extern int sgx_printf(const char *format, ...);
extern void sgx_putchar(char c);
extern void sgx_print_hex(unsigned long addr);
extern void *sgx_malloc(size_t numbytes);
extern void sgx_free(void *ptr);
extern void sgx_malloc_init();

extern int sgx_tolower(int c);
extern int sgx_toupper(int c);
extern int sgx_islower(int c);
extern int sgx_isupper(int c);
extern int sgx_isdigit(int c);
extern int sgx_isspace(int c);
extern int sgx_isalnum(int c);
extern int sgx_isxdigit(int c);

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

extern int sgx_attest_target(struct sockaddr *quote_addr, socklen_t quote_addrlen, struct sockaddr *challenger_addr, socklen_t challenger_addrlen);
extern int sgx_intra_for_quoting(struct sockaddr *server_addr, socklen_t addrlen);
extern int sgx_remote(const struct sockaddr *target_addr, socklen_t addrlen);
