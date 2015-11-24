#include <sys/socket.h>

#include <string.h>
#include <sgx-lib.h>

ssize_t recv(int fd, void *buf, size_t len, int flags)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    int tmp_len;
    int rt;
    int i;

    for (i = 0; i < len / SGXLIB_MAX_ARG + 1; i++) {
        stub->fcode = FUNC_RECV;
        stub->out_arg1 = fd;
        stub->out_arg3 = flags;

        if (i == len / SGXLIB_MAX_ARG)
        	tmp_len = (int)len % SGXLIB_MAX_ARG;
        else
        	tmp_len = SGXLIB_MAX_ARG;
        
        stub->out_arg2 = tmp_len;
        sgx_exit(stub->trampoline);

        memcpy((uint8_t *)buf + i * SGXLIB_MAX_ARG, stub->in_data1, tmp_len);
        rt += stub->in_arg1;
    }

    return rt;
}
