#include "sgx-tor-trampoline.h"

#include <sgx-user.h>
#include <sgx-crypto.h>
#include <sgx-malloc.h>
#include <sgx-lib.h>

int main(int argc, char **argv)
{
    if(!sgx_init_tor())
        err(1, "failed to init sgx");

    char *binary;
    char *size;
    char *offset;
    char *code_start;
    char *code_end;
    char *data_start;
    char *data_end;
    char *entry;
    FILE *fp = NULL;
    unsigned char *buffer;
    long nbytes;
    int n;
    unsigned char *codes;

    binary     = argv[1];
    size       = argv[2];
    offset     = argv[3];
    code_start = argv[4];
    code_end   = argv[5];
    data_start = argv[6];
    data_end   = argv[7];
    entry      = argv[8];

    nbytes = atoi(size);
    buffer = malloc(nbytes);
    memset(buffer, 0, nbytes);

    fp = fopen(binary, "rb");
    if (fp == NULL)
        return 0;

    n = fread(buffer, nbytes, 1, fp);
    // XXX: fread will return 0 when MAX_BUFFER_SIZE > BINARY SIZE
	if (n < 0)
        return 0;

    long ecode_size = strtol(code_end, NULL, 16) - strtol(code_start, NULL, 16);
    long edata_size = strtol(data_end, NULL, 16) - strtol(data_start, NULL, 16);
    int ecode_page_n = ((ecode_size - 1) / PAGE_SIZE) + 1;
    int edata_page_n = ((edata_size - 1) / PAGE_SIZE) + 1;
    int n_of_pages = ecode_page_n + edata_page_n;
    long code_offset = strtol(offset, NULL, 16);
    long entry_offset = strtol(entry, NULL, 16) - strtol(code_start, NULL, 16);
    long edata = strtol(data_start, NULL, 16);

    printf("ecode_size: %ld edata_size: %ld offset: %lx entry_offset: %lx\n", ecode_size,
                                                                              edata_size, code_offset, entry_offset);

    codes = (unsigned char *)memalign(PAGE_SIZE, n_of_pages * PAGE_SIZE);
    memset(codes, 0, n_of_pages * PAGE_SIZE);
    memcpy(codes, buffer + code_offset, n_of_pages * PAGE_SIZE);

    tcs_t *tcs = test_init_enclave(codes, entry_offset, n_of_pages);
    if (!tcs)
        err(1, "failed to run enclave");
	return 0;
}
