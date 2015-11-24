#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

#include <sgx-lib.h>

int socket(int domain, int type, int protocol)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_SOCKET;
    stub->out_arg1 = domain;
    stub->out_arg2 = type;
    stub->out_arg3 = protocol;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}
