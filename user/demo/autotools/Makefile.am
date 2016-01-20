bin_PROGRAMS = hello

hello_SOURCES = hello.c

hello_CFLAGS = -g -nostdinc -Wall -pedantic -Wno-unused-function	\
-std=gnu11 -I@LIBSGXDIR@/musl-libc/include -I@LIBSGXDIR@/include	\
-I@LIBSGXDIR@/../user/include -fno-stack-protector -static			\
-fvisibility=hidden

hello_LDFLAGS = -static -nostdlib -nostartfiles		\
-Wl,-T,@LIBSGXDIR@/../user/sgx.lds -L@LIBSGXDIR@

hello_LDADD = @LIBSGXDIR@/sgx-entry.o -lsgx -lc-sgx -lpolarssl-sgx
