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

// test network send 

#include "test.h"

void enclave_main()
{
// Disable due to using old API
#if 0
    char ip[] = "127.0.0.1";
    char sendPort[] = "34444";
    char recvPort[] = "35555";
    char buf[512] = "";
    char msg[512] = "Are you there? :)";

    targetinfo_t targetinfo;
    char *reportdata;
    unsigned char *outputdata;

    // get report
    reportdata = sgx_memalign(128, 64);
    sgx_memset(reportdata, 0, 64);
    outputdata = sgx_memalign(512, 512);
    sgx_memset(outputdata, 0, 512);

    sgx_report(&targetinfo, reportdata, outputdata);

    // receive ping from non-enclave process
    sgx_recv(recvPort, buf);
    sgx_puts(buf);

    // send report
    sgx_send(ip, sendPort, outputdata, 512);

    //close all sockets
    sgx_close_sock();
#endif
    sgx_exit(NULL);
}
