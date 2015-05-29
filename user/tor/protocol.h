#pragma once

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>

#define RB_MODE_RD 0
#define RB_MODE_WR 1

/*
#define TMP_DIRECTORY_CONF "/tmp/tor_ipc_conf"
#define TMP_DIRECTORY_RUN "/tmp/tor_ipc_run"
#define TMP_FILE_NUMBER_FMT "%s/tor_pipe_%d"
#define NAME_BUF_SIZE (256)
*/

//#define rb_init(X) pipe_init()
//#define rb_write_string(a,b,c) rb_write(a, b, strlen(b)+1)
//#define rb_open(a,b,c) pipe_open(a,c)
//#define rb_write write
//#define rb_read read
//#define rb_unlocksem(x)
//#define rb_locksem(x)
//#define rb_close(X) close(*(X))

extern char TMP_DIRECTORY_CONF[];
extern char TMP_DIRECTORY_RUN[];
extern char TMP_FILE_NUMBER_FMT[];
extern int NAME_BUF_SIZE;

extern int pipe_init(int flag_dir);
extern int pipe_open(int unique_id, int is_write, int flag_dir);
