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

#include <sgx-lib.h>
#include <sgx.h>

#include "tp-lib.h"

#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

void enclave_main()
{
    RSA *rsa;
    BIGNUM *e;

    rsa = RSA_new();

    e = BN_new();
    BN_set_word(e, 65537);

    if (!RSA_generate_key_ex(rsa, 1024, e, NULL))
        sgx_debug("rsa generation failed.\n");
    else {
        sgx_debug("rsa generation succeeded.\n");
        sgx_print_bytes(rsa->p, 1024);
        sgx_print_bytes(rsa->q, 1024);
        sgx_print_bytes(rsa->d, 1024);
	}

    if(RSA_check_key(rsa) <= 0)
        sgx_debug("Wrong RSA key\n");

    sgx_exit(NULL);
}
