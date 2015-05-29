// An enclave test case for sgx_recv.
// Through using sgx_recv ABI, it is possible to receive a message.
// See sgx/user/sgxLib.c and sgx/user/sgx-user.c for detail.

#include "test.h"

void enclave_main()
{
    // Set port for receiving a message
    char port[] = "34444";
    char buf[512] = "";

    // Receive a message
    sgx_recv(port, buf);

    // Print a message received
    sgx_puts(buf);

    sgx_exit(NULL);
}
