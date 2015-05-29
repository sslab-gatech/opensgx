// The simplest function call inside the enclave.

#include "test.h"

int func_in_enclave(int a, int b)
{
    return a + b;
}

void enclave_main()
{
    int rtn = func_in_enclave(5, 6);
    (void) rtn;

    sgx_exit(NULL);
}
