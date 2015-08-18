// Hello world enclave program.
// The simplest case which uses opensgx ABI.
// See sgx/user/sgxLib.c and sgx/user/sgx-user.c for detail.

#include "test.h"

void enclave_main()
{
    char *ptr = "Hello world!";
    char *np = 0;
    int i = 10;
    unsigned int bs = sizeof(int)*8;
    int mi;
    int hex = 1024;

    mi = (1 << (bs-1)) + 1;
    sgx_printf("%s\n", ptr);
    sgx_printf("printf test\n");
    sgx_printf("%s is null pointer\n", np);
    sgx_printf("%d = 10\n", i);
    sgx_printf("%d = - max int\n", mi);
    sgx_printf("%x = %d in hex\n", hex, hex);
    sgx_printf("ptr locates in 0x%x\n", (unsigned long)ptr);
    sgx_print_hex((unsigned long)ptr);
    sgx_printf("\n");

    sgx_exit(NULL);
}
