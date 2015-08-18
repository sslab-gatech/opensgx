#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sha.h>

#include "protocol.h"
#include "tor-lib.h"

#include <sgx-lib.h>
#include <sgx.h>

#define IDENTITY_KEY_BITS 3072
#define SIGNING_KEY_BITS 2048
#define CERTIFICATE_BUF_SIZE 4096
#define SIGNING_BUF_SIZE 1024

#define SERIAL_NUMBER_SIZE 8	// from tortls.c
#define DIROBJ_MAX_SIG_LEN 256	// from routerparse.h

// From strlcpy.c
/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t strlcpy(char *dst, const char *src, size_t siz)
{
    register char *d = dst;
    register const char *s = src;
    register size_t n = siz;

    if (n == 0)
        return(sgx_strlen(s));
    while (*s != '\0') {
        if (n != 1) {
            *d++ = *s;
            n--;
        }
        s++;
    }
    *d = '\0';

    return(s - src);    /* count does not include NUL */
}

// From strlcat.c
/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t
strlcat(char *dst, const char *src, size_t siz)
{
    register char *d = dst;
    register const char *s = src;
    register size_t n = siz;
    size_t dlen;

    /* Find the end of dst and adjust bytes left but don't go past end */
    while (n-- != 0 && *d != '\0')
        d++;
    dlen = d - dst;
    n = siz - dlen;

    if (n == 0)
        return(dlen + sgx_strlen(s));
    while (*s != '\0') {
        if (n != 1) {
            *d++ = *s;
            n--;
        }
        s++;
    }
    *d = '\0';

    return(dlen + (s - src));   /* count does not include NUL */
}

// From torint.h
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
#define INT_MAX 0x7fffffffL
#define SSIZE_T_MAX INT64_MAX
#define SSIZE_T_CEILING ((ssize_t)(SSIZE_T_MAX-16))
#define SIZE_T_CEILING  ((size_t)(SSIZE_T_MAX-16))

// From address.h
#define INET_NTOA_BUF_LEN 16

// From compat.h
#define I64_FORMAT "%lld"
#define PREDICT_LIKELY(exp) __builtin_expect(!!(exp), 1)
#define I64_PRINTF_ARG(a) ((long long signed int)(a))

/** Helper: Deal with confused or out-of-bounds values from localtime_r and
 * friends.  (On some platforms, they can give out-of-bounds values or can
 * return NULL.)  If <b>islocal</b>, this is a localtime result; otherwise
 * it's from gmtime.  The function returned <b>r</b>, when given <b>timep</b>
 * as its input. If we need to store new results, store them in
 * <b>resultbuf</b>. */
struct tm *correct_tm(int islocal, const time_t *timep, struct tm *resultbuf,
           struct tm *r)
{
    const char *outcome;

    if (PREDICT_LIKELY(r)) {
        if (r->tm_year > 8099) { /* We can't strftime dates after 9999 CE. */
            r->tm_year = 8099;
            r->tm_mon = 11;
            r->tm_mday = 31;
            r->tm_yday = 365;
            r->tm_hour = 23;
            r->tm_min = 59;
            r->tm_sec = 59;
        }
        return r;
    }

    /* If we get here, gmtime or localtime returned NULL. It might have done
     * this because of overrun or underrun, or it might have done it because of
     * some other weird issue. */
    if (timep) {
        if (*timep < 0) {
            r = resultbuf;
            r->tm_year = 70; /* 1970 CE */
            r->tm_mon = 0;
            r->tm_mday = 1;
            r->tm_yday = 1;
            r->tm_hour = 0;
            r->tm_min = 0 ;
            r->tm_sec = 0;
            outcome = "Rounding up to 1970";
            goto done;
        } else if (*timep >= INT32_MAX) {
            /* Rounding down to INT32_MAX isn't so great, but keep in mind that we
             * only do it if gmtime/localtime tells us NULL. */
            r = resultbuf;
            r->tm_year = 137; /* 2037 CE */
            r->tm_mon = 11;
            r->tm_mday = 31;
            r->tm_yday = 365;
            r->tm_hour = 23;
            r->tm_min = 59;
            r->tm_sec = 59;
            outcome = "Rounding down to 2037";
            goto done;
        }
    }

    /* If we get here, then gmtime/localtime failed without getting an extreme
     * value for *timep */
//    tor_fragile_assert();
    r = resultbuf;
    sgx_memset(resultbuf, 0, sizeof(struct tm));
    outcome="can't recover";
done:
//    err(1, "%s("I64_FORMAT") failed with error %s: %s",
//            islocal?"localtime":"gmtime",
//            timep?I64_PRINTF_ARG(*timep):0,
//            strerror(errno),
//            outcome);

    sgx_puts("failed with error in correct_tm");
    return r;
}

struct tm *tor_gmtime_r(const time_t *timep, struct tm *result)
{
  struct tm *r;
//  assert(result);

  r = sgx_gmtime(timep);
  if (r)
    sgx_memcpy(result, r, sizeof(struct tm));

  return result;
//  return correct_tm(0, timep, result, r);
}

// From util.h
#define ISO_TIME_LEN 19

/** Set <b>buf</b> to the ISO8601 encoding of the GMT value of <b>t</b>.
 * The buffer must be at least ISO_TIME_LEN+1 bytes long.
 */
void format_iso_time(char *buf, time_t t)
{
  struct tm tm_ori;
  struct tm *tm;

  tor_gmtime_r(&t, &tm_ori);
  sgx_strftime(buf, ISO_TIME_LEN+1, "%Y-%m-%d %H:%M:%S", &tm_ori);

//  sgx_strftime(buf, ISO_TIME_LEN+1, "%Y-%m-%d %H:%M:%S", tor_gmtime_r(&t, &tm_ori));
}

// From crypto.h
/** A public key, or a public/private key-pair. */
typedef struct crypto_pk_t
{
  int refs; /**< reference count, so we don't have to copy keys */
  RSA *key; /**< The key itself */
}crypto_pk_t;

/** Length of the output of our message digest. */
#define DIGEST_LEN 20
/** Length of the output of our second (improved) message digests.  (For now
 * this is just sha256, but it could be any other 256-bit digest.) */
#define DIGEST256_LEN 32
/** Length of encoded public key fingerprints, including space; but not
 * including terminating NUL. */
