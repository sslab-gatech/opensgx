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

// Hello world enclave program.
// The simplest case which uses opensgx ABI.
// See sgx/user/sgxLib.c and sgx/user/sgx-user.c for detail.

#include "test.h"

struct test_meth_st {
    int (*func1) (void);
    int (*func2) (void);
};

static int test1(void);
static int test2(void);

typedef struct test_meth_st MY_METHOD;

MY_METHOD my_meth = {
    test1,
    test2
};

static int test1(void) {
    return 1;
}

static int test2(void) {
    return 2;
}

/* main operation. communicate with tor-gencert & tor process */
//int main(int argc, char *argv[])
void enclave_main()
{
    MY_METHOD *temp = &my_meth;

    int a = temp->func1();
    int b = temp->func2();
    sgx_printf("Result = %d, %d\n", a, b);

    sgx_exit(NULL);
}
