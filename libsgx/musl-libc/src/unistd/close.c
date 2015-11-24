#include <unistd.h>
#include <errno.h>
#include "libc.h"

#include <sgx-lib.h>

int close(int fd)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_CLOSE;
    stub->out_arg1 = fd;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}
