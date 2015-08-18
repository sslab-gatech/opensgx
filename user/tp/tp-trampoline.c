#include "sgx-tp-trampoline.h"
#include <string.h>
#include <sgx-kern.h>
#include <sgx-user.h>
#include <sgx-trampoline.h>
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
#include <ctype.h>

#include <openssl/objects.h>
#include <openssl/ssl.h>

//TODO : fcode_to_str, tp_trampoline should be implemented
static
const char *fcode_to_str(fcode_t_tp fcode)
{
    switch (fcode) {
        case FUNC_UNSET_TP    : return "UNSET";
        case FUNC_GMTIME   : return "GMTIME";
        case FUNC_DEBUG       : return "DEBUG";
        case FUNC_PRINT_BYTES : return "PRINT_BYTES";
        default:
        {
            sgx_dbg(err, "unknown function code (%d)", fcode);
                assert(false);
        }
    }
}

static
void dbg_dump_stub_out(sgx_stub_info_tp *stub)
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
void dbg_dump_stub_in(sgx_stub_info_tp *stub)
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
void clear_abi_in_fields(sgx_stub_info_tp *stub)   //from non-enclave to enclave
{
    if(stub != NULL){
        memset(stub->in_data1, 0 , SGXLIB_MAX_ARG);
        memset(stub->in_data2, 0 , SGXLIB_MAX_ARG);
        memset(&stub->in_tm, 0, sizeof(struct tm));
    }
}


static
void clear_abi_out_fields(sgx_stub_info_tp *stub)  //from enclave to non-enclave
{
    if(stub != NULL){
        stub->fcode = FUNC_UNSET_TP;
        stub->out_arg1 = 0;
        stub->out_arg2 = 0;
        stub->out_arg4 = 0;
        memset(stub->out_data1, 0, SGXLIB_MAX_ARG);
        memset(stub->out_data2, 0, SGXLIB_MAX_ARG);
        memset(stub->out_data3, 0, SGXLIB_MAX_ARG);
    }
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
void sgx_debug_tramp(char msg[])
{
    fprintf(stderr, "[SGX DEBUG] %s\n", msg);
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
void sgx_trampoline_tp()
{
    sgx_msg(info, "Trampoline Entered");
    sgx_stub_info_tp *stub = (sgx_stub_info_tp *)STUB_ADDR_TP;

    clear_abi_in_fields(stub);

    //dbg_dump_stub_out(stub);

    fprintf(stderr, "++++++ Function code: %s\n",
            fcode_to_str(stub->fcode));

    char *ptr = NULL;

    switch (stub->fcode) {
    case FUNC_GMTIME:
        sgx_gmtime_tramp((time_t *)&stub->out_arg4, &stub->in_tm);
        break;
    case FUNC_PRINT_BYTES:
        sgx_print_bytes_tramp(stub->out_data1, stub->out_arg1);
        break;
    case FUNC_DEBUG:
        sgx_debug_tramp(stub->out_data1);
        break;
    default:
        sgx_msg(warn, "Incorrect function code");
        return;
        break;
    }

    clear_abi_out_fields(stub);
    //dbg_dump_stub_in(stub);
    // ERESUME at the end w/ info->tcs

    sgx_stub_info *stub_ori = (sgx_stub_info *)STUB_ADDR;
    stub->tcs = stub_ori->tcs;

    sgx_resume(stub->tcs, 0);
}

int sgx_init_tp(void)
{
    assert(sizeof(struct sgx_stub_info) < PAGE_SIZE);

    sgx_stub_info *stub = mmap((void *)STUB_ADDR, PAGE_SIZE,
                               PROT_READ|PROT_WRITE,
                               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (stub == MAP_FAILED)
        return 0;

    sgx_stub_info_tp *stub_tp = mmap((void *)STUB_ADDR_TP, PAGE_SIZE,
                               PROT_READ|PROT_WRITE,
                               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (stub_tp == MAP_FAILED)
        return 0;

    //stub area init
    memset((void *)stub, 0x00, PAGE_SIZE);

    stub->abi = OPENSGX_ABI_VERSION;
    stub->trampoline = (void *)(uintptr_t)sgx_trampoline;

    //stub tp init
    memset((void *)stub_tp, 0x00, PAGE_SIZE);

    stub_tp->abi = OPENSGX_ABI_VERSION;
    stub_tp->trampoline = (void *)(uintptr_t)sgx_trampoline_tp;

    return sys_sgx_init();
}

