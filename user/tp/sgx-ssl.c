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

// test openssl api

#include <sgx-lib.h>
#include <sgx.h>

#include "tp-lib.h"

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>

/*char cert_s[] =
"-----BEGIN CERTIFICATE-----\n\
MIIDeTCCAmGgAwIBAgIJALF5iN1q6Iz2MA0GCSqGSIb3DQEBBQUAMFMxCzAJBgNV\n\
BAYTAlVTMQwwCgYDVQQIDANBVEwxDDAKBgNVBAcMA0FUTDELMAkGA1UECgwCR1Qx\n\
DjAMBgNVBAsMBUdUSVNDMQswCQYDVQQDDAJNUzAeFw0xNTA3MTkyMDM0MzdaFw0x\n\
NjA3MTgyMDM0MzdaMFMxCzAJBgNVBAYTAlVTMQwwCgYDVQQIDANBVEwxDDAKBgNV\n\
BAcMA0FUTDELMAkGA1UECgwCR1QxDjAMBgNVBAsMBUdUSVNDMQswCQYDVQQDDAJN\n\
UzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL0772bGXvrh8Ufm5ebE\n\
9UhWhQFcYZUTaKyAkdVT56GK14cS3/CkDBq43puQ6hL8ml1RxLl/0nedzv/fWRzm\n\
K4+OHJI76tYzwy7M8vlSKvy/Xk4j9N79YB1cajkHe8UCb8WcPPOaQoix1W5stlyz\n\
7oJZ2UORjn+OXef+dB8Szgxa9toX7NTmix8VnHfKmpz17o3I8+fJZdNBvRrAkSRN\n\
qNJdMaYcMoHw50vKW9Ik6+kWY/6JllPQ6zTrIIk+wgRfXAAplRC4Ak45e5HlxbtR\n\
lw4Lts3G5WrgIJ/oRAMH7JdSPSjnMKxbA3MDkiQ5SsvrK24MzObXOOkB+9Ftilh4\n\
BKcCAwEAAaNQME4wHQYDVR0OBBYEFH3RpUqPPmr8bU2E37DfcbfRSpokMB8GA1Ud\n\
IwQYMBaAFH3RpUqPPmr8bU2E37DfcbfRSpokMAwGA1UdEwQFMAMBAf8wDQYJKoZI\n\
hvcNAQEFBQADggEBAIixbI665NnO0Oeyn6ImYgl9rr5YIIRORNBFxRzZkwmJQduN\n\
cudwhT3wBbtPpNLPlxV0zI31/VwA1UNmE4tY7bKdgZCZMV7MrnyCiyDgCNT0R+8r\n\
eqgg5i4NvoXp5uqPwelwSkydAm3zFoXovBiuRybonoxe8mne7pTet/UVH3uqwLhy\n\
s0yxx9zUSJB1LQRSQcgLtKt/g9rr6O/tHYsYCZwjq+9/5QOey5eVNTT9gZPVhjif\n\
7i2w23XORpQRAQaU6YAttl2vV90aHYR3K9eKjrjwdqrq/6+UnruZ8kwK8m9B+50G\n\
xQUzytmqxBvJFABH4nN2q33+aVoSuZexedS7Mt4=\n\
-----END CERTIFICATE-----";

char pkey_s[] =
"-----BEGIN PRIVATE KEY-----\n\
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQC9O+9mxl764fFH\n\
5uXmxPVIVoUBXGGVE2isgJHVU+ehiteHEt/wpAwauN6bkOoS/JpdUcS5f9J3nc7/\n\
31kc5iuPjhySO+rWM8MuzPL5Uir8v15OI/Te/WAdXGo5B3vFAm/FnDzzmkKIsdVu\n\
bLZcs+6CWdlDkY5/jl3n/nQfEs4MWvbaF+zU5osfFZx3ypqc9e6NyPPnyWXTQb0a\n\
wJEkTajSXTGmHDKB8OdLylvSJOvpFmP+iZZT0Os06yCJPsIEX1wAKZUQuAJOOXuR\n\
5cW7UZcOC7bNxuVq4CCf6EQDB+yXUj0o5zCsWwNzA5IkOUrL6ytuDMzm1zjpAfvR\n\
bYpYeASnAgMBAAECggEBAJEtdTqM//tL8lcuXrzP6DoqHnpvzHGZZVnKfZeDepZl\n\
tXpsUaAFyz+JC0egQuR0JV1oyRtjZC3exRtq16wjLdJBvcu26jCRn7G9DL+YeWBU\n\
1N3wGgIls2JzLw83nY4Ek4mwltZxR5Alls/T2Yh/hoE8u0FqXz+fEo5UmfPo8mlo\n\
4JCwQ0ZfRSMq8c8+ym+KmzaqD4BEUYrQ2c3Kjz5DeGBsmhnd+SiGttqwQhUJI6lC\n\
dUr9F6tqlT22oQs81oVnDFhEd2o8AOipl4FAVOadTtslEiSd3vcHit+Tv1QMcurK\n\
9fxe6qO3rVTjXSWTRAuu/Rk3fKsiY0wHymiQX78ZAlECgYEA9LhoVpWxAH5WvZ/n\n\
N7ySfQT4G1QkX+9WID7/Xb7+tNaFcffXVveI3Aw2jhdeasZ6rhvhE+uMpFC4BBQs\n\
g3Skk7kdoWW+x7oOtCl18m1tb4HVqJbydcwVbvbCQqe4tNGPKL81s9sR/foUSq3W\n\
1Wb9hIKl51kx4OejrJvV5rv4a6kCgYEAxfTQiOYMLMMiPj5SpjYJ+GJALtTo1r8h\n\
spDQmPlNT3B9vfezEeqRGwzzfGZv7GsD3xRDfSpbVqX7KTJ4At3OZXvbjDag7Lb9\n\
s8aM8tJMA/Hrzay+8QNqDRB4Cp091yc9uiCK/r8d6R/o6M4VypZEAakpmDQtGow4\n\
ffAqH17Dn88CgYACervT5MBeWOlYEn6tOePiEGLTJA8aSbp9qSW9NWovOt0vQmuU\n\
Hf3s/NKwrdvvCQPFL9Mf+Ir8PzUeFXY9/riMJpv+PqGoNGJhwOnJAwLQ1mfrxVu/\n\
hcnRzf453qkoa9cfepB4ugd8o0QBXbGHh/uZlBlKNsUimjY2UX3hUJY7wQKBgBTw\n\
NC6GxrrlWPXkebGC/cL8AXXypz4vsUzF7IqBtB+28lXpoVM/0GFgGVELNMN4/kvW\n\
RN00YbzkiZBt+iuzcmVhmHNYQ+VF5cfiNH5qqAeyyxJSN1Ae4FTJbSkSQLxFNk49\n\
qvYPGQbTjTsysTNgeYkDb5bdzfeE5WTMxmfTCjYZAoGBALYTqNPBNZC1NGyMAOva\n\
eJliaK9mFyZMMJ9ek9ZICmePAVsLT22XqR8vxXbuw8n8c67Hg+PJV9urj+oftNwf\n\
33PWgJtung/wQru34BB6i0vxyOyZLNSvWeJDDEZMjiXpbuKG2sUA9alHmXv/UW0O\n\
TLnPxZ+pbmC6Rqn73Ayz6FG4\n\
-----END PRIVATE KEY-----";*/

