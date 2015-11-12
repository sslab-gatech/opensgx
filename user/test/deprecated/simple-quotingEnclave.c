#include "test.h"

//quoting enclave
void enclave_main()
{
    //port-10000 for remote attestation
    int port = 10000;
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    sgx_intra_for_quoting((struct sockaddr *)&server_addr, sizeof(server_addr));

}
