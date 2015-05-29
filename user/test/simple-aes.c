// An enclave test case for simple encryption/decryption using openssl library.
// By linking sslLib.a while compile this application, 
// it is possible to use openssl library functions inside the enclave.
// sslLib.a is customized version of libcrypto.a of openssl.

#include "test.h"
#include <openssl/aes.h>

#define KEY_BIT 128

// Key for simple aes test
unsigned char my_key[16] = "thiskeyisverybad";
AES_KEY my_aes_key;

void enclave_main()
{
    // Print original message
    char buf[] = "Encrypt me";
    sgx_puts(buf);

    // Simple encryption
    private_AES_set_encrypt_key(my_key, KEY_BIT, &my_aes_key);
    AES_encrypt((unsigned char *)buf, (unsigned char *)buf, &my_aes_key);

    // Print encrypted message
    sgx_puts(buf);

    // Simple decryption
    private_AES_set_decrypt_key(my_key, KEY_BIT, &my_aes_key);
    AES_decrypt((unsigned char *)buf, (unsigned char *)buf, &my_aes_key);

    // Print decrypted message
    sgx_puts(buf);
   
    sgx_exit(NULL);
}
