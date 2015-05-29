#include "cpu.h"
#include "exec/helper-proto.h"
#include "crypto.h"
#include "sgx-dbg.h"
#include "polarssl/aes_cmac128.h"

static
void aes_cmac_start(CPUX86State *env) {
    aes_cmac128_context *ctx = (aes_cmac128_context *)env->regs[R_EBX];
    uint8_t *key = (uint8_t *)env->regs[R_ECX];

    aes_cmac128_starts(ctx, key);
}

static
void aes_cmac_update(CPUX86State *env) {
    aes_cmac128_context *ctx = (aes_cmac128_context *)env->regs[R_EBX];
    uint8_t *msg = (uint8_t *)env->regs[R_ECX];
    size_t msg_len = (size_t) env->regs[R_EDX];

    aes_cmac128_update(ctx, msg, msg_len);
}

static
void aes_cmac_final(CPUX86State *env) {
    aes_cmac128_context *ctx = (aes_cmac128_context *)env->regs[R_EBX];
    uint8_t *mac = (uint8_t *)env->regs[R_ECX];

    aes_cmac128_final(ctx, mac);
}

void helper_crypto(CPUX86State *env)
{
    switch (env->regs[R_EAX]) {
        case AES_CMAC_Start:
            aes_cmac_start(env);
            break;
        case AES_CMAC_Update:
            aes_cmac_update(env);
            break;
        case AES_CMAC_Final:
            aes_cmac_final(env);
            break;
        default:
            sgx_err("not implemented yet");
    }
}