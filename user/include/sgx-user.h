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

int cur_keid;

// user-level libs
extern void enclu(enclu_cmd_t leaf, uint64_t rbx, uint64_t rcx, uint64_t rdx,
                  out_regs_t* out_regs);
tcs_t *init_enclave(void *base_addr, unsigned int entry_offset, unsigned int n_of_pages, char *conf);

extern void exception_handler(void);

extern void sgx_enter(tcs_t *tcs, void (*aep)());
extern void sgx_resume(tcs_t *tcs, void (*aep)());

extern int sgx_host_read(void *buf, int len);
extern int sgx_host_write(void *buf, int len);

extern void collecting_enclu_stat(void);

/* Macros to define user-side enclave calls with different argument
 * numbers */
#define ENCCALL1(name, type1)                                   \
void name(tcs_t *tcs, void (*aep)(), type1 arg1) {              \
        register type1 rdi asm("rdi") __attribute((unused));    \
        rdi = arg1;                                             \
        asm volatile(                                           \
                ".byte 0x0F\n\t"                                \
                ".byte 0x01\n\t"                                \
                ".byte 0xd7\n\t"                                \
        : "=c"(aep)                                             \
                : "a"((uint32_t)ENCLU_EENTER),                  \
          "b"(tcs),                                             \
          "c"(aep),                                             \
          "r"(rdi)                                              \
                : "memory", "r11", "cc"                         \
        );                                                      \
}

#define ENCCALL2(name, type1, type2)                            \
void name(tcs_t *tcs, void (*aep)(), type1 arg1, type2 arg2) {  \
        register type1 rdi asm("rdi") __attribute((unused));    \
        register type2 rsi asm("rsi") __attribute((unused));    \
        rdi = arg1;                                             \
        rsi = arg2;                                             \
        asm volatile(                                           \
                ".byte 0x0F\n\t"                                \
                ".byte 0x01\n\t"                                \
                ".byte 0xd7\n\t"                                \
        : "=c"(aep)                                             \
                : "a"((uint32_t)ENCLU_EENTER),                  \
          "b"(tcs),                                             \
          "c"(aep),                                             \
          "r"(rdi),                                             \
          "r"(rsi)                                              \
                : "memory", "r11", "cc"                         \
        );                                                      \
}
