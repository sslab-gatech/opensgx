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

#include <sgx-lib.h>
#include <stdarg.h>

// one 4k page : enclave page & offset

static unsigned long cur_heap_ptr = 0x0;
static unsigned long heap_end = 0x0;
static int has_initialized = 0;
static void *managed_memory_start = 0;
static int g_total_chunk = 0;

void _enclu(enclu_cmd_t leaf, uint64_t rbx, uint64_t rcx, uint64_t rdx,
           out_regs_t *out_regs)
{
   out_regs_t tmp;
   asm volatile(".byte 0x0F\n\t"
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
        asm volatile ("" : : : "memory"); // Compile time Barrier
        asm volatile ("movl %%eax, %0\n\t"
            "movq %%rbx, %1\n\t"
            "movq %%rcx, %2\n\t"
            "movq %%rdx, %3\n\t"
            :"=a"(out_regs->oeax),
             "=b"(out_regs->orbx),
             "=c"(out_regs->orcx),
             "=d"(out_regs->ordx));
    }
}

void sgx_malloc_init() {
     sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
     stub->fcode = FUNC_MALLOC;
     stub->mcode = MALLOC_INIT;
     // Enclave exit & jump into user-space trampoline
     sgx_exit(stub->trampoline);

     cur_heap_ptr = (unsigned long)stub->heap_beg;
     heap_end = (unsigned long)stub->heap_end;

     ////
     managed_memory_start = (unsigned long)stub->heap_beg;
     has_initialized = 1;
     g_total_chunk = 0;
}

void sgx_free(void *ptr) {
     struct mem_control_block *mcb;
     mcb = ptr - sizeof(struct mem_control_block);
     mcb->is_available = 1;
     unsigned int chunk_size = mcb->size - sizeof(struct mem_control_block);
     sgx_memset(ptr,0,chunk_size); 
     return;
}

void *sgx_malloc(size_t numbytes) {
     //the below mechanism is largely from "Inside memory management from IBM"
     void *current_location;
     struct mem_control_block *current_location_mcb;
     void *memory_location;
     sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

     if (!has_initialized) {
          sgx_malloc_init();
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
          current_location = current_location + current_location_mcb->size;
          total_chunk--;
     }
     if (!memory_location) {
          void *last_heap_ptr = (void *)cur_heap_ptr;
          unsigned long extra_secinfo_size = sizeof(secinfo_t) + (SECINFO_ALIGN_SIZE - 1);

          if ((cur_heap_ptr + extra_secinfo_size + numbytes ) > heap_end) {
             secinfo_t *secinfo = sgx_memalign(SECINFO_ALIGN_SIZE, sizeof(secinfo_t));

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
     memory_location = memory_location + sizeof(struct mem_control_block);
     return memory_location;
}

void *sgx_realloc(void *ptr, size_t size){
    void *new;
    if (ptr == NULL) {
        return sgx_malloc(size);
    } else {
        if (size == 0) {
             sgx_free(ptr);
             return NULL;
        }
        new = sgx_malloc(size);
        if (new != NULL) {
            //if new size > old size, old_size+alpha is written to new. Thus, some of garbage values would be copied
            //if old size > new size, new_size is written to new. Thus, some of old values would be lossed
            //sgx_print_hex(new);
            sgx_memcpy(new, ptr, size);
            return new;
        } else {
            return NULL;
        }
    }
}

void *sgx_memalign(size_t align, size_t size) {

    void *mem = sgx_malloc(size + (align - 1));
    void *ptr = (void *)(((unsigned long)mem + ((unsigned long)align - 1)) & ~ ((unsigned long)align - 1));

    return ptr;
}

void sgx_puts(char buf[]) {

    size_t size = sgx_strlen(buf);
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    // puts
    stub->fcode = FUNC_PUTS;
    sgx_memcpy(stub->out_data1, buf, size);

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);
}

void *sgx_memcpy (void *dest, const void *src, size_t size)
{
    asm volatile("" ::: "memory");
    asm volatile("movq %0, %%rdi\n\t"
                 "movq %1, %%rsi\n\t"
                 "movl %2, %%ecx\n\t"
                 "rep movsb \n\t"
                 :
                 :"a"((uint64_t)dest),
                  "b"((uint64_t)src),
                  "c"((uint32_t)size));

    return dest;
}

void *sgx_memmove(void *dest, const void *src, size_t size)
{
    asm volatile("" ::: "memory");
    asm volatile("movq %0, %%rdi\n\t"
                 "movq %1, %%rsi\n\t"
                 "movl %2, %%ecx\n\t"
                 "rep movsb \n\t"
                 :
                 :"a"((uint64_t)dest),
                  "b"((uint64_t)src),
                  "c"((uint32_t)size));

    return dest;
}

time_t sgx_time(time_t *t)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_TIME;

    sgx_exit(stub->trampoline);

    if (t != NULL)
        sgx_memcpy(t, stub->out_data1, sizeof(time_t));

    return stub->in_arg3;
}


