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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <polarssl/rsa.h>
#include <polarssl/sha256.h>

int sgx_remote_attest_challenger(const char *target_ip, int target_port, const char *challenge)
{
    //Network information of Target enclave
    int target_fd;
    report_t quote;
    unsigned char *rsa_N, *rsa_E;
    rsa_context rsa;
    unsigned char hash[32], sign[32];

    rsa_N = malloc(sizeof(mpi));
    rsa_E = malloc(sizeof(mpi));
        
    //Connect to Target enclave
    puts("Connecting to Target enclave ...");
    target_fd = sgx_connect_server(target_ip, target_port);
    if(target_fd < 0) {
        puts("sgx_connect_server error\n");
        return -1;
    }
    puts("Target enclave connected!");
    
    //Send challenge
    puts("Sending challenge to Target enclave ...");
    challenge = "Are you enclave?";
    if(sgx_write_sock(target_fd, (void *)challenge, strlen(challenge)) < 0) {
        puts("sgx_write_sock error\n");
        goto failed;
    }

    puts("Waiting for RSA key and QUOTE ...");
    //Receive rsa_N
    memset(rsa_N, 0, sizeof(mpi));
    if(sgx_read_sock(target_fd, rsa_N, sizeof(mpi)) <= 0) {
        puts("sgx_read_sock error\n");
        goto failed;
    }
    puts("Received rsa_N from Target enclave");

    //Receive rsa_E
    memset(rsa_E, 0, sizeof(mpi));
    if(sgx_read_sock(target_fd, rsa_E, sizeof(mpi)) <= 0) {
        puts("sgx_read_sock error\n");
        goto failed;
    }
    puts("Received rsa_E from Target enclave");
    
    //Receive QUOTE
    memset(&quote, 0, sizeof(quote));
    if(sgx_get_report(target_fd, &quote) < 0) {
        puts("sgx_get_report error\n");
        goto failed;
    }
    puts("Received QUOTE from Target enclave");
    
    //Verify QUOTE
    mpi_read_binary(&rsa.N, rsa_N, sizeof(mpi));
    mpi_read_binary(&rsa.E, rsa_E, sizeof(mpi));
    rsa.len = (mpi_msb(&rsa.N)+7)>>3;
    sha256((unsigned char *)&quote, sizeof(report_t), hash, 0);
    memcpy(sign, &quote.mac, 16);
    
    if(rsa_pkcs1_verify(&rsa, NULL, NULL, RSA_PUBLIC,
            POLARSSL_MD_NONE, 0, hash, sign) != 0) {
        puts("Failed to verify!\n");
        close(target_fd);
        goto failed;
    }
    puts("Verify Success!");

    return 1;
failed:
    close(target_fd);
    return -1;
}

int sgx_remote_attest_target(int challenger_port, int quote_port, char *conf)
{
    // Quoting enclave in same platform
    const char *quote_ip = "127.0.0.1";

    targetinfo_t targetinfo;
    unsigned char nonce[64];
    char read_buf[512];

    report_t report;
    report_t report_send;
    report_t quote;
    
    int client_fd, quote_fd;
    sigstruct_t *sigstruct; 
    char *resultmsg;
    keyrequest_t keyreq;
    unsigned char report_key[DEVICE_KEY_LENGTH];
    unsigned char *rsa_N, *rsa_E;

    rsa_N = malloc(sizeof(mpi));
    rsa_E = malloc(sizeof(mpi));
        
    //server socket for Challenger
    puts("Listening to Challenger ...");
    client_fd = sgx_make_server(challenger_port);
    if(client_fd < 0) {
        puts("sgx_make_server error\n");
        return -1;
    }
    puts("Challenger accepted");

    //Get challenge data from Challenger
    puts("Receiving challenge data ...");
    memset(read_buf, 0, 512);
    if (sgx_read_sock(client_fd, read_buf, 512) <= 0) {
        puts("sgx_read_sock error\n");
        return -1;
    }
    printf("Challeger msg: %s\n", read_buf);

    //Connect to Quoting enclave
    puts("Connecting to Quoting enclave ...");
    quote_fd = sgx_connect_server(quote_ip, quote_port);
    if(quote_fd < 0) {
        puts("sgx_connect_server error\n");
        close(client_fd);
        return -1;
    }
    puts("Quoting enclave connected!");

    //Get SIGSTRUCT of Quoting enclave
    sigstruct = sgx_load_sigstruct(conf);
    puts("Got SIGSTRUCT!");

    //EREPORT with sigstruct
    puts("Sending REPORT to Quoting enclave ...");
    memcpy(&targetinfo.measurement, &sigstruct->enclaveHash, 32);
    memcpy(&targetinfo.attributes, &sigstruct->attributes, 16);
    memcpy(&targetinfo.miscselect, &sigstruct->miscselect, 4);
    sgx_report(&targetinfo, nonce, &report_send);
    if(sgx_write_sock(quote_fd, &report_send, sizeof(report_t)) < 0) {
        puts("sgx_write_sock error\n");
        goto failed;
    }

    //Get REPORT from Quoting enclave   
    if(sgx_get_report(quote_fd, &report) < 0) {
        puts("sgx_get_report error\n");
        goto failed;
    }
    puts("Received REPORT from Quoting enclave");

    //Get Report key from QEMU
    keyreq.keyname = REPORT_KEY;
    memcpy(&keyreq.keyid, &report.keyid, 16);
    memcpy(&keyreq.miscmask, &report.miscselect, 4);
    sgx_getkey(&keyreq, report_key);
    puts("Received Report Key");

    //Check MAC matching
    if(sgx_match_mac(report_key, &report) < 0) {
        puts("Mac not match!\n");
        goto failed;
    }
    puts("MAC match, PASS!");

    //Send intra attestaion result to Quoting enclave
    puts("Sending result message to Quoting enclave ...");
    resultmsg = "Good";
    if(sgx_write_sock(quote_fd, resultmsg, strlen(resultmsg)) < 0) {
        puts("sgx_write_sock error\n");
    }

    puts("Waiting for RSA key and QUOTE ...");
    //Receive rsa_N
    memset(rsa_N, 0, sizeof(mpi));
    if(sgx_read_sock(quote_fd, rsa_N, sizeof(mpi)) <= 0) {
        puts("sgx_read_sock error\n");
        goto failed;
    }
    puts("Received rsa_N from Quoting enclave");

    //Receive rsa_E
    memset(rsa_E, 0, sizeof(mpi));
    if(sgx_read_sock(quote_fd, rsa_E, sizeof(mpi)) <= 0) {
        puts("sgx_read_sock error\n");
        goto failed;
    }
    puts("Received rsa_E from Quoting enclave");
    
    //Receive QUOTE
    memset(&quote, 0, sizeof(quote));
    if(sgx_get_report(quote_fd, &quote) < 0) {
        goto failed;
    }
    puts("Received QUOTE from Quoting enclave");

    //send to challenger
    puts("Sending RSA & QUOTE to Challenger");
    if(sgx_write_sock(client_fd, rsa_N, sizeof(mpi)) < 0) {
        puts("sgx_write_sock error\n");
        goto failed;
    }
    if(sgx_write_sock(client_fd, rsa_E, sizeof(mpi)) < 0) {
        puts("sgx_write_sock error\n");
        goto failed;
    }
    if(sgx_write_sock(client_fd, &quote, sizeof(report_t)) < 0) {
        puts("sgx_write_sock error\n");
        goto failed;
    }

    close(quote_fd);
    close(client_fd);
    puts("Target enclave end");
    return 1;
failed:
    close(quote_fd);
    close(client_fd);
    return -1;
}

