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

typedef enum {
    MT_SECS,
    MT_TCS,
    MT_TLS,
    MT_CODE,
    MT_SSA,
    MT_STACK,
    MT_HEAP,
} mem_type_t;


extern bool sys_sgx_init(void);
extern int sys_create_enclave(void *base, unsigned int code_pages,
                              tcs_t *tcs, sigstruct_t *sig, einittoken_t *token,
                              int intel_flag);
extern int sys_stat_enclave(int keid, keid_t *stat);
extern unsigned long get_epc_heap_beg();
extern unsigned long get_epc_heap_end();
extern unsigned long sys_add_epc(int keid);

// For unit test
void test_ecreate(pageinfo_t *pageinfo, epc_t *epc);
int test_einit(uint64_t sigstruct, uint64_t secs, uint64_t einittoken);
void test_eadd(pageinfo_t *pageinfo, epc_t *epc);
void test_eextend(uint64_t pageChunk);
void test_eaug(pageinfo_t *pageinfo, epc_t *epc);

// XXX: may need these during testing
int test_alloc_keid(void);

