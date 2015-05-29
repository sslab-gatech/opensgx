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
#include <string.h>
#include <stdlib.h> /* malloc */
#include <of.h>
#include <pci.h>
#include <kernel.h>
#include <cpu.h>
#include <fileio.h>
#include <ioctl.h> /* ioctl */
#include "modules.h"

/* Application entry point .*/
extern int _start(unsigned char *arg_string, long len);
extern int main(int, char**);
int _start_kernel(unsigned long p0, unsigned long p1);
void * malloc_aligned(size_t size, int align);

unsigned long exception_stack_frame;

snk_fileio_t fd_array[FILEIO_MAX];

extern uint64_t tb_freq;

int glue_init(snk_kernel_t *, unsigned int *, size_t, size_t);
void glue_release(void);

extern char _lowmem_start;
extern char _lowmem_end;
extern char __client_start;
extern char __client_end;

snk_kernel_t snk_kernel_interface = {
	.version          = 1,
	.print            = printk,
	.us_delay         = udelay,
	.ms_delay         = mdelay,
	.k_malloc         = malloc,
	.k_malloc_aligned = malloc_aligned,
	.k_free           = free,
	.strcmp           = strcmp,
	.snk_call         = main,
	.k_open           = open,
	.k_close          = close,
	.k_read           = read,
	.k_write          = write,
	.k_ioctl          = ioctl,
	.modules_remove   = rmmod_by_type,
	.modules_load     = insmod_by_type,
};

void * malloc_aligned(size_t size, int align)
{
	unsigned long p = (unsigned long) malloc(size + align - 1);
	p = p + align - 1;
	p = p & ~(align - 1);

	return (void *) p;
}

int _start_kernel(unsigned long p0, unsigned long p1)
{
	int rc;
	unsigned int timebase;

	/* initialize all file descriptor by marking them as empty */
	for(rc=0; rc<FILEIO_MAX; ++rc) {
		fd_array[rc].type = FILEIO_TYPE_EMPTY;
		fd_array[rc].idx  = rc;
	}

	/* this is step is e.g. resposible to initialize file descriptor 0 and 1 for STDIO */
	rc = glue_init(&snk_kernel_interface, &timebase, (size_t)(unsigned long)&__client_start,
	               (size_t)(unsigned long)&__client_end - (size_t)(unsigned long)&__client_start);
	if(rc < 0)
		return -1;

	tb_freq = (uint64_t) timebase;
	modules_init();
	rc = _start((unsigned char *) p0, p1);
	modules_term();

	glue_release();
	return rc;
}

