SGX_LIBS := sgxLib.o sslLib.a 
SGX_RUNTIME := sgx-runtime.o sgx-test-runtime.o
SGX_OBJS := sgx-user.o sgx-kern.o sgx-kern-epc.o sgx-utils.o sgx-signature.o
SSL_OBJS := polarssl/rsa.o polarssl/entropy.o polarssl/ctr_drbg.o \
	polarssl/bignum.o polarssl/md.o polarssl/oid.o polarssl/asn1parse.o polarssl/sha1.o \
	polarssl/sha512.o polarssl/aes.o polarssl/entropy_poll.o polarssl/aesni.o \
	polarssl/timing.o polarssl/md_wrap.o polarssl/sha256.o polarssl/md5.o \
	polarssl/ripemd160.o polarssl/net.o polarssl/aes_cmac128.o


CFLAGS := -g -Iinclude -Iopenssl/include -Wall -pedantic -Wno-unused-function -std=gnu11 -fno-stack-protector -static -fPIC -fvisibility=hidden

HDRS := $(wildcard include/sgx*.h)
BINS := $(patsubst %.c,%,$(wildcard test/*.c)) \
        $(patsubst %.c,%,$(wildcard test/test_kern/*.c)) \
        $(patsubst %.c,%,$(wildcard non_enclave/*.c))
ALL  := $(BINS) sgx-tool

all: $(ALL)



polarssl/%.o: polarssl/%.c
	$(CC) -c  $(CFLAGS) -o $@ $<

%.o: %.c $(HDRS)
	$(CC) -c $(CFLAGS) -o $@ $<

sgx-tool: sgx-tool.o $(SGX_OBJS) $(SSL_OBJS)
	$(CC) $^ $(CFLAGS) -o $@

sgx-%.o: sgx-%.c $(HDRS)
	$(CC)  -c $(CFLAGS) $< -o $@

example1: example1.S
	$(CC) -nostdlib $< -o $@

SGX_LIBS: sgxLib.c
	$(CC) $(CFLAGS)  -c -o $@ $^

test/test_kern/%: test/test_kern/%.o $(SGX_OBJS) $(SSL_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

test/%: test/%.o $(SGX_OBJS) $(SSL_OBJS) $(SGX_LIBS) sgx-test-runtime.o
	$(CC) $(CFLAGS) -Wl,-T,sgx.lds $^ -o $@

demo/%.sgx: demo/%.o $(SGX_OBJS) $(SSL_OBJS) $(SGX_LIBS) sgx-runtime.o
	$(CC) $(CFLAGS) -Wl,-T,sgx.lds $^ -o $@

non_enclave/%: non_enclave/%.o nonEncLib.o
	$(CC) $(CFLAGS) $^ -o $@

clean: tor_clean
	rm -f polarssl/*.o *.o $(ALL) test2/*.o sgx-runtime.o

.PHONY: polarsslobjs all clean
