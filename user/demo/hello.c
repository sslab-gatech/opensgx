// hello world

#include <sgx-lib.h>

void enclave_main()
{
    char *hello = "hello sgx!\n";
    sgx_puts(hello);
    sgx_exit(NULL);
}

