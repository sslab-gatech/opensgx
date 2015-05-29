// An enclave test case for sgx_realloc
// Usage of sgx_realloc is same as glibc realloc() function.
// See sgx/user/sgxLib.c and sgx/user/sgx-user.c for detail.

#include "test.h"

void enclave_main()
{
    char buf[] = "this is the message in a buffer assigned from malloc";
    char *test = sgx_malloc(50);
    sgx_memcpy(test, buf, 50);
    sgx_puts(test);

    test = sgx_realloc(test, 70);
    sgx_print_hex(test);
    sgx_puts(test);

    test = sgx_realloc(test, 30);
    sgx_print_hex(test);
    sgx_puts(test);

    sgx_free(test); 
}
