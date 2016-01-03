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

#define RB_MODE_RD 0
#define RB_MODE_WR 1

extern char TMP_DIRECTORY_CONF[];
extern char TMP_DIRECTORY_RUN[];
extern char TMP_FILE_NUMBER_FMT[];
extern int NAME_BUF_SIZE;

extern int pipe_init(int flag_dir);
extern int pipe_open(char *unique_id, int is_write, int flag_dir);
