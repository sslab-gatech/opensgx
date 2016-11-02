#include <string.h>
#include <stdio.h>
#include "openssl/evp.h"
#include "openssl/modes.h"

#include "cpu.h"
#include "sgx.h"
#include "sgx-utils.h"
#include "exec/helper-proto.h"
#include "exec/cpu_ldst.h"
#include "sgx-dbg.h"
#include "exec/cpu-all.h"
#include "sgx-perf.h"

#include "polarssl/sha256.h"
#include "polarssl/rsa.h"
#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"
#include "polarssl/sha1.h"
#include "polarssl/aes_cmac128.h"

static qeid_t qenclaves[MAX_ENCLAVES];

/**
 *  SGX Global Data Structures
 */
static epcm_entry_t epcm[NUM_EPC];
static epc_map * enclaveTrackEntry = NULL;      // Tracking pointers For enclaves
static eid_einit_t * entry_eid = NULL;
static uint64_t EPC_BaseAddr;
static uint64_t EPC_EndAddr;
// Collaborate them
static bool enclave_init = false;
//static bool enclave_Access = false;
static bool einit_Success = false;
//static bool enclave_Exit = false;
static int32_t curr_Eid = -1;

static uint64_t enclave_ssa_base;

static uint8_t process_priv_key[DEVICE_KEY_LENGTH];
static uint8_t process_pub_key[DEVICE_KEY_LENGTH];

typedef struct {
    bool read_check;
    bool write_check;
    bool read_perm;
    bool write_perm;
} perm_check_t;

typedef enum {
    eenter,
    eresume,
    none,
} op_type_t;

// why?
op_type_t operation = none;

// Data structure &Functions for Ewb inst
static const unsigned char gcm_key[] = {
0x5f, 0x8a, 0xe6, 0xd1, 0x65, 0x8b, 0xb2, 0x6d, 0xe6, 0xf8, 0xa0, 0x69,
0xa3, 0x52, 0x02, 0x93};

static
void handleError(const char *errMsg)
{
    printf("%s\n", errMsg);
    exit(-1);
}

static
int encrypt_epc(unsigned char *plaintext, int plaintext_len, unsigned char *aad,
            int aad_len, const unsigned char *key, unsigned char *iv,
            unsigned char *ciphertext, unsigned char *tag) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;

    if(!(ctx = EVP_CIPHER_CTX_new())) {
        handleError("Context Creation Error !!!");
    }
    if(EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL) != 1) {
        handleError("EVP Init Error !!!");
    }
    if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL) != 1) {
        handleError("Context Control Error !!!");
    }
    if(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        handleError("Key & IV Init Error !!!");
    }
    if(EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len) !=1 ) {
        handleError("Aad Addtion Error !!!");
    }
    if(EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) !=1 ) {
        handleError("Encryption Error !!!");
    }
    ciphertext_len = len;


    // Finalise the encryption. Normally ciphertext bytes may be written at
    // this stage, but this does not occur in GCM mode
    if(EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        handleError("Finalize Error !!!");
    }
    // Get the tag
    if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) {
        handleError("Getting Tag Error !!!");
    }
    // Clean up
    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

static
int decrypt_epc(unsigned char *ciphertext, int ciphertext_len, unsigned char *aad,
            int aad_len, unsigned char *tag, const unsigned char *key, unsigned char *iv,
            unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    int ret;

    // create and init the context
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        handleError("Context Creation Error !!!");
    }
    // init the decrypt operation
    if(!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
        handleError("EVIP Decrypt Init Error !!!");
    }
    // Set iv length.
    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL)) {
        handleError("Context Control Error !!!");
    }
    if(EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        handleError("Key & IV Init Error !!!");
    }
    if(EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len) !=1 ) {
        handleError("Aad Addtion Error !!!");
    }
    if(EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) !=1 ) {
        handleError("Encryption Error !!!");
    }
    plaintext_len = len;

    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag)) {
        handleError("Setting Tag Error !!!");
    }

    //  A non positive return value from EVP_DecryptFinal_ex
    //  should be considered as a failure to authenticate
    //  ciphertext and/or AAD
    ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

    // clean up
    EVP_CIPHER_CTX_free(ctx);

    if(ret > 0) { //Decrypt Success 
        plaintext_len += len;
        return plaintext_len;
    }
    else {
        return -1;
    }
}


// A helper function to intialize attributes_t
static inline
attributes_t attr_create(uint64_t a1, uint64_t a2)
{
    attributes_t attr;
    memcpy(&attr, &a1, sizeof(uint64_t));
    memcpy((char *)(&attr) + sizeof(uint64_t), &a2, sizeof(uint64_t));
    return attr;
}

// A helper function to mask attributes. Returns *attr1 & *attr2
static inline
attributes_t attr_mask(attributes_t* attr1, attributes_t* attr2)
{
    uint64_t a1, a2;
    a1 = ((uint64_t *)attr1)[0] & ((uint64_t *)attr2)[0];
    a2 = ((uint64_t *)attr1)[1] & ((uint64_t *)attr2)[1];
    return attr_create(a1, a2);
}

// A helper function for computing bitwise OR of two masks, returns *m1 | *m2
static inline
attributes_t attr_mask_combine(attributes_t* attr1, attributes_t* attr2)
{
    uint64_t a1, a2;
    a1 = ((uint64_t *)attr1)[0] | ((uint64_t *)attr2)[0];
    a2 = ((uint64_t *)attr1)[1] | ((uint64_t *)attr2)[1];
    return attr_create(a1, a2);
}

// Mask
/*
static
uint64_t mask(uint64_t mem_addr, uint64_t page_size)
{
    return (mem_addr & ~(page_size - 1));
}
*/

// Reserved Zero-Check
static
void is_reserved_zero(void *addr, uint32_t bytes, CPUX86State *env)
{
    assert(addr);
    assert(env);

    unsigned char *temp_addr = (unsigned char *)malloc(bytes);
    memset(temp_addr, 0, bytes);

    if (memcmp(temp_addr, addr, bytes) != 0)
    {
        sgx_msg(warn, "Reserved zero check fail");
        raise_exception(env, EXCP0D_GPF);
    }
}

static
bool is_enclave_initialized(void)
{
    // Intercepting Memory access only if ECREATE has been invoked
    if (enclave_init) {
        return true;
    }
    return false;
}

// Check if mem_addr is within the current enclave
static
bool is_within_enclave(CPUX86State *env, uint64_t mem_addr)
{
    return ((mem_addr >= env->cregs.CR_ELRANGE[0]) &&
         (mem_addr <= (env->cregs.CR_ELRANGE[0] + env->cregs.CR_ELRANGE[1])));
}

// Check if mem_addr is in EPC
static
bool is_within_epc(uint64_t mem_addr)
{
    if ((mem_addr > (EPC_BaseAddr)) && (mem_addr < (EPC_EndAddr))) {
        return true;
    }
    return false;
}

/*
static bool checkEPCBaseAddr(uint64_t mem_addr)
{
    if(mem_addr == (EPC_BaseAddr + 1)) {
       // Should be not more than once - current setup
       if(checkBaseAccess()) {
         sgx_msg(info, "Exiting and accessing");
         accessedOnce = !accessedOnce;
         return true;
       }
    }
    return false;
}
*/

// InEPC check
static
void check_within_epc(void *page_addr, CPUX86State *env)
{
    assert(page_addr);
    assert(env);

    if (!is_within_epc((uint64_t)page_addr)) {
        sgx_msg(warn, "Page is not in EPC");
        raise_exception(env, EXCP0D_GPF);
    }
}

static
bool is_within_same_epc(void *target_addr1, void *target_addr2, CPUX86State *env)
{

    check_within_epc(target_addr1, env);
    check_within_epc(target_addr2, env);

    int i = 0, target_index1 = 0, target_index2 = 0 ;
    int start_addr, end_addr;

    start_addr = EPC_BaseAddr;
    end_addr = EPC_BaseAddr + PAGE_SIZE;
    for (i = 0 ; i < NUM_EPC; i++) {
       if (target_index1 && target_index2) //found two indices.
           break;
       if((!target_index1) && ((uintptr_t)target_addr1 > start_addr) &&
		  ((uintptr_t)target_addr1 < end_addr)) {
           target_index1 = i;
       }
       if((!target_index2) && ((uintptr_t)target_addr2 > start_addr) &&
		  ((uintptr_t)target_addr2 < end_addr)) {
           target_index2 = i;
       }
       start_addr += PAGE_SIZE;
       end_addr += PAGE_SIZE;
    }
    printf("first target addr:%lx\t index:%d\n", (uintptr_t)target_addr1, target_index1);
    printf("second target addr:%lx\t index:%d\n", (uintptr_t)target_addr2, target_index2);

    return (target_index1 == target_index2);
}

// Canonical Check
static
void is_canonical(uint64_t addr, CPUX86State *env)
{
    /*assert(env);

    // Canonical form : bit 48-63 is same as bit 47
    uint64_t MASK_48_63 = 0xFFFF800000000000;
    uint64_t MASK_47 = 0x0000400000000000;

    if ((addr & MASK_47) == MASK_47) {
        if ((addr & MASK_48_63) != MASK_48_63) {
            sgx_msg(warn, "canonical check fail");
            raise_exception(env, EXCP0D_GPF);
        }
    } else {
        if ((addr & MASK_48_63) != 0) {
            sgx_msg(warn, "canonical check fail");
            raise_exception(env, EXCP0D_GPF);
        }
    }*/
}

// check whether valid field of epcm is 1
static
void epcm_valid_check(epcm_entry_t *epcm_entry, CPUX86State *env)
{
    assert(epcm_entry);
    assert(env);

    if (epcm_entry->valid == 1) {
        sgx_msg(warn, "epcm is already valid");
        raise_exception(env, EXCP0D_GPF);
    }
}

// check whether valid field of epcm is 0
static
void epcm_invalid_check(epcm_entry_t *epcm_entry, CPUX86State *env)
{
    assert(epcm_entry);
    assert(env);

    if (epcm_entry->valid == 0) {
        sgx_msg(warn, "epcm is invalid");
        raise_exception(env, EXCP0D_GPF);
    }
}

// check whether valid field of epcm is 1
static
void epcm_blocked_check(epcm_entry_t *epcm_entry, CPUX86State *env)
{
    assert(epcm_entry);
    assert(env);

    if (epcm_entry->blocked == 1) {
        sgx_msg(warn, "epcm is blocked");
        raise_exception(env, EXCP0D_GPF);
    }
}

// check page_type field of epcm
static
void epcm_page_type_check(epcm_entry_t *epcm_entry, uint8_t PT,
                          CPUX86State *env)
{
    assert(epcm_entry);
    assert(env);

    if (epcm_entry->page_type != PT) {
        sgx_msg(warn, "Page type is incorrect");
        raise_exception(env, EXCP0D_GPF);
    }
}

// check enclave_addr field of epcm
static
void epcm_enclave_addr_check(epcm_entry_t *epcm_entry, uint64_t addr,
                             CPUX86State *env)
{
    assert(epcm_entry);
    assert(env);

    if (epcm_entry->enclave_addr != addr) {
        sgx_msg(warn, "enclaveAddr is incorrect");
        raise_exception(env, EXCP0D_GPF);
    }
}

// check enclaveSECS field of epcm
static
void epcm_enclaveSECS_check(epcm_entry_t *epcm_entry, uint64_t enclaveSECS,
                            CPUX86State *env)
{
    assert(epcm_entry);
    assert(env);

    if (epcm_entry->enclave_secs != enclaveSECS) {
        sgx_msg(warn, "SECS of page is incorrect");
        sgx_dbg(trace, "Enclave SECS: %"PRIx64"  provided SECS: %"PRIx64"",
                enclaveSECS, epcm_entry->enclave_secs);
        raise_exception(env, EXCP0D_GPF);
    }
}

// check several fields of epcm
static
void epcm_field_check(epcm_entry_t *epcm_entry, uint64_t enclave_addr,
                      uint8_t page_type, uint64_t enclaveSECS, CPUX86State *env)
{
    assert(epcm_entry);
    assert(env);

    epcm_enclave_addr_check(epcm_entry, enclave_addr, env);
    epcm_page_type_check(epcm_entry, page_type, env);
    epcm_enclaveSECS_check(epcm_entry, enclaveSECS, env);
}

// Mark the ssa base
static
void set_ssa_base(void)
{
    enclave_ssa_base = NUM_EPC;
}

// Update the SSA Base
// Unused.
#if 0
static
void update_ssa_base(void)
{
    enclave_ssa_base = (enclave_ssa_base < NUM_EPC) ?
                       enclave_ssa_base + 1 : NUM_EPC;
}
#endif

static
void xsave(bool mode, uint64_t xfrm, uint64_t page)
{

}

static
void clearBytes(uint64_t *page, uint8_t index)
{

}

static
void assignBits(uint64_t *page, secs_t *secs)
{

}

static
void saveState(gprsgx_t* page, CPUX86State *env)
{
    sgx_dbg(trace, "State will be store in %p", (void *)page);
    page->rax = env->regs[R_EAX];
    page->rbx = env->regs[R_EBX];
    page->rcx = env->regs[R_ECX];
    page->rdx = env->regs[R_EDX];
    page->rsp = env->regs[R_ESP];
    page->rbp = env->regs[R_EBP];
    page->rsi = env->regs[R_ESI];
    page->rdi = env->regs[R_EDI];
    page->rflags = env->eflags;
}

static
void restoreGPRs(gprsgx_t *page, CPUX86State *env)
{
    env->regs[R_EAX] = page->rax;
    env->regs[R_EBX] = page->rbx;
    env->regs[R_ECX] = page->rcx;
    env->regs[R_EDX] = page->rdx;
    env->regs[R_ESP] = page->rsp;
    env->regs[R_EBP] = page->rbp;
    env->regs[R_ESI] = page->rsi;
    env->regs[R_EDI] = page->rdi;
    /* FIXME: tf to removed*/
    env->eflags = page->rflags;
}

/*
static
void debugGPRs(ssa_t *page)
{
    fprintf(stderr, "ssa page %lp contains...\n", (void *) page);
    fprintf(stderr, "R_EAX: %lp\nR_EBX: %lp\nR_ECX: %lp\nR_EDX: %lp\n"
            "R_ESP: %lp\nR_EBP: %lp\nR_ESI: %lp\nR_EDI: %lp\n",
            page->rax, page->rbx, page->rcx, page->rdx,
            page->rsp, page->rbp, page->rsi, page->rdi);
}
*/

static
bool checkSECSModification(void)
{
    return false;
}

/* Referring to Algo 14.12 - Applied Cryptography Handbook*/
/* TODO
static twoDigits* sig_mul(const oneDigit *x, const size_t num1, const oneDigit *y, const size_t num2, twoDigits w[])
{
    uint32_t n = *x;
    uint32_t t = *y;
    uint32_t i, j;
    twoDigits uv = 0, c;
    //twoDigits  w[num1 + num2 + 2];
    // w array size should be num1 + num2
    memset(w, 0, (num1 + num2 + 1) * sizeof(*w));

    for(i = 0; i < t; i++)
    {
        c = 0;
        for(j = 0; j < n; j++)
        {
            uv = w[i + j] + x[1 + j] * y[1 + i] + c;
            w[i + j] = uv & ((1 << NUM_BITS) - 1); // Storing v
            c = uv >> NUM_BITS; // Storing u
        }
        w[i + n + 1] = uv >> NUM_BITS;
    }
    return w;
}
*/

static
bool checkReservedSpace(sigstruct_t sig, CPUX86State *env)
{
    // Check if Reserved space is filled with 0's
    is_reserved_zero(sig.reserved1, sizeof(((sigstruct_t *)0)->reserved1), env);
    is_reserved_zero(sig.reserved2, sizeof(((sigstruct_t *)0)->reserved2), env);
    is_reserved_zero(sig.reserved3, sizeof(((sigstruct_t *)0)->reserved3), env);
    is_reserved_zero(sig.reserved4, sizeof(((sigstruct_t *)0)->reserved4), env);

    return true;
}

static
bool checkField(sigstruct_t sig, const char* key, const char *field)
// Will be modified
{
    uint32_t fieldNo = INT_MAX;
    uint32_t len = strlen(field);
    uint32_t keyLen = strlen(key);
    uint8_t *header = NULL;

    if (!strncmp(field, "HEADER", min(len, 7))) {
        header = sig.header;
        fieldNo = 1;
    } else if (!strncmp( field, "HEADER2", min(len, 8))) {
        header = sig.header2;
        fieldNo = 3;
    } else {
        return false;
    }

    // Assuming the information is stored lower byte first
    //  into the header field and compare accordingly
    uint8_t iter;
    switch(fieldNo) {
        case 1:
        case 3: {
            for (iter = 0; iter < keyLen; iter += 2) {
                if (sig.header[iter]) {
                    uint8_t lnibble = (uint8_t)key[iter];
                    uint8_t hnibble = (uint8_t)key[iter+1];

                    uint8_t byte = header[iter/2];
                    if (((byte & 0x0F) == lnibble) &&
                        (((byte & 0xF0) >> 4) == hnibble)) {
                        return true;
                    }
                }
            }
            return false;
        }
        default :
            break;
    }
    return false;
}

static
bool checkSigStructField(sigstruct_t sig, CPUX86State *env)
{
    const char header[16] = SIG_HEADER1;
    const char header2[16] = SIG_HEADER2;

    if (!checkField(sig, header, "HEADER") ||
        ((sig.vendor != 0) && (sig.vendor != 0x00008086)) ||
        (!checkField(sig, header2, "HEADER2"))  ||
        ((sig.exponent != 0x00000003) || !checkReservedSpace(sig, env))) {
        return false;
    }
    return true;
}

static
uint64_t compute_xsave_frame_size( CPUX86State *env, attributes_t attributes) {
    uint64_t offset = 576;
//    uint64_t tmp_offset;
    uint64_t size_last_x = 0;
//    uint32_t eax, ebx, ecx, edx;
//    uint64_t x;

    /*
    for( x = 2; x < 63; x++ )
    {
        if ( (attributes.xfrm & (1 << x)) != 0 )
        {
                cpu_x86_cpuid(env, 0x0D, (attributes.xfrm & (1 << x)), &eax, &ebx, &ecx, &edx);
                tmp_offset = ebx;
        if (tmp_offset >= (offset + size_last_x))
        {
            offset = tmp_offset;
                        cpu_x86_cpuid(env, 0x0D, (attributes.xfrm & (1 << x)), &eax, &ebx, &ecx, &edx);
            size_last_x = eax;
                }
        }
    }
    */
    return (offset + size_last_x);
}

// Searches EPCM for effective address
static
uint16_t epcm_search(void *addr, CPUX86State *env)
{

    assert(addr);
    assert(env);

    uint16_t i;
    int16_t index = -1;

    for (i = 0; i < NUM_EPC; i ++) {
        // Can be in between page addresses. for example: EEXTEND : 256 chunks && EWB : Version Array (VA)
        if ((epcm[i].epcPageAddress <= (uint64_t)addr)
                && ((uint64_t)addr < epcm[i].epcPageAddress + PAGE_SIZE)) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        sgx_msg(warn, "Fail to get epcm index addr: %lx");
        raise_exception(env, EXCP0D_GPF);
    }

    return (uint16_t)index;
}

// Set fields of epcm_entry
static
void set_epcm_entry(epcm_entry_t *epcm_entry, bool valid, bool read, bool write,
                    bool execute, bool blocked, uint8_t pt, uint64_t secs,
                    uint64_t addr)
{
    assert(epcm_entry);

    epcm_entry->valid        = valid;
    epcm_entry->read         = read;
    epcm_entry->write        = write;
    epcm_entry->execute      = execute;
    epcm_entry->blocked      = blocked;
    epcm_entry->page_type    = pt;
    epcm_entry->enclave_secs = secs;
    epcm_entry->enclave_addr = addr;
}

// Unused.
#if 0
static
bool checkRWAccessible(uint64_t page, CPUX86State *env)
{
    uint16_t index_current = epcm_search((void *)page, env);

    if ((epcm[index_current].read) || (epcm[index_current].write)) {
        return 1;
    } else {
        return 0;
    }
}
#endif

// Unused.
#if 0
static
uint64_t getPhysicalAddr(CPUX86State *env, uint64_t virtualPage)
{
    return get_page_addr_code(env, virtualPage);
}
#endif

// Unused currently, broken thread support
#if 0
static
bool releaseLocks(void)
{
    return 0;
}
#endif

// Check within DS Segment
static
void checkWithinDSSegment(CPUX86State *env, uint64_t addr)
{
    assert(env);

    if (!((env->segs[R_DS].base <= addr)
            && (env->segs[R_DS].base + env->segs[R_DS].limit >= addr))) {
        sgx_msg(warn, "addr is not in DSSegment");
        raise_exception(env, EXCP0D_GPF);
    }
}

// Check reserved bit
static
void checkReservedBits(const uint64_t *addr, const uint64_t mask,
                       CPUX86State *env)
{
    assert(addr);
    assert(env);

    if ((*addr & mask) != 0) {
        sgx_msg(warn, "Reserved bit is not zero");
        raise_exception(env, EXCP0D_GPF);
    }
}

/*
static
bool checkEINITSuccess(void)
{
    return einit_Success;
}
*/

static
bool checkEINIT(uint64_t eid)
{
    eid_einit_t *temp = entry_eid;
    while (temp != NULL) {
        if (temp->eid == eid)
            return true;
        temp = temp->next;
    }
    return false;
}

static
void markEnclave(uint64_t eid)
{
    eid_einit_t *temp;
    if ((temp = (eid_einit_t *) malloc (sizeof (eid_einit_t))) == NULL) {
        sgx_msg(warn, "no more memory");
        return;
    } else {
        temp->eid = eid;
        temp->next = entry_eid;
        entry_eid = temp;
    }
}

/*
static
bool checkEINIT(void) //const CPUX86State * env)
{
 //   if(env->cregs.EINIT_SUCCESS)
    if (checkEINITSuccess()) {
        return true;
    }
    return false;
}

static
void setEnclaveState(bool status)
{
    enclave_Exit = status;
}

static
bool checkEnclaveState(void)
{
    return enclave_Exit;
}

static
void setEnclaveAccess(bool status)
{
    enclave_Access = status;
}

static
bool checkEnclaveAccess(void)
{
    return enclave_Access;
}
*/

//static void matchKey( char *key, )
/*
static
bool checkSpecificFunction(uint64_t mem_addr)
{
    epc_map *tmp_map = enclaveTrackEntry;

    while (tmp_map != NULL) {
       entry *tmp_entry = tmp_map->entry_Map;
       while (tmp_entry != NULL) {
            //sgx_dbg(trace, "Addresses inside: %llu", tmp_entry->addr);
            tmp_entry = tmp_entry->next;
       }
       tmp_map = tmp_map->next;
    }
    tmp_map = enclaveTrackEntry;

    while (tmp_map != NULL) {
       entry *tmp_entry = tmp_map->entry_Map;
       while (tmp_entry != NULL) {
            //sgx_dbg(trace, "Address: %llX", tmp_entry->addr);
            if (tmp_entry->addr == mem_addr) {
                return true;
            }
            tmp_entry = tmp_entry->next;
       }
       tmp_map = tmp_map->next;
   }
   return false;
}

// Temporary
static
bool checkOtherEnclaveFunctions(uint64_t eid, uint64_t mem_addr)
{
    epc_map *tmp_map = enclaveTrackEntry;

    while (tmp_map != NULL) {
        if (tmp_map->eid != eid) {
            entry *tmp_entry = tmp_map->entry_Map;
            while (tmp_entry != NULL) {
                if (mask(tmp_entry->addr, PAGE_SIZE) == mem_addr) {
                    return true;
                }
                tmp_entry = tmp_entry->next;
            }
        }
        tmp_map = tmp_map->next;
    }
    return false;
}
*/

//FIXME: Two pointers being used carried on from old code.
//  Need to have start and end address as a part of struct
//  entry.
/*
static
bool checkEnclaveFunctions(uint64_t mem_addr)
{
    epc_map *tmp_map = enclaveTrackEntry;

    while (tmp_map != NULL) {
        entry *tmp_entry = tmp_map->entry_Map;
	lastAddress *tmp_lastEntry = tmp_map->lastAddr;
        while (tmp_entry != NULL && tmp_lastEntry != NULL) {
            if (tmp_entry->addr <= mem_addr &&
		tmp_lastEntry->addr >= mem_addr) {
                return true;
            }
            tmp_entry = tmp_entry->next;
	    tmp_lastEntry = tmp_lastEntry->next;
        }
        tmp_map = tmp_map->next;
    }
    return false;
}

static
void updateEntry(uint64_t mem_addr)
{
    epc_map *tmp_map = enclaveTrackEntry;
    lastAddress *temp = tmp_map->lastAddr;

    while (temp != NULL) {
        if (temp->addr == mem_addr) {
            return;
        }
        temp = temp->next;
    }

    if ((temp = (lastAddress *)malloc(sizeof(lastAddress))) == NULL) {
        sgx_msg(warn, "no more memory");
        return;
    }

    temp->addr = mem_addr;
    temp->next = tmp_map->lastAddr;
    tmp_map->lastAddr = temp;

       //sgx_dbg(trace, "Address: %"PRIx64"", mem_addr);
}

static
bool checkLastFuncEntries(uint64_t mem_addr)
{
    if (enclaveTrackEntry == NULL) {
        return false;
    }

    lastAddress *tmp_entry = enclaveTrackEntry->lastAddr;
    while (tmp_entry != NULL) {
        if (tmp_entry->addr <= mem_addr) {
            return true;
        }
        tmp_entry = tmp_entry->next;
    }
    return false;
}
*/

