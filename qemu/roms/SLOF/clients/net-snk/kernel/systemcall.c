/******************************************************************************
 * Copyright (c) 2004, 2008 IBM Corporation
 * All rights reserved.
 * This program and the accompanying materials
 * are made available under the terms of the BSD License
 * which accompanies this distribution, and is available at
 * http://www.opensource.org/licenses/bsd-license.php
 *
 * Contributors:
 *     IBM Corporation - initial implementation
 *****************************************************************************/

#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <of.h>
#include <netdriver_int.h>
#include <fileio.h>
#include <ioctl.h>
#include "modules.h"

extern int vsprintf(char *, const char *, va_list);
extern void _exit(int status);

void exit(int status);
int printk(const char*, ...);

int open(const char* name, int flags)
{
	int fd, i;

	/* search free file descriptor */
	for(fd=0; fd<FILEIO_MAX; ++fd) {
		if(fd_array[fd].type == FILEIO_TYPE_EMPTY) {
			break;
		}
	}
	if(fd == FILEIO_MAX) {
		printk ("Can not open \"%s\" because file descriptor list is full\n", name);
		/* there is no free file descriptor available */
		return -2;
	}

	for(i=0; i<MODULES_MAX; ++i) {
		if(!snk_modules[i] || !snk_modules[i]->open) {
			continue;
		}

		if(snk_modules[i]->running == 0) {
			snk_modules[i]->init();
		}

		if(snk_modules[i]->open(&fd_array[fd], name, flags) == 0)
			break;
	}

	if(i==MODULES_MAX) {
		/* file not found */
		return -1;
	}

	return fd;
}

int socket(int domain, int type, int proto, char *mac_addr)
{
	snk_module_t *net_module;

	net_module = get_module_by_type(MOD_TYPE_NETWORK);
	if( !net_module ||  !net_module->init) {
		printk("No net_init function available");
		return -1;
	}

	/* Init net device driver */
	if(net_module->running == 0) {
		net_module->init();
	}

	if(net_module->running == 0)
		return -2;

	memcpy(mac_addr, &net_module->mac_addr[0], 6);
	return 0;
}

int close(int fd)
{
	if(fd < 0 || fd >= FILEIO_MAX
	|| fd_array[fd].type == FILEIO_TYPE_EMPTY
	|| fd_array[fd].close == 0)
		return -1;

	return fd_array[fd].close(&fd_array[fd]);
}

ssize_t read(int fd, void *buf, size_t len)
{
	if(fd < 0 || fd >= FILEIO_MAX
	|| fd_array[fd].type == FILEIO_TYPE_EMPTY
	|| fd_array[fd].read == 0)
		return -1;

	return fd_array[fd].read(&fd_array[fd], buf, len);
}

ssize_t write (int fd, const void *buf, size_t len)
{
    char dest_buf[512];
    char *dest_buf_ptr;
    const char *dbuf = buf;
    int i;

    if (fd == 1 || fd == 2) {
	dest_buf_ptr = &dest_buf[0];
        for (i = 0; i < len && i < 256; i++)
        {
            *dest_buf_ptr++ = *dbuf++;
            if (dbuf[-1] == '\n')
                *dest_buf_ptr++ = '\r';
	}
	len = dest_buf_ptr - &dest_buf[0];
	buf = &dest_buf[0];
    }

    if(fd < 0 || fd >= FILEIO_MAX
	|| fd_array[fd].type == FILEIO_TYPE_EMPTY
	|| fd_array[fd].write == 0)
		return -1;

    return fd_array[fd].write(&fd_array[fd], (void *)buf, len);
}

ssize_t lseek (int fd, long offset, int whence)
{
	return 0; // this syscall is unused !!!
#if 0
    if (whence != 0)
	return -1;

    of_seek (fd_array[fd], (unsigned int) (offset>>32), (unsigned int) (offset & 0xffffffffULL));

    return offset;
#endif
}

int ioctl (int fd, int request, void* data)
{
	if (fd < 0
	 || fd >= FILEIO_MAX
	 || fd_array[fd].type == FILEIO_TYPE_EMPTY)
		return -1;
	if (!fd_array[fd].ioctl) { /* for backwards compatibility with network modules */
		snk_module_t *net_module;

		net_module = get_module_by_type(MOD_TYPE_NETWORK);
		if ( !net_module || !net_module->ioctl ) {
			printk("No net_ioctl function available");
			return -1;
		}

		return net_module->ioctl(request, data);
	}

	return fd_array[fd].ioctl(&fd_array[fd], request, data);
}

int recv(int fd, void *packet, int packet_len, int flags)
{
	snk_module_t *net_module;

	net_module = get_module_by_type(MOD_TYPE_NETWORK);
	if( !net_module || !net_module->read ) {
		printk("No net_receive function available");
		return -1;
	}

	return net_module->read(packet, packet_len);
}

int send(int fd, const void *packet, int packet_len, int flags)
{
	snk_module_t *net_module;

	net_module = get_module_by_type(MOD_TYPE_NETWORK);
	if( !net_module || !net_module->write ) {
		printk("No net_xmit function available");
		return -1;
	}

	return net_module->write((void *)packet, packet_len);
}

int sendto(int fd, const void *packet, int packet_len, int flags,
	   const void *sock_addr, int sock_addr_len)
{
	return send(fd, packet, packet_len, flags);
}

void exit(int status)
{
	_exit(status);
}

int printk(const char* fmt, ...)
{
	int count;
	va_list ap;
	char buffer[256];
	va_start (ap, fmt);
	count=vsprintf(buffer, fmt, ap);
	write (1, buffer, count);
	va_end (ap);
	return count;
}
