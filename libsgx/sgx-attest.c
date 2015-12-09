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

#include <polarssl/aes_cmac128.h>
#include <polarssl/sha256.h>
#include <polarssl/rsa.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/entropy.h>

#define EXPONENT 3
#define KEY_SIZE 128

int
sgx_attest_target(struct sockaddr *quote_addr, socklen_t quote_addrlen,
	struct sockaddr *challenger_addr, socklen_t challenger_addrlen)
{
    char *reportdata;
    targetinfo_t targetinfo;
    keyrequest_t keyreq;
    unsigned char *outputdata;
    unsigned char *outputdata_key;
    uint8_t *reportkey;

    report_t report_ori;
    report_t report_mac;
    report_t report_target;
    report_t quote;

    int wait = 0;
    int sock;
    socklen_t len;
    int server_fd, client_fd;
    struct sockaddr_in client_addr;

    char *write_buf;
    char *read_buf;

    char *rsa_N;
    char *rsa_E; 

    unsigned char *mac;
    unsigned char *remac;
    aes_cmac128_context *ctx;

    reportdata 		= memalign(128, 64);
    write_buf 		= malloc(2048);
    read_buf 		= malloc(2048);
    outputdata 		= memalign(512, 512);
    outputdata_key 	= memalign(128, 128);
    reportkey 		= malloc(16);
    mac             = malloc(16);
    remac           = malloc(16);
    ctx             = malloc(sizeof(aes_cmac128_context));
    rsa_N           = malloc(sizeof(mpi));
    rsa_E           = malloc(sizeof(mpi));

    //server socket for challenger
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
	    puts("Enclave Application: Cannot open stream socket\n");
    	sgx_exit(NULL);
    }

    if(bind(server_fd, (struct sockaddr *)challenger_addr, challenger_addrlen) < 0)
    {
	    puts("Enclave Application: Cannot bind local address.\n");
    	sgx_exit(NULL);
    }

    if(listen(server_fd, 5) < 0)
    {
	    puts("Enclave Application: Cannot listening connect.\n");
    	sgx_exit(NULL);
    }

    len = sizeof(client_addr);

    puts("Enclave application is waiting for challenger...\n");
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
    if(client_fd < 0)
    {
	    puts("Enclave Application: Accept failed.\n");
	    sgx_exit(NULL);
    }

    read(client_fd, read_buf, 512); 

    //start remote attestation
    if(strcmp(read_buf, "START_REMOTE") == 0)
    {
    	//with quoting enclave
    	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    	{
    	    puts("Cannot create socket.\n");
    	    sgx_exit(NULL);
    	}

        if(connect(sock, (struct sockaddr *)quote_addr, quote_addrlen) < 0)
        {
                puts("Cannot connect.\n");
                sgx_exit(NULL);
        }   

        strcpy(write_buf, "start");
        write(sock, write_buf, 512); //1

        //receive report
        read(sock, read_buf, 512); //2

        memcpy(&report_target, read_buf, 512);

        memcpy(&targetinfo.miscselect, &report_target.miscselect, 4);
        memcpy(&targetinfo.attributes, &report_target.attributes, 16);
        memcpy(&targetinfo.measurement, &report_target.mrenclave, 32);
        memset(reportdata, 0, sizeof(reportdata));
        sgx_report(&targetinfo, reportdata, outputdata);

        while(wait < 100000000) wait++;
        wait = 0;

        write(sock, outputdata, 512); //3
        memset(read_buf, 0, 2048);

        read(sock, read_buf, 512); //4
        memcpy(&report_ori, read_buf, 512);
        memcpy(&report_mac, read_buf, 512);

        //egetkey
        keyreq.keyname = REPORT_KEY;
        memcpy(&keyreq.keyid, &report_ori.keyid, 16);
        memcpy(&keyreq.miscmask, &report_ori.miscselect, 4);
        sgx_getkey(&keyreq, outputdata_key);

        //report key
        memcpy(reportkey, outputdata_key, 16);

        aes_cmac128_starts(ctx, reportkey);
        aes_cmac128_update(ctx, (uint8_t *)&report_mac, 416);
        aes_cmac128_final(ctx, report_mac.mac);

        memcpy(mac, &report_ori.mac, 16);
        memcpy(remac, &report_mac.mac, 16);

        if(memcmp(mac, remac, 16) != 0)
        {
            puts("Error Mac!\n");
        }

        //get public key and quote
        read(sock, rsa_N, sizeof(mpi)); 
        read(sock, rsa_E, sizeof(mpi)); 
        read(sock, &quote, sizeof(report_t));

        //send to challenger
        write(client_fd, rsa_N, sizeof(mpi));
        write(client_fd, rsa_E, sizeof(mpi));
        write(client_fd, &quote, sizeof(report_t));
    }
}