#define FINGERPRINT_LEN 49
/** Length of hex encoding of SHA1 digest, not including final NUL. */
#define HEX_DIGEST_LEN 40
/** Length of our public keys. */
#define PK_BYTES (1024/8)


/** used by tortls.c: wrap an RSA* in a crypto_pk_t. */
crypto_pk_t *crypto_new_pk_from_rsa_(RSA *rsa)
{
    crypto_pk_t *env = NULL;
//    assert(rsa);
    env = (crypto_pk_t *)sgx_malloc(sizeof(crypto_pk_t));
    env->refs = 1;
    env->key = rsa;
    return env;
}

/** Allocate and return storage for a public key.  The key itself will not yet
 * be set.
 */
crypto_pk_t *crypto_pk_new(void)
{
    RSA *rsa;
    rsa = RSA_new();
    
//    assert(rsa);
    return crypto_new_pk_from_rsa_(rsa);
}

/** Generate a <b>bits</b>-bit new public/private keypair in <b>env</b>.
 * Return 0 on success, -1 on failure.
 */
int crypto_pk_generate_key_with_bits(crypto_pk_t *env, int bits)
{
    if (env->key)
        RSA_free(env->key);

    {
        BIGNUM *e = BN_new();

        RSA *r = NULL;
        if (!e)
            goto done;
        if (! BN_set_word(e, 65537))
            goto done;

        r = RSA_new();
        if (!r)
            goto done;

        if (RSA_generate_key_ex(r, bits, e, NULL) == -1)
            goto done;

        env->key = r;
        r = NULL;
done:
        if (e)
            BN_clear_free(e);
        if (r)
            RSA_free(r);
    }

    if (!env->key) {
        sgx_puts("generate RSA key");
        return -1;
    }

    return 0;
}

/** Helper, used by tor-checkkey.c and tor-gencert.c.  Return the RSA from a
 * crypto_pk_t. */
RSA *crypto_pk_get_rsa_(crypto_pk_t *env)
{
    return env->key;
}

/** Release a reference to an asymmetric key; when all the references
 * are released, free the key.
 */
void crypto_pk_free(crypto_pk_t *env)
{
    if (!env)
        return;

    if (--env->refs > 0)
        return;
//    assert(env->refs == 0);

    if (env->key)
        RSA_free(env->key);

    sgx_free(env);
}

/** Return the size of the public key modulus in <b>env</b>, in bytes. */
size_t crypto_pk_keysize(crypto_pk_t *env)
{
//    assert(env);
//    assert(env->key);

    return (size_t) RSA_size(env->key);
}

/** Sign <b>fromlen</b> bytes of data from <b>from</b> with the private key in
 * <b>env</b>, using PKCS1 padding.  On success, write the signature to
 * <b>to</b>, and return the number of bytes written.  On failure, return
 * -1.
 *
 * <b>tolen</b> is the number of writable bytes in <b>to</b>, and must be
 * at least the length of the modulus of <b>env</b>.
 */
int crypto_pk_private_sign(crypto_pk_t *env, char *to, size_t tolen,
        const char *from, size_t fromlen)
{
    int r;
//    assert(env);
//    assert(from);
//    assert(to);
//    assert(fromlen < INT_MAX);
//    assert(tolen >= crypto_pk_keysize(env));
    if (!env->key->p)
        /* Not a private key */
        return -1;

    r = RSA_private_encrypt((int)fromlen,
            (unsigned char*)from, (unsigned char*)to,
            env->key, RSA_PKCS1_PADDING);
    if (r<0) {
        err(1, "generating RSA signature");
        return -1;
    }
    return r;
}

/** Base64 encode <b>srclen</b> bytes of data from <b>src</b>.  Write
 * the result into <b>dest</b>, if it will fit within <b>destlen</b>
 * bytes.  Return the number of bytes written on success; -1 if
 * destlen is too short, or other failure.
 */
int base64_encode(char *dest, size_t destlen, const char *src, size_t srclen)
{
    /* FFFF we might want to rewrite this along the lines of base64_decode, if
     * it ever shows up in the profile. */
    EVP_ENCODE_CTX ctx;
    int len, ret;
//    assert(srclen < INT_MAX);

    /* 48 bytes of input -> 64 bytes of output plus newline.
       Plus one more byte, in case I'm wrong.
     */
    if (destlen < ((srclen/48)+1)*66)
        return -1;
    if (destlen > SIZE_T_CEILING)
        return -1;

    EVP_EncodeInit(&ctx);
    EVP_EncodeUpdate(&ctx, (unsigned char*)dest, &len,
            (unsigned char*)src, (int)srclen);
    EVP_EncodeFinal(&ctx, (unsigned char*)(dest+len), &ret);
    ret += len;
    return ret;
}

/** Compute the SHA1 digest of the <b>len</b> bytes on data stored in
 * <b>m</b>.  Write the DIGEST_LEN byte result into <b>digest</b>.
 * Return 0 on success, -1 on failure.
 */
int
crypto_digest(char *digest, const char *m, size_t len)
{
//    assert(m);
//    assert(digest);
    return (SHA1((const unsigned char*)m,len,(unsigned char*)digest) == NULL);
}

/** Given a private or public key <b>pk</b>, put a SHA1 hash of the
 * public key into <b>digest_out</b> (must have DIGEST_LEN bytes of space).
 * Return 0 on success, -1 on failure.
 */
int crypto_pk_get_digest(crypto_pk_t *pk, char *digest_out)
{
    unsigned char *buf = NULL;
    int len;

    len = i2d_RSAPublicKey(pk->key, &buf);
    if (len < 0 || buf == NULL)
        return -1;
    if (crypto_digest(digest_out, (char*)buf, len) < 0) {
        OPENSSL_free(buf);
        return -1;
    }
    OPENSSL_free(buf);
    return 0;
}

/** Encode the <b>srclen</b> bytes at <b>src</b> in a NUL-terminated,
 * uppercase hexadecimal string; store it in the <b>destlen</b>-byte buffer
 * <b>dest</b>.
 */
void base16_encode(char *dest, size_t destlen, const char *src, size_t srclen)
{
    const char *end;
    char *cp;

//    assert(destlen >= srclen*2+1);
//    assert(destlen < SIZE_T_CEILING);

    cp = dest;
    end = src+srclen;
    while (src<end) {
        *cp++ = "0123456789ABCDEF"[ (*(const uint8_t*)src) >> 4 ];
        *cp++ = "0123456789ABCDEF"[ (*(const uint8_t*)src) & 0xf ];
        ++src;
    }
    *cp = '\0';
}

