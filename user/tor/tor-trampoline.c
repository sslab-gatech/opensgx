#include "sgx-tor-trampoline.h"
#include <string.h>
#include <sgx-kern.h>
#include <sgx-user.h>
#include <sgx-utils.h>
#include <sgx-crypto.h>
#include <sgx-trampoline.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sgx-malloc.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

//TODO : fcode_to_str, tor_trampoline should be implemented
static
const char *fcode_to_str(fcode_t_tor fcode)
{
    switch (fcode) {
        case FUNC_UNSET_TOR   : return "UNSET";
        case FUNC_OPEN    : return "OPEN";
        case FUNC_MKDIR   : return "MKDIR";
        case FUNC_MKNOD   : return "MKNOD";
        case FUNC_SNPRINTF: return "SNPRINTF";
        case FUNC_STRNCASECMP : return "STRNCASECMP";
        case FUNC_STRNCMP     : return "STRNCMP";
        case FUNC_ISSPACE     : return "ISSPACE";
        case FUNC_TOLOWER     : return "TOLOWER";
        case FUNC_ISDIGIT     : return "ISDIGIT";
        case FUNC_DEBUG       : return "DEBUG";
        case FUNC_LOCALTIME   : return "LOCALTIME";
        case FUNC_GMTIME   : return "GMTIME";
        case FUNC_STRFTIME : return "STRFTIME";
        case FUNC_MKTIME   : return "MKTIME";
        case FUNC_PRINT_BYTES : return "PRINT_BYTES";
        default:
        {
            sgx_dbg(err, "unknown function code (%d)", fcode);
                assert(false);
        }
    }
}

static
void dbg_dump_stub_out(sgx_stub_info_tor *stub)
{

    fprintf(stderr, "\n");
    fprintf(stderr, "++++++ FROM ENCLAVE ++++++\n");
    fprintf(stderr, "++++++ ABI Version : %d ++++++\n",
            stub->abi);
    fprintf(stderr, "++++++ Function code: %s\n",
            fcode_to_str(stub->fcode));
    fprintf(stderr, "++++++ Out_arg1: %d  Out_arg2: %d  Out_arg3: %d\n",
            stub->out_arg1, stub->out_arg2, stub->out_arg3);
    fprintf(stderr, "++++++ Out Data1 ++++++\n");
    hexdump(stderr, (void *)stub->out_data1, 32);
    fprintf(stderr, "\n");
    fprintf(stderr, "++++++ Out Data2 ++++++\n");
    hexdump(stderr, (void *)stub->out_data2, 32);
    fprintf(stderr, "\n");
    fprintf(stderr, "++++++ Out Data3 ++++++\n");
    hexdump(stderr, (void *)stub->out_data3, 32);

    fprintf(stderr, "++++++ Tcs:%p\n",
            (void *) stub->tcs);

    fprintf(stderr, "\n");

}

static
void dbg_dump_stub_in(sgx_stub_info_tor *stub)
{
    fprintf(stderr, "\n");
    fprintf(stderr, "++++++ TO ENCLAVE ++++++\n");
    fprintf(stderr, "++++++ In_arg1: %d  In_arg2: %d  in_arg3: %ld\n",
            stub->in_arg1, stub->in_arg2, stub->in_arg3);
    fprintf(stderr, "++++++ In Data1 ++++++\n");
    hexdump(stderr, (void *)stub->in_data1, 32);
    fprintf(stderr, "\n");
    fprintf(stderr, "++++++ In Data2 ++++++\n");
    hexdump(stderr, (void *)stub->in_data2, 32);
    fprintf(stderr, "\n");

}

static
void clear_abi_in_fields(sgx_stub_info_tor *stub)   //from non-enclave to enclave
{
    if(stub != NULL){
        stub->in_arg1 = 0;
        stub->in_arg2 = 0;
        stub->in_arg3 = 0;
        memset(stub->in_data1, 0 , SGXLIB_MAX_ARG);
        memset(stub->in_data2, 0 , SGXLIB_MAX_ARG);
        memset(&stub->in_tm, 0 , sizeof(struct tm));
    }

}


static
void clear_abi_out_fields(sgx_stub_info_tor *stub)  //from enclave to non-enclave
{
    if(stub != NULL){
        stub->fcode = FUNC_UNSET_TOR;
        stub->out_arg1 = 0;
        stub->out_arg2 = 0;
        stub->out_arg3 = 0;
        stub->out_arg4 = 0;
        memset(stub->out_data1, 0, SGXLIB_MAX_ARG);
        memset(stub->out_data2, 0, SGXLIB_MAX_ARG);
        memset(stub->out_data3, 0, SGXLIB_MAX_ARG);
        memset(&stub->out_tm, 0, sizeof(struct tm));
    }
}

static
int sgx_mkdir_tramp(char pathname[], mode_t mode)
{
    mkdir(pathname, mode);
    if(errno != EEXIST)
        return -1;
    else
        return 0;
}

static
int sgx_mknod_tramp(char pathname[], mode_t mode, dev_t dev)
{
    mknod(pathname, mode, dev);
    if(errno != EEXIST)
        return -1;
    else
        return 0;
}

static
int sgx_open_tramp(char pathname[], int flag)
{
    return open(pathname, flag);
}

static
int sgx_snprintf_tramp(char dst[], size_t dst_size, char buf1[], char buf2[], int arg)
{
    return snprintf(dst, dst_size, buf1, buf2, arg);
}

static
int sgx_strncasecmp_tramp(const char *s1, const char *s2, size_t n)
{
    return strncasecmp(s1, s2, n);
}