char cert_s[] =
"-----BEGIN CERTIFICATE-----\n\
MIICWDCCAcGgAwIBAgIJAOZBf8IQDFPOMA0GCSqGSIb3DQEBBQUAMEUxCzAJBgNV\n\
BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\n\
aWRnaXRzIFB0eSBMdGQwHhcNMTUwNzI0MjI0NTAxWhcNMTYwNzIzMjI0NTAxWjBF\n\
MQswCQYDVQQGEwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50\n\
ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKB\n\
gQCmeRN8GRoVY/Y2n+qzCahOMF9YXavR9ePR8/Rrmvb/6uYi7uSzXijVvenCr3zB\n\
ktjc1aIonscilirMMAj985bwIiPnU4TF/G6H4QcLVLPDb/AdktKHBFtPO0+W82tK\n\
B8t/E60MaDRTMHdiiTd3NL+nYpZZdbZMyuXuM/Fqtf/2PQIDAQABo1AwTjAdBgNV\n\
HQ4EFgQU5dj19ll8RiZlHev6rDfZtGmIscMwHwYDVR0jBBgwFoAU5dj19ll8RiZl\n\
Hev6rDfZtGmIscMwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQUFAAOBgQAX+zwt\n\
yYYP0OnKLymNg2raikLQeYKNX5C95wdtER3Dhgml+IV09xKSS5TrNP1m099sItIF\n\
Da6++sNmSNfqPzIlRMfL9nCLYbkGaIqn0HCQCRgxVHskKD0VS1LwV6WFOs12Hbtq\n\
cI/UkETmTJ/AYS5Ur2QGW8fcoxK5CoiSTrKzXg==\n\
-----END CERTIFICATE-----";

