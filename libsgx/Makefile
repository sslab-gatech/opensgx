LIBSGX_OBJS = sgx-basics.o sgx-main.o

LIBC_OBJS   = libc/sgx_strchr.o libc/sgx_inet-pton.o libc/sgx_qsort.o libc/sgx_memchr.o \
              libc/sgx_strcpy.o libc/sgx_strncpy.o libc/sgx_strcmp.o libc/sgx_strncmp.o \
              libc/sgx_memset.o libc/sgx_strlen.o libc/sgx_memcmp.o libc/sgx_strcasecmp.o \
              libc/sgx_strncase.o libc/sgx_strnlen.o libc/sgx_strcat.o libc/sgx_strncat.o

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

