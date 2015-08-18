ATTEST_C_LIBS := attest_c/attestLib.o sgxLib.o attest_c/polarssl_*
ATTEST_C_OBJS := attest_c/attest-trampoline.o  attest_c/attest-runtime.o
ATTEST_C_BINS := $(patsubst %.c, %, $(wildcard attest_c/sgx-*.c))
#TOR_ALL := $(TOR_LIBS) $(TOR_OBJS) 
ATTEST_C_ALL := $(ATTEST_C_BINS)

all: $(ATTEST_C_ALL)

%.o: %.c $(HDRS)
	$(CC) -c $(CFLAGS) -o $@ $<

attest_c/sgx-%: attest_c/sgx-%.o $(SGX_OBJS) $(SSL_OBJS) $(ATTEST_C_LIBS) $(ATTEST_C_OBJS) 
	$(CC) $(CFLAGS) -Wl,-T,attest_c/attest_c.lds $^ -o $@


attest_c/%.o: attest_c/%.c $(SGX_LIBS) $(SGX_OBJS)
	$(CC) $(CFLAGS) -c $^ -o $@ 

attest_c_clean:
	rm -f attest_c/*.o $(ATTEST_C_ALL)
