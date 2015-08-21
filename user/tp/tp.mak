TP_LIBS := tp/tpLib.o sgxLib.o tp/ssl/*.o tp/crypto/*.o polarssl_sgx/*.o
TP_OBJS := tp/tp-trampoline.o  tp/tp-runtime.o
TP_BINS := $(patsubst %.c, %, $(wildcard tp/sgx-*.c))
TP_ALL := $(TP_BINS)

all: $(TP_ALL) tp/loader

tp/loader: tp/loader.o $(SGX_OBJS) $(SSL_OBJS) tp/tp-trampoline.o
	$(CC) $(CFLAGS) $^ -o $@

tp/sgx-%: tp/sgx-%.o $(SGX_OBJS) $(SSL_OBJS) $(TP_LIBS) $(TP_OBJS) $(LIB_OBJS) $(SSL_SGX_OBJS) 
	$(CC) $(CFLAGS) -static -Wl,-T,tp/tp.lds $^ -o $@

tp/sgx-%.o: tp/sgx-%.c $(SGX_LIBS) $(SGX_OBJS) $(LIB_OBJS)
	$(CC) $(CFLAGS) -c $^ -o $@

tp/tp%.o: tp/tp%.c $(SGX_LIBS) $(SGX_OBJS)
	$(CC) $(CFLAGS) -c $^ -o $@

tp/loader.o: tp/loader.c $(SGX_LIBS) $(SGX_OBJS) tp/tp-trampoline.o
	$(CC) $(CFLAGS) -c $^ -o $@

tp_clean:
	rm -f tp/*.o $(TP_ALL)
