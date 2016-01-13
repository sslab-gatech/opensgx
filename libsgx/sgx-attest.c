/* Copyright (C) 1991-2015 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <sgx-lib.h>
#include <sgx-shared.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <polarssl/aes_cmac128.h>
#include <polarssl/sha1.h>
#include <polarssl/sha256.h>
#include <polarssl/rsa.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/entropy.h>

#define EXPONENT 3
#define KEY_SIZE 128

int sgx_make_server(int port) 
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;
    
    server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        puts("socket error");
        return -1;
    }

    //Make reusable socket
    int enable = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        puts("setsockopt error");
        close(server_fd);
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        puts("bind error");
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 10) != 0) {
        puts("listen error");
        close(server_fd);
        return -1;
    }
    
    addr_len = sizeof(client_addr);
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        puts("accept error");
        close(server_fd);
        return -1;
    }

    close(server_fd);
    return client_fd;
}

int sgx_connect_server(const char *target_ip, int target_port)
{
    struct sockaddr_in target_addr;
    int target_fd;

    //Make connection with Quoting enclave
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_port);
    if(inet_pton(AF_INET, target_ip, &target_addr.sin_addr) <= 0) {
        puts("inet_pton error");
        return -1;
    }
        
    //socket for connecting intra enclave B
    target_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (target_fd == -1) {
        puts("socket errorr\n");
        return -1;
    }

    if(connect(target_fd, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0)
    {
        puts("connect error\n");
        return -1;
    }

    return target_fd;
}

int sgx_read_sock(int fd, void *buf, int len)
{
    int n = 0;
    while((n = read(fd, buf, len)) <= 0) {
        if (n == -1) {
            puts("read error");
            return -1;
        }
    }
    return n;
}

int sgx_write_sock(int fd, void *buf, int len)
{
    return write(fd, buf, len);
}

int sgx_get_report(int fd, report_t *report)
{
    if(sgx_read_sock(fd, report, sizeof(report_t)) < 0) {
        return -1;
    }

    return 1;
}

int sgx_match_mac(unsigned char *report_key, report_t *report)
{
    unsigned char mac[MAC_SIZE];
    aes_cmac128_context ctx;

    //Compute MAC
    aes_cmac128_starts(&ctx, report_key);
    aes_cmac128_update(&ctx, (uint8_t *)report, 416);
    aes_cmac128_final(&ctx, mac);
    
    //Compare mac and report->mac
    if(memcmp(mac, report->mac, MAC_SIZE) != 0)
    {
        return -1;
    }
    return 1;
}

int sgx_make_quote(const char* pers, report_t *report, 
        unsigned char *rsa_N, unsigned char *rsa_E)
{
    entropy_context entropy;
    ctr_drbg_context ctr_drbg;
    rsa_context rsa;
    unsigned char hash[32], sign[32];
    int ret;

    sha256((unsigned char *)report, sizeof(report_t), hash, 0);

    entropy_init(&entropy);

    if((ret = ctr_drbg_init(&ctr_drbg, entropy_func, &entropy, 
            (const unsigned char *)pers, strlen(pers))) != 0)
    {
        printf("Failed! ctr_drbg_init returned %d\n", ret);
        return -1;
    }

    rsa_init(&rsa, RSA_PKCS_V15, 0);
    
    puts("Generating RSA key ...");

    if((ret = rsa_gen_key(&rsa, ctr_drbg_random, &ctr_drbg, KEY_SIZE, EXPONENT)) != 0)
    {
        printf("Failed! rsa_gen_key returned %d\n", ret);   
        return -1;
    }   
    
    mpi_write_binary(&rsa.N, rsa_N, sizeof(mpi));
    mpi_write_binary(&rsa.E, rsa_E, sizeof(mpi));

    puts("Signing RSA ...");

    if((ret = rsa_pkcs1_sign(&rsa, NULL, NULL, RSA_PRIVATE, POLARSSL_MD_NONE, 
                    0, hash, sign)) != 0)
    {
        printf("Sign error! ret = %d\n", ret);      
        return -1;
    }
    
    memcpy(&report->mac, sign, 16);
    puts("Making QUOTE done!");
    
    return 1;
}
