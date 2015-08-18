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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <err.h>

#include <sgx-user.h>
#include <sgx-utils.h>

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

unsigned char *swap_endian(unsigned char *in, size_t bytes)
{
    unsigned char *out;
    int i;

    out = malloc(bytes);
    for (i = 0; i < bytes; i++) {
        out[i] = in[bytes - i - 1];
    }

    return out;
}

void fmt_hash(uint8_t hash[32], char out[64+1])
{
    int i;
    for (i = 0; i < 32; i++) {
        snprintf(&out[i*2], 3, "%02X", hash[i]);
    }
    out[64] = '\0';
}

char *fmt_bytes(uint8_t *bytes, int size)
{
    char *buf = malloc(size*2 + 1);
    if (!buf)
        return NULL;

    for (int i = 0; i < size; i ++)
        snprintf(&buf[i*2], 3, "%02X", *(bytes + i));

    buf[size*2] = '\0';
    return buf;
}

char *dump_sigstruct(sigstruct_t *s)
{
    char *msg = malloc(4096);
    if (!msg)
        return NULL;

    char *hdr           = fmt_bytes(swap_endian(s->header, 16), 16);
    char *vendor        = fmt_bytes(swap_endian((unsigned char *)&s->vendor, 4), 4);
    char *date          = fmt_bytes(swap_endian((unsigned char *)&s->date, 4), 4);
    char *hdr2          = fmt_bytes(swap_endian(s->header2, 16), 16);
    char *swid          = fmt_bytes(swap_endian((unsigned char *)&s->swdefined, 4), 4);
    char *rsv1          = fmt_bytes(s->reserved1, 84);
    char *pub           = fmt_bytes(s->modulus, 384);
    char *ext           = fmt_bytes(swap_endian((unsigned char *)&s->exponent, 4), 4);
    char *sig           = fmt_bytes(s->signature, 384);
    char *mselect_rsv2  = fmt_bytes(s->miscselect.reserved2, 3);
    char *mmasck_rsv2   = fmt_bytes(s->miscmask.reserved2, 3);
    char *rsv2          = fmt_bytes(s->reserved2, 20);
    char *attrs_rsv4    = fmt_bytes(s->attributes.reserved4, 7);
    char *attrs_xfrm    = fmt_bytes(swap_endian((unsigned char *)&s->attributes.xfrm, 8), 8);
    char *attrmask_rsv4 = fmt_bytes(s->attributeMask.reserved4, 7);
    char *attrmask_xfrm = fmt_bytes(swap_endian((unsigned char *)&s->attributeMask.xfrm, 8), 8);
    char *hash          = fmt_bytes(s->enclaveHash, 32);
    char *rsv3          = fmt_bytes(s->reserved3, 32);
    char *prodid        = fmt_bytes(swap_endian((unsigned char *)&s->isvProdID, 2), 2);
    char *svn           = fmt_bytes(swap_endian((unsigned char *)&s->isvSvn, 2), 2);
    char *rsv4          = fmt_bytes(s->reserved4, 12);
    char *q1            = fmt_bytes(s->q1, 384);
    char *q2            = fmt_bytes(s->q2, 384);

    snprintf(msg, 4096,"\
HEADER        : %s\n\
VENDOR        : %s\n\
DATE          : %s\n\
HEADER2       : %s\n\
SWDEFINO      : %s\n\
RESERVED1     : %s\n\
MODULUS       : %s\n\
EXPONENT      : %s\n\
SIGNATURE     : %s\n\
MISCSELECT\n\
.EXINFO       : %d\n\
.RESERVED     : %d%s\n\
MISCMASK\n\
.EXINFO       : %d\n\
.RESERVED     : %d%s\n\
RESERVED2     : %s\n\
ATTRIBUTES\n\
.RESERVED1    : %d\n\
.DEBUG        : %d\n\
.MODE64BIT    : %d\n\
.RESERVED2    : %d\n\
.PROVISIONKEY : %d\n\
.EINITTOKENKEY: %d\n\
.RESERVED3    : %d%s\n\
.XFRM         : %s\n\
ATTRIBUTEMASK\n\
.RESERVED1    : %d\n\
.DEBUG        : %d\n\
.MODE64BIT    : %d\n\
.RESERVED2    : %d\n\
.PROVISIONKEY : %d\n\
.EINITTOKENKEY: %d\n\
.RESERVED3    : %d%s\n\
.XFRM         : %s\n\
ENCLAVEHASH   : %s\n\
RESERVED3     : %s\n\
ISVPRODID     : %s\n\
ISVSVN        : %s\n\
RESERVED4     : %s\n\
Q1            : %s\n\
Q2            : %s",
            hdr,
            vendor,
            date,
            hdr2,
            swid,
            rsv1,
            pub,
            ext,
            sig,
            s->miscselect.exinfo,
            s->miscselect.reserved1, mselect_rsv2,
            s->miscmask.exinfo,
            s->miscmask.reserved1, mmasck_rsv2,
            rsv2,
            s->attributes.reserved1,
            s->attributes.debug,
            s->attributes.mode64bit,
            s->attributes.reserved2,
            s->attributes.provisionkey,
            s->attributes.einittokenkey,
            s->attributes.reserved3, attrs_rsv4,
            attrs_xfrm,
            s->attributeMask.reserved1,
            s->attributeMask.debug,
            s->attributeMask.mode64bit,
            s->attributeMask.reserved2,
            s->attributeMask.provisionkey,
            s->attributeMask.einittokenkey,
            s->attributeMask.reserved3, attrmask_rsv4,
            attrmask_xfrm,
            hash,
            rsv3,
            prodid,
            svn,
            rsv4,
            q1,
            q2);

    free(hdr);
    free(vendor);
    free(date);
    free(hdr2);
    free(rsv1);
    free(swid);
    free(pub);
    free(ext);
    free(sig);
    free(mselect_rsv2);
    free(mmasck_rsv2);
    free(rsv2);
    free(attrs_rsv4);
    free(attrs_xfrm);
    free(attrmask_rsv4);
    free(attrmask_xfrm);
    free(hash);
    free(rsv3);
    free(prodid);
    free(svn);
    free(rsv4);
    free(q1);
    free(q2);

    return msg;
}
char *dbg_dump_sigstruct(sigstruct_t *s)
{
    char *msg = malloc(2048);
    if (!msg)
        return NULL;

    char *hdr           = fmt_bytes(swap_endian(s->header, 16), 16);
    char *vendor        = fmt_bytes(swap_endian((unsigned char *)&s->vendor, 4), 4);
    char *date          = fmt_bytes(swap_endian((unsigned char *)&s->date, 4), 4);
    char *hdr2          = fmt_bytes(swap_endian(s->header2, 16), 16);
    char *swid          = fmt_bytes(swap_endian((unsigned char *)&s->swdefined, 4), 4);
    char *rsv1          = fmt_bytes(s->reserved1, 32);
    char *pub           = fmt_bytes(s->modulus, 32);
    char *ext           = fmt_bytes(swap_endian((unsigned char *)&s->exponent, 4), 4);
    char *sig           = fmt_bytes(s->signature, 32);
    char *mselect_rsv2  = fmt_bytes(s->miscselect.reserved2, 3);
    char *mmasck_rsv2   = fmt_bytes(s->miscmask.reserved2, 3);
    char *rsv2          = fmt_bytes(s->reserved2, 20);
    char *attrs_rsv4    = fmt_bytes(s->attributes.reserved4, 7);
    char *attrs_xfrm    = fmt_bytes(swap_endian((unsigned char *)&s->attributes.xfrm, 8), 8);
    char *attrmask_rsv4 = fmt_bytes(s->attributeMask.reserved4, 7);
    char *attrmask_xfrm = fmt_bytes(swap_endian((unsigned char *)&s->attributeMask.xfrm, 8), 8);
    char *hash          = fmt_bytes(s->enclaveHash, 32);
    char *rsv3          = fmt_bytes(s->reserved3, 32);
    char *prodid        = fmt_bytes(swap_endian((unsigned char *)&s->isvProdID, 2), 2);
    char *svn           = fmt_bytes(swap_endian((unsigned char *)&s->isvSvn, 2), 2);
    char *rsv4          = fmt_bytes(s->reserved4, 12);
    char *q1            = fmt_bytes(s->q1, 32);
    char *q2            = fmt_bytes(s->q2, 32);

    snprintf(msg, 2048,"\
HEADER        : %s\n\
VENDOR        : %s\n\
DATE          : %s\n\
HEADER2       : %s\n\
SWDEFINO      : %s\n\
RESERVED1     : %s..\n\
MODULUS       : %s..\n\
EXPONENT      : %s\n\
SIGNATURE     : %s..\n\
MISCSELECT\n\
.EXINFO       : %d\n\
.RESERVED     : %d%s\n\
MISCMASK\n\
.EXINFO       : %d\n\
.RESERVED     : %d%s\n\
RESERVED2     : %s\n\
ATTRIBUTES\n\
.RESERVED1    : %d\n\
.DEBUG        : %d\n\
.MODE64BIT    : %d\n\
.RESERVED2    : %d\n\
.PROVISIONKEY : %d\n\
.EINITTOKENKEY: %d\n\
.RESERVED3    : %d%s\n\
.XFRM         : %s\n\
ATTRIBUTEMASK\n\
.RESERVED1    : %d\n\
.DEBUG        : %d\n\
.MODE64BIT    : %d\n\
.RESERVED2    : %d\n\
.PROVISIONKEY : %d\n\
.EINITTOKENKEY: %d\n\
.RESERVED3    : %d%s\n\
.XFRM         : %s\n\
ENCLAVEHASH   : %s\n\
RESERVED3     : %s\n\
ISVPRODID     : %s\n\
ISVSVN        : %s\n\
RESERVED4     : %s\n\
Q1            : %s..\n\
Q2            : %s..",
            hdr,
            vendor,
            date,
            hdr2,
            swid,
            rsv1,
            pub,
            ext,
            sig,
            s->miscselect.exinfo,
            s->miscselect.reserved1, mselect_rsv2,
            s->miscmask.exinfo,
            s->miscmask.reserved1, mmasck_rsv2,
            rsv2,
            s->attributes.reserved1,
            s->attributes.debug,
            s->attributes.mode64bit,
            s->attributes.reserved2,
            s->attributes.provisionkey,
            s->attributes.einittokenkey,
            s->attributes.reserved3, attrs_rsv4,
            attrs_xfrm,
            s->attributeMask.reserved1,
            s->attributeMask.debug,
            s->attributeMask.mode64bit,
            s->attributeMask.reserved2,
            s->attributeMask.provisionkey,
            s->attributeMask.einittokenkey,
            s->attributeMask.reserved3, attrmask_rsv4,
            attrmask_xfrm,
            hash,
            rsv3,
            prodid,
            svn,
            rsv4,
            q1,
            q2);

    free(hdr);
    free(vendor);
    free(date);
    free(hdr2);
    free(rsv1);
    free(swid);
    free(pub);
    free(ext);
    free(sig);
    free(mselect_rsv2);
    free(mmasck_rsv2);
    free(rsv2);
    free(attrs_rsv4);
    free(attrs_xfrm);
    free(attrmask_rsv4);
    free(attrmask_xfrm);
    free(hash);
    free(rsv3);
    free(prodid);
    free(svn);
    free(rsv4);
    free(q1);
    free(q2);

    return msg;
}

