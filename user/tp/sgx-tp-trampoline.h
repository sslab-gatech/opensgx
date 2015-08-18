#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <err.h>
#include <assert.h>
#include <time.h>
#include <sgx.h>

//about a page
#define STUB_ADDR_TP   0x80700000
#define SGXLIB_MAX_ARG  512

// Add New STUB_ADDR for TP specific trampoline operations

typedef enum {
    FUNC_UNSET_TP,
    FUNC_GMTIME,
    FUNC_PRINT_BYTES,
    FUNC_DEBUG // To be added.
} fcode_t_tp;

typedef struct sgx_stub_info_tp {
    int   abi;
    void  *trampoline;
    tcs_t *tcs;

    // in : from non-enclave to enclave
    char in_data1[SGXLIB_MAX_ARG];
    char in_data2[SGXLIB_MAX_ARG];
    int  in_arg1;
    int  in_arg2;
    struct tm in_tm;

    // out : from enclave to non-enclave
    fcode_t_tp fcode;
    int  out_arg1;
    int  out_arg2;
    int  out_arg3;
    time_t out_arg4;
    char out_data1[SGXLIB_MAX_ARG];
    char out_data2[SGXLIB_MAX_ARG];
    char out_data3[SGXLIB_MAX_ARG];
} sgx_stub_info_tp;

extern void sgx_trampoline_tp(void);
