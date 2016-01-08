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

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <getopt.h>
#include <time.h>

#include <sgx.h>
#include <sgx-user.h>
#include <sgx-crypto.h>
#include <sgx-utils.h>
#include <sgx-loader.h>

void cmd_genkey(char *bits)
{
    // generate RSA key pair
    int key_bits;
    size_t key_bytes;

    key_bits = atoi(bits);

    key_bytes = key_bits >> 3;
    if ((key_bits & 0x7) > 0)
        key_bytes++;

    uint8_t pubkey[key_bytes];
    uint8_t seckey[key_bytes];

    rsa_context rsa;
    rsa_key_generate(pubkey, seckey, &rsa, key_bits);

    uint8_t P[key_bytes];
    uint8_t Q[key_bytes];
    uint8_t E[key_bytes];

    mpi_write_binary(&rsa.P, P, key_bytes);
    mpi_write_binary(&rsa.Q, Q, key_bytes);
    mpi_write_binary(&rsa.E, E, key_bytes);

    char *pubkey_str = fmt_bytes(pubkey, key_bytes);
    char *seckey_str = fmt_bytes(seckey, key_bytes);
    char *p_str = fmt_bytes(P, key_bytes);
    char *q_str = fmt_bytes(Q, key_bytes);
    char *e_str = fmt_bytes(E, key_bytes);

    printf("# generated config\n");
    printf("PUBKEY: %s\n", pubkey_str);
    printf("SECKEY: %s\n", seckey_str);
    printf("P: %s\n", p_str);
    printf("Q: %s\n", q_str);
    printf("E: %s\n", e_str);

    free(pubkey_str);
    free(seckey_str);
    free(p_str);
    free(q_str);
    free(e_str);
}

void cmd_pkg(char *bin)
{
    // TODO
}

void cmd_measure(char *binary)
{
    void *code;
    void *entry;
    size_t npages;
    unsigned long entry_offset;
    unsigned char hash[32];
    int toff;

    code = load_elf_enclave(binary, &npages, &entry, &toff);
    if (code == NULL) {
        err(1, "Please provide valid a binary file.");
    }

    entry_offset = (unsigned long)entry - (unsigned long)code;
    generate_enclavehash(hash, code, npages, entry_offset);

    // generate sgx-[binary].conf
    // # ENTRY: (size, offset)
    // HASH: XXX
    // PUBKEY: ...

    char *hash_str = fmt_bytes(hash, 32);
    printf("# generated measurement\n");
    printf("MEASUREMENT: %s\n", hash_str);
}

