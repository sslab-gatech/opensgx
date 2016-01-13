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

// Remote attestation - Target Enclave

#include "test.h"

void enclave_main()
{
    int challenger_port, quote_port, ret;
    char *conf = "../user/demo/simple-remote-attest-quote.conf";
    challenger_port = 8025; 
    quote_port = 8026;

    ret = sgx_remote_attest_target(challenger_port, quote_port, conf);
    if(ret == 1) {
        puts("Remote Attestaion Success!");
    } else {
        puts("Remote Attestation Fail!");
    }
    
    sgx_exit(NULL);
}
