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
#include <sgx-loader.h>
#include <sgx-trampoline.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <sgx-malloc.h>
#include <err.h>

#define is_aligned(addr, bytes) \
     ((((uintptr_t)(const void *)(addr)) & (bytes - 1)) == 0)

int main(int argc, char **argv)
{
    char *binary;
    char *conf;
    void *entry;
    void *base_addr;
    size_t npages;
    unsigned long entry_offset;
    int toff;

    if (argc < 1) {
        err(1, "Please specifiy binary to load\n");
    }
    binary = argv[1];

    if (argc > 1) {
        conf = argv[2];
    } else {
        conf = NULL;
    }

    if(!sgx_init())
        err(1, "failed to init sgx");

    base_addr = load_elf_enclave(binary, &npages, &entry, &toff);
    if (base_addr == NULL) {
        err(1, "Please provide valid binary/configuration files.");
    }

    entry_offset = (uint64_t)entry - (uint64_t)base_addr;
    tcs_t *tcs = init_enclave(base_addr, entry_offset, npages, conf);
    if (!tcs)
        err(1, "failed to run enclave");

    void (*aep)() = exception_handler;
    sgx_enter(tcs, aep);

    return 0;
}
