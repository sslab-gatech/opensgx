// test openssl api

#include "test.h"
#include <openssl/bn.h>
#include <openssl/rsa.h>

BIGNUM *bn = NULL;
RSA *rsa = NULL;

void enclave_main()
{
    // Bignum allocation for global variable
    bn = BN_new();
    sgx_print_hex((unsigned long)bn);

    char *test = sgx_malloc(16);
    sgx_print_hex((unsigned long)test);

    // Bignum allocation for local variable
    BIGNUM *bn2 = BN_new();
    sgx_print_hex((unsigned long)bn2);

    char *test2 = sgx_malloc(16);
    sgx_print_hex((unsigned long)test2);

    // RSA allocation for global variable
    rsa = RSA_new();
    sgx_print_hex((unsigned long)rsa);

    // RSA allocation for local variable
    RSA *rsa2 = RSA_new();
    sgx_print_hex((unsigned long)rsa2);
   
    sgx_exit(NULL);
}
