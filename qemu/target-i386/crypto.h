#include <stdio.h>

typedef enum {
    /* CMAC functions */
    AES_CMAC_Start  = 0x00,
    AES_CMAC_Update = 0x01,
    AES_CMAC_Final  = 0x02
} crypto_fun_t;

