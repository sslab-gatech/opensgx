ATTEST_LIBS := attest/attestLib.o sgxLib.o attest/polarssl_*
ATTEST_OBJS := attest/attest-trampoline.o attest/attest-runtime.o
ATTEST_BINS := $(patsubst %.c, %, $(wildcard attest/sgx-*.c))
#TOR_ALL := $(TOR_LIBS) $(TOR_OBJS)
ATTEST_ALL := $(ATTEST_BINS)

all: $(ATTEST_ALL)

%.o: %.c $(HDRS)
	$(CC) -c $(CFLAGS) -o $@ $<

attest/sgx-%: attest/sgx-%.o $(SGX_OBJS) $(SSL_OBJS) $(ATTEST_LIBS) $(ATTEST_OBJS)
	$(CC) $(CFLAGS) -Wl,-T,attest/attest.lds $^ -o $@


attest/%.o: attest/%.c $(SGX_LIBS) $(SGX_OBJS)
	$(CC) $(CFLAGS) -c $^ -o $@

attest_clean:
	rm -f attest/*.o $(ATTEST_ALL)
