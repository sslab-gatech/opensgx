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