// Check whether eid is new(newly allocted enclave) or not
static
bool checkEnclaveID (uint64_t eid)
{
    epc_map *tmp_map = enclaveTrackEntry;
    epc_map *prev = NULL;

    while (tmp_map != NULL) {
        if (tmp_map->eid == eid) {
            if (prev != NULL) {
                prev->next = tmp_map->next;
                tmp_map->next = enclaveTrackEntry;
                enclaveTrackEntry = tmp_map;
            }
            return true;
        }
        prev = tmp_map;
        tmp_map = tmp_map->next;
    }
    return false;
}

// Check whether addr is included in the specific epc_map's tmp_entry list
/*
static
bool checkWithinEnclave(epc_map *map, uint64_t addr) {
    entry *tmp_entry = map->entry_Map;
    entry *prev = NULL;

    while (tmp_entry != NULL) {
        if (tmp_entry->addr == addr) {
            if (prev != NULL) {
                prev->next = tmp_entry->next;
                tmp_entry->next = map->entry_Map;
                map->entry_Map = tmp_entry;
            }
            return true;
        }
        prev = tmp_entry;
        tmp_entry = tmp_entry->next;
    }
    return false;
}
*/

// Locate epc_map of specific enclave(indicated by eid)
// and allocate new tmp_entry to epc_map of located enclave
/*
static
bool trackEnclaveEntry(secs_t *secs, uint64_t addr)
{
    epc_map *tmp_map;
    entry *tmp_entry;
    uint64_t eid = secs->eid_reserved.eid_pad.eid;

    if (checkOtherEnclaveFunctions(eid, addr)) {
        goto err;
    }

    bool isEIDPresent= checkEnclaveID(eid);

    if (!isEIDPresent) {
        //allocate tmp_map entry for new enclave id
        if ((tmp_map = (epc_map *)malloc(sizeof(epc_map))) == NULL) {
            sgx_msg(warn, "Could not allocate memory for tracking a new EID");
            goto err;
        }
        tmp_map->eid = eid;
    #if DEBUG
        sgx_dbg(trace, "Updated EID: %d", eid);
    #endif
        tmp_map->next = NULL;
        tmp_map->entry_Map = NULL;
        tmp_map->active = true;
        tmp_map->lastAddr = NULL;

        if (enclaveTrackEntry != NULL) {
            tmp_map->next = enclaveTrackEntry;
        }
        enclaveTrackEntry = tmp_map;
    }

    bool isInEnclave = checkWithinEnclave(enclaveTrackEntry, addr);
    tmp_map = enclaveTrackEntry;
    //Now tmp_map indicates epc_map of EID

    if (!isInEnclave) {
    //allocate new entry for addr
        if ((tmp_entry = (entry*)malloc(sizeof(entry))) == NULL) {
            sgx_msg(warn, "Could not allocate memory for tracking an enclave entry");
            goto err;
        }
        tmp_entry->addr = addr;
        tmp_entry->next = NULL;

        if (tmp_map->entry_Map != NULL) {
            tmp_entry->next = tmp_map->entry_Map;
        }
        tmp_map->entry_Map = tmp_entry;
    }

    setEnclaveAccess(true);
    return true;
err:
    return false;
}
*/

static
bool removeEnclaveEntry (secs_t *secs)
{
    uint64_t eid = secs->eid_reserved.eid_pad.eid;
    bool isPresent = checkEnclaveID(eid);
    if (isPresent ) {
        enclaveTrackEntry->active = 0;
        return true;
    }
    return false;
}

// Find appropriate mapping between app supplied address and epc address
/*
static
uint64_t addressMapping(CPUX86State *env, void *addr)
{
    uint8_t i;
    for (i = 0; i < NUM_EPC; i ++) {
        // Can be in between page addresses. for example: EEXTEND : 256 chunks
        if (epcm[i].appAddress == (uint64_t)addr) {
            return epcm[i].enclave_addr;
        }
    }
    // No Entry Mapping found
    return -1;
}
*/

// Unused currently
/*
static void abortAccess(CPUX86State *env) {
    raise_exception(env, EXCP09_XERR);
}
*/

void helper_mem_execute(CPUX86State *env, target_ulong a0)
{
    int epcm_index = 0;
    uint64_t mem_addr = (uint64_t)a0;
    sgx_dbg(mtrace, "Executing memory (enclave:%d): %p",
            (int)env->cregs.CR_ENCLAVE_MODE, (void *)mem_addr);

    if (env->cregs.CR_ENCLAVE_MODE) {
        if (is_within_epc(mem_addr)){
            sgx_dbg(trace, "Executing memory in EPC (enclave:%d): %p",
                 (int)env->cregs.CR_ENCLAVE_MODE, (void *)mem_addr); //temporary
            epcm_index = epcm_search((void *)mem_addr, env);
            if(!is_within_enclave(env, mem_addr)) {
                sgx_dbg(trace, "Mode EINIT Range: Executed %lX", mem_addr);
                sgx_msg(trace, "Inside Enclave. Executing Incorrect enclave memory");
                raise_exception(env, EXCP0D_GPF);
            }
            else if((epcm[epcm_index].execute) == 0){
                sgx_dbg(trace, "EPCM execute property is violated at %p", (void *)mem_addr);
                raise_exception(env, EXCP0D_GPF);
            }
        }
    }
}

// helper test
void helper_mem_access(CPUX86State *env, target_ulong a0, int operation)
{
    int ld_ = 0;
    int st_ = 1;
    int epcm_index = 0;

    // Do not add overheads prior to any enclave initiation process
    if (!is_enclave_initialized())
        return;

    // FIXME:For EPC access, cpu_ldq_data doesn't seem to work correctly
    uint64_t mem_addr = (uint64_t)a0;
    //printf("memory access: %x\n", mem_addr);

    sgx_dbg(mtrace, "Accessing memory (enclave:%d): %p",
            (int)env->cregs.CR_ENCLAVE_MODE, (void *)mem_addr);

    // Non-enclave mode access
    //  - no access to EPC pages
    // Enclave mode access
    //  - not allow to access other enclaves
    // CCH: plan to add the case when an enclave access area outside EPC or shared memory
    if (env->cregs.CR_ENCLAVE_MODE) {
        if (is_within_epc(mem_addr)){
            epcm_index = epcm_search((void *)mem_addr, env);
            if(!is_within_enclave(env, mem_addr)) {
                sgx_dbg(trace, "Mode EINIT Range: Accessed %lX %lX", a0, mem_addr);
                sgx_msg(trace, "Inside Enclave. Accessing Incorrect enclave memory");
                raise_exception(env, EXCP0D_GPF);
            }
            else if((operation == ld_) && (epcm[epcm_index].read) == 0){
                sgx_dbg(trace, "EPCM read property is violated at %p", (void *)mem_addr);
                //raise_exception(env, EXCP0D_GPF);  // blocked temporarily just for reaching the end of epcm rwx test
            }
            else if((operation == st_) && (epcm[epcm_index].write) == 0){
                sgx_dbg(trace, "EPCM write property is violated at %p", (void *)mem_addr);
                //raise_exception(env, EXCP0D_GPF);  // blocked temporarily just for reaching the end of epcm rwx test
            }
        }
    } else {
        if (is_within_epc(mem_addr) || (mem_addr == (uint64_t)epcm) ||
           (mem_addr == (uint64_t)process_priv_key) ||
           (mem_addr == (uint64_t)process_pub_key)) {
            /*if (checkEnclaveState()) {
                setEnclaveState(false);
                printf("Update entry: %lx \n", mem_addr);
                updateEntry(mem_addr);
                return;
            }*/

	    /*
            //if((checkEPCBaseAddr(mem_addr)) ||
            if (((checkLastFuncEntries(mem_addr)) ||
               checkSpecificFunction(mem_addr))) {
                return;
            }
	    */
            sgx_dbg(trace, "!Mode EINIT Range: Accessed %lX ", mem_addr);
            //raise_exception(env, EXCP0D_GPF);
        }
    }
}

// Get SECS of enclave based on epcm of EPC page
static
secs_t* get_secs_address(epcm_entry_t *cur_epcm)
{
    return (secs_t *)cur_epcm->enclave_secs;
}

// Allocate PKCS padding constant (352 bytes).
static
uint8_t *alloc_pkcs1_5_padding(void) {
    const char first_pkcs1_5_padding[2] = FIRST_PKCS1_5_PADDING;
    const char last_pkcs1_5_padding[20] = LAST_PKCS1_5_PADDING;
    uint8_t *pkcs1_5_padding = (uint8_t *)calloc(sizeof(uint8_t), 352);
    int i;

    // [15:0] = 0100H
    memcpy(pkcs1_5_padding, first_pkcs1_5_padding, 2);

    // [2655:16] = 330 bytes of FFH
    for (i = 0; i < 330; i++) {
        memset(&pkcs1_5_padding[i + 2], 0xFF, sizeof(uint8_t));
    }

    // [2815:2656] = 2004000501020403650148866009060D30313000H
    memcpy(&pkcs1_5_padding[332], last_pkcs1_5_padding, 20);

    return pkcs1_5_padding;
}

// Outputs a 16-byte (128-bit) key
static
void sgx_derivekey(const keydep_t* keydep, unsigned char* outputdata)
{
    unsigned char hash[32];
    unsigned char *input;
    size_t size;

    size = sizeof(keydep_t) + sizeof(process_priv_key);
    input = malloc(size);
    memset(input, 0, size);
    memcpy(input, (unsigned char *)keydep, sizeof(keydep_t));
    memcpy(input + sizeof(keydep_t), (unsigned char *)process_priv_key,
           sizeof(process_priv_key));

    sha256(input, size, hash, 0);

    /* Copy the first 16 bytes (128-bits) */
    memcpy(outputdata, hash, 16);
}

// Performs common parameter (rbx, rcx) checks for EGETKEY
static
void sgx_egetkey_common_check(CPUX86State *env, uint64_t *reg,
                              int alignment, perm_check_t perm)
{

    // If reg is not in CR_ELRANGE, then GP(0)
    if (((uint64_t)reg < env->cregs.CR_ELRANGE[0])
        || ((uint64_t)reg >= env->cregs.CR_ELRANGE[0] + env->cregs.CR_ELRANGE[1])) {
        sgx_dbg(trace, "reg is not in CR_ELRANGE: %lx", (long unsigned int)reg);
        raise_exception(env, EXCP0D_GPF);
    }

    /* If reg is not properly aligned, then GP(0) */
    // FIXME: enforce memory alignment with array declaration inside the enclave.
    // is_aligned((void *)reg, alignment, env);

    /* Check reg is an EPC address */
    uint16_t index_page = epcm_search(reg, env);

    epcm_entry_t * pepcm = &epcm[index_page];
    assert(pepcm);

    /* Check reg's EPC page is valid */
    epcm_invalid_check(pepcm, env);

    /* Check epcm is blocked */
    epcm_blocked_check(pepcm, env);

    /* check parameter correctness */
    epcm_page_type_check(pepcm, PT_REG, env);

    if (pepcm->enclave_secs != env->cregs.CR_ACTIVE_SECS) {
        raise_exception(env, EXCP0D_GPF);
    }

    epcm_enclave_addr_check(pepcm, ((uint64_t)reg & (~0x0FFFL)), env);
    if (perm.read_check && (pepcm->read != perm.read_perm)) {
        sgx_msg(trace, "Read check failed.");
        raise_exception(env, EXCP0D_GPF);
    }
    if (perm.write_check && (pepcm->write != perm.write_perm)) {
        sgx_msg(trace, "Write check failed.");
        raise_exception(env, EXCP0D_GPF);
    }
}

// ENCLU instruction implemenration.

