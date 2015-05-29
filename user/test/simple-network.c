// test network recv

#include "test.h"

// one 4k page : enclave page & offset
// Very first page chunk should be 4K aligned
void enclave_main()
{
    char ip[] = "127.0.0.1";
    char sendPort[] = "34444";
    char recvPort[] = "35555";
    char buf[512] = "";
    char msg[512] = "Are you there? :)";

    sgx_send(ip, sendPort, msg, sgx_strlen(msg));

    sgx_recv(recvPort, buf);
    sgx_puts(buf);

    sgx_memcpy(msg, "Second Ping :)", 15); 
    sgx_send(ip, sendPort, msg, sgx_strlen(msg));

    //close all sockets
    sgx_close_sock();

    sgx_exit(NULL);
}
