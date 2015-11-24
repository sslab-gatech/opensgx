#include "time_impl.h"
#include <errno.h>

#include <string.h>
#include <sgx-lib.h>

struct tm *gmtime(const time_t *t)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    struct tm temp_tm;

    stub->fcode = FUNC_GMTIME;
    stub->out_arg4 = *t;

    sgx_exit(stub->trampoline);
    memcpy(&temp_tm, &stub->in_tm, sizeof(struct tm));

    return &temp_tm;
}