unsigned char *load_measurement(char *conf)
{

    FILE *fp = fopen(conf, "r");
    if (!fp)
        err(1, "failed to locate %s", conf);

    char *line = NULL;
    size_t len = 0;

    unsigned char *measurement;
    measurement = malloc(32);
    memset(measurement, 0, 32);

    const int nmeasurement = strlen("MEASUREMENT: ");

    while (getline(&line, &len, fp) != -1) {;
        if (len > 0 && line[0] == '#')
            continue;

        if (!strncmp(line, "MEASUREMENT: ", nmeasurement)) {
            load_bytes_from_str(measurement, line + nmeasurement, 32);
            break;
        }
    }

    free(line);
    fclose(fp);

    return measurement;
}

sigstruct_t *load_sigstruct(char *conf)
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

#if 0
    // Check if load successfully
    char *msg = dbg_dump_sigstruct(sigstruct);
    printf("# debug sigstruct:\n%s\n", msg);
    free(msg);
#endif

    free(line);
    fclose(fp);

    return sigstruct;
}

char *dbg_dump_einittoken(einittoken_t *t)
{
    char *msg = malloc(2048);
    if (!msg)
        return NULL;

    char *valid         = fmt_bytes(swap_endian((unsigned char *)&t->valid, 4), 4);
    char *rsv1          = fmt_bytes(t->reserved1, 44);
    char *attr_rsv4     = fmt_bytes(t->attributes.reserved4, 7);
    char *attr_xfrm     = fmt_bytes(swap_endian((unsigned char *)&t->attributes.xfrm, 8), 8);
    char *mrenclave     = fmt_bytes(t->mrEnclave, 32);
    char *rsv2          = fmt_bytes(t->reserved2, 32);
    char *mrsigner      = fmt_bytes(t->mrSigner, 32);
    char *rsv3          = fmt_bytes(t->reserved3, 32);
    char *cpusvn        = fmt_bytes(swap_endian(t->cpuSvnLE, 16), 16);
    char *rsv4          = fmt_bytes(t->reserved4, 24);
    char *maskmisc_rsv2 = fmt_bytes(t->maskedmiscSelectLE.reserved2, 3);
    char *maskattr_rsv4 = fmt_bytes(t->maskedAttributesLE.reserved4, 7);
    char *maskattr_xfrm = fmt_bytes(swap_endian((unsigned char *)&t->maskedAttributesLE.xfrm, 8), 8);
    char *keyid         = fmt_bytes(swap_endian(t->keyid, 32), 32);
    char *mac           = fmt_bytes(t->mac, 16);


    snprintf(msg, 2048,"\
VALID             : %s\n\
RESERVED1         : %s\n\
ATTRIBUTES\n\
.RESERVED1        : %d\n\
.DEBUG            : %d\n\
.MODE64BIT        : %d\n\
.RESERVED2        : %d\n\
.PROVISIONKEY     : %d\n\
.EINITTOKENKEY    : %d\n\
.RESERVED3        : %d%s\n\
.XFRM             : %s\n\
MRENCLAVE         : %s\n\
RESERVED2         : %s\n\
MRSIGNER          : %s\n\
RESERVED3         : %s\n\
CPUSVNLE          : %s\n\
ISVPRODIDLE       : %x\n\
ISVSVNLE          : %x\n\
RESERVED4         : %s\n\
MASKEDMISCSELECTLE\n\
.EXINFO           : %d\n\
.RESERVED         : %d%s\n\
MASKEDATTRIBUTESLE\n\
.RESERVED1        : %d\n\
.DEBUG            : %d\n\
.MODE64BIT        : %d\n\
.RESERVED2        : %d\n\
.PROVISIONKEY     : %d\n\
.EINITTOKENKEY    : %d\n\
.RESERVED3        : %d%s\n\
.XFRM             : %s\n\
KEYID             : %s\n\
MAC               : %s",
            valid,
            rsv1,
            t->attributes.reserved1,
            t->attributes.debug,
            t->attributes.mode64bit,
            t->attributes.reserved2,
            t->attributes.provisionkey,
            t->attributes.einittokenkey,
            t->attributes.reserved3, attr_rsv4,
            attr_xfrm,
            mrenclave,
            rsv2,
            mrsigner,
            rsv3,
            cpusvn,
            t->isvprodIDLE,
            t->isvsvnLE,
            rsv4,
            t->maskedmiscSelectLE.exinfo,
            t->maskedmiscSelectLE.reserved1,
            maskmisc_rsv2, t->maskedAttributesLE.reserved1,
            t->maskedAttributesLE.debug,
            t->maskedAttributesLE.mode64bit,
            t->maskedAttributesLE.reserved2,
            t->maskedAttributesLE.provisionkey,
            t->maskedAttributesLE.einittokenkey,
            t->maskedAttributesLE.reserved3, maskattr_rsv4,
            maskattr_xfrm,
            keyid,
            mac);

    free(valid);
    free(rsv1);
    free(attr_rsv4);
    free(attr_xfrm);
    free(mrenclave);
    free(rsv2);
    free(mrsigner);
    free(rsv3);
    free(cpusvn);
    free(rsv4);
    free(maskmisc_rsv2);
    free(maskattr_rsv4);
    free(maskattr_xfrm);
    free(keyid);
    free(mac);

    return msg;
}

