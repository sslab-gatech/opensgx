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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <sgx-user.h>
#include <sgx-utils.h>
#include <sgx-signature.h>

#ifndef GET_UINT32_BE
#define GET_UINT32_BE(n,b,i)                            \
{                                                       \
   (n) = ( (uint32_t) (b)[(i)    ] << 24 )              \
       | ( (uint32_t) (b)[(i) + 1] << 16 )              \
       | ( (uint32_t) (b)[(i) + 2] <<  8 )              \
       | ( (uint32_t) (b)[(i) + 3]       );             \
}
#endif

#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n,b,i)                            \
{                                                       \
    (b)[(i)    ] = (unsigned char) ( (n) >> 24 );       \
    (b)[(i) + 1] = (unsigned char) ( (n) >> 16 );       \
    (b)[(i) + 2] = (unsigned char) ( (n) >>  8 );       \
    (b)[(i) + 3] = (unsigned char) ( (n)       );       \
}
#endif

static uint64_t g_update_counter;

static
void sha256init(unsigned char *hash)
{
    sha256_context ctx;

    sha256_init(&ctx);
    sha256_starts(&ctx, 0);

    PUT_UINT32_BE(ctx.state[0], hash,  0);
    PUT_UINT32_BE(ctx.state[1], hash,  4);
    PUT_UINT32_BE(ctx.state[2], hash,  8);
    PUT_UINT32_BE(ctx.state[3], hash, 12);
    PUT_UINT32_BE(ctx.state[4], hash, 16);
    PUT_UINT32_BE(ctx.state[5], hash, 20);
    PUT_UINT32_BE(ctx.state[6], hash, 24);
    PUT_UINT32_BE(ctx.state[7], hash, 28);
}

static
void sha256update(unsigned char *input, unsigned char *hash)
{
    sha256_context ctx;

    sha256_init(&ctx);
    GET_UINT32_BE(ctx.state[0], hash,  0);
    GET_UINT32_BE(ctx.state[1], hash,  4);
    GET_UINT32_BE(ctx.state[2], hash,  8);
    GET_UINT32_BE(ctx.state[3], hash, 12);
    GET_UINT32_BE(ctx.state[4], hash, 16);
    GET_UINT32_BE(ctx.state[5], hash, 20);
    GET_UINT32_BE(ctx.state[6], hash, 24);
    GET_UINT32_BE(ctx.state[7], hash, 28);

    sha256_process(&ctx, input);

    PUT_UINT32_BE(ctx.state[0], hash,  0);
    PUT_UINT32_BE(ctx.state[1], hash,  4);
    PUT_UINT32_BE(ctx.state[2], hash,  8);
    PUT_UINT32_BE(ctx.state[3], hash, 12);
    PUT_UINT32_BE(ctx.state[4], hash, 16);
    PUT_UINT32_BE(ctx.state[5], hash, 20);
    PUT_UINT32_BE(ctx.state[6], hash, 24);
    PUT_UINT32_BE(ctx.state[7], hash, 28);

    sha256_free(&ctx);
}

static
void sha256final(unsigned char *hash, size_t len)
{
    sha256_context ctx;

    sha256_init(&ctx);
    ctx.total[0] = (uint32_t)len;
    ctx.total[0] &= 0xFFFFFFFF;

    if (ctx.total[0] < (uint32_t)len)
        ctx.total[1]++;

    GET_UINT32_BE(ctx.state[0], hash,  0);
    GET_UINT32_BE(ctx.state[1], hash,  4);
    GET_UINT32_BE(ctx.state[2], hash,  8);
    GET_UINT32_BE(ctx.state[3], hash, 12);
    GET_UINT32_BE(ctx.state[4], hash, 16);
    GET_UINT32_BE(ctx.state[5], hash, 20);
    GET_UINT32_BE(ctx.state[6], hash, 24);
    GET_UINT32_BE(ctx.state[7], hash, 28);
    memset(hash, 0, 32);
    sha256_finish(&ctx, hash);

    sha256_free(&ctx);
}

