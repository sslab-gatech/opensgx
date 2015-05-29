#pragma once

#include <sgx.h>
#include <sgx-user.h>
#include <sgx-kern.h>


#define sgx_exit(ptr) {                         \
    asm volatile("movl %0, %%eax\n\t"           \
                 "movq %1, %%rbx\n\t"           \
                 ".byte 0x0F\n\t"               \
                 ".byte 0x01\n\t"               \
                 ".byte 0xd7\n\t"               \
                 :                              \
                 :"a"((uint32_t)ENCLU_EEXIT),   \
                  "b"((uint64_t)ptr));          \
}

#define sgx_report(tgtinfo, rptdata, output) {  \
    asm volatile("movl %0, %%eax\n\t"           \
                 "movq %1, %%rbx\n\t"           \
                 "movq %2, %%rcx\n\t"           \
                 "movq %3, %%rdx\n\t"           \
                 ".byte 0x0F\n\t"               \
                 ".byte 0x01\n\t"               \
                 ".byte 0xd7\n\t"               \
                 :                              \
                 :"a"((uint32_t)ENCLU_EREPORT), \
                  "b"((uint64_t)tgtinfo),       \
                  "c"((uint64_t)rptdata),       \
                  "d"((uint64_t)output));       \
}

#define sgx_getkey(keyreq, output) {            \
    asm volatile("movl %0, %%eax\n\t"           \
                 "movq %1, %%rbx\n\t"           \
                 "movq %2, %%rcx\n\t"           \
                 ".byte 0x0F\n\t"               \
                 ".byte 0x01\n\t"               \
                 ".byte 0xd7\n\t"               \
                 :                              \
                 :"a"((uint32_t)ENCLU_EGETKEY), \
                  "b"((uint64_t)keyreq),        \
                  "c"((uint64_t)output));       \
}

extern unsigned long cur_heap_ptr;
extern unsigned long heap_end;

extern void sgx_print_hex(unsigned long addr);
extern void _enclu(enclu_cmd_t leaf, uint64_t rbx, uint64_t rcx, uint64_t rdx,
           out_regs_t *out_regs);
extern void *sgx_malloc(int size);
extern void *sgx_realloc(void *ptr, size_t size);
extern void *sgx_memalign(size_t align, size_t size);
extern void sgx_free(void *ptr);
extern void sgx_puts(char buf[]);
extern size_t sgx_strlen(const char *string);
extern int sgx_strcmp (const char *str1, const char *str2);
extern int sgx_memcmp (const void *ptr1, const void *ptr2, size_t num);
extern void *sgx_memset (void *ptr, int value, size_t num);
extern void *sgx_memcpy (void *dest, const void *src, size_t size);
extern int sgx_recv(const char *port, const char *buf);
extern int sgx_send(const char *ip, const char *port, const void *msg, size_t length);
extern void sgx_close_sock(void);
