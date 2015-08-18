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

#include <nonEncLib.h>

void main()
{
    char ip[] = "127.0.0.1";
    char sendPort[] = "35555";
    char recvPort[] = "34444";
    char buf[512] = "";
    char msg[512] = "FROM NON_ENCLAVE: HELLO ENCLAVE";
    int n_recv = 0, n_send;

    n_send = send_func(ip, sendPort, msg, strlen(msg));
    printf("%d bytes are sent !!\n",n_send);
    n_recv = recv_func(recvPort, buf, 512);
    printf("%d bytes are received !!\n",n_recv);
    hexdump(stdout, buf, n_recv);

    close_socket();
}
