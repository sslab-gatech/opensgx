#include <stdio.h>

#include <sgx-lib.h>

int putchar(int c)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    stub->out_arg1 = (int)c;
    stub->fcode = FUNC_PUTCHAR;

    sgx_exit(stub->trampoline);
}
