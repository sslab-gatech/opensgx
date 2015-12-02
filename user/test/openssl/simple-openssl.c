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

// test openssl api

#include "../test.h"
#include <openssl/bn.h>
#include <openssl/rsa.h>

BIGNUM *bn = NULL;
RSA *rsa = NULL;

void enclave_main()
{
    // Bignum allocation for global variable
    bn = BN_new();
    printf("%x\n", (unsigned long)bn);

    char *test = malloc(16);
    printf("%x\n", (unsigned long)test);

    // Bignum allocation for local variable
    BIGNUM *bn2 = BN_new();
    printf("%x\n", (unsigned long)bn2);

    char *test2 = malloc(16);
    printf("%x\n", (unsigned long)test2);

    // RSA allocation for global variable
    rsa = RSA_new();
    printf("%x\n", (unsigned long)rsa);

    // RSA allocation for local variable
    RSA *rsa2 = RSA_new();
    printf("%x\n", (unsigned long)rsa2);

    sgx_exit(NULL);
}
