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

// test network recv

#include "test.h"

// one 4k page : enclave page & offset
// Very first page chunk should be 4K aligned
void enclave_main()
{
    char ip[] = "127.0.0.1";
    char sendPort[] = "34444";
    char recvPort[] = "35555";
    char buf[512] = "";
    char msg[512] = "Are you there? :)";

    sgx_send(ip, sendPort, msg, sgx_strlen(msg));

    sgx_recv(recvPort, buf);
    sgx_puts(buf);

    sgx_memcpy(msg, "Second Ping :)", 15); 
    sgx_send(ip, sendPort, msg, sgx_strlen(msg));

    //close all sockets
    sgx_close_sock();

    sgx_exit(NULL);
}
