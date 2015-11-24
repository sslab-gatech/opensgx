#include <sgx-lib.h>
#include <string.h>

int puts(const char *s)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

	stub->fcode = FUNC_PUTS;
	memcpy(stub->out_data1, s, strlen(s));

	sgx_exit(stub->trampoline);

    return stub->in_arg1;
}
