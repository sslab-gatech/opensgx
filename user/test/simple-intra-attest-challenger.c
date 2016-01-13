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

// Intra attestation - Enclave A (challenger enclave)

#include "test.h"

void enclave_main()
{
    int target_port, ret;
    char *conf = "../user/demo/simple-intra-attest-target.conf";
    target_port = 8024;

    ret = sgx_intra_attest_challenger(target_port, conf);
    if(ret == 1) {
        puts("Intra Attestaion Success!");
    } else {
        puts("Intra Attestation Fail!");
    }
    
    sgx_exit(NULL);
}
