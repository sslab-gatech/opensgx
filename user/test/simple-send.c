// An enclave test case for sgx_send.
// Through using sgx_send ABI, it is possible to send a message.
// See sgx/user/sgxLib.c and sgx/user/sgx-user.c for detail.

#include "test.h"

void enclave_main()
{
    // Set ip and port for sending a message
    char ip[] = "127.0.0.1";
    char port[] = "34444";
    char msg[] = "Are you there? :)";

    // Send a message
    sgx_send(ip, port, msg, sgx_strlen(msg));

    sgx_exit(NULL);
}
