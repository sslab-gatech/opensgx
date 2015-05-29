#pragma once

#include <sgx.h>

extern bool sys_sgx_init(void);
extern int syscall_create_enclave(void *entry, void *codes, unsigned int code_pages, 
                                  tcs_t *tcs, sigstruct_t *sig, einittoken_t *token, 
                                  int intel_flag);
extern int syscall_stat_enclave(int keid, keid_t *stat);
extern unsigned long get_epc_heap_beg();
extern unsigned long get_epc_heap_end();
extern unsigned long syscall_execute_EAUG();

// For unit test
void test_ecreate(pageinfo_t *pageinfo, epc_t *epc);
int test_einit(uint64_t sigstruct, uint64_t secs, uint64_t einittoken);
void test_eadd(pageinfo_t *pageinfo, epc_t *epc);
void test_eextend(uint64_t pageChunk);
void test_eaug(pageinfo_t *pageinfo, epc_t *epc);

// XXX: may need these during testing
int test_alloc_keid(void);

