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

#include <sgx-lib.h>

void enclave_main()
{
    int *a = malloc(20);
    printf("%lx\n", (uint64_t)a);
    int *b = malloc(4096*6);
    printf("%lx\n", (uint64_t)b);
    free(a);
    free(b);
    int *c = malloc(40);
    printf("%lx\n", (uint64_t)c);
    int *d = malloc(16);
    printf("%lx\n", (uint64_t)d);
    free(c);
    a = malloc(30);
    printf("%lx\n", (uint64_t)a);
    free(a);
    b = malloc(50);
    printf("%lx\n", (uint64_t)b);
    c = malloc(4096);
    printf("%lx\n", (uint64_t)c);
    d = malloc(40);
    printf("%lx\n", (uint64_t)d);
    int *e = malloc(20);
    printf("%lx\n", (uint64_t)e);

    sgx_exit(NULL);
}
