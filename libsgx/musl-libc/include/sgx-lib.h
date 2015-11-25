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

#define sgx_exit(ptr) {                                 \
    __asm__ __volatile__("movl %0, %%eax\n\t"           \
                         "movq %1, %%rbx\n\t"           \
                         ".byte 0x0F\n\t"               \
                         ".byte 0x01\n\t"               \
                         ".byte 0xd7\n\t"               \
                         :                              \
                         :"a"((uint32_t)ENCLU_EEXIT),   \
                          "b"((uint64_t)ptr));          \
}

#define sgx_report(tgtinfo, rptdata, output) {          \
    __asm__ __volatile__("movl %0, %%eax\n\t"           \
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

#define sgx_getkey(keyreq, output) {                    \
    __asm__ __volatile__("movl %0, %%eax\n\t"           \
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

extern int sgx_attest_target(struct sockaddr *quote_addr, socklen_t quote_addrlen, struct sockaddr *challenger_addr, socklen_t challenger_addrlen);
extern int sgx_intra_for_quoting(struct sockaddr *server_addr, socklen_t addrlen);
extern int sgx_remote(const struct sockaddr *target_addr, socklen_t addrlen);

extern int sgx_enclave_read(void *buf, int len);
extern int sgx_enclave_write(void *buf, int len);
