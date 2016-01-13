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

// Remote attestation - Challenger

#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void enclave_main()
{
    const char *target_ip = "127.0.0.1";
    int target_port = 8025, ret;
    const char *challenge = "Are you SGX enclave?";

    ret = sgx_remote_attest_challenger(target_ip, target_port, challenge);
    if(ret == 1) {
        puts("Remote Attestaion Success!");
    } else {
        puts("Remote Attestation Fail!");
    }

    sgx_exit(NULL);
}