static
void measure_chunk_page(void *measurement, void *page, uint64_t chunk_offset)
{
    uint64_t tmp_update_field[8];

    tmp_update_field[0] = STRING_EEXTEND;
    tmp_update_field[1] = chunk_offset;
    memset(&tmp_update_field[2], 0, 48);
    sha256update((unsigned char *)tmp_update_field, measurement);
    g_update_counter++;

#if 0
    {
        char hash[64+1];
        fmt_hash(measurement, hash);
        sgx_dbg(info, "pre-measurement extend: %.20s.., counter: %ld", hash,
                                                                g_update_counter);
    }
#endif

    unsigned char *cast_page = (unsigned char *)page;
    sha256update((unsigned char *)(&cast_page[0]),   measurement);
    sha256update((unsigned char *)(&cast_page[64]),  measurement);
    sha256update((unsigned char *)(&cast_page[128]), measurement);
    sha256update((unsigned char *)(&cast_page[192]), measurement);
    g_update_counter += 4;

#if 0
    {
        char hash[64+1];
        fmt_hash(measurement, hash);
        sgx_dbg(info, "pre-measurement extend: %.20s.., counter: %ld", hash,
                                                                g_update_counter);
    }
#endif

}

static
void measure_page_add(void *measurement, void *page, secinfo_t *secinfo,
                      uint64_t page_offset)
{
    uint64_t tmp_update_field[8];
    int i;

#if 0
    hexdump(stderr, page, 32);
#endif

    tmp_update_field[0] = STRING_EADD;
    tmp_update_field[1] = page_offset;
    memcpy(&tmp_update_field[2], secinfo, 48);
    sha256update((unsigned char *)tmp_update_field, measurement);
    g_update_counter++;

#if 0
    {
        char hash[64+1];
        fmt_hash(measurement, hash);
        sgx_dbg(info, "pre-measurement add: %.20s.., counter: %ld", hash,
                                                                g_update_counter);
    }
#endif

    unsigned char *cast_page = (unsigned char *)page;
    for (i = 0; i < PAGE_SIZE/MEASUREMENT_SIZE; i++) {
        uint64_t chunk_offset = i * MEASUREMENT_SIZE;
        measure_chunk_page(measurement, &cast_page[chunk_offset],
                           page_offset + chunk_offset);
    }
}

static
void measure_enclave_create(void *measurement, uint32_t ssa_frame_size,
                            uint64_t enclave_size)
{
    uint8_t tmp_update_field[64];

    uint64_t hash_ecreate = STRING_ECREATE;
    memset(&tmp_update_field[0], 0, 64);
    memcpy(&tmp_update_field[0], &hash_ecreate, 8);
    memcpy(&tmp_update_field[8], &ssa_frame_size, 4);
    memcpy(&tmp_update_field[12], &enclave_size, 8);
    memset(&tmp_update_field[20], 0, 44);
    sha256update((unsigned char *)tmp_update_field, measurement);
    g_update_counter++;

#if 0
    {
        char hash[64+1];
        fmt_hash(measurement, hash);
        sgx_dbg(info, "pre-measurement create: %.20s.., counter: %ld", hash,
                                                                       g_update_counter);
    }
#endif
}

uint8_t get_tls_npages(tcs_t *tcs) {
    return(to_npages(tcs->fslimit + 1) +
           to_npages(tcs->gslimit + 1));
}

// Initialize thread storage.
static
void init_thread_storage(tcs_t *tcs)
{
    // Granularity set at PAGE level.
    tcs->fslimit = PAGE_SIZE - 1;
    tcs->gslimit = PAGE_SIZE - 1;

    // Make GS follows by the end of FS range.
    tcs->ofsbasgx = 0;
    tcs->ogsbasgx = tcs->ofsbasgx + tcs->fslimit + 1;
}

// Set tcs strack frame.
static
void set_stack_frame(tcs_t *tcs)
{
    tcs->nssa = STACK_PAGE_FRAMES_PER_THREAD;
    tcs->cssa = 2;
}

// Set tcs oentry.
static
void set_entry(tcs_t *tcs, size_t offset)
{
    int tls_npages = get_tls_npages(tcs);
    tcs->oentry = ((tls_npages) * PAGE_SIZE) + offset;
}