int sgx_remote_attest_quote(int target_port)
{
    report_t report;
    report_t report_send;
    targetinfo_t targetinfo;
    unsigned char nonce[64];
    char read_buf[512];
    int client_fd;    
    keyrequest_t keyreq;
    unsigned char report_key[DEVICE_KEY_LENGTH];
    const char *pers = "rsa_genkey";
    unsigned char *rsa_N, *rsa_E;
    
    rsa_N       = malloc(sizeof(mpi));
    rsa_E       = malloc(sizeof(mpi));

    puts("Listening to Target enclave ...");
    client_fd = sgx_make_server(target_port);
    if(client_fd < 0) {
        puts("sgx_make_server error\n");
        return -1;
    }
    puts("Target enclave accepted");
      
    //Get REPORT from Target enclave
    if(sgx_get_report(client_fd, &report) < 0) {
        puts("sgx_get_report error\n");
        goto failed;
    }
    puts("Received REPORT from Target enclave");
    
    //Get Report key from QEMU
    keyreq.keyname = REPORT_KEY;
    memcpy(&keyreq.keyid, &report.keyid, 16);
    memcpy(&keyreq.miscmask, &report.miscselect, 4);    
    sgx_getkey(&keyreq, report_key);
    puts("Received Report Key");

    //Check MAC matching
    if(sgx_match_mac(report_key, &report) < 0) {
        puts("Mac not match!\n");
        goto failed;
    }
    puts("MAC match, PASS!");

    //EREPORT with given report
    puts("Sending REPORT to Target enclave ...");
    memcpy(&targetinfo.measurement, &report.mrenclave, 32);
    memcpy(&targetinfo.attributes, &report.attributes, 16);
    memcpy(&targetinfo.miscselect, &report.miscselect, 4);
    sgx_report(&targetinfo, nonce, &report_send);
    if(sgx_write_sock(client_fd, &report_send, sizeof(report_t)) < 0) {
        puts("sgx_write_sock error\n");
        goto failed;
    }
    
    //Get intra attestation result of Target enclave
    memset(read_buf, 0, sizeof(read_buf));
    if(sgx_read_sock(client_fd, read_buf, sizeof(read_buf)) <= 0) {
        puts("sgx_read_sock error\n");
        goto failed;
    }
    printf("Target enclave msg: %s\n", read_buf);
    if(strcmp(read_buf, "Good") != 0) {
        puts("Target enclave denied");
        goto failed;
    }

    //Make RSA & QUOTE
    sgx_make_quote(pers, &report, rsa_N, rsa_E);

    //Send quote
    if(sgx_write_sock(client_fd, rsa_N, sizeof(mpi)) < 0) {
        puts("sgx_write_sock error\n");
        goto failed;
    }
    if(sgx_write_sock(client_fd, rsa_E, sizeof(mpi)) < 0) {
        puts("sgx_write_sock error\n");
        goto failed;
    }
    if(sgx_write_sock(client_fd, &report, sizeof(report_t)) < 0) {
        puts("sgx_write_sock error\n");
        goto failed;
    }
    
    close(client_fd);
    puts("Quoting enclave end");
    return 1;
failed:
    close(client_fd);
    return -1;
}
