// The simplest enclave which accesses a global variable

#include "test.h"

int global_var;

void enclave_main()
{
    global_var = 'a';

    sgx_puts((char *)&global_var);
    sgx_print_hex((unsigned long)global_var);

    sgx_exit(NULL);
}