// Initalize tcs.
void set_tcs_fields(tcs_t *tcs, size_t offset) {
    init_thread_storage(tcs);
    set_stack_frame(tcs);
    set_entry(tcs, offset);
}

// Update the TCS Fields in Kernel module.
void update_tcs_fields(tcs_t *tcs, int tls_page_offset, int ssa_page_offset)
{
    uint64_t tls_offset = tls_page_offset * PAGE_SIZE;
    uint64_t ssa_offset = ssa_page_offset * PAGE_SIZE;

    tcs->ofsbasgx += tls_offset;
    tcs->ogsbasgx += tls_offset;
    tcs->oentry   += tls_offset;

    tcs->ossa = ssa_offset;
}

void generate_enclavehash(void *hash, void *code, int code_pages,
                          size_t entry_offset)
{
    tcs_t *tmp_tcs;
    secinfo_t tmp_secinfo;
    epc_t current_page;
    uint32_t ssa_frame_size;
    uint64_t enclave_size;
    uint64_t page_offset = 0;
    void *page;

    // Pre-compute tcs.
    tmp_tcs = (tcs_t *)memalign(PAGE_SIZE, sizeof(tcs_t));
    if (!tmp_tcs)
        err(1, "failed to allocate tcs");
    memset(tmp_tcs, 0, sizeof(tcs_t));
    set_tcs_fields(tmp_tcs, entry_offset);

    int sec_npages = 1;
    int tcs_npages = 1;
    int tls_npages = get_tls_npages(tmp_tcs);
    int ssa_npages = STACK_PAGE_FRAMES_PER_THREAD;
    int heap_npages = HEAP_PAGE_FRAMES;

    int npages = sec_npages + tcs_npages + tls_npages + code_pages +
                 ssa_npages + heap_npages;

    // Initialize hash value.
    memset(hash, 0, 32);
    sha256init(hash);
    g_update_counter = 0;

    // Pre-compute ssa frame and enclave size.
    ssa_frame_size = 1;

    // Set enclave_size
    npages = rop2(npages);
    enclave_size = PAGE_SIZE * npages;

    // Update measurement for ECREATE.
    measure_enclave_create(hash, ssa_frame_size, enclave_size);
    page_offset += PAGE_SIZE;

    // tcs update.
    int tls_page_offset = sec_npages + tcs_npages;
    int ssa_page_offset = sec_npages + tcs_npages + tls_npages + code_pages;
    update_tcs_fields(tmp_tcs, tls_page_offset, ssa_page_offset);

    // Initialize secinfo.
    memset(&tmp_secinfo, 0, sizeof(tmp_secinfo));
    tmp_secinfo.flags.pending = 0;
    tmp_secinfo.flags.modified = 0;
    tmp_secinfo.flags.reserved1 = 0;
    memset(tmp_secinfo.flags.reserved2, 0, sizeof(tmp_secinfo.flags.reserved2));
    memset(tmp_secinfo.reserved, 0, sizeof(tmp_secinfo.reserved));

    // TCS page setting.
    tmp_secinfo.flags.r = 0;
    tmp_secinfo.flags.w = 0;
    tmp_secinfo.flags.x = 0;
    tmp_secinfo.flags.page_type = PT_TCS;

    // Update measurement for EADD.
    memcpy(&current_page, tmp_tcs, PAGE_SIZE);
    measure_page_add(hash, &current_page, &tmp_secinfo, page_offset);
    page_offset += PAGE_SIZE;

    // REG page setting.
    tmp_secinfo.flags.r = 1;
    tmp_secinfo.flags.w = 1;
    tmp_secinfo.flags.x = 1;
    tmp_secinfo.flags.page_type = PT_REG;

    // Measure tls pages.
    page = (void *)empty_page;
    for (int i = 0; i < tls_npages; i++) {
        memset(&current_page, 0, PAGE_SIZE);
        memcpy(&current_page, &page, sizeof(uintptr_t));
        measure_page_add(hash, &current_page, &tmp_secinfo, page_offset);
        page_offset += PAGE_SIZE;
    }

    // Measure code pages.
    page = (void *)code;
    for (int i = 0; i < code_pages; i++) {
        memset(&current_page, 0, PAGE_SIZE);
        memcpy(&current_page, page, PAGE_SIZE);
        measure_page_add(hash, &current_page, &tmp_secinfo, page_offset);
        page = (void *)((uintptr_t)page + PAGE_SIZE);
        page_offset += PAGE_SIZE;
    }

    // Measrue ssa pages.
    page = (void *)empty_page;
    for (int i = 0; i < ssa_npages; i++) {
        memset(&current_page, 0, PAGE_SIZE);
        memcpy(&current_page, &page, sizeof(uintptr_t));
        measure_page_add(hash, &current_page, &tmp_secinfo, page_offset);
        page_offset += PAGE_SIZE;
    }

    // Measure heap pages.
    page = (void *)empty_page;
    for (int i = 0; i < heap_npages; i++) {
        memset(&current_page, 0, PAGE_SIZE);
        memcpy(&current_page, &page, sizeof(uintptr_t));
        measure_page_add(hash, &current_page, &tmp_secinfo, page_offset);
        page_offset += PAGE_SIZE;
    }

    // Finalize hash
    g_update_counter = g_update_counter * 512;
    sha256final(hash, g_update_counter);
}

