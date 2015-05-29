// An enclave test case for using heap
// An enclave program of Opensgx requests pre-allocated EPC heap region
// using sgx_malloc.
// Usage of sgx_malloc is same as glibc malloc() function.
// See sgx/user/sgxLib.c and sgx/user/sgx-user.c for detail.

#include "test.h"

void enclave_main()
{
    sgx_malloc(20);
    int *b = sgx_malloc(4096*6);

    if (b != NULL) {
       sgx_print_hex(100);
    }
}
