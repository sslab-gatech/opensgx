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
    int port = 5566;
    int srvr_fd;
    int clnt_fd;
    int n;
    char buf[256];
    struct sockaddr_in addr;

    srvr_fd = sgx_socket(PF_INET, SOCK_STREAM, 0);

    if (srvr_fd == -1) {
        sgx_exit(NULL);
    }

    sgx_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = sgx_htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (sgx_bind(srvr_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        sgx_exit(NULL);
    }

    if (sgx_listen(srvr_fd, 10) != 0) {
        sgx_exit(NULL);
    }

    while (1) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        clnt_fd = sgx_accept(srvr_fd, (struct sockaddr *)&addr, &len);
        if (clnt_fd < 0) {
            sgx_puts("ERROR on accept\n");
            continue;
        }

        sgx_memset(buf, 0, 256);
        //int n = sgx_read(clnt_fd, buf, 255);
        int n = sgx_recv(clnt_fd, buf, 255, 0);
        if (n < 0)
            sgx_puts("ERROR on read\n");

        sgx_puts(buf);

        //n = sgx_write(clnt_fd, "Successfully received", 21);
        n = sgx_send(clnt_fd, "Successfully received", 21, 0);
        if (n < 0)
            sgx_puts("ERROR on write\n");

        sgx_close(clnt_fd);
    }

    sgx_close(srvr_fd);

    sgx_exit(NULL);
}
