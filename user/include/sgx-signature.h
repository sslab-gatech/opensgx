#pragma once

#include <sgx.h>

#define STRING_ECREATE 0x0045544145524345
#define STRING_EADD    0x0000000044444145
#define STRING_EEXTEND 0x00444E4554584545

//extern void generate_enclavehash(void *hash, void *entry, size_t size, tcs_t *tcs);

//extern void generate_enclavehash(void *hash, void *entries[], unsigned int codes_size[],
//                                 int n_of_codes, tcs_t *tcs);
extern void generate_enclavehash(void *hash, void *code, int code_pages,
                                 size_t tcs);

extern void generate_einittoken_mac(einittoken_t *token, uint64_t le_tcs,
                                    uint64_t le_aep);

extern void generate_launch_key(unsigned char *device_key, unsigned char *launch_key);

extern uint8_t get_tls_npages(tcs_t *tcs);

extern void set_tcs_fields(tcs_t *tcs, size_t offset);
extern void update_tcs_fields(tcs_t *tcs, int tls_page_offset, int ssa_page_offset);

extern void rsa_key_generate(uint8_t *pubkey, uint8_t *seckey, rsa_context *rsa, int bits);


// for rsa key pair generation
rsa_context *load_rsa_keys(char *conf, uint8_t *pubkey, uint8_t *seckey,
                           int bits);
void rsa_sign(rsa_context *ctx, rsa_sig_t sig, unsigned char *bytes, int len);

// for mac generation
void cmac(unsigned char *key, unsigned char *input, size_t bytes, unsigned char *mac);

// linked from sgx-kern
extern char *empty_page;
