#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <err.h>
#include <assert.h>
#include <sys/mman.h>
#include <math.h>

#define SGX_KERNEL
#include <sgx-kern.h>
#include <sgx-utils.h>
#include <sgx-kern-epc.h>
#include <sgx-signature.h>

#define NUM_THREADS 1

keid_t kenclaves[MAX_ENCLAVES];

char *empty_page;
static epc_t *epc_heap_beg;
static epc_t *epc_heap_end;

static einittoken_t *app_token;
static int cur_eid;
static epc_t *cur_secs;

static void encls_qemu_init(uint64_t startPage, uint64_t endPage);
static void set_cpusvn(uint8_t svn);
static void set_intel_pubkey(uint64_t pubKey);

void set_app_token(einittoken_t *token)
{
    app_token = token;
}

// encls() : Execute an encls instruction
// out_regs store the output value returned from qemu
static
void encls(encls_cmd_t leaf, uint64_t rbx, uint64_t rcx,
           uint64_t rdx, out_regs_t* out)
{
   sgx_dbg(ttrace,
           "leaf=%d, rbx=0x%"PRIx64", rcx=0x%"PRIx64", rdx=0x%"PRIx64")",
           leaf, rbx, rcx, rdx);

   out_regs_t tmp;
   asm volatile(".byte 0x0F\n\t"
                ".byte 0x01\n\t"
                ".byte 0xcf\n\t"
                :"=a"(tmp.oeax),
                 "=b"(tmp.orbx),
                 "=c"(tmp.orcx),
                 "=d"(tmp.ordx)
                :"a"((uint32_t)leaf),
                 "b"(rbx),
                 "c"(rcx),
                 "d"(rdx)
                :"memory");

    if (out != NULL) {
        *out = tmp;
    }
}

static
void ECREATE(pageinfo_t *pageinfo, epc_t *epc)
{
    // RBX: PAGEINFO(In, EA)
    // RCX: EPCPAGE(In, EA)
    encls(ENCLS_ECREATE,
          (uint64_t)pageinfo,
          (uint64_t)epc_to_vaddr(epc),
          0x0, NULL);
}

static
int EINIT(uint64_t sigstruct, epc_t *secs, uint64_t einittoken)
{
    // RBX: SIGSTRUCT(In, EA)
    // RCX: SECS(In, EA)
    // RDX: EINITTOKEN(In, EA)
    // RAX: ERRORCODE(Out)
    out_regs_t out;
    encls(ENCLS_EINIT, sigstruct, (uint64_t)epc_to_vaddr(secs), einittoken, &out);
    return -(int)(out.oeax);
}

static
void EADD(pageinfo_t *pageinfo, epc_t *epc)
{
    // RBX: PAGEINFO(In, EA)
    // RCX: EPCPAGE(In, EA)
    encls(ENCLS_EADD,
          (uint64_t)pageinfo,
          (uint64_t)epc_to_vaddr(epc),
          0x0, NULL);
}

static
void EEXTEND(uint64_t pageChunk)
{
    // RCX: 256B Page Chunk to be hashed(In, EA)
    encls(ENCLS_EEXTEND, 0x0, pageChunk, 0x0, NULL);
}

static
void EAUG(pageinfo_t *pageinfo, epc_t *epc)
{
    // RBX: PAGEINFO(In, EA)
    // RCX: EPCPAGE(In, EA)
    encls(ENCLS_EAUG,
          (uint64_t)pageinfo,
          (uint64_t)epc_to_vaddr(epc),
          0x0, NULL);
}

static
void encls_qemu_init(uint64_t startPage, uint64_t endPage)
{
    // Function just for initializing EPCM within QEMU
    // based on EPC address in user code
    encls(ENCLS_OSGX_INIT, startPage, endPage, 0x0, NULL);
}

static
void encls_epcm_clear(uint64_t target_epc)
{
    encls(ENCLS_OSGX_EPCM_CLR, target_epc, 0x0, 0x0, NULL);
}

static
void encls_stat(int keid, qstat_t *qstat)
{
    encls(ENCLS_OSGX_STAT, keid, qstat, 0x0, NULL);
}

