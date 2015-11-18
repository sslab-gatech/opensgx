// An enclave test case for stub & trampoline interface.
// Before trampoline to non-enclave region,
// Fields of stub data structure are set before jumping.
// See sgx/user/sgx-user.c for detail.

#include "test.h"

void enclave_main()
{
    // Get an address of reserved stub region
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    // In this example, PUTS operation will be tested
    char buf[] = "hello world";
    stub->fcode = FUNC_PUTS;
    sgx_memcpy(stub->out_data1, buf, sizeof(buf));

    // Enclave exit & jump into userspace trampoline
    sgx_exit(stub->trampoline);

    char buf2[] = "good night";
    stub->fcode = FUNC_PUTS;
    sgx_memcpy(stub->out_data1, buf2, sizeof(buf2));

    sgx_exit(stub->trampoline);

    sgx_exit(NULL);
}
