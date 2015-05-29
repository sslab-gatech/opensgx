#include <sgx-lib.h>
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

    sgx_puts("heap_ptr address");
//    sgx_print_hex((unsigned long)cur_heap_ptr);
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    if(cur_heap_ptr == 0){
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
    if(((unsigned long)cur_heap_ptr + extra_secinfo_size + ((size+1)/8)*8) > heap_end){
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
        if(out.oeax == 0){   // No error occurred in EACCEPT
            heap_end += PAGE_SIZE;
        }
        else{
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
    if(ptr == NULL){
        return sgx_malloc(size);
    }
    else{
        if(size == 0){
             sgx_free(ptr);
             return NULL;
        } 
        new = sgx_malloc(size);
        if(new != NULL){      
            //if new size > old size, old_size+alpha is written to new. Thus, some of garbage values would be copied
            //if old size > new size, new_size is written to new. Thus, some of old values would be lossed
            //sgx_print_hex(new);
            sgx_memcpy(new, ptr, size);
            return new;
        }
        else{
            return NULL; 
        }
    }
}

void *sgx_memalign(size_t align, size_t size){

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

int sgx_send(const char *ip, const char *port, const void *msg, size_t length) {
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

int sgx_recv(const char *port, const char *buf) {

    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    //recv
    stub->fcode = FUNC_RECV;
    sgx_memcpy(stub->out_data1, port, sizeof(port));
    stub->out_arg1 = SGXLIB_MAX_ARG;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    // recv failure check 
    if(stub->in_arg1 < 0 ){
        return;
    }
    else {
        sgx_memcpy(buf, stub->in_data1, stub->in_arg1);
    }

    // return #of bytes recv
    return stub->in_arg1;
}

void sgx_close_sock(void) {
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    stub->fcode = FUNC_CLOSE_SOCK; 

    sgx_exit(stub->trampoline);
}
