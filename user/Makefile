BASE_CFLAGS = -g -Iinclude -Ishare/include -Wall -pedantic -Wno-unused-function -std=gnu11

# SGX LIB
LIBSGXDIR = ../libsgx
SGX_CFLAGS = $(BASE_CFLAGS) -I$(LIBSGXDIR)/include -fno-stack-protector -static -fPIC -fvisibility=hidden
SGX_LDFLAGS = -static -nostdlib -Wl,-T,sgx.lds
SGX_LIBS = $(LIBSGXDIR)/libsgx.a $(LIBSGXDIR)/libpolarssl-sgx.a
SGX_LDLIBS = -L$(LIBSGXDIR) -lsgx -lpolarssl-sgx

# Host code/tool
SGX_HOST_RUNTIME = sgx-runtime.o sgx-test-runtime.o
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
        $(patsubst %.c,%,$(wildcard non_enclave/*.c))
ALL  := sgx-tool sgx-runtime $(BINS) $(POLARSSL_LIB)

all: $(ALL)

sgx-runtime: sgx-runtime.o $(SGX_HOST_OBJS) $(POLARSSL_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

libpolarssl.a : $(POLARSSL_OBJS)
	$(AR) rs $@ $^

polarssl/%.o: polarssl/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

sgx-tool: sgx-tool.o $(SGX_HOST_OBJS) $(POLARSSL_LIB)
	$(CC) $^ $(CFLAGS) -o $@ $(LDLIBS)

test/%.o: test/%.c
	$(CC) -c $(SGX_CFLAGS) -o $@ $<

demo/%.o: demo/%.c
	$(CC) -c $(SGX_CFLAGS) -o $@ $<

test/%: test/%.o $(SGX_LIBS)
	$(CC) $(SGX_LDFLAGS) $< -o $@ $(SGX_LDLIBS)

demo/%.sgx: demo/%.o $(SGX_LIBS)
	$(CC) $(SGX_LDFLAGS) $< -o $@ $(SGX_LDLIBS)

non_enclave/%: non_enclave/%.o nonEncLib.o
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f polarssl/*.o lib/*.o *.o $(ALL) test/*.o sgx-runtime.o

.PHONY: polarsslobjs all clean