/** Copy <b>in</b> to the <b>outlen</b>-byte buffer <b>out</b>, adding spaces
 * every four spaces. */
void crypto_add_spaces_to_fp(char *out, size_t outlen, const char *in)
{
    int n = 0;
    char *end = out+outlen;
//    assert(outlen < SIZE_T_CEILING);

    while (*in && out<end) {
        *out++ = *in++;
        if (++n == 4 && *in && out<end) {
            n = 0;
            *out++ = ' ';
        }
    }
//    assert(out<end);
    *out = '\0';
}

/** Given a private or public key <b>pk</b>, put a fingerprint of the
 * public key into <b>fp_out</b> (must have at least FINGERPRINT_LEN+1 bytes of
 * space).  Return 0 on success, -1 on failure.
 *
 * Fingerprints are computed as the SHA1 digest of the ASN.1 encoding
 * of the public key, converted to hexadecimal, in upper case, with a
 * space after every four digits.
 *
 * If <b>add_space</b> is false, omit the spaces.
 */
int crypto_pk_get_fingerprint(crypto_pk_t *pk, char *fp_out, int add_space)
{
    char digest[DIGEST_LEN];
    char hexdigest[HEX_DIGEST_LEN+1];
    if (crypto_pk_get_digest(pk, digest)) {
        return -1;
    }
    base16_encode(hexdigest,sizeof(hexdigest),digest,DIGEST_LEN);
    if (add_space) {
        crypto_add_spaces_to_fp(fp_out, FINGERPRINT_LEN+1, hexdigest);
    } else {
        sgx_strncpy(fp_out, hexdigest, HEX_DIGEST_LEN+1);
    }
    return 0;
}

/** Given a private or public key <b>pk</b>, put a hashed fingerprint of
 * the public key into <b>fp_out</b> (must have at least FINGERPRINT_LEN+1
 * bytes of space).  Return 0 on success, -1 on failure.
 *
 * Hashed fingerprints are computed as the SHA1 digest of the SHA1 digest
 * of the ASN.1 encoding of the public key, converted to hexadecimal, in
 * upper case.
 */
int crypto_pk_get_hashed_fingerprint(crypto_pk_t *pk, char *fp_out)
{
    char digest[DIGEST_LEN], hashed_digest[DIGEST_LEN];
    if (crypto_pk_get_digest(pk, digest)) {
        return -1;
    }
    if (crypto_digest(hashed_digest, digest, DIGEST_LEN)) {
        return -1;
    }
    base16_encode(fp_out, FINGERPRINT_LEN + 1, hashed_digest, DIGEST_LEN);
    return 0;
}

/** Read a PEM-encoded private key from the <b>len</b>-byte string <b>s</b>
 * into <b>env</b>.  Return 0 on success, -1 on failure.  If len is -1,
 * the string is nul-terminated.
 */
/* Used here, and used for testing. */
int crypto_pk_read_private_key_from_string(crypto_pk_t *env,
                                       const char *s, ssize_t len)
{
    BIO *b;

//    assert(env);
//    assert(s);
//    assert(len < INT_MAX && len < SSIZE_T_CEILING);

    /* Create a read-only memory BIO, backed by the string 's' */
    b = BIO_new_mem_buf((char*)s, (int)len);
    if (!b)
        return -1;

    if (env->key)
        RSA_free(env->key);

    env->key = PEM_read_bio_RSAPrivateKey(b,NULL,NULL,NULL);

    BIO_free(b);

    if (!env->key) {
        err(1, "Error parsing private key");
        return -1;
    }
    return 0;
}

/** Return true iff <b>env</b> has a valid key.
 */
int crypto_pk_check_key(crypto_pk_t *env)
{
    int r;
//    assert(env);

    r = RSA_check_key(env->key);
    if (r <= 0)
        sgx_puts("Error in checking RSA key");
//        err(1,"checking RSA key");
    return r;
}

/** Increase the reference count of <b>env</b>, and return it.
 */
crypto_pk_t *
crypto_pk_dup_key(crypto_pk_t *env)
{
//    assert(env);
//    assert(env->key);

    env->refs++;
    return env;
}

/** Read a PEM-encoded public key from the first <b>len</b> characters of
 * <b>src</b>, and store the result in <b>env</b>.  Return 0 on success, -1 on
 * failure.
 */
int crypto_pk_read_public_key_from_string(crypto_pk_t *env, const char *src,
                                      size_t len)
{
    BIO *b;

//    assert(env);
//    assert(src);
//    assert(len<INT_MAX);

    b = BIO_new(BIO_s_mem()); /* Create a memory BIO */
    if (!b)
        return -1;

    BIO_write(b, src, (int)len);

    if (env->key)
        RSA_free(env->key);
    env->key = PEM_read_bio_RSAPublicKey(b, NULL, NULL, NULL);
    BIO_free(b);
    if (!env->key) {
        err(1, "reading public key from string");
        return -1;
    }

    return 0;
}

/** Helper function to implement crypto_pk_write_*_key_to_string. */
int crypto_pk_write_key_to_string_impl(crypto_pk_t *env, char **dest,
                                   size_t *len, int is_public)
{
    BUF_MEM *buf;
    BIO *b;
    int r;

//    assert(env);
//    assert(env->key);
//    assert(dest);

    b = BIO_new(BIO_s_mem()); /* Create a memory BIO */
    if (!b)
        return -1;

    /* Now you can treat b as if it were a file.  Just use the
     * PEM_*_bio_* functions instead of the non-bio variants.
     */
    if (is_public)
        r = PEM_write_bio_RSAPublicKey(b, env->key);
    else
        r = PEM_write_bio_RSAPrivateKey(b, env->key, NULL,NULL,0,NULL,NULL);

    if (!r) {
        err(1, "writing RSA key to string");
        BIO_free(b);
        return -1;
    }

    BIO_get_mem_ptr(b, &buf);
    (void)BIO_set_close(b, BIO_NOCLOSE); /* so BIO_free doesn't free buf */
    BIO_free(b);

    *dest = sgx_malloc(buf->length+1);
    sgx_memcpy(*dest, buf->data, buf->length);
    (*dest)[buf->length] = 0; /* nul terminate it */
    *len = buf->length;
    BUF_MEM_free(buf);

    return 0;
}

