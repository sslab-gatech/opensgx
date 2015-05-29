// test network recv

#include "test.h"

// one 4k page : enclave page & offset
// Very first page chunk should be 4K aligned
void enclave_main()
{
    char port[] = "34444";
    char buf[512] = "";

    sgx_recv(port, buf);

    sgx_exit(NULL);
}