static
void set_intel_pubkey(uint64_t pubKey)
{
    // Function to set CSR_INTELPUBKEYHASH
    encls(ENCLS_OSGX_PUBKEY, pubKey, 0x0, 0x0, NULL);
}

static
void set_cpusvn(uint8_t svn)
{
    // Set cpu svn.
    encls(ENCLS_OSGX_CPUSVN, svn, 0x0, 0x0, NULL);
}

static
int init_enclave(epc_t *secs, sigstruct_t *sig, einittoken_t *token)
{
    return EINIT((uint64_t)sig, secs, (uint64_t)token);
}

static
secinfo_t *alloc_secinfo(bool r, bool w, bool x, page_type_t pt) {
    secinfo_t *secinfo = memalign(SECINFO_ALIGN_SIZE, sizeof(secinfo_t));
    if (!secinfo)
        return NULL;

    memset(secinfo, 0, sizeof(secinfo_t));

    secinfo->flags.page_type = pt;
    secinfo->flags.r = r;
    secinfo->flags.w = w;
    secinfo->flags.x = x;

    return secinfo;
}

static
secs_t *alloc_secs(uint64_t enclave_addr, uint64_t enclave_size, bool intel_flag)
{
    const int SECS_SIZE = MIN_ALLOC * PAGE_SIZE;
    secs_t *secs = (secs_t *)memalign(SECS_SIZE, sizeof(secs_t));
    if (!secs)
        return NULL;

    memset(secs, 0, sizeof(secs_t));

    // XXX. set ssaFramesize, currently use it as 1 temporarily
    secs->ssaFrameSize         = 1;
    secs->attributes.mode64bit = true;
    secs->attributes.debug     = false;
    secs->attributes.xfrm      = 0x03;

    if (intel_flag) {
        secs->attributes.provisionkey  = false;
        secs->attributes.einittokenkey = true;
    } else {
        secs->attributes.provisionkey  = true;
        secs->attributes.einittokenkey = false;
    }

    secs->baseAddr = enclave_addr;
    secs->size     = enclave_size;

    return secs;
}

static
epc_t *ecreate(int eid, uint64_t enclave_addr, uint64_t enclave_size, bool intel_flag)
{
    pageinfo_t *pageinfo = memalign(PAGEINFO_ALIGN_SIZE, sizeof(pageinfo_t));
    if (!pageinfo)
        err(1, "failed to allocate pageinfo");

    secs_t *secs = alloc_secs(enclave_addr, enclave_size, intel_flag);
    if (!secs)
        err(1, "failed to allocate sec");

    secinfo_t *secinfo = alloc_secinfo(true, true, false, PT_SECS);
    if (!secinfo)
        err(1, "failed to allocate secinfo");

    pageinfo->srcpge  = (uint64_t)secs;
    pageinfo->secinfo = (uint64_t)secinfo;
    pageinfo->secs    = 0; // not used
    pageinfo->linaddr = 0; // not used

    epc_t *epc = get_epc(eid, SECS_PAGE);
    if (!epc)
        err(1, "failed to allocate EPC page for SECS");

    ECREATE(pageinfo, epc);

    //
    // NOTE.
    //  upon ECREATE error, it faults. safely assumes it succeeds.
    //

    free(pageinfo);
    free(secinfo);
    free(secs);

    return epc;
}

static
void measure_enclave_page(uint64_t page_chunk_addr)
{
    EEXTEND(page_chunk_addr);
}

