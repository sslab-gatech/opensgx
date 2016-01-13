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

#include <sgx-lib.h>
#include <sgx-shared.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>


// one 4k page : enclave page & offset

static
void _enclu(enclu_cmd_t leaf, uint64_t rbx, uint64_t rcx, uint64_t rdx,
            out_regs_t *out_regs)
{
   out_regs_t tmp;
   asm volatile(".byte 0x0F\n\t"
                ".byte 0x01\n\t"
                ".byte 0xd7\n\t"
                :"=a"(tmp.oeax),
                 "=b"(tmp.orbx),
                 "=c"(tmp.orcx),
                 "=d"(tmp.ordx)
                :"a"((uint32_t)leaf),
                 "b"(rbx),
                 "c"(rcx),
                 "d"(rdx)
                :"memory");

    // Check whether function requires out_regs
    if (out_regs != NULL) {
        asm volatile ("" : : : "memory"); // Compile time Barrier
        asm volatile ("movl %%eax, %0\n\t"
            "movq %%rbx, %1\n\t"
            "movq %%rcx, %2\n\t"
            "movq %%rdx, %3\n\t"
            :"=a"(out_regs->oeax),
             "=b"(out_regs->orbx),
             "=c"(out_regs->orcx),
             "=d"(out_regs->ordx));
    }
}

int sgx_enclave_read(void *buf, int len)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    if (len <= 0) {
        return -1;
    }

    memcpy(buf, stub->in_shm, len);
    memset(stub->in_shm, 0, SGXLIB_MAX_ARG);

    return len;
}

int sgx_enclave_write(void *buf, int len)
{
    sgx_stub_info *stub = (sgx_stub_info *)STUB_ADDR;

    if (len <= 0) {
        return -1;
    }
    memset(stub->out_shm, 0, SGXLIB_MAX_ARG);
    memcpy(stub->out_shm, buf, len);

    return len;
}

void reverse(unsigned char *in, size_t bytes)
{
    unsigned char temp;
    int i;
    int end;

    end = bytes - 1;
    for (i = 0; i < bytes / 2; i++) {
        temp    = in[i];
        in[i]   = in[end];
        in[end] = temp;
        end--;
    }
}

void load_bytes_from_str(uint8_t *key, char *bytes, size_t size)
{
    if (bytes && (bytes[0] == '\n' || bytes[0] == '\0')) {
        return;
    }

    for (int i = 0; i < size; i++) {
        sscanf(bytes + i*2, "%02X", (unsigned int *)&key[i]);
    }
}