einittoken_t *load_einittoken(char *conf)
{
    FILE *fp = fopen(conf, "r");
    if (!fp)
        err(1, "failed to locate %s", conf);

    char *line = NULL;
    size_t len = 0;
    einittoken_t *token;
    int load_start;

    token = memalign(EINITTOKEN_ALIGN_SIZE, sizeof(einittoken_t));
    memset(token, 0, sizeof(einittoken_t));

    // Instance of prefix length
    const int nstart         = strlen("# EINITTOKEN START");
    const int nend           = strlen("# EINITTOKEN END");
    const int nprefix        = strlen("VALID             : ");
    const int nattr          = strlen("ATTRIBUTES");
    const int nmaskedmisc    = strlen("MASKEDMISCSELECTLE");
    const int nmaskedattr    = strlen("MASKEDATTRIBUTESLE");

    load_start = 0;
    while (getline(&line, &len, fp) != -1) {
        // find starting point
        if (!strncmp(line, "# EINITTOKEN START", nstart))
            load_start = 1;

        if (!load_start)
            continue;

        // load ends
        if (!strncmp(line, "# EINITTOKEN END", nend))
            break;

        // start loading
        // skip comments
        if (len > 0 && line[0] == '#')
            continue;

        if (!strncmp(line, "VALID             : ", nprefix)) {
            load_bytes_from_str((unsigned char *)&token->valid, line + nprefix, 4);
            reverse((unsigned char *)&token->valid, 4);
        } else if (!strncmp(line, "RESERVED1         : ", nprefix)) {
            load_bytes_from_str(token->reserved1, line + nprefix, 44);
        } else if (!strncmp(line, "ATTRIBUTES", nattr)) {
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED1        : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->attributes.reserved1 = 1;
                    else
                        token->attributes.reserved1 = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".DEBUG            : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->attributes.debug = 1;
                    else
                        token->attributes.debug = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".MODE64BIT        : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->attributes.mode64bit = 1;
                    else
                        token->attributes.mode64bit = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED2        : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->attributes.reserved2 = 1;
                    else
                        token->attributes.reserved2 = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".PROVISIONKEY     : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->attributes.provisionkey = 1;
                    else
                        token->attributes.provisionkey = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".EINITTOKENKEY    : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->attributes.einittokenkey = 1;
                    else
                        token->attributes.einittokenkey = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED3        : ", nprefix)) {
                    if (strncmp(line + nprefix, "00", 2))
                        continue;
                    token->attributes.reserved3 = 0;
                    load_bytes_from_str(token->attributes.reserved4,
                                        line + nprefix + 2, 7);
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".XFRM             : ", nprefix)) {
                    load_bytes_from_str((unsigned char *)&token->attributes.xfrm,
                                        line + nprefix, 8);
                    reverse((unsigned char *)&token->attributes.xfrm, 8);
                }
            }
        } else if (!strncmp(line, "MRENCLAVE         : ", nprefix)) {
            load_bytes_from_str(token->mrEnclave, line + nprefix, 32);
        } else if (!strncmp(line, "RESERVED2         : ", nprefix)) {
            load_bytes_from_str(token->reserved2, line + nprefix, 32);
        } else if (!strncmp(line, "MRSIGNER          : ", nprefix)) {
            load_bytes_from_str(token->mrSigner, line + nprefix, 32);
        } else if (!strncmp(line, "RESERVED3         : ", nprefix)) {
            load_bytes_from_str(token->reserved3, line + nprefix, 32);
        } else if (!strncmp(line, "CPUSVNLE          : ", nprefix)) {
            load_bytes_from_str(token->cpuSvnLE, line + nprefix, 16);
        } else if (!strncmp(line, "ISVPRODIDLE       : ", nprefix)) {
            load_bytes_from_str((unsigned char *)&token->isvprodIDLE, line + nprefix, 2);
            reverse((unsigned char *)&token->isvprodIDLE, 2);
        } else if (!strncmp(line, "ISVSVNLE          : ", nprefix)) {
            load_bytes_from_str((unsigned char *)&token->isvsvnLE, line + nprefix, 2);
            reverse((unsigned char *)&token->isvsvnLE, 2);
        } else if (!strncmp(line, "RESERVED4         : ", nprefix)) {
            load_bytes_from_str(token->reserved4, line + nprefix, 24);
        } else if (!strncmp(line, "MASKEDMISCSELECTLE", nmaskedmisc)) {
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".EXINFO       : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->maskedmiscSelectLE.exinfo = 1;
                    else
                        token->maskedmiscSelectLE.exinfo = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED     : ", nprefix)) {
                    if (strncmp(line + nprefix, "000", 3))
                        continue;
                    token->maskedmiscSelectLE.reserved1 = 0;
                    load_bytes_from_str(token->maskedmiscSelectLE.reserved2,
                                        line + nprefix + 3, 3);
                }
            }
        } else if (!strncmp(line, "MASKEDATTRIBUTESLE", nmaskedattr)) {
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED1        : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->maskedAttributesLE.reserved1 = 1;
                    else
                        token->maskedAttributesLE.reserved1 = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".DEBUG            : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->maskedAttributesLE.debug = 1;
                    else
                        token->maskedAttributesLE.debug = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".MODE64BIT        : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->maskedAttributesLE.mode64bit = 1;
                    else
                        token->maskedAttributesLE.mode64bit = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED2        : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->maskedAttributesLE.reserved2 = 1;
                    else
                        token->maskedAttributesLE.reserved2 = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".PROVISIONKEY     : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->maskedAttributesLE.provisionkey = 1;
                    else
                        token->maskedAttributesLE.provisionkey = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".EINITTOKENKEY    : ", nprefix)) {
                    if ((line + nprefix) &&
                        (line[nprefix] == '\0' || line[nprefix] == '\n'))
                        continue;
                    if (line[nprefix] == '1')
                        token->maskedAttributesLE.einittokenkey = 1;
                    else
                        token->maskedAttributesLE.einittokenkey = 0;
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".RESERVED3        : ", nprefix)) {
                    if (strncmp(line + nprefix, "00", 2))
                        continue;
                    token->maskedAttributesLE.reserved3 = 0;
                    load_bytes_from_str(token->maskedAttributesLE.reserved4,
                                        line + nprefix + 2, 7);
                }
            }
            if (getline(&line, &len, fp) != -1) {
                if (!strncmp(line, ".XFRM             : ", nprefix)) {
                    load_bytes_from_str((unsigned char *)&token->maskedAttributesLE.xfrm,
                                        line + nprefix, 8);
                    reverse((unsigned char *)&token->maskedAttributesLE.xfrm, 8);
                }
            }
        } else if (!strncmp(line, "KEYID             : ", nprefix)) {
            load_bytes_from_str(token->keyid, line + nprefix, 32);
            reverse(token->keyid, 32);
        } else if (!strncmp(line, "MAC               : ", nprefix)) {
            load_bytes_from_str(token->mac, line + nprefix, 16);
        }
    }

