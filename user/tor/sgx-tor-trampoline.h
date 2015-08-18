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
/*
#include "sgx-tor-trampoline.h"
#include <string.h>
#include <sgx-kern.h>
#include <sgx-user.h>
#include <sgx-utils.h>
#include <sgx-crypto.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sgx-malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

#include <openssl/objects.h>
#include <openssl/ssl.h>
*/

//about a page
#define STUB_ADDR_TOR   0x80700000
#define SGXLIB_MAX_ARG  512

// Add New STUB_ADDR for Tor specific trampoline operations

typedef enum {
    FUNC_UNSET_TOR,
    FUNC_OPEN,
    FUNC_MKDIR,
    FUNC_MKNOD,
    FUNC_SNPRINTF,
    FUNC_QSORT,
    FUNC_STRNCASECMP,
    FUNC_STRNCMP,
    FUNC_ISSPACE,
    FUNC_TOLOWER,
    FUNC_ISDIGIT,
    FUNC_DEBUG,
    FUNC_LOCALTIME,
    FUNC_GMTIME,
    FUNC_STRFTIME,
    FUNC_MKTIME,
    FUNC_PRINT_BYTES // To be added
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
    size_t in_arg3;
    struct tm in_tm;

    // out : from enclave to non-enclave
    fcode_t_tor fcode;
    int  out_arg1;
    int  out_arg2;
    int  out_arg3;
    time_t out_arg4;
    struct tm out_tm;
    char out_data1[SGXLIB_MAX_ARG];
    char out_data2[SGXLIB_MAX_ARG];
    char out_data3[SGXLIB_MAX_ARG];
}sgx_stub_info_tor;

extern void sgx_trampoline_tor(void);
