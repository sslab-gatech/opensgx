#include <sys/socket.h>
#include "syscall.h"

#include <sgx-lib.h>

int listen(int fd, int backlog)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_LISTEN;
    stub->out_arg1 = fd;
    stub->out_arg2 = backlog;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}
