LIBSGX_OBJS = sgx-basics.o sgx-main.o

LIBC_OBJS   = libc/strchr.o libc/inet-pton.o libc/qsort.o libc/memchr.o \
			  libc/strcpy.o libc/strncpy.o libc/strcmp.o libc/strncmp.o \
              libc/memset.o libc/strlen.o libc/memcmp.o libc/strcasecmp.o \
              libc/strncase.o libc/strnlen.o libc/strcat.o libc/strncat.o

POLARSSL_OBJS = polarssl/rsa.o polarssl/entropy.o polarssl/ctr_drbg.o \
                polarssl/bignum.o polarssl/md.o polarssl/oid.o polarssl/asn1parse.o \
				polarssl/sha1.o polarssl/sha512.o polarssl/aes.o polarssl/entropy_poll.o \
                polarssl/aesni.o polarssl/timing.o polarssl/md_wrap.o polarssl/sha256.o \
                polarssl/md5.o polarssl/ripemd160.o polarssl/net.o polarssl/aes_cmac128.o

BASE_CFLAGS = -g -Iinclude -I../user/share/include -Wall -pedantic -Wno-unused-function -std=gnu11
ENC_CFLAGS = -fno-stack-protector -static -fPIC -fvisibility=hidden
CFLAGS = $(BASE_CFLAGS) $(ENC_CFLAGS)

all: libsgx.a libpolarssl-sgx.a

libsgx.a: $(LIBSGX_OBJS) $(LIBC_OBJS)
	$(AR) rs $@ $^

libpolarssl-sgx.a: $(POLARSSL_OBJS)
	$(AR) rs $@ $^

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

libc/%.o: libc/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

polarssl/%.o: polarssl/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -rf $(LIBSGX_OBJS) $(LIBC_OBJS) $(POLARSSL_OBJS)