// add (copy) a single page to a epc page
static
bool add_page_to_epc(void *page, epc_t *epc, epc_t *secs, page_type_t pt)
{
    pageinfo_t *pageinfo = memalign(PAGEINFO_ALIGN_SIZE, sizeof(pageinfo_t));
    if (!pageinfo)
        err(1, "failed to allocate pageinfo");

    secinfo_t *secinfo = alloc_secinfo(true, true, false, pt);
    if (!secinfo)
        err(1, "failed to allocate secinfo");

    if (pt == PT_REG) {
        secinfo->flags.x = true;
        // change permissions of a page table entry
        sgx_dbg(ttrace, "+x to %p", (void *)epc);
        if (mprotect(epc, PAGE_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC) == -1)
            err(1, "failed to add executable permission");
    }

    pageinfo->srcpge  = (uint64_t)page;
    pageinfo->secinfo = (uint64_t)secinfo;
    pageinfo->secs    = (uint64_t)epc_to_vaddr(secs);
    pageinfo->linaddr = (uint64_t)epc_to_vaddr(epc);

    sgx_dbg(eadd, "add/copy %p -> %p", page, epc_to_vaddr(epc));
    if (sgx_dbg_eadd)
        hexdump(stderr, page, 32);

    EADD(pageinfo, epc);

    // for EEXTEND
    for(int i = 0; i < PAGE_SIZE/MEASUREMENT_SIZE; i++)
        measure_enclave_page((uint64_t)epc_to_vaddr(epc) + i*MEASUREMENT_SIZE);

    free(pageinfo);
    free(secinfo);

    return true;
}

static
bool aug_page_to_epc(epc_t *epc, epc_t *secs)
{
    pageinfo_t *pageinfo = memalign(PAGEINFO_ALIGN_SIZE, sizeof(pageinfo_t));
    if (!pageinfo) {
        err(1, "failed to allocate pageinfo");
        return false;
    }

    pageinfo->srcpge  = 0;
    pageinfo->secinfo = 0;
    pageinfo->secs    = (uint64_t)secs;
    pageinfo->linaddr = (uint64_t)epc_to_vaddr(epc);

    EAUG(pageinfo, epc);

    free(pageinfo);

    return true;
}

// add multiple pages to epc pages (will be allocated)
static
bool add_pages_to_epc(int eid, void *page, int npages,
                      epc_t *secs, epc_type_t epc_pt, page_type_t pt)
{
    for (int i = 0; i < npages; i++) {
        epc_t *epc = get_epc(eid, (uint64_t)epc_pt);
        if (!epc)
            return false;
        if (!add_page_to_epc(page, epc, secs, pt))
            return false;
        page = (void *)((uintptr_t)page + PAGE_SIZE);
    }
    return true;
}

// add multiple empty pages to epc pages (will be allocated)
static
bool add_empty_pages_to_epc(int eid, int npages, epc_t *secs,
                            epc_type_t epc_pt, page_type_t pt, bool is_heap)
{
    for (int i = 0; i < npages; i ++) {
        epc_t *epc = get_epc(eid, epc_pt);
        if (!epc)
            return false;
        if (!add_page_to_epc(empty_page, epc, secs, pt))
            return false;
        if (i == 0 && is_heap) {
            epc_heap_beg = epc;
            printf("DEBUG epc heap beg is set as %p\n",(void *)epc_heap_beg);
        }
        if ((i == npages -1) && is_heap) {
            epc_heap_end = (epc_t *)((char *)epc + PAGE_SIZE - 1);
            printf("DEBUG epc heap end is set as %p\n",(void *)epc_heap_end);
        }
    }
    return true;
}

unsigned long get_epc_heap_beg() {
    return (unsigned long)epc_heap_beg;
}

unsigned long get_epc_heap_end() {
    return (unsigned long)epc_heap_end;
}

// init custom data structures for qemu-sgx
bool sys_sgx_init(void)
{
    // enclave map
    for (int i = 0; i < MAX_ENCLAVES; i ++) {
        memset(&(kenclaves[i]), 0, sizeof(keid_t));
        kenclaves[i].keid = -1;
    }

    init_epc(NUM_EPC);

    // QEMU Setup initialization for SGX
    encls_qemu_init((uint64_t)get_epc_region_beg(),
                    (uint64_t)get_epc_region_end());

    // Set default cpu svn
    set_cpusvn(CPU_SVN);

    // Initialize an empty page for later use.
    empty_page = memalign(PAGE_SIZE, PAGE_SIZE);
    memset(empty_page, 0, PAGE_SIZE);

    return true;
}

// allocate keid
static
int alloc_keid(void)
{
    for (int i = 0; i < MAX_ENCLAVES; i ++) {
        if (kenclaves[i].keid == -1) {
            kenclaves[i].keid = i;
            return i;
        }
    }
    return -1;
}