void generate_einittoken_mac(einittoken_t *token, uint64_t le_tcs,
                             uint64_t le_aep)
{
    sgx_dbg(trace, "le_tcs: %lx le_aep: %lx", le_tcs, le_aep);
    asm("movl %0, %%eax\n\t"
        "movq %1, %%rbx\n\t"
        "movq %2, %%rcx\n\t"
        "movq %3, %%rdx\n\t"
        ".byte 0x0F\n\t"
        ".byte 0x01\n\t"
        ".byte 0xd7\n\t"
        :
        :"a"((uint32_t)ENCLU_EENTER),
         "b"(le_tcs),
         "c"(le_aep),
         "d"((uint64_t)0));

    // XXX: seems need to do something before return
    //      otherwise will get segfault...
    sgx_msg(trace, "return here");
    return;
}

void rsa_key_generate(uint8_t *pubkey, uint8_t *seckey, rsa_context *rsa, int bits)
{
    int ret;

    entropy_context entropy;
    entropy_init(&entropy);

    const char *pers = "rsa_genkey";
    ctr_drbg_context ctr_drbg;
    ret = ctr_drbg_init(&ctr_drbg, entropy_func, &entropy,
                        (const unsigned char *)pers,
                        strlen(pers));
    if (ret)
        err(ret, "failed to initiate entropy: %d", ret);

    size_t bytes;
    bytes = bits >> 3;
    if ((bits & 0x7) > 0)
        bytes++;

    rsa_init(rsa, RSA_PKCS_V15, 0);

    // NOTE. size in bits
    ret = rsa_gen_key(rsa, ctr_drbg_random, &ctr_drbg,
                      bits, SGX_RSA_EXPONENT);
    if (ret)
        err(ret, "failed to generate RSA keys: %d", ret);

    mpi_write_binary(&rsa->N, pubkey, bytes);
    mpi_write_binary(&rsa->D, seckey, bytes);
}

