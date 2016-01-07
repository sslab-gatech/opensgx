#include "test.h"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

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

// For simplicity, this function do simple operation.
// In the realistic scenario, key creation, signature generation and etc will be
// the possible example.
void do_secret(char *buf) 
{
    for(int i=0; i<strlen(buf); i++)
        buf[i]++;
}

/* main operation. communicate with tor-gencert & tor process */
void enclave_main(int argc, char **argv)
{
    int fd_ea = -1;
    int fd_ae = -1;

    char port_enc_to_app[NAME_BUF_SIZE];
    char port_app_to_enc[NAME_BUF_SIZE];

    if(argc != 4) {
        printf("Usage: ./test.sh sgx-tor [PORT_ENCLAVE_TO_APP] [PORT_APP_TO_ENCLAVE]\n");
        sgx_exit(NULL);
    }
    
    strcpy(port_enc_to_app, argv[2]);
    strcpy(port_app_to_enc, argv[3]);

    if(pipe_init(0) < 0) {
            puts("Error in pipe_init");
            sgx_exit(NULL);
    }

    if((fd_ea = pipe_open(port_enc_to_app, RB_MODE_WR, 0)) < 0) {
            puts("Error in pipe_open");
            sgx_exit(NULL);
    }

    if((fd_ae = pipe_open(port_app_to_enc, RB_MODE_RD, 0)) < 0) {
            puts("Error in pipe_open");
            sgx_exit(NULL);
    }

    // Read the request operations
    int len;
    char msg[20];
    char tmp_buf[20];
    
    read(fd_ae, &len, sizeof(int));
    read(fd_ae, msg, len+1);

    if(!strncmp(msg, "Do Something", len)) {
        // Here, secure operation should be executed.
        read(fd_ae, tmp_buf, 20);
        do_secret(tmp_buf);
    }
 
    // Send the result
    write(fd_ea, tmp_buf, 20);       

    close(fd_ea);
    close(fd_ae);
}
