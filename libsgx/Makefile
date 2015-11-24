LIBSGX_OBJS = sgx-basics.o

POLARSSL_OBJS = polarssl/rsa.o polarssl/entropy.o polarssl/ctr_drbg.o \
                polarssl/bignum.o polarssl/md.o polarssl/oid.o polarssl/asn1parse.o \
				polarssl/sha1.o polarssl/sha512.o polarssl/aes.o polarssl/entropy_poll.o \
                polarssl/aesni.o polarssl/timing.o polarssl/md_wrap.o polarssl/sha256.o \
                polarssl/md5.o polarssl/ripemd160.o polarssl/net.o polarssl/aes_cmac128.o

BASE_CFLAGS = -g -Iinclude -Imusl-libc/include -I../user/share/include \
              -Wall -pedantic -Wno-unused-function -std=gnu11
ENC_CFLAGS = -fno-stack-protector -static -fvisibility=hidden
CFLAGS = $(BASE_CFLAGS) $(ENC_CFLAGS)

ALL = libc-sgx.a libsgx.a libpolarssl-sgx.a sgx-entry.o

all: $(ALL)

libc-sgx.a:
	cp ../user/share/include/sgx-shared.h musl-libc/include
	(cd musl-libc; ./configure)
	make -C musl-libc
	cp musl-libc/lib/libc.a ./libc-sgx.a

libsgx.a: $(LIBSGX_OBJS)
	$(AR) rs $@ $^

libpolarssl-sgx.a: $(POLARSSL_OBJS)
	$(AR) rs $@ $^

%.o: %.c libc-sgx.a
	$(CC) -c $(CFLAGS) -o $@ $<

polarssl/%.o: polarssl/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -rf $(LIBSGX_OBJS) $(LIBC_OBJS) $(POLARSSL_OBJS) $(ALL)

