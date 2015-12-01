#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include "libc.h"
#include "atomic.h"
#include "pthread_impl.h"

#include <stdarg.h>
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

static uint64_t cur_heap_ptr = 0x0;
static uint64_t heap_end = 0x0;
static int has_initialized = 0;
static void *managed_memory_start = 0;
static int g_total_chunk = 0;

struct mem_control_block {
    int is_available;
    int size;
};

void _malloc_init() {
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    stub->fcode = FUNC_MALLOC;
    stub->mcode = MALLOC_INIT;

	// Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    cur_heap_ptr = stub->heap_beg;
    heap_end = stub->heap_end;

    managed_memory_start = (void *)(uintptr_t)stub->heap_beg;
    has_initialized = 1;
    g_total_chunk = 0;
}

void *malloc(size_t numbytes) {
    //the below mechanism is largely from "Inside memory management from IBM"
    void *current_location;
    struct mem_control_block *current_location_mcb;
    void *memory_location;
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    if (!has_initialized) {
        _malloc_init();
    }
     numbytes = numbytes + sizeof(struct mem_control_block);
     memory_location = 0;

     current_location = managed_memory_start;

     int total_chunk = g_total_chunk;
     while (total_chunk) {
          current_location_mcb = (struct mem_control_block *)current_location;
          if (current_location_mcb->is_available) {
               if (current_location_mcb->size >= numbytes) {
                    current_location_mcb->is_available = 0;
                    memory_location = current_location;
                    break;
               }
          }
          current_location =
	          (struct mem_control_block *)((uintptr_t *)current_location +
                                                      current_location_mcb->size);
          total_chunk--;
     }
     if (!memory_location) {
          void *last_heap_ptr = (void *)cur_heap_ptr;
          unsigned long extra_secinfo_size = sizeof(secinfo_t) + (SECINFO_ALIGN_SIZE - 1);

          if ((cur_heap_ptr + extra_secinfo_size + numbytes ) > heap_end) {
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
             _enclu(ENCLU_EACCEPT, (uint64_t)secinfo, (uint64_t)pending_page, 0, &out);  // Check whether OS-provided pending page is legitimate for EPC heap area
             if (out.oeax == 0) {   // No error occurred in EACCEPT
                 heap_end += PAGE_SIZE;
             } else {
                 return NULL;
             }
          }
          cur_heap_ptr = (unsigned long)last_heap_ptr + numbytes;
          memory_location = last_heap_ptr;

          current_location_mcb = memory_location;
          current_location_mcb->is_available = 0;
          current_location_mcb->size = numbytes;
          g_total_chunk++; //g_total_chunk is added only when a new chunk is allocted (excluding the case of a using previously used chunk)
     }
     memory_location =
         (struct mem_control_block *)((uintptr_t *)memory_location +
                                                 sizeof(struct mem_control_block));
     return memory_location;
}

void free(void *ptr) {
     struct mem_control_block *mcb;
     mcb = (struct mem_control_block *) ((uintptr_t *)ptr - sizeof(struct mem_control_block));
     mcb->is_available = 1;
     unsigned int chunk_size = mcb->size - sizeof(struct mem_control_block);
     memset(ptr,0,chunk_size);
     return;
}

void *realloc(void *ptr, size_t size){
    void *new;
    if (ptr == NULL) {
        return malloc(size);
    } else {
        if (size == 0) {
             free(ptr);
             return NULL;
        }
        new = malloc(size);
        if (new != NULL) {
            //if new size > old size, old_size+alpha is written to new. Thus, some of garbage values would be copied
            //if old size > new size, new_size is written to new. Thus, some of old values would be lossed
            //sgx_print_hex(new);
            memcpy(new, ptr, size);
            return new;
        } else {
            return NULL;
        }
    }
}
