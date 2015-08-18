#include "sgx-attest-trampoline.h"
#include <string.h>
#include <sgx-kern.h>
#include <sgx-user.h>
#include <sgx-utils.h>
#include <sgx-crypto.h>
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
/*
        case FUNC_OPEN    : return "OPEN";
        case FUNC_CLOSE   : return "CLOSE";
        case FUNC_MKDIR   : return "MKDIR";
        case FUNC_MKNOD   : return "MKNOD";
        case FUNC_READ    : return "READ";
        case FUNC_WRITE   : return "WRITE";
        case FUNC_SNPRINTF: return "SNPRINTF";
        case FUNC_TIME    : return "TIME";
*/
        case FUNC_MEMCHR  : return "MEMCHR";
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
    fprintf(stderr, "++++++ Out_arg1: %d  Out_arg2: %d\n",
            stub->out_arg1, stub->out_arg2);
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
    fprintf(stderr, "++++++ In_arg1: %d  In_arg2: %d\n",
            stub->in_arg1, stub->in_arg2);
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
        memset(stub->in_data1, 0 , SGXLIB_MAX_ARG);
        memset(stub->in_data2, 0 , SGXLIB_MAX_ARG);
    }

}


static
void clear_abi_out_fields(sgx_stub_info_tor *stub)  //from enclave to non-enclave
{
    if(stub != NULL){
        stub->fcode = FUNC_UNSET_TOR;
        stub->out_arg1 = 0;
        stub->out_arg2 = 0;
        memset(stub->out_data1, 0, SGXLIB_MAX_ARG);
        memset(stub->out_data2, 0, SGXLIB_MAX_ARG);
        memset(stub->out_data3, 0, SGXLIB_MAX_ARG);
    }
}
static
void ERESUME(tcs_t *tcs, void (*aep)()) {
    // RBX: TCS (In, EA)
    // RCX: AEP (In, EA)
    enclu(ENCLU_ERESUME, (uint64_t)tcs, (uint64_t)aep, 0, NULL);
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
int sgx_close_tramp(int fd)
{
    return close(fd);
}

static
int sgx_snprintf_tramp(char dst[], size_t dst_size, char buf1[], char buf2[], int arg)
{
    return snprintf(dst, dst_size, buf1, buf2, arg);
}

static
int sgx_write_tramp(int fd, void *buf, size_t count)
{
    return write(fd, buf, count);
}

static
int sgx_read_tramp(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

static
int sgx_time_tramp(char arg1[])
{
    time_t t;
    int ret = time(&t);
    memcpy(arg1, &t, sizeof(time_t));
    return ret;
}

static
void *sgx_memchr_tramp(void *s, int c, size_t n)
{
    return memchr(s, c, n);
}

//Trampoline code for stub handling in user
void sgx_trampoline_tor()
{
    sgx_msg(info, "Trampoline Entered");
    sgx_stub_info_tor *stub = (sgx_stub_info_tor *)STUB_ADDR_TOR;

    clear_abi_in_fields(stub);

    dbg_dump_stub_out(stub);

    char *ptr = NULL;

    switch (stub->fcode) {
/*
    case FUNC_OPEN:
        stub->in_arg1 = sgx_open_tramp(stub->out_data1, stub->out_arg1);
        break;
    case FUNC_CLOSE:
        stub->in_arg1 = sgx_close_tramp(stub->out_arg1);
        break;
    case FUNC_MKDIR:
        stub->in_arg1 = sgx_mkdir_tramp(stub->out_data1, (mode_t)stub->out_arg1);
        break;
    case FUNC_MKNOD:
        stub->in_arg1 = sgx_mknod_tramp(stub->out_data1, (mode_t)stub->out_arg1, 
                                        (dev_t)stub->out_arg2);
        break;
    case FUNC_WRITE:
        stub->in_arg1 = sgx_write_tramp(stub->out_arg1, stub->in_data1, (size_t)stub->out_arg2);
        break;
    case FUNC_READ:
        stub->in_arg1 = sgx_read_tramp(stub->out_arg1, stub->in_data1, (size_t)stub->out_arg2);
        break;
    case FUNC_SNPRINTF:
        stub->in_arg1 = sgx_snprintf_tramp(stub->in_data1, (size_t)stub->out_arg1, stub->out_data1,
                                           stub->out_data2, stub->out_arg2);
        break;
    case FUNC_TIME:
        stub->in_arg1 = sgx_time_tramp(stub->in_data1);
        break;
*/
    case FUNC_MEMCHR:
        ptr = sgx_memchr_tramp(stub->out_data1, stub->out_arg1, stub->out_arg2);

        if(ptr != NULL)
            memcpy(stub->in_data1, ptr, strlen(ptr));
        break;
    default:
        sgx_msg(warn, "Incorrect function code");
        return;
        break;
    }

    clear_abi_out_fields(stub);
    dbg_dump_stub_in(stub);
    // ERESUME at the end w/ info->tcs

    sgx_stub_info *stub_ori = (sgx_stub_info *)STUB_ADDR;
    stub->tcs = stub_ori->tcs;

    ERESUME(stub->tcs, 0);
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

