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

#include <string.h>
#include <sgx-trampoline.h>
#include <sgx-user.h>
#include <sgx-kern.h>
#include <sgx-utils.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netdb.h>
#include <unistd.h>
#include <sgx-malloc.h>
#include <stdarg.h>
#include <malloc.h>

const char *fcode_to_str(fcode_t fcode)
{
    switch (fcode) {
    case FUNC_PUTS        : return "PUTS";
    case FUNC_MALLOC      : return "MALLOC";
    case FUNC_FREE        : return "FREE";
    case FUNC_READ        : return "READ";
    case FUNC_WRITE       : return "WRITE";
    case FUNC_CLOSE       : return "CLOSE";
    case FUNC_PUTCHAR     : return "PUTCHAR";
    case FUNC_GMTIME      : return "GMTIME";
    case FUNC_TIME        : return "TIME";
    case FUNC_SOCKET      : return "SOCKET";
    case FUNC_BIND        : return "BIND";
    case FUNC_LISTEN      : return "LISTEN";
    case FUNC_ACCEPT      : return "ACCEPT";
    case FUNC_CONNECT     : return "CONNECT";
    case FUNC_SEND        : return "SEND";
    case FUNC_RECV        : return "RECV";

    // only for testing purpose
    case FUNC_SYSCALL     : return "SYSCALL";
    default:
        {
            sgx_dbg(err, "unknown function code (%d)", fcode);
                assert(false);
        }
    }
}

static
void dbg_dump_stub_out(sgx_stub_info *stub)
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
void dbg_dump_stub_in(sgx_stub_info *stub)
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
    fprintf(stderr, "++++++ ret:%x\n",
            stub->ret);
    fprintf(stderr, "++++++ pending page:%x\n",
            stub->pending_page);
    fprintf(stderr, "\n");

}

int sgx_puts_tramp(char *data)
{
    return puts(data);
}

/*
//TODO: could implement general syscall stub
// but we don't need it now. igrnore it
void sgx_syscall()
{
}
*/