/** PEM-encode the public key portion of <b>env</b> and write it to a
 * newly allocated string.  On success, set *<b>dest</b> to the new
 * string, *<b>len</b> to the string's length, and return 0.  On
 * failure, return -1.
 */
    int
crypto_pk_write_public_key_to_string(crypto_pk_t *env, char **dest,
        size_t *len)
{
    return crypto_pk_write_key_to_string_impl(env, dest, len, 1);
}

/** used by tortls.c: get an equivalent EVP_PKEY* for a crypto_pk_t.  Iff
 * private is set, include the private-key portion of the key. */
EVP_PKEY *crypto_pk_get_evp_pkey_(crypto_pk_t *env, int private)
{
    RSA *key = NULL;
    EVP_PKEY *pkey = NULL;
//    assert(env->key);
    if (private) {
        if (!(key = RSAPrivateKey_dup(env->key)))
            goto error;
    } else {
        if (!(key = RSAPublicKey_dup(env->key)))
            goto error;
    }
    if (!(pkey = EVP_PKEY_new()))
        goto error;
    if (!(EVP_PKEY_assign_RSA(pkey, key)))
        goto error;
    return pkey;
error:
    if (pkey)
        EVP_PKEY_free(pkey);
    if (key)
        RSA_free(key);
    return NULL;
}


#define crypto_pk_generate_key(env)                     \
  crypto_pk_generate_key_with_bits((env), (PK_BYTES*8))

int key_enc_to_tor;
int key_tor_to_enc;

/* For Directory authority */
int authority_num;

EVP_PKEY identity_key_set;
int identity_key_flag;

EVP_PKEY signing_key_set;
int signing_key_flag;

EVP_PKEY *identity_key = NULL;
EVP_PKEY *signing_key = NULL;

char certificate[CERTIFICATE_BUF_SIZE];

char address[INET_NTOA_BUF_LEN+32];
int addr_success;
int months_lifetime;

char *vote_sig = NULL;
char *consensus_sig = NULL;
char *consensus_sig2 = NULL;
int consensus_sig_count = 0;

/* For exit node */
int exit_node_num;
crypto_pk_t *secret_id_key = NULL;
crypto_pk_t *client_id_key = NULL;
crypto_pk_t *onionkey = NULL;
crypto_pk_t *lastonionkey = NULL;
/*
#ifdef CURVE25519_ENABLED
curve25519_keypair_t curve25519_onion_key;
curve25519_keypair_t last_curve25519_onion_key;
#endif
*/

static RSA * generate_key(int bits)
{
	RSA *rsa = NULL;
	crypto_pk_t *env = crypto_pk_new();

	if (crypto_pk_generate_key_with_bits(env,bits) < 0)
	    goto done;

	rsa = crypto_pk_get_rsa_(env);
 	rsa = RSAPrivateKey_dup(rsa);

 done:
  	crypto_pk_free(env);
  	return rsa;
}

/** Return a newly allocated X509 name with commonName <b>cname</b>. */
static X509_NAME *
tor_x509_name_new(const char *cname)
{
	int nid;
	X509_NAME *name;
	if (!(name = X509_NAME_new()))
		return NULL;
	if ((nid = OBJ_txt2nid("commonName")) == NID_undef) goto error;
	if (!(X509_NAME_add_entry_by_NID(name, nid, MBSTRING_ASC,
					(unsigned char*)cname, -1, -1, 0)))
		goto error;
	return name;
error:
	X509_NAME_free(name);
	return NULL;
}


/** Helper: used to generate signatures for routers, directories and
 * network-status objects.  Given a <b>digest_len</b>-byte digest in
 * <b>digest</b> and a secret <b>private_key</b>, generate an PKCS1-padded
 * signature, BASE64-encode it, surround it with -----BEGIN/END----- pairs,
 * and return the new signature on success or NULL on failure.
*/
char *
router_get_dirobj_signature(const char *digest,
                            size_t digest_len,
                            crypto_pk_t *private_key)
{
    char *signature;
    size_t i, keysize;
    int siglen;
    char *buf = NULL;
    size_t buf_len;
    /* overestimate of BEGIN/END lines total len. */
#define BEGIN_END_OVERHEAD_LEN 64

    keysize = crypto_pk_keysize(private_key);
    signature = (char *)sgx_malloc(keysize);
    siglen = crypto_pk_private_sign(private_key, signature, keysize,
            digest, digest_len);
    if (siglen < 0) {
        sgx_puts("Couldn't sign digest!\n");
        goto err;
    }

    /* The *2 here is a ridiculous overestimate of base-64 overhead. */
    buf_len = (siglen * 2) + BEGIN_END_OVERHEAD_LEN;
    buf = (char *)sgx_malloc(buf_len);

    if (strlcpy(buf, "-----BEGIN SIGNATURE-----\n", buf_len) >= buf_len)
        goto truncated;

    i = sgx_strlen(buf);
    if (base64_encode(buf+i, buf_len-i, signature, siglen) < 0) {
        sgx_puts("Couldn't base64-encode signature\n");
        goto err;
    }

    if (strlcat(buf, "-----END SIGNATURE-----\n", buf_len) >= buf_len)
        goto truncated;

    sgx_free(signature);
    return buf;

truncated:
    sgx_puts("tried to exceed string length.\n");
err:
    sgx_free(signature);
    sgx_free(buf);
    return NULL;

}

int
router_append_dirobj_signature(char *buf, size_t buf_len, const char *digest,
                               size_t digest_len, crypto_pk_t *private_key)
{
  size_t sig_len, s_len;
  char *sig = router_get_dirobj_signature(digest, digest_len, private_key);
  if (!sig) {
    sgx_puts("No signature generated\n");
    return -1;
  }
  sig_len = sgx_strlen(sig);
  s_len = sgx_strlen(buf);
  if (sig_len + s_len + 1 > buf_len) {
    sgx_puts("Not enough room for signature\n");
    sgx_free(sig);
    return -1;
  }
  sgx_memcpy(buf+s_len, sig, sig_len+1);
  sgx_free(sig);
  return 0;
}


/** Encode <b>key</b> in the format used in directory documents; return
 * a newly allocated string holding the result or NULL on failure. */
static char *key_to_string_priv(EVP_PKEY *key)


