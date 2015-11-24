#include <sys/socket.h>

#include <string.h>
#include <sgx-lib.h>

ssize_t send(int fd, const void *buf, size_t len, int flags)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    int tmp_len;
    int i;
    ssize_t rt;

    rt = 0;
    for (i = 0; i < len / SGXLIB_MAX_ARG + 1; i++) {
        stub->fcode = FUNC_SEND;
        stub->out_arg1 = fd;
        stub->out_arg3 = flags;

        if (i == len / SGXLIB_MAX_ARG)
        	tmp_len = (int)len % SGXLIB_MAX_ARG;
        else
        	tmp_len = SGXLIB_MAX_ARG;

        stub->out_arg2 = tmp_len;
        memcpy(stub->out_data1, (uint8_t *)buf + i * SGXLIB_MAX_ARG, tmp_len);
        sgx_exit(stub->trampoline);
        rt += stub->in_arg1;
    }

    return rt;
}
