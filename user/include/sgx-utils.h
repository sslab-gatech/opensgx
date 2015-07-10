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

#pragma once

#include <sgx.h>

extern void reverse(unsigned char *in, size_t bytes);
extern unsigned char *swap_endian(unsigned char *in, size_t bytes);
extern void fmt_hash(uint8_t hash[32], char out[65]);
extern char *fmt_bytes(uint8_t *bytes, int size);
extern unsigned char *load_measurement(char *conf);
extern char *dump_sigstruct(sigstruct_t *s);
extern char *dbg_dump_sigstruct(sigstruct_t *s);
extern sigstruct_t *load_sigstruct(char *conf);
extern char *dbg_dump_einittoken(einittoken_t *t);
extern einittoken_t *load_einittoken(char *conf);
extern void hexdump(FILE *fp, void *addr, int len);
extern void load_bytes_from_str(uint8_t *key, char *bytes, size_t size);
extern int rop2(int val);