{
	BUF_MEM *buf;
	BIO *b;
	RSA *rsa = EVP_PKEY_get1_RSA(key);
	char *result;
	if (!rsa)
		return NULL;

	b = BIO_new(BIO_s_mem());
	if (!PEM_write_bio_RSAPrivateKey(b, rsa, NULL, NULL, 0, NULL, NULL)) 
		return NULL;

	BIO_get_mem_ptr(b, &buf);
	(void) BIO_set_close(b, BIO_NOCLOSE);
	BIO_free(b);
	result = (char *)sgx_malloc(buf->length + 1);
	sgx_memcpy(result, buf->data, buf->length);
	result[buf->length] = 0;
	BUF_MEM_free(buf);

	return result;
}

/* create identity key */
static int create_identity_key() 
{
	RSA *key;

	if(identity_key_flag == 1) {
        sgx_puts("--create-identity-key was specified, but already exists.\n");
		return 1;
	}

	if(!(key = generate_key(IDENTITY_KEY_BITS))) {
		sgx_puts("Couldn't generate identity key.\n");
		return 1;
	}

	identity_key = EVP_PKEY_new();

	if(!(EVP_PKEY_assign_RSA(identity_key, key))) {
		sgx_puts("Couldn't assign identity key.\n");
		return 1;
	}

	sgx_memcpy(&identity_key_set, identity_key, sizeof(EVP_PKEY));
	identity_key_flag = 1;

	return 0;
}

/* load identity key */
static int load_identity_key()
{
	if(identity_key_flag == 0) {
		sgx_puts("No identity key found.\n");
		return 1;
	}

	return 0;
}

/* create signing key */
static int create_signing_key() 
{
	if(signing_key_flag == 1) {
		sgx_puts("Signing key already exists.\n");
		return 1;
	}

	RSA *key;
	
	if(!(key = generate_key(SIGNING_KEY_BITS))) {
		sgx_puts("Couldn't generate signing key.\n");
		return 1;
	}

	signing_key = EVP_PKEY_new();

	if(!(EVP_PKEY_assign_RSA(signing_key, key))) {
		sgx_puts("Couldn't assign signing key.\n");
		return 1;
	}

	sgx_memcpy(&signing_key_set, signing_key, sizeof(EVP_PKEY));
	signing_key_flag = 1;
	return 0;
}

/* load signing key */
static int load_signing_key()
{
	if(signing_key_flag == 0) {
		sgx_printf("No signing key found.\n");
		return 1;
	}

	return 0;
}

/** Encode <b>key</b> in the format used in directory documents; return
 * a newly allocated string holding the result or NULL on failure. */
static char *key_to_string(EVP_PKEY *key)
{
	BUF_MEM *buf;
	BIO *b;
	RSA *rsa = EVP_PKEY_get1_RSA(key);
	char *result;
	if (!rsa)
		return NULL;

	b = BIO_new(BIO_s_mem());
	if (!PEM_write_bio_RSAPublicKey(b, rsa)) 
		return NULL;

	BIO_get_mem_ptr(b, &buf);
	(void) BIO_set_close(b, BIO_NOCLOSE);
	BIO_free(b);
	result = (char *)sgx_malloc(buf->length + 1);
	sgx_memcpy(result, buf->data, buf->length);
	result[buf->length] = 0;
	BUF_MEM_free(buf);

	return result;
}


/** Set <b>out</b> to the hex-encoded fingerprint of <b>pkey</b>. */
static int
get_fingerprint(EVP_PKEY *pkey, char *out)
{
  int r = 1;
  crypto_pk_t *pk = crypto_new_pk_from_rsa_(EVP_PKEY_get1_RSA(pkey));
  if (pk) {
    r = crypto_pk_get_fingerprint(pk, out, 0);
    crypto_pk_free(pk);
  }
  return r;
}

/** Set <b>out</b> to the hex-encoded fingerprint of <b>pkey</b>. */
static int
get_digest(EVP_PKEY *pkey, char *out)
{
  int r = 1;
  crypto_pk_t *pk = crypto_new_pk_from_rsa_(EVP_PKEY_get1_RSA(pkey));
  if (pk) {
    r = crypto_pk_get_digest(pk, out);
    crypto_pk_free(pk);
  }
  return r;
}


/* create a new certificate */
static int generate_certificate()
{
	time_t now = sgx_time(NULL);

	struct tm tm;
	char published[ISO_TIME_LEN+1];
	char expires[ISO_TIME_LEN+1];
	char id_digest[DIGEST_LEN];
	char fingerprint[FINGERPRINT_LEN+1];
	char *ident = key_to_string(identity_key);
	char *signing = key_to_string(signing_key);

	size_t signed_len;
	char digest[DIGEST_LEN];
	char signature[1024];
	int r;

	get_fingerprint(identity_key, fingerprint);
	get_digest(identity_key, id_digest);

	sgx_localtime_r(&now, &tm);
	format_iso_time(published, now);

	tm.tm_mon += months_lifetime;
    time_t expire = sgx_mktime(&tm);
	format_iso_time(expires, expire);

    strlcat(certificate, "dir-key-certificate-version 3", sizeof(certificate));
    if(addr_success) {
        strlcat(certificate, "\ndir-address ", sizeof(certificate));
        strlcat(certificate, address, sizeof(certificate));
    }
    strlcat(certificate, "\nfingerprint ", sizeof(certificate));
    strlcat(certificate, fingerprint, sizeof(certificate));
    strlcat(certificate, "\ndir-key-published ", sizeof(certificate));
    strlcat(certificate, published, sizeof(certificate));
    strlcat(certificate, "\ndir-key-expires ", sizeof(certificate));
    strlcat(certificate, expires, sizeof(certificate));
    strlcat(certificate, "\ndir-identity-key\n", sizeof(certificate));
    strlcat(certificate, ident, sizeof(certificate));
    strlcat(certificate, "dir-signing-key\n", sizeof(certificate));
    strlcat(certificate, signing, sizeof(certificate));
    strlcat(certificate, "dir-key-crosscert\n", sizeof(certificate));
    strlcat(certificate, "-----BEGIN ID SIGNATURE-----\n", sizeof(certificate));

	sgx_free(ident);
	sgx_free(signing);

	/* Append a cross-certification */
	r = RSA_private_encrypt(DIGEST_LEN, (unsigned char*)id_digest,
			(unsigned char*)signature,
			EVP_PKEY_get1_RSA(signing_key),
			RSA_PKCS1_PADDING);
	signed_len = sgx_strlen(certificate);
	base64_encode(certificate+signed_len, sizeof(certificate)-signed_len, signature, r);

	strlcat(certificate,
			"-----END ID SIGNATURE-----\n"
			"dir-key-certification\n", sizeof(certificate));

	signed_len = sgx_strlen(certificate);
	SHA1((const unsigned char*)certificate,signed_len,(unsigned char*)digest);

	r = RSA_private_encrypt(DIGEST_LEN, (unsigned char*)digest,
			(unsigned char*)signature,
			EVP_PKEY_get1_RSA(identity_key),
			RSA_PKCS1_PADDING);
	strlcat(certificate, "-----BEGIN SIGNATURE-----\n", sizeof(certificate));

	signed_len = sgx_strlen(certificate);
	base64_encode(certificate+signed_len, sizeof(certificate)-signed_len, signature, r);

	strlcat(certificate, "-----END SIGNATURE-----\n", sizeof(certificate));

    sgx_puts(certificate);

	return 0;
}

