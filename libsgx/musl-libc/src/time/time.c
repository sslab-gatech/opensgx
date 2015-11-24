#include <time.h>
#include "syscall.h"

#include <string.h>
#include <sgx-lib.h>

time_t time(time_t *t)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_TIME;

    sgx_exit(stub->trampoline);

    if (t != NULL)
        memcpy(t, stub->out_data1, sizeof(time_t));

    return stub->in_arg3;
}
