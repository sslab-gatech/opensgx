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

unsigned long cur_heap_ptr = 0x0;

unsigned long heap_end = 0x0;

// one 4k page : enclave page & offset

void sgx_print_hex(unsigned long addr) {

    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = PRINT_HEX;
    stub->addr = addr;
    sgx_exit(stub->trampoline);
}

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

void *sgx_malloc(int size) {

//    sgx_puts("heap_ptr address");
//    sgx_print_hex((unsigned long)cur_heap_ptr);
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    if (cur_heap_ptr == 0) {
        stub->fcode = FUNC_MALLOC;
        stub->mcode = MALLOC_INIT;
        // Enclave exit & jump into user-space trampoline
        sgx_exit(stub->trampoline);

        cur_heap_ptr = (unsigned long)stub->heap_beg;
        heap_end = (unsigned long)stub->heap_end;
    }

    void *last_heap_ptr = (void *)cur_heap_ptr;
    unsigned long extra_secinfo_size = sizeof(secinfo_t) + (SECINFO_ALIGN_SIZE - 1);

    // Check whether the malloc request overfills EPC heap area
    if ((cur_heap_ptr + extra_secinfo_size + ((size + 1) / 8) * 8) > heap_end) {
        //XXX: calling sgx_puts in here makes prob.
        //sgx_puts("DEBUG pending page");

        secinfo_t *secinfo = sgx_memalign(SECINFO_ALIGN_SIZE, sizeof(secinfo_t));

        secinfo->flags.r = 1;
        secinfo->flags.w = 1;
        secinfo->flags.x = 0;
        secinfo->flags.pending = 1;
        secinfo->flags.modified = 0;
        secinfo->flags.reserved1 = 0;
        secinfo->flags.page_type = PT_REG;
        int i = 0;
        for(i = 0 ; i< 6; i++){
            secinfo->flags.reserved2[i] = 0;
        }

        stub->fcode = FUNC_MALLOC;
        stub->mcode = REQUEST_EAUG;
        // Enclave exit & jump into user-space trampoline
        sgx_exit(stub->trampoline);
        unsigned long pending_page = stub->pending_page;
//        sgx_print_hex(pending_page);

        // EACCEPT should be called with [RBX:the address of secinfo, RCX:the adress of pending page]
        out_regs_t out;
        _enclu(ENCLU_EACCEPT, (uint64_t)secinfo, (uint64_t)pending_page, 0, &out);  // Check whether OS-provided pending page is legitimate for EPC heap area
        if (out.oeax == 0) {   // No error occurred in EACCEPT
            heap_end += PAGE_SIZE;
        } else {
            return NULL;
        }

    }
    cur_heap_ptr = (unsigned long)last_heap_ptr + ((size+1)/8)*8;

    //sgx_print_hex(heap_end);
    //sgx_print_hex((unsigned long)last_heap_ptr);
    return last_heap_ptr;
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

void sgx_free(void *ptr) {

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

size_t sgx_strlen(const char *string) {
    size_t len = 0;
    asm volatile("" ::: "memory");
    asm volatile("push  %%rdi\n\t"
                 "xor   %%rcx, %%rcx\n\t"
                 "movq  %1, %%rdi\n\t"
                 "not   %%ecx\n\t"
                 "xor   %%eax, %%eax\n\t"
                 "xor   %%al, %%al\n\t"
                 "cld\n\t"
                 "repne scasb\n\t"
                 "not   %%ecx\n\t"
                 "pop   %%rdi\n\t"
                 "lea   -0x1(%%ecx), %%eax\n\t"
                 :"=a"(len)
                 :"r"((uint64_t) string)
                 :"%rdi");
    return len;
}

int sgx_strcmp (const char *str1, const char *str2)
{
    int result = 0;
    asm volatile("" ::: "memory");
    asm volatile("push %%rsi\n\t"
                 "push %%rdi\n\t"
                 "movq %1, %%rsi\n\t"
                 "movq %2, %%rdi\n\t"

                 "REPEAT :\n\t"
                 "movzbl (%%rsi), %%eax\n\t"
                 "movzbl (%%rdi), %%ebx\n\t"
                 "sub %%bl, %%al\n\t"

                 "ja END\n\t"
                 "jb BELOW\n\t"
                 "je EQUAL\n\t"

                 "EQUAL :\n\t"
                 "inc %%rsi\n\t"
                 "inc %%rdi\n\t"
                 "test %%bl, %%bl\n\t"
                 "jnz REPEAT\n\t"

                 "BELOW :\n\t"
                 "neg %%rax\n\t"
                 "neg %%al\n\t"

                 "END :\n\t"
                 "pop %%rdi\n\t"
                 "pop %%rsi\n\t"
                 :"=a"(result)
                 :"r"((uint64_t)str1),
                  "r"((uint64_t)str2)
                 :"%rsi", "%rdi");


    return result;
}

int sgx_memcmp (const void *ptr1, const void *ptr2, size_t num)
{
    int result = 0;
    asm volatile("" ::: "memory");
    asm volatile("push %%rsi\n\t"
                 "push %%rdi\n\t"
                 "movq %1, %%rsi\n\t"
                 "movq %2, %%rdi\n\t"
                 "movq %3, %%rcx\n\t"
                 "xor %%rax,%%rax\n\t"
                 "cld\n\t"
                 "cmp %%rcx, %%rcx\n\t"
                 "repe cmpsb\n\t"

                 "jb CMP_BELOW\n\t"
                 "ja CMP_ABOVE\n\t"
                 "je CMP_END\n\t"

                 "CMP_ABOVE :\n\t"
                 "seta %%al\n\t"
                 "jmp END\n\t"

                 "CMP_BELOW :\n\t"
                 "setb %%al\n\t"
                 "neg %%rax\n\t"
                 "jmp END\n\t"

                 "CMP_END :\n\t"
                 "pop %%rdi\n\t"
                 "pop %%rsi\n\t"
                 :"=a"(result)
                 :"r"((uint64_t)ptr1),
                  "r"((uint64_t)ptr2),
                  "c"(num)
                 :"%rsi", "%rdi");


    return result;
}

void *sgx_memset (void *ptr, int value, size_t num)
{
    asm volatile("" ::: "memory");
    asm volatile("xor %%rax, %%rax\n\t"
                 "movq %0, %%rdi\n\t"
                 "movb %1, %%al\n\t"
                 "movq %2, %%rcx\n\t"
                 "body:"
                 "mov %%al, 0x0(%%rdi)\n\t"
                 "lea 0x1(%%rdi), %%rdi\n\t"
                 "loop body\n\t"
                 :
                 :"r"((uint64_t) ptr),
                  "r"((uint8_t) value),
                  "r"((uint64_t) num)
                 :"%rdi", "%al", "%rcx");

    return ptr;
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

int sgx_send(const char *ip, const char *port, const void *msg, size_t length)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_SEND;
    sgx_memcpy(stub->out_data1, ip, sgx_strlen(ip));
    sgx_memcpy(stub->out_data2, port, sgx_strlen(port));
    sgx_memcpy(stub->out_data3, msg, length);
    stub->out_arg1 = length;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    // return #of bytes sent.
    return stub->in_arg1;
}

int sgx_recv(const char *port, const char *buf)
{

    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    //recv
    stub->fcode = FUNC_RECV;
    sgx_memcpy(stub->out_data1, port, sizeof(port));
    stub->out_arg1 = SGXLIB_MAX_ARG;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    // recv failure check
    if (stub->in_arg1 < 0 ) {
        return;
    }
    else {
        sgx_memcpy(buf, stub->in_data1, stub->in_arg1);
    }

    // return #of bytes recv
    return stub->in_arg1;
}

int sgx_socket()
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_SOCKET;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_bind(int sockfd, int port)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_BIND;
    stub->out_arg1 = sockfd;
    stub->out_arg2 = port;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_listen(int sockfd)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_LISTEN;
    stub->out_arg1 = sockfd;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_accept(int sockfd)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_ACCEPT;
    stub->out_arg1 = sockfd;

    sgx_exit(stub->trampoline);

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

void sgx_close_sock(void) {
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    stub->fcode = FUNC_CLOSE_SOCK;

    sgx_exit(stub->trampoline);
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
