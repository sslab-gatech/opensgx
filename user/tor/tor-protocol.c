#include <sgx-lib.h>

#include "protocol.h"
#include "tor-lib.h"

char TMP_DIRECTORY_CONF[] = "/tmp/tor_ipc_conf";
char TMP_DIRECTORY_RUN[] = "/tmp/tor_ipc_run";
char TMP_FILE_NUMBER_FMT[] =  "%s/tor_pipe_%d";
int NAME_BUF_SIZE = 256;

int pipe_init(int flag_dir)
{
	int ret;

	if(flag_dir == 0)
		ret = sgx_mkdir(TMP_DIRECTORY_CONF, 20, 0770);
	else if(flag_dir == 1)
		ret = sgx_mkdir(TMP_DIRECTORY_RUN, 20, 0770);

	if(ret == -1)
	{
        sgx_puts("Fail to sgx_mkdir");
        return -1;
	}
	return 0;
}

int pipe_open(int unique_id, int is_write, int flag_dir)
{
	char name_buf[NAME_BUF_SIZE];

	if(flag_dir == 0)
		sgx_snprintf(name_buf, sizeof(name_buf), TMP_FILE_NUMBER_FMT, 20,
						TMP_DIRECTORY_CONF, 20, unique_id);
	else if(flag_dir == 1)
		sgx_snprintf(name_buf, sizeof(name_buf), TMP_FILE_NUMBER_FMT, 20,
						TMP_DIRECTORY_RUN, 20, unique_id);

	int ret = sgx_mknod(name_buf, 40, S_IFIFO | 0770, 0);
	if(ret == -1)
	{
        sgx_puts("Fail to sgx_mknod");
        exit(1);
	}

	int flag = O_ASYNC;
	if(is_write)
		flag |= O_WRONLY;
	else
		flag |= O_RDONLY;

	int fd = sgx_open(name_buf, 40, flag);

	if(fd == -1)
	{
        sgx_puts("Fail to sgx_open");
		return -1;
	}

	return fd;

}
