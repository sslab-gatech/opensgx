#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <err.h>
#include <assert.h>
#include <sgx.h>

//about a page
#define STUB_ADDR_TOR   0x80700000
#define SGXLIB_MAX_ARG  512

// Add New STUB_ADDR for Tor specific trampoline operations

typedef enum {
    FUNC_UNSET_TOR,
    FUNC_OPEN,
    FUNC_CLOSE,
    FUNC_MKDIR,
    FUNC_MKNOD,
    FUNC_WRITE,
    FUNC_READ,
    FUNC_SNPRINTF,
    FUNC_TIME
}fcode_t_tor;

typedef struct sgx_stub_info_tor {
    int   abi;
    void  *trampoline;
    tcs_t *tcs;

    // in : from non-enclave to enclave
    char in_data1[SGXLIB_MAX_ARG];
    char in_data2[SGXLIB_MAX_ARG];
    int  in_arg1;
    int  in_arg2;

    // out : from enclave to non-enclave
    fcode_t_tor fcode;
    int  out_arg1;
    int  out_arg2;
    char out_data1[SGXLIB_MAX_ARG];
    char out_data2[SGXLIB_MAX_ARG];
    char out_data3[SGXLIB_MAX_ARG];
}sgx_stub_info_tor;

extern void sgx_trampoline_tor(void);
