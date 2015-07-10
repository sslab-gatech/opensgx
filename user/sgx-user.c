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
#include <sgx-kern.h>
#include <sgx-user.h>
#include <sgx-utils.h>
#include <sgx-signature.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sgx-malloc.h>
#include <stdarg.h>

static keid_t stat;
static uint64_t _tcs_app;
static int send_fd = 0;
static int recv_fd = 0, client_fd = 0;

// (ref. r2:5.2)
// out_regs store the output value returned from qemu */
void enclu(enclu_cmd_t leaf, uint64_t rbx, uint64_t rcx, uint64_t rdx,
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

static
void EENTER(tcs_t *tcs, void (*aep)())
{
    // RBX: TCS (In, EA)
    // RCX: AEP (In, EA)
    enclu(ENCLU_EENTER, (uint64_t)tcs, (uint64_t)aep, 0, NULL);
}

static
void ERESUME(tcs_t *tcs, void (*aep)()) {
    // RBX: TCS (In, EA)
    // RCX: AEP (In, EA)
    enclu(ENCLU_ERESUME, (uint64_t)tcs, (uint64_t)aep, 0, NULL);
}

static
void EMODPE() {
    // RBX: SECINFO (In, EA)
    // RCX: EPCPAGE (In, EA)
}

static
void EACCEPTCOPY() {
    // RBX: SECINFO (In, EA)
    // RCX: EPCPAGE (In, EA)
}

void exception_handler(void)
{
    sgx_msg(trace, "Asy_Call\n");
    uint64_t aep = 0x00;
    uint64_t rdx = 0x00;

	asm("movl %0, %%eax\n\t"
        "movq %1, %%rbx\n\t"
        "movq %2, %%rcx\n\t"
        "movq %3, %%rdx\n\t"
        ".byte 0x0F\n\t"
        ".byte 0x01\n\t"
        ".byte 0xd7\n\t"
        :
        :"a"((uint32_t)ENCLU_ERESUME),
         "b"((uint64_t)_tcs_app),
         "c"((uint64_t)aep),
         "d"((uint64_t)rdx));
}

// (ref re:2.13, EINIT/p88)
// Set up sigstruct fields require to be signed.
static
sigstruct_t *alloc_sigstruct(void)
{
    sigstruct_t *s = memalign(PAGE_SIZE, sizeof(sigstruct_t));
    if (!s)
        return NULL;

    // Initializate with 0s
    memset(s, 0, sizeof(sigstruct_t));

    // HEADER(16 bytes)
    uint8_t header[16] = SIG_HEADER1;
    memcpy(s->header, swap_endian(header, 16), 16);

    // VENDOR(4 bytes)
    // Non-Intel Enclave;
    s->vendor = 0x00000000;

    // DATE(4 bytes)
    s->date = 0x20150101;

    // HEADER2(16 bytes)
    uint8_t header2[16] = SIG_HEADER2;
    memcpy(s->header2, swap_endian(header2, 16), 16);

    // SWDEFINTO(4 bytes)
    s->swdefined = 0x00000000;

    // MISCSELECT(4 bytes)
    //s->miscselect = 0x0;

    // MISCMASK(4 bytes)
    //s->miscmask = 0x0;

    // ATTRIBUTES(16 bytes)
    memset(&s->attributes, 0, sizeof(attributes_t));
    s->attributes.mode64bit = true;
    s->attributes.provisionkey = true;
    s->attributes.einittokenkey = false;
    s->attributes.xfrm = 0x03;

    // ATTRIBUTEMAST(16 bytes)
    memset(&s->attributeMask, 0 ,sizeof(attributes_t));
    s->attributeMask.mode64bit = true;
    s->attributeMask.provisionkey = true;
    s->attributeMask.einittokenkey = false;
    s->attributeMask.xfrm = 0x03;

    // ISVPRODID(2 bytes)
    s->isvProdID = 0x0001;

    // ISVSVN(2 bytes)
    s->isvSvn = 0x0001;

    return s;
}


// Set up einittoken fields require to be signed.
static
einittoken_t *alloc_einittoken(rsa_key_t pubkey, sigstruct_t *sigstruct)
{
    einittoken_t *t = memalign(EINITTOKEN_ALIGN_SIZE, sizeof(einittoken_t));
    if (!t)
        return NULL;

    // Initializate with 0s
    memset(t, 0, sizeof(einittoken_t));

    // VALID(4 bytes)
    t->valid = 0x00000001;

    // ATTRIBUTES(16 bytes)
    memset(&t->attributes, 0, sizeof(attributes_t));
    t->attributes.mode64bit = true;
    t->attributes.provisionkey = true;
    t->attributes.einittokenkey = false;
    t->attributes.xfrm = 0x03;

    // MRENCLAVE(32 bytes)
    memcpy(&t->mrEnclave, &sigstruct->enclaveHash, sizeof(t->mrEnclave));

    // MRSIGNER(32 bytes)
    sha256(pubkey, KEY_LENGTH, (unsigned char *)&t->mrSigner, 0);

    return t;
}


// (ref re:2.13)
// Fill the fields not required for signature after signing.
static
void update_sigstruct(sigstruct_t *sigstruct, rsa_key_t pubkey, rsa_sig_t sig)
{
    // MODULUS (384 bytes)
    memcpy(sigstruct->modulus, pubkey, sizeof(rsa_key_t));

    // EXPONENT (4 bytes)
    sigstruct->exponent = SGX_RSA_EXPONENT;

    // SIGNATURE (384 bytes)
    memcpy(sigstruct->signature, sig, sizeof(rsa_sig_t));

    // TODO: sig->q1 = floor(signature^2 / modulus)
    //       sig->q2 = floor((signature^3 / modulus) / modulus)
}

static
void update_einittoken(einittoken_t *token)
{
/*
    memcpy(token.cpuSvnLE, keyreq.cpusvn, sizeof(token.cpuSvnLE));
    memcpy(&token.isvsvnLE, &keyreq.isvsvn, sizeof(token.isvsvnLE));
    memcpy(token.keyid, keyreq.keyid, sizeof(token.keyid));
    memcpy(&token.isvprodIDLE, &sig.isvProdID, sizeof(token.isvprodIDLE));
*/
    // TODO: Mask einittoken attribute field with keyreq.attributeMask for maskedattributele
    // TODO : Set KEYID field
}

tcs_t *run_enclave(void *entry, void *codes, unsigned int n_of_pages, char *conf)
{
    assert(sizeof(tcs_t) == PAGE_SIZE);

    // allocate TCS
    tcs_t *tcs = (tcs_t *)memalign(PAGE_SIZE, sizeof(tcs_t));
    if (!tcs)
        err(1, "failed to allocate tcs");

    memset(tcs, 0, sizeof(tcs_t));

    // XXX. tcs structure is freed at the end! maintain as part of
    // keid structure
    _tcs_app = (uint64_t)tcs;

    // Calculate the offset for setting oentry of tcs
    size_t offset = (uintptr_t)entry - (uintptr_t)codes;
    set_tcs_fields(tcs, offset);

    // XXX. exception handler is app specific? then pass it through
    // argument.
    void (*aep)() = exception_handler;

    // load sigstruct from file
    sigstruct_t *sigstruct = load_sigstruct(conf);

    // load einittoken from file
    einittoken_t *token = load_einittoken(conf);

    sgx_dbg(trace, "entry: %p", entry);

    int keid = syscall_create_enclave(entry, codes, n_of_pages, tcs, sigstruct, token, false);
    if (keid < 0)
        err(1, "failed to create enclave");

    keid_t stat;
    if (syscall_stat_enclave(keid, &stat) < 0)
        err(1, "failed to stat enclave");

    // please check STUB_ADDR is mmaped in the main before enable below
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    stub->tcs = stat.tcs;

    EENTER(stat.tcs, aep);
    free(tcs);

    return stat.tcs;
}

// Test an enclave w/o any sigstruct
//   - mock sign
//   - compute mac
//   - execute entry
//   - return upon exit

static
void print_eid_stat(keid_t stat) {
     printf("--------------------------------------------\n");
     printf("kern in count\t: %d\n",stat.kin_n);
     printf("kern out count\t: %d\n",stat.kout_n);
     printf("--------------------------------------------\n");
     printf("encls count\t: %d\n",stat.qstat.encls_n);
     printf("ecreate count\t: %d\n",stat.qstat.ecreate_n);
     printf("eadd count\t: %d\n",stat.qstat.eadd_n);
     printf("eextend count\t: %d\n",stat.qstat.eextend_n);
     printf("einit count\t: %d\n",stat.qstat.einit_n);
     printf("eaug count\t: %d\n",stat.qstat.eaug_n);
     printf("--------------------------------------------\n");
     printf("enclu count\t: %d\n",stat.qstat.enclu_n);
     printf("eenter count\t: %d\n",stat.qstat.eenter_n);
     printf("eresume count\t: %d\n",stat.qstat.eresume_n);
     printf("eexit count\t: %d\n",stat.qstat.eexit_n);
     printf("egetkey count\t: %d\n",stat.qstat.egetkey_n);
     printf("ereport count\t: %d\n",stat.qstat.ereport_n);
     printf("eaccept count\t: %d\n",stat.qstat.eaccept_n);
     printf("--------------------------------------------\n");
     printf("mode switch count : %d\n",stat.qstat.mode_switch);
     printf("tlb flush count\t: %d\n",stat.qstat.tlbflush_n);
     printf("--------------------------------------------\n");
     printf("Pre-allocated EPC SSA region\t: 0x%lx\n",stat.prealloc_ssa);
     printf("Pre-allocated EPC Heap region\t: 0x%lx\n",stat.prealloc_heap);
     printf("Later-Augmented EPC Heap region\t: 0x%lx\n",stat.augged_heap);
     long total_epc_heap = stat.prealloc_heap + stat.augged_heap;
     printf("Total EPC Heap region\t: 0x%lx\n",total_epc_heap);
}


tcs_t *test_run_enclave(void *entry, void *codes, unsigned int n_of_code_pages)
{
    assert(sizeof(tcs_t) == PAGE_SIZE);

    // allocate TCS
    tcs_t *tcs = (tcs_t *)memalign(PAGE_SIZE, sizeof(tcs_t));
    if (!tcs)
        err(1, "failed to allocate tcs");

    memset(tcs, 0, sizeof(tcs_t));

    // XXX. tcs structure is freed at the end! maintain as part of
    // keid structure
    _tcs_app = (uint64_t)tcs;

    // Calculate the offset for setting oentry of tcs
    size_t offset = (uintptr_t)entry - (uintptr_t)codes;
    set_tcs_fields(tcs, offset);

    // XXX. exception handler is app specific? then pass it through
    // argument.
    void (*aep)() = exception_handler;

    // generate RSA key pair
    rsa_key_t pubkey;
    rsa_key_t seckey;

    // load rsa key from conf
    rsa_context *ctx = load_rsa_keys("conf/test.key", pubkey, seckey, KEY_LENGTH_BITS);
    {
        char *pubkey_str = fmt_bytes(pubkey, sizeof(pubkey));
        char *seckey_str = fmt_bytes(seckey, sizeof(pubkey));

        sgx_dbg(info, "pubkey: %.40s..", pubkey_str);
        sgx_dbg(info, "seckey: %.40s..", seckey_str);

        free(pubkey_str);
        free(seckey_str);
    }

    // set sigstruct which will be used for signing
    sigstruct_t *sigstruct = alloc_sigstruct();
    if (!sigstruct)
        err(1, "failed to allocate sigstruct");

    // for testing, all zero = bypass
    memset(sigstruct->enclaveHash, 0, sizeof(sigstruct->enclaveHash));

    // signing with private key
    rsa_sig_t sig;
    rsa_sign(ctx, sig, (unsigned char *)sigstruct, sizeof(sigstruct_t));

    // set sigstruct after signing
    update_sigstruct(sigstruct, pubkey, sig);

    // set einittoken which will be used for MAC
    einittoken_t *token = alloc_einittoken(pubkey, sigstruct);
    if (!token)
        err(1, "failed to allocate einittoken");

    sgx_dbg(trace, "entry: %p", entry);

    int keid = syscall_create_enclave(entry, codes, n_of_code_pages, tcs, sigstruct, token, false);
    if (keid < 0)
        err(1, "failed to create enclave");

    if (syscall_stat_enclave(keid, &stat) < 0)
        err(1, "failed to stat enclave");

// Enable here for stub !
// please check STUB_ADDR is mmaped in the main before enable below
    sgx_stub_info *stub= (sgx_stub_info *)STUB_ADDR;
    stub->tcs = stat.tcs;

    EENTER(stat.tcs, aep);
    free(ctx);
    free(tcs);

    if (syscall_stat_enclave(keid, &stat) < 0)
        err(1, "failed to stat enclave");

    print_eid_stat(stat);

    return stat.tcs;
}


static
const char *fcode_to_str(fcode_t fcode)
{
    switch (fcode) {
	case FUNC_PUTS        : return "PUTS";
    case FUNC_CLOSE_SOCK  : return "CLOSE_SOCK";
    case FUNC_SEND        : return "SEND";
    case FUNC_RECV        : return "RECV";

	case FUNC_MALLOC      : return "MALLOC";
    case FUNC_FREE        : return "FREE";
    case FUNC_SYSCALL     : return "SYSCALL";
    case PRINT_HEX        : return "PRINT_HEX";
    case FUNC_PUTCHAR     : return "PUTCHAR";
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
    fprintf(stderr, "++++++ pending page:%lx\n",
            stub->pending_page);
    fprintf(stderr, "\n");

}

void sgx_puts_tramp(char *data)
{
    puts(data);
}

/*
//TODO: could implement general syscall stub
// but we don't need it now. igrnore it
void sgx_syscall()
{
}
*/
void sgx_close_sock_tramp(void)
{
    close(recv_fd);
    close(send_fd);
    close(client_fd);
    fprintf(stdout, "All sockets are closed :)\n");
}

int sgx_send_tramp(char *ip, char *port, void *msg, size_t len)
{
    struct addrinfo hints, *result, *iter;
    int errcode;
    int sentBytes;
    static char *savedIp = NULL, *savedPort = NULL;

    //for piggyback
    if (send_fd == 0 || strcmp(savedIp,ip) || strcmp(savedPort, port)) {
        savedIp = ip;
        savedPort = port;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        errcode = getaddrinfo(ip, port, &hints, &result);
        if (errcode != 0) {
            perror("getaddr info");
            return -1;
        }

        for (iter = result; iter != NULL; iter = iter->ai_next) {
            send_fd = socket(iter->ai_family, iter->ai_socktype,
                            iter->ai_protocol);

            if (send_fd == -1) {
                perror("socket");
                continue;
            }

            if (connect(send_fd, iter->ai_addr, iter->ai_addrlen) == -1) {
                close(send_fd);
                perror("connect");
                continue;
            }

            break;
        }

        if (iter == NULL) {
            fprintf(stderr, "failed to connect\n");
            return -2;
        }
    }

    if ((sentBytes = send(send_fd, msg, len, 0)) == -1)
    {
        fprintf(stderr, "failed to send\n");
        return -3;
    }

    sgx_dbg(info, "send success: %d bytes are sent\n", sentBytes);
    return sentBytes;
}

int sgx_recv_tramp(char *port, void *buf, size_t len)
{
    struct addrinfo hints;
    struct addrinfo *result, *iter;

    static char *savedPort = NULL;
    struct sockaddr_in client_addr;
    int client_addr_len;
    int errcode;
    int nread;

    //for piggyback
    if (recv_fd == 0 || strcmp(port, savedPort)) {
        savedPort = port;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        errcode = getaddrinfo(NULL, port, &hints, &result);
        if (errcode != 0) {
            perror("getaddrinfo");
            return -1;
        }

        for (iter = result; iter != NULL; iter = iter->ai_next) {
            recv_fd = socket(iter->ai_family, iter->ai_socktype,
                            iter->ai_protocol);
            if (recv_fd == -1)
                continue;
            if (bind(recv_fd, iter->ai_addr, iter->ai_addrlen) == 0)
                break;    /*success*/

            close(recv_fd);
        }

        if (iter == NULL) {
            fprintf(stderr, "failed to bind\n");
            return -2;
        }

        freeaddrinfo(result);

        listen(recv_fd, 1);


        sgx_msg(info, "Waiting for incoming connections...");
        client_addr_len = sizeof(struct sockaddr_in);
        client_fd = accept(recv_fd, (struct sockaddr *)&client_addr,
                             (socklen_t *)&client_addr_len);

        if (client_fd < 0) {
            perror("accept");
            close(recv_fd);
            return -3;
        }
    }

    nread = recv(client_fd, buf, len, 0);
    if (nread == -1) {
        perror("recv");
        close(recv_fd);
        close(client_fd);
        return -4;
    }
    sgx_dbg(info, "recv success   : %d bytes received", nread);
    sgx_dbg(info, "received string: %s", (char *)buf);
    return nread;
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
    int n_read = 0;
    int n_send = 0;

    sgx_msg(info, "Trampoline Entered");
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    clear_abi_in_fields(stub);

    dbg_dump_stub_out(stub);

    switch (stub->fcode) {
    case FUNC_PUTS:
        //sgx_puts(srcData)
        sgx_puts_tramp(stub->out_data1);
        break;
    case FUNC_SEND:
        // sgx_send(ip, port, msg, len)
        n_send = sgx_send_tramp((char *)stub->out_data1, (char *)stub->out_data2,
                       (void *)stub->out_data3, (size_t)stub->out_arg1);
        stub->in_arg1 = n_send;
        break;
    case FUNC_RECV:
        //sgx_recv(port, recv_buf, bufSize): return #of bytes read from the recv
        n_read = sgx_recv_tramp((char *)stub->out_data1, (void *)stub->in_data1,
                               (size_t)stub->out_arg1);
        stub->in_arg1 = n_read;
        break;
    case FUNC_CLOSE_SOCK:
	sgx_close_sock_tramp();
    case FUNC_MALLOC:
        if (stub->mcode == MALLOC_INIT) {
            epc_heap_beg = get_epc_heap_beg();
            stub->heap_beg = epc_heap_beg;
            epc_heap_end = get_epc_heap_end();
            stub->heap_end = epc_heap_end;
        }
        else if (stub->mcode == REQUEST_EAUG) {
            pending_page = syscall_execute_EAUG();
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
    case PRINT_HEX:
        printf("print hex test: %p\n", (void *)stub->addr);
        break;
    case FUNC_PUTCHAR:
        putchar(stub->out_arg1);
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
    dbg_dump_stub_in(stub);
    // ERESUME at the end w/ info->tcs
    ERESUME(stub->tcs, 0);
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

