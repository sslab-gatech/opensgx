// test network send 

#include "test.h"

void enclave_main()
{
    char ip[] = "127.0.0.1";
    char sendPort[] = "34444";
    char recvPort[] = "35555";
    char buf[512] = "";
    char msg[512] = "Are you there? :)";

    targetinfo_t targetinfo;
    char *reportdata;
    unsigned char *outputdata;

    // get report
    reportdata = sgx_memalign(128, 64);
    sgx_memset(reportdata, 0, 64);
    outputdata = sgx_memalign(512, 512);
    sgx_memset(outputdata, 0, 512);

    sgx_report(&targetinfo, reportdata, outputdata);

    // receive ping from non-enclave process
    sgx_recv(recvPort, buf);
    sgx_puts(buf);

    // send report
    sgx_send(ip, sendPort, outputdata, 512);

    //close all sockets
    sgx_close_sock();

    sgx_exit(NULL);
}
