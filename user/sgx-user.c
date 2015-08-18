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
#include <sgx-crypto.h>
#include <sgx-trampoline.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sgx-malloc.h>
#include <stdarg.h>
#include <malloc.h>

static keid_t stat;
static uint64_t _tcs_app;

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

void sgx_enter(tcs_t *tcs, void (*aep)())
{
    // RBX: TCS (In, EA)
    // RCX: AEP (In, EA)
    enclu(ENCLU_EENTER, (uint64_t)tcs, (uint64_t)aep, 0, NULL);
}

void sgx_resume(tcs_t *tcs, void (*aep)()) {
    // RBX: TCS (In, EA)
    // RCX: AEP (In, EA)
    enclu(ENCLU_ERESUME, (uint64_t)tcs, (uint64_t)aep, 0, NULL);
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

static
int isInstructionValid(unsigned long addr)
{
    char target[256];
    char cmd[1024];
    char *script = "check_inst.sh";
    char *file_name = "sgx-ssl.inst";

    snprintf(target, sizeof(target), "%lx:", addr);
    fprintf(stderr, "%s\n", target);

    snprintf(cmd, 1024, "sh %s %s %s", script, file_name, target);

    if (system(cmd) != 0)
        return 1;

    return 0;
}

tcs_t *init_enclave(void *base, unsigned int offset, unsigned int n_of_pages, char *conf)
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
    //size_t offset = (uintptr_t)entry - (uintptr_t)codes;
    set_tcs_fields(tcs, offset);

    // XXX. exception handler is app specific? then pass it through
    // argument.
    void (*aep)() = exception_handler;

    // load sigstruct from file
    sigstruct_t *sigstruct = load_sigstruct(conf);

    // load einittoken from file
    einittoken_t *token = load_einittoken(conf);

    //sgx_dbg(trace, "entry: %p", entry);

    int keid = sys_create_enclave(base, n_of_pages, tcs, sigstruct, token, false);
    if (keid < 0)
        err(1, "failed to create enclave");

    keid_t stat;
    if (sys_stat_enclave(keid, &stat) < 0)
        err(1, "failed to stat enclave");

    // please check STUB_ADDR is mmaped in the main before enable below
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    stub->tcs = stat.tcs;

    //sgx_enter(stat.tcs, aep);
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


tcs_t *test_init_enclave(void *base, unsigned int offset, unsigned int n_of_code_pages)
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
    //size_t offset = (uintptr_t)entry - (uintptr_t)codes;
    set_tcs_fields(tcs, offset);
    sgx_dbg(trace, "offset: %lx", (unsigned int)offset);

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

    //sgx_dbg(trace, "entry: %p", entry);

    int keid = sys_create_enclave(base, n_of_code_pages, tcs, sigstruct, token, false);
    if (keid < 0)
        err(1, "failed to create enclave");
    cur_keid = keid;

    if (sys_stat_enclave(keid, &stat) < 0)
        err(1, "failed to stat enclave");

// Enable here for stub !
// please check STUB_ADDR is mmaped in the main before enable below
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;
    stub->tcs = stat.tcs;

    sgx_enter(stat.tcs, aep);
    free(ctx);
    free(tcs);

    if (sys_stat_enclave(keid, &stat) < 0)
        err(1, "failed to stat enclave");

    print_eid_stat(stat);

    return stat.tcs;
}

int sgx_host_read(void *buf, int len)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    if (len <= 0) {
        return -1;
    }
    memcpy(buf, stub->out_data1, len);
    memset(stub->out_data1, 0, SGXLIB_MAX_ARG);

    return len;
}

int sgx_host_write(void *buf, int len)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    if (len <= 0) {
        return -1;
    }
    memset(stub->in_data1, 0, SGXLIB_MAX_ARG);
    memcpy(stub->in_data1, buf, len);

    return len;
}

void *OpenSGX_loader(char *binary, int size, long offset, int n_of_pages)
{
    FILE *fp = NULL;
    unsigned char *buffer;
    int n;
    unsigned char *base_addr;

    buffer = malloc(size);
    memset(buffer, 0, size);

    fp = fopen(binary, "rb");
    if (fp == NULL)
        return 0;

    n = fread(buffer, size, 1, fp);
    // XXX: fread will return 0 when MAX_BUFFER_SIZE > BINARY SIZE
    if (n < 0)
        return 0;

    base_addr = (unsigned char *)memalign(PAGE_SIZE, n_of_pages * PAGE_SIZE);
    memset(base_addr, 0, n_of_pages * PAGE_SIZE);
    memcpy(base_addr, buffer + offset, n_of_pages * PAGE_SIZE);

    return base_addr;
}
