#pragma once

#define SGX_KERNEL
#include <sgx.h>

#define EPC_ADDR       0x40008000

// linear address is in fact just addr of epc page (physical page)
static inline
void* epc_to_vaddr(epc_t *epc) {
    return epc;
}

typedef enum {
    FREE_PAGE = 0x0,
    SECS_PAGE = 0x1,
    TCS_PAGE  = 0x2,
    REG_PAGE  = 0x3,
    RESERVED  = 0x4
} epc_type_t;

typedef struct {
    int key;
    epc_type_t type;
} epc_info_t;


// exported
extern void init_epc(int nepc);

extern epc_t *get_epc(int key, epc_type_t pt);
extern epc_t *get_epc_region_beg(void);
extern epc_t *get_epc_region_end(void);
extern epc_t *alloc_epc_pages(int npages, int key);
extern epc_t *alloc_epc_page(int key);
extern void free_epc_pages(epc_t *epc);

extern void dbg_dump_epc(void);

extern int find_epc_type(void *addr);

extern void free_reserved_epc_pages(epc_t *epc);
