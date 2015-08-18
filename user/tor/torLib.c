#include <sgx-lib.h>
#include "sgx-tor-trampoline.h"

// one 4k page : enclave page & offset

int sgx_mkdir(char pathname[], size_t size, mode_t mode) 
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_MKDIR;
    sgx_memcpy(stub->out_data1, pathname, size);
    stub->out_arg1 = (int)mode;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_mknod(char pathname[], size_t size, mode_t mode, dev_t dev)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_MKNOD;
    sgx_memcpy(stub->out_data1, pathname, size);
    stub->out_arg1 = (int)mode;
    stub->out_arg2 = (int)dev;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_open(char pathname[], size_t size, int flag)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_OPEN;
    sgx_memcpy(stub->out_data1, pathname, size);
    stub->out_arg1 = flag;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

// Note that this is not regular snprintf, specialized one for Tor
int sgx_snprintf(char dst[], int dst_size, char buf1[], size_t size1, 
                 char buf2[], size_t size2, int arg1)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_SNPRINTF;
    stub->out_arg1 = dst_size;
    stub->out_arg2 = arg1;
    sgx_memcpy(stub->out_data1, buf1, size1);
    sgx_memcpy(stub->out_data2, buf2, size2);

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);
    sgx_memcpy(dst, stub->in_data1, dst_size);

    return stub->in_arg1;
}

/*
time_t sgx_time(time_t *arg1)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;
    stub->fcode = FUNC_TIME;

    // Enclave exit & jump into user-space trampoline
    sgx_exit(stub->trampoline);
    sgx_memcpy(arg1, stub->in_data1, sizeof(time_t));

    return (time_t)stub->in_arg1;
}

int sgx_strncasecmp(const char *s1, const char *s2, size_t n)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_STRNCASECMP;

    sgx_memcpy(stub->out_data1, s1, n);
    sgx_memcpy(stub->out_data2, s2, n);
    stub->out_arg1 = n;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_strncmp(const char *s1, const char *s2, size_t n)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_STRNCMP;

    sgx_memcpy(stub->out_data1, s1, n);
    sgx_memcpy(stub->out_data2, s2, n);
    stub->out_arg1 = n;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
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

int sgx_isspace(int c)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_ISSPACE;

    stub->out_arg1 = c;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_tolower(int c)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_TOLOWER;

    stub->out_arg1 = c;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}

int sgx_isdigit(int c)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_ISDIGIT;

    stub->out_arg1 = c;

    sgx_exit(stub->trampoline);

    return stub->in_arg1;
}
*/

void sgx_debug(char msg[])
{
    size_t size = sgx_strlen(msg);
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_DEBUG;
    sgx_memcpy(stub->out_data1, msg, size);

    sgx_exit(stub->trampoline);
}

struct tm *sgx_localtime_r(const time_t *timep, struct tm *result)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_LOCALTIME;
    stub->out_arg4 = *timep;

    sgx_exit(stub->trampoline);
    sgx_memcpy(result, &stub->in_tm, sizeof(struct tm));

    return result;
}

struct tm *sgx_gmtime(const time_t *timep)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;
    struct tm temp_tm;

    stub->fcode = FUNC_GMTIME;
    stub->out_arg4 = *timep;
    
    sgx_exit(stub->trampoline);
    sgx_memcpy(&temp_tm, &stub->in_tm, sizeof(struct tm));

    return &temp_tm;
}

size_t sgx_strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_STRFTIME;
    stub->out_arg4 = max;
    sgx_memcpy(&stub->out_tm, tm, sizeof(struct tm));
    sgx_memcpy(stub->out_data1, format, sgx_strlen(format));

    sgx_exit(stub->trampoline);
    sgx_memcpy(s, stub->in_data1, max);

    return stub->in_arg1;
}

time_t sgx_mktime(struct tm *tm)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_MKTIME;
    sgx_memcpy(&stub->out_tm, tm, sizeof(struct tm));

    sgx_exit(stub->trampoline);

    return stub->in_arg3;
}

void sgx_print_bytes(char *s, size_t n)
{
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    stub->fcode = FUNC_PRINT_BYTES;
    sgx_memcpy(stub->out_data1, s, n);
    stub->out_arg1 = n;

    sgx_exit(stub->trampoline);
}
