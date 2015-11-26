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
    printf("%s\n", ptr);
    printf("printf test\n");
    printf("%s is null pointer\n", np);
    printf("%d = 10\n", i);
    printf("%d = - max int\n", mi);
    printf("%x = %d in hex\n", hex, hex);
    printf("ptr locates in 0x%lx\n", (uint64_t)ptr);
    printf("%lx\n", (uint64_t)ptr);
    printf("\n");

    sgx_exit(NULL);
}
