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

// Get SIGSTRUCT value from hello.conf

#include "test.h"

void print_loop(uint8_t *arr, int len)
{
    int i;
    for (i = 0; i < len; i ++) {
        printf("%02X", arr[i]);
    }
    printf("\n");
}

void enclave_main()
{
    sigstruct_t *sigstruct;
    char *conf = "demo/hello.conf";

    sigstruct = sgx_load_sigstruct(conf);

    printf("MRENCLAVE value:\n");
    printf("enclave hash: ");
    print_loop(sigstruct->enclaveHash, 32);

    printf("MISCSELECT value:\n");
    miscselect_t miscselect = sigstruct->miscselect;
    printf("exinfo: %u\n", miscselect.exinfo);
    printf("reserved: ");
    printf("%X", miscselect.reserved1);
    print_loop(miscselect.reserved2, 3);

    printf("ATTRIBUTES value:\n");
    attributes_t attributes = sigstruct->attributes;
    printf("reserved1: %u\n", attributes.reserved1);
    printf("debug: %u\n", attributes.debug);
    printf("mode64bit: %u\n", attributes.mode64bit);
    printf("reserved2: %u\n", attributes.reserved2);
    printf("provisionkey: %u\n", attributes.provisionkey);
    printf("einittokenkey: %u\n", attributes.einittokenkey);
    printf("reserved3: %u", attributes.reserved3);
    print_loop(attributes.reserved4, 7);
    printf("xfirm: %016X\n", attributes.xfrm);


    sgx_exit(NULL);
}