int
sgx_intra_for_quoting(struct sockaddr *server_addr, socklen_t addrlen)
{
    char *reportdata;
    targetinfo_t targetinfo;
    keyrequest_t keyreq;
    unsigned char *outputdata;
    unsigned char *outputdata_key;
    char *read_buf;
    uint8_t *reportkey;

    report_t report_ori;
    report_t report_mac;
    report_t report_target;
    report_t quote;

    unsigned char *mac;
    unsigned char *remac;
    aes_cmac128_context *ctx;
    rsa_context *rsa;
    ctr_drbg_context *ctr_drbg;
    entropy_context *entropy;

    char *rsa_N;
    char *rsa_E;
    char *hash;
    char *sign;
    const char *pers = "rsa_genkey";

    int ret;
    int wait = 0;
    socklen_t len;
    int server_fd, client_fd;
    struct sockaddr_in client_addr;

    reportdata 		= memalign(128, 64);
    outputdata 		= memalign(512, 512);
    read_buf 		= malloc(2048);
    outputdata_key 	= memalign(128, 128);
    reportkey 		= malloc(16);
    mac 		= malloc(16);
    remac 		= malloc(16);
    ctx			= malloc(sizeof(aes_cmac128_context));
    rsa			= malloc(sizeof(rsa_context));
    ctr_drbg	= malloc(sizeof(ctr_drbg_context));
    rsa_N		= malloc(sizeof(mpi));
    rsa_E		= malloc(sizeof(mpi));
    hash		= malloc(32);
    sign		= malloc(32);
    entropy		= malloc(sizeof(entropy_context));

    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
	    puts("Quoting enclave: Cannot open stream socket\n");
    	sgx_exit(NULL);
    }

    if(bind(server_fd, (struct sockaddr *)server_addr, addrlen) < 0)
    {
	    puts("Quoting enclave : Cannot bind local address.\n");
    	sgx_exit(NULL);
    }

    if(listen(server_fd, 5) < 0)
    {
	    puts("Quoting enclave : Cannot listening connect.\n");
    	sgx_exit(NULL);
    }

    //Quoting enclave waits for connection request.
    len = sizeof(client_addr);
	puts("Quoting enclave is waiting for connection request...\n");
	client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
	if(client_fd < 0)
	{
	    puts("Quoting enclave : Accept failed.\n");
	    sgx_exit(NULL);
	}

	read(client_fd, read_buf, 512); //1

	//start intra attestation
	if(strcmp(read_buf, "start") == 0)
	{
	    memset(outputdata, 0, 512);
	    sgx_report(&targetinfo, reportdata, outputdata);

	    //send report	
	    write(client_fd, outputdata, 512); //2

	    //receive report
	    read(client_fd, read_buf, 512); //3

	    memcpy(&report_ori, read_buf, 512);
	    memcpy(&report_mac, read_buf, 512);

	    //egetkey
	    keyreq.keyname = REPORT_KEY;
	    memcpy(&keyreq.keyid, &report_ori.keyid, 16);
	    memcpy(&keyreq.miscmask, &report_ori.miscselect, 4);
	    sgx_getkey(&keyreq, outputdata_key);

	    //report key
	    memcpy(reportkey, outputdata_key, 16);

	    aes_cmac128_starts(ctx, reportkey);
	    aes_cmac128_update(ctx, (uint8_t *)&report_mac, 416);
	    aes_cmac128_final(ctx, report_mac.mac);

	    memcpy(mac, &report_ori.mac, 16);
	    memcpy(remac, &report_mac.mac, 16);

	    if(memcmp(mac, remac, 16) != 0)
	    {
		    puts("Error Mac!\n");
	    }

	    //get target info from received data
	    memcpy(&report_target, read_buf, sizeof(report_t));
	    memcpy(&targetinfo.miscselect, &report_target.miscselect, 4);
	    memcpy(&targetinfo.attributes, &report_target.attributes, 16);
	    memcpy(&targetinfo.measurement, &report_target.mrenclave, 32);

	    memset(reportdata, 0, sizeof(reportdata));

	    sgx_report(&targetinfo, reportdata, outputdata);
	    while(wait < 100000000) wait++;
	    wait = 0;     
	    write(client_fd, outputdata, 512);

	    sha256(&report_ori, 416, hash, 0);
	    entropy_init(entropy);

	    //rsa
	    if((ret = ctr_drbg_init(ctr_drbg, entropy_func, entropy, 
			    (const unsigned char *)pers, strlen(pers))) != 0)
	    {
		    printf("Failed! ctr_drbg_init returned %d\n", ret);
	    }
	    rsa_init(rsa, RSA_PKCS_V15, 0);

	    if((ret = rsa_gen_key(rsa, ctr_drbg_random, ctr_drbg, KEY_SIZE, EXPONENT)) != 0)
	    {
		    printf("Failed! rsa_gen_key returned %d\n", ret);
	    }
	    mpi_write_binary(&rsa->N, rsa_N, sizeof(mpi));
	    mpi_write_binary(&rsa->E, rsa_E, sizeof(mpi));

	    if((ret = rsa_pkcs1_sign(rsa, NULL, NULL, RSA_PRIVATE, POLARSSL_MD_NONE, 
			    16, hash, sign)) != 0)
	    {
		    printf("Sign error! ret = %d\n", ret);
	    }

	    //send quote
	    memcpy(&report_mac.mac, sign, 16);
	    memcpy(&quote, &report_mac, sizeof(report_t));

	    write(client_fd, rsa_N, sizeof(mpi));
	    write(client_fd, rsa_E, sizeof(mpi));
	    write(client_fd, &quote, sizeof(report_t));

	}
}

