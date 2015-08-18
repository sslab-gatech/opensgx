#include <sgx-lib.h>

#define is_aligned(addr, bytes) \
     ((((uintptr_t)(const void *)(addr)) & (bytes - 1)) == 0)

extern void ENCT_START;
extern void ENCT_END;
extern void ENCD_START;
extern void ENCD_END;

void enclave_start() \
    __attribute__((section(".enc_text")));
void enclave_start()
{
    enclave_main();
    sgx_exit(NULL);
}

int main(int argc, char **argv)
{
/*
    if(!sgx_init_tor())
        err(1, "failed to init sgx");

    //XXX n_of_pages should be set properly
    //n_of_pages = n_of_enc_code + n_of_enc_data
    //improper setting of n_of_pages could contaminate other EPC area
    //e.g. if n_of_pages mistakenly doesn't consider enc_data section,
    //memory write access to enc_data section could make write access on other EPC page.
    void *entry = (void *)(uintptr_t)enclave_start;
    void *codes = (void *)(uintptr_t)&ENCT_START;
    unsigned long ecode_size = (unsigned long)&ENCT_END - (unsigned long)&ENCT_START;
    unsigned long edata_size = (unsigned long)&ENCD_END - (unsigned long)&ENCD_START;
    unsigned int ecode_page_n = ((ecode_size - 1) / PAGE_SIZE) + 1;
    printf("DEBUG n_of_code_pages: %d\n", ecode_page_n);
    unsigned int edata_page_n = ((edata_size - 1) / PAGE_SIZE) + 1;
    unsigned int n_of_pages = ecode_page_n + edata_page_n;
    printf("n_of_pages: %d\n", n_of_pages);

    assert(is_aligned((uintptr_t)codes, PAGE_SIZE));

    // indirect call handling
    void *edata = (void *)(uintptr_t)&ENCD_START;

    tcs_t *tcs = test_init_enclave(entry, codes, n_of_pages, ecode_size, edata, edata_size);
    if (!tcs)
        err(1, "failed to run enclave");
*/
//    free(tcs);
        return 0;
}

