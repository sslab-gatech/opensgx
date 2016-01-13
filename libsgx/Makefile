LIBSGX_OBJS = sgx-basics.o sgx-attest.o sgx-intra-attest.o sgx-remote-attest.o

POLARSSL_OBJS = polarssl/rsa.o polarssl/entropy.o polarssl/ctr_drbg.o \
                polarssl/bignum.o polarssl/md.o polarssl/oid.o polarssl/asn1parse.o \
				polarssl/sha1.o polarssl/sha512.o polarssl/aes.o polarssl/entropy_poll.o \
                polarssl/aesni.o polarssl/timing.o polarssl/md_wrap.o polarssl/sha256.o \
                polarssl/md5.o polarssl/ripemd160.o polarssl/net.o polarssl/aes_cmac128.o

BASE_CFLAGS = -g -Iinclude -Imusl-libc/include -I../user/share/include \
              -Wall -pedantic -Wno-unused-function -std=gnu11
ENC_CFLAGS = -fno-stack-protector -static -fvisibility=hidden
CFLAGS = $(BASE_CFLAGS) $(ENC_CFLAGS)
ASFLAGS = -D__ASSEMBLY__ -Iinclude -Wall -fno-stack-protector -static -fPIC

TOP = $(notdir $(basename $(PWD)))
ifeq ($(TOP),libsgx)
	TOP = $(PWD)
else
	TOP = $(PWD)/libsgx
endif

ALL = libc-sgx.a libsgx.a libpolarssl-sgx.a sgx-entry.o

all: $(ALL)

libc-sgx.a:
	cp ../user/share/include/sgx-shared.h musl-libc/include
	(cd musl-libc; ./configure)
	make -C musl-libc
	cp musl-libc/lib/libc.a ./libc-sgx.a

libpolarssl-sgx.a: mbedtls-config-libsgx.h
	CFLAGS="-g $(ENC_CFLAGS) -I$(TOP)/musl-libc/include -I$(TOP) -DPOLARSSL_CONFIG_FILE='<mbedtls-config-libsgx.h>'" make -C mbedtls SHARED=1 clean lib
	rm -rf include/polarssl
	mkdir -p include
	cp -r mbedtls/include/polarssl include/
	cp mbedtls-config-libsgx.h include/polarssl/config.h
	cp mbedtls/library/libpolarssl.a ./libpolarssl-sgx.a

libsgx.a: $(LIBSGX_OBJS)
	$(AR) rs $@ $^

%.o: %.c libc-sgx.a
	$(CC) -c $(CFLAGS) -o $@ $<

polarssl/%.o: polarssl/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -rf $(LIBSGX_OBJS) $(LIBC_OBJS) $(POLARSSL_OBJS) $(ALL)

