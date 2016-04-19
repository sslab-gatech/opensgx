#define __SYSCALL_LL_E(x) (x)
#define __SYSCALL_LL_O(x) (x)

#include <sgx-lib.h>

static __inline long __syscall0(long n)
{
    unsigned long ret;
    //memory and register barrier
    __asm__ __volatile__ (""::: "rcx", "r11", "memory");
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_SYSCALL;
    stub->sys_args[0] = 0; // num of syscall args

    sgx_exit(stub->trampoline);
    ret = stub->sys_ret;
    return ret;
}

static __inline long __syscall1(long n, long a1)
{
    unsigned long ret;
    //memory and register barrier
    __asm__ __volatile__ (""::: "rcx", "r11", "memory");
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    
    stub->fcode = FUNC_SYSCALL;
    stub->sys_args[0] = 1; // num of syscall args
    stub->sys_args[1] = a1;

    sgx_exit(stub->trampoline);
    ret = stub->sys_ret;
    return ret;
}

static __inline long __syscall2(long n, long a1, long a2)
{
    unsigned long ret;
    //memory and register barrier
    __asm__ __volatile__ (""::: "rcx", "r11", "memory");
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    
    stub->fcode = FUNC_SYSCALL;
    stub->sys_args[0] = 2; // num of syscall args
    stub->sys_args[1] = a1; 
    stub->sys_args[2] = a2; 

    sgx_exit(stub->trampoline);
    ret = stub->sys_ret;
}

static __inline long __syscall3(long n, long a1, long a2, long a3)
{
    unsigned long ret;
    //memory and register barrier
    __asm__ __volatile__ (""::: "rcx", "r11", "memory");
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    
    stub->fcode = FUNC_SYSCALL;
    stub->sys_args[0] = 3; // num of syscall args
    stub->sys_args[1] = a1;
    stub->sys_args[2] = a2; 
    stub->sys_args[3] = a3; 

    sgx_exit(stub->trampoline);                   
    ret = stub->sys_ret;

}

static __inline long __syscall4(long n, long a1, long a2, long a3, long a4)
{
    unsigned long ret;
    //memory and register barrier
    __asm__ __volatile__ (""::: "rcx", "r11", "memory");
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_SYSCALL;
    stub->sys_args[0] = 4; // num of syscall args
    stub->sys_args[1] = a1;
    stub->sys_args[2] = a2;
    stub->sys_args[3] = a3;
    stub->sys_args[4] = a4;

    sgx_exit(stub->trampoline);
    ret = stub->sys_ret;

}

static __inline long __syscall5(long n, long a1, long a2, long a3, long a4, long a5)
{
    unsigned long ret;
    //memory and register barrier
    __asm__ __volatile__ (""::: "rcx", "r11", "memory");
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_SYSCALL;
    stub->sys_args[0] = 5; // num of syscall args
    stub->sys_args[1] = a1;
    stub->sys_args[2] = a2;
    stub->sys_args[3] = a3;
    stub->sys_args[4] = a4;
    stub->sys_args[5] = a5;

    sgx_exit(stub->trampoline);
    ret = stub->sys_ret;
}

static __inline long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
    unsigned long ret;
    //memory and register barrier
    __asm__ __volatile__ (""::: "rcx", "r11", "memory");
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    stub->fcode = FUNC_SYSCALL;
    stub->sys_args[0] = 6; // num of syscall args
    stub->sys_args[1] = a1;
    stub->sys_args[2] = a2;
    stub->sys_args[3] = a3;
    stub->sys_args[4] = a4;
    stub->sys_args[5] = a5;
    stub->sys_args[6] = a6;

    sgx_exit(stub->trampoline);
    ret = stub->sys_ret;
}
