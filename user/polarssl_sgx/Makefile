OBJS = bignum.o entropy.o sha256.o entropy_poll.o timing.o ctr_drbg.o aes.o dhm.o rsa.o \
       aes_cmac128.o sha1.o md.o sha256.o
      #oid.o asn1parse.o \
       sha512.o aesni.o md_wrap.o \
       md5.o ripemd160.o pem.o des.o base64.o #net.o 

%.o: %.c
	$(CC) -c -Iinclude -fPIC -Wall -o $@ $<

all: $(OBJS)

clean: 
	rm -f *.o 

.PHONY: clean
