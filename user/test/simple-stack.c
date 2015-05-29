// The simplest enclave enter/exit with stack.
// With -O3 option, the enclave_main() doesn't do anything
// because of code optimization. For testing this, please remove -O3 option.

#include "test.h"

void enclave_main()
{
    int i = 0;
    int c = 6;

    while (i < 6) {
        i++;
    }
    c = 7;

    (void) c;

    sgx_exit(NULL);
}
