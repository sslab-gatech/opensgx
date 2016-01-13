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

// Intra attestation - Enclave B (target enclave)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sgx.h>
#include <sgx-user.h>
#include <sgx-kern.h>
#include <sgx-lib.h>

void enclave_main()
{
    int challenger_port = 8024, ret;

    ret = sgx_intra_attest_target(challenger_port);
    if(ret == 1) {
        puts("Intra Attestaion Success!");
    } else {
        puts("Intra Attestation Fail!");
    }
    
    sgx_exit(NULL);
}
