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

    srvr_fd = sgx_socket();

    if (srvr_fd == -1) {
        sgx_exit(NULL);
    }

    if (sgx_bind(srvr_fd, port) != 0) {
        sgx_exit(NULL);
    }

    if (sgx_listen(srvr_fd) != 0) {
        sgx_exit(NULL);
    }

    while (1) {
        clnt_fd = sgx_accept(srvr_fd);
        sgx_close(clnt_fd);
    }

    sgx_close(srvr_fd);

    sgx_exit(NULL);
}
