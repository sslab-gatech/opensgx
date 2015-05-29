// An enclave test case for sgx library.
// Memory operations inside the enclave should use own sgx library functions.
// Opensgx supports several library functions such as 
// sgx_memset, sgx_memcpy, sgx_memcmp and so on.
// Usage of these functions are same as glibc functions.
// See sgx/user/sgxLib.c for detail.

#include "test.h"
#define MATCH "MATCH\n"
#define UNMATCH "UNMATCH\n"

void enclave_main()
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    char buf1[] = "hello world\n";
    char buf2[] = "hello world\n";

    // sgx_memcpy test
    sgx_memcpy(buf1, buf2, sgx_strlen(buf1));

    // sgx_puts & sgx_strlen test
    sgx_puts(buf1);

    // sgx_strcmp test
    if(!(sgx_strcmp(buf1, buf2)))
        sgx_puts(MATCH);
    else
        sgx_puts(UNMATCH);

    // sgx_memcmp test
    if(!(sgx_memcmp(buf1, buf2, 5)))
        sgx_puts(MATCH);
    else 
        sgx_puts(UNMATCH);

    // sgx_memset test
    sgx_memset(buf1, 'A', 5);
    sgx_puts(buf1);

    // sgx_strcmp test
    if(!(sgx_strcmp(buf1, buf2)))
        sgx_puts(MATCH);
    else
        sgx_puts(UNMATCH);

    sgx_exit(NULL);
}