// TODO. 1. param entry should be deleted
//       2. param intel_flag looks ugly, integrate it to sig or tcs
// init an enclave
// XXX. need a big lock
// XXX. sig should reflects intel_flag, so don't put it as an arugment

int syscall_create_enclave(void *entry, void *code, unsigned int code_pages,
                           tcs_t *tcs, sigstruct_t *sig, einittoken_t *token,
                           int intel_flag)
{
    int ret = -1;
    int eid = alloc_keid();
    kenclaves[eid].kin_n++;

    // full
    if (eid == -1)
        return -1;
    cur_eid = eid;

    //      enclave (@eid) w/ npages
    //      |
    //      v
    // EPC: [SECS][TCS][TLS]+[TARGET]+[SSA][HEAP][RESV]
    //
    // Note, npages must be power of 2.
    int sec_npages  = 1;
    int tcs_npages  = 1;
    int tls_npages  = get_tls_npages(tcs);
    int ssa_npages  = STACK_PAGE_FRAMES_PER_THREAD;
    int heap_npages = HEAP_PAGE_FRAMES;
    int npages = sec_npages + tcs_npages + tls_npages \
        + code_pages + ssa_npages + heap_npages;
    npages = rop2(npages);

    epc_t *enclave = alloc_epc_pages(npages, eid);
    if (!enclave)
        goto err;

    // allocate secs
    int enclave_size = PAGE_SIZE * npages;
    printf("DEBUG npages is %d\n",npages);
    printf("encalve size is %x\n", PAGE_SIZE * npages);

    void *enclave_addr = epc_to_vaddr(enclave);

    epc_t *secs = ecreate(eid, (uint64_t)enclave_addr, enclave_size, intel_flag);
    if (!secs)
        goto err;
    cur_secs = secs;

    sgx_dbg(info, "enclave addr: %p (size: 0x%x w/ secs = %p)",
            enclave_addr, enclave_size, epc_to_vaddr(secs));

    // get epc for TCS
    epc_t *tcs_epc = get_epc(eid, TCS_PAGE);
    if (!tcs_epc)
        goto err;

    int tls_page_offset = sec_npages + tcs_npages;
    int ssa_page_offset = sec_npages + tcs_npages + tls_npages + code_pages;
    update_tcs_fields(tcs, tls_page_offset, ssa_page_offset);

    sgx_dbg(info, "add tcs %p (@%p)", (void *)tcs, (void *)epc_to_vaddr(tcs_epc));
    if (!add_page_to_epc(tcs, epc_to_vaddr(tcs_epc), secs, PT_TCS))
        goto err;

    // allocate TLS pages
    sgx_dbg(info, "add tls (fs/gs) pages: %p (%d pages)",
            entry, tls_npages);
    if (!add_empty_pages_to_epc(eid, tls_npages, secs, REG_PAGE, PT_REG, 0))
        err(1, "failed to add pages");

    // allocate code pages
    sgx_dbg(info, "add target code/data: %p (%d pages)",
            code, code_pages);
    if (!add_pages_to_epc(eid, code, code_pages, secs, REG_PAGE, PT_REG))
        err(1, "failed to add pages");

    // allocate SSA pages
    sgx_dbg(info, "add ssa pages: %p (%d pages)",
            entry, ssa_npages);
    if (!add_empty_pages_to_epc(eid, ssa_npages, secs, REG_PAGE, PT_REG, 0))
        err(1, "failed to add pages");
    kenclaves[eid].prealloc_ssa = ssa_npages * PAGE_SIZE;

    // allocate heap pages
    sgx_dbg(info, "add heap pages: %p (%d pages)",
            entry, heap_npages);
    if (!add_empty_pages_to_epc(eid, heap_npages, secs, REG_PAGE, PT_REG, 1))
        err(1, "failed to add pages");
    kenclaves[eid].prealloc_heap = heap_npages * PAGE_SIZE;

    // dump sig structure
    {
        char *msg = dbg_dump_sigstruct(sig);
        sgx_dbg(info, "sigstruct:\n%s", msg);
        free(msg);
    }

    if (init_enclave(secs, sig, token))
        goto err;

    // commit
    ret = eid;

//    dbg_dump_epc();

    // remove reserved pages
    free_reserved_epc_pages(enclave);

    // update per-enclave info
    kenclaves[eid].tcs = epc_to_vaddr(tcs_epc);
    kenclaves[eid].enclave = (uint64_t)enclave;

    kenclaves[eid].kout_n++;
    return ret;

 err:
    free_epc_pages(enclave);
    kenclaves[eid].kout_n++;
    return -1;
}