char pkey_s[] =
"-----BEGIN PRIVATE KEY-----\n\
MIICdwIBADANBgkqhkiG9w0BAQEFAASCAmEwggJdAgEAAoGBAKZ5E3wZGhVj9jaf\n\
6rMJqE4wX1hdq9H149Hz9Gua9v/q5iLu5LNeKNW96cKvfMGS2NzVoiiexyKWKsww\n\
CP3zlvAiI+dThMX8bofhBwtUs8Nv8B2S0ocEW087T5bza0oHy38TrQxoNFMwd2KJ\n\
N3c0v6dilll1tkzK5e4z8Wq1//Y9AgMBAAECgYEAhK0qolU/PJ0WtiJt45Nm2Col\n\
U0AUmJnooIRV4Qz4nq6QDHdpPqtk0DU0AT3rqDtpK1f4jXc+LoqQQXxnLj7do+uQ\n\
yVEUpceJ8n5eXkbR+GzCxPtIp4l6lgzV0wCXyZ96IDO21ORPNJ51TSt4NX3n154w\n\
iC9JSNtIzQgLDNC75+ECQQDZaMgz+yGtf8KM3pJocDym9FssdNshD9duinLh3QiZ\n\
YPTulcCHohvUvJTfVdbJ48utQxdBB4nEouSat2dl2/3jAkEAxAW2stvAouvR8+Tk\n\
LNdFtAehoU9EhUx9XtirCTrEbziglVphlYD1nXmUc/ESDmpbvNBEXPumu1z+JSpi\n\
OJx1XwJABB7CPInz12/mZfkJ2UTXQWq8F5mXVYcRVBz3lGQ194Io4iSgY3GlCWER\n\
iTH9QhI5F+1/kVHtQHa90ljLcti6dQJALfXA/nKz2f88vNif/zuCJlHbvbyeLjre\n\
8kwO3h0fTYyTajFEzA7uh7un6P9O4n5hgAW84ahHUYreM8yaNvfINQJBAKgQwbQN\n\
oJq3MJ2x0iTSGXLnMCu2BgSHc388QJoOG60a5tba5LUY3wvWxv9wWgT1rql+e/tK\n\
br4cl4OWl6JxaTs=\n\
-----END PRIVATE KEY-----";


void enclave_main()
{
    SSL_METHOD *method;
    SSL_CTX *ctx;
    BIO *bio_cert;
    BIO *bio_pkey;
    X509 *cert = NULL;
    EVP_PKEY *pkey = NULL;
    int port = 5566;
    int srvr_fd;
    int clnt_fd;
    struct sockaddr_in addr;

    // Initialize ssl
    SSL_library_init();

    // Initialize ctx
    OpenSSL_add_all_algorithms();
    method = TLSv1_2_server_method();
    ctx = SSL_CTX_new(method);

    // Load certificate
    bio_cert = BIO_new(BIO_s_mem());
    BIO_puts(bio_cert, cert_s);

    cert = PEM_read_bio_X509(bio_cert, NULL, 0, NULL);

    if (cert == NULL) {
        sgx_debug("cert is NULL");
    } else {
        sgx_debug("cert is not NULL");
    }

    bio_pkey = BIO_new(BIO_s_mem());
    BIO_puts(bio_pkey, pkey_s);

    pkey = PEM_read_bio_PrivateKey(bio_pkey, NULL, 0, NULL);

    if (pkey == NULL) {
        sgx_debug("key is NULL\n");
    } else {
        sgx_debug("key is not NULL\n");
    }

    if (SSL_CTX_use_certificate(ctx, cert) <= 0) {
    	sgx_debug("SSL_CTX_use_certificate failed\n");
        //sgx_exit(NULL);
    } else {
        sgx_debug("SSL_CTX_use_certificate succeeded\n");
    }

    if (SSL_CTX_use_PrivateKey(ctx, pkey) <= 0) {
    	sgx_debug("SSL_CTX_use_PrivateKey failed\n");
        //sgx_exit(NULL);
    } else {
    	sgx_debug("SSL_CTX_use_PrivateKey succeeded\n");
    }

    SSL_CTX_set_ecdh_auto(ctx, 1);
    SSL_CTX_set_options(ctx, SSL_OP_SINGLE_ECDH_USE);

    srvr_fd = sgx_socket(PF_INET, SOCK_STREAM, 0);

    if (srvr_fd == -1) {
        sgx_exit(NULL);
    }

    sgx_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = sgx_htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (sgx_bind(srvr_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        sgx_exit(NULL);
    }

    if (sgx_listen(srvr_fd, 10) != 0) {
        sgx_exit(NULL);
    }

    while (1) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        SSL *ssl;
        int sd;
        int bytes;
        char buf[256];
        const char* echo="<html><body><pre>%s</pre></body></html>\n\n";

        clnt_fd = sgx_accept(srvr_fd, (struct sockaddr *)&addr, &len);
        if (clnt_fd < 0) {
            sgx_puts("ERROR on accept\n");
            continue;
        }

        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, clnt_fd);
        if (SSL_accept(ssl) == -1)
            sgx_puts("SSL accept failed\n");
        else {
            sgx_puts("SSL accept succeeded\n");

            bytes = SSL_read(ssl, buf, sizeof(buf));
            if (bytes > 0) {
                buf[bytes] = 0;
                sgx_puts(buf);
                char msg[] = "Successfully connected\n";
                SSL_write(ssl, msg, sgx_strlen(msg));
            }
        }

        sd = SSL_get_fd(ssl);
        //SSL_free(ssl);
        sgx_close(sd);
        //sgx_close(clnt_fd);
    }

    sgx_close(srvr_fd);

    sgx_exit(NULL);
}