int directory_configure(int fd_te, int fd_et)
{
	/* routine for directory authority */
//	addr_success = 0;
//	months_lifetime = 0;

	int buf_len;
	char *tmp_buf;

	sgx_puts("Directory authority initialization.\n");

	// identity key process
	sgx_read(fd_te, &buf_len, sizeof(int));
	tmp_buf = (char *)sgx_malloc(buf_len+1);
	sgx_read(fd_te, tmp_buf, buf_len+1);

	if(!sgx_strncmp(tmp_buf, "CR_IDENTITY_KEY", buf_len)) {
		sgx_puts("Creating identity key.\n");

		if(create_identity_key()) {
            sgx_puts("creating identity_key fail");
			buf_len = sgx_strlen("CR_IDENTITY_KEY_ERROR");
			sgx_write(fd_et, &buf_len, sizeof(int));
			sgx_write(fd_et, "CR_IDENTITY_KEY_ERROR", buf_len+1);
			return 0;			
		}

		buf_len = sgx_strlen("CR_IDENTITY_KEY_DONE");
		sgx_write(fd_et, &buf_len, sizeof(int));
		sgx_write(fd_et, "CR_IDENTITY_KEY_DONE", buf_len+1);
	}
	else if(!sgx_strncmp(tmp_buf, "LD_IDENTITY_KEY", buf_len)) {
		sgx_puts("Load identity key.\n");

		if(load_identity_key()) {
			sgx_puts("loading identity_key fail");
			buf_len = sgx_strlen("LD_IDENTITY_KEY_ERROR");
			sgx_write(fd_et, &buf_len, sizeof(int));
			sgx_write(fd_et, "LD_IDENTITY_KEY_ERROR", buf_len+1);
			return 0;			
		}

		buf_len = sgx_strlen("LD_IDENTITY_KEY_DONE");
		sgx_write(fd_et, &buf_len, sizeof(int));
		sgx_write(fd_et, "LD_IDENTITY_KEY_DONE", buf_len+1);
	}

	sgx_free(tmp_buf);

	// signing key process
	sgx_read(fd_te, &buf_len, sizeof(int));
	tmp_buf = (char *)sgx_malloc(buf_len+1);
	sgx_read(fd_te, tmp_buf, buf_len+1);

	if(!sgx_strncmp(tmp_buf, "CR_SIGNING_KEY", buf_len)) {
//		printf("Creating signing key of %d.\n", authority_num);
		sgx_puts("Creating signing key.\n");

		if(create_signing_key()) {
			buf_len = sgx_strlen("CR_SIGNING_KEY_ERROR");
			sgx_write(fd_et, &buf_len, sizeof(int));
			sgx_write(fd_et, "CR_IDENTITY_KEY_ERROR", buf_len+1);
			return 0; 			
		}

		buf_len = sgx_strlen("CR_SIGNING_KEY_DONE");
		sgx_write(fd_et, &buf_len, sizeof(int));
		sgx_write(fd_et, "CR_SIGNING_KEY_DONE", buf_len+1);
	}
	else if(!sgx_strncmp(tmp_buf, "LD_SIGNING_KEY", buf_len)) {
//		printf("Load signing key of %d.\n", authority_num);
		sgx_puts("Load signing key.\n");

		if(load_signing_key()) {
			buf_len = sgx_strlen("LD_SIGNING_KEY_ERROR");
			sgx_write(fd_et, &buf_len, sizeof(int));
			sgx_write(fd_et, "LD_SIGNING_KEY_ERROR", buf_len+1);
			return 0;			
		}

		buf_len = sgx_strlen("LD_SIGNING_KEY_DONE");
		sgx_write(fd_et, &buf_len, sizeof(int));
		sgx_write(fd_et, "LD_SIGNING_KEY_DONE", buf_len+1);
	}

	sgx_free(tmp_buf);

	// recv data related to certificate
//	printf("Receiving global variables for certificate.\n");
	sgx_puts("Receiving global variables for certificate.\n");
	sgx_read(fd_te, &buf_len, sizeof(int));
	sgx_read(fd_te, address, buf_len+1);
	addr_success = 1;
    int temp_var;
	sgx_read(fd_te, &temp_var, sizeof(int));
    months_lifetime = temp_var;

//	printf("Creating certificate of %d.\n", authority_num);
    sgx_puts("Creating certificate.\n");
	if(generate_certificate()) {
		buf_len = sgx_strlen("CR_CERTIFICATE_ERROR");
		sgx_write(fd_et, &buf_len, sizeof(int));
		sgx_write(fd_et, "CR_CERTIFICATE_ERROR", buf_len+1);
		return 0;			
	}

	buf_len = sgx_strlen("CR_CERTIFICATE_DONE");
	sgx_write(fd_et, &buf_len, sizeof(int));
	sgx_write(fd_et, "CR_CERTIFICATE_DONE", buf_len+1);

//    sgx_printf("cert len = %d\n", sgx_strlen(certificate));

    for(int i=0;i<8;i++)
        sgx_write(fd_et, certificate+i*512, 512);
    sgx_puts("Send successfully!\n");

	return 1;
}

