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

#include "test.h"

// one 4k page : enclave page & offset
// Very first page chunk should be 4K aligned
void enclave_main()
{
    int port = 5566;
    char ip[] = "127.0.0.1";
    int srvr_fd;
    int n;
    char buf[256];
    struct sockaddr_in addr;

    srvr_fd = sgx_socket(PF_INET, SOCK_STREAM, 0);

    sgx_memset(buf, '0',sizeof(buf));
    if (srvr_fd == -1) {
        sgx_exit(NULL);
    }

    sgx_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = sgx_htons(port);

    if (sgx_inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        sgx_exit(NULL);
    }

    if (sgx_connect(srvr_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        sgx_close(srvr_fd);
        sgx_exit(NULL);
    }

    if (srvr_fd < 0) {
        sgx_puts("Cannot connect to server\n");
        sgx_exit(NULL);
    }

    n = sgx_write(srvr_fd, "Test server message\n", 21);
    if (n < 0)
        sgx_puts("failed to write\n");

    n = sgx_read(srvr_fd, buf, 255);
    if (n < 0)
        sgx_puts("failed to read\n");

    sgx_puts(buf);

    sgx_close(srvr_fd);
    sgx_exit(NULL);
}