rsa_context *load_rsa_keys(char *conf, uint8_t *pubkey, uint8_t *seckey, int bits)
{
    FILE *fp = fopen(conf, "r");
    if (!fp)
        err(1, "failed to locate %s", conf);

    char *line = NULL;
    size_t len = 0;

    const int npubkey = strlen("PUBKEY: ");
    const int nseckey = strlen("SECKEY: ");
    const int np = strlen("P: ");
    const int nq = strlen("Q: ");
    const int ne = strlen("E: ");

    size_t bytes;
    bytes = bits >> 3;
    if ((bits & 0x7) > 0)
        bytes++;

    uint8_t *pk;
    uint8_t *sk;
    uint8_t *p;
    uint8_t *q;
    uint8_t *e;

    pk = malloc(bytes);
    sk = malloc(bytes);
    p = malloc(bytes);
    q = malloc(bytes);
    e = malloc(bytes);

    while (getline(&line, &len, fp) != -1) {
        // skip comments
        if (len > 0 && line[0] == '#')
            continue;

        if (!strncmp(line, "PUBKEY: ", npubkey))
            load_bytes_from_str(pk, line + npubkey, bytes);
        else if (!strncmp(line, "SECKEY: ", nseckey))
            load_bytes_from_str(sk, line + nseckey, bytes);
        else if (!strncmp(line, "P: ", np))
            load_bytes_from_str(p, line + np, bytes);
        else if (!strncmp(line, "Q: ", nq))
            load_bytes_from_str(q, line + nq, bytes);
        else if (!strncmp(line, "E: ", ne))
            load_bytes_from_str(e, line + ne, bytes);
    }

    // Check if load successfully
    if (sgx_dbg_rsa) {
        char *pubkey_str = fmt_bytes(pk, bytes);
        char *seckey_str = fmt_bytes(sk, bytes);
        char *p_str = fmt_bytes(p, bytes);
        char *q_str = fmt_bytes(q, bytes);
        char *e_str = fmt_bytes(e, bytes);

        sgx_dbg(ttrace, "pubkey: %.40s..", pubkey_str);
        sgx_dbg(ttrace, "seckey: %.40s..", seckey_str);
        sgx_dbg(ttrace, "p: %.40s..", p_str);
        sgx_dbg(ttrace, "q: %.40s..", q_str);
        sgx_dbg(ttrace, "e: %.40s..", e_str);

        free(pubkey_str);
        free(seckey_str);
        free(p_str);
        free(q_str);
        free(e_str);
    }

    free(line);
    fclose(fp);

    // XXX: workaroud to avoid first three bytes in pubkey set to zero during
    //      file loading.
    memcpy(pubkey, pk, bytes);
    memcpy(seckey, sk, bytes);

    rsa_context *ctx = malloc(sizeof(rsa_context));
    if (!ctx)
        err(1, "failed to allocate rsa ctx");

    rsa_init(ctx, RSA_PKCS_V15, 0);

    // setup ctx
    mpi_read_binary(&ctx->N, pubkey, bytes);
    mpi_read_binary(&ctx->D, seckey, bytes);
    mpi_read_binary(&ctx->P, p, bytes);
    mpi_read_binary(&ctx->Q, q, bytes);
    mpi_read_binary(&ctx->E, e, bytes);

    int ret;
    mpi P1, Q1, H;
    mpi_init(&P1);
    mpi_init(&Q1);
    mpi_init(&H);
    MPI_CHK(mpi_sub_int(&P1, &ctx->P, 1));
    MPI_CHK(mpi_sub_int(&Q1, &ctx->Q, 1));
    MPI_CHK(mpi_mul_mpi(&H, &P1, &Q1));
    MPI_CHK(mpi_inv_mod(&ctx->D , &ctx->E, &H));
    MPI_CHK(mpi_mod_mpi(&ctx->DP, &ctx->D, &P1));
    MPI_CHK(mpi_mod_mpi(&ctx->DQ, &ctx->D, &Q1));
    MPI_CHK(mpi_inv_mod(&ctx->QP, &ctx->Q, &ctx->P));

    ctx->len = mpi_size(&ctx->N);
cleanup:
    return ctx;
}

void rsa_sign(rsa_context *ctx, rsa_sig_t sig,
              unsigned char *bytes, int len)
{
    // generate hash for current sigstruct
    unsigned char hash[HASH_SIZE];
    sha1(bytes, len, hash);
#if 0
    {
        char *hash_str = fmt_bytes(hash, HASH_SIZE);
        sgx_dbg(info, "hash: %s", hash_str);
        free(hash_str);
    }
#endif
    // make signature
    int ret = rsa_pkcs1_sign(ctx, NULL, NULL, RSA_PRIVATE,
                             POLARSSL_MD_SHA1, HASH_SIZE, hash,
                             (unsigned char *)sig);
    if (ret)
        err(1, "failed to sign: 0x%x", -ret);
}

