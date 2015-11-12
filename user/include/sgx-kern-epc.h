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

#define SGX_KERNEL
#include <sgx.h>

#define EPC_ADDR       0x4fffc000

// linear address is in fact just addr of epc page (physical page)
static inline
void* epc_to_vaddr(epc_t *epc) {
    return epc;
}

typedef enum {
    FREE_PAGE = 0x0,
    SECS_PAGE = 0x1,
    TCS_PAGE  = 0x2,
    REG_PAGE  = 0x3,
    RESERVED  = 0x4
} epc_type_t;

typedef struct {
    int key;
    epc_type_t type;
} epc_info_t;


// exported
extern void init_epc(int nepc);

extern epc_t *get_epc(int key, epc_type_t pt);
extern epc_t *get_epc_region_beg(void);
extern epc_t *get_epc_region_end(void);
extern epc_t *alloc_epc_pages(int npages, int key);
extern epc_t *alloc_epc_page(int key);
extern void free_epc_pages(epc_t *epc);

extern void dbg_dump_epc(void);

extern int find_epc_type(void *addr);

extern void free_reserved_epc_pages(epc_t *epc);