sigstruct_t *sgx_load_sigstruct(char *conf)
{
    FILE *fp = fopen(conf, "r");
    if (!fp)
        err(1, "failed to locate %s", conf);

    char *line = NULL;
    size_t len = 0;
    sigstruct_t *sigstruct;
    int load_start;

    sigstruct = memalign(PAGE_SIZE, sizeof(sigstruct_t));
    memset(sigstruct, 0, sizeof(sigstruct_t));

    // Instance of prefix length
    const int nstart         = strlen("# SIGSTRUCT START");
    const int nend           = strlen("# SIGSTRUCT END");
    const int nprefix        = strlen("HEADER        : ");
    const int nmiscselect    = strlen("MISCSELECT");
    const int nmiscmask      = strlen("MISCMASK");
    const int nattributes    = strlen("ATTRIBUTES");
    const int nattributemask = strlen("ATTRIBUTEMASK");

    load_start = 0;
    while (getline(&line, &len, fp) != -1) {
        // find starting point
        if (!strncmp(line, "# SIGSTRUCT START", nstart))
            load_start = 1;

        if (!load_start)
            continue;

        // load ends
        if (!strncmp(line, "# SIGSTRUCT END", nend))
            break;

        // start loading
        // skip comments
        if (len > 0 && line[0] == '#')
            continue;

        if (!strncmp(line, "HEADER        : ", nprefix)) {
            load_bytes_from_str(sigstruct->header, line + nprefix, 16);
            reverse(sigstruct->header, 16);
        } else if (!strncmp(line, "VENDOR        : ", nprefix)) {
            load_bytes_from_str((unsigned char *)&sigstruct->vendor, line + nprefix, 4);
            reverse((unsigned char *)&sigstruct->vendor, 4);
        } else if (!strncmp(line, "DATE          : ", nprefix)) {
            load_bytes_from_str((unsigned char *)&sigstruct->date, line + nprefix, 4);
            reverse((unsigned char *)&sigstruct->date, 4);
        } else if (!strncmp(line, "HEADER2       : ", nprefix)) {
            load_bytes_from_str(sigstruct->header2, line + nprefix, 16);
            reverse(sigstruct->header2, 16);
        } else if (!strncmp(line, "SWDEFINO: ", nprefix)) {
            load_bytes_from_str((unsigned char *)&sigstruct->swdefined, line + nprefix, 4);
            reverse((unsigned char *)&sigstruct->swdefined, 4);
        } else if (!strncmp(line, "RESERVED1     : ", nprefix)) {
            load_bytes_from_str(sigstruct->reserved1, line + nprefix, 84);
        } else if (!strncmp(line, "MODULUS       : ", nprefix)) {
            load_bytes_from_str(sigstruct->modulus, line + nprefix, 384);
        } else if (!strncmp(line, "EXPONENT      : ", nprefix)) {
            load_bytes_from_str((unsigned char *)&sigstruct->exponent, line + nprefix, 4);
            reverse((unsigned char *)&sigstruct->exponent, 4);
        } else if (!strncmp(line, "SIGNATURE     : ", nprefix)) {
            load_bytes_from_str(sigstruct->signature, line + nprefix, 384);
        } else if (!strncmp(line, "MISCSELECT", nmiscselect)) {
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".EXINFO       : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->miscselect.exinfo = 1;
                    else
                        sigstruct->miscselect.exinfo = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED     : ", nprefix)) {
                    if (strncmp(line + nprefix, "000", 3))
                        continue;
                    sigstruct->miscselect.reserved1 = 0;
                    load_bytes_from_str(sigstruct->miscselect.reserved2,
                                        line + nprefix + 3, 3);
                }
            }
        } else if (!strncmp(line, "MISCMASK", nmiscmask)) {
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".EXINFO       : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->miscmask.exinfo = 1;
                    else
                        sigstruct->miscmask.exinfo = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED     : ", nprefix)) {
                    if (strncmp(line + nprefix, "000", 3))
                        continue;
                    sigstruct->miscmask.reserved1 = 0;
                    load_bytes_from_str(sigstruct->miscmask.reserved2,
                                        line + nprefix + 3, 3);
                }
            }
        } else if (!strncmp(line, "ATTRIBUTES", nattributes)) {
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED1    : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributes.reserved1 = 1;
                    else
                        sigstruct->attributes.reserved1 = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".DEBUG        : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributes.debug = 1;
                    else
                        sigstruct->attributes.debug = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".MODE64BIT    : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributes.mode64bit = 1;
                    else
                        sigstruct->attributes.mode64bit = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED2    : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributes.reserved2 = 1;
                    else
                        sigstruct->attributes.reserved2 = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".PROVISIONKEY : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributes.provisionkey = 1;
                    else
                        sigstruct->attributes.provisionkey = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".EINITTOKENKEY: ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributes.einittokenkey = 1;
                    else
                        sigstruct->attributes.einittokenkey = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED3    : ", nprefix)) {
                    if (strncmp(line + nprefix, "00", 2))
                        continue;
                    sigstruct->attributes.reserved3 = 0;
                    load_bytes_from_str(sigstruct->attributes.reserved4,
                                        line + nprefix + 2, 7);
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".XFRM         : ", nprefix)) {
                    load_bytes_from_str((unsigned char *)&sigstruct->attributes.xfrm,
                                        line + nprefix, 8);
                    reverse((unsigned char *)&sigstruct->attributes.xfrm, 8);
                }
            }
        } else if (!strncmp(line, "ATTRIBUTEMASK", nattributemask)) {
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED1    : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributeMask.reserved1 = 1;
                    else
                        sigstruct->attributeMask.reserved1 = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".DEBUG        : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributeMask.debug = 1;
                    else
                        sigstruct->attributeMask.debug = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".MODE64BIT    : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributeMask.mode64bit = 1;
                    else
                        sigstruct->attributeMask.mode64bit = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED2    : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributeMask.reserved2 = 1;
                    else
                        sigstruct->attributeMask.reserved2 = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".PROVISIONKEY : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributeMask.provisionkey = 1;
                    else
                        sigstruct->attributeMask.provisionkey = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".EINITTOKENKEY: ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        sigstruct->attributeMask.einittokenkey = 1;
                    else
                        sigstruct->attributeMask.einittokenkey = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED3    : ", nprefix)) {
                    if (strncmp(line + nprefix, "00", 2))
                        continue;
                    sigstruct->attributeMask.reserved3 = 0;
                    load_bytes_from_str(sigstruct->attributeMask.reserved4,
                                        line + nprefix + 2, 7);
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".XFRM         : ", nprefix)) {
                    load_bytes_from_str((unsigned char *)&sigstruct->attributeMask.xfrm,
                                        line + nprefix, 8);
                    reverse((unsigned char *)&sigstruct->attributeMask.xfrm, 8);
                }
            }
        } else if (!strncmp(line, "ENCLAVEHASH   : ", nprefix)) {
            load_bytes_from_str(sigstruct->enclaveHash, line + nprefix, 32);
            //reverse(sigstruct->enclaveHash, 32);
        } else if (!strncmp(line, "RESERVED3     : ", nprefix)) {
            load_bytes_from_str(sigstruct->reserved3, line + nprefix, 32);
        } else if (!strncmp(line, "ISVPRODID     : ", nprefix)) {
            load_bytes_from_str((unsigned char *)&sigstruct->isvProdID, line + nprefix, 2);
            reverse((unsigned char *)&sigstruct->isvProdID, 2);
        } else if (!strncmp(line, "ISVSVN        : ", nprefix)) {
            load_bytes_from_str((unsigned char *)&sigstruct->isvSvn, line + nprefix, 2);
            reverse((unsigned char *)&sigstruct->isvSvn, 2);
        } else if (!strncmp(line, "RESERVED4     : ", nprefix)) {
            load_bytes_from_str(sigstruct->reserved4, line + nprefix, 12);
        } else if (!strncmp(line, "Q1            : ", nprefix)) {
            load_bytes_from_str(sigstruct->q1, line + nprefix, 384);
        } else if (!strncmp(line, "Q2            : ", nprefix)) {
            load_bytes_from_str(sigstruct->q2, line + nprefix, 384);
       }
    }

    free(line);
    fclose(fp);

    return sigstruct;
}