ssize_t sgx_write(int fd, const void *buf, size_t count)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    int tmp_len;

    for(int i=0;i<count/SGXLIB_MAX_ARG+1;i++) {
        stub->fcode = FUNC_WRITE;
        stub->out_arg1 = fd;

        if(i == count/SGXLIB_MAX_ARG)
            tmp_len = (int)count % SGXLIB_MAX_ARG;
        else
            tmp_len = SGXLIB_MAX_ARG;

        stub->out_arg2 = tmp_len;
        sgx_memcpy(stub->out_data1, buf+i*SGXLIB_MAX_ARG, tmp_len);
        sgx_exit(stub->trampoline);
    }

    return stub->in_arg1;
}

ssize_t sgx_read(int fd, void *buf, size_t count)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    int tmp_len;

    for(int i=0;i<count/SGXLIB_MAX_ARG+1;i++) {
        stub->fcode = FUNC_READ;
        stub->out_arg1 = fd;

        if(i == count/SGXLIB_MAX_ARG)
            tmp_len = (int)count % SGXLIB_MAX_ARG;
        else
            tmp_len = SGXLIB_MAX_ARG;

        stub->out_arg2 = tmp_len;
        sgx_exit(stub->trampoline);
        sgx_memcpy(buf+i*SGXLIB_MAX_ARG, stub->in_data1, tmp_len);
    }

    return stub->in_arg1;
}

int sgx_close(int fd)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_CLOSE;
    stub->out_arg1 = fd;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_socket(int domain, int type, int protocol)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_SOCKET;
    stub->out_arg1 = domain;
    stub->out_arg2 = type;
    stub->out_arg3 = protocol;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_BIND;
    stub->out_arg1 = sockfd;
    sgx_memcpy(stub->out_data1, addr, addrlen);
    stub->out_arg2 = addrlen;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_listen(int sockfd, int backlog)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_LISTEN;
    stub->out_arg1 = sockfd;
    stub->out_arg2 = backlog;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_ACCEPT;
    stub->out_arg1 = sockfd;

    sgx_exit(stub->trampoline);

    sgx_memcpy(addr, stub->out_data1, sizeof(struct sockaddr));
    sgx_memcpy(addrlen, stub->out_data2, sizeof(socklen_t));
    return stub->in_arg1;
}

int sgx_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_CONNECT;
    stub->out_arg1 = sockfd;
    sgx_memcpy(stub->out_data1, addr, addrlen);
    stub->out_arg2 = addrlen;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

ssize_t sgx_send(int fd, const void *buf, size_t len, int flags)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_SEND;
    sgx_memcpy(stub->out_data1, buf, len);
    stub->out_arg1 = fd;
    stub->out_arg2 = (int)len;
    stub->out_arg3 = flags;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

ssize_t sgx_recv(int fd, void *buf, size_t len, int flags)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_RECV;
    stub->out_arg1 = fd;
    stub->out_arg2 = (int)len;
    stub->out_arg3 = flags;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    sgx_memcpy(buf, stub->in_data1, len);

    return stub->in_arg1;
}

int sgx_enclave_read(void *buf, int len)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    if (len <= 0) {
        return -1;
    }
    sgx_memcpy(buf, stub->in_data1, len);
    sgx_memset(stub->in_data1, 0, SGXLIB_MAX_ARG);

    return len;
}

int sgx_enclave_write(void *buf, int len)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    if (len <= 0) {
        return -1;
    }
    sgx_memset(stub->out_data1, 0, SGXLIB_MAX_ARG);
    sgx_memcpy(stub->out_data1, buf, len);

    return len;
}

void sgx_putchar(char c) {
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    stub->out_arg1 = (int)c;
    stub->fcode = FUNC_PUTCHAR;

    sgx_exit(stub->trampoline);
}

static
void printchar(char **str, int c)
{
    sgx_putchar((char)c);
}

