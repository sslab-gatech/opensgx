#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "sgx.h"
#include "sgx-dbg.h"
#include "sgx-utils.h"

#include "polarssl/sha256.h"

void fmt_hash(uint8_t hash[32], char out[64+1])
{
    int i;
    for (i = 0; i < 32; i ++) {
        snprintf(&out[i*2], 3, "%02X", hash[i]);
    }
    out[64] = '\0';
}

char *fmt_bytes(uint8_t *bytes, int size)
{
    char *buf = malloc(size*2 + 1);
    if (!buf)
        return NULL;

    int i;
    for (i = 0; i < size; i ++)
        snprintf(&buf[i*2], 3, "%02X", *(bytes + i));

    buf[size*2] = '\0';
    return buf;
}

static
void load_rsa_key_from_str(uint8_t *key, char *bytes, size_t size)
{
    int i;
    for (i = 0; i < size; i++) {
        sscanf(bytes + i*2, "%02X", (unsigned int *)&key[i]);
    }

    if (sgx_dbg_rsa) {
        char *loaded = fmt_bytes(key, size);
        sgx_dbg(rsa, "read from %s", bytes);
        sgx_dbg(rsa, "load to   %s", loaded);
        free(loaded);
    }
}

rsa_context *load_rsa_keys(const char *conf, uint8_t *pubkey, uint8_t *seckey, int bits)
{
    FILE *fp = fopen(conf, "r");
    if (!fp)
        sgx_msg(err, "Failed to open file");

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

    uint8_t *p;
    uint8_t *q;
    uint8_t *e;

    p = malloc(bytes);
    q = malloc(bytes);
    e = malloc(bytes);

    while (getline(&line, &len, fp) != -1) {
        // skip comments
        if (len > 0 && line[0] == '#')
            continue;

        if (!strncmp(line, "PUBKEY: ", npubkey))
            load_rsa_key_from_str(pubkey, line + npubkey, bytes);
        else if (!strncmp(line, "SECKEY: ", nseckey))
            load_rsa_key_from_str(seckey, line + nseckey, bytes);
        else if (!strncmp(line, "P: ", np))
            load_rsa_key_from_str(p, line + np, bytes);
        else if (!strncmp(line, "Q: ", nq))
            load_rsa_key_from_str(q, line + nq, bytes);
        else if (!strncmp(line, "E: ", ne))
            load_rsa_key_from_str(e, line + ne, bytes);
    }

    // Check if load successfully
    if (sgx_dbg_rsa) {
        char *pubkey_str = fmt_bytes(pubkey, bytes);
        char *seckey_str = fmt_bytes(seckey, bytes);
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

    rsa_context *ctx = malloc(sizeof(rsa_context));
    if (!ctx)
        sgx_msg(err, "failed to allocate rsa ctx");

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

int file_exist(const char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}
