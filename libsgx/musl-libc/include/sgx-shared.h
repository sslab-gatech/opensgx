/*
 *  Copyright (C) 2015, OpenSGX team, Georgia Tech & KAIST, All Rights Reserved
 *
 *  This file is part of OpenSGX (https://github.com/sslab-gatech/opensgx).
 *
 *  OpenSGX is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenSGX is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenSGX.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <inttypes.h>
#include <time.h>

#define PAGE_SIZE  4096

// Hardware configuration
#define CPU_SVN                  1
#define PAGE_SIZE                4096
#define MEASUREMENT_SIZE         256
#define NUM_EPC                  2000
#define MIN_ALLOC                2

// Enclave configuration
#define STACK_PAGE_FRAMES_PER_THREAD 250
#define HEAP_PAGE_FRAMES             300

// EINITTOKEN MAC size
#define MAC_SIZE               16

// For ALIGNMENT
#define EINITTOKEN_ALIGN_SIZE  512
#define PAGEINFO_ALIGN_SIZE    32
#define SECINFO_ALIGN_SIZE     64

// For RSA
#define KEY_LENGTH             384
#define KEY_LENGTH_BITS        3072
#define DEVICE_KEY_LENGTH      16
#define DEVICE_KEY_LENGTH_BITS 128
#define SGX_RSA_EXPONENT       3
#define HASH_SIZE              20

typedef uint8_t rsa_key_t[KEY_LENGTH];
typedef uint8_t rsa_sig_t[KEY_LENGTH];

#define SIG_HEADER1 \
    {0x06, 0x00, 0x00, 0x00, 0xE1, 0x00, 0x00, 0x00, \
     0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}
#define SIG_HEADER2 \
    {0x01, 0x01, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, \
     0x60, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}

//about a page
#define STUB_ADDR       0x80800000
#define HEAP_ADDR       0x80900000
#define SGXLIB_MAX_ARG  512

typedef enum {
    FUNC_UNSET,
    FUNC_PUTS,
    FUNC_PUTCHAR,

    FUNC_MALLOC,
    FUNC_FREE,

    FUNC_SYSCALL,
    FUNC_READ,
    FUNC_WRITE,
    FUNC_CLOSE,

    FUNC_GMTIME,
    FUNC_TIME,
    FUNC_SOCKET,
    FUNC_BIND,
    FUNC_LISTEN,
    FUNC_ACCEPT,
    FUNC_CONNECT,
    FUNC_SEND,
    FUNC_RECV
    // ...
} fcode_t;

typedef enum {
    MALLOC_UNSET,
    MALLOC_INIT,
    REQUEST_EAUG,
} mcode_t;

typedef struct sgx_stub_info {
    int  abi;
    void *trampoline;
    void *tcs;

    // in : from non-enclave to enclave
    uint64_t heap_beg;
    uint64_t heap_end;
    uint32_t pending_page;
    int  ret;
    char in_data1[SGXLIB_MAX_ARG];
    char in_data2[SGXLIB_MAX_ARG];
    char in_shm[SGXLIB_MAX_ARG];
    int  in_arg1;
    int  in_arg2;
    uint32_t in_arg3;
    struct tm in_tm;

   // out : from enclave to non-enclave
   fcode_t fcode;
   mcode_t mcode;
   uint32_t *addr;
   int  out_arg1;
   int  out_arg2;
   int  out_arg3;
   time_t out_arg4;
   char out_data1[SGXLIB_MAX_ARG];
   char out_data2[SGXLIB_MAX_ARG];
   char out_data3[SGXLIB_MAX_ARG];
   char out_shm[SGXLIB_MAX_ARG];
} sgx_stub_info;


typedef enum {
    ENCLS_ECREATE      = 0x00,
    ENCLS_EADD         = 0x01,
    ENCLS_EINIT        = 0x02,
    ENCLS_EREMOVE      = 0x03,
    ENCLS_EDBGRD       = 0x04,
    ENCLS_EDBGWR       = 0x05,
    ENCLS_EEXTEND      = 0x06,
    ENCLS_ELDB         = 0x07,
    ENCLS_ELDU         = 0x08,
    ENCLS_EBLOCK       = 0x09,
    ENCLS_EPA          = 0x0A,
    ENCLS_EWB          = 0x0B,
    ENCLS_ETRACK       = 0x0C,
    ENCLS_EAUG         = 0x0D,
    ENCLS_EMODPR       = 0x0E,
    ENCLS_EMODT        = 0x0F,

    // custom hypercalls
    ENCLS_OSGX_INIT      = 0x10,
    ENCLS_OSGX_PUBKEY    = 0x11,
    ENCLS_OSGX_EPCM_CLR  = 0x12,
    ENCLS_OSGX_CPUSVN    = 0x13,
    ENCLS_OSGX_STAT      = 0x14,
    ENCLS_OSGX_SET_STACK = 0x15,
} encls_cmd_t;

typedef enum {
   ENCLU_EREPORT      = 0x00,
   ENCLU_EGETKEY      = 0x01,
   ENCLU_EENTER       = 0x02,
   ENCLU_ERESUME      = 0x03,
   ENCLU_EEXIT        = 0x04,
   ENCLU_EACCEPT      = 0x05,
   ENCLU_EMODPE       = 0x06,
   ENCLU_EACCEPTCOPY  = 0x07,
} enclu_cmd_t;

typedef enum {
   PT_SECS = 0x00,
   PT_TCS  = 0x01,
   PT_REG  = 0x02,
   PT_VA   = 0x03,
   PT_TRIM = 0x04
} page_type_t;

typedef enum {
   LAUNCH_KEY         = 0x00,          //!< Launch key
   PROVISION_KEY      = 0x01,          //!< Provisioning Key
   PROVISION_SEAL_KEY = 0x02,          //!< Provisioning Seal Key
   REPORT_KEY         = 0x03,          //!< Report Key
   SEAL_KEY           = 0x04,          //!< Report seal key
} keyname_type_t;

typedef struct {
   uint32_t oeax;
   uint64_t orbx;
   uint64_t orcx;
   uint64_t ordx;
} out_regs_t;

// SGX Data structures
typedef struct {
    uint8_t byte_in_page[512];
} epcpage_t;
typedef unsigned char epc_t[PAGE_SIZE];

typedef struct {
   uint64_t linaddr;
   uint64_t srcpge;
   uint64_t secinfo;
   uint64_t secs;
 } pageinfo_t;

typedef struct  {
   unsigned int r:1;
   unsigned int w:1;
   unsigned int x:1;
   unsigned int pending:1;
   unsigned int modified:1;
   unsigned int reserved1:3;
   uint8_t page_type;
   uint8_t reserved2[6];
} secinfo_flags_t;

typedef struct {
   secinfo_flags_t flags;
   uint64_t reserved[7];
} secinfo_t;

typedef struct {
    unsigned int dbgoptin:1;
    unsigned int reserved1:31;
    uint32_t reserved2;
} tcs_flags_t;

typedef struct {
    uint64_t reserved1;
    tcs_flags_t flags;                  //!< Thread's Execution Flags
    uint64_t ossa;
    uint32_t cssa;
    uint32_t nssa;
    uint64_t oentry;
    uint64_t reserved2;
    uint64_t ofsbasgx;                  //!< Added to Base Address of Enclave to get FS Address
    uint64_t ogsbasgx;                  //!< Added to Base Address of Enclave to get GS Address
    uint32_t fslimit;
    uint32_t gslimit;
    uint64_t reserved3[503];
} tcs_t;

typedef struct  {
    unsigned int reserved1 : 1;
    unsigned int debug : 1;             //!< If 1, enclave permits debugger to r/w
    unsigned int mode64bit : 1;         //!< Enclave runs in 64- bit mode
    unsigned int reserved2 : 1;
    unsigned int provisionkey : 1;      //!< "" available from EGETKEY
    unsigned int einittokenkey : 1;     //!< "" available from EGETKEY
    unsigned int reserved3 : 2;         //!< 63:6 (58 bits) is reserved
    uint8_t      reserved4[7];
    uint64_t     xfrm;                  //!< XSAVE Feature Request Mask
} attributes_t;

typedef struct {
    unsigned int exinfo : 1;
    unsigned int reserved1 : 7;
    uint8_t      reserved2[3];
} miscselect_t;

typedef struct {
    uint8_t      header[16];
    uint32_t     vendor;
    uint32_t     date;
    uint8_t      header2[16];
    uint32_t     swdefined;
    uint8_t      reserved1[84];
    uint8_t      modulus[384];
    uint32_t     exponent;
    uint8_t      signature[384];
    miscselect_t miscselect;
    miscselect_t miscmask;
    uint8_t      reserved2[20];
    attributes_t attributes;
    attributes_t attributeMask;
    uint8_t      enclaveHash[32];
    uint8_t      reserved3[32];
    uint16_t     isvProdID;
    uint16_t     isvSvn;
    uint8_t      reserved4[12];
    uint8_t      q1[384]; 
    uint8_t      q2[384];
} sigstruct_t;

typedef struct {
    uint32_t     valid;
    uint8_t      reserved1[44];
    attributes_t attributes;
    uint8_t      mrEnclave[32];
    uint8_t      reserved2[32];
    uint8_t      mrSigner[32];
    uint8_t      reserved3[32];
    uint8_t      cpuSvnLE[16];
    uint16_t     isvprodIDLE;
    uint16_t     isvsvnLE;
    uint8_t      reserved4[24];
    miscselect_t maskedmiscSelectLE;
    attributes_t maskedAttributesLE;
    uint8_t      keyid[32];
    uint8_t      mac[16];
} einittoken_t;

typedef struct {
    uint8_t      cpusvn[16];
    miscselect_t miscselect;
    uint8_t      reserved[28];
    attributes_t attributes;
    uint8_t      mrenclave[32];
    uint8_t      reserved2[32];
    uint8_t      mrsigner[32];
    uint8_t      reserved3[96];
    uint16_t     isvProdID;
    uint16_t     isvsvn;
    uint8_t      reserved4[60];
    uint8_t      reportData[64];
    uint8_t      keyid[32];
    uint8_t      mac[16];
} report_t;

typedef struct {
    uint8_t      measurement[32];
    attributes_t attributes;
    uint8_t      reserved1[4];
    miscselect_t miscselect;
    uint8_t      reserved2[456];
} targetinfo_t;

#define FIRST_PKCS1_5_PADDING \
    {0x00, 0x01}

#define LAST_PKCS1_5_PADDING \
    {0x00, 0x30, 0x31, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20}


typedef struct {
    unsigned int mrenclave:1;
    unsigned int mrsigner:1;
    unsigned int reserved:14;
} keypolicy_t;

typedef struct {
    uint16_t     keyname;
    keypolicy_t  keypolicy;
    uint16_t     isvsvn;
    uint16_t     reserved1;
    uint8_t      cpusvn[16];
    attributes_t attributeMask;
    uint8_t      keyid[32];
    miscselect_t miscmask;
    uint8_t      reserved2[436];
} keyrequest_t;

typedef struct {
    keyname_type_t keyname;
    uint16_t       isvprodID;
    uint16_t       isvsvn;
    uint64_t       ownerEpoch[2];
    attributes_t   attributes;
    attributes_t   attributesMask;
    uint8_t        mrEnclave[32];
    uint8_t        mrSigner[32];
    uint8_t        keyid[32];
    uint8_t        seal_key_fuses[16];
    uint8_t        cpusvn[16];
    miscselect_t   miscselect;
    miscselect_t   miscmask;
    uint64_t       padding[44];
} keydep_t;

typedef struct {
    uint64_t eid;
    uint64_t padding[44];
} secs_eid_pad_t;

typedef union {
    secs_eid_pad_t eid_pad;
    uint8_t reserved[3828];
} secs_eid_reserved_t;

typedef struct {
    uint64_t            size;
    uint64_t            baseAddr;
    uint32_t            ssaFrameSize;
    miscselect_t        miscselect;
    uint8_t             reserved1[24];
    attributes_t        attributes;
    uint8_t             mrEnclave[32];
    uint8_t             reserved2[32];
    uint8_t             mrSigner[32];
    uint8_t             reserved3[96];
    uint16_t            isvprodID;
    uint16_t            isvsvn;
    uint64_t            mrEnclaveUpdateCounter; 
    secs_eid_reserved_t eid_reserved;
} secs_t;