// Allocate PKCS padding constant (352 bytes).
static
uint8_t *alloc_pkcs1_5_padding(void) {
    unsigned char first_pkcs1_5_padding[2] = FIRST_PKCS1_5_PADDING;
    unsigned char last_pkcs1_5_padding[20] = LAST_PKCS1_5_PADDING;
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
void sgx_derivekey(const keydep_t* keydep, unsigned char *device_key,
                   unsigned char* outputdata)
{
    unsigned char hash[32];
    unsigned char *input;
    size_t size;

    size = sizeof(keydep_t) + DEVICE_KEY_LENGTH;
    input = malloc(size);
    memset(input, 0, size);
    memcpy(input, (unsigned char *)keydep, sizeof(keydep_t));
    memcpy(input + sizeof(keydep_t), device_key, DEVICE_KEY_LENGTH);

    sha256(input, size, hash, 0);

    /* Copy the first 16 bytes (128-bits) */
    memcpy(outputdata, hash, 16);
}


// Set up einittoken fields require to be signed.
static
einittoken_t *alloc_einittoken_le(void)
{
    einittoken_t *t = memalign(EINITTOKEN_ALIGN_SIZE, sizeof(einittoken_t));
    if (!t)
        return NULL;

    // Initializate with 0s
    memset(t, 0, sizeof(einittoken_t));

    // VALID(4 bytes)
    // zero when signer is Intel
    t->valid = 0x00000000;

    // Zero for other fields when signer is Intel
    // ATTRIBUTES(16 bytes)
    memset(&t->attributes, 0, sizeof(attributes_t));

    // MRENCLAVE(32 bytes)
    memset(&t->mrEnclave, 0, sizeof(t->mrEnclave));

    // MRSIGNER(32 bytes)
    memset(&t->mrSigner, 0, sizeof(t->mrSigner));

    // CPUSVNLE(16 bytes)
    memset(&t->cpuSvnLE, 0, sizeof(t->cpuSvnLE));

    // ISVPRODIDLE(2 bytes)
    t->isvprodIDLE = 0x0000;

    // ISVSVNLE(2 bytes)
    t->isvsvnLE = 0x0000;

    // KEYID(32 bytes)
    memset(&t->keyid, 0, sizeof(t->keyid));

    // MAC(16 bytes)
    memset(&t->mac, 0, sizeof(t->mac));

    return t;
}

void generate_launch_key(unsigned char *device_key, unsigned char *launch_key)
{
    uint8_t *pkcs1_5_padding;
    einittoken_t *token;
    keydep_t tmp_keydep;

    token = alloc_einittoken_le();
    pkcs1_5_padding = alloc_pkcs1_5_padding();

    // Set up key dependencies
    tmp_keydep.keyname   = LAUNCH_KEY;
    tmp_keydep.isvprodID = token->isvprodIDLE;
    tmp_keydep.isvsvn    = token->isvsvnLE;
    memset(tmp_keydep.ownerEpoch,      0,                           16);
    memcpy(&tmp_keydep.attributes,     &token->maskedAttributesLE,  16);
    memset(&tmp_keydep.attributesMask, 0,                           16);
    memset(tmp_keydep.mrEnclave,       0,                           32);
    memset(tmp_keydep.mrSigner,        0,                           32);
    memcpy(tmp_keydep.keyid,           token->keyid,                32);
    memcpy(tmp_keydep.cpusvn,          token->cpuSvnLE,             16);
    memcpy(tmp_keydep.padding,         pkcs1_5_padding,            352);
    memcpy(&tmp_keydep.miscselect,     &token->maskedmiscSelectLE,   4);
    memset(&tmp_keydep.miscmask,       0,                            4);

    // XXX: temporarily set
    memset(tmp_keydep.ownerEpoch,      0,                           16);
    memset(tmp_keydep.seal_key_fuses,  0,                           16);

    // Calculate derived key
    memset(launch_key, 0, 16);
    sgx_derivekey(&tmp_keydep, device_key, launch_key);
}

void cmac(unsigned char *key, unsigned char *input, size_t bytes, unsigned char *mac)
{
    aes_cmac128_context ctx;
    aes_cmac128_starts(&ctx, key);
    aes_cmac128_update(&ctx, input, bytes);
    aes_cmac128_final(&ctx, mac);
}
