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
#include <unistd.h>
#include <string.h>

int sgx_intra_attest_challenger(int target_port, char *conf)
{
    int target_fd;
    const char *target_ip = "127.0.0.1";
    targetinfo_t targetinfo;
    sigstruct_t *sigstruct;
    keyrequest_t keyreq;
    report_t report;
    report_t report_send;
    unsigned char nonce[64];
    unsigned char report_key[DEVICE_KEY_LENGTH];


    //Connect to Target enclave (Intra)
    puts("Connecting to Target enclave ...");
    target_fd = sgx_connect_server(target_ip, target_port);
    if(target_fd < 0) {
        puts("sgx_connect_server error\n");
        return -1;
    }
    puts("Target enclave accept!");
    
    //Get SIGSTRUCT of enclave B
    sigstruct = sgx_load_sigstruct(conf);
    puts("Got SIGSTRUCT!");

    //EREPORT with sigstruct
    puts("Sending REPORT to Target enclave ...");
    memcpy(&targetinfo.measurement, &sigstruct->enclaveHash, 32);
    memcpy(&targetinfo.attributes, &sigstruct->attributes, 16);
    memcpy(&targetinfo.miscselect, &sigstruct->miscselect, 4);
    sgx_report(&targetinfo, nonce, &report_send);
    if(sgx_write_sock(target_fd, &report_send, sizeof(report_t)) < 0) {
        puts("sgx_write_sock error\n");
        goto failed;
    }
    
    //Get REPORT from target enclave
    if(sgx_get_report(target_fd, &report) < 0) {
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

    close(target_fd);
    puts("Challenger enclave end");
    return 1;

failed:
    close(target_fd);
    return -1;
}


int sgx_intra_attest_target(int challenger_port)
{
    int client_fd;
    targetinfo_t targetinfo;
    keyrequest_t keyreq;
    report_t report;
    report_t report_send;
    unsigned char nonce[64];
    unsigned char report_key[DEVICE_KEY_LENGTH];

    //server socket for Challenger enclave (Intra)
    puts("Listening to Challenger enclave ...");
    client_fd = sgx_make_server(challenger_port);
    if(client_fd < 0) {
        puts("sgx_connect_server error\n");
        return -1;
    }
    puts("Challenger accepted");
      
    //Get REPORT from Challenger enclvae
    if(sgx_get_report(client_fd, &report) < 0) {
        puts("sgx_get_report error\n");
        goto failed;
    }
    puts("Received REPORT from Challenger enclave");

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
    puts("Sending REPORT to Challenger enclave ...");
    memcpy(&targetinfo.measurement, &report.mrenclave, 32);
    memcpy(&targetinfo.attributes, &report.attributes, 16);
    memcpy(&targetinfo.miscselect, &report.miscselect, 4);
    sgx_report(&targetinfo, nonce, &report_send);
    if(sgx_write_sock(client_fd, &report_send, sizeof(report_t)) < 0) {
        puts("sgx_write_sock error\n");
        goto failed;
    }

    close(client_fd);
    puts("Target enclave end");
    return 1;
failed:
    close(client_fd);
    return -1;
}
