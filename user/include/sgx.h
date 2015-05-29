#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// polarssl related headers
#include <polarssl/ctr_drbg.h>
#include <polarssl/entropy.h>
#include <polarssl/rsa.h>
#include <polarssl/sha1.h>
#include <polarssl/sha256.h>
#include <polarssl/aes_cmac128.h>

#define OPENSGX_ABI_VERSION 1
#define SGX_USERLIB
#include "../../qemu/target-i386/sgx.h"

#include "../../qemu/target-i386/crypto.h"

typedef struct {
    uint32_t oeax;
    uint64_t orbx;
    uint64_t orcx;
    uint64_t ordx;
} out_regs_t;

// round size to pages
static inline
int to_npages(int size) {
    if (size == 0)
        return 0;
    return (size - 1) / PAGE_SIZE + 1;
}

// OS resource management for enclave
#define MAX_ENCLAVES 16

typedef struct {
    unsigned int mode_switch;
    unsigned int tlbflush_n;

    unsigned int encls_n;
    unsigned int ecreate_n;
    unsigned int eadd_n;
    unsigned int eextend_n;
    unsigned int einit_n;
    unsigned int eaug_n;
 
    unsigned int enclu_n;
    unsigned int eenter_n;
    unsigned int eresume_n;
    unsigned int eexit_n;
    unsigned int egetkey_n;
    unsigned int ereport_n;
    unsigned int eaccept_n;
} qstat_t;

typedef struct {
    int keid;
    uint64_t enclave;
    tcs_t *tcs;
    // XXX. stats
    unsigned int kin_n;
    unsigned int kout_n;
    unsigned long prealloc_ssa;
    unsigned long prealloc_heap;
    unsigned long augged_heap;

    qstat_t qstat;
} keid_t;

// user-level libs
extern void enclu(enclu_cmd_t leaf, uint64_t rbx, uint64_t rcx, uint64_t rdx,
                  out_regs_t* out_regs);
tcs_t *run_enclave(void *entry, void *codes, unsigned int n_of_pages, char *conf);

extern tcs_t *test_run_enclave(void *entry, void *codes, unsigned int n_of_code_pages);
extern void exception_handler(void);
extern void make_sigstruct_before_sign(sigstruct_t *sig, void *entry, size_t size,
                                       tcs_t *tcs);
extern void make_sigstruct_after_sign(sigstruct_t *sig, uint8_t *pubKey, rsa_sig_t signature);
extern void make_einittoken_before_MAC(uint8_t *pubKey, sigstruct_t *sig);
extern void make_einittoken_after_MAC(void);

extern void tb_splitter();
extern void testEnclaveAccess(void);
