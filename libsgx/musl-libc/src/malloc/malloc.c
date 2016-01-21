#define _GNU_SOURCE
#include <stdlib.h>
#include "libc.h"

#include <sgx-lib.h>

static
void _enclu(enclu_cmd_t leaf, uint64_t rbx, uint64_t rcx, uint64_t rdx,
            out_regs_t *out_regs)
{
   out_regs_t tmp;
   __asm__ __volatile__(".byte 0x0F\n\t"
                        ".byte 0x01\n\t"
                        ".byte 0xd7\n\t"
                        :"=a"(tmp.oeax),
                         "=b"(tmp.orbx),
                         "=c"(tmp.orcx),
                         "=d"(tmp.ordx)
                        :"a"((uint32_t)leaf),
                         "b"(rbx),
                         "c"(rcx),
                         "d"(rdx)
                        :"memory");

    // Check whether function requires out_regs
    if (out_regs != NULL) {
        __asm__ __volatile__ ("" : : : "memory"); // Compile time Barrier
        __asm__ __volatile__ ("movl %%eax, %0\n\t"
            "movq %%rbx, %1\n\t"
            "movq %%rcx, %2\n\t"
            "movq %%rdx, %3\n\t"
            :"=a"(out_regs->oeax),
             "=b"(out_regs->orbx),
             "=c"(out_regs->orcx),
             "=d"(out_regs->ordx));
    }
}

static
void* morecore(void) {
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    secinfo_t *secinfo = memalign(SECINFO_ALIGN_SIZE, sizeof(secinfo_t));

    secinfo->flags.r = 1;
    secinfo->flags.w = 1;
    secinfo->flags.x = 0;
    secinfo->flags.pending = 1;
    secinfo->flags.modified = 0;
    secinfo->flags.reserved1 = 0;
    secinfo->flags.page_type = PT_REG;
    int i = 0;
    for (i = 0 ; i< 6; i++) {
        secinfo->flags.reserved2[i] = 0;
    }

    stub->fcode = FUNC_MALLOC;
    stub->mcode = REQUEST_EAUG;
    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);
    unsigned long pending_page = stub->pending_page;

    // EACCEPT should be called with [RBX:the address of secinfo, RCX:the adress of pending page]
    out_regs_t out;
    _enclu(ENCLU_EACCEPT, (uint64_t)secinfo, (uint64_t)pending_page, 0, &out);
    if (out.oeax == 0) {   // No error occurred in EACCEPT
        return (void*)pending_page;
    } else {
        return NULL;
    }
}

#define MMAP(s) morecore()
#define ONLY_MSPACES 1
#define USE_LOCKS 0
#define SINGLE_PAGE_EAUG 1
#include "dlmalloc.inc" /* XXX: ugly include .. updating dlmalloc.inc does not trigger make */

static mspace _ms = NULL;

uint64_t heap_start = 0x0;
uint64_t heap_size = 0x0;

void _malloc_init() {
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    stub->fcode = FUNC_MALLOC;
    stub->mcode = MALLOC_INIT;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    heap_start = stub->heap_beg;
    heap_size = stub->heap_end - stub->heap_beg;
    printf("heap = %lx, size = %lx\n", heap_start, heap_size);

    _ms = create_mspace_with_base((void*)heap_start,
            (size_t)heap_size, 0);
}

void* malloc(size_t bytes) {
    if (!_ms) _malloc_init();
    return mspace_malloc(_ms, bytes);
}

void free(void* mem) {
    if (!_ms) _malloc_init();
    mspace_free(_ms, mem);
}

void* calloc(size_t n_elements, size_t elem_size) {
    if (!_ms) _malloc_init();
    return mspace_calloc(_ms, n_elements, elem_size);
}

void* realloc(void* oldMem, size_t bytes) {
    if (!_ms) _malloc_init();
    return mspace_realloc(_ms, oldMem, bytes);
}

void* __memalign(size_t alignment, size_t bytes) {
    if (!_ms) _malloc_init();
    return mspace_memalign(_ms, alignment, bytes);
}

size_t malloc_usable_size(const void* mem) {
    if (!_ms) _malloc_init();
    return mspace_usable_size(mem);
}

int posix_memalign(void** memptr, size_t alignment, size_t bytes) {
    if (alignment < sizeof(void *)) return EINVAL;
    if (!memptr) return 1;
    if (!_ms) _malloc_init();
    *memptr = mspace_memalign(_ms, alignment, bytes);
    if (!*memptr) return 1;
    return 0;
}

/* add if needed
struct mallinfo mallinfo() {
    return mspace_mallinfo(_ms);
}

// already defined in src/legacy/valloc.c
void* valloc(size_t bytes) {
    if (!_ms) _malloc_init();
    return mspace_memalign(_ms, PAGE_SIZE, bytes);
}
*/

weak_alias(__memalign, memalign);
