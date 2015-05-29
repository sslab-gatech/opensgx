// hello world

#include "test.h"

void enclave_main()
{
    keyrequest_t keyreq;
    char *outputdata;

    keyreq.keyname = REPORT_KEY;

    outputdata = sgx_memalign(128, 128);

    sgx_getkey(&keyreq, outputdata);
    sgx_puts(outputdata);

    sgx_exit(NULL);
}
