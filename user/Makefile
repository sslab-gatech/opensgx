BASE_CFLAGS = -g -Iinclude -Ishare/include -Wall -pedantic -Wno-unused-function -std=gnu11

# SGX LIB
LIBSGXDIR = ../libsgx
SGX_CFLAGS = $(BASE_CFLAGS) -I$(LIBSGXDIR)/include -I$(LIBSGXDIR)/musl-libc/include \
             -fno-stack-protector -static -fvisibility=hidden
SGX_LDFLAGS = -static -nostdlib -nostartfiles -Wl,-T,sgx.lds
SGX_LDFLAGS2 = -static -nostdlib -nostartfiles -Wl,-T,sgx2.lds
SGX_LIBS = $(LIBSGXDIR)/sgx-entry.o $(LIBSGXDIR)/libsgx.a \
           $(LIBSGXDIR)/libpolarssl-sgx.a $(LIBSGXDIR)/libc-sgx.a

# OpenSSL Cases
OPENSSL_CFLAGS = $(SGX_CFLAGS) -I$(LIBSGXDIR)/openssl/include
OPENSSL_LIBS = $(LIBSGXDIR)/sgx-entry.o $(LIBSGXDIR)/libssl.a \
               $(LIBSGXDIR)/libcrypto.a $(LIBSGXDIR)/libsgx.a \
               $(LIBSGXDIR)/libc-sgx.a

# Host code/tool
SGX_HOST_RUNTIME = sgx-runtime.o sgx-host.o
SGX_HOST_OBJS = sgx-user.o sgx-kern.o sgx-kern-epc.o sgx-utils.o sgx-trampoline.o \
                sgx-crypto.o sgx-loader.o
POLARSSL_LIB = libpolarssl.a
POLARSSL_OBJS = polarssl/rsa.o polarssl/entropy.o polarssl/ctr_drbg.o \
	            polarssl/bignum.o polarssl/md.o polarssl/oid.o polarssl/asn1parse.o \
                polarssl/sha1.o polarssl/sha512.o polarssl/aes.o polarssl/entropy_poll.o \
                polarssl/aesni.o polarssl/timing.o polarssl/md_wrap.o polarssl/sha256.o \
                polarssl/md5.o polarssl/ripemd160.o polarssl/net.o polarssl/aes_cmac128.o
LDLIBS = -L. -lpolarssl -lelf

CFLAGS := $(BASE_CFLAGS) -fno-stack-protector -fvisibility=hidden

HDRS := $(wildcard include/sgx*.h)
BINS := $(patsubst %.c,%,$(wildcard test/*.c)) \
        $(patsubst %.c,%,$(wildcard test/openssl/*.c)) \
        $(patsubst %.c,%,$(wildcard test/tor/sgx-*.c)) \
        $(patsubst %.c,%,$(wildcard non_enclave/*.c))
ALL  := sgx-tool sgx-runtime $(BINS) $(POLARSSL_LIB) sgx-host

all: $(ALL)

sgx-runtime: sgx-runtime.o $(SGX_HOST_OBJS) $(POLARSSL_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

sgx-host: sgx-host.o $(SGX_HOST_OBJS) $(POLARSSL_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

libpolarssl.a : $(POLARSSL_OBJS)
	$(AR) rs $@ $^

polarssl/%.o: polarssl/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

sgx-tool: sgx-tool.o $(SGX_HOST_OBJS) $(POLARSSL_LIB)
	$(CC) $^ $(CFLAGS) -o $@ $(LDLIBS)

test/openssl/%.o: test/openssl/%.c
	$(CC) -c $(OPENSSL_CFLAGS) -o $@ $<

test/tor/%.o: test/tor/%.c
	$(CC) -c $(OPENSSL_CFLAGS) -o $@ $<

test/%.o: test/%.c
	$(CC) -c $(SGX_CFLAGS) -o $@ $<

demo/%.o: demo/%.c
	$(CC) -c $(SGX_CFLAGS) -o $@ $<

test/openssl/%: test/openssl/%.o $(OPENSSL_LIBS)
	$(CC) $(SGX_LDFLAGS) $< -o $@ $(OPENSSL_LIBS)

test/tor/%: test/tor/%.o $(OPENSSL_LIBS)
	$(CC) $(SGX_LDFLAGS) $< -o $@ $(OPENSSL_LIBS)

test/%: test/%.o $(SGX_LIBS)
	$(CC) $(SGX_LDFLAGS) $< -o $@ $(SGX_LIBS)

demo/%.sgx: demo/%.o $(SGX_LIBS)
	$(CC) $(SGX_LDFLAGS) $< -o $@ $(SGX_LIBS)

non_enclave/%: non_enclave/%.o nonEncLib.o
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f polarssl/*.o lib/*.o *.o $(ALL) test/*.o sgx-runtime.o demo/*.conf demo/*.sgx

.PHONY: polarsslobjs all clean
