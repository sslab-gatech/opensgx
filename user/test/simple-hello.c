// Hello world enclave program.
// The simplest case which uses opensgx ABI.
// See sgx/user/sgxLib.c and sgx/user/sgx-user.c for detail.

#include "test.h"

void enclave_main()
{
    char *hello = "hello sgx!\n";
    sgx_puts(hello);

    sgx_exit(NULL);
}
