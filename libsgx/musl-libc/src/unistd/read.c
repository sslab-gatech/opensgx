#include <unistd.h>
#include "libc.h"

#include <string.h>
#include <sgx-lib.h>

ssize_t read(int fd, void *buf, size_t count)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    int tmp_len;
    ssize_t rt;
    int i;

    rt = 0;
    for (i = 0; i < count / SGXLIB_MAX_ARG + 1; i++) {
        stub->fcode = FUNC_READ;
        stub->out_arg1 = fd;

        if (i == count / SGXLIB_MAX_ARG)
            tmp_len = (int)count % SGXLIB_MAX_ARG;
        else
            tmp_len = SGXLIB_MAX_ARG;

        stub->out_arg2 = tmp_len;
        sgx_exit(stub->trampoline);
        memcpy((uint8_t *)buf + i * SGXLIB_MAX_ARG, stub->in_data1, tmp_len);
        rt += stub->in_arg1;
    }

    return rt;
}