int directory_request(int fd_et, int fd_te)
{
	int buf_len;
	char *tmp_buf = NULL;

	while(1) {
		sgx_read(fd_te, &buf_len, sizeof(int));
		tmp_buf = (char *)sgx_malloc(buf_len+1);
		sgx_read(fd_te, tmp_buf, buf_len+1);

		// CERTIFICATE VERIFICATION. 
		if(!sgx_strncmp(tmp_buf, "CERTIFICATE_VERIFY", buf_len)) {
			sgx_printf("Certificate verification for directory authority %d\n", 
					authority_num);
			size_t len;
			char *tmp_signing_key_str = NULL;
			sgx_read(fd_te, &len, sizeof(size_t));
			tmp_signing_key_str = (char *)sgx_malloc(len+1);
			sgx_read(fd_te, tmp_signing_key_str, len+1);

//			char *tmp_ori_str = key_to_string_priv(&signing_key_set);
			char *tmp_ori_str = key_to_string_priv(signing_key);

			if(!sgx_memcmp(tmp_signing_key_str, tmp_ori_str, len)) {
				sgx_printf("Verification Failed!\n");
				buf_len = sgx_strlen("CERTFICATE_VERIFY_ERROR");
				sgx_write(fd_et, &buf_len, sizeof(int));
				sgx_write(fd_et, "CERIFICATE_VERIFY_ERROR", buf_len+1);
				return 0;
			}

			buf_len = sgx_strlen("CERTFICATE_VERIFY_DONE");
			sgx_write(fd_et, &buf_len, sizeof(int));
			sgx_write(fd_et, "CERIFICATE_VERIFY_DONE", buf_len+1);

			sgx_free(tmp_signing_key_str);
			sgx_free(tmp_ori_str);
		
			sgx_free(tmp_buf);
			continue;
		}

		// Voting
		if(!sgx_strncmp(tmp_buf, "VOTING_START", buf_len)) {
			sgx_printf("Voting of %d is started!\n", authority_num);

            // DIGEST COMPUTING
//            char *tmp_ori_str = key_to_string_priv(&signing_key_set);
            char *tmp_ori_str = key_to_string_priv(signing_key);
            crypto_pk_t *tmp_signing_key = crypto_pk_new();
            crypto_pk_read_private_key_from_string(tmp_signing_key, 
													tmp_ori_str, -1);
            char signing_key_digest[DIGEST_LEN];

            if(crypto_pk_get_digest(tmp_signing_key, signing_key_digest) < 0) {
                sgx_puts("Error computing signing key digest\n");
				buf_len = sgx_strlen("GET_DIGEST_ERROR");
				sgx_write(fd_et, &buf_len, sizeof(int));
                sgx_write(fd_et, "GET_DIGEST_ERROR", buf_len+1);
                return 0;
            }

			buf_len = sgx_strlen("GET_DIGEST_DONE");
			sgx_write(fd_et, &buf_len, sizeof(int));
			sgx_write(fd_et, "GET_DIGEST_DONE", buf_len+1);

			// Fingerprint
            char fingerprint[FINGERPRINT_LEN+1];

            if(crypto_pk_get_fingerprint(tmp_signing_key, fingerprint, 0) < 0) {
                sgx_puts("Error getting fingerprint for signing key\n");
				buf_len = sgx_strlen("GET_FINGERPRINT_ERROR");
				sgx_write(fd_et, &buf_len, sizeof(int));
                sgx_write(fd_et, "GET_FINGERPRINT_ERROR", buf_len+1);
                return 0;
            }

			buf_len = sgx_strlen("GET_FINGERPRINT_DONE");
			sgx_write(fd_et, &buf_len, sizeof(int));
			sgx_write(fd_et, "GET_FINGERPRINT_DONE", buf_len+1);

			sgx_write(fd_et, fingerprint, FINGERPRINT_LEN+1);

			// Vote signing
			char *sig = NULL;
			char digest[DIGEST_LEN];

			sgx_read(fd_te, digest, DIGEST_LEN);
            
            sig = router_get_dirobj_signature(digest, DIGEST_LEN, 
                                                tmp_signing_key);

			if(!sig) {
                sgx_puts("Unable to sign networkstatus vote!\n");
				buf_len = sgx_strlen("VOTE_SIGN_ERROR");
				sgx_write(fd_et, &buf_len, sizeof(int));
                sgx_write(fd_et, "VOTE_SIGN_ERROR", buf_len+1);
                return 0;
			}

			buf_len = sgx_strlen("VOTE_SIGN_DONE");
			sgx_write(fd_et, &buf_len, sizeof(int));
			sgx_write(fd_et, "VOTE_SIGN_DONE", buf_len+1);

            vote_sig = sgx_malloc(sgx_strlen(sig)+1);
            sgx_memcpy(vote_sig, sig, sgx_strlen(sig)+1);

			sgx_free(sig);
			sgx_free(tmp_signing_key);
			sgx_free(tmp_ori_str);
			sgx_free(tmp_buf);
			continue;
		}

        // Concensus 
        if(!sgx_strncmp(tmp_buf, "CONSENSUS_START", buf_len)) {
            sgx_printf("Computing consensus of %d is started!\n", authority_num);

			// fingerprint
//            char *tmp_ori_str = key_to_string_priv(&signing_key_set);
            char *tmp_ori_str = key_to_string_priv(signing_key);
            crypto_pk_t *tmp_signing_key = crypto_pk_new();
            crypto_pk_read_private_key_from_string(tmp_signing_key, tmp_ori_str, -1);

            char fingerprint[HEX_DIGEST_LEN+1];

            if(crypto_pk_get_fingerprint(tmp_signing_key, fingerprint, 0) < 0) {
                sgx_puts("Error getting fingerprint for signing key\n");
				buf_len = sgx_strlen("GET_FINGERPRINT_ERROR");
				sgx_write(fd_et, &buf_len, sizeof(int));
                sgx_write(fd_et, "GET_FINGERPRINT_ERROR", buf_len+1);
                return 0;
            }

            sgx_puts("Consensus - Getting fingerprint finished!\n");
			buf_len = sgx_strlen("GET_FINGERPRINT_DONE");
			sgx_write(fd_et, &buf_len, sizeof(int));
			sgx_write(fd_et, "GET_FINGERPRINT_DONE", buf_len+1);
			sgx_write(fd_et, fingerprint, HEX_DIGEST_LEN+1);

            // Consensus signing
            char *sig = NULL;
            char digest[DIGEST256_LEN];
			int digest_len;

			sgx_read(fd_te, &digest_len, sizeof(int));
			sgx_read(fd_te, digest, digest_len);

            sig = router_get_dirobj_signature(digest, 
                                            digest_len, tmp_signing_key);

            if(!sig) {
                sgx_puts("Couldn't sign consensus networkstatus\n");
				buf_len = sgx_strlen("CONSENSUS_SIGN_ERROR");
				sgx_write(fd_et, &buf_len, sizeof(int));
                sgx_write(fd_et, "CONSENSUS_SIGN_ERROR", buf_len+1);
                return 0;
            }

            sgx_puts("Consensus signing finished!\n");
			buf_len = sgx_strlen("CONSENSUS_SIGN_DONE");
			sgx_write(fd_et, &buf_len, sizeof(int));
			sgx_write(fd_et, "CONSENSUS_SIGN_DONE", buf_len+1);

			int sig_len = sgx_strlen(sig);
			sgx_write(fd_et, &sig_len, sizeof(int));
			sgx_write(fd_et, sig, sig_len+1);

            if(consensus_sig_count == 0) {
                if(consensus_sig != NULL)
                    sgx_free(consensus_sig2);
                consensus_sig2 = sgx_malloc(sgx_strlen(sig)+1);
                sgx_memcpy(consensus_sig2, sig, sgx_strlen(sig)+1);
                consensus_sig_count++;
            }
            else {
                if(consensus_sig != NULL)
                    sgx_free(consensus_sig);
                consensus_sig = sgx_malloc(sgx_strlen(sig)+1);
                sgx_memcpy(consensus_sig, sig, sgx_strlen(sig)+1);
                consensus_sig_count = 0;
            }

			sgx_free(sig);
			sgx_free(tmp_ori_str);
			sgx_free(tmp_signing_key);
			sgx_free(tmp_buf);

			continue;
		}

        // Concensus sig verify
        if(!sgx_strncmp(tmp_buf, "CONSENSUS_SIG_VERIFY", buf_len)) {
            sgx_puts("Consensus verification\n");

            int sig_len;
            char signature[1536];

            sgx_read(fd_te, &sig_len, sizeof(int));

            int i;
            for(i=0;i<3;i++)
                sgx_read(fd_te, signature+i*512, 512);

            char *tmp_str1 = sgx_memchr(signature+280, '-', 500);
            sgx_printf("%s\n", tmp_str1);

            char *tmp_str2 = sgx_memchr(signature+800, '-', 500);
            sgx_printf("%s\n", tmp_str2);

            if(sgx_strncmp(tmp_str1, consensus_sig, sgx_strlen(consensus_sig))) {
                buf_len = sgx_strlen("CONSENSUS_SIG_VERIFY_ERROR");
                sgx_write(fd_et, &buf_len, sizeof(int));
                sgx_write(fd_et, "CONSENSUS_SIG_VERIFY_ERROR", buf_len+1);
            }

            if(sgx_strncmp(tmp_str2, consensus_sig2, sgx_strlen(consensus_sig2))) {
                buf_len = sgx_strlen("CONSENSUS_SIG_VERIFY_ERROR");
                sgx_write(fd_et, &buf_len, sizeof(int));
                sgx_write(fd_et, "CONSENSUS_SIG_VERIFY_ERROR", buf_len+1);
            }

            buf_len = sgx_strlen("CONSENSUS_SIG_VERIFY_DONE");
            sgx_write(fd_et, &buf_len, sizeof(int));
            sgx_write(fd_et, "CONSENSUS_SIG_VERIFY_DONE", buf_len+1);

            sgx_free(tmp_buf);
            continue;
        }
	}

	return 1;
}

