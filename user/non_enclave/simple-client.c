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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>

int main(int count, char *strings[]) {
    int server;
    char buf[256];
    int bytes;
    char *hostname, *portnum;
    struct hostent *host;
    struct sockaddr_in addr;

    if (count != 3) {
      printf("usage: %s <hostname> <portnum>\n", strings[0]);
      return 0;
    }

    hostname = strings[1];
    portnum = strings[2];

    if ((host = gethostbyname(hostname)) == NULL) {
        return 0;
    }

    server = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(portnum));
    addr.sin_addr.s_addr = *(long*)(host->h_addr);
    if (connect(server, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(server);
        return 0;
    }

    if (server < 0) {
        printf("cannot connect to server\n");
        return 0;
    }

    bytes = write(server, "Test server message\n", 21);
    if (bytes < 0)
        printf("failed to write\n");

    bytes = read(server, buf, 255);
    if (bytes < 0)
        printf("failed to read\n");

    printf("Get %s\n", buf);

    close(server);
    return 0;
}
