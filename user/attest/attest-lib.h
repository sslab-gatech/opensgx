#pragma once

#include <sgx.h>
#include <sgx-user.h>
#include <sgx-kern.h>

/*
extern int sgx_mkdir(char pathname[], size_t size, mode_t mode);
extern int sgx_mknod(char pathname[], size_t size, mode_t mode, dev_t dev);
extern int sgx_open(char pathname[], size_t size, int flag);
extern int sgx_close(int fd);
extern ssize_t sgx_write(int fd, void *buf, size_t count);
extern ssize_t sgx_read(int fd, void *buf, size_t count);
// Note that this is not regular snprintf, specialized one for Tor
extern int sgx_snprintf(char dst[], int dst_size, char buf1[], size_t size1, 
                        char buf2[], size_t size2, int arg1);
extern int sgx_time(time_t *arg1);
*/
extern void *sgx_memchr (const void *s, int c, size_t n);
