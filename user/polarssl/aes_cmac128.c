/* Reference : https://polarssl.org/discussions/generic/authentication-token */

#include "polarssl/aes_cmac128.h"

#define MIN(a,b)           ((a)<(b)?(a):(b))
#define _MSB(x)            (((x)[0] & 0x80)?1:0)

/** 
 * zero a structure 
 */
#define ZERO_STRUCT(x)      memset((char *)&(x), 0, sizeof(x))

/** 
 * zero a structure given a pointer to the structure 
 */
#define ZERO_STRUCTP(x)     do{ if((x) != NULL) memset((char *)(x), 0, sizeof(*(x)));} while(0)


/* For CMAC Calculation */
static unsigned char const_Rb[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};
static unsigned char const_Zero[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static inline void aes_cmac_128_left_shift_1(const uint8_t in[16], uint8_t out[16])
{
    uint8_t overflow = 0;
    int8_t i;

    for (i = 15; i >= 0; i--) {
        out[i] = in[i] << 1;
        out[i] |= overflow;
        overflow = _MSB(&in[i]);
     } 
}

static inline void aes_cmac_128_xor(const uint8_t in1[16], const uint8_t in2[16], 
                                        uint8_t out[16])
{
    uint8_t i;

    for (i = 0; i < 16; i++) {
        out[i] = in1[i] ^ in2[i];
    }
}

/*
 * AES-CMAC-128 context setup
 */
void aes_cmac128_starts(aes_cmac128_context *ctx, const uint8_t K[16])
{
    uint8_t L[16];
    unsigned char iv[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    /* Zero struct of aes_context */
    ZERO_STRUCTP(ctx);
    /* Initialize aes_context */
    aes_setkey_enc(&ctx->aes_key, K, 128);

    /* step 1 - generate subkeys k1 and k2 */
    aes_crypt_cbc(&ctx->aes_key, AES_ENCRYPT, 16, iv, const_Zero, L);

    if (_MSB(L) == 0) {
        aes_cmac_128_left_shift_1(L, ctx->K1);
    } else {
        uint8_t tmp_block[16];

        aes_cmac_128_left_shift_1(L, tmp_block);
        aes_cmac_128_xor(tmp_block, const_Rb, ctx->K1);
        ZERO_STRUCT(tmp_block);
    }

    if (_MSB(ctx->K1) == 0) {
        aes_cmac_128_left_shift_1(ctx->K1, ctx->K2);
    } else {
        uint8_t tmp_block[16];

        aes_cmac_128_left_shift_1(ctx->K1, tmp_block);
        aes_cmac_128_xor(tmp_block, const_Rb, ctx->K2);
        ZERO_STRUCT(tmp_block);
   }

    ZERO_STRUCT(L);
}

/*
 * AES-CMAC-128 process message
 */
void aes_cmac128_update(aes_cmac128_context *ctx, const uint8_t *_msg, size_t _msg_len)
{
    unsigned char iv[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t tmp_block[16];
    uint8_t Y[16];
    const uint8_t *msg = _msg;
    size_t msg_len = _msg_len;

    /*
     * copy the remembered last block
     */
    ZERO_STRUCT(tmp_block);
    if (ctx->last_len) {
        memcpy(tmp_block, ctx->last, ctx->last_len);
    }

    /*
     * check if we expand the block
     */
    if (ctx->last_len < 16) {
        size_t len = MIN(16 - ctx->last_len, msg_len);

        memcpy(&tmp_block[ctx->last_len], msg, len);
        memcpy(ctx->last, tmp_block, 16);
        msg += len;
        msg_len -= len;
        ctx->last_len += len;
    }

    if (msg_len == 0) {
        /* if it is still the last block, we are done */
        ZERO_STRUCT(tmp_block);
        return;
    }

    /*
     * It is not the last block anymore
     */
    ZERO_STRUCT(ctx->last);
    ctx->last_len = 0;

    /*
     * now checksum everything but the last block
     */
    aes_cmac_128_xor(ctx->X, tmp_block, Y);
    aes_crypt_cbc(&ctx->aes_key, AES_ENCRYPT, 16, iv, Y, ctx->X);

    while (msg_len > 16) {
        memcpy(tmp_block, msg, 16);
        msg += 16;
        msg_len -= 16;

        aes_cmac_128_xor(ctx->X, tmp_block, Y);
        aes_crypt_cbc(&ctx->aes_key, AES_ENCRYPT, 16, iv, Y, ctx->X);
    }

    /*
     * copy the last block, it will be processed in
     * aes_cmac128_final().
     */
     memcpy(ctx->last, msg, msg_len);
     ctx->last_len = msg_len;

    ZERO_STRUCT(tmp_block);
    ZERO_STRUCT(Y);
}

/*
 * AES-CMAC-128 compute T
 */
void aes_cmac128_final(aes_cmac128_context *ctx, uint8_t T[16])
{
    unsigned char iv[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t tmp_block[16];
    uint8_t Y[16];

    if (ctx->last_len < 16) {
        ctx->last[ctx->last_len] = 0x80;
        aes_cmac_128_xor(ctx->last, ctx->K2, tmp_block);
    } else {
        aes_cmac_128_xor(ctx->last, ctx->K1, tmp_block);
    }

    aes_cmac_128_xor(tmp_block, ctx->X, Y);
    aes_crypt_cbc(&ctx->aes_key, AES_ENCRYPT, 16, iv, Y, T);

    ZERO_STRUCT(tmp_block);
    ZERO_STRUCT(Y);
    ZERO_STRUCTP(ctx);
}
