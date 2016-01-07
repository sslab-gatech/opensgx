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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define RB_MODE_RD 0
#define RB_MODE_WR 1

char TMP_DIRECTORY_CONF[] = "/tmp/ipc_conf";
char TMP_DIRECTORY_RUN[] = "/tmp/ipc_run";
char TMP_FILE_NUMBER_FMT[] =  "/pipe_";
int NAME_BUF_SIZE = 256;

static int pipe_init(int flag_dir)
{
	int ret;

	if(flag_dir == 0)
		ret = mkdir(TMP_DIRECTORY_CONF, 0770);
	else if(flag_dir == 1)
		ret = mkdir(TMP_DIRECTORY_RUN, 0770);

	if(ret == -1)
	{
		if(errno != EEXIST) {
                puts("Fail to mkdir");
                return -1;
        }
	}
	return 0;
}

static int pipe_open(char *unique_id, int is_write, int flag_dir)
{
	char name_buf[NAME_BUF_SIZE];

    if (flag_dir == 0) {
        strcpy(name_buf, TMP_DIRECTORY_CONF);
        strcpy(name_buf+strlen(name_buf), TMP_FILE_NUMBER_FMT);
        strcpy(name_buf+strlen(name_buf), unique_id);
    }
    else if (flag_dir == 1) {
        strcpy(name_buf, TMP_DIRECTORY_RUN);
        strcpy(name_buf+strlen(name_buf), TMP_FILE_NUMBER_FMT);
        strcpy(name_buf+strlen(name_buf), unique_id);
    }

	int ret = mknod(name_buf, S_IFIFO | 0770, 0);
	if(ret == -1)
	{
        if(errno != EEXIST) {
            puts("Fail to mknod");
            return -1;
        }
	}

	int flag = O_ASYNC;
	if(is_write)
		flag |= O_WRONLY;
	else
		flag |= O_RDONLY;

	int fd = open(name_buf, flag);

    if(fd == -1)
    {
        puts("Fail to open");
        return -1;
    }

    return fd;
}

int main(int argc, char *argv[]) {
    int fd_ea = -1;
    int fd_ae = -1;

    char port_enc_to_app[NAME_BUF_SIZE];
    char port_app_to_enc[NAME_BUF_SIZE];

    if(argc != 3){
        printf("Usage: ./simple-pipe [PORT_ENCLAVE_TO_APP] [PORT_APP_TO_ENCLAVE]\n");
        exit(1);
    }
    
    strcpy(port_enc_to_app, argv[1]);
    strcpy(port_app_to_enc, argv[2]);

    if(pipe_init(0) < 0) {
        perror("Error in pipe_init");
        exit(1);
    }

    if((fd_ea = pipe_open(port_enc_to_app, RB_MODE_RD, 0)) < 0) {
        perror("Error in pipe_open");
        exit(1);
    }

    if((fd_ae = pipe_open(port_app_to_enc, RB_MODE_WR, 0)) < 0) {
        perror("Error in pipe_open");
        exit(1);
    }
    
    // Request some operations
    int len;
    len = strlen("Do Something");
    write(fd_ae, &len, sizeof(int));
    write(fd_ae, "Do Something", len+1);

    // Send input
    char tmp_buf[20];
    strcpy(tmp_buf, "11111111");
    write(fd_ae, tmp_buf, 20);
    printf("Input = %s\n", tmp_buf);

    // Receive the result
    read(fd_ea, tmp_buf, 20); 
    printf("Output = %s\n", tmp_buf);

    return 0;
}
