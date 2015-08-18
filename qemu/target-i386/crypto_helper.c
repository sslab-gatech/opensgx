#include <fcntl.h>
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
