/*
 *  Copyright (C) 2015, OpenSGX team, Georgia Tech & KAIST, All Rights Reserved
 *
 *  This file is part of OpenSGX (https://github.com/sslab-gatech/opensgx).
 *
 *  OpenSGX is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenSGX is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenSGX.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <sgx-kern.h>
#include <sgx-user.h>
#include <sgx-utils.h>
#include <sgx-crypto.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <sgx-malloc.h>

#include <sgx-lib.h>

#define is_aligned(addr, bytes) \
     ((((uintptr_t)(const void *)(addr)) & (bytes - 1)) == 0)

int main(int argc, char **argv)
{
    char *binary;
    char *size;
    char *offset;
    char *code_start;
    char *code_end;
    char *data_start;
    char *data_end;
    char *entry;
    char *base_addr;

    binary     = argv[1];
    size       = argv[2];
    offset     = argv[3];
    code_start = argv[4];
    code_end   = argv[5];
    data_start = argv[6];
    data_end   = argv[7];
    entry      = argv[8];

    long ecode_size = strtol(code_end, NULL, 16) - strtol(code_start, NULL, 16);
    long edata_size = strtol(data_end, NULL, 16) - strtol(data_start, NULL, 16);
    int ecode_page_n = ((ecode_size - 1) / PAGE_SIZE) + 1;
    int edata_page_n = ((edata_size - 1) / PAGE_SIZE) + 1;
    int n_of_pages = ecode_page_n + edata_page_n;
    long entry_offset = strtol(entry, NULL, 16) - strtol(code_start, NULL, 16);

    printf("ecode_size: %ld edata_size: %ld entry_offset: %lx\n", ecode_size,
                                                                  edata_size, entry_offset);

    long code_offset = strtol(offset, NULL, 16);
    int binary_size = atoi(size);

    if(!sgx_init())
        err(1, "failed to init sgx");
    base_addr = OpenSGX_loader(binary, binary_size, code_offset, n_of_pages);

    tcs_t *tcs = init_enclave(base_addr, entry_offset, n_of_pages, argv[9]);
    if (!tcs)
        err(1, "failed to run enclave");

    void (*aep)() = exception_handler;
    sgx_enter(tcs, aep);

    char *buf = malloc(11);
    sgx_host_read(buf, 11);
    printf("%s", buf);

//    free(tcs);
    return 0;
}
