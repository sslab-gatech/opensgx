// The simplest enclave enter/exit.

#include "test.h"

void enclave_main()
{
    // exitptr = NULL means right after eenter()
    sgx_exit(NULL);
}
