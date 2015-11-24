#include <sys/socket.h>
#include "syscall.h"
#include "libc.h"

#include <string.h>
#include <sgx-lib.h>

int accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict len)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_ACCEPT;
    stub->out_arg1 = fd;

    sgx_exit(stub->trampoline);

    memcpy(addr, stub->out_data1, sizeof(struct sockaddr));
    memcpy(len, stub->out_data2, sizeof(socklen_t));
    return stub->in_arg1;
}
