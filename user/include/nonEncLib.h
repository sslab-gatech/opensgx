#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

extern void close_socket(void);
extern int recv_func(char *port, void *buf, size_t len);
extern int send_func(char *ip, char *port, void *msg, size_t len);
extern void hexdump(FILE *fd, void *addr, int len);