/* main operation. communicate with tor-gencert & tor process */
//int main(int argc, char *argv[])
void enclave_main()
{
    int fd_et = -1;
    int fd_te = -1;

    key_enc_to_tor = 3333;
    key_tor_to_enc = 4444;
    authority_num = 2;
/*
	if(argc != 3) {
		printf("Usage: ./enclave_process [KEY_ENCLAVE_TO_TOR] [KEY_TOR_TO_ENCLAVE]\n");
		exit(1);
	}
	
	key_enc_to_tor = atoi(argv[1]);
	key_tor_to_enc = atoi(argv[2]);

	if(key_enc_to_tor == 1111)	
		authority_num = 1;
	else if(key_enc_to_tor == 3333)	
		authority_num = 2;
	else if(key_enc_to_tor == 5555)
		authority_num = 3;
	else if(key_enc_to_tor == 7777)
		exit_node_num = 3;				// Set exit node as test003r
*/
// ------------------ initialization for pipe --------------- //
	if(pipe_init(0) < 0) {
        sgx_puts("Error in pipe_init");
        sgx_exit(NULL);
	}

	if((fd_et = pipe_open(key_enc_to_tor, RB_MODE_WR, 0)) < 0) {
        sgx_puts("Error in pipe_open");
        sgx_exit(NULL);
	}

	if((fd_te = pipe_open(key_tor_to_enc, RB_MODE_RD, 0)) < 0) {
        sgx_puts("Error in pipe_open");
        sgx_exit(NULL);
	}

// ------------------- chutney configure ------------------- //

	int retval = 0;

	retval = directory_configure(fd_te, fd_et);

	if(retval == 0) {
		sgx_puts("Error occurred. Quit program\n");
		sgx_exit(NULL);
	}

    sgx_close(fd_et);
    sgx_close(fd_te);

// ------------------- chutney start ------------------- //

    key_enc_to_tor = 3434;
    key_tor_to_enc = 4343;

	if(pipe_init(1) < 0) {
        perror("Error in pipe_init");
        exit(1);
	}

	if((fd_et = pipe_open(key_enc_to_tor, RB_MODE_WR, 1)) < 0) {
        perror("Error in pipe_open");
        exit(1);
	}

	if((fd_te = pipe_open(key_tor_to_enc, RB_MODE_RD, 1)) < 0) {
        perror("Error in pipe_open");
        exit(1);
	}

    retval = directory_request(fd_et, fd_te);

    if(retval == 0) 
        sgx_printf("Error occurred. Quit program\n");

	sgx_close(fd_et);
	sgx_close(fd_te);

    sgx_exit(NULL);
}
