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

// An enclave test case for using heap
// An enclave program of Opensgx requests pre-allocated EPC heap region
// using sgx_malloc.
// Usage of sgx_malloc is same as glibc malloc() function.
// See sgx/user/sgxLib.c and sgx/user/sgx-user.c for detail.

#include "test.h"

void enclave_main()
{
    int *a = sgx_malloc(20);
    sgx_print_hex(a);
    int *b = sgx_malloc(4096*6);
    sgx_print_hex(b);
    sgx_free(a);
    sgx_free(b);
    int *c = sgx_malloc(40);
    sgx_print_hex(c);
    int *d = sgx_malloc(16);
    sgx_print_hex(d);
    sgx_free(c);
    a = sgx_malloc(30);
    sgx_print_hex(a);
    sgx_free(a);
    b = sgx_malloc(50);
    sgx_print_hex(b);
    c = sgx_malloc(4096);
    sgx_print_hex(c);
    d = sgx_malloc(40);
    sgx_print_hex(d);
    int *e = sgx_malloc(20);
    sgx_print_hex(e);
}
