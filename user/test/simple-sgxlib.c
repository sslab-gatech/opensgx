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

// An enclave test case for sgx library.
// Memory operations inside the enclave should use own sgx library functions.
// Opensgx supports several library functions such as
// memset, memcpy, memcmp and so on.
// Usage of these functions are same as glibc functions.
// See sgx/user/sgxLib.c for detail.

#include "test.h"
#define MATCH "MATCH\n"
#define UNMATCH "UNMATCH\n"

void enclave_main()
{
    char buf1[] = "hello world\n";
    char buf2[] = "hello world\n";

    // sgx_memcpy test
    memcpy(buf1, buf2, strlen(buf1));

    // sgx_puts & sgx_strlen test
    puts(buf1);

    // sgx_strcmp test
    if(!(strcmp(buf1, buf2)))
        puts(MATCH);
    else
        puts(UNMATCH);

    // sgx_memcmp test
    /*if(!(sgx_memcmp(buf1, buf2, 5)))
        sgx_puts(MATCH);
    else
        sgx_puts(UNMATCH);*/

    // sgx_memset test
    memset(buf1, 'A', 5);
    puts(buf1);

    // sgx_strcmp test
    if(!(strcmp(buf1, buf2)))
        puts(MATCH);
    else
        puts(UNMATCH);

    sgx_exit(NULL);
}