#define PAD_RIGHT 1
#define PAD_ZERO 2

static
int prints(char **out, const char *string, int width, int pad) {
    register int pc = 0, padchar = ' ';

    if (width > 0) {
        register int len = 0;
        register const char *ptr;
        for (ptr = string; *ptr; ++ptr) ++len;
        if (len >= width) width = 0;
        else width -= len;
        if (pad & PAD_ZERO) padchar = '0';
    }
    if (!(pad & PAD_RIGHT)) {
        for ( ; width > 0; --width) {
            printchar (out, padchar);
            ++pc;
        }
    }
    for ( ; *string ; ++string) {
        printchar (out, *string);
        ++pc;
    }
    for ( ; width > 0; --width) {
        printchar (out, padchar);
        ++pc;
    }

    return pc;
}

/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 12

static
int printi(char **out, int i, int b, int sg, int width, int pad, int letbase) {
    char print_buf[PRINT_BUF_LEN];
    register char *s;
    register int t, neg = 0, pc = 0;
    register unsigned int u = i;

    if (i == 0) {
        print_buf[0] = '0';
        print_buf[1] = '\0';
        return prints (out, print_buf, width, pad);
    }

    if (sg && b == 10 && i < 0) {
        neg = 1;
        u = -i;
    }

    s = print_buf + PRINT_BUF_LEN-1;
    *s = '\0';

    while (u) {
        t = u % b;
        if ( t >= 10 )
            t += letbase - '0' - 10;
        *--s = t + '0';
        u /= b;
    }

    if (neg) {
        if ( width && (pad & PAD_ZERO) ) {
            printchar (out, '-');
            ++pc;
            --width;
        }
        else {
            *--s = '-';
        }
    }

    return pc + prints (out, s, width, pad);
}

static
int sgx_print(char **out, const char *format, va_list args) {
    register int width, pad;
    register int pc = 0;
    char scr[2];

    for (; *format != 0; ++format) {
        if (*format == '%') {
            ++format;
            width = pad = 0;
            if (*format == '\0') break;
            if (*format == '%') goto out;
            if (*format == '-') {
                ++format;
                pad = PAD_RIGHT;
            }
            while (*format == '0') {
                ++format;
                pad |= PAD_ZERO;
            }
            for ( ; *format >= '0' && *format <= '9'; ++format) {
                width *= 10;
                width += *format - '0';
            }
            if (*format == 's') {
                register void *s = (char *)va_arg( args, int );
                pc += prints (out, s?s:"(null)", width, pad);
                continue;
            }
            if (*format == 'd') {
                pc += printi (out, va_arg( args, int ), 10, 1, width, pad, 'a');
                continue;
            }
            if (*format == 'x') {
                pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'a');
                continue;
            }
            if (*format == 'X') {
                pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'A');
                continue;
            }
            if (*format == 'u') {
                pc += printi (out, va_arg( args, int ), 10, 0, width, pad, 'a');
                continue;
            }
            if (*format == 'c') {
                /* char are converted to int then pushed on the stack */
                scr[0] = (char)va_arg( args, int );
                scr[1] = '\0';
                pc += prints (out, scr, width, pad);
                continue;
            }
        }
        else {
out:
            printchar (out, *format);
            ++pc;
        }
    }
    if (out) **out = '\0';
    va_end(args);
    return pc;
}

int sgx_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    return sgx_print(0, format, args);
}


void sgx_print_hex(unsigned long addr) {
    sgx_printf("%x\n", (unsigned long)addr);
}

int sgx_tolower(int c)
{
  return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

int sgx_toupper(int c)
{
  return c >= 'a' && c <= 'z' ? c - 32 : c;
}

int sgx_islower(int c)
{
  return c >= 'a' && c <= 'z' ?  1 : 0;
}

int sgx_isupper(int c)
{
  return c >= 'A' && c <= 'Z' ? 1 : 0;
}


int sgx_isdigit(int c)
{
    return c >= '0' && c <= '9' ? 1 : 0; 
}

int sgx_isspace(int c)
{
    if (c == ' '  || c == '\t' || c == '\n' ||
        c == '\v' || c == '\f' || c == '\r')
        return 1;

    return 0;
}

int sgx_isalnum(int c)
{
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9'))
        return 1;

    return 0;
}

int sgx_isxdigit(int c)
{
    if ((c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F'))
        return 1;

    return 0;
}