int syscall_stat_enclave(int keid, keid_t *stat)
{
    kenclaves[cur_eid].kin_n++;
    if (keid < 0 || keid >= MAX_ENCLAVES) {
        kenclaves[cur_eid].kout_n++;
        return -1;
    }
    //*stat = kenclaves[keid];
    if (stat == NULL) {
        kenclaves[cur_eid].kout_n++;
        return -1;
    }

    encls_stat(keid, &(kenclaves[cur_eid].qstat));
    kenclaves[cur_eid].kout_n++;
    memcpy(stat, &(kenclaves[keid]), sizeof(keid_t));

	return 0;
}

unsigned long syscall_execute_EAUG() {
    kenclaves[cur_eid].kin_n++;
    epc_t *secs = cur_secs;
    int eid = cur_eid;
    printf("DEBUG current eid is %d\n", eid);

    epc_t *free_epc_page = alloc_epc_page(eid);
    if (free_epc_page == NULL) {
        kenclaves[cur_eid].kout_n++;
        return 0;
    }

    epc_t *epc = get_epc(eid, (uint64_t)REG_PAGE);
    if (!epc) {
        kenclaves[cur_eid].kout_n++;
        return 0;
    }
    printf("DEBUG get epc works\n");
    if (!aug_page_to_epc(epc, secs)) {
        kenclaves[cur_eid].kout_n++;
        return 0;
    }
    kenclaves[cur_eid].augged_heap += PAGE_SIZE;
    kenclaves[cur_eid].kout_n++;
    return (unsigned long)epc;
}

// For unit test
void test_ecreate(pageinfo_t *pageinfo, epc_t *epc)
{
    // RBX: PAGEINFO(In, EA)
    // RCX: EPCPAGE(In, EA)
    encls(ENCLS_ECREATE,
          (uint64_t)pageinfo,
          (uint64_t)epc_to_vaddr(epc),
          0x0, NULL);
}

int test_einit(uint64_t sigstruct, uint64_t secs, uint64_t einittoken)
{
    // RBX: SIGSTRUCT(In, EA)
    // RCX: SECS(In, EA)
    // RDX: EINITTOKEN(In, EA)
    // RAX: ERRORCODE(Out)
    out_regs_t out;
    encls(ENCLS_EINIT, sigstruct, secs, einittoken, &out);
    return -(int)(out.oeax);
}

void test_eadd(pageinfo_t *pageinfo, epc_t *epc)
{
    // RBX: PAGEINFO(In, EA)
    // RCX: EPCPAGE(In, EA)
    encls(ENCLS_EADD,
          (uint64_t)pageinfo,
          (uint64_t)epc_to_vaddr(epc),
          0x0, NULL);
}

void test_eextend(uint64_t pageChunk)
{
    // RCX: 256B Page Chunk to be hashed(In, EA)
    encls(ENCLS_EEXTEND, 0x0, pageChunk, 0x0, NULL);
}

void test_eaug(pageinfo_t *pageinfo, epc_t *epc)
{
    // RBX: PAGEINFO(In, EA)
    // RCX: EPCPAGE(In, EA)
    encls(ENCLS_EAUG,
          (uint64_t)pageinfo,
          (uint64_t)epc_to_vaddr(epc),
          0x0, NULL);
}

// allocate keid
int test_alloc_keid(void)
{
    for (int i = 0; i < MAX_ENCLAVES; i ++) {
        if (kenclaves[i].keid == -1) {
            kenclaves[i].keid = i;
            return i;
        }
    }
    return -1;
}
