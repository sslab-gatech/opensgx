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

// An enclave test case for sgx_realloc
// Usage of sgx_realloc is same as glibc realloc() function.
// See sgx/user/sgxLib.c and sgx/user/sgx-user.c for detail.

#include "test.h"

void enclave_main()
{
    char buf[] = "this is the message in a buffer assigned from malloc";
    char *test = malloc(50);
    memcpy(test, buf, 50);
    puts(test);

    test = realloc(test, 70);
    printf("%lx\n", (uint64_t)test);
    puts(test);

    test = realloc(test, 30);
    printf("%lx\n", (uint64_t)test);
    puts(test);

    free(test);

    sgx_exit(NULL);
}
