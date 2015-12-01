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

// hello world

#include "test.h"

void enclave_main()
{
    keyrequest_t keyreq;
    char *outputdata;

    keyreq.keyname = REPORT_KEY;

    outputdata = memalign(128, 128);

    sgx_getkey(&keyreq, outputdata);
    printf("%x\n", (uint64_t) &keyreq);
    printf("%x\n", (uint64_t) outputdata);

    puts(outputdata);

    sgx_exit(NULL);
}
