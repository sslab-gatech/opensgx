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

// An enclave test case for faulty enclave access.
// Faulty enclave access to other enclave region will raise segmentation fault.

#include "test.h"

void enclave_main()
{
    uint64_t ptr;
    asm("leaq (%%rip), %0;": "=r"(ptr));

    // 20 pages after here : this is an EPC region but not for this enclave
    *(char *)(ptr + 0x20000) = 1;

    sgx_exit(NULL);
}
