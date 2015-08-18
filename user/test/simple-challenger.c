#include "test.h"

//challenger
void enclave_main()
{
    //port and ip of target enclave
    int target_port = 10001;
    char target_ip[] = "127.0.0.1";

    struct sockaddr_in target_addr;

    target_addr.sin_family = AF_INET;
    target_addr.sin_port = sgx_htons(target_port);

    sgx_remote((struct sockaddr *)&target_addr, sizeof(target_addr));
}
