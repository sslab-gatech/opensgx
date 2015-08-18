#include <sgx-lib.h>
#include "sgx-tp-trampoline.h"

// one 4k page : enclave page & offset

void sgx_debug(char msg[])
{
    size_t size = sgx_strlen(msg);
    sgx_stub_info_tp *stub = (sgx_stub_info_tp *)STUB_ADDR_TP;

    stub->fcode = FUNC_DEBUG;
    sgx_memcpy(stub->out_data1, msg, size);

    sgx_exit(stub->trampoline);
}

void sgx_print_bytes(char *s, size_t n)
{
    sgx_stub_info_tp *stub = (sgx_stub_info_tp *)STUB_ADDR_TP;

    stub->fcode = FUNC_PRINT_BYTES;
    sgx_memcpy(stub->out_data1, s, n);
    stub->out_arg1 = n;

    sgx_exit(stub->trampoline);
}

struct tm *sgx_gmtime(const time_t *timep)
{
    sgx_stub_info_tp *stub = (sgx_stub_info_tp *)STUB_ADDR_TP;
    struct tm temp_tm;

    stub->fcode = FUNC_GMTIME;
    stub->out_arg4 = *timep;
    
    sgx_exit(stub->trampoline);
    sgx_memcpy(&temp_tm, &stub->in_tm, sizeof(struct tm));

    return &temp_tm;
}

