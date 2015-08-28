SGX_LIBS := sgxLib.o sslLib.a
SGX_RUNTIME := sgx-runtime.o sgx-test-runtime.o
SGX_OBJS := sgx-user.o sgx-kern.o sgx-kern-epc.o sgx-utils.o sgx-trampoline.o sgx-crypto.o

SSL_OBJS := polarssl/rsa.o polarssl/entropy.o polarssl/ctr_drbg.o \
	polarssl/bignum.o polarssl/md.o polarssl/oid.o polarssl/asn1parse.o polarssl/sha1.o \
	polarssl/sha512.o polarssl/aes.o polarssl/entropy_poll.o polarssl/aesni.o \
	polarssl/timing.o polarssl/md_wrap.o polarssl/sha256.o polarssl/md5.o \
	polarssl/ripemd160.o polarssl/net.o polarssl/aes_cmac128.o

LIB_OBJS := lib/sgx-strchr.o lib/sgx-inet-pton.o lib/sgx-qsort.o lib/sgx-memchr.o \
    lib/sgx-strcpy.o lib/sgx-strncpy.o lib/sgx-strcmp.o lib/sgx-strncmp.o lib/sgx-memset.o \
    lib/sgx-strlen.o lib/sgx-memcmp.o lib/sgx-strcasecmp.o lib/sgx-strncase.o lib/sgx-strnlen.o \
    lib/sgx-strcat.o lib/sgx-strncat.o lib/sgx-attest.o

SSL_SGX_OBJS = polarssl_sgx/bignum.o polarssl_sgx/entropy.o polarssl_sgx/sha256.o polarssl_sgx/entropy_poll.o \
               polarssl_sgx/timing.o polarssl_sgx/ctr_drbg.o polarssl_sgx/aes.o polarssl_sgx/dhm.o \
               polarssl_sgx/rsa.o polarssl_sgx/aes_cmac128.o polarssl_sgx/sha1.o polarssl_sgx/md.o \
               polarssl_sgx/sha256.o

CFLAGS := -g -Iinclude -Iopenssl/include -Wall -pedantic -Wno-unused-function -std=gnu1x -fno-stack-protector -fvisibility=hidden

HDRS := $(wildcard include/sgx*.h)
BINS := $(patsubst %.c,%,$(wildcard test/*.c)) \
        $(patsubst %.c,%,$(wildcard test/test_kern/*.c)) \
        $(patsubst %.c,%,$(wildcard non_enclave/*.c))
ALL  := $(BINS) sgx-tool sgx-test-runtime sgx-runtime

all: $(ALL)

#include tor/tor.mak

#include tp/tp.mak

sgx-runtime: sgx-runtime.o $(SGX_OBJS) $(SSL_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

sgx-test-runtime: sgx-test-runtime.o $(SGX_OBJS) $(SSL_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

polarssl/%.o: polarssl/%.c
	$(CC) -c  $(CFLAGS) -o $@ $<

polarssl_sgx/%.o: polarssl_sgx/%.c
	$(CC) -c  -I./polarssl_sgx/include -fPIC -Wall -o $@ $<

#lib/%.o: lib/%.c sgxLib.o $(SSL_SGX_OBJS)
lib/%.o: lib/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.c $(HDRS)
	$(CC) -c $(CFLAGS) -o $@ $<

sgx-tool: sgx-tool.o $(SGX_OBJS) $(SSL_OBJS)
	$(CC) $^ $(CFLAGS) -o $@

sgx-%.o: sgx-%.c $(HDRS)
	$(CC) -c $(CFLAGS) $< -o $@

example1: example1.S
	$(CC) -nostdlib $< -o $@

SGX_LIBS: sgxLib.c
	$(CC) $(CFLAGS) -c -o $@ $^

test/test_kern/%: test/test_kern/%.o $(SGX_OBJS) $(SSL_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

test/%: test/%.o $(SGX_OBJS) $(SSL_OBJS) $(SGX_LIBS) $(LIB_OBJS) $(SSL_SGX_OBJS) sgx-main.o
	$(CC) $(CFLAGS) -Wl,-T,sgx.lds $^ -o $@

demo/%.sgx: demo/%.o $(SGX_OBJS) $(SSL_OBJS) $(SGX_LIBS) $(LIB_OBJS) $(SSL_SGX_OBJS) sgx-main.o
	$(CC) $(CFLAGS) -Wl,-T,sgx.lds $^ -o $@

non_enclave/%: non_enclave/%.o nonEncLib.o
	$(CC) $(CFLAGS) $^ -o $@

#clean: tp_clean tor_clean
clean:
	rm -f polarssl/*.o lib/*.o *.o $(ALL) test2/*.o sgx-runtime.o

.PHONY: polarsslobjs all clean