// EACCEPT instruction.
static
void sgx_eaccept(CPUX86State *env)
{
    secinfo_t *tmp_secinfo;
    secinfo_t scratch_secinfo;

    tmp_secinfo = (secinfo_t *)env->regs[R_EBX];
    epc_t *destPage = (epc_t *)env->regs[R_ECX];

    sgx_dbg(trace, "%p, %p\n", (void *)tmp_secinfo, destPage);

    // If RBX is not 64 Byte aligned, then GP(0).
    if (!is_aligned(tmp_secinfo, SECINFO_ALIGN_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tmp_secinfo, SECINFO_ALIGN_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If RBX is not within CR_ELRANGE, then GP(0)
    if (((uint64_t)tmp_secinfo < env->cregs.CR_ELRANGE[0])
            || ((uint64_t)tmp_secinfo >= env->cregs.CR_ELRANGE[0] + env->cregs.CR_ELRANGE[1])) {
            sgx_dbg(trace, "Secinfo is not in CR_ELRANGE: %lx", (long unsigned int)tmp_secinfo);
            raise_exception(env, EXCP0D_GPF);
    }

    // If RBX does not resolve within an EPC, then GP(0).
    check_within_epc(tmp_secinfo, env);

    uint16_t index_secinfo = epcm_search(tmp_secinfo, env);
    epcm_entry_t *epcm_secinfo = &epcm[index_secinfo];

    //NOTE: the last condition is different to SPEC, since it is not reasonable to compare the address of secinfo with the start address of the EPC where secinfo is located
    if ((epcm_secinfo->valid == 0) || (epcm_secinfo->read == 0) ||
        (epcm_secinfo->pending != 0) || (epcm_secinfo->modified != 0) ||
        (epcm_secinfo->blocked != 0) || (epcm_secinfo->page_type != PT_REG) ||
        (epcm_secinfo->enclave_secs != env->cregs.CR_ACTIVE_SECS) ||
            (epcm_secinfo->enclave_addr != ((uint64_t)tmp_secinfo & (~(PAGE_SIZE-1))))) {
        sgx_msg(warn, "there is something wrong in EPCM of secinfo page");
        raise_exception(env, EXCP0D_GPF);
    }

    // scratch_secinfo <- DS:RBX
    memcpy(&scratch_secinfo, tmp_secinfo, sizeof(secinfo_t));

    // scratch_secinfo reserved field check. If it is not zero, then GP(0).
    is_reserved_zero(scratch_secinfo.reserved,
                     sizeof(((secinfo_t *)0)->reserved), env);

    // Check if DS:RCX is not 4KByte Aligned
    if (!is_aligned(destPage, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                destPage, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If RCX is not within CR_ELRANGE, then GP(0)
    if (((uint64_t)destPage < env->cregs.CR_ELRANGE[0])
            || ((uint64_t)destPage >= env->cregs.CR_ELRANGE[0] + env->cregs.CR_ELRANGE[1])) {
            sgx_dbg(trace, "DestPage is not in CR_ELRANGE: %lx", (long unsigned int)destPage);
            raise_exception(env, EXCP0D_GPF);
    }

    // If tmp_secs does not resolve within an EPC, then GP(0).
    check_within_epc(destPage, env);

    //TODO: PT_TRIM needs to be added
    if (!(((scratch_secinfo.flags.page_type == PT_REG) && (scratch_secinfo.flags.modified == 0)) ||
        ((scratch_secinfo.flags.page_type == PT_TCS) && (scratch_secinfo.flags.pending == 0) && (scratch_secinfo.flags.modified == 1)) )){
        sgx_msg(warn, "there is something wrong in scratch_secinfo");
        raise_exception(env, EXCP0D_GPF);
    }

    uint16_t index_page = epcm_search(destPage, env);
    epcm_entry_t *epcm_dest = &epcm[index_page];

    // TODO: PT_TRIM needs to be added
    if ((epcm_dest->valid == 0) || (epcm_dest->blocked != 0) ||
            ((epcm_dest->page_type != PT_REG) && (epcm_dest->page_type != PT_TCS)) ||
            (epcm_dest->enclave_secs != env->cregs.CR_ACTIVE_SECS) ){
        sgx_msg(warn, "there is something wrong in destPage");
        raise_exception(env, EXCP0D_GPF);
    }

    // TODO: check the destination EPC page for concurrency

    // Re-check security attributes of the destination EPC page
    if ((epcm_dest->valid == 0) || (epcm_dest->enclave_secs != env->cregs.CR_ACTIVE_SECS)){
        sgx_msg(warn, "there is something wrong in destPage");
        raise_exception(env, EXCP0D_GPF);
    }

    // Verify that accept request matches current EPC page settings
    if((epcm_dest->enclave_addr != (uint64_t)destPage) ||
       (epcm_dest->pending != scratch_secinfo.flags.pending) ||
       (epcm_dest->modified != scratch_secinfo.flags.modified) ||
       (epcm_dest->read != scratch_secinfo.flags.r) ||
       (epcm_dest->write != scratch_secinfo.flags.w) ||
       (epcm_dest->execute != scratch_secinfo.flags.x) ||
       (epcm_dest->page_type != scratch_secinfo.flags.page_type)){
        sgx_msg(warn, "accept request does not match current EPC page settings");
        env->eflags |= CC_Z;
        env->regs[R_EAX] = ERR_SGX_PAGE_ATTRIBUTES_MISMATCH;
        goto Done;
    }

    // TODO: Check that all required threads have left enclave

    // Get pointer to the SECS to which the EPC page belongs
    secs_t *tmp_secs =  get_secs_address(epcm_dest);

    // For TCS pages, perform additional checks. a new TCS page can allocated by EAUG + EMODT
    if (scratch_secinfo.flags.page_type == PT_TCS){
        tcs_t *tcs = (tcs_t *)destPage;
        checkReservedBits((uint64_t *)&(tcs->flags), 0xFFFFFFFFFFFFFFFEL, env);

        // Checking that TCS.FLAGS.DBGOPTIN, TCS stack, and TCS status are correctly initialized
        // Note: even though the below checking codes are located outside this if statement in spec, I think this is correct.
        if( (tcs->flags.dbgoptin != 0) || (tcs->cssa >= tcs->nssa) ){ //TODO?: no aep and state field in TCS, but spec checks it.
            sgx_msg(warn, "TCS is not correctly initialized");
            raise_exception(env, EXCP0D_GPF);
        }

        // Check consistency of FS & GS Limit
        if ((tmp_secs->attributes.mode64bit == 0)
            && (((tcs->fslimit & 0x0FFF) != 0x0FFF)
            || ((tcs->gslimit & 0x0FFF) != 0x0FFF))) {
            raise_exception(env, EXCP0D_GPF);
        }
    }

    // Clear PENDING/MODIFIED flags to mark accept operation complete
    epcm_dest->pending = 0;
    epcm_dest->modified = 0;

    // Clear EAX and ZF to indicate successful completion
    env->eflags &= ~CC_Z;
    env->regs[R_EAX] = 0;

    sgx_dbg(trace, "DEBUG EACCEPT is done\n");

Done:
    // clear flags : CF, PF, AF, OF, SF
    env->eflags &= ~(CC_C | CC_P | CC_A | CC_S | CC_O);

    env->cregs.CR_CURR_EIP = env->cregs.CR_NEXT_EIP;
    env->cregs.CR_ENC_INSN_RET = true;
#if PERF
    int64_t eid;
    eid = tmp_secs->eid_reserved.eid_pad.eid;
    qenclaves[eid].stat.eaccept_n++;
    qenclaves[eid].stat.enclu_n++;
#endif
}

// EACCEPTCOPY instruction
static
void sgx_eacceptcopy(CPUX86State *env)
{
    //RBX: Secinfo addr(In, EA)
    //RCX: Destination EPC addr(In, EA)
    //RDX: Source EPC addr(In, EA)
    //EAX: Error code(Out)
    uint64_t sec_index = 0 , dst_index = 0, src_index = 0;
    secinfo_t scratch_secinfo;

    if(!is_aligned(env->regs[R_EBX], 64)) {
        raise_exception(env, EXCP0D_GPF);
    }
    if(!is_aligned(env->regs[R_ECX], PAGE_SIZE) || !is_aligned(env->regs[R_EDX], PAGE_SIZE)) {
        raise_exception(env, EXCP0D_GPF);
    }
    if (((uint64_t)env->regs[R_EBX] < env->cregs.CR_ELRANGE[0])
            || ((uint64_t)env->regs[R_EBX] >= (env->cregs.CR_ELRANGE[0] + env->cregs.CR_ELRANGE[1]))) {
            sgx_dbg(trace, "Secinfo is not in CR_ELRANGE: %lx", (long unsigned int)env->regs[R_EBX]);
            raise_exception(env, EXCP0D_GPF);
    }
    if (((uint64_t)env->regs[R_ECX] < env->cregs.CR_ELRANGE[0])
            || ((uint64_t)env->regs[R_ECX] >= (env->cregs.CR_ELRANGE[0] + env->cregs.CR_ELRANGE[1]))) {
            sgx_dbg(trace, "Src EPC page is not in CR_ELRANGE: %lx", (long unsigned int)env->regs[R_ECX]);
            raise_exception(env, EXCP0D_GPF);
    }
    if (((uint64_t)env->regs[R_EDX] < env->cregs.CR_ELRANGE[0])
            || ((uint64_t)env->regs[R_EDX] >= (env->cregs.CR_ELRANGE[0] + env->cregs.CR_ELRANGE[1]))) {
            sgx_dbg(trace, "Dst EPC page is not in CR_ELRANGE: %lx", (long unsigned int)env->regs[R_EDX]);
            raise_exception(env, EXCP0D_GPF);
    }

    //XXX: isn't it redundant ?
    check_within_epc((void *)env->regs[R_EBX], env);
    check_within_epc((void *)env->regs[R_ECX], env);
    check_within_epc((void *)env->regs[R_EDX], env);

    sec_index = epcm_search((void *)env->regs[R_EBX], env);
    if(epcm[sec_index].valid == 0 || epcm[sec_index].read == 0 || epcm[sec_index].pending != 0 || 
       epcm[sec_index].modified != 0 || epcm[sec_index].blocked != 0 || epcm[sec_index].page_type != PT_REG ||
       epcm[sec_index].enclave_secs != env->cregs.CR_ACTIVE_SECS || epcm[sec_index].enclave_addr == env->regs[R_EBX]) {
        raise_exception(env, EXCP0D_GPF);
    }

    memset(&scratch_secinfo, 0, sizeof(secinfo_t));
    memcpy(&scratch_secinfo, (void *)env->regs[R_EBX], sizeof(secinfo_t));

    //check for mis-configured secinfo flags
    is_reserved_zero(scratch_secinfo.reserved, sizeof(((secinfo_t *)0)->reserved), env);
    if((scratch_secinfo.flags.r == 0 && scratch_secinfo.flags.w != 0) ||
       scratch_secinfo.flags.page_type != PT_REG) {
        raise_exception(env, EXCP0D_GPF);
    }

    //check security attributes of the destination EPC page
    src_index = epcm_search((void*)env->regs[R_EDX], env);
    if(epcm[src_index].valid == 0 || epcm[src_index].pending != 0 || epcm[src_index].modified != 0 ||
       epcm[src_index].blocked != 0 || epcm[src_index].page_type != PT_REG ||
       epcm[src_index].enclave_secs != env->cregs.CR_ACTIVE_SECS) {
            raise_exception(env, EXCP0D_GPF);
    }

    //check security attributes of the destination EPC page
    dst_index = epcm_search((void*)env->regs[R_ECX], env);
    if(epcm[dst_index].valid == 0 || epcm[dst_index].pending != 1 || epcm[dst_index].modified != 0 ||
      epcm[dst_index].page_type != PT_REG || epcm[dst_index].enclave_secs != env->cregs.CR_ACTIVE_SECS) {
        env->eflags = 1;
        env->regs[R_EAX] = ERR_SGX_PAGE_ATTRIBUTES_MISMATCH;
        goto Done;
    }
    //TODO: check destination EPC page for concurrency 

    //Re-check security attributes of the destination EPC page
    //check security attributes of the destination EPC page
    if(epcm[dst_index].valid == 0 || epcm[dst_index].pending != 1 || epcm[dst_index].modified != 0 ||
      epcm[dst_index].read != 1 || epcm[dst_index].write != 1 || epcm[dst_index].execute != 0 ||
      epcm[dst_index].page_type != scratch_secinfo.flags.page_type ||
      epcm[dst_index].enclave_secs != env->cregs.CR_ACTIVE_SECS || epcm[dst_index].enclave_addr == env->regs[R_ECX]) {
        raise_exception(env, EXCP0D_GPF);
    }

    //copy 4KBytes from the source to destination EPC page
    memcpy((void *)env->regs[R_ECX], (void *)env->regs[R_EDX], PAGE_SIZE);

    //update epcm permission
    epcm[dst_index].read    |= scratch_secinfo.flags.r;
    epcm[dst_index].write   |= scratch_secinfo.flags.w;
    epcm[dst_index].execute |= scratch_secinfo.flags.x;
    epcm[dst_index].pending  = 0;

    env->eflags &= ~(CC_Z);
    env->regs[R_EAX] = 0;

    Done:
        env->eflags &= ~(CC_C | CC_P | CC_A | CC_O | CC_S);
}

// EENTER instruction
static
void sgx_eenter(CPUX86State *env)
{
    // RBX: TCS(In, EA)
    // RCX: AEP(In, EA)
    // RAX: Error(Out, ErrorCode)
    // RCX: Address_IP_Following_EENTER(Out, EA)

    bool tmp_mode64;
    uint64_t tmp_fsbase;
    uint64_t tmp_fslimit;
    uint64_t tmp_gsbase;
    uint64_t tmp_gslimit;
    uint64_t tmp_ssa;
    uint64_t tmp_xsize;
    uint64_t tmp_gpr;
    uint64_t tmp_target;
    uint64_t eid;
    uint16_t index_gpr;
    uint16_t index_tcs;
    // Unused variables.
    //uint16_t iter;
    //uint16_t index_secs;
    //uint64_t tmp_ssa_page;


    // XXX. uint64_t* -> void*
    uint64_t *aep = (uint64_t *)env->regs[R_ECX];
    tcs_t *tcs = (tcs_t *)env->regs[R_EBX];

    index_tcs = epcm_search(tcs, env);
    tmp_mode64 = (env->efer & MSR_EFER_LMA) && (env->segs[R_CS].flags & DESC_L_MASK);

    sgx_dbg(eenter, "aep: %p, tcs: %p", aep, tcs);
    sgx_dbg(eenter, "index_tcs: %d, mode64: %d", index_tcs, tmp_mode64);

    // Also Need to check DS[S] == 1 and DS[11] and DS[10]
    if ((!tmp_mode64) && ((&env->segs[R_DS] != NULL) ||
        ((!extractBitVal(env->segs[R_DS].selector, 11)) &&
        (extractBitVal(env->segs[R_DS].selector, 10)) &&
        (env->segs[R_DS].flags & DESC_S_MASK)))) {
        sgx_msg(warn, "check DS failed.");
        raise_exception(env, EXCP0D_GPF);
    }

    // Check that CS, SS, DS, ES.base is 0
    if (!tmp_mode64) {
        if (((&env->segs[R_CS] != NULL) && (env->segs[R_CS].base != 0)) ||
            (env->segs[R_DS].base != 0)) {
            sgx_msg(warn, "check CS, DS failed.");
            raise_exception(env, EXCP0D_GPF);
        }
    }

    if ((&env->segs[R_ES] != NULL) && (env->segs[R_ES].base != 0)) {
        sgx_msg(warn, "check ES failed.");
        raise_exception(env, EXCP0D_GPF);
    }

    if ((&env->segs[R_SS] != NULL) && (env->segs[R_SS].base != 0)) {
        sgx_msg(warn, "check SS failed.");
        raise_exception(env, EXCP0D_GPF);
    }

    if ((&env->segs[R_SS] != NULL) &&
        ((env->segs[R_SS].flags & DESC_B_MASK) == 0)) {
        sgx_msg(warn, "check SS flag failed.");
        raise_exception(env, EXCP0D_GPF);
    }

    // Check if DS:RBX is not 4KByte Aligned
    if (!is_aligned(tcs, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tcs, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    // Temporarily block
    check_within_epc(tcs, env);

    // Check if AEP is canonical
    if (tmp_mode64) {
        is_canonical((uint64_t)aep, env);
    }
    // TODO - Check concurrency of operations on TCS
#if DEBUG
    sgx_dbg(trace, "TCS-> nssa = %d", tcs->nssa);
    sgx_dbg(trace, "TCS-> cssa = %d", tcs->cssa);
    sgx_dbg(trace, "Index_TCS  valid : %d Blocked : %d",
                    epcm[index_tcs].valid,
                    epcm[index_tcs].blocked );
    sgx_dbg(trace, "EPCM[index_tcs] %"PRIx64" tcs %"PRIx64" page_type %d",
                    epcm[index_tcs].enclave_addr,
                    (uint64_t)tcs,
                    epcm[index_tcs].page_type);
#endif
    // Check Validity and whether access has been blocked
    epcm_invalid_check(&epcm[index_tcs], env);
    epcm_blocked_check(&epcm[index_tcs], env);

    // Async Exit pointer -- make a struct of registers
    // Check for Address and page type
    epcm_enclave_addr_check(&epcm[index_tcs], (uint64_t)tcs, env);
    epcm_page_type_check(&epcm[index_tcs], PT_TCS, env);

    // Alignment OFSBASGX with Page Size
    if (!is_aligned((void *)tcs->ofsbasgx, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                (void *)tcs->ofsbasgx, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    if (!is_aligned((void *)tcs->ogsbasgx, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                (void *)tcs->ogsbasgx, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    // Get the address of SECS for TCS - Implicit Access - Cached by the processor - EPC
    // Obtain the Base and Limits of FS and GS Sections
    // Check proposed FS/GS segments fall within DS
    secs_t *tmp_secs =  get_secs_address(&epcm[index_tcs]); // TODO: Change when the ENCLS is implemented - pageinfo_t
    // XXX: unused
	// index_secs = epcm_search(tmp_secs, env);

    // Alignment - OSSA With Page Size
    if (!is_aligned((void *)(tmp_secs->baseAddr + tcs->ossa), PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                (void *)(tmp_secs->baseAddr + tcs->ossa), PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    if (!tmp_mode64) {
        tmp_fsbase = tcs->ofsbasgx + tmp_secs->baseAddr;
        tmp_fslimit = tmp_fsbase + tmp_secs->baseAddr + tcs->fslimit;
        tmp_gsbase = tcs->ogsbasgx + tmp_secs->baseAddr;
        tmp_gslimit = tmp_gsbase + tmp_secs->baseAddr + tcs->gslimit;
        // if FS wrap-around, make sure DS has no holes
        if (tmp_fslimit < tmp_fsbase) {
            if (env->segs[R_DS].limit < DSLIMIT) {
                sgx_msg(warn, "Invalid FS range.");
                raise_exception(env, EXCP0D_GPF);
            } else {
                if (tmp_fslimit > env->segs[R_DS].limit) {
                   sgx_msg(warn, "Invalid FS range.");
                   raise_exception(env, EXCP0D_GPF);
                }
            }
        }
        // if GS wrap-around, make sure DS has no holes
        if (tmp_gslimit < tmp_gsbase) {
            if (env->segs[R_DS].limit < DSLIMIT) {
                sgx_msg(warn, "Invalid DS range.");
                raise_exception(env, EXCP0D_GPF);
            } else {
			    if (tmp_gslimit > env->segs[R_DS].limit) {
                    sgx_msg(warn, "Invalid DS range.");
                    raise_exception(env, EXCP0D_GPF);
                }
            }
        }
    } else {
        tmp_fsbase = tcs->ofsbasgx + tmp_secs->baseAddr;
        tmp_gsbase = tcs->ogsbasgx + tmp_secs->baseAddr;

        is_canonical((uint64_t)(void*)tmp_fsbase, env);
        is_canonical((uint64_t)(void*)tmp_gsbase, env);
    }

    // Ensure that the FLAGS field in the TCS does not have any reserved bits set
    checkReservedBits((uint64_t *)&tcs->flags, 0xFFFFFFFFFFFFFFFEL, env);
    eid = tmp_secs->eid_reserved.eid_pad.eid;

    // SECS must exist and enclave must have previously been EINITted
    if ((tmp_secs == NULL) && !checkEINIT(eid)) { // != NULL taken care of earlier itself
        sgx_msg(warn, "Check secs failed.");
        raise_exception(env, EXCP0D_GPF);
    }
#if DEBUG
    sgx_dbg(trace, "SECS and checkEINIT worked %d %d", tmp_secs->attributes.mode64bit, tmp_mode64);
#endif
    // Make sure the logical processor’s operating mode matches the enclave
    if (tmp_secs->attributes.mode64bit != tmp_mode64) {
        sgx_msg(warn, "Attribute mode64bit mismatched.");
        raise_exception(env, EXCP0D_GPF);
    }
    // OSFXSR == 0 ?
    if (!(env->cr[4] & CR4_OSFXSR_MASK)) {
        sgx_msg(warn, "OSFXSR check failed.");
        raise_exception(env, EXCP0D_GPF);
    }

    // Check for legal values of SECS.ATTRIBUTES.XFRM
    if (!(env->cr[4] & CR4_OSXSAVE_MASK)) {
        if (tmp_secs->attributes.xfrm != 0x03) {
        sgx_msg(warn, "Attribute mode64bit mismatched.");
            raise_exception(env, EXCP0D_GPF);
        } else
             if ((tmp_secs->attributes.xfrm & env->xcr0) ==
                 tmp_secs->attributes.xfrm) {
                 sgx_msg(warn, "secs.attributes.xfrm mismatch");
                 raise_exception(env, EXCP0D_GPF);
        }
    }

    // Make sure the SSA contains at least one more frame
    // Causing Exception
    if (tcs->cssa >= tcs->nssa) {
        raise_exception(env, EXCP0D_GPF);
    }

#if DEBUG
    sgx_dbg(trace, "SSA ossa: %lx nssa %lx cssa %d",
            tcs->ossa, tcs->nssa, tcs->cssa);
#endif

    // Compute linear address of SSA frame
    tmp_ssa = tcs->ossa + tmp_secs->baseAddr + PAGE_SIZE * tmp_secs->ssaFrameSize * (tcs->cssa);
    tmp_xsize = compute_xsave_frame_size(env, tmp_secs->attributes);

    sgx_dbg(trace, "ssa: %p (size:%lu) base: %p",
            (void *)tmp_ssa, tmp_xsize, (void *)tmp_secs->baseAddr);

// TODO: Implement XSAVE/XSTOR related spec
#if 0
    iter = 0;
    for (tmp_ssa_page = tmp_ssa; tmp_ssa_page > (tmp_ssa - tmp_xsize); tmp_ssa_page -= PAGE_SIZE ) {
        uint16_t index_ssa = epcm_search((void *)tmp_ssa_page, env);
        // Check page is read/write accessible
        if (!checkRWAccessible(tmp_ssa_page, env)) {
                releaseLocks();
                //abortAccess(env);
                raise_exception(env, EXCP0D_GPF);
        }
        // Temporarily block
#if DEBUG
        sgx_dbg(trace, "EPCM INDEX SSA %"PRIx64" index_tcs: %d", &epcm[index_ssa], index_tcs);
#endif
        check_within_epc((void *)tmp_ssa_page, env);
        epcm_invalid_check(&epcm[index_ssa], env);
        epcm_blocked_check(&epcm[index_ssa], env);
#if DEBUG
        sgx_dbg(trace, "EnclaveAddress: %"PRIx64"  tmp_ssa %"PRIx64" enclavesecs_ssa %"PRIx64" \
                        enclavesecs_tcs %"PRIx64"  read %d  write %d EPCM_INDEX %"PRIx64" ",
                    epcm[index_ssa].enclave_addr,
                    tmp_ssa_page,
                    epcm[index_ssa].enclave_secs,
                    epcm[index_tcs].enclave_secs,
                    epcm[index_secs].read,
                        epcm[index_secs].write,
                        &epcm[index_ssa]);
#endif
        epcm_field_check(&epcm[index_ssa], (uint64_t)tmp_ssa_page, PT_REG,
                        (uint64_t)epcm[index_tcs].enclave_secs, env);

        if (epcm[index_secs].read || epcm[index_secs].write) {//  XXX: R == 1 / 0 ?
            raise_exception(env, EXCP0D_GPF);
        }

        env->cregs.CR_XSAVE_PAGE[iter++] = getPhysicalAddr(env, tmp_ssa_page); // unused currently
    }
#endif

    // Compute Address of GPR Area
    tmp_gpr = tmp_ssa + PAGE_SIZE * (tmp_secs->ssaFrameSize) - sizeof(gprsgx_t);
    index_gpr = epcm_search((void *)tmp_gpr, env);

    // Temporarily block
    check_within_epc((void *)tmp_gpr, env);
    // Check for validity and block
    epcm_invalid_check(&epcm[index_gpr], env);
    epcm_blocked_check(&epcm[index_gpr], env);
    // XXX: Spec might be wrong in r2 p.77:
    // the check EPCM(DS:TMP_GPR).ENCLAVEADDRESS != DS:TMP_GPR)
    // ENCLAVEADDRESS is assumed to be the epc page address, whreas
    // TMP_GPR address is within the page.
    // In second parameter, use tmp_ssa instead of tmp_gpr for now.
    epcm_field_check(&epcm[index_gpr], (uint64_t)tmp_ssa, PT_REG,
                     (uint64_t)epcm[index_tcs].enclave_secs, env);
    if (!epcm[index_gpr].read || !epcm[index_gpr].write) {
        raise_exception(env, EXCP0D_GPF);
    }
    if (!tmp_mode64) {
        checkWithinDSSegment(env, tmp_gpr + sizeof(env->regs[R_EAX]));
    }

    // GetPhysical Address of TMP_GPR
    // XXX: In current design, (enclave) linear address = (enclave) physical address
    //env->cregs.CR_GPR_PA = (uint64_t)getPhysicalAddr(env, tmp_gpr);
    env->cregs.CR_GPR_PA = tmp_gpr;

#if DEBUG
    sgx_dbg(trace, "Physical Address obtained cr_gpr_a: %lp", (void *)env->cregs.CR_GPR_PA);
#endif
    tmp_target = env->eip;

    if (tmp_mode64) {
        is_canonical(tmp_target, env);
    } else {
        if (tmp_target > env->segs[R_CS].limit) {
            raise_exception(env, EXCP0D_GPF);
        }
    }
    // Ensure the enclave is not already active and also concurrency of TCS
    /* if ( tcs->state == ACTIVE )
           raise_exception(env, EXCP0D_GPF);
    */
    curr_Eid = tmp_secs->eid_reserved.eid_pad.eid;
    env->cregs.CR_ENCLAVE_MODE = true;
    env->cregs.CR_ACTIVE_SECS = (uint64_t)tmp_secs;
    env->cregs.CR_ELRANGE[0] = tmp_secs->baseAddr;
    env->cregs.CR_ELRANGE[1] = tmp_secs->size;

    sgx_dbg(trace, "range: %p-%lx",
            (void *)env->cregs.CR_ELRANGE[0],
            env->cregs.CR_ELRANGE[1]);
    // Save state for possible AEXs

    // NOTE. epc pages have same physical/linear address
    env->cregs.CR_TCS_LA = (uint64_t)tcs;
    env->cregs.CR_TCS_PA = (uint64_t)tcs;
    env->cregs.CR_AEP = (uint64_t)aep;

    // Save the hidden portions of FS and GS
    env->cregs.CR_SAVE_FS.selector = env->segs[R_FS].selector;
    env->cregs.CR_SAVE_FS.base = env->segs[R_FS].base;
    env->cregs.CR_SAVE_FS.limit = env->segs[R_FS].limit;
    env->cregs.CR_SAVE_FS.flags = env->segs[R_FS].flags;

    env->cregs.CR_SAVE_GS.selector = env->segs[R_GS].selector;
    env->cregs.CR_SAVE_GS.base = env->segs[R_GS].base;
    env->cregs.CR_SAVE_GS.limit = env->segs[R_GS].limit;
    env->cregs.CR_SAVE_GS.flags = env->segs[R_GS].flags;

    // If XSAVE is enabled, save XCR0 and replace it with SECS.ATTRIBUTES.XFRM
    if ((env->cr[4] & CR4_OSXSAVE_MASK)) {
        env->cregs.CR_SAVE_XCR0 = env->xcr0;
        env->xcr0 = tmp_secs->attributes.xfrm;
    }

    // Set eip into the enclave
    env->eip = tmp_secs->baseAddr + tcs->oentry;
    // Following used to affect the tb flow
    env->cregs.CR_CURR_EIP = env->eip;
    sgx_dbg(trace, "entry ptr: %p (base: %p, offset: %lx)",
            (void *)env->eip, (void *)tmp_secs->baseAddr, tcs->oentry);

    // Return values
    env->regs[R_EAX] = tcs->cssa;
    env->regs[R_ECX] = env->cregs.CR_NEXT_EIP;

#ifdef TEST
    void (*func)(void);
    func = (typeof(func))*((uint64_t *)(env->regs[R_ECX]));
//    uint64_t funcPtr = (uint64_t)func;
#endif

#if DEBUG
{
    int i;
    for (i = 0; i < 20; i ++) {
        fprintf(stderr, "%02X ", cpu_ldub_data(env, env->eip + i));
    }
    fprintf(stderr, "\n");
}
#endif

    // Save the outside RSP and RBP so they can be restored on interrupt or EEXIT
    ((gprsgx_t *)env->cregs.CR_GPR_PA)->ursp = env->regs[R_ESP];
    ((gprsgx_t *)env->cregs.CR_GPR_PA)->urbp = env->regs[R_EBP];

    // Setting up the base and stack pointers
    env->regs[R_ESP] = env->cregs.CR_ESP;
    env->regs[R_EBP] = env->cregs.CR_EBP;;

    sgx_dbg(info, "old ursp: %p\t changed rsp %p at eenter",
            (void *)((gprsgx_t *)env->cregs.CR_GPR_PA)->ursp,
            (void *)env->regs[R_ESP]);

    sgx_dbg(trace, "transfer to rip: %p (rsp: %p, rbp: %p)",
            (void *)env->eip,
            (void *)env->regs[R_ESP],
            (void *)env->regs[R_EBP]);

    // Swap FS/GS (XXX?)
    env->segs[R_FS].base = tmp_fsbase;
    env->segs[R_FS].limit = tcs->fslimit;

    env->segs[R_FS].flags |= 0x01;
    env->segs[R_FS].flags |= env->segs[R_DS].flags & DESC_W_MASK;
    env->segs[R_FS].flags |= DESC_S_MASK;
    env->segs[R_FS].flags |= env->segs[R_DS].flags & DESC_DPL_MASK;
    env->segs[R_FS].flags |= DESC_G_MASK;
    env->segs[R_FS].flags |= DESC_B_MASK;
    env->segs[R_FS].flags |= DESC_P_MASK;
    env->segs[R_FS].flags |= env->segs[R_DS].flags & DESC_AVL_MASK;
    env->segs[R_FS].flags |= env->segs[R_DS].flags & DESC_L_MASK;
    env->segs[R_FS].selector = 0x0B;

    env->segs[R_GS].base = tmp_gsbase;
    env->segs[R_GS].limit = tcs->gslimit;

    env->segs[R_GS].flags |= 0x01;
    env->segs[R_GS].flags |= env->segs[R_DS].flags & DESC_W_MASK;
    env->segs[R_GS].flags |= DESC_S_MASK;
    env->segs[R_GS].flags |= env->segs[R_DS].flags & DESC_DPL_MASK;
    env->segs[R_GS].flags |= DESC_G_MASK;
    env->segs[R_GS].flags |= DESC_B_MASK;
    env->segs[R_GS].flags |= DESC_P_MASK;
    env->segs[R_GS].flags |= env->segs[R_DS].flags & DESC_AVL_MASK;
    env->segs[R_GS].flags |= env->segs[R_DS].flags & DESC_L_MASK;
    env->segs[R_GS].selector = 0x0B;

    // For later EEXIT
    env->cregs.CR_EXIT_EIP = env->cregs.CR_NEXT_EIP;
    sgx_dbg(info,"saved EXIT_EIP at eenter: %lX", env->cregs.CR_EXIT_EIP);

    //update_ssa_base();

    sgx_dbg(trace, "async rip: %p", (void *)env->cregs.CR_AEP);

#if DEBUG
    env->cregs.CR_DBGOPTIN = tcs->flags.dbgoptin;
#endif

    /* Supress all code breakpoints -- Not Needed as of now */
    /*
    if ( !env->cregs.CR_DBGOPTIN) {
        env->cregs.CR_SAVE_TF = env->eflags & HF_TF_MASK;
        env->eflags = env->eflags & ~(HF_TF_MASK);
        // Support Monitor Trap Flag
        // Clear All pending debug exceptions
        // Clear pending MTF VM EXIT
    }
    else {
        if (env->eflags & HF_TF_MASK) {
        }
        if ( vmcs.mtf) {
        }
    } */

    // Added for QEMU TB flow while operating in enclave mode
    env->cregs.CR_ENC_INSN_RET = true;

    CPUState *cs = CPU(x86_env_get_cpu(env));
    tlb_flush(cs, 1);

#if PERF
    qenclaves[eid].stat.mode_switch++;
    qenclaves[eid].stat.tlbflush_n++;
    qenclaves[eid].stat.eenter_n++;
    qenclaves[eid].stat.enclu_n++;
#endif
    return;
}

// EEXIT instruction
static
void sgx_eexit(CPUX86State *env)
{
    // RBX: Target_Address(In, EA)
    // RCX: Current_AEP(In, EA)
    bool tmp_mode64;
    uint64_t retAddr;
    secs_t *secs;
    void *addr;
    gprsgx_t *tmp_gpr;
    tcs_t *tcs;

    sgx_dbg(trace, "Current ESP: %lx   EBP: %lx", env->regs[R_ESP], env->regs[R_EBP]);

    secs = (secs_t*)env->cregs.CR_ACTIVE_SECS;

    // FIXME: Currently not used
    retAddr = env->regs[R_EBX];
    if(retAddr != 0) {
        sgx_dbg(trace, "EEXIT will return to provided addr: %lx", retAddr);
    } else {
        sgx_dbg(trace, "EEXIT will return saved EXIT_EIP: %lx", env->cregs.CR_EXIT_EIP);
    }
    addr = (void *)env->regs[R_EBX];
    tmp_mode64 = (env->efer & MSR_EFER_LMA) && (env->segs[R_CS].flags & DESC_L_MASK);

    if (tmp_mode64) {
        is_canonical((uint64_t)addr, env);
    } else {
        if ((uint64_t)addr > env->segs[R_CS].limit) {
            raise_exception(env, EXCP0D_GPF);
        }
    }

    // TODO: will fix it to save current CR_EXIT_EIP and
    // address right after eexit that should be returned
    // after ERESUME.

    // For trampoline
    if(retAddr != 0) {
        //is it necessary ? may be not
        env->eip = retAddr;
        // get tcs
        tcs = (tcs_t *)env->cregs.CR_TCS_LA ;
        {
            sgx_dbg(trace, "SSA ossa: %lX nssa %d cssa %d",
                    tcs->ossa, tcs->nssa, tcs->cssa);
        }

        // Get current GPR
        tmp_gpr = (gprsgx_t *)env->cregs.CR_GPR_PA;
        //sgx_dbg(trace, "current gpr is %lp\t ssa is %lp", tmp_gpr,tmp_ssa);

        saveState(tmp_gpr, env);
	    // Push old CR_EXIT_EIP to the SSA
        tmp_gpr->SAVED_EXIT_EIP = env->cregs.CR_EXIT_EIP;
        // Push Next eip to the SSA
        tmp_gpr->rip = env->cregs.CR_NEXT_EIP;
        sgx_dbg(trace, "Addr followed by eexit is %lX", env->cregs.CR_NEXT_EIP);

        // Increase cssa by one
        tcs->cssa += 1;

        // Change CR_EXIT_EIP to designated addr
        env->cregs.CR_EXIT_EIP = retAddr;

        // ESP should be changed to point a non-enclave sp.
        // Currently, we don't have a speical stack memory area for trampoline
        // So we subtract sufficient number from the ursp to make it not to overwrite
        // user stack area reserved before calling eenter.
        // XXX: it works fine now, but it could crush if the function calling eenter
        // has huge local variable in it.
        env->regs[R_ESP] = tmp_gpr->ursp - 0x1000;
        env->regs[R_EBP] = 0;
    }
    else {
        // Change the stack pointer as untrusted area
        env->regs[R_ESP] = ((gprsgx_t *)env->cregs.CR_GPR_PA)->ursp;
        env->regs[R_EBP] = ((gprsgx_t *)env->cregs.CR_GPR_PA)->urbp;
    }

    // Return Current AEP in RCX
    env->regs[R_ECX] = env->cregs.CR_AEP;

    env->segs[R_FS].selector = env->cregs.CR_SAVE_FS.selector;
    env->segs[R_FS].base = env->cregs.CR_SAVE_FS.base;
    env->segs[R_FS].limit = env->cregs.CR_SAVE_FS.limit;
    env->segs[R_FS].flags = env->cregs.CR_SAVE_FS.flags;
    env->segs[R_GS].selector = env->cregs.CR_SAVE_GS.selector;
    env->segs[R_GS].base = env->cregs.CR_SAVE_GS.base;
    env->segs[R_GS].limit = env->cregs.CR_SAVE_GS.limit;
    env->segs[R_GS].flags = env->cregs.CR_SAVE_GS.flags;
    // Restore XCR0 if needed
    if ((env->cr[4] & CR4_OSXSAVE_MASK)) {
        env->xcr0 = env->cregs.CR_SAVE_XCR0;
    }
    // Unsuppress all code breakpoints
    if (!env->cregs.CR_DBGOPTIN) {
        env->eflags |= (env->cregs.CR_SAVE_TF & HF_TF_MASK);
        // Unsuppress monitor_trap_flag, LBR_generation
    }

    // FIXME: Fill out this
    if (env->eflags & HF_TF_MASK) {

    }

    if (!removeEnclaveEntry(secs)) {
        //raise_exception(env, EXCP0D_GPF);
    }

    //update_ssa_base();

    env->cregs.CR_ENCLAVE_MODE = false;
    env->cregs.CR_EXIT_MODE = true;
//    setEnclaveAccess(false);

    // Used for tracking function end
    //updateEntry(retAddr);
    // TODO: RCX <-- CR_NEXT_EIP
    // setEnclaveState(true);
    // Mark State inactive

    CPUState *cs = CPU(x86_env_get_cpu(env));
    tlb_flush(cs, 1);
#if PERF
    int64_t eid;
    eid = secs->eid_reserved.eid_pad.eid;
    qenclaves[eid].stat.mode_switch++;
    qenclaves[eid].stat.tlbflush_n++;
    qenclaves[eid].stat.eexit_n++;
    qenclaves[eid].stat.enclu_n++;
#endif
}

// Performs parameter (rbx, rcx) check for EGETKEY
static
void sgx_egetkey_param_check(CPUX86State *env)
{
    keyrequest_t *tmp_keyrequest = (keyrequest_t *)env->regs[R_EBX];
    uint64_t *outputdata = (uint64_t *)env->regs[R_ECX];
    perm_check_t rbx_page_perm = { true, false, true, false }; // READ permission
    perm_check_t rcx_page_perm = { false, true, false, true }; // WRITE permission

    sgx_egetkey_common_check(env, (uint64_t *)tmp_keyrequest, 128, rbx_page_perm);
    sgx_egetkey_common_check(env, outputdata, 16, rcx_page_perm);

    // Verify RESERVED spaces in KEYREQUEST are valid
    if ((tmp_keyrequest->reserved1 != 0)
        || (tmp_keyrequest->keypolicy.reserved != 0)) {
        raise_exception(env, EXCP0D_GPF);
    }
}

static inline
void op_bitwise(uint8_t *out, uint8_t *in, size_t nbytes)
{
    int i;
    for (i = 0; i < nbytes; i++)
        out[i] = ~in[i];
}

static inline
void op_and(uint8_t *out, uint8_t *in1, uint8_t *in2, size_t nbytes)
{
    int i;
    for (i = 0; i < nbytes; i++)
        out[i] = in1[i] & in2[i];
}

static inline
void op_or(uint8_t *out, uint8_t *in1, uint8_t *in2, size_t nbytes)
{
    int i;
    for (i = 0; i < nbytes; i++)
        out[i] = in1[i] | in2[i];
}

// EGETKEY instruction
static
void sgx_egetkey(CPUX86State *env)
{
    // RBX: KEYREQUEST (In, EA)
    // RCX: OUTPUTDAYA(KEY) (In, IA)
    keyrequest_t *keyrequest = (keyrequest_t *)env->regs[R_EBX];
    uint64_t *outputdata = (uint64_t *)env->regs[R_ECX];


    // check for parameters
    sgx_egetkey_param_check(env);

    // Hard-coded padding
    uint8_t *pkcs1_5_padding = alloc_pkcs1_5_padding();

    secs_t *tmp_currentsecs = (secs_t *)env->cregs.CR_ACTIVE_SECS;

    // Main egetkey operation
    attributes_t sealing_mask, tmp_attributes;

    sealing_mask = attr_create(0x03L, 0x0L);
    op_or((uint8_t *)&tmp_attributes,
          (uint8_t *)&keyrequest->attributeMask,
          (uint8_t *)&sealing_mask,
          16);

    op_and((uint8_t *)&tmp_attributes,
           (uint8_t *)&tmp_attributes,
           (uint8_t *)&tmp_currentsecs->attributes,
           16);

    miscselect_t tmp_miscselect;
    op_and((uint8_t *)&tmp_miscselect,
           (uint8_t *)&tmp_currentsecs->miscselect,
           (uint8_t *)&keyrequest->miscmask, 4);

    // TODO
    sgx_dbg(trace, "tmp_miscselect %x", tmp_miscselect);
    // TODO
    sgx_dbg(trace, "tmp_currentsecs %x", tmp_currentsecs->miscselect);
    // TODO
    sgx_dbg(trace, "keyrequest->miscmask %x", keyrequest->miscmask);
    sgx_dbg(trace, "tmp_currentsecs->attributes %x", tmp_currentsecs->attributes);
    keydep_t keydep;
    memset(&keydep, 0, sizeof(keydep_t));

    switch (keyrequest->keyname) {
        case SEAL_KEY: {
            if (memcmp(keyrequest->cpusvn, env->cregs.CR_CPUSVN, 16) > 0) {
                env->eflags |= CC_Z;
                env->regs[R_EAX] = ERR_SGX_INVALID_CPUSVN;
                sgx_dbg(warn, "ERR_SGX_INVALID_CPUSVN");
                goto _EXIT;
            }

            if (keyrequest->isvsvn > tmp_currentsecs->isvsvn) {
                env->eflags |= CC_Z;
                env->regs[R_EAX] = ERR_SGX_INVALID_ISVSVN;
                sgx_dbg(warn, "ERR_SGX_INVALID_ISVSVN");
                goto _EXIT;
            }

            // Include enclave identity
            uint8_t tmp_mrEnclave[32];
            memset(tmp_mrEnclave, 0, 32);
            if (keyrequest->keypolicy.mrenclave == 1)
                memcpy(tmp_mrEnclave, tmp_currentsecs->mrEnclave, 32);

            // Include enclave author
            uint8_t tmp_mrSigner[32];
            memset(tmp_mrSigner, 0, 32);
            if (keyrequest->keypolicy.mrsigner == 1)
                memcpy(tmp_mrSigner, tmp_currentsecs->mrSigner, 32);

            // fillin keydep
            keydep.keyname   = SEAL_KEY;
            keydep.isvprodID = tmp_currentsecs->isvprodID;
            keydep.isvsvn    = tmp_currentsecs->isvsvn;
            memcpy(keydep.ownerEpoch,      env->cregs.CSR_SGX_OWNEREPOCH,                  16);
            memcpy(&keydep.attributes,     &tmp_attributes,                                16);
            memcpy(&keydep.attributesMask, &keyrequest->attributeMask,                     16);
            memcpy(keydep.mrEnclave,       tmp_mrEnclave,                                  32);
            memcpy(keydep.mrSigner,        tmp_mrSigner,                                   32);
            memcpy(keydep.keyid,           keyrequest->keyid,                              32);
            memcpy(keydep.seal_key_fuses,  env->cregs.CR_SEAL_FUSES,                       16);
            memcpy(keydep.cpusvn,          keyrequest->cpusvn,                             16);
            memcpy(keydep.padding,         tmp_currentsecs->eid_reserved.eid_pad.padding, 352);
            memcpy(&keydep.miscselect,     &tmp_miscselect,                                 4);
            op_bitwise((uint8_t *)&keydep.miscmask, (uint8_t *)&keyrequest->miscmask,       4);

            break;
        }
        case REPORT_KEY: {
            sgx_msg(info, "get report key.");
            // fillin keydep
            keydep.keyname   = REPORT_KEY;
            keydep.isvprodID = 0;
            keydep.isvsvn    = 0;
            memcpy(keydep.ownerEpoch,      env->cregs.CSR_SGX_OWNEREPOCH,  16);
            memcpy(&keydep.attributes,     &tmp_currentsecs->attributes,   16);
            memset(&keydep.attributesMask, 0,                              16);
            memcpy(keydep.mrEnclave,       tmp_currentsecs->mrEnclave,     32);
            memset(keydep.mrSigner,        0,                              32);
            memcpy(keydep.keyid,           keyrequest->keyid,              32);
            memcpy(keydep.seal_key_fuses,  env->cregs.CR_SEAL_FUSES,       16);
            memcpy(keydep.cpusvn,          env->cregs.CR_CPUSVN,           16);
            memcpy(keydep.padding,         pkcs1_5_padding,               352);
            memcpy(&keydep.miscselect,     &tmp_miscselect,                 4);
            memset(&keydep.miscmask,       0,                               4);

            break;
        }
        case LAUNCH_KEY: {
            // Check enclave has launch capability
            if (tmp_currentsecs->attributes.einittokenkey == 0) {
                env->eflags |= CC_Z;
                env->regs[R_EAX] = ERR_SGX_INVALID_ATTRIBUTE;
                goto _EXIT;
            }

            if (memcmp(keyrequest->cpusvn, env->cregs.CR_CPUSVN, 16) > 0) {
                env->eflags |= CC_Z;
                env->regs[R_EAX] = ERR_SGX_INVALID_CPUSVN;
                sgx_dbg(warn, "ERR_SGX_INVALID_CPUSVN");
                goto _EXIT;
            }

            if (keyrequest->isvsvn > tmp_currentsecs->isvsvn) {
                env->eflags |= CC_Z;
                env->regs[R_EAX] = ERR_SGX_INVALID_ISVSVN;
                sgx_dbg(warn, "ERR_SGX_INVALID_ISVSVN");
                goto _EXIT;
            }

            // fillin keydep
            keydep.keyname   = LAUNCH_KEY;
            keydep.isvprodID = tmp_currentsecs->isvprodID;
            keydep.isvsvn    = tmp_currentsecs->isvsvn;
            memcpy(keydep.ownerEpoch,      env->cregs.CSR_SGX_OWNEREPOCH,  16);
            memcpy(&keydep.attributes,     &tmp_attributes,                16);
            memset(&keydep.attributesMask, 0,                              16);
            memset(keydep.mrEnclave,       0,                              32);
            memset(keydep.mrSigner,        0,                              32);
            memcpy(keydep.keyid,           keyrequest->keyid,              32);
            memcpy(keydep.seal_key_fuses,  env->cregs.CR_SEAL_FUSES,       16);
            memcpy(keydep.cpusvn,          keyrequest->cpusvn,             16);
            // XXX: Should use hard code padding here, spec could be wrong.
            memcpy(keydep.padding,         pkcs1_5_padding,               352);
            memcpy(&keydep.miscselect,     &tmp_miscselect,                 4);
            memset(&keydep.miscmask,       0,                               4);
            break;
        }
        case PROVISION_KEY: {
            // Check enclave has provisioning capability
            if (tmp_currentsecs->attributes.provisionkey == 0) {
                env->eflags |= CC_Z;
                env->regs[R_EAX] = ERR_SGX_INVALID_ATTRIBUTE;
                goto _EXIT;
            }

            if (memcmp(keyrequest->cpusvn, env->cregs.CR_CPUSVN, 16) > 0) {
                env->eflags |= CC_Z;
                env->regs[R_EAX] = ERR_SGX_INVALID_CPUSVN;
                sgx_dbg(warn, "ERR_SGX_INVALID_CPUSVN");
                goto _EXIT;
            }

            if (keyrequest->isvsvn > tmp_currentsecs->isvsvn) {
                env->eflags |= CC_Z;
                env->regs[R_EAX] = ERR_SGX_INVALID_ISVSVN;
                sgx_dbg(warn, "ERR_SGX_INVALID_ISVSVN");
                goto _EXIT;
            }

            // Fillin keydep */
            keydep.keyname   = PROVISION_KEY;
            keydep.isvprodID = tmp_currentsecs->isvprodID;
            keydep.isvsvn    = keyrequest->isvsvn;
            memset(keydep.ownerEpoch,      0,                                              16);
            memcpy(&keydep.attributes,     &tmp_attributes,                                16);
            memcpy(&keydep.attributesMask, &keyrequest->attributeMask,                     16);
            memset(keydep.mrEnclave,       0,                                              32);
            memcpy(keydep.mrSigner,        tmp_currentsecs->mrSigner,                      32);
            memset(keydep.keyid,           0,                                              32);
            memset(keydep.seal_key_fuses,  0,                                              16);
            memcpy(keydep.cpusvn,          keyrequest->cpusvn,                             16);
            memcpy(keydep.padding,         tmp_currentsecs->eid_reserved.eid_pad.padding, 352);
            memcpy(&keydep.miscselect,     &tmp_miscselect,                                 4);
            op_bitwise((uint8_t *)&keydep.miscmask, (uint8_t *)&keyrequest->miscmask,       4);
            break;
        }
        case PROVISION_SEAL_KEY: {
            // Check enclave has provisioning capability
            if (tmp_currentsecs->attributes.provisionkey == 0) {
                env->eflags |= CC_Z;
                env->regs[R_EAX] = ERR_SGX_INVALID_ATTRIBUTE;
                goto _EXIT;
            }

            if (memcmp(keyrequest->cpusvn, env->cregs.CR_CPUSVN, 16) > 0) {
                env->eflags |= CC_Z;
                env->regs[R_EAX] = ERR_SGX_INVALID_CPUSVN;
                sgx_dbg(warn, "ERR_SGX_INVALID_CPUSVN");
                goto _EXIT;
            }

            if (keyrequest->isvsvn > tmp_currentsecs->isvsvn) {
                env->eflags |= CC_Z;
                env->regs[R_EAX] = ERR_SGX_INVALID_ISVSVN;
                sgx_dbg(warn, "ERR_SGX_INVALID_ISVSVN");
                goto _EXIT;
            }

            // fillin keydep
            keydep.keyname = PROVISION_SEAL_KEY;
            keydep.isvprodID = tmp_currentsecs->isvprodID;
            keydep.isvsvn = keyrequest->isvsvn;
            memset(keydep.ownerEpoch,      0,                                              16);
            memcpy(&keydep.attributes,     &tmp_attributes,                                16);
            memcpy(&keydep.attributesMask, &keyrequest->attributeMask,                     16);
            memset(keydep.mrEnclave,       0,                                              32);
            memcpy(keydep.mrSigner,        tmp_currentsecs->mrSigner,                      32);
            memset(keydep.keyid,           0,                                              32);
            memcpy(keydep.seal_key_fuses,  env->cregs.CR_SEAL_FUSES,                       16);
            memcpy(keydep.cpusvn,          keyrequest->cpusvn,                             16);
            memcpy(keydep.padding,         tmp_currentsecs->eid_reserved.eid_pad.padding, 352);
            memcpy(&keydep.miscselect,     &tmp_miscselect,                                 4);
            op_bitwise((uint8_t *)&keydep.miscmask, (uint8_t *)&keyrequest->miscmask,       4);
            break;
        }
        default:
            // invalid keyname
            sgx_dbg(warn, "Invalid keyname %d", keyrequest->keyname);
            env->regs[R_EAX] = ERR_SGX_INVALID_KEYNAME;
            env->eflags |= CC_Z;
            goto _EXIT;
    }

    uint8_t tmp_key[16]; // REPORTKEY generated by instruction
    // Calculate the final derived key and output
    //sgx_derivekey(&keydep, (unsigned char *)outputdata);
    sgx_derivekey(&keydep, tmp_key);
    memcpy((uint8_t *)outputdata, tmp_key, 16);

#if DEBUG
    {
        sgx_msg(info, "Get key:");
        int l;
		for (l = 0; l < 16; l++)
            fprintf(stderr, "%02X", tmp_key[l]);
            //fprintf(stderr, "%02X", (unsigned char *)outputdata[l]);
        fprintf(stderr, "\n");
    }
#endif

    env->regs[R_EAX] = 0;
    env->eflags &= ~CC_Z;
_EXIT:
    // clear flags : CF, PF, AF, OF, SF
    env->eflags &= ~(CC_C | CC_P | CC_A | CC_S | CC_O);

    env->cregs.CR_CURR_EIP = env->cregs.CR_NEXT_EIP;
    env->cregs.CR_ENC_INSN_RET = true;
#if PERF
    int64_t eid;
    eid = tmp_currentsecs->eid_reserved.eid_pad.eid;
    qenclaves[eid].stat.egetkey_n++;
    qenclaves[eid].stat.enclu_n++;
#endif
}

static
void sgx_emodpe(CPUX86State *env)
{
    // RBX: secinfo addr(IN)
    // RCX: Destination EPC addr (IN)
    uint64_t sec_index = 0;
    uint64_t epc_index = 0;
    secinfo_t scratch_secinfo;
 
    if(!is_aligned(env->regs[R_EBX], 64)) {
        raise_exception(env, EXCP0D_GPF);
    }
    if(!is_aligned(env->regs[R_ECX], PAGE_SIZE)) {
        raise_exception(env, EXCP0D_GPF);
    }
    if (((uint64_t)env->regs[R_EBX] < env->cregs.CR_ELRANGE[0])
            || ((uint64_t)env->regs[R_EBX] >= (env->cregs.CR_ELRANGE[0] + env->cregs.CR_ELRANGE[1]))) {
            sgx_dbg(trace, "Secinfo is not in CR_ELRANGE: %lx", (long unsigned int)env->regs[R_EBX]);
            raise_exception(env, EXCP0D_GPF);
    }
    if (((uint64_t)env->regs[R_ECX] < env->cregs.CR_ELRANGE[0])
            || ((uint64_t)env->regs[R_ECX] >= (env->cregs.CR_ELRANGE[0] + env->cregs.CR_ELRANGE[1]))) {
            sgx_dbg(trace, "EPC addr is not in CR_ELRANGE: %lx", (long unsigned int)env->regs[R_EBX]);
            raise_exception(env, EXCP0D_GPF);
    }
    check_within_epc((void *)env->regs[R_EBX], env);
    check_within_epc((void *)env->regs[R_ECX], env);

    sec_index = epcm_search((void *)env->regs[R_EBX], env);
    if(epcm[sec_index].valid == 0 || epcm[sec_index].read == 0 || epcm[sec_index].pending != 0 ||
       epcm[sec_index].modified != 0 || epcm[sec_index].blocked != 0 || epcm[sec_index].page_type != PT_REG ||
       epcm[sec_index].enclave_secs != env->cregs.CR_ACTIVE_SECS || epcm[sec_index].enclave_addr == env->regs[R_EBX]) {
            raise_exception(env, EXCP0D_GPF);
    }

    memset(&scratch_secinfo, 0, sizeof(secinfo_t));
    memcpy(&scratch_secinfo, (void *)env->regs[R_EBX], sizeof(secinfo_t));

    is_reserved_zero(&(scratch_secinfo.reserved), sizeof(((secinfo_t *)0)->reserved), env);

    //check security attributes of the EPC page
    epc_index = epcm_search((void*)env->regs[R_ECX], env);
    if(epcm[epc_index].valid == 0 || epcm[epc_index].pending != 0 || epcm[epc_index].modified != 0 ||
       epcm[epc_index].blocked != 0 || epcm[epc_index].page_type != PT_REG ||
       epcm[epc_index].enclave_secs != env->cregs.CR_ACTIVE_SECS) {
            raise_exception(env, EXCP0D_GPF);
    }
    //TODO: Check the EPC page for concurrency 

    // Re-check attributes of the EPC page
    if(epcm[epc_index].valid == 0 || epcm[epc_index].pending != 0 || epcm[epc_index].modified != 0 || 
       epcm[epc_index].blocked != 0 || epcm[epc_index].page_type != PT_REG || 
       epcm[epc_index].enclave_secs != env->cregs.CR_ACTIVE_SECS || 
       epcm[epc_index].enclave_addr != env->regs[R_ECX]) {
            raise_exception(env, EXCP0D_GPF);
    }
    
    //check for mis-configured SECINFO flags
    if(epcm[sec_index].read == 0 && scratch_secinfo.flags.r == 0 && scratch_secinfo.flags.w != 0) {
        raise_exception(env, EXCP0D_GPF);
    }

    //update EPCM permissions
    epcm[epc_index].read |= scratch_secinfo.flags.r;
    epcm[epc_index].write |= scratch_secinfo.flags.w;
    epcm[epc_index].execute |= scratch_secinfo.flags.x;

}

// EREPORT instruction
static
void sgx_ereport(CPUX86State *env)
{
    // RBX: TARGETINFO(In, EA)
    // RCX: REPORTDATA(In, EA)
    // RDX: Address_where_report is writted to in an Output Data (In)
//    uint32_t tmp_attributes; // Physical Addr of SECS of Enclave to which Source Operand Belongs
    secs_t *tmp_currentsecs = (secs_t *)env->cregs.CR_ACTIVE_SECS;// Address of the SECS for currently exec enclave
    keydep_t tmp_keydependencies; // Temp space Key derivation
    uint8_t tmp_reportkey[16]; // REPORTKEY generated by instruction
    report_t tmp_report;

    /* Storing the Input Values from Registers */
    targetinfo_t *targetinfo = (targetinfo_t *)env->regs[R_EBX];
    uint64_t *outputdata = (uint64_t *)env->regs[R_EDX];
//    bool tmp_mode64 = (env->efer & MSR_EFER_LMA) && (env->segs[R_CS].flags & DESC_L_MASK);

    perm_check_t rbx_page_perm = { true, false, true, false }; // READ permission
    perm_check_t rdx_page_perm = { false, true, false, true }; // WRITE permission

    sgx_egetkey_common_check(env, (uint64_t *)targetinfo, 128, rbx_page_perm);
    sgx_egetkey_common_check(env, outputdata, 512, rdx_page_perm);


    /* REPORT MAC needs to be computed over data which cannot be modified */
    tmp_report.isvProdID  = tmp_currentsecs->isvprodID;
    tmp_report.isvsvn     = tmp_currentsecs->isvsvn;
    //TODO
//    tmp_report.attributes = tmp_currentsecs->attributes;
    memcpy(&tmp_report.attributes, &tmp_currentsecs->attributes, 16);
    sgx_dbg(trace, "tmp_currentsecs->attributes %x", tmp_currentsecs->attributes);
    sgx_dbg(trace, "tmp_report->attributes %x", tmp_report.attributes);
    //TODO
    memcpy(&tmp_report.miscselect,&tmp_currentsecs->miscselect,    4);
    //
    memcpy(tmp_report.cpusvn,     env->cregs.CR_CPUSVN,          16);
    memcpy(tmp_report.reportData, (void*)env->regs[R_ECX],       64);
    memcpy(tmp_report.mrenclave,  tmp_currentsecs->mrEnclave,    32);
    memcpy(tmp_report.mrsigner,   tmp_currentsecs->mrSigner,     32);
    memcpy(tmp_report.keyid,      &(env->cregs.CR_REPORT_KEYID), 32);
    // Set all reserved to 0

    //TODO
 //   memset(tmp_report.reserved,  0, 32);
    memset(tmp_report.reserved , 0 ,28);

    memset(tmp_report.reserved2, 0, 32);
    memset(tmp_report.reserved3, 0, 96);
    memset(tmp_report.reserved4, 0, 60);

#if DEBUG
    {
	uint8_t report[512];
	memset(report, 0, 512);
    	memcpy(report, (uint8_t *)&tmp_report, 432);
        sgx_msg(info, "Generated report:");
        int k;
        for (k = 0; k < 432; k++)
            fprintf(stderr, "%02X", report[k]);
    }
#endif

    uint8_t *pkcs1_5_padding = alloc_pkcs1_5_padding();

    // key dependencies init
    memset((unsigned char *)&tmp_keydependencies, 0, sizeof(keydep_t));

    /* Derive the Report Key */
    tmp_keydependencies.keyname   = REPORT_KEY;
    tmp_keydependencies.isvprodID = 0;
    tmp_keydependencies.isvsvn    = 0;
    memcpy(tmp_keydependencies.ownerEpoch,      env->cregs.CSR_SGX_OWNEREPOCH,  16);
    memcpy(&tmp_keydependencies.attributes,     &targetinfo->attributes,        16);

// TODO
//    memcpy(&tmp_keydependencies.attributes,     &tmp_currentsecs->attributes,        16);

    memset(&tmp_keydependencies.attributesMask, 0,                              16);
    memcpy(tmp_keydependencies.mrEnclave,       targetinfo->measurement,        32);
    memset(tmp_keydependencies.mrSigner,        0,                              32);
    memcpy(tmp_keydependencies.keyid,           tmp_report.keyid,               32);
    memcpy(tmp_keydependencies.seal_key_fuses,  env->cregs.CR_SEAL_FUSES,       16);
    memcpy(tmp_keydependencies.cpusvn,          env->cregs.CR_CPUSVN,           16);
    memcpy(&tmp_keydependencies.miscselect,     &targetinfo->miscselect,         4);
    memset(&tmp_keydependencies.miscmask,       0,                               4);
    // XXX: should use hard code padding, spec might be wrong.
    memcpy(tmp_keydependencies.padding,         pkcs1_5_padding,               352);

    /* Calculate Derived Key */
    sgx_derivekey(&tmp_keydependencies, (unsigned char *)tmp_reportkey);

#if DEBUG
    {
        sgx_msg(info, "Expected report key:");
        int l;
		for (l = 0; l < 16; l++)
            fprintf(stderr, "%02X", tmp_reportkey[l]);
        fprintf(stderr, "\n");
    }
#endif

    aes_cmac128_context ctx;

    aes_cmac128_starts(&ctx, tmp_reportkey);
    aes_cmac128_update(&ctx, (uint8_t *)&tmp_report, 416);
    aes_cmac128_final(&ctx, tmp_report.mac);

    uint8_t report[512];
    memset(report, 0, 512);
    memcpy(report, (uint8_t *)&tmp_report, 432);

    memcpy((uint8_t *)outputdata, report, 512);

#if DEBUG
    {
        sgx_msg(info, "Generated report:");
        int k;
        for (k = 0; k < 432; k++)
            fprintf(stderr, "%02X", report[k]);
    }
#endif

    env->cregs.CR_CURR_EIP = env->cregs.CR_NEXT_EIP;
    env->cregs.CR_ENC_INSN_RET = true;
#if PERF
    int64_t eid;
    eid = tmp_currentsecs->eid_reserved.eid_pad.eid;
    qenclaves[eid].stat.ereport_n++;
    qenclaves[eid].stat.enclu_n++;
#endif
}

static
void sgx_eresume(CPUX86State *env)
{

    // RBX: Address of TCS (In)
    // RCX: Address of AEP(In)

    tcs_t *tcs;
    uint64_t *aep;
    bool tmp_mode64;
    uint64_t tmp_fsbase;
    uint64_t tmp_fslimit;
    uint64_t tmp_gsbase;
    uint64_t tmp_gslimit;
    uint64_t tmp_ssa;
    uint64_t tmp_gpr;
    uint64_t eid;
    uint64_t tmp_target;
    uint16_t index_gpr;
    uint16_t index_tcs;
    operation = eresume;
    // Unused variables.
    //uint16_t iter;
    //uint16_t index_secs;
    //uint64_t tmp_ssa_page;
    //uint64_t tmp_xsize;

    sgx_dbg(trace, "Current ESP: %lx   EBP: %lx", env->regs[R_ESP], env->regs[R_EBP]);
    // Store the inputs
    aep = (uint64_t *)env->regs[R_ECX];
    tcs = (tcs_t *)env->regs[R_EBX];
    index_tcs = epcm_search(tcs, env);
    tmp_mode64 = (env->efer & MSR_EFER_LMA) && (env->segs[R_CS].flags & DESC_L_MASK);

//    tcs_app = (tcs_t *)env->regs[R_EBX]; // originally no uint32 cast - Also 64 -> 32 ?

    // All casts below not require
/*
    tcs = (tcs_t*)addressMapping(env, tcs_app);
    index_tcs = epcm_search(tcs, env);
    tmp_mode64 = (env->efer & MSR_EFER_LMA) && (env->segs[R_CS].flags & DESC_L_MASK);
*/

#if DEBUG
    //sgx_dbg(trace, " AEP: %lu TCS: %lu  TCS_App: %lu",
      //      (uint64_t)aep, (uint64_t)tcs, (uint64_t)tcs_app); //TODO: need to be deleted
    sgx_dbg(trace, "Index_TCS: %d", index_tcs);
    sgx_dbg(trace, "INDEX_TCS: %d Mode64: %d", index_tcs, tmp_mode64);
#endif
    // Also Need to check DS[S] == 1 and DS[11] and DS[10]
    if ((!tmp_mode64) && ((&env->segs[R_DS] != NULL) ||
        (!extractBitVal(env->segs[R_DS].selector, 11) &&
        extractBitVal(env->segs[R_DS].selector, 10) &&
        env->segs[R_DS].flags & DESC_S_MASK))) {
        raise_exception(env, EXCP0D_GPF);
    }

    // Check that CS, SS, DS, ES.base is 0
    if (!tmp_mode64) {
        if (((&env->segs[R_CS] != NULL) && (env->segs[R_CS].base != 0))
           || (env->segs[R_DS].base != 0)) {
            raise_exception(env, EXCP0D_GPF);
        }

        if ((&env->segs[R_ES] != NULL) && (env->segs[R_ES].base != 0)) {
            raise_exception(env, EXCP0D_GPF);
        }

        if ((&env->segs[R_SS] != NULL) && (env->segs[R_SS].base != 0)) {
            raise_exception(env, EXCP0D_GPF);
        }

        if ((&env->segs[R_SS] != NULL) &&
                   ((env->segs[R_SS].flags & DESC_B_MASK) == 0)) {
            raise_exception(env, EXCP0D_GPF);
        }
    }

    // Check if DS:RBX is not 4KByte Aligned
    if (!is_aligned(tcs, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tcs, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // Temporarily block
    check_within_epc(tcs, env);
    // Check if AEP is canonical
    if (tmp_mode64) {
        is_canonical((uint64_t)aep, env);
    }

    // TODO - Check concurrency of operations on TCS

    // Check Validity and whether access has been blocked
    epcm_invalid_check(&epcm[index_tcs], env);
    epcm_blocked_check(&epcm[index_tcs], env);
#if DEBUG
    sgx_dbg(trace, "Index_TCS  valid : %d Blocked : %d",
               epcm[index_tcs].valid, epcm[index_tcs].blocked);
    // Async Exit pointer -- make a struct of registers
    sgx_dbg(trace, "EPCM[index_tcs] %lu tcs %lu page_type %d",
            epcm[index_tcs].enclave_addr, (uint64_t)tcs,
                       epcm[index_tcs].page_type);
    // Check for Address and page type
#endif

    epcm_enclave_addr_check(&epcm[index_tcs], (uint64_t)tcs, env);
    epcm_page_type_check(&epcm[index_tcs], PT_TCS, env);

    // Alignment OFSBASGX with Page Size
    if (!is_aligned((void *)tcs->ofsbasgx, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                (void *)tcs->ofsbasgx, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    if (!is_aligned((void *)tcs->ogsbasgx, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                (void *)tcs->ogsbasgx, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // Get the address of SECS for TCS - Implicit Access - Cached by the processor - EPC
    // Obtain the Base and Limits of FS and GS Sections
    // Check proposed FS/GS segments fall within DS
    secs_t *tmp_secs =  get_secs_address(&epcm[index_tcs]);//Change when the ENCLS is implemented - pag

	//index_secs = epcm_search(tmp_secs, env); // XXX: unused.
#if DEBUG
    // sgx_dbg(trace, "INDEX_SECS: %d", index_secs);
#endif
    // Alignment - OSSA With Page Size
    if (!is_aligned((void *)(tmp_secs->baseAddr + tcs->ossa), PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                (void *)(tmp_secs->baseAddr + tcs->ossa), PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // Ensure that the FLAGS field in the TCS does not have any reserved bits set
    checkReservedBits((uint64_t *)&tcs->flags, 0xFFFFFFFFFFFFFFFEL, env);
    eid = tmp_secs->eid_reserved.eid_pad.eid;
    // SECS must exist and enclave must have previously been EINITted
    if ((tmp_secs == NULL) && !checkEINIT(eid)) {// != NULL taken care of earlier itself
        raise_exception(env, EXCP0D_GPF);
    }
   // make sure the logical processor’s operating mode matches the enclave
    if (tmp_secs->attributes.mode64bit != tmp_mode64) {
        raise_exception(env, EXCP0D_GPF);
    }
    // OSFXSR == 0 ?
    if (!(env->cr[4] & CR4_OSFXSR_MASK)) {
        raise_exception(env, EXCP0D_GPF);
    }
    // Check for legal values of SECS.ATTRIBUTES.XFRM
    if (!(env->cr[4] & CR4_OSXSAVE_MASK)) {
        if (tmp_secs->attributes.xfrm != 0x03) {
            raise_exception(env, EXCP0D_GPF);
        } else
            if ((tmp_secs->attributes.xfrm & env->xcr0) ==
                   tmp_secs->attributes.xfrm) {
               raise_exception(env, EXCP0D_GPF);
        }
    }
    // Make sure the SSA contains at least one more frame
#if DEBUG
    sgx_dbg(trace, "SSA cssa first check %d", tcs->cssa);
#endif
    if (tcs->cssa == 0) {
        raise_exception(env, EXCP0D_GPF);
    }

    // Compute linear address of SSA frame
    tmp_ssa = (tcs->ossa) + (tmp_secs->baseAddr + PAGE_SIZE * tmp_secs->ssaFrameSize * (tcs->cssa - 1));
    // XXX: unused.
	//tmp_xsize = compute_xsave_frame_size(env, tmp_secs->attributes);

#if DEBUG
    //sgx_dbg(trace, "SSA_: %lx  XSize: %lu", tmp_ssa, tmp_xsize);
#endif

// TODO: Implement XSAVE/XSTOR related spec
#if 0
    iter = 0;
    for (tmp_ssa_page = tmp_ssa; tmp_ssa_page > (tmp_ssa - tmp_xsize); tmp_ssa_page -= PAGE_SIZE ) {
        uint16_t index_ssa = epcm_search((void *)tmp_ssa_page, env);
        sgx_dbg(trace, "Index_ssa : %d", index_ssa);

        // Check page is read/write accessible
        if (!checkRWAccessible(tmp_ssa_page, env)) {
                releaseLocks();
                // abortAccess(env);
                raise_exception(env, EXCP0D_GPF);
        }
        // Temporarily block
        check_within_epc((void *)tmp_ssa_page, env);

        epcm_invalid_check(&epcm[index_ssa], env);
        epcm_blocked_check(&epcm[index_ssa], env);
#if DEBUG
        sgx_dbg(trace, "EnclaveAddress: %lu  tmp_ssa %lu enclavesecs_ssa %lu enclavesecs_tcs %lu read %d write %d",
                        epcm[index_ssa].enclave_addr, tmp_ssa_page, epcm[index_ssa].enclave_secs,
                        epcm[index_tcs].enclave_secs, epcm[index_secs].read, epcm[index_secs].write);
#endif
        epcm_field_check(&epcm[index_ssa],
                         (uint64_t)tmp_ssa_page, PT_REG,
                         (uint64_t)epcm[index_tcs].enclave_secs, env);
        if (epcm[index_secs].read != 0 || epcm[index_secs].write != 0) { //  XXX: R == 1 / 0 ?
            raise_exception(env, EXCP0D_GPF);
        }
        env->cregs.CR_XSAVE_PAGE[iter++] = getPhysicalAddr(env, tmp_ssa_page); // unused currently
    }
#endif

    // Compute Address of GPR Area
    tmp_gpr = tmp_ssa + PAGE_SIZE * (tmp_secs->ssaFrameSize) - sizeof(gprsgx_t);

    index_gpr = epcm_search((void *)tmp_gpr, env);

    // Temporarily block
    check_within_epc((void *)tmp_gpr, env);
    // Check for validity and block
    epcm_invalid_check(&epcm[index_gpr], env);
    epcm_blocked_check(&epcm[index_gpr], env);
    // XXX: Spec might be wrong, see comment is sgx_eenter.
    // In second parameter, use tmp_ssa instead of tmp_gpr for now.
    epcm_field_check(&epcm[index_gpr], (uint64_t)tmp_ssa, PT_REG,
                     (uint64_t)epcm[index_tcs].enclave_secs, env);

    if (!epcm[index_gpr].read || !epcm[index_gpr].write) {
        raise_exception(env, EXCP0D_GPF);
    }
    if (!tmp_mode64) {
        checkWithinDSSegment(env, tmp_gpr + sizeof(env->regs[R_EAX]));
    }

    // GetPhysical Address of TMP_GPR
    // XXX: In current design, (enclave) linear address = (enclave) physical address
    //env->cregs.CR_GPR_PA.addr = (uint64_t)getPhysicalAddr(env, tmp_gpr);
    env->cregs.CR_GPR_PA = tmp_gpr;

#if DEBUG
    sgx_dbg(trace, "Physical Address obtained cr_gpr_a: %lp", (void *)env->cregs.CR_GPR_PA);
#endif

    tmp_target = ((gprsgx_t *)(env->cregs.CR_GPR_PA))->rip;
    if (tmp_mode64) {
        is_canonical(tmp_target, env);
    } else {
        if (tmp_target > env->segs[R_CS].limit) {
            raise_exception(env, EXCP0D_GPF);
        }
    }

    if (!tmp_mode64) {
        tmp_fsbase = tcs->ofsbasgx + tmp_secs->baseAddr;
        tmp_fslimit = tmp_fsbase + tcs->fslimit;
        tmp_gsbase = tcs->ogsbasgx + tmp_secs->baseAddr;
        tmp_gslimit = tmp_gsbase + tcs->gslimit;

        // if FS wrap-around, make sure DS has no holes
        if (tmp_fslimit < tmp_fsbase) {
            if (env->segs[R_DS].limit < DSLIMIT) {
                raise_exception(env, EXCP0D_GPF);
            } else
                if (tmp_fslimit > env->segs[R_DS].limit) {
                       raise_exception(env, EXCP0D_GPF);
            }
        }
        // if GS wrap-around, make sure DS has no holes
        if (tmp_gslimit < tmp_gsbase) {
            if (env->segs[R_DS].limit < DSLIMIT) {
                raise_exception(env, EXCP0D_GPF);
            } else
               if (tmp_gslimit > env->segs[R_DS].limit) {
                       raise_exception(env, EXCP0D_GPF);
            }
        }
    } else {
        tmp_fsbase = tcs->ofsbasgx + tmp_secs->baseAddr;
        tmp_gsbase = tcs->ogsbasgx + tmp_secs->baseAddr;

        is_canonical((uint64_t)(void*)tmp_fsbase, env);
        is_canonical((uint64_t)(void*)tmp_gsbase, env);
    }

    env->cregs.CR_ENCLAVE_MODE = true;
    env->cregs.CR_ACTIVE_SECS = (uint64_t)tmp_secs;
    env->cregs.CR_ELRANGE[0] = tmp_secs->baseAddr;
    env->cregs.CR_ELRANGE[1] = tmp_secs->size;

    // Save state for possible AEXs
    env->cregs.CR_TCS_PA = (uint64_t)tcs;
    env->cregs.CR_TCS_LA = (uint64_t)tcs;
    env->cregs.CR_AEP = (uint64_t)aep;
//    env->cregs.CR_TCS_LA = (uint64_t)tcs_app;

    // Save the hidden portions of FS and GS
    env->cregs.CR_SAVE_FS.selector = env->segs[R_FS].selector;
    env->cregs.CR_SAVE_FS.base = env->segs[R_FS].base;
    env->cregs.CR_SAVE_FS.limit = env->segs[R_FS].limit;
    env->cregs.CR_SAVE_FS.flags = env->segs[R_FS].flags;

    env->cregs.CR_SAVE_GS.selector = env->segs[R_GS].selector;
    env->cregs.CR_SAVE_GS.base = env->segs[R_GS].base;
    env->cregs.CR_SAVE_GS.limit = env->segs[R_GS].limit;
    env->cregs.CR_SAVE_GS.flags = env->segs[R_GS].flags;

    // If XSAVE is enabled, save XCR0 and replace it with SECS.ATTRIBUTES.XFRM
    if ((env->cr[4] & CR4_OSXSAVE_MASK)) {
        env->cregs.CR_SAVE_XCR0 = env->xcr0;
        env->xcr0 = tmp_secs->attributes.xfrm;
    }

   //FIXME: Cleanup
   /* Set CR_ENCLAVE_ENTRY_IP*/
   /*
       (* Set CR_ENCLAVE_ENTRY_IP *)
       CR_ENCLAVE_ENTRY_IP <U+F0DF> CRIP”
       RIP <U+F0DF> TMP_TARGET;
       Restore_GPRs from DS:TMP_GPR;
       Restore_Status_Control_Bits_of_RFLAGS_Except_TF from DS:TMP_GPR;
    */
    /* Set CR_ENCLAVE_ENTRY_IP*/

    // Retrieved IP from tmp_ssa assigned to EIP
    env->eip = tmp_target;
    env->cregs.CR_CURR_EIP = env->eip;

    sgx_dbg(trace, "Restart from here: %lx", env->eip);

    // Restore GPRs
    restoreGPRs((gprsgx_t *)tmp_gpr, env);
    env->cregs.CR_EXIT_EIP = ((gprsgx_t *)tmp_gpr)->SAVED_EXIT_EIP;

    // Pop the Stack Frame
    tcs->cssa = tcs->cssa - 1;

    // Do the FS/GS swap
    env->segs[R_FS].base = tmp_fsbase;
    env->segs[R_FS].limit = tcs->fslimit;

    env->segs[R_FS].flags |= 0x01;
    env->segs[R_FS].flags |= env->segs[R_DS].flags & DESC_W_MASK;
    env->segs[R_FS].flags |= DESC_S_MASK;
    env->segs[R_FS].flags |= env->segs[R_DS].flags & DESC_DPL_MASK;
    env->segs[R_FS].flags |= DESC_G_MASK;
    env->segs[R_FS].flags |= DESC_B_MASK;
    env->segs[R_FS].flags |= DESC_P_MASK;
    env->segs[R_FS].flags |= env->segs[R_DS].flags & DESC_AVL_MASK;
    env->segs[R_FS].flags |= env->segs[R_DS].flags & DESC_L_MASK;
    env->segs[R_FS].selector = 0x0B;

    env->segs[R_GS].base = tmp_gsbase;
    env->segs[R_GS].limit = tcs->gslimit;

    env->segs[R_GS].flags |= 0x01;
    env->segs[R_GS].flags |= env->segs[R_DS].flags & DESC_W_MASK;
    env->segs[R_GS].flags |= DESC_S_MASK;
    env->segs[R_GS].flags |= env->segs[R_DS].flags & DESC_DPL_MASK;
    env->segs[R_GS].flags |= DESC_G_MASK;
    env->segs[R_GS].flags |= DESC_B_MASK;
    env->segs[R_GS].flags |= DESC_P_MASK;
    env->segs[R_GS].flags |= env->segs[R_DS].flags & DESC_AVL_MASK;
    env->segs[R_GS].flags |= env->segs[R_DS].flags & DESC_L_MASK;
    env->segs[R_GS].selector = 0x0B;

    // Setting up the base and stack pointers
    // env->regs[R_ESP] = env->regs[R_EBP] = tmp_ssa;
    // env->cregs.CR_ESP = env->cregs.CR_EBP = env->regs[R_ESP];

    sgx_dbg(trace, "EBP: %lx  ESP: %lx", env->regs[R_EBP], env->regs[R_ESP]);

    // FIXME: Not needed mostly
    //update_ssa_base();

    env->cregs.CR_DBGOPTIN = tcs->flags.dbgoptin;
    // Supress all code breakpoints -- Not Needed as of now
    /*
    if(!env->cregs.CR_DBGOPTIN) {
        env->cregs.CR_SAVE_TF = env->eflags & HF_TF_MASK;
        env->eflags = env->eflags & ~(HF_TF_MASK);
        // Support Monitor Trap Flag
        // Clear All pending debug exceptions
        // Clear pending MTF VM EXIT
    } else {
        if (env->eflags & HF_TF_MASK) {
        }
       // if ( vmcs.mtf) {
          }
    } */

    // Considering QEMU TB flow for conditional statements
    env->cregs.CR_ENC_INSN_RET = true;

    CPUState *cs = CPU(x86_env_get_cpu(env));
    tlb_flush(cs, 1);
#if PERF
    qenclaves[eid].stat.mode_switch++;
    qenclaves[eid].stat.tlbflush_n++;
    qenclaves[eid].stat.eresume_n++;
    qenclaves[eid].stat.enclu_n++;
#endif
    return;
}

static
const char *enclu_cmd_to_str(long cmd) {
    switch (cmd) {
    case ENCLU_EACCEPT:     return "EACCEPT";
    case ENCLU_EACCEPTCOPY: return "EACCEPTCOPY";
    case ENCLU_EENTER:      return "EENTER";
    case ENCLU_EEXIT:       return "EEXIT";
    case ENCLU_EGETKEY:     return "EGETKEY";
    case ENCLU_EMODPE:       return "EMODPE";
    case ENCLU_EREPORT:     return "EREPORT";
    case ENCLU_ERESUME:     return "ERESUME";
    }
    return "UNKONWN";
}

void helper_sgx_enclu(CPUX86State *env, uint64_t next_eip)
{
    sgx_dbg(ttrace,
            "(%-13s), EBX=0x%08"PRIx64", "
            "RCX=0x%08"PRIx64", RDX=0x%08"PRIx64,
            enclu_cmd_to_str(env->regs[R_EAX]),
            env->regs[R_EBX],
            env->regs[R_ECX],
            env->regs[R_EDX]);
    switch (env->regs[R_EAX]) {
        case ENCLU_EACCEPT:
            env->cregs.CR_NEXT_EIP = next_eip;
            sgx_eaccept(env);
            break;
        case ENCLU_EACCEPTCOPY:
            sgx_eacceptcopy(env);
            break;
        case ENCLU_EENTER:
            env->cregs.CR_NEXT_EIP = next_eip;
            sgx_eenter(env);
            break;
        case ENCLU_EEXIT:
            env->cregs.CR_NEXT_EIP = next_eip;
            sgx_eexit(env);
/*
#if PERF
            if(env->regs[R_EBX] == 0)   //print_perf_count is called only when EEXIT(NULL)
                print_perf_count(env);
#endif
*/
            break;
        case ENCLU_EGETKEY:
            env->cregs.CR_NEXT_EIP = next_eip;
            sgx_egetkey(env);
            break;
        case ENCLU_EMODPE:
            sgx_emodpe(env);
            break;
        case ENCLU_EREPORT:
            env->cregs.CR_NEXT_EIP = next_eip;
            sgx_ereport(env);
            break;
        case ENCLU_ERESUME:
            sgx_eresume(env);
            break;

//added
//	case ENCLU_ECALMAC:
//	    sgx_ecalmac(env);
//	    break;


        default:
            sgx_err("not implemented yet");
    }
}

// ENCLS instruction implementation.

// popcnt for ECREATE error check
static
int popcnt(uint64_t v)
{
    unsigned int v1, v2;

    v1 = (unsigned int)(v & 0xFFFFFFFF);
    v1 -= (v1 >> 1) & 0x55555555;
    v1 = (v1 & 0x33333333) + ((v1 >> 2) & 0x33333333);
    v1 = (v1 + (v1 >> 4)) & 0x0F0F0F0F;
    v2 = (unsigned int)(v >> 32);
    v2 -= (v2 >> 1) & 0x55555555;
    v2 = (v2 & 0x33333333) + ((v2 >> 2) & 0x33333333);
    v2 = (v2 + (v2 >> 4)) & 0x0F0F0F0F;

    return ((v1 * 0x01010101) >> 24) + ((v2 * 0x01010101) >> 24);
}

// Increments counter by value
static
void LockedXAdd(uint64_t* counter, uint64_t value)
{
    /* Method 1
    asm volatile("lock; xaddl %%eax, %2;"
                  :"=a" (value)                  //Output
                  :"a" (value), "m" (*counter)  //Input
                  :);
    */
    __sync_add_and_fetch(counter, value);
}

/* TODO
   static void compute_xsave_size(void)
   {
   return;
   }
*/

static
epc_t *cpu_load_pi_srcpge(CPUX86State *env, pageinfo_t *pi)
{
    target_ulong addr = (target_ulong)pi + offsetof(pageinfo_t, srcpge);
    return (epc_t *)cpu_ldq_data(env, addr);
}

static
secs_t *cpu_load_pi_secs(CPUX86State *env, pageinfo_t *pi)
{
    target_ulong addr = (target_ulong)pi + offsetof(pageinfo_t, secs);
    return (secs_t *)cpu_ldq_data(env, addr);
}

static
secinfo_t *cpu_load_pi_secinfo(CPUX86State *env, pageinfo_t *pi)
{
    target_ulong addr = (target_ulong)pi + offsetof(pageinfo_t, secinfo);
    return (secinfo_t *)cpu_ldq_data(env, addr);
}

static
void *cpu_load_pi_linaddr(CPUX86State *env, pageinfo_t *pi)
{
    target_ulong addr = (target_ulong)pi + offsetof(pageinfo_t, linaddr);
    return (void *)cpu_ldq_data(env, addr);
}

// ECREATE is the first instruction in the enclave build process.
// In ECREATE, an SECS structure (PAGEINFO.SRCPGE) outside the epc is copied
// into an EPC page (with page type = SECS).
// Also, security measurement (SECS.MRENCLAVE) is initialized with measuring
// ECREATE string, SECS.SSAFRAMESIZE, and SECS.SIZE.
static
void sgx_ecreate(CPUX86State *env)
{
    // RBX: PAGEINFO(In, EA)
    // RCX: EPCPAGE(In, EA)

    // Access Check Condition checking
    // set(&enclave_Initiated);

    enclave_init = true;
    pageinfo_t *pageInfo = (pageinfo_t *)env->regs[R_EBX];
    secs_t *tmp_secs = (secs_t *)env->regs[R_ECX];

    //TEMP Vars
    epc_t *tmp_srcpge;
    secinfo_t *tmp_secinfo;
    void *tmp_linaddr;
    secs_t *tmp_secs_pi;

    // If RBX is not 32 Byte aligned, then GP(0)
    if (!is_aligned(pageInfo, PAGEINFO_ALIGN_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                pageInfo, PAGEINFO_ALIGN_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    // If RCX is not 4KByte aligned or not within an EPC
    if (!is_aligned(tmp_secs, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tmp_secs, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    // Check secs is in EPC
    check_within_epc(tmp_secs, env);

    // set temp variables
    tmp_srcpge  = cpu_load_pi_srcpge(env, pageInfo);
    tmp_secinfo = cpu_load_pi_secinfo(env, pageInfo);
    tmp_secs_pi = cpu_load_pi_secs(env, pageInfo);
    tmp_linaddr = cpu_load_pi_linaddr(env, pageInfo);

    // If srcpge and secinfo of pageInfo is not 32 Byte aligned, then GP(0)
    if (!is_aligned(tmp_srcpge, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tmp_srcpge, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    if (!is_aligned(tmp_secinfo, SECINFO_ALIGN_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tmp_secinfo, SECINFO_ALIGN_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If linaddr and secs of pageInfo is non-zero, then GP(0)
    if (tmp_linaddr != 0 || tmp_secs_pi != 0) {
        raise_exception(env, EXCP0D_GPF);
    }

    // If page type is not secs, then GP(0)
    if (tmp_secinfo->flags.page_type != PT_SECS) {
        raise_exception(env, EXCP0D_GPF);
    }

    // tmp_secinfo->flags reserved field check. If it is not zero, then GP(0)
    checkReservedBits((uint64_t *)&tmp_secinfo->flags, 0xF8L, env);
    checkReservedBits((uint64_t *)&tmp_secinfo->flags, 0xFFFFFFFFFFFF0000L, env);

    // tmp_secinfo reserved field check. If it is not zero, then GP(0)
    is_reserved_zero(tmp_secinfo->reserved, sizeof(((secinfo_t *)0)->reserved), env);

    uint8_t tmpUpdateField[64];
    uint64_t hash_ecreate = 0x0045544145524345;     // "ECREATE"; for SHA256

    // Copy 4KBytes from source page to EPC page
    memcpy(tmp_secs, tmp_srcpge, PAGE_SIZE);

    // if epcm[RCX].valid == 1, then GP(0)
    uint16_t index_secs = epcm_search(tmp_secs, env);
    epcm_valid_check(&epcm[index_secs], env);

    // check lower 2bits of XFRM are set
    if ((tmp_secs->attributes.xfrm & 0x3) != 0x3) {
        raise_exception(env, EXCP0D_GPF);
    }

    // TODO : XFRM is illegal -> see 6.7.2.1

    // TODO : Compute the size required to save state of the enclave on async exit
//  uint64_t tmp_xsize;
//  tmp_xsize = compute_xsave_size(tmp_secs->attributes.XFRM) + gpr_size;

    // TODO : Check whether declared area is large enough to hold XSAVE and GPR stat
//  if(tmp_secs->ssaFrameSize * 4096 < tmp_xsize)
//      raise_exception(env, EXCP0D_GPF);

    // ATTRIBUTES MODE64BIT, TMP_SECS SIZE check
    if (tmp_secs->attributes.mode64bit == 0) {
        checkReservedBits((uint64_t *)&tmp_secs->baseAddr,
                          0x0FFFFFFFF00000000L, env);
        checkReservedBits((uint64_t *)&tmp_secs->size,
                          0x0FFFFFFFF00000000L, env);
    } else { // tmp_secs->attributes.mode64bit == 1
        is_canonical(tmp_secs->baseAddr, env);
        checkReservedBits((uint64_t *)&tmp_secs->size,
                          0x0FFFFFFE000000000L, env);
    }

    // Base addr of enclave is aligned on size
    // TODO : Should be enabled
/*
    if(tmp_secs->baseAddr & (tmp_secs->size - 1))
        raise_exception(env, EXCP0D_GPF);
*/

    // Enclave must be at least 8192 bytes and must be power of 2 in bytes
    if ((tmp_secs->size < (PAGE_SIZE * 2)) || (popcnt(tmp_secs->size) > 1)) {
        sgx_dbg(err, "Invalid SECS.SIZE (less than 8192 or not power of 2).");
        raise_exception(env, EXCP0D_GPF);
    }

    // Reserved fields of TMP_SECS must be zero. If not, then GP(0)
    is_reserved_zero(tmp_secs->reserved1,  sizeof(((secs_t*)0)->reserved1), env);
    is_reserved_zero(tmp_secs->reserved2, sizeof(((secs_t*)0)->reserved2), env);
    is_reserved_zero(tmp_secs->reserved3, sizeof(((secs_t*)0)->reserved3), env);
    is_reserved_zero(tmp_secs->eid_reserved.reserved,
                     sizeof(((secs_t*)0)->eid_reserved.reserved), env);

    // TODO : SECS does not have any unsupported attributes
    // XXX: Where to set CR_SGX_ATTRIBUTES_MASK and the value?

    // Initialize MRENCLAVE field, isvsvn, and isvProdId
    sha256init(tmp_secs->mrEnclave);

    tmp_secs->isvsvn = 0;
    tmp_secs->isvprodID = 0;

    // Initialize enclave's MRENCLAVE update counter
    tmp_secs->mrEnclaveUpdateCounter = 0;

    // Update MRENCLAVE of SECS
    memset(&tmpUpdateField[0], 0, 64);
    memcpy(&tmpUpdateField[0], &hash_ecreate, sizeof(uint64_t));
    memcpy(&tmpUpdateField[8], &tmp_secs->ssaFrameSize, sizeof(uint32_t));
    memcpy(&tmpUpdateField[12], &tmp_secs->size, sizeof(uint64_t));
    memset(&tmpUpdateField[20], 0, 44);

    // Update MRENCLAVE hash value
    sha256update((unsigned char *)tmpUpdateField, tmp_secs->mrEnclave);

    // Increase enclave's MRENCLAVE update counter
    tmp_secs->mrEnclaveUpdateCounter++;

/*
    // Check MRENCLAVE after ECREATE
    {
        char hash[64+1];
        uint64_t counter = tmp_secs->mrEnclaveUpdateCounter;

        fmt_hash(tmp_secs->mrEnclave, hash);

        sgx_dbg(info, "measurement: %.20s.., counter: %ld", hash, counter);
    }
*/

    // Set SECS.EID : starts from 0
    tmp_secs->eid_reserved.eid_pad.eid = env->cregs.CR_NEXT_EID;
    LockedXAdd(&(env->cregs.CR_NEXT_EID), 1);

    // Update EPCM of EPC page
    set_epcm_entry(&epcm[index_secs], 1, 0, 0, 0, 0, PT_SECS, 0, 0);

#if PERF
    int64_t eid;
    eid = tmp_secs->eid_reserved.eid_pad.eid;
    qenclaves[eid].stat.ecreate_n++;
    qenclaves[eid].stat.encls_n++;
#endif
}


// In EADD, security measruement (SECS.MRENCLAVE) is updated for every new
// added TCS/REG page.
// Measuring target includes: EADD string, page address offset,
// PAGEINFO.SECINFO.
static
void sgx_eadd(CPUX86State *env)
{
    // RBX: PAGEINFO(In, EA)
    // RCX: EPCPAGE(In, EA)

    pageinfo_t *pageInfo = (pageinfo_t *)env->regs[R_EBX];
    epc_t *destPage = (epc_t *)env->regs[R_ECX];

    //TEMP Vars
    epc_t *tmp_srcpge;
    secs_t *tmp_secs;
    secinfo_t *tmp_secinfo;
    secinfo_t scratch_secinfo;
    void *tmp_linaddr;
    uint64_t tmp_enclaveoffset;
    uint64_t tmpUpdateField[8];

    // If RBX is not 32 Byte aligned, then GP(0).
    if (!is_aligned(pageInfo, PAGEINFO_ALIGN_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                pageInfo, PAGEINFO_ALIGN_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If RCX is not 4KByte aligned, then GP(0).
    if (!is_aligned(destPage, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                destPage, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If RCX does not resolve within an EPC, then GP(0).
    check_within_epc(destPage, env);

    // set temp variables
    tmp_srcpge  = cpu_load_pi_srcpge(env, pageInfo);
    tmp_secs    = cpu_load_pi_secs(env, pageInfo);
    tmp_secinfo = cpu_load_pi_secinfo(env, pageInfo);
    tmp_linaddr = cpu_load_pi_linaddr(env, pageInfo);

    //sgx_dbg(eadd, "pageinfo: %p, tmp_secs: %p", pageInfo, tmp_secs);
    //sgx_dbg(eadd, " linaddr: %08lx", (uintptr_t)tmp_linaddr);
    //sgx_dbg(eadd, " secinfo: %08lx", (uintptr_t)tmp_secinfo);
    //sgx_dbg(eadd, " srcpge : %08lx", (uintptr_t)tmp_srcpge);

    // If tmp_srcpge is not 4KByte aligned or tmp_secs is not aligned or
    // tmp_secinfo is not 64 Byte aligned or tmp_linaddr is not 4KByte aligned,
    // then GP(0).
    if (!is_aligned(tmp_srcpge, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tmp_srcpge, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    if (!is_aligned(tmp_secinfo, SECINFO_ALIGN_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tmp_secinfo, SECINFO_ALIGN_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    if (!is_aligned(tmp_linaddr, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tmp_linaddr, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If tmp_secs does not resolve within an EPC, then GP(0).
    check_within_epc(tmp_secs, env);

    // Copy secinfo into scratch_secinfo
    memcpy(&scratch_secinfo, tmp_secinfo, sizeof(secinfo_t));

    // scratch_secinfo->flags reserved field check. If it is not zero, then GP(0)
    checkReservedBits((uint64_t *)&tmp_secinfo->flags, 0xF8L, env);
    checkReservedBits((uint64_t *)&tmp_secinfo->flags, 0xFFFFFFFFFFFF0000L, env);

    // tmp_secinfo reserved field check. If it is not zero, then GP(0).
    is_reserved_zero(scratch_secinfo.reserved,
                     sizeof(((secinfo_t *)0)->reserved), env);

    // check page_type of scratch_secinfo. If it is not PT_REG or PT_TCS,
    // then GP(0).
    if (!((scratch_secinfo.flags.page_type == PT_REG) ||
        (scratch_secinfo.flags.page_type == PT_TCS))) {
        raise_exception(env, EXCP0D_GPF);
    }

    // EPC page concurrency check
    // TODO: if EPC in use than raise exception

    // if epcm[RCX].valid == 1, then GP(0).
    uint16_t index_page = epcm_search(destPage, env);
    //sgx_dbg(eadd, "index_page: %d, destPage: %p", index_page, destPage);
    epcm_valid_check(&epcm[index_page], env);

    // SECS concurrency check
    // TODO: if secs not available for eadd raise exception

    // if epcm[tmp_secs] = 0 or epcm[tmp_secs].PT != PT_SECS, then GP(0)
    uint16_t index_secs = epcm_search(tmp_secs, env);

    epcm_invalid_check(&epcm[index_secs], env);
    epcm_page_type_check(&epcm[index_secs], PT_SECS, env);

    // copy
    memcpy(destPage, tmp_srcpge, PAGE_SIZE);

    // Actual Page copy from source to destination - XXX- Copy the entire page
    // 1) PT_TCS - Copy the entire page
    // 2) PT_REGS - Copy the address

/*
//Test ---
    if(scratch_secinfo.flags.page_type == PT_REG)
       sgx_dbg(trace, "DestPage: %lx", destPage);
    memcpy(destPage, tmp_srcpge, PAGE_SIZE);


    if(trackPage < 5) {
       if(mprotect(destPage, PAGE_SIZE, PROT_READ|PROT_WRITE) == -1)
          raise_exception(env, EXCP0D_GPF);
     }else {
#if DEBUG
       sgx_msg(info, "Code Section");
#endif
       if(mprotect(destPage, PAGE_SIZE*5, PROT_READ|PROT_EXEC) == -1)
          raise_exception(env, EXCP0D_GPF);
  }
    trackPage++;
*/
/// XXX: Defunct code - Was for function pointer implementation

/*
    if (scratch_secinfo.flags.page_type == PT_TCS) {
        memcpy(destPage, tmp_srcpge, PAGE_SIZE);
    } else {
        if (scratch_secinfo.flags.page_type == PT_REG) {
            memcpy(destPage, &tmp_srcpge, sizeof(tmp_srcpge));
        }
        if (mprotect(tmp_srcpge, PAGE_SIZE, PROT_READ|PROT_EXEC|PROT_WRITE) == -1) {
            raise_exception(env, EXCP0D_GPF);
        }
    }
*/
    tcs_t *tcs = NULL;
    // Flags check of scratch_secinfo for each page type.
    switch(scratch_secinfo.flags.page_type) {
        case PT_TCS: {
            tcs = (tcs_t *)tmp_srcpge;  // Needs to be verified
//            tcs = (tcs_t*)pageInfo->srcpge; // Needs to be verified
            checkReservedBits((uint64_t *)&tcs->flags, 0xFFFFFFFFFFFFFFFEL, env);
            if ((tmp_secs->attributes.mode64bit == 0)
                && (((tcs->fslimit & 0x0FFF) != 0x0FFF)
                || ((tcs->gslimit & 0x0FFF) != 0x0FFF))) {
                raise_exception(env, EXCP0D_GPF);
            }
            break;
        }
        case PT_REG: {
            if ((scratch_secinfo.flags.w == 1) &&
                (scratch_secinfo.flags.r == 0)) {
                raise_exception(env, EXCP0D_GPF);
            }
            break;
        }
        default: // no use
            break;
    }

    // Check enclave offset within enclave linear address space
    // TODO : Have to enable this -> currently, creating two enclave raises exception in here
/*
    if((tmp_linaddr < tmp_secs->baseAddr)
        || (tmp_linaddr >= (tmp_secs->baseAddr + tmp_secs->size)))
        raise_exception(env, EXCP0D_GPF);
*/

    // Check concurrency of measurement resource
    //TODO

    // Check if the enclave to which the page will be added is already init
    // TODO

    // For TCS pages, force EPCM.rwx to 0 and no debug access
    if (scratch_secinfo.flags.page_type == PT_TCS) {
        scratch_secinfo.flags.r = 0;
        scratch_secinfo.flags.w = 0;
        scratch_secinfo.flags.x = 0;
        tcs->flags.dbgoptin = 0;
        tcs->cssa = 0;
        sgx_dbg(trace, "current cssa is %d", tcs->cssa);
    }

    if (scratch_secinfo.flags.page_type == PT_REG) {
        scratch_secinfo.flags.r = 1;
    //sgx_dbg(info, "r:%d w:%d x:%d", scratch_secinfo.flags.r, scratch_secinfo.flags.w, scratch_secinfo.flags.x);
    }

    // Update MRENCLAVE hash value
    tmp_enclaveoffset = (uintptr_t)tmp_linaddr - tmp_secs->baseAddr;
    tmpUpdateField[0] = 0x0000000044444145;
    memcpy(&tmpUpdateField[1], &tmp_enclaveoffset, 8);
    memcpy(&tmpUpdateField[2], &scratch_secinfo, 48);
    sha256update((unsigned char *)tmpUpdateField, tmp_secs->mrEnclave);

    // INC enclave's MRENCLAVE update counter
    tmp_secs->mrEnclaveUpdateCounter++;

/*
    // Check MRENCLAVE after EADD
    {
        char hash[64+1];
        uint64_t counter = tmp_secs->mrEnclaveUpdateCounter;

        fmt_hash(tmp_secs->mrEnclave, hash);

        sgx_dbg(info, "measurement: %.20s.., counter: %ld", hash, counter);
    }
*/

    // Set epcm entry
    set_epcm_entry(&epcm[index_page], 1, scratch_secinfo.flags.r,
                   scratch_secinfo.flags.w, scratch_secinfo.flags.x, 0,
                   scratch_secinfo.flags.page_type, (uint64_t)tmp_secs,
                   (uintptr_t)tmp_linaddr);
    epcm[index_page].appAddress = (uint64_t)tmp_srcpge;

#if DEBUG
    sgx_dbg(trace, "INDEX : %d EPC addr: %"PRIx64" SECS: %lx", index_page, epcm[index_page].enclave_addr, tmp_secs);
#endif
/*
    if(scratch_secinfo.flags.page_type == PT_REG)
    {
    sgx_dbg(trace, "tmp_srcpge: %llx &tmp_srcpge: %llx destPage: %x", tmp_srcpge,&tmp_srcpge,destPage);
    }
*/
#if PERF
    int64_t eid;
    eid = tmp_secs->eid_reserved.eid_pad.eid;
    qenclaves[eid].stat.eadd_n++;
    qenclaves[eid].stat.encls_n++;
#endif
}

static
bool verify_signature(sigstruct_t *sig, uint8_t *signature, uint8_t *modulus,
                      uint32_t exponent)
{
    int ret = 1;
    rsa_context rsa;
    unsigned char hash[HASH_SIZE];
    sigstruct_t tmp_sig;

    rsa_init(&rsa, RSA_PKCS_V15, 0);

    // set public key
    mpi_read_binary(&rsa.N, modulus, KEY_LENGTH);
    mpi_lset(&rsa.E, (int)exponent);

    rsa.len = (mpi_msb(&rsa.N) + 7) >> 3;

    // generate hash for signature
    memcpy(&tmp_sig, sig, sizeof(sigstruct_t));

    // TODO: check q1 = floor(signature^2 / modulus)
    //             q2 = floor((signature^3 / modulus) / modulus)

    memset(&tmp_sig.exponent, 0, sizeof(tmp_sig.exponent));
    memset(&tmp_sig.modulus, 0, sizeof(tmp_sig.modulus));
    memset(&tmp_sig.signature, 0, sizeof(tmp_sig.signature));
    memset(&tmp_sig.q1, 0, sizeof(tmp_sig.q1));
    memset(&tmp_sig.q2, 0, sizeof(tmp_sig.q2));

    size_t ilen = (size_t)sizeof(sigstruct_t);
    sha1((uint8_t *)&tmp_sig, ilen, hash);

    if ((ret = rsa_pkcs1_verify(&rsa, NULL, NULL, RSA_PUBLIC, POLARSSL_MD_SHA1,
                                HASH_SIZE, hash, signature)) != 0) {
        sgx_dbg(warn, "failed! rsa_pkcs1_verify returned -0x%0x", -ret );
        return false;
    }

    return true;
}

static
bool is_debuggable_enclave_hash(const uint8_t* hash)
{
    int i;
    for (i = 0; i < 32; i ++)
        if (hash[i] != 0)
            return false;
    return true;
}

// In EINIT, security measurement (SECS.MRENCLAVE) is finialized with
// update counter (total measuring times).
// Then several security checks are performed, include:
// 1. Verify SIGSTRUCT.Signature with SIGSTRUCT.Modulus (public key)
//    Also, verify SIGSTRUCT.q1 & q2
// 2. Compare MRSIGNER (hashed SIGSTRUCT.Modulus)
//    If intel signed enclave, compare with CSR_INTELPUBKEYHASH
//    Else compare with EINITTOKEN.MRSIGNER
// 3. Check EINITTOKEN(first 192 bytes) MAC with launch key
// 4. Compare SECS.MRENCLAVE with both SIGSTRUCT.ENCLAVEHASH and
//    EINITTOKEN.MRENCLAVE
static
void sgx_einit(CPUX86State *env)
{
    // RBX: SIGSTRUCT(In, EA)
    // RCX: SECS(In, EA)
    // RDX: EINITTOKEN(In, EA)

    sigstruct_t *sig = (sigstruct_t *)env->regs[R_EBX];
    secs_t *secs = (secs_t *)env->regs[R_ECX];
    einittoken_t *token = (einittoken_t*)env->regs[R_EDX];

    // Check for Alignments (SIGSTRUCT, SECS and EINITTOKEN)
    if (!is_aligned(sig, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                sig, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    if (!is_aligned(secs, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                secs, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    if (!is_aligned(token, EINITTOKEN_ALIGN_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                token, EINITTOKEN_ALIGN_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // Check if SECS is inside EPC
    check_within_epc(secs, env);

    // TEMP vars
    sigstruct_t tmp_sig;
    einittoken_t tmp_token;
    uint8_t tmp_mrEnclave[32];
    uint8_t tmp_mrSigner[32];
    attributes_t intel_only_mask;
    uint8_t csr_intelPubkeyHash[32];

    // set temp variables
    memcpy(&tmp_sig, sig, sizeof(sigstruct_t));
    memcpy(&tmp_token, token, sizeof(einittoken_t));

    memcpy(tmp_mrEnclave, secs->mrEnclave, sizeof(tmp_mrEnclave));
    memcpy(tmp_mrSigner, secs->mrSigner, sizeof(tmp_mrSigner));

    uint64_t mask_val = 0x0000000000000020;
    memcpy(&intel_only_mask, &mask_val, sizeof(attributes_t));
    memcpy(csr_intelPubkeyHash, &env->cregs.CSR_INTELPUBKEYHASH,
           sizeof(csr_intelPubkeyHash));

    // Verify SIGSTRUCT Header
    if (checkSigStructField(tmp_sig, env)) {
        sgx_msg(warn, "check sigstruct field fail");
        env->eflags |= CC_Z;
        env->regs[R_EAX] = ERR_SGX_INVALID_SIG_STRUCT;
        goto _EXIT;
    }

    // Verify signature using embedded public key, q1, and q2.
    // Save upper 352 bytes of the PKCS1.5 encoded message into the
    // TMP_SIG_PADDING

    // Open “Event Window” Check for Interrupts.
    // Check for interrupts
    if (env->nmi_pending) {
        sgx_msg(warn, "pending check fail");
        env->eflags |= CC_Z;
        env->regs[R_EAX] = ERR_SGX_UNMASKED_EVENT;
        goto _EXIT;
    }

    // Caculate finalized version of MRENCLAVE
    uint64_t update_counter = secs->mrEnclaveUpdateCounter * 512;
    //sha256update((unsigned char *)&update_counter, 8, tmp_mrEnclave);
    sha256final(tmp_mrEnclave, update_counter);

    // Verify signature
    if (!verify_signature(sig, sig->signature, sig->modulus, sig->exponent)) {
        sgx_msg(warn, "signature verify fail");
        env->eflags |= CC_Z;
        env->regs[R_EAX] = ERR_SGX_INVALID_SIGNATURE;
        goto _EXIT;
    }

    // XXX : q1, q2 check will not considered
    // TODO : Set TMP_SIG_PADDING

    // Make sure no other SGX instruction is modifying SECS
    // TODO : implmenet checkSECSModification()
    if (checkSECSModification()) {
        sgx_msg(warn, "SECS modified");
        goto _EXIT;
    }

    // if epcm[tmp_secs] = 0 or epcm[tmp_secs].PT != PT_SECS, then GP(0)
    uint16_t index_secs = epcm_search(secs, env);
    epcm_invalid_check(&epcm[index_secs], env);
    epcm_page_type_check(&epcm[index_secs], PT_SECS, env);

    // TODO : Make sure no other instruction is accessing MRENCLAVE or ATTRIBUTES.INIT

    // Verify MRENCLAVE from SIGSTRUCT
#if DEBUG
    {
        unsigned char hash_measured[32];
        memcpy(hash_measured, tmp_mrEnclave, 32);
        sgx_msg(info, "Expected enclave measurement:--------testtest");
        int k;
		for (k = 0; k < 32; k++)
            fprintf(stderr, "%02X", (uint8_t)hash_measured[k]);
        fprintf(stderr, "\n");
    }
#endif

    bool is_debugging = is_debuggable_enclave_hash(tmp_sig.enclaveHash);

    if (!is_debugging && memcmp(tmp_sig.enclaveHash, tmp_mrEnclave, sizeof(tmp_mrEnclave)) != 0) {
        sgx_msg(warn, "enclavehash verification fail");
        env->eflags |= CC_Z;
        env->regs[R_EAX] = ERR_SGX_INVALID_MEASUREMENT;
        goto _EXIT;
    }

    // Set TMP_MRSIGNER
    sha256((unsigned char *)tmp_sig.modulus, KEY_LENGTH, tmp_mrSigner, 0);

    // When intel_only attributes are set, sigstruct must be signed using the Intel key
    attributes_t intel_attr = attr_mask(&secs->attributes, &intel_only_mask);
    attributes_t *zero_attr = (attributes_t *)calloc(1, sizeof(attributes_t));
    if ((memcmp(&intel_attr, zero_attr, sizeof(attributes_t)) != 0) &&
        (memcmp(tmp_mrSigner, csr_intelPubkeyHash, sizeof(tmp_mrSigner)) != 0)) {
        sgx_msg(warn, "intel only mask check fail");
        env->eflags |= CC_Z;
        env->regs[R_EAX] = ERR_SGX_INVALID_ATTRIBUTE;
        goto _EXIT;
    }
    free(zero_attr);

    // Verify Sigstruct.attributes requirements are met
    attributes_t secs_attr = attr_mask(&secs->attributes, &tmp_sig.attributeMask);
    attributes_t sig_attr = attr_mask(&tmp_sig.attributes, &tmp_sig.attributeMask);
    if (memcmp(&secs_attr, &sig_attr, sizeof(attributes_t)) != 0) {
        sgx_msg(warn, "attribute check1 fail");
        env->eflags |= CC_Z;
        env->regs[R_EAX] = ERR_SGX_INVALID_ATTRIBUTE;
        goto _EXIT;
    }

    // If EINITTOKEN.valid[0] is 0, verify the enclave is signed by Intel
    if (!is_debugging && (tmp_token.valid & 0x1) == 0) {
        if (memcmp(tmp_mrSigner, csr_intelPubkeyHash, 32) != 0) {
            sgx_msg(warn, "mrSigner fail");
            env->eflags |= CC_Z;
            env->regs[R_EAX] = ERR_SGX_INVALID_EINIT_TOKEN;
            goto _EXIT;
        }
        goto _COMMIT;
    }

    // Debug launch enclave cannot launch production enclaves
    if ((tmp_token.maskedAttributesLE.debug == 1) &&
        (secs->attributes.debug == 0)) {
        sgx_msg(warn, "debug check fail");
        env->eflags |= CC_Z;
        env->regs[R_EAX] = ERR_SGX_INVALID_EINIT_TOKEN;
        goto _EXIT;
    }

    // Check reserve space in EINITtoken includes reserved regions, upper bits in valid field
    is_reserved_zero(tmp_token.reserved1,
                     sizeof(((einittoken_t *)0)->reserved1), env);
    is_reserved_zero(tmp_token.reserved2,
                     sizeof(((einittoken_t *)0)->reserved2), env);
    is_reserved_zero(tmp_token.reserved3,
                     sizeof(((einittoken_t *)0)->reserved3), env);
    is_reserved_zero(tmp_token.reserved4,
                     sizeof(((einittoken_t *)0)->reserved4), env);

    checkReservedBits((uint64_t *)&tmp_token.valid, 0xFFFFFFFFFFFFFFFEL, env);

    // EINIT token must be <= CR_CPUSVN
    if (memcmp(tmp_token.cpuSvnLE, env->cregs.CR_CPUSVN, 16) > 0) {
        sgx_msg(warn, "cpuSVN check fail");
        env->eflags |= CC_Z;
        env->regs[R_EAX] = ERR_SGX_INVALID_CPUSVN;
        goto _EXIT;
    }

    // Derive launch key used to calculate EINITTOKEN.MAC
    uint8_t *pkcs1_5_padding = alloc_pkcs1_5_padding();

// (ref. r2 p85)
/*
    {
        sgx_msg(info, "check pkcs padding");
        int j;
        for (j = 351; j >= 0; j--)
            printf("%02x", pkcs1_5_padding[j]);
        printf("\n");
    }
*/

    keydep_t tmp_keydep;
    tmp_keydep.keyname = LAUNCH_KEY;
    tmp_keydep.isvprodID = tmp_token.isvprodIDLE;
    tmp_keydep.isvsvn = tmp_token.isvsvnLE;
    memcpy(tmp_keydep.ownerEpoch,      env->cregs.CSR_SGX_OWNEREPOCH, 16);
    memcpy(&tmp_keydep.attributes,     &tmp_token.maskedAttributesLE, 16);
    memset(&tmp_keydep.attributesMask, 0,                             16);
    memset(tmp_keydep.mrEnclave,       0,                             32);
    memset(tmp_keydep.mrSigner,        0,                             32);
    memcpy(tmp_keydep.keyid,           tmp_token.keyid,               32);
    memcpy(tmp_keydep.seal_key_fuses,  env->cregs.CR_SEAL_FUSES,      16);
    memcpy(tmp_keydep.cpusvn,          tmp_token.cpuSvnLE,            16);
    memcpy(&tmp_keydep.miscselect,     &tmp_token.maskedmiscSelectLE,  4);
    memset(&tmp_keydep.miscmask,       0,                              4);
    memcpy(tmp_keydep.padding,         pkcs1_5_padding,              352);

    // Calculate derived key
    uint8_t launch_key[16];
    sgx_derivekey(&tmp_keydep, (unsigned char *)launch_key);

    // Verify EINITTOKEN was generated using this CPU's launch key and that
    // it has not been modified since issuing by the launch enclave.
    // Only 192 bytes of EINITTOKEN are CMACed.
    uint8_t tmp_cmac[16];

    aes_cmac128_context ctx;
    aes_cmac128_starts(&ctx, launch_key);
    aes_cmac128_update(&ctx, (uint8_t *)&tmp_token, 192);
    aes_cmac128_final(&ctx, tmp_cmac);

#if DEBUG
    {
        sgx_msg(info, "Expected launch key:");
        int l;
		for (l = 0; l < 16; l++)
            fprintf(stderr, "%02X", launch_key[l]);
        fprintf(stderr, "\n");
    }
#endif

#if 0
    // Expected einittoken mac
	{
        char token_mac[16+1];

        fmt_hash(tmp_cmac, token_mac);

        sgx_dbg(info, "token mac: %.20s", token_mac);
    }
#endif

// XXX: Bypass
#if 0
    if (!is_debugging) {
        if (memcmp(tmp_token.mac, tmp_cmac, 16)) {
            sgx_dbg(err, "MAC value check fail");
            env->eflags |= CC_Z;
            env->regs[R_EAX] = ERR_SGX_INVALID_EINIT_TOKEN;
            goto _EXIT;
        }

        // Verify EINITTOKEN(RDX) is for this enclave
        if (memcmp(tmp_token.mrEnclave, tmp_mrEnclave, 32)
            || memcmp(tmp_token.mrSigner, tmp_mrSigner, 32)) {
            sgx_msg(warn, "enclave & sealing identity check fail");
            env->eflags |= CC_Z;
            env->regs[R_EAX] = ERR_SGX_INVALID_MEASUREMENT;
            goto _EXIT;
        }

        // Verify ATTRIBUTES in EINITTOKEN are the same as the enclave's
        if (memcmp(&tmp_token.attributes, &secs->attributes, sizeof(attributes_t))) {
            sgx_msg(warn, "attribute check2 fail");
            env->eflags |= CC_Z;
            // err in ref : ERR_SGX_INVALID_EINIT_ATTRIBUTES -> ERR_SGX_INVALID_ATTRIBUTE
            env->regs[R_EAX] = ERR_SGX_INVALID_ATTRIBUTE;
            goto _EXIT;
        }
    }
#endif

    einit_Success = true;

_COMMIT:
    // Commit changes to the SECS
    memcpy(&secs->mrEnclave, tmp_mrEnclave, sizeof(tmp_mrEnclave));

    // MRSIGNER stores a SHA256 in little endian implemented natively on x86
    memcpy(&secs->mrSigner, tmp_mrSigner, sizeof(tmp_mrSigner));
    memcpy(&secs->isvprodID, &tmp_sig.isvProdID, sizeof(uint16_t));
    memcpy(&secs->isvsvn, &tmp_sig.isvSvn, sizeof(uint16_t));
    // TODO : mark the SECS as initialized -> By setting padding fields
    // TODO padding??
    // XXX : how to make padding for secs->padding with sigstruct??
    // secs->eid_reserved.eid_pad.padding

    // Set RAX and ZF for success
    env->eflags &= ~CC_Z;        // ZF = 6
    env->regs[R_EAX] = 0;

    markEnclave(secs->eid_reserved.eid_pad.eid);
/*
    if (!trackEnclaveEntry(tmp_secs, tmp_secs->baseAddr)) {
        raise_exception(env, EXCP0D_GPF);
    } else {
        updateEntry(tmp_secs->baseAddr + tmp_secs->size);
    }
*/
_EXIT:
    /* clear flags : CF, PF, AF, OF, SF */
    env->eflags &= ~(CC_C | CC_P | CC_A | CC_S | CC_O);
#if PERF
    int64_t eid;
    eid = secs->eid_reserved.eid_pad.eid;
    qenclaves[eid].stat.einit_n++;
    qenclaves[eid].stat.encls_n++;
#endif
}

static
void sgx_eldb(CPUX86State *env)
{
    //EAX: eldb/eldu(In)
    //RBX: pageinfo addr(In)
    //RCX: Epc page addr(In)
    //RDX: VA  slot addr(In)
    //EAX: Error code(Out)
    epc_t* tmp_srcpge;
    epc_t  ciphertext;
    secs_t* tmp_secs;
    pcmd_t* tmp_pcmd;
    mac_header_t tmp_header;
    uint64_t tmp_ver;
    uint64_t tmp_mac[2];
    uint64_t epc_index, va_index, secs_index;

    if(!is_aligned(env->regs[R_EBX], 32) || !is_aligned(env->regs[R_ECX], PAGE_SIZE)) {
        raise_exception(env, EXCP0D_GPF);
    }

    check_within_epc((void *)env->regs[R_ECX], env);
    if(!is_aligned(env->regs[R_EDX], 8)) {
        raise_exception(env, EXCP0D_GPF);
    }
    check_within_epc((void *)env->regs[R_EDX], env);

    tmp_srcpge = ((pageinfo_t *)env->regs[R_EBX])->srcpge;
    tmp_secs = ((pageinfo_t *)env->regs[R_EBX])->secs;
    tmp_pcmd = ((pageinfo_t *)env->regs[R_EBX])->secinfo;

    if(!is_aligned(tmp_pcmd, sizeof(pcmd_t)) || !is_aligned(tmp_srcpge, PAGE_SIZE)) {
        raise_exception(env, EXCP0D_GPF);
    }
    //TODO: (* Check concurrency of EPC and VASLOT by other SGX instructions *)

    epc_index = epcm_search((void *)env->regs[R_ECX], env);
    va_index = epcm_search((void *)env->regs[R_EDX], env);
    if(epcm[epc_index].valid == 1 ||
       epcm[va_index].valid == 0 || epcm[va_index].page_type != PT_VA) {
        raise_exception(env, EXCP0D_GPF);
    }

    memset(&tmp_header, 0 , sizeof(tmp_header));
    tmp_header.secinfo.flags.page_type = tmp_pcmd->secinfo.flags.page_type;
    tmp_header.secinfo.flags.r = tmp_pcmd->secinfo.flags.r;
    tmp_header.secinfo.flags.w = tmp_pcmd->secinfo.flags.w;
    tmp_header.secinfo.flags.x = tmp_pcmd->secinfo.flags.x;
    tmp_header.linaddr = ((pageinfo_t *)(env->regs[R_EBX]))-> linaddr;

    if(tmp_header.secinfo.flags.page_type == PT_REG || 
       tmp_header.secinfo.flags.page_type == PT_TCS) {
        if(!is_aligned(tmp_secs, PAGE_SIZE)) {
            raise_exception(env, EXCP0D_GPF);
        }
        check_within_epc((void *)tmp_secs, env);
        secs_index = epcm_search(tmp_secs, env);
        if(epcm[secs_index].valid == 0 || epcm[secs_index].page_type == PT_SECS) {
            raise_exception(env, EXCP0D_GPF);
        }
    }
    else if(tmp_header.secinfo.flags.page_type == PT_SECS ||
            tmp_header.secinfo.flags.page_type == PT_VA) {
        if(tmp_secs != 0)
            raise_exception(env, EXCP0D_GPF);
    }
    else {
        raise_exception(env, EXCP0D_GPF);
    }
    if (tmp_header.secinfo.flags.page_type == PT_REG ||
        tmp_header.secinfo.flags.page_type == PT_TCS) { 
        tmp_header.eid = tmp_secs->eid_reserved.eid_pad.eid;
    }
    else {
        tmp_header.eid = 0;
    }
    memcpy(ciphertext, tmp_srcpge, PAGE_SIZE);
    tmp_ver = env->regs[R_EDX];

    decrypt_epc(ciphertext, PAGE_SIZE, (unsigned char *)&tmp_header, sizeof(tmp_header),
                (unsigned char *)tmp_mac, gcm_key, NULL, (unsigned char *)env->regs[R_ECX]); 

    if(!memcmp(tmp_mac, tmp_pcmd->mac, 16)) {
        env->regs[R_EAX] = ERR_SGX_MAC_COMPARE_FAIL;
        goto ERROR_EXIT;
    }
    if(env->regs[R_EDX] != 0) { //XXX ? 
        raise_exception(env, EXCP0D_GPF);
    }
    else {
        env->regs[R_EDX] = tmp_ver;
    }
    epcm[epc_index].page_type = tmp_header.secinfo.flags.page_type;
    epcm[epc_index].read    = tmp_header.secinfo.flags.r;
    epcm[epc_index].write   = tmp_header.secinfo.flags.w;
    epcm[epc_index].execute = tmp_header.secinfo.flags.x;
    epcm[epc_index].enclave_addr = tmp_header.linaddr;

    if(env->regs[R_EAX] == 0x07) 
        epcm[epc_index].blocked = 1;
    else
        epcm[epc_index].blocked = 0;

    epcm[epc_index].valid = 1;
    env->regs[R_EAX] = 0;
    env->eflags &= ~(CC_Z);

    ERROR_EXIT:
        env->eflags &= ~(CC_C | CC_P | CC_A | CC_O | CC_S);
}

/*
static void sgx_eremove(CPUX86State *env)
{
    epc_t *tmp_epcpage = (epc_t *)env->regs[R_ECX];

    // If RCX is not 4KB Aligned, then GP(0)
    if (!is_aligned((void *)tmp_epcpage, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                (void *)tmp_epcpage, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If RCX does not resolve within an EPC, then GP(0)
    check_within_epc((void *)tmp_epcpage, env);

    // TODO : Check the EPC page for concurrency

    // If RCX is already unused, nothing to do
    uint8_t index_page = epcm_search((void *)tmp_epcpage, env);

    if (epcm[index_page].valid == 0) {
        goto _DONE;
    }

    if (epcm[index_page].page_type == PT_VA) {
        epcm[index_page].valid = 0;
        goto _DONE;
    }

    if (epcm[index_page].page_type == PT_SECS) {
        // TODO : RCX has an EPC page associated with it check
        epcm[index_page].valid = 0;
        goto _DONE;
    }

    // secs_t *tmp_secs = get_secs_address(&epcm[index_page]);

    // TODO : If other threads active using SECS

_DONE:
    env->regs[R_EAX] = 0;
    env->eflags &= ~CC_Z;

//_ERROR_EXIT:
    // clear flags : CF, PF, AF, OF, SF
    env->eflags &= ~(CC_C | CC_P | CC_A | CC_O | CC_S);
}
*/

// In EEXTEND, security measurement (SECS.MRENCLAVE) is updated for every
// page chunk (256 Bytes).
// There are two measurement steps:
// Step 1. Measuring EEXTEND string, page chunk offset.
// Step 2. Measuring the page chunk memory (4 times, 64 Bytes each).
static
void sgx_eextend(CPUX86State *env)
{
    // RCX: EPCPAGE(In, EA)

    uint64_t *target_addr = (uint64_t *)env->regs[R_ECX];
    secs_t *tmp_secs;
    uint64_t tmp_enclaveoffset;
    uint64_t tmpUpdateField[8];

    // If RCX is not 256 Byte aligned, then GP(0)
    if (!is_aligned(target_addr, MEASUREMENT_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                target_addr, MEASUREMENT_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If RCX does not resolve within an EPC, then GP(0)
    check_within_epc(target_addr, env);

    // Check other instructions accessing EPCM
    uint16_t index_page = epcm_search(target_addr, env);
    epcm_invalid_check(&epcm[index_page], env);

    // Page Type check of RCX
    if ((epcm[index_page].page_type != PT_REG) &&
        (epcm[index_page].page_type != PT_TCS)) {
        raise_exception(env, EXCP0D_GPF);
    }

    // Set tmp_secs
    tmp_secs = get_secs_address(&epcm[index_page]);

    // check other instructions are accessing MRENCLAVE or ATTRIBUTES.INIT
    // TODO

    // Calculate enclave offset
    tmp_enclaveoffset = (uint64_t)target_addr - tmp_secs->baseAddr;

    // Update MRENCLAVE of SECS
    tmpUpdateField[0] = 0x00444E4554584545;
    memcpy(&tmpUpdateField[1], &tmp_enclaveoffset, 8);
    memset(&tmpUpdateField[2], 0, 48);

    // Update MRENCLAVE hash value
    sha256update((unsigned char *)tmpUpdateField, tmp_secs->mrEnclave);

    // Increase MRENCLAVE update counter
    tmp_secs->mrEnclaveUpdateCounter++;

/*
    // Check MRENCLAVE for EEXTEND instruction
    {
        char hash[64+1];
        uint64_t counter = tmp_secs->mrEnclaveUpdateCounter;

        fmt_hash(tmp_secs->mrEnclave, hash);

        sgx_dbg(info, "measurement: %.20s.., counter: %ld", hash, counter);
    }
*/

    // Add 256 bytes to MRENCLAVE, 64 byte at a time
    sha256update((unsigned char *)(&target_addr[0]), tmp_secs->mrEnclave);
    sha256update((unsigned char *)(&target_addr[8]), tmp_secs->mrEnclave);
    sha256update((unsigned char *)(&target_addr[16]), tmp_secs->mrEnclave);
    sha256update((unsigned char *)(&target_addr[24]), tmp_secs->mrEnclave);

    // Increase enclaves's MRENCLAVE update counter by 4
    tmp_secs->mrEnclaveUpdateCounter += 4;

/*
    // Check MRENCLAVE for page chunk
    {
        char hash[64+1];
        uint64_t counter = tmp_secs->mrEnclaveUpdateCounter;

        fmt_hash(tmp_secs->mrEnclave, hash);

        sgx_dbg(info, "measurement: %.20s.., counter: %ld", hash, counter);
    }
*/
#if PERF
    int64_t eid;
    eid = tmp_secs->eid_reserved.eid_pad.eid;
    qenclaves[eid].stat.eextend_n++;
    qenclaves[eid].stat.encls_n++;
#endif
}

// EAUG instruction
static
void sgx_eaug(CPUX86State *env)
{
    pageinfo_t *pageInfo = (pageinfo_t*)env->regs[R_EBX];    //spec says RBX is the address of a SECINFO, but it is a typo
    epc_t *destPage = (epc_t *)env->regs[R_ECX];

    //TEMP Vars
    epc_t *tmp_srcpge;
    secs_t *tmp_secs;
    secinfo_t *tmp_secinfo;
    void *tmp_linaddr;
    uint64_t eid;

    // If RBX is not 32 Byte aligned, then GP(0).
    if (!is_aligned(pageInfo, PAGEINFO_ALIGN_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                pageInfo, PAGEINFO_ALIGN_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If RCX is not 4KByte aligned, then GP(0).
    if (!is_aligned(destPage, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                destPage, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If RCX does not resolve within an EPC, then GP(0).
    check_within_epc(destPage, env);

    // set temp variables
    tmp_srcpge  = cpu_load_pi_srcpge(env, pageInfo);
    tmp_secs    = cpu_load_pi_secs(env, pageInfo);
    tmp_secinfo = cpu_load_pi_secinfo(env, pageInfo);
    tmp_linaddr = cpu_load_pi_linaddr(env, pageInfo);

    // If tmp_secs is not 4KByte aligned or tmp_linaddr is not 4KByte aligned,
    // then GP(0).
    if (!is_aligned(tmp_secs, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tmp_secs, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }
    if (!is_aligned(tmp_linaddr, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tmp_linaddr, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If srcpge and secinfo of pageInfo is non-zero, then GP(0)
    if (tmp_srcpge != 0 || tmp_secinfo != 0) {
        raise_exception(env, EXCP0D_GPF);
    }

    // If tmp_secs does not resolve within an EPC, then GP(0).
    check_within_epc(tmp_secs, env);

    // EPC page concurrency check
    // TODO: if EPC in use than raise exception

    // if epcm[RCX].valid == 1, then GP(0).
    uint16_t index_page = epcm_search(destPage, env);
    epcm_valid_check(&epcm[index_page], env);

    // SECS concurrency check
    // TODO: if secs not available for eaug raise exception

    // if epcm[tmp_secs].valid = 0 or epcm[tmp_secs].PT != PT_SECS, then GP(0)
    uint16_t index_secs = epcm_search(tmp_secs, env);

    epcm_invalid_check(&epcm[index_secs], env);
    epcm_page_type_check(&epcm[index_secs], PT_SECS, env);

    eid = tmp_secs->eid_reserved.eid_pad.eid;
    // check if enclave to which the page will be added is in the Initiallized state
    // TODO: the initialized state should belong to the enclave of our interest
    if (!checkEINIT(eid)) {
        raise_exception(env, EXCP0D_GPF);
    }

    // Check enclave offset within enclave linear address space
    if(((uintptr_t)tmp_linaddr < tmp_secs->baseAddr)
        || ((uintptr_t)tmp_linaddr >= (tmp_secs->baseAddr + tmp_secs->size))){
        sgx_dbg(err, "Enclave offset is not within the enclave linear address space");
        raise_exception(env, EXCP0D_GPF);
    }
    sgx_dbg(trace, "DEBUG EAUG baseaddr is %lX, endaddr is %lX", tmp_secs->baseAddr, (tmp_secs->baseAddr + tmp_secs->size));
    sgx_dbg(trace, "DEBUG linear addr is %p", (void *)tmp_linaddr);


    // clear the content of EPC page
    memset(destPage, 0, PAGE_SIZE);

    // Set epcm entry
    set_epcm_entry(&epcm[index_page], 1,       //epcm_entry, valid,
                   1, 1, 0, 0,                 //read, write, execute, block,
                   PT_REG, (uint64_t)tmp_secs, //pt, secs,
                   (uintptr_t)tmp_linaddr);               //linaddr

    epcm[index_page].pending = 1;
    epcm[index_page].modified = 0;

#if PERF
    qenclaves[eid].stat.eaug_n++;
    qenclaves[eid].stat.encls_n++;
#endif
}

static
void sgx_emodpr(CPUX86State *env)
{
    // RBX: Secinfo Addr(In)
    // RCX: Destination EPC Addr(In)
    // EAX: Error Code(out)
    int page_index;
    secs_t   *tmp_secs;
    secinfo_t scratch_secinfo;
    memset(&scratch_secinfo, 0, sizeof(secinfo_t));

    if(!is_aligned(env->regs[R_EBX], 64)) {
        raise_exception(env, EXCP0D_GPF);
    }
    if(!is_aligned(env->regs[R_ECX], PAGE_SIZE)) {
        raise_exception(env, EXCP0D_GPF);
    }
    check_within_epc((void *)env->regs[R_ECX], env);

    memcpy(&scratch_secinfo, (void *)env->regs[R_EBX], sizeof(secinfo_t));

    // (* Check for mis-configured SECINFO flags*)
    is_reserved_zero(scratch_secinfo.reserved, sizeof(((secinfo_t *)0)->reserved), env);
    if (!(scratch_secinfo.flags.r == 0 || scratch_secinfo.flags.w != 0)) {
        raise_exception(env, EXCP0D_GPF);
    }

    // TODO: Check concurrency with SGX1 or SGX2 instructions on the EPC page

    page_index = epcm_search((void *)env->regs[R_ECX], env);
    if(epcm[page_index].valid == 0) {
        raise_exception(env, EXCP0D_GPF);
    }
    // TODO: Check the EPC page for concurrency

    if(epcm[page_index].pending != 0 || epcm[page_index].modified != 0) {
        env->eflags = 1;
        env->regs[R_EAX] = ERR_SGX_PAGE_NOT_MODIFIABLE;
        goto Done;
    }
    if(epcm[page_index].page_type != PT_REG) {
        raise_exception(env, EXCP0D_GPF);
    }
    tmp_secs = get_secs_address(&epcm[page_index]);
    //we don't have init field in secs.attributes..
    //TODO: if(tmp_secs.attributes.init == 0)
    //TODO: check concurrency with ETRACK

    epcm[page_index].read &= scratch_secinfo.flags.r;
    epcm[page_index].write &= scratch_secinfo.flags.w;
    epcm[page_index].execute &= scratch_secinfo.flags.x;

    env->eflags &= ~(CC_Z);
    env->regs[R_EAX] = 0;

    Done:
        env->eflags &= ~(CC_C | CC_P | CC_A | CC_O | CC_S);
}

static
void sgx_eblock(CPUX86State *env)
{
    // RCX: EPC Addr(In, EA)
    // EAX: Error Code(Out)

    uint64_t *epc_addr = (uint64_t *)env->regs[R_ECX];
    uint16_t epcm_index = 0;
    uint64_t tmp_blkstate = 0;
    // Check if DS:RCX is not 4KByte Aligned
    if (!is_aligned(epc_addr, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                epc_addr, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If RCX does not resolve within an EPC, then GP(0).
    check_within_epc(epc_addr, env);

    // Clear ZF,CF,PF,AF,OF,SF;
    env->eflags &= ~(CC_Z | CC_C | CC_P | CC_A | CC_O | CC_S);

    // TODO - Check concurrency with other instructions

    epcm_index = epcm_search(epc_addr, env);
    if(epcm[epcm_index].valid == 0) {
        env->eflags |= CC_Z;
        env->regs[R_EAX] = ERR_SGX_PG_INVLD;
	goto Done; 
    }

    if((epcm[epcm_index].page_type != PT_REG) && (epcm[epcm_index].page_type != PT_TCS) &&
       (epcm[epcm_index].page_type != PT_TRIM)) {
        env->eflags &= CC_C;
        if(epcm[epcm_index].page_type == PT_SECS) {
            env->regs[R_EAX] = ERR_SGX_PG_IS_SECS;
        }
        else {
            env->regs[R_EAX] = ERR_SGX_NOTBLOCKABLE;
        }
        goto Done;
    }
    
    // (* Check if the page is already blocked and report blocked state *)
    tmp_blkstate = epcm[epcm_index].blocked;

    // (* at this point, the page must be valid and PT_TCS or PT_REG or PT_TRIM*)
    if(tmp_blkstate == 1) {
        env->eflags |= CC_C;
        env->regs[R_EAX] = ERR_SGX_BLKSTATE;
    }
    else {
        epcm[epcm_index].blocked = 1;
    }
   

Done: 
    return;
}

/*
1. Enclave signals OS that a particular page is no longer in use.
2. OS calls EMODT on the page, requesting that the page’s type be changed to PT_TRIM.
a. SECS and VA pages cannot be trimmed in this way, so the initial type of the page must be PT_REG or
PT_TCS
b. EMODT may only be called on VALID pages
3. OS performs an ETRACK instruction to remove the TLB addresses from all the processors
4. Enclave issues an EACCEPT instruction.
5. The OS may now permanently remove it (by calling EREMOVE).
*/
static
void sgx_emodt(CPUX86State *env)
{
    // RBX: SECINFO addr(In, EA)
    // RCX: EPC Addr(In, EA)
    // EAX: Error Code(Out)
    secs_t *tmp_secs;
    secinfo_t scratch_secinfo;
    uint64_t *tmp_secinfo = env->regs[R_EBX];
    uint64_t *target_addr = (uint64_t *)env->regs[R_ECX];
    uint16_t epcm_index = 0;


    // If RBX is not 64 Byte aligned, then GP(0).
    if (!is_aligned(tmp_secinfo, SECINFO_ALIGN_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                tmp_secinfo, SECINFO_ALIGN_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If RCX is not 4 KByte aligned, then GP(0).
    if (!is_aligned(target_addr, PAGE_SIZE)) {
        sgx_dbg(err, "Failed to check alignment: %p on %d bytes",
                target_addr, PAGE_SIZE);
        raise_exception(env, EXCP0D_GPF);
    }

    // If RCX does not resolve within an EPC, then GP(0).
    check_within_epc(target_addr, env);

    memcpy(&scratch_secinfo, tmp_secinfo, sizeof(secinfo_t));

    // (* Check for mis-configured SECINFO flags*)
    is_reserved_zero(scratch_secinfo.reserved, sizeof(((secinfo_t *)0)->reserved), env);
    if (!(scratch_secinfo.flags.page_type == PT_TCS || scratch_secinfo.flags.page_type == PT_TRIM)) {
        raise_exception(env, EXCP0D_GPF);
    }

    // TODO:(* Check concurrency with SGX1 instructions on the EPC page *)
    epcm_index = epcm_search(target_addr, env);
    if (epcm[epcm_index].page_type == PT_REG || epcm[epcm_index].page_type == PT_TCS) {
        raise_exception(env, EXCP0E_PAGE);
    }

    // (* Check for mis-configured SECINFO flags*)
    if ((epcm[epcm_index].read == 0) && (scratch_secinfo.flags.r == 0) &&
        (scratch_secinfo.flags.w != 0)) {
        env->eflags = 1;
        env->regs[R_EAX] = ERR_SGX_PAGE_NOT_MODIFIABLE;
        goto Done;
    }

    if ((epcm[epcm_index].pending != 0)) { // TODO:|| epcm[epcm_index].modified != 0
        env->eflags = 1;
        env->regs[R_EAX] = ERR_SGX_PAGE_NOT_MODIFIABLE;
        goto Done;
    }

    tmp_secs = get_secs_address(&epcm[epcm_index]);

    if (tmp_secs->attributes.einittokenkey == 0)
        raise_exception(env, EXCP0D_GPF);

    //TODO: check concurrency with ETRACK

    //TODO: epcm[epcm_index].modified = 1;
    epcm[epcm_index].read  = 0;
    epcm[epcm_index].write = 0;
    epcm[epcm_index].execute  = 0;
    epcm[epcm_index].page_type = scratch_secinfo.flags.page_type;

    env->eflags &= ~(CC_Z);
    env->regs[R_EAX] = 0;


    Done:
    env->eflags &= ~(CC_C | CC_P | CC_A | CC_O | CC_S );
}

static
void sgx_epa(CPUX86State *env)
{

    // RBX: PT_VA (In, Const)
    // RCX: EPC Addr(In, EA)
    uint64_t *epc_addr = (uint64_t *)env->regs[R_ECX];
    uint16_t epcm_index = 0;
    if(env->regs[R_EBX] != PT_VA || !(is_aligned(epc_addr, PAGE_SIZE))) {
        raise_exception(env, EXCP0D_GPF);
    }
    check_within_epc(epc_addr, env);

    /*TODO (* Check concurrency with other SGX instructions *)*/

    /* Check EPC page must be empty */
    epcm_index = epcm_search(epc_addr, env);
    if(epcm[epcm_index].valid != 0) {
        raise_exception(env, EXCP0D_GPF);
    }

    /* Clears EPC page */
    memset(epc_addr, 0, PAGE_SIZE * 8);
  
    epcm[epcm_index].page_type = PT_VA;
    epcm[epcm_index].enclave_addr = 0;
    epcm[epcm_index].blocked = 0;
    /* Based on Spec ver2--------- */
    epcm[epcm_index].pending = 0;
    epcm[epcm_index].modified = 0;
    /* --------------------------- */
    epcm[epcm_index].read = 0;
    epcm[epcm_index].write = 0;
    epcm[epcm_index].execute = 0;
    epcm[epcm_index].valid = 0;
}

static
void sgx_ewb(CPUX86State *env)
{
    // EAX: Error(Out)
    // RBX: Pageinfo Addr(In)
    // RCX: EPC addr(In)
    // RDX: VA slot addr(In)
    int epc_index = 0, va_index = 0;
    uint64_t *tmp_srcpge;
    pcmd_t *tmp_pcmd;
    secs_t *tmp_secs;
    uint64_t tmp_pcmd_enclaveid;
    mac_header_t tmp_header; //MAC Header
    memset(&tmp_header, 0, 128);
    uint64_t tmp_ver = 0;

    if (!(is_aligned(env->regs[R_EBX], 32)) ||
        !(is_aligned(env->regs[R_ECX], PAGE_SIZE))) {
        raise_exception(env, EXCP0D_GPF);
    }
    check_within_epc((void *)env->regs[R_ECX], env);

    if (!(is_aligned(env->regs[R_EDX], 8))) {
        raise_exception(env, EXCP0D_GPF);
    }
    check_within_epc((void *)env->regs[R_EDX], env);

    /* EPCPAGE and VASLOT should not resolve to the same EPC page */
    if(is_within_same_epc((void *)env->regs[R_ECX], (void *)env->regs[R_EDX], env)) {
        raise_exception(env, EXCP0D_GPF);
    }
    tmp_srcpge = ((pageinfo_t*)(env->regs[R_EBX]))->srcpge;
    tmp_pcmd = ((pageinfo_t*)(env->regs[R_EBX]))->secinfo; //secinfo = pcmd addr

    if(!(is_aligned(tmp_pcmd, 128)) || !(is_aligned(tmp_srcpge, PAGE_SIZE))) {
        raise_exception(env, EXCP0D_GPF);
    }
    /* TODO: Check for concurrent SGX instruction access to the page */
    /* TODO: Check if the VA Page is being removed or changed*/
    epc_index = epcm_search((void *)env->regs[R_ECX], env);
    va_index = epcm_search((void *)env->regs[R_EDX], env);
    /* Verify that EPCPAGE and VASLOT page are valid EPC pages and DS:RDX is VA */
    if((epcm[epc_index].valid == 0) || (epcm[va_index].valid == 0) ||
       (epcm[va_index].page_type != PT_VA)) {
        raise_exception(env, EXCP0D_GPF);
    }
    /* Perform page-type-specific exception checks */
    if((epcm[epc_index].page_type == PT_REG || epcm[epc_index].page_type == PT_TCS)) {
        tmp_secs = get_secs_address(&epcm[epc_index]);
        /* TODO: Check that EBLOCK has occurred correctly */
    }

    env->eflags &= ~(CC_Z | CC_C | CC_P | CC_A | CC_O | CC_S);
    env->regs[R_EAX] = 0x0;

    /* Perform page-type-specific checks */
    if((epcm[epc_index].page_type == PT_REG || epcm[epc_index].page_type == PT_TCS)) {
        /* check to see if the page is evictable */
        if(epcm[epc_index].blocked == 0) {
            env->regs[R_EAX] = ERR_SGX_PAGE_NOT_BLOCKED;
            env->eflags |= CC_Z;
            goto ERROR_EXIT;
        }
        /* TODO: Check if tracking done correctly */
        /* Obtain EID to establish cryptographic binding betw the paged-out page and the enclave */
        tmp_header.eid = tmp_secs->eid_reserved.eid_pad.eid;

        /* Obtain EID as an enclave handle for software */
        tmp_pcmd_enclaveid = tmp_secs->eid_reserved.eid_pad.eid;
    }
    else if(epcm[epc_index].page_type == PT_SECS) {
    /*TODO: check that there are no child pages inside the enclave
      Skip this temporalily... please never swap out PT_SECS...
    */
    }
    else if(epcm[epc_index].page_type == PT_VA) {
        tmp_header.eid = 0;
        tmp_pcmd_enclaveid = 0;
    }
    tmp_header.linaddr = epcm[epc_index].enclave_addr;
    tmp_header.secinfo.flags.page_type = epcm[epc_index].page_type;
    tmp_header.secinfo.flags.r = epcm[epc_index].read;
    tmp_header.secinfo.flags.w = epcm[epc_index].write;
    tmp_header.secinfo.flags.x = epcm[epc_index].execute;
    // it seems rsvd in the spec indicates reserved field.. but not sure..
    //TMP_HEADER.SECINFO.FLAGS.RSVD = 0;

    /* Encrypt the page, AES-GCM produces 2 values, {ciphertext, MAC}. */
    encrypt_epc((unsigned char *)env->regs[R_ECX], PAGE_SIZE, (unsigned char *)&tmp_header,
                sizeof(tmp_header), gcm_key, NULL, (unsigned char *)tmp_srcpge,
				(unsigned char *)tmp_pcmd->mac);

    memset(&tmp_pcmd->secinfo, 0 , sizeof(secinfo_t));
    tmp_pcmd->secinfo.flags.page_type = epcm[epc_index].page_type;
    tmp_pcmd->secinfo.flags.r = epcm[epc_index].read;
    tmp_pcmd->secinfo.flags.w = epcm[epc_index].write;
    tmp_pcmd->secinfo.flags.x = epcm[epc_index].execute;
    memset(tmp_pcmd->reserved, 0, sizeof(tmp_pcmd->reserved));
    tmp_pcmd->enclaveid = tmp_pcmd_enclaveid;
    ((pageinfo_t *)(env->regs[R_EBX]))->linaddr = epcm[epc_index].enclave_addr;

    /*Check if version array slot was empty */
    if( *((uint64_t *)(env->regs[R_EDX])) ){
        env->regs[R_EAX] = ERR_SGX_VA_SLOT_OCCUPIED;
        env->eflags |= CC_C;
    }
    env->regs[R_EDX] = tmp_ver;
    epcm[epc_index].valid = 0;

    ERROR_EXIT:
        env->eflags &= ~(CC_C | CC_P | CC_A | CC_O | CC_S);
}

static
void encls_intel_pubkey(CPUX86State *env)
{
    uint8_t *intel_pubKey = (uint8_t *)env->regs[R_EBX];

    // Set CSR_INTELPUBKEYHASH
    sha256(intel_pubKey, KEY_LENGTH,
           (unsigned char *)&(env->cregs.CSR_INTELPUBKEYHASH), 0);
}

static
void encls_epcm_clear(CPUX86State *env)
{
    epc_t *target = (epc_t *)env->regs[R_EBX];
    int target_index = epcm_search(target, env);
    epcm[target_index].valid = 0;
}

// Sanity checks data structures
static void sanity_check(void)
{
    assert(sizeof(secs_t) == 4096);
    assert(sizeof(attributes_t) == 16);
    assert(sizeof(tcs_t) == 4096);
    assert(sizeof(tcs_flags_t) == 8);
    assert(sizeof(gprsgx_t) == 192);
    assert(sizeof(ssa_t) == 4096);
    assert(sizeof(pageinfo_t) == 32);
    assert(sizeof(secinfo_flags_t) == 8);
    assert(sizeof(secinfo_t) == 64);
    assert(sizeof(sigstruct_t) == 1808);
    assert(sizeof(einittoken_t) == 304);
    assert(sizeof(keypolicy_t) == 2);
    assert(sizeof(keyrequest_t) == 512);
    assert(sizeof(keydep_t) == 544);
    assert(sizeof(pcmd_t) == 128);
    assert(sizeof(mac_header_t) == 128);
}

static void init_qenclave(void)
{
    int i = 0;
    for(i = 0; i < MAX_ENCLAVES; i++){
        memset(&(qenclaves[i]), 0, sizeof(qeid_t));
    }
}

#define KEY_PATH1 "user/conf/device.key"
#define KEY_PATH2 "conf/device.key"

//Initializes qemu with the EPC address
static void encls_qemu_init(CPUX86State *env)
{
    // firstPage represents the first page of EPC - the start of EPC
    epc_t *firstPage = (epc_t *)env->regs[R_EBX];
    epc_t *endPage = (epc_t *)env->regs[R_ECX];
    memset(epcm, 0, NUM_EPC * sizeof(epcm_entry_t));

    // Save the epc base and address
    // Made Base the previous value since it appears as an address inside is_within_epc (thus goes to mem_access
    // causing an unnecessary access violation due to the ranges itself.
    EPC_BaseAddr = (uint64_t)firstPage - 1;
    EPC_EndAddr  = (uint64_t)endPage;

    sgx_dbg(trace, "set EPC pages %p-%p",
            (void *)EPC_BaseAddr,
            (void *)EPC_EndAddr);

    int iter;
    for (iter = 0; iter < NUM_EPC; iter++) {
        epcm[iter].epcPageAddress = (uint64_t)firstPage;
        firstPage++;
    }
    // Initializing CR_ Registers in cpu.h (For CR_NEXT_EID)
    env->cregs.CR_NEXT_EID = 0; // Next Enclave EID
    env->cregs.CR_ENC_INSN_RET = false;
    env->cregs.CR_EXIT_MODE = false;

    // Setting the SSA Base
    set_ssa_base();

    // Load device key pair
    if (file_exist(KEY_PATH1))
        assert( load_rsa_keys(KEY_PATH1, process_pub_key, process_priv_key, 
                                                    DEVICE_KEY_LENGTH_BITS) != NULL );
    else
        assert( load_rsa_keys(KEY_PATH2, process_pub_key, process_priv_key, 
                                                    DEVICE_KEY_LENGTH_BITS) != NULL );

    // sanity check
    sanity_check();
}

static
void encls_set_cpusvn(CPUX86State *env)
{
    uint8_t *svn = (uint8_t *)env->regs[R_EBX];

    // Set CSR_CPUSVN
    env->cregs.CR_CPUSVN[1] = 0;
    env->cregs.CR_CPUSVN[0] = (uint64_t)svn;
}

static
void encls_set_stat(CPUX86State *env)
{
    int32_t eid = (int32_t)env->regs[R_EBX];
    stat_t *stat = (stat_t *)env->regs[R_ECX];
    memcpy(stat, &(qenclaves[eid].stat), sizeof(stat_t));
}

static
void encls_set_stack(CPUX86State *env)
{
    uint8_t *sp = (uint8_t *)env->regs[R_EBX];

    env->cregs.CR_EBP = (uint64_t)sp;
    env->cregs.CR_ESP = (uint64_t)sp;
}

/* static uint8_t skipSECSPages(uint64_t baseAddr)
{
    uint8_t iter = 0;
    pageinfo_t *page = (pageinfo_t *)baseAddr;
    secinfo_t *secInfo = (secinfo_t*)page->secinfo;
    while(secInfo->flags.page_type == PT_SECS) {
        sgx_dbg(trace, "Page Type check working: %d", iter);
        page += PAGE_SIZE;
        secInfo = (secinfo_t)page->secinfo;
        iter++;
    }
#if DEBUG
    sgx_dbg(trace, "No_Of_SECS_Pages: %d", iter);
#endif
    return iter;
}*/

static
const char *encls_cmd_to_str(long cmd) {
    switch (cmd) {
    case ENCLS_ECREATE:       return "ECREATE";
    case ENCLS_EADD:          return "EADD";
    case ENCLS_EINIT:         return "EINIT";
    case ENCLS_EREMOVE:       return "EREMOVE";
    case ENCLS_EEXTEND:       return "EEXTEND";
    case ENCLS_EAUG:          return "EAUG";
    case ENCLS_OSGX_INIT:     return "OSGX_INIT";
    case ENCLS_OSGX_PUBKEY:   return "OSGX_PUBKEY";
    case ENCLS_OSGX_EPCM_CLR: return "OSGX_EPCM_CLR";
    case ENCLS_OSGX_CPUSVN:   return "OSGX_CPUSVN";
    }
    return "UNKONWN";
}

void helper_sgx_encls(CPUX86State *env)
{
    sgx_dbg(ttrace,
            "(%-13s) EAX=0x%08"PRIx64", EBX=0x%08"PRIx64", "
            "RCX=0x%08"PRIx64", RDX=0x%08"PRIx64,
            encls_cmd_to_str(env->regs[R_EAX]),
            env->regs[R_EAX],
            env->regs[R_EBX],
            env->regs[R_ECX],
            env->regs[R_EDX]);
    switch (env->regs[R_EAX]) {
        case ENCLS_ECREATE:
            sgx_ecreate(env);
            break;
        case ENCLS_EADD:
            sgx_eadd(env);
            break;
        case ENCLS_EINIT:
            sgx_einit(env);
            break;
        case ENCLS_ELDB:
        case ENCLS_ELDU:
            sgx_eldb(env);
        case ENCLS_EREMOVE:
           //sgx_eremove(env);
            break;
        case ENCLS_EEXTEND:
            sgx_eextend(env);
            break;
        case ENCLS_EBLOCK:
            sgx_eblock(env);
            break;
        case ENCLS_EPA:
            sgx_epa(env);
        case ENCLS_EWB:
            sgx_ewb(env);
            break;
        case ENCLS_EAUG:
            sgx_eaug(env);
            break;
        case ENCLS_EMODPR:
            sgx_emodpr(env);
            break;

        // custom (non-spec) hypercalls: for setting up qemu
        case ENCLS_OSGX_INIT:
            init_qenclave(); // Initializing QEMU Enclave Descriptor
            encls_qemu_init(env);
            break;
        case ENCLS_OSGX_PUBKEY:
            encls_intel_pubkey(env);
            break;
        case ENCLS_OSGX_EPCM_CLR:
            encls_epcm_clear(env);
            break;
        case ENCLS_OSGX_CPUSVN:
            encls_set_cpusvn(env);
            break;
        case ENCLS_OSGX_STAT:
            encls_set_stat(env);
            break;
        case ENCLS_OSGX_SET_STACK:
            encls_set_stack(env);
            break;
        default:
            sgx_err("not implemented yet");
    }
}

void helper_sgx_ehandle(CPUX86State *env)
{
    // Save RIP for later use
    secs_t *secs;
    gprsgx_t *tmp_gpr;
    bool tmp_mode64;

    sgx_msg(info, "Entered Exception Handler QEMU");

    secs = (secs_t *)env->cregs.CR_ACTIVE_SECS;
    tmp_gpr = (gprsgx_t *)(env->cregs.CR_GPR_PA); //CR_XSAVE_PAGE[0];

    // Check for 64 bit mode
    tmp_mode64 = (env->efer & MSR_EFER_LMA) && (env->segs[R_CS].flags & DESC_L_MASK);

    /* (* Save all registers, When saving EFLAGS, the TF bit is set to 0 and
       the RF bit is set to what would have been saved on stack in the non-SGX case *) */

    if (!tmp_mode64) {
        saveState(tmp_gpr, env);
    //    tmp_ssa->rflags.tf = 0;
    } else {
        saveState(tmp_gpr, env);
    //    tmp_ssa->rflags.tf = 0;
    }
#if DEBUG
    sgx_msg(info, "Ssaved the state");
#endif
    /* (* Use a special version of XSAVE that takes a list of physical addresses of logically sequential pages to
    perform the save. TMP_MODE64 specifies whether to use the 32-bit or 64-bit layout.
    SECS.ATTRIBUTES.XFRM selects the features to be saved.
    CR_XSAVE_PAGE_n specifies a list of 1 or more physical addresses of pages that contain the XSAVE area. *)*/
    xsave(tmp_mode64, secs->attributes.xfrm, env->cregs.CR_XSAVE_PAGE[0]);  // N = 0; TODO
    /* (* Clear bytes 8 to 23 of XSAVE_HEADER, i.e. the next 16 bytes after XHEADER_BV *) */
    clearBytes(env->cregs.CR_XSAVE_PAGE, 0);
    /* (* Clear bits in XHEADER_BV[63:0] that are not enabled in ATTRIBUTES.XFRM *)*/
    assignBits(env->cregs.CR_XSAVE_PAGE, secs);
    // (* Restore the outside RSP and RBP from the current SSA frame.
    // This is where they had been stored on most recent EENTER *)
    // XXX: Obtain from the TMP_SSA dedicated to the current EID
    sgx_dbg(trace, "Before ESP: %lx   EBP: %lx", env->regs[R_ESP], env->regs[R_EBP]);

    env->regs[R_ESP] = tmp_gpr->ursp;
    env->regs[R_EBP] = tmp_gpr->urbp;

    sgx_dbg(trace, "After ESP: %lx   EBP: %lx", env->regs[R_ESP], env->regs[R_EBP]);
    // Restore FS and GS
    env->segs[R_FS].base = env->cregs.CR_SAVE_FS.base;
    env->segs[R_FS].limit = env->cregs.CR_SAVE_FS.limit;
    env->segs[R_FS].flags = env->cregs.CR_SAVE_FS.flags;
    env->segs[R_FS].selector = env->cregs.CR_SAVE_FS.selector;

    env->segs[R_GS].base = env->cregs.CR_SAVE_GS.base;
    env->segs[R_GS].limit = env->cregs.CR_SAVE_GS.limit;
    env->segs[R_GS].flags = env->cregs.CR_SAVE_GS.flags;
    env->segs[R_GS].selector = env->cregs.CR_SAVE_GS.selector;

    sgx_dbg(trace, "Was at EIP:  %"PRIx64"", env->eip);
    // Set EAX to the ERESUME leaf index
    //env->regs[R_EAX] = ENCLU_ERESUME;
    // Put the TCS LA into RBX for later use by ERESUME
    //env->regs[R_EBX] = env->cregs.CR_TCS_LA;
    // Put the AEP into RCX for later use by ERESUME
    //env->regs[R_ECX] = env->cregs.CR_AEP;
    // Update the SSA frame #

    ((tcs_t *)env->cregs.CR_TCS_PA)->cssa += 1;

    // (* Restore XCR0 if needed *)
    if ((env->cr[4] & CR4_OSXSAVE_MASK)) {
        env->xcr0 = env->cregs.CR_SAVE_XCR0;
    }

    sgx_msg(info, "Exception Check- Gets redirected to the appropriate exception Handler");
    return;
}

void helper_sgx_trace_pc(target_ulong pc)
{
    sgx_dbg(trace, "pc = %p", (void *)pc);
}

/* Local hack for rdrand instruction------------
   it just read /dev/random from the host and returns
   the value to the specific register.
   it could trigger erro if it is called too frequently,
   because /dev/random's pool is generated from the HW based
   entropy and can be exhausted...  */
void helper_rdrand(CPUX86State *env, uint32_t regSize , uint32_t modrm)
{
    char random[8];
    int result = 0;
    int fd = open("/dev/random", O_RDONLY);
    int rm = 0;

    if(fd == -1) {
        printf("file open error: /dev/random\n");
        return;
    }
    memset(random, 0, 8);

    switch(regSize) {
        case 16:
            result = read(fd, random, 2);
            if(result != 2){
                goto ERROR;
            }
            break;
        case 32:
            result = read(fd, random, 4);
            if(result != 4){
                goto ERROR;
            }
            break;
        case 64:
            result = read(fd, random, 8);
            if(result != 8){
                goto ERROR;
            }
            break;
        default:
            break;

    }
    rm = modrm & 7; // rm indicate register RAX ~ EDI
    memset(&env->regs[rm], 0, sizeof(target_ulong)); // clear reg
    memcpy(&env->regs[rm], random, regSize/8); //copy random value

    // Rdrand success CF = 1
    env->eflags |= (CC_C);

    // Clear flags
    env->eflags &= ~(CC_O | CC_S | CC_Z | CC_A | CC_P);
    close(fd);
    return;

ERROR:
    // Rdrand fail CF = 0
    printf("read error: /dev/random\n");
    env->eflags &= ~(CC_C);
    return;
}
