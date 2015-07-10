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

#include "nonEncLib.h"

static int send_fd = 0;
static int recv_fd = 0, client_fd = 0;

void hexdump(FILE *fd, void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                fprintf(fd, "  %s\n", buff);

            // Output the offset.
            fprintf(fd, "  %04x ", i);
        }

        // Now the hex code for the specific character.
        fprintf(fd, " %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        fprintf(fd, "   ");
        i++;
    }

    // And print the final ASCII bit.
    fprintf(fd, "  %s\n", buff);
}


int send_func(char *ip, char *port, void *msg, size_t len)
{
    struct addrinfo hints, *result, *iter;
    int errcode;
    int sentBytes;
    static char *savedIp = NULL, *savedPort = NULL; 

    //for piggyback
    if(send_fd == 0 || strcmp(savedIp,ip) || strcmp(savedPort, port)) {
        savedIp = ip;
        savedPort = port;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        errcode = getaddrinfo(ip, port, &hints, &result);
        if(errcode != 0)
        {
            perror("getaddr info");
            return -1;
        }

        for(iter = result; iter != NULL; iter = iter->ai_next){
            send_fd = socket(iter->ai_family, iter->ai_socktype,
                            iter->ai_protocol);

            if(send_fd == -1)
            {
                perror("socket");
                continue;
            }

            if(connect(send_fd, iter->ai_addr, iter->ai_addrlen) == -1)
            {
                close(send_fd);
                perror("connect");
                continue;
            }

            break;
        }

        if (iter == NULL) {
            fprintf(stderr, "failed to connect\n");
            return -2;
        }
    }

    if((sentBytes = send(send_fd, msg, len, 0)) == -1)
    {
        fprintf(stderr, "failed to send\n");
        return -3;
    }

    return sentBytes;
}

int recv_func(char *port, void *buf, size_t len)
{
    struct addrinfo hints;
    struct addrinfo *result, *iter;

    static char *savedPort = NULL;
    struct sockaddr_in client_addr;
    int client_addr_len;
    int errcode;
    int nread;

    //for piggyback
    if(recv_fd == 0 || strcmp(port, savedPort)) {
        savedPort = port;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        errcode = getaddrinfo(NULL, port, &hints, &result);
        if (errcode != 0)
        {
            perror("getaddrinfo");
            return -1;
        }

        for (iter = result; iter != NULL; iter = iter->ai_next) {
            recv_fd = socket(iter->ai_family, iter->ai_socktype,
                            iter->ai_protocol);
            if (recv_fd == -1)
                continue;
            if (bind(recv_fd, iter->ai_addr, iter->ai_addrlen) == 0)
                break;    /*success*/

            close(recv_fd);
        }

        if (iter == NULL)
        {
            fprintf(stderr, "failed to bind\n");
            return -2;
        }

        freeaddrinfo(result);

        listen(recv_fd, 1);


        printf("Waiting for incoming connections...\n");
        client_addr_len = sizeof(struct sockaddr_in);
        client_fd = accept(recv_fd, (struct sockaddr *)&client_addr,
                             (socklen_t *)&client_addr_len);

        if (client_fd < 0)
        {
            perror("accept");
            close(recv_fd);
            return -3;
        }
    }

    nread = recv(client_fd, buf, len, 0);
    if (nread == -1)
    {
        perror("recv");
        close(recv_fd);
        close(client_fd);
        return -4;
    }
    return nread;
}


void close_socket(void)
{
    close(send_fd);
    close(recv_fd);
    close(client_fd);
}