int 
sgx_remote(const struct sockaddr *target_addr, socklen_t addrlen)
{
    int ret;
    int sock;
    char *write_buf;
    char *read_buf;

    rsa_context *rsa;

    report_t quote;

    char *rsa_N;
    char *rsa_E;
    char *hash;
    char *sign;
    const char *pers = "rsa_genkey";

    write_buf = malloc(2048);
    read_buf  = malloc(2048);
    rsa       = malloc(sizeof(rsa_context));
    rsa_N     = malloc(sizeof(mpi));
    rsa_E     = malloc(sizeof(mpi));
    hash      = malloc(32);
    sign      = malloc(32);

    rsa_init(rsa, RSA_PKCS_V15, 0);

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
	    puts("Cannot create socket.\n");
    	sgx_exit(NULL);
    }

    if(connect(sock, (struct sockaddr *)target_addr, addrlen) < 0)
    {
	    puts("Cannot connect.\n");
    	sgx_exit(NULL)
    }

    strcpy(write_buf, "START_REMOTE");
    write(sock, write_buf, 512); //1

    read(sock, rsa_N, sizeof(mpi));
    read(sock, rsa_E, sizeof(mpi));
    read(sock, &quote, sizeof(report_t));

    mpi_read_binary(&rsa->N, rsa_N, sizeof(mpi));
    mpi_read_binary(&rsa->E, rsa_E, sizeof(mpi));

    rsa->len = (mpi_msb(&rsa->N)+7)>>3;

    sha256(&quote, 416, hash, 0);
    memcpy(sign, &quote.mac, 16);

    if((ret = rsa_pkcs1_verify(rsa, NULL, NULL, RSA_PUBLIC,
		    POLARSSL_MD_NONE, 16, hash, sign)) != 0)
    {
	    puts("Failed to verify!\n");
    }
    else
    {
	    puts("Success!\n");
    }
}

