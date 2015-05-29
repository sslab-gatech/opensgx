// An enclave test case for faulty enclave access.
// Faulty enclave access to other enclave region will raise segmentation fault.

#include "test.h"

void enclave_main()
{
    uint64_t ptr;
    asm("leaq (%%rip), %0;": "=r"(ptr));

    // 20 pages after here : this is an EPC region but not for this enclave
    *(char *)(ptr + 0x20000) = 1;

    sgx_exit(NULL);
}