#if 0
    // Check if load successfully
    char *msg = dbg_dump_einittoken(token);
    printf("# debug einittoken:\n%s\n", msg);
    free(msg);
#endif

    free(line);
    fclose(fp);

    return token;
}

void hexdump(FILE *fd, void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                fprintf(fd, "  %s\n", buff);

            // Output the offset.
            fprintf(fd, "  %04x ", i);
        }

        // Now the hex code for the specific character.
        fprintf(fd, " %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        fprintf(fd, "   ");
        i++;
    }

    // And print the final ASCII bit.
    fprintf(fd, "  %s\n", buff);
}

// NOTE. arg/ret should be unsigned int, but current code in sgx-*
// don't properly distinguish signed/unsigned
int rop2(int val)
{
    unsigned int n = 1;
    while (n < val)
        n <<= 1;
    return n;
}

void load_bytes_from_str(uint8_t *key, char *bytes, size_t size)
{
    if (bytes && (bytes[0] == '\n' || bytes[0] == '\0')) {
        return;
    }

    for (int i = 0; i < size; i++) {
        sscanf(bytes + i*2, "%02X", (unsigned int *)&key[i]);
    }

    if (sgx_dbg_rsa) {
        char *loaded = fmt_bytes(key, size);
        sgx_dbg(rsa, "read from %s", bytes);
        sgx_dbg(rsa, "load to   %s", loaded);
        free(loaded);
    }
}

#ifdef UNITTEST

int main(int argc, char *argv[])
{
    uint8_t bytes[10] = {1,2,3,4,5,6,7,8,9,10};
    hexdump(stderr, bytes, 10);
    char *msg = fmt_bytes(bytes, 10);
    printf("msg: %s\n", msg);
    free(msg);

    assert(rop2(5) ==  8);
    assert(rop2(6) ==  8);
    assert(rop2(7) ==  8);

    return 0;
}

#endif