static
int sgx_strncmp_tramp(const char *s1, const char *s2, size_t n)
{
    int rv = strncmp(s1, s2, n);
    fprintf(stderr, "strncmp(%s, %s, %d) return %d\n", s1, s2, n, rv);

    //return strncmp(s1, s2, n);
    return rv;
}

static
int sgx_isspace_tramp(int c)
{
    return isspace(c);
}

static
int sgx_tolower_tramp(int c)
{
    return tolower(c);
}

static
int sgx_isdigit_tramp(int c)
{
    return isdigit(c);
}

static
void sgx_debug_tramp(char msg[])
{
    fprintf(stderr, "[DEBUG SGX] %s\n", msg);
}

static
struct tm *sgx_localtime_r_tramp(time_t *timep, struct tm *result)
{
    return localtime_r(timep, result);
}

static
struct tm *sgx_gmtime_tramp(time_t *timep, struct tm *result)
{
    struct tm *temp_tm;
    temp_tm = gmtime(timep);
    memcpy(result, temp_tm, sizeof(struct tm));
    
    return result;
}

static
size_t sgx_strftime_tramp(char *s, size_t max, char *format,
                       const struct tm *tm)
{
    return strftime(s, max, format, tm);
}

static
time_t sgx_mktime_tramp(struct tm *tm)
{
    return mktime(tm);
}

static
void sgx_print_bytes_tramp(char *s, int n)
{
    int i;
    fprintf(stderr, "[SGX PRINT BYTES] \n");
    for (i = 0; i < n; i++)
        fprintf(stderr, "%02hhX ", s[i]);
    fprintf(stderr, "\n");
}

//Trampoline code for stub handling in user
void sgx_trampoline_tor()
{
    sgx_msg(info, "Trampoline Entered");
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    clear_abi_in_fields(stub);

//    dbg_dump_stub_out(stub);

    char *ptr = NULL;

    switch (stub->fcode) {
    case FUNC_OPEN:
        stub->in_arg1 = sgx_open_tramp(stub->out_data1, stub->out_arg1);
        break;
    case FUNC_MKDIR:
        stub->in_arg1 = sgx_mkdir_tramp(stub->out_data1, (mode_t)stub->out_arg1);
        break;
    case FUNC_MKNOD:
        stub->in_arg1 = sgx_mknod_tramp(stub->out_data1, (mode_t)stub->out_arg1,
                                        (dev_t)stub->out_arg2);
        break;
    case FUNC_SNPRINTF:
        stub->in_arg1 = sgx_snprintf_tramp(stub->in_data1, (size_t)stub->out_arg1, stub->out_data1,
                                           stub->out_data2, stub->out_arg2);
        break;
    case FUNC_STRNCASECMP:
        stub->in_arg1 = sgx_strncasecmp_tramp(stub->out_data1, stub->out_data2, stub->out_arg1);
        break;
    case FUNC_STRNCMP:
        stub->in_arg1 = sgx_strncmp_tramp(stub->out_data1, stub->out_data2, stub->out_arg1);
        break;
    case FUNC_ISSPACE:
        stub->in_arg1 = sgx_isspace_tramp(stub->out_arg1);
        break;
    case FUNC_TOLOWER:
        stub->in_arg1 = sgx_tolower_tramp(stub->out_arg1);
        break;
    case FUNC_ISDIGIT:
        stub->in_arg1 = sgx_isdigit_tramp(stub->out_arg1);
        break;
    case FUNC_DEBUG:
        sgx_debug_tramp(stub->out_data1);
        break;
    case FUNC_LOCALTIME:
        sgx_localtime_r_tramp(&stub->out_arg4, &stub->in_tm);
        break;
    case FUNC_GMTIME:
        sgx_gmtime_tramp((time_t *)&stub->out_arg4, &stub->in_tm);
        break;
    case FUNC_STRFTIME:
        sgx_strftime_tramp(stub->in_data1, stub->out_arg4, stub->out_data1, &stub->out_tm);
        puts(stub->in_data1);
        break;
    case FUNC_MKTIME:
        stub->in_arg3 = sgx_mktime_tramp(&stub->out_tm);
        break;
    case FUNC_PRINT_BYTES:
        sgx_print_bytes_tramp(stub->out_data1, stub->out_arg1);
        break;
    default:
        sgx_msg(warn, "Incorrect function code");
        return;
        break;
    }

    clear_abi_out_fields(stub);
//    dbg_dump_stub_in(stub);
    // ERESUME at the end w/ info->tcs

    sgx_stub_info *stub_ori = (sgx_stub_info *)STUB_ADDR;
    stub->tcs = stub_ori->tcs;

    sgx_resume(stub->tcs, 0);
}

int sgx_init_tor(void)
{
    assert(sizeof(struct sgx_stub_info) < PAGE_SIZE);

    sgx_stub_info *stub = mmap((void *)STUB_ADDR, PAGE_SIZE,
                               PROT_READ|PROT_WRITE,
                               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (stub == MAP_FAILED)
        return 0;

    sgx_stub_info_tor *stub_tor = mmap((void *)STUB_ADDR_TOR, PAGE_SIZE,
                               PROT_READ|PROT_WRITE,
                               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (stub_tor == MAP_FAILED)
        return 0;

    //stub area init
    memset((void *)stub, 0x00, PAGE_SIZE);

    stub->abi = OPENSGX_ABI_VERSION;
    stub->trampoline = (void *)(uintptr_t)sgx_trampoline;

    //stub tor init
    memset((void *)stub_tor, 0x00, PAGE_SIZE);

    stub_tor->abi = OPENSGX_ABI_VERSION;
    stub_tor->trampoline = (void *)(uintptr_t)sgx_trampoline_tor;

    return sys_sgx_init();
}

