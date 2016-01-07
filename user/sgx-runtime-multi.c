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

/* This is used for testing multiple enclaves */
#include <string.h>
#include <sgx-kern.h>
#include <sgx-user.h>
#include <sgx-utils.h>
#include <sgx-crypto.h>
#include <sgx-loader.h>
#include <sgx-trampoline.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <sgx-malloc.h>
#include <err.h>
#include <string.h>
#include <fcntl.h>

#define is_aligned(addr, bytes) \
     ((((uintptr_t)(const void *)(addr)) & (bytes - 1)) == 0)

int main(int argc, char **argv)
{
    char *binary1, *binary2;
    char *conf1, *conf2;
    void *entry1, *entry2;
    void *base_addr1, *base_addr2;
    size_t npages1, npages2;
    unsigned long entry_offset1, entry_offset2;
    int toff1, toff2;

    if (argc < 1) {
        err(1, "Please specifiy binary to load\n");
    }
    binary1 = argv[1];

    if (argc > 1) {
        conf1 = argv[2];
    } else {
        conf1 = NULL;
    }

    if(!sgx_init())
         err(1, "failed to init sgx");

    base_addr1 = load_elf_enclave(binary1, &npages1, &entry1, &toff1);
    if (base_addr1 == NULL) {
         err(1, "Please provide valid binary/configuration files.");
    }

    entry_offset1 = (uint64_t)entry1 - (uint64_t)base_addr1;
    tcs_t *tcs = init_enclave(base_addr1, entry_offset1, npages1, conf1);
    if (!tcs)
        err(1, "failed to run enclave");

    void (*aep)() = exception_handler;
    sgx_enter(tcs, aep);

    if (argc > 3) {
        binary2 = argv[3];
    } else {
        goto end;
    }

    if (argc > 4) {
        conf2 = argv[4];
    } else {
        conf2 = NULL;
    }

    base_addr2 = load_elf_enclave(binary2, &npages2, &entry2, &toff2);
    if (base_addr2 == NULL) {
        err(1, "Please provide valid binary/configuration files.");
    }

    entry_offset2 = (uint64_t)entry2 - (uint64_t)base_addr2;
    tcs = init_enclave(base_addr2, entry_offset2, npages2, conf2);
    if (!tcs)
        err(1, "failed to run enclave 2");

    printf("%lx %lx\n", base_addr1, base_addr2);
    printf("%lx %lx\n", entry1, entry2);
    printf("%lx %lx\n", entry_offset1, entry_offset2);

    sgx_enter(tcs, aep);

end:
    return 0;
}
