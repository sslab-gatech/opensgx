/*
 * protocol.h
 */

#ifndef TOR_PROTOCOL_H_
#define TOR_PROTOCOL_H_

#include <arpa/inet.h>
#define TMP_DIRECTORY_CONF "/tmp/tor_ipc_conf"
#define TMP_DIRECTORY_RUN "/tmp/tor_ipc_run"
#define TMP_FILE_NUMBER_FMT "%s/tor_pipe_%d"
#define NAME_BUF_SIZE (256)

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

#ifndef IPC_MODE
#define IPC_MODE
#endif

static int pipe_init(int flag_dir)
{
	int ret;

	if (flag_dir == 0)
		ret = mkdir(TMP_DIRECTORY_CONF, 0770);
	else if (flag_dir == 1)
		ret = mkdir(TMP_DIRECTORY_RUN, 0770);

	if (ret == -1) {
		if(errno != EEXIST) {
			perror("mkdir");
			return -1;
		}
	}

	return 0;
}

static int pipe_open(int unique_id, int is_write, int flag_dir)
{
	char name_buf[NAME_BUF_SIZE];

	if (flag_dir == 0)
		snprintf(name_buf, sizeof(name_buf), TMP_FILE_NUMBER_FMT, 
						TMP_DIRECTORY_CONF, unique_id);
	else if (flag_dir == 1)
		snprintf(name_buf, sizeof(name_buf), TMP_FILE_NUMBER_FMT, 
						TMP_DIRECTORY_RUN, unique_id);

	int ret = mknod(name_buf, S_IFIFO | 0770, 0);
	if (ret == -1) {
		if (errno != EEXIST) {
			perror("mknod");
			exit(1);
		}
	}

	int flag = O_ASYNC;
	if (is_write)
		flag |= O_WRONLY;
	else
		flag |= O_RDONLY;

	int fd = open(name_buf, flag);
	if (fd == -1) {
		perror("open");
		return -1;
	}

	return fd;
}

#endif /* TOR_PROTOCOL_H_ */