void cmd_gen_sigstruct(char *conf)
{
    sigstruct_t s;

    unsigned char header[16] = SIG_HEADER1;
    unsigned char header2[16] = SIG_HEADER2;
    unsigned char xfrm_default[8] = {0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x03};
    unsigned char isvprodid_default[2] = {0x00, 0x00};
    unsigned char isvsvn_default[2] = {0x00, 0x00};
    int exinfo = 0;
    int debug = 0;
    int mode64bit = 1;
    int provisionkey = 1;
    int einittokenkey = 0;

    unsigned char *measurement = load_measurement(conf);

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    long date = (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday;

    memcpy(&s.header,  swap_endian(header, 16),  16);
    memcpy(&s.header2, swap_endian(header2, 16), 16);

    // For non-intel enclave
    memset(&s.vendor, 0, 4);

    // Set default value
    memset(&s.swdefined, 0, 4);

    // fill reserved fields with 0s
    memset(s.reserved1, 0, 84);
    memset(s.reserved2, 0, 20);
    memset(s.reserved3, 0, 32);
    memset(s.reserved4, 0, 12);
    s.miscselect.reserved1 = 0;
    memset(s.miscselect.reserved2, 0, 3);
    s.miscmask.reserved1 = 0;
    memset(s.miscmask.reserved2, 0, 3);
    s.attributes.reserved1 = 0;
    s.attributes.reserved2 = 0;
    s.attributes.reserved3 = 0;
    memset(s.attributes.reserved4, 0, 7);
    s.attributeMask.reserved1 = 0;
    s.attributeMask.reserved2 = 0;
    s.attributeMask.reserved3 = 0;
    memset(s.attributeMask.reserved4, 0, 7);

    char *hdr           = fmt_bytes(swap_endian(s.header, 16), 16);
    char *hdr2          = fmt_bytes(swap_endian(s.header2, 16), 16);
    char *vendor        = fmt_bytes((uint8_t*)&vendor, 4);
    char *enclavehash   = fmt_bytes(measurement, 32);
    char *swdef         = fmt_bytes((uint8_t*)&s.swdefined, 4);
    char *xfrm          = fmt_bytes(xfrm_default, 8);
    char *isvprodid     = fmt_bytes(isvprodid_default, 2);
    char *isvsvn        = fmt_bytes(isvsvn_default, 2);
    char *rsv1          = fmt_bytes(s.reserved1, 84);
    char *rsv2          = fmt_bytes(s.reserved2, 20);
    char *rsv3          = fmt_bytes(s.reserved3, 32);
    char *rsv4          = fmt_bytes(s.reserved4, 12);
    char *mselect_rsv2  = fmt_bytes(s.miscselect.reserved2, 3);
    char *mmasck_rsv2   = fmt_bytes(s.miscmask.reserved2, 3);
    char *attrs_rsv4    = fmt_bytes(s.attributes.reserved4, 7);
    char *attrmask_rsv4 = fmt_bytes(s.attributes.reserved4, 7);

    printf("# generated enclave signature structure (SIGSTRUCT)\n");
    printf("# SIGSTRUCT START\n");
    printf("HEADER        : %s\n", hdr);
    printf("VENDOR        : %s\n", vendor);
    printf("DATE          : %ld\n", date);
    printf("HEADER2       : %s\n", hdr2);
    printf("SWDEFINO      : %s\n", swdef);
    printf("RESERVED1     : %s\n", rsv1);
    printf("MODULUS       : \n");
    printf("EXPONENT      : \n");
    printf("SIGNATURE     : \n");
    printf("MISCSELECT\n");
    printf(".EXINFO       : %d\n", exinfo);
    printf(".RESERVED     : %d%s\n", s.miscselect.reserved1,
                                     mselect_rsv2);
    printf("MISCMASK\n");
    printf(".EXINFO       : %d\n", exinfo);
    printf(".RESERVED     : %d%s\n", s.miscmask.reserved1,
                                     mmasck_rsv2);
    printf("RESERVED2     : %s\n", rsv2);
    printf("ATTRIBUTES\n");
    printf(".RESERVED1    : %d\n", s.attributes.reserved1);
    printf(".DEBUG        : %d\n", debug);
    printf(".MODE64BIT    : %d\n", mode64bit);
    printf(".RESERVED2    : %d\n", s.attributes.reserved2);
    printf(".PROVISIONKEY : %d\n", provisionkey);
    printf(".EINITTOKENKEY: %d\n", einittokenkey);
    printf(".RESERVED3    : %d%s\n", s.attributes.reserved3,
                                     attrs_rsv4);
    printf(".XFRM         : %s\n", xfrm);
    printf("ATTRIBUTEMASK\n");
    printf(".RESERVED1    : %d\n", s.attributeMask.reserved1);
    printf(".DEBUG        : %d\n", debug);
    printf(".MODE64BIT    : %d\n", mode64bit);
    printf(".RESERVED2    : %d\n", s.attributeMask.reserved2);
    printf(".PROVISIONKEY : %d\n", provisionkey);
    printf(".EINITTOKENKEY: %d\n", einittokenkey);
    printf(".RESERVED3    : %d%s\n", s.attributeMask.reserved3,
                                     attrmask_rsv4);
    printf(".XFRM         : %s\n", xfrm);
    printf("ENCLAVEHASH   : %s\n", enclavehash);
    printf("RESERVED3     : %s\n", rsv3);
    printf("ISVPRODID     : %s\n", isvprodid);
    printf("ISVSVN        : %s\n", isvsvn);
    printf("RESERVED4     : %s\n", rsv4);
    printf("Q1            : \n");
    printf("Q2            : \n");
    printf("# SIGSTRUCT END\n");
}

void cmd_sign(char *conf, char *key)
{
    rsa_sig_t sign;
    rsa_key_t pubkey;
    rsa_key_t seckey;
    rsa_context *ctx;
    sigstruct_t *sigstruct;

    // Load sigstruct from file
    sigstruct = load_sigstruct(conf);

    // Ignore fields don't need to sign
    memset(sigstruct->modulus, 0, 384);
    sigstruct->exponent = 0;
    memset(sigstruct->signature, 0, 384);
    memset(sigstruct->q1, 0, 384);
    memset(sigstruct->q2, 0, 384);

    // Load rsa keys from file
    ctx = load_rsa_keys(key, pubkey, seckey, KEY_LENGTH_BITS);
#if 0
    {
        char *pubkey_str = fmt_bytes(pubkey, KEY_LENGTH);
        char *seckey_str = fmt_bytes(seckey, KEY_LENGTH);

        printf("PUBKEY: %.40s..\n", pubkey_str);
        printf("SECKEY: %.40s..\n", seckey_str);

        free(pubkey_str);
        free(seckey_str);
    }
#endif

    // Generate rsa sign on sigstruct with private key
    rsa_sign(ctx, sign, (unsigned char *)sigstruct, sizeof(sigstruct_t));

    // Compute q1, q2
    unsigned char *q1, *q2;
    q1 = malloc(384);
    q2 = malloc(384);
    memset(q1, 0, 384);
    memset(q2, 0, 384);

    mpi Q1, Q2, S, M, T1, T2, R;
    mpi_init(&Q1);
    mpi_init(&Q2);
    mpi_init(&S);
    mpi_init(&M);
    mpi_init(&T1);
    mpi_init(&T2);
    mpi_init(&R);

    // q1 = signature ^ 2 / modulus
    mpi_read_binary(&S, sign, 384);
    mpi_read_binary(&M, pubkey, 384);
    mpi_mul_mpi(&T1, &S, &S);
    mpi_div_mpi(&Q1, &R, &T1, &M);

    // q2 = (signature ^ 3 - q1 * signature * modulus) / modulus
    mpi_init(&R);
    mpi_mul_mpi(&T1, &T1, &S);
    mpi_mul_mpi(&T2, &Q1, &S);
    mpi_mul_mpi(&T2, &T2, &M);
    mpi_sub_mpi(&Q2, &T1, &T2);
    mpi_div_mpi(&Q2, &R, &Q2, &M);

    mpi_write_binary(&Q1, q1, 384);
    mpi_write_binary(&Q2, q2, 384);

    mpi_free(&Q1);
    mpi_free(&Q2);
    mpi_free(&S);
    mpi_free(&M);
    mpi_free(&T1);
    mpi_free(&T2);
    mpi_free(&R);

    sigstruct = load_sigstruct(conf);
    sigstruct->exponent = 3;
    memcpy(sigstruct->modulus, pubkey, 384);
    memcpy(sigstruct->signature, sign, 384);
    memcpy(sigstruct->q1, q1, 384);
    memcpy(sigstruct->q2, q2, 384);

    char *msg = dump_sigstruct(sigstruct);
    printf("# SIGSTRUCT START\n");
    printf("%s\n", msg);
    printf("# SIGSTRUCT END\n");

    /*unsigned char exp[4] = { 0x00, 0x00, 0x00, 0x03 };
    char *mod_str = fmt_bytes(pubkey, 384);
    char *exp_str = fmt_bytes(exp, 4);
    char *sign_str = fmt_bytes(sign, 384);
    char *q1_str = fmt_bytes(q1, 384);
    char *q2_str = fmt_bytes(q2, 384);

    printf("# sign information\n");
    printf("MODULUS       : %s\n", mod_str);
    printf("EXPONENT      : %s\n", exp_str);
    printf("SIGNATURE     : %s\n", sign_str);
    printf("Q1            : %s\n", q1_str);
    printf("Q2            : %s\n", q2_str);

    free(mod_str);
    free(exp_str);
    free(sign_str);

    unsigned char signer[32];
    sha256(pubkey, KEY_LENGTH, signer, 0);
    char *signer_str = fmt_bytes(pubkey, 32);
    printf("# hash of public key\n");
    printf("MRSIGNER      : %s\n", signer_str);*/
}

void cmd_gen_einittoken(char *conf)
{
    einittoken_t t;
    unsigned char valid_default[4] = {0x00, 0x00, 0x00, 0x01};
    unsigned char xfrm_default[8] = {0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x03};
    unsigned char isvprodid_default[2] = {0x00, 0x00};
    unsigned char isvsvn_default[2] = {0x00, 0x00};
    int exinfo = 0;
    int debug = 0;
    int mode64bit = 1;
    int provisionkey = 0;
    int einittokenkey = 1;

    sigstruct_t *s;
    s = load_sigstruct(conf);
    unsigned char signer[32];
    sha256(s->modulus, KEY_LENGTH, signer, 0);
    char *mrsigner = fmt_bytes(signer, 32);
    char *mrenclave = fmt_bytes(s->enclaveHash, 32);

    // set default value
    memset(t.cpuSvnLE, 0, 16);
    memset(t.keyid, 0, 32);

    // fill reserved fields with 0s
    memset(t.reserved1, 0, 44);
    memset(t.reserved2, 0, 32);
    memset(t.reserved3, 0, 32);
    memset(t.reserved4, 0, 24);
    t.attributes.reserved1 = 0;
    t.attributes.reserved2 = 0;
    t.attributes.reserved3 = 0;
    memset(t.attributes.reserved4, 0, 7);
    t.maskedmiscSelectLE.reserved1 = 0;
    memset(t.maskedmiscSelectLE.reserved2, 0, 3);
    t.maskedAttributesLE.reserved1 = 0;
    t.maskedAttributesLE.reserved2 = 0;
    t.maskedAttributesLE.reserved3 = 0;
    memset(t.maskedAttributesLE.reserved4, 0, 7);

    char *cpusvnle      = fmt_bytes(t.cpuSvnLE, 16);
    char *keyid         = fmt_bytes(t.keyid, 32);
    char *valid         = fmt_bytes(valid_default, 4);
    char *xfrm          = fmt_bytes(xfrm_default, 8);
    char *isvprodid     = fmt_bytes(isvprodid_default, 2);
    char *isvsvn        = fmt_bytes(isvsvn_default, 2);
    char *rsv1          = fmt_bytes(t.reserved1, 44);
    char *rsv2          = fmt_bytes(t.reserved2, 32);
    char *rsv3          = fmt_bytes(t.reserved3, 32);
    char *rsv4          = fmt_bytes(t.reserved4, 24);
    char *attr_rsv4     = fmt_bytes(t.attributes.reserved4, 7);
    char *maskmisc_rsv2 = fmt_bytes(t.maskedmiscSelectLE.reserved2, 3);
    char *maskattr_rsv4 = fmt_bytes(t.maskedAttributesLE.reserved4, 7);

    printf("# generated EINIT token structure (EINITTOKEN)\n");
    printf("# EINITTOKEN START\n");
    printf("VALID             : %s\n", valid);
    printf("RESERVED1         : %s\n", rsv1);
    printf("ATTRIBUTES\n");
    printf(".RESERVED1        : %d\n", t.attributes.reserved1);
    printf(".DEBUG            : %d\n", debug);
    printf(".MODE64BIT        : %d\n", mode64bit);
    printf(".RESERVED2        : %d\n", t.attributes.reserved2);
    printf(".PROVISIONKEY     : %d\n", provisionkey);
    printf(".EINITTOKENKEY    : %d\n", einittokenkey);
    printf(".RESERVED3        : %d%s\n", t.attributes.reserved3, attr_rsv4);
    printf(".XFRM             : %s\n", xfrm);
    printf("MRENCLAVE         : %s\n", mrenclave);
    printf("RESERVED2         : %s\n", rsv2);
    printf("MRSIGNER          : %s\n", mrsigner);
    printf("RESERVED3         : %s\n", rsv3);
    printf("CPUSVNLE          : %s\n", cpusvnle);
    printf("ISVPRODIDLE       : %s\n", isvprodid);
    printf("ISVSVNLE          : %s\n", isvsvn);
    printf("RESERVED4         : %s\n", rsv4);
    printf("MASKEDMISCSELECTLE\n");
    printf(".EXINFO           : %d\n", exinfo);
    printf(".RESERVED         : %d%s\n", t.maskedmiscSelectLE.reserved1,
                                         maskmisc_rsv2);
    printf("MASKEDATTRIBUTESLE\n");
    printf(".RESERVED1        : %d\n", t.maskedAttributesLE.reserved1);
    printf(".DEBUG            : %d\n", debug);
    printf(".MODE64BIT        : %d\n", mode64bit);
    printf(".RESERVED2        : %d\n", t.maskedAttributesLE.reserved2);
    printf(".PROVISIONKEY     : %d\n", provisionkey);
    printf(".EINITTOKENKEY    : %d\n", einittokenkey);
    printf(".RESERVED3        : %d%s\n", t.maskedAttributesLE.reserved3,
                                         maskattr_rsv4);
    printf(".XFRM             : %s\n", xfrm);
    printf("KEYID             : %s\n", keyid);
    printf("MAC               : \n");
    printf("# EINITTOKEN END\n");
}

void cmd_mac(char *conf, char *key)
{
    printf("# MAC information\n");

    einittoken_t *token;
    token = load_einittoken(conf);

    unsigned char device_pubkey[DEVICE_KEY_LENGTH];
    unsigned char device_seckey[DEVICE_KEY_LENGTH];
    unsigned char launch_key[DEVICE_KEY_LENGTH];
    unsigned char mac[MAC_SIZE];
    //do we need this?
    //rsa_context *ctx;
    char *launch_key_str;
    char *mac_str;

    load_rsa_keys(key, device_pubkey, device_seckey, DEVICE_KEY_LENGTH_BITS);
    {
        char *pubkey_str = fmt_bytes(device_pubkey, DEVICE_KEY_LENGTH);
        char *seckey_str = fmt_bytes(device_seckey, DEVICE_KEY_LENGTH);

        printf("DEVICE PUBKEY     : %s\n", pubkey_str);
        printf("DEVICE SECKEY     : %s\n", seckey_str);

        free(pubkey_str);
        free(seckey_str);
    }

    generate_launch_key(device_seckey, launch_key);

    cmac(launch_key, (unsigned char *)token, 192, mac);

    launch_key_str = fmt_bytes(launch_key, DEVICE_KEY_LENGTH);
    mac_str = fmt_bytes(mac, MAC_SIZE);
    printf("LAUNCH LEY        : %s\n", launch_key_str);
    printf("MAC               : %s\n", mac_str);

    memcpy(token->mac, mac, 16);
    char *msg = dbg_dump_einittoken(token);
    printf("# EINITTOKEN START\n");
    printf("%s\n", msg);
    printf("# EINITTOKEN END\n");
}

void cmd_help()
{
    printf("[usage] sgx-tool {opts}\n");
    printf("  -k|--keygen       : generate RSA key with given bits (-k BITS)\n");
    printf("  -h|--help         : help message\n");
    printf("  -p|--pkg          : package a static binary\n");
    printf("  -m|--measure      : measure a binary with given region\n");
    printf("                      (-m BINARY)\n");
    printf("  -s|--sign         : generate rsa sign on a sigstruct with private key\n");
    printf("                      (-s SIGSTRUECT --key=KEYFILE)\n");
    printf("  -M|--mac          : generate mac on a einittoken with Launch Key\n");
    printf("                      (-M EINITTOKEN --key=KEYFILE)\n");
    printf("  -S|--sigstructgen : generate a sigstruct format\n");
    printf("  -E|--einittokengen: generate a einittoken format\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    struct option options[] = {
        {"keygen"       , required_argument, 0, 'k'},
        {"help"         , no_argument      , 0, 'h'},
        {"pkg"          , required_argument, 0, 'p'},
        {"measure"      , required_argument, 0, 'm'},
        {"sign"         , required_argument, 0, 's'},
        {"mac"          , required_argument, 0, 'M'},
        {"key"          , required_argument, 0, 'K'},
        {"sigstructgen" , required_argument, 0, 'S'},
        {"einittokengen", required_argument, 0, 'E'},
        {"sigstruct"    , required_argument, 0, 'g'},
        {"einittoken"   , required_argument, 0, 't'},
        {0, 0, 0, 0}
    };

    while (1) {
        int optind = 0;
        char c = getopt_long(argc, argv, "k:hp:m:s:M:S:E:r:c:", options, &optind);
        if (c == -1)
            break;

        switch (c) {
        case 'k':
            cmd_genkey(optarg);
            break;
        case 'p':
            cmd_pkg(optarg);
            break;
        case 'h':
            cmd_help();
            break;
        case 'm': {
            cmd_measure(optarg);
            break;
        }
        case 's': {
            char *conf = optarg;
            char *keyfile;
            c = getopt_long(argc, argv, "K:", options, &optind);
            keyfile = optarg;
            cmd_sign(conf, keyfile);
            break;
        }
        case 'M': {
            char *conf = optarg;
            char *keyfile;
            c = getopt_long(argc, argv, "K:", options, &optind);
            keyfile = optarg;
            cmd_mac(conf, keyfile);
            break;
        }
        case 'S':
            cmd_gen_sigstruct(optarg);
            break;;
        case 'E':
            cmd_gen_einittoken(optarg);
            break;
    }
    }

    return 0;
}
