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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// polarssl related headers
#include <polarssl/ctr_drbg.h>
#include <polarssl/entropy.h>
#include <polarssl/rsa.h>
#include <polarssl/sha1.h>
#include <polarssl/sha256.h>
#include <polarssl/aes_cmac128.h>
#include <polarssl/dhm.h>

#define OPENSGX_ABI_VERSION 1
#define SGX_USERLIB
#include "../../qemu/target-i386/sgx.h"

#include "../../qemu/target-i386/crypto.h"

typedef struct {
    uint32_t oeax;
    uint64_t orbx;
    uint64_t orcx;
    uint64_t ordx;
} out_regs_t;

// round size to pages
static inline
int to_npages(int size) {
    if (size == 0)
        return 0;
    return (size - 1) / PAGE_SIZE + 1;
}

// OS resource management for enclave
#define MAX_ENCLAVES 16

typedef struct {
    unsigned int mode_switch;
    unsigned int tlbflush_n;

    unsigned int encls_n;
    unsigned int ecreate_n;
    unsigned int eadd_n;
    unsigned int eextend_n;
    unsigned int einit_n;
    unsigned int eaug_n;
 
    unsigned int enclu_n;
    unsigned int eenter_n;
    unsigned int eresume_n;
    unsigned int eexit_n;
    unsigned int egetkey_n;
    unsigned int ereport_n;
    unsigned int eaccept_n;
} qstat_t;

typedef struct {
    int keid;
    uint64_t enclave;
    tcs_t *tcs;
    epc_t *secs;
    // XXX. stats
    unsigned int kin_n;
    unsigned int kout_n;
    unsigned long prealloc_ssa;
    unsigned long prealloc_stack;
    unsigned long prealloc_heap;
    unsigned long augged_heap;

    qstat_t qstat;
} keid_t;
