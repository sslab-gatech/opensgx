#pragma once

extern void *load_elf_enclave(char *filename, size_t *npages, void **entry, int *offset);