static
int sgx_write_tramp(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

static
int sgx_read_tramp(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

static
int sgx_close_tramp(int fd)
{
    return close(fd);
}

static
time_t sgx_time_tramp(void *t)
{
    return time((time_t *)t);
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
int sgx_socket_tramp(int domain, int type, int protocol)
{
    return socket(domain, type, protocol);
}

static
int sgx_bind_tramp(int sockfd, void *addr, int addrlen)
{
    return bind(sockfd, (struct sockaddr *)addr, addrlen);
}

static
int sgx_listen_tramp(int sockfd, int backlog)
{
    return listen(sockfd, backlog);
}

static
int sgx_accept_tramp(int sockfd, void *addr, void *addrlen)
{
    return accept(sockfd, (struct sockaddr *)addr, (socklen_t *)addrlen);
}

static
int sgx_connect_tramp(int sockfd, void *addr, int addrlen)
{
    return connect(sockfd, (struct sockaddr *)addr, addrlen);
}

static
int sgx_send_tramp(int fd, const void *buf, size_t len, int flags)
{
    return send(fd, buf, len, flags);
}

static
int sgx_recv_tramp(int fd, void *buf, size_t len, int flags)
{
    return recv(fd, buf, len, flags);
}

static
void clear_abi_in_fields(sgx_stub_info *stub)   //from non-enclave to enclave
{
    if (stub != NULL) {
        stub->ret = 0;
        stub->pending_page = 0;
        memset(stub->in_data1, 0 , SGXLIB_MAX_ARG);
        memset(stub->in_data2, 0 , SGXLIB_MAX_ARG);
    }

    //TODO:stub->heap_beg/heap_end need to be cleared after data section is relocated into enclave.
}

static
void clear_abi_out_fields(sgx_stub_info *stub)  //from enclave to non-enclave
{
    if (stub != NULL) {
        stub->fcode = FUNC_UNSET;
        stub->mcode = MALLOC_UNSET;
        stub->out_arg1 = 0;
        stub->out_arg2 = 0;
        stub->addr = 0;
        memset(stub->out_data1, 0, SGXLIB_MAX_ARG);
        memset(stub->out_data2, 0, SGXLIB_MAX_ARG);
        memset(stub->out_data3, 0, SGXLIB_MAX_ARG);
    }
}

//Trampoline code for stub handling in user
void sgx_trampoline()
{
    unsigned long epc_heap_beg = 0;
    unsigned long epc_heap_end = 0;
    unsigned long pending_page = 0;

    sgx_msg(user, "Trampoline Entered");
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    clear_abi_in_fields(stub);
    //printf("Trampoline Entered fcode: %d mcode: %d\n", stub->fcode, stub->mcode);

    sgx_dbg(user, "Function code: %s", fcode_to_str(stub->fcode));
    //dbg_dump_stub_out(stub);

    switch (stub->fcode) {
    case FUNC_PUTS:
        stub->in_arg1 = sgx_puts_tramp(stub->out_data1);
        break;
    case FUNC_MALLOC:
        if (stub->mcode == MALLOC_INIT) {
            epc_heap_beg = get_epc_heap_beg();
            stub->heap_beg = epc_heap_beg;
            epc_heap_end = get_epc_heap_end();
            stub->heap_end = epc_heap_end;
        }
        else if (stub->mcode == REQUEST_EAUG) {
            pending_page = sys_add_epc(cur_keid);
            if (!pending_page)
                printf("DEBUG failed in EAUG\n");
            else{
                printf("DEBUG succeed in EAUG\n");
                printf("DEBUG pending page is %p\n", (void *)pending_page);
                stub->pending_page = pending_page;
            }
        }
        else{
            sgx_msg(warn, "Incorrect malloc code");
        }
        break;
    case FUNC_FREE:
        break;
    case FUNC_PUTCHAR:
        putchar(stub->out_arg1);
        break;
    case FUNC_GMTIME:
        sgx_gmtime_tramp((time_t *)&stub->out_arg4, &stub->in_tm);
        break;
    case FUNC_TIME:
        stub->in_arg3 = sgx_time_tramp(stub->out_data1);
        break;
    case FUNC_WRITE:
        stub->in_arg1 = sgx_write_tramp(stub->out_arg1, stub->out_data1, (size_t)stub->out_arg2);
        break;
    case FUNC_READ:
        stub->in_arg1 = sgx_read_tramp(stub->out_arg1, stub->in_data1, (size_t)stub->out_arg2);
        break;
    case FUNC_CLOSE:
        stub->in_arg1 = sgx_close_tramp(stub->out_arg1);
        break;
    case FUNC_SOCKET:
        stub->in_arg1 = sgx_socket_tramp(stub->out_arg1, stub->out_arg2, stub->out_arg3);
        break;
    case FUNC_BIND:
        stub->in_arg1 = sgx_bind_tramp(stub->out_arg1, stub->out_data1, stub->out_arg2);
        break;
    case FUNC_LISTEN:
        stub->in_arg1 = sgx_listen_tramp(stub->out_arg1, stub->out_arg2);
        break;
    case FUNC_ACCEPT:
        stub->in_arg1 = sgx_accept_tramp(stub->out_arg1, stub->out_data1, stub->out_data2);
        break;
    case FUNC_CONNECT:
        stub->in_arg1 = sgx_connect_tramp(stub->out_arg1, stub->out_data1, stub->out_arg2);
        break;
    case FUNC_SEND:
        stub->in_arg1 = sgx_send_tramp(stub->out_arg1, stub->out_data1, (size_t)stub->out_arg2, stub->out_arg3);
        break;
    case FUNC_RECV:
        stub->in_arg1 = sgx_recv_tramp(stub->out_arg1, stub->in_data1, (size_t)stub->out_arg2, stub->out_arg3);
        break;
/*
    case FUNC_SYSCALL:
        sgx_syscall();
    break;
*/
    default:
        sgx_msg(warn, "Incorrect function code");
        return;
        break;
    }

    clear_abi_out_fields(stub);
    //dbg_dump_stub_in(stub);
    // ERESUME at the end w/ info->tcs
    sgx_resume(stub->tcs, 0);
}

int sgx_init(void)
{
    assert(sizeof(struct sgx_stub_info) < PAGE_SIZE);

    sgx_stub_info *stub = mmap((void *)STUB_ADDR, PAGE_SIZE,
                               PROT_READ|PROT_WRITE,
                               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (stub == MAP_FAILED)
        return 0;

    //stub area init
    memset((void *)stub, 0x00, PAGE_SIZE);

    stub->abi = OPENSGX_ABI_VERSION;
    stub->trampoline = (void *)(uintptr_t)sgx_trampoline;

    return sys_sgx_init();
}
