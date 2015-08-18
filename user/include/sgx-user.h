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

extern tcs_t *test_init_enclave(void *base_addr, unsigned int entry_offset, unsigned int n_of_code_pages);
extern void exception_handler(void);

extern void sgx_enter(tcs_t *tcs, void (*aep)());
extern void sgx_resume(tcs_t *tcs, void (*aep)());
