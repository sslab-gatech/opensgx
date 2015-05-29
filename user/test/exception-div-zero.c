// An enclave test case for divide by zero exception.
// It will raise divide by zero exception.

#include "test.h"

void enclave_main()
{
    int a = 0;
    int b = 1;
    int c;

    // Here, divide by zero occurs
    c = b/a;
    if (c == 1)
        while (1);

    sgx_exit(NULL);
}
