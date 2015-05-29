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
#define STUB_ADDR       0x80800000
#define HEAP_ADDR       0x80900000
#define SGXLIB_MAX_ARG  512

typedef enum {
    FUNC_UNSET,
    FUNC_PUTS,
    FUNC_CLOSE_SOCK,
    FUNC_SEND,
    FUNC_RECV,

    FUNC_MALLOC,
    FUNC_FREE,

    FUNC_SYSCALL,
    PRINT_HEX,
    // ...
}fcode_t;

typedef enum {
    MALLOC_UNSET,
    MALLOC_INIT,
    REQUEST_EAUG,
} mcode_t;


typedef struct sgx_stub_info {
    int   abi;
    void  *trampoline;
    tcs_t *tcs;

    // in : from non-enclave to enclave
    unsigned long heap_beg;
    unsigned long heap_end;
    unsigned long pending_page;
    int  ret;
    char in_data1[SGXLIB_MAX_ARG];
    char in_data2[SGXLIB_MAX_ARG];
    int  in_arg1;
    int  in_arg2;

    // out : from enclave to non-enclave
    fcode_t fcode;
    mcode_t mcode;
    unsigned long addr;
    int  out_arg1;
    int  out_arg2;
    char out_data1[SGXLIB_MAX_ARG];
    char out_data2[SGXLIB_MAX_ARG];
    char out_data3[SGXLIB_MAX_ARG];
} sgx_stub_info;

extern void execute_code(void);
extern void sgx_trampoline(void);
extern int sgx_init(void);
