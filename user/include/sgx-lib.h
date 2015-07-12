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
#include <stdarg.h>

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

extern unsigned long cur_heap_ptr;
extern unsigned long heap_end;

extern void sgx_print_hex(unsigned long addr);
extern void _enclu(enclu_cmd_t leaf, uint64_t rbx, uint64_t rcx, uint64_t rdx,
           out_regs_t *out_regs);
extern void *sgx_malloc(int size);
extern void *sgx_realloc(void *ptr, size_t size);
extern void *sgx_memalign(size_t align, size_t size);
extern void sgx_free(void *ptr);
extern void sgx_puts(char buf[]);
extern size_t sgx_strlen(const char *string);
extern int sgx_strcmp (const char *str1, const char *str2);
extern int sgx_memcmp (const void *ptr1, const void *ptr2, size_t num);
extern void *sgx_memset (void *ptr, int value, size_t num);
extern void *sgx_memcpy (void *dest, const void *src, size_t size);
extern int sgx_recv(const char *port, const char *buf);
extern int sgx_send(const char *ip, const char *port, const void *msg, size_t length);
extern int sgx_socket();
extern int sgx_bind(int sockfd, int port);
extern int sgx_listen(int sockfd);
extern int sgx_accept(int sockfd);
extern int sgx_close(int fd);
extern void sgx_close_sock(void);
extern int sgx_printf(const char *format, ...);
extern void sgx_putchar(char c);
