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

#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <err.h>
#include <assert.h>
#include <sgx.h>
#include <sys/socket.h>

//about a page
#define STUB_ADDR       0x80800000
#define HEAP_ADDR       0x80900000
#define SGXLIB_MAX_ARG  512

typedef enum {
    FUNC_UNSET,
    FUNC_PUTS,
    FUNC_PUTCHAR,

    FUNC_MALLOC,
    FUNC_FREE,

    FUNC_SYSCALL,
    FUNC_READ,
    FUNC_WRITE,
    FUNC_CLOSE,

    FUNC_TIME,
    FUNC_SOCKET,
    FUNC_BIND,
    FUNC_LISTEN,
    FUNC_ACCEPT,
    FUNC_CONNECT,
    FUNC_SEND,
    FUNC_RECV
    // ...
} fcode_t;

typedef enum {
    MALLOC_UNSET,
    MALLOC_INIT,
    REQUEST_EAUG,
} mcode_t;


typedef struct sgx_stub_info {
    int   abi;
    void  *trampoline;
    tcs_t *tcs;

    // in : from non-enclave to enclave
    unsigned long heap_beg;
    unsigned long heap_end;
    unsigned long pending_page;
    int  ret;
    char in_data1[SGXLIB_MAX_ARG];
    char in_data2[SGXLIB_MAX_ARG];
    int  in_arg1;
    int  in_arg2;
    unsigned long in_arg3;

    // out : from enclave to non-enclave
    fcode_t fcode;
    mcode_t mcode;
    unsigned long *addr;
    int  out_arg1;
    int  out_arg2;
    int  out_arg3;
    char out_data1[SGXLIB_MAX_ARG];
    char out_data2[SGXLIB_MAX_ARG];
    char out_data3[SGXLIB_MAX_ARG];
} sgx_stub_info;

extern void execute_code(void);
extern void sgx_trampoline(void);
extern int sgx_init(void);
