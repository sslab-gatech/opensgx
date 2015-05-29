#include "polarssl/rsa.h"

#define max(a,b) \
   ({ __typeof__ (a) _a = (a);  \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a);  \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define is_aligned(addr, bytes) \
    ((((uintptr_t)(const void *)(addr)) & (bytes - 1)) == 0)

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

void fmt_hash(uint8_t hash[32], char out[64+1]);
char *fmt_bytes(uint8_t *bytes, int size);
rsa_context *load_rsa_keys(const char *conf, uint8_t *pubkey, uint8_t *seckey, int bits);

void sha256init(unsigned char *hash);
void sha256update(unsigned char *input, unsigned char *hash);
void sha256final(unsigned char *hash, size_t len);
int file_exist(const char *filename);
