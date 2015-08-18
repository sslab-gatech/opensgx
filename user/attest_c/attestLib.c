#include <sgx-lib.h>
#include "attest-lib.h"
#include "sgx-attest-trampoline.h"

// one 4k page : enclave page & offset
/*
int sgx_mkdir(char pathname[], size_t size, mode_t mode) 
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_MKDIR;
    sgx_memcpy(stub->out_data1, pathname, size);
    stub->out_arg1 = (int)mode;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_mknod(char pathname[], size_t size, mode_t mode, dev_t dev)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_MKNOD;
    sgx_memcpy(stub->out_data1, pathname, size);
    stub->out_arg1 = (int)mode;
    stub->out_arg2 = (int)dev;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_open(char pathname[], size_t size, int flag)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_OPEN;
    sgx_memcpy(stub->out_data1, pathname, size);
    stub->out_arg1 = flag;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_close(int fd)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_CLOSE;
    stub->out_arg1 = fd;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

ssize_t sgx_write(int fd, void *buf, size_t count)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_WRITE;
    sgx_memcpy(stub->out_data1, buf, count);
    stub->out_arg1 = fd;
    stub->out_arg2 = (int)count;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

ssize_t sgx_read(int fd, void *buf, size_t count)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_READ;
    stub->out_arg1 = fd;
    stub->out_arg2 = (int)count;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);
    sgx_memcpy(buf, stub->in_data1, count);

    return stub->in_arg1;
}

// Note that this is not regular snprintf, specialized one for Tor
int sgx_snprintf(char dst[], int dst_size, char buf1[], size_t size1, 
                 char buf2[], size_t size2, int arg1)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_SNPRINTF;
    stub->out_arg1 = dst_size;
    stub->out_arg2 = arg1;
    sgx_memcpy(stub->out_data1, buf1, size1);
    sgx_memcpy(stub->out_data2, buf2, size2);

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);
    sgx_memcpy(dst, stub->in_data1, dst_size);

    return stub->in_arg1;
}

int sgx_time(time_t *arg1)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_TIME;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);
    sgx_memcpy(arg1, stub->in_data1, sizeof(time_t));

    return stub->in_arg1;
}
*/

void *sgx_memchr(const void *s, int c, size_t n)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_MEMCHR;
    stub->out_arg1 = c;
    stub->out_arg2 = n;
    sgx_memcpy(stub->out_data1, s, n);

    sgx_exit(stub->trampoline);
    
    return (void *)stub->in_data1;
}
