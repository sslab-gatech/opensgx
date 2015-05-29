/******************************************************************************
 * Copyright (c) 2011 IBM Corporation
 * All rights reserved.
 * This program and the accompanying materials
 * are made available under the terms of the BSD License
 * which accompanies this distribution, and is available at
 * http://www.opensource.org/licenses/bsd-license.php
 *
 * Contributors:
 *     IBM Corporation - initial implementation
 *****************************************************************************/
/*
 * A pseudo-module that uses the the "write" and "read" functions via the
 * client interface to handle the given device.
 * Normally the net-snk uses the various net_xxx modules from the romfs to
 * drive the network cards. However, in case we do not have a net-snk driver
 * for the given card and it has been initialized via FCODE instead, we've
 * got to use the Open Firmware Client Interface "write" and "read" functions
 * to talk to the NIC. This is achieved via this pseudo-module here.
 */

#include <of.h>
#include <string.h>
#include <netdriver_int.h>
#include <fileio.h>
#include <stdint.h>
#include <kernel.h>

#define DEBUG 0
#if DEBUG
#define	dprintf(str, ...)  snk_kernel_interface.print(str, ## __VA_ARGS__)
#else
#define	dprintf(str, ...) do{}while(0)
#endif

extern snk_kernel_t snk_kernel_interface;

snk_module_t * cimod_check_and_install(void);
static int cimod_init(void);
static int cimod_term(void);
static int cimod_read(char *buffer, int len);
static int cimod_write(char *buffer, int len);
static int cimod_ioctl(int request, void *data);

snk_module_t ci_module = {
	.version = 1,
	.type    = MOD_TYPE_NETWORK,
	.running = 0,
	.link_addr = (char*) 1,
	.init    = cimod_init,
	.term    = cimod_term,
	.write   = cimod_write,
	.read    = cimod_read,
	.ioctl   = cimod_ioctl
};

static ihandle_t myself;


snk_module_t *
cimod_check_and_install(void)
{
	uint8_t tmpbuf[8];

	dprintf("entered cimod_check_and_install!\n");

	myself = of_interpret_1("my-parent", tmpbuf);
	dprintf("cimod: myself=%lx\n", myself);

	/* Check whether "read" and "write" functions are provided by the
	 * device tree node: */
	if (of_read(myself, tmpbuf, 0) == -1
	    || of_write(myself, tmpbuf, 0) == -1) {
		dprintf("cimod: missing read or write!\n");
		return NULL;
	}

	return &ci_module;
}

static int
cimod_init(void)
{
	get_mac(&ci_module.mac_addr[0]);
	ci_module.running = 1;
	dprintf("client-interface module initialized!\n");
	return 0;
}

static int
cimod_term(void)
{
	ci_module.running = 0;
	dprintf("cimod term called!\n");
	return 0;
}

static int
cimod_read(char *buffer, int len)
{
	int ret;

	ret = of_read(myself, buffer, len);
	dprintf("cimod read returned: %i!\n", ret);

	return ret;
}

static int
cimod_write(char *buffer, int len)
{
	int ret;

	ret = of_write(myself, buffer, len);
	dprintf("cimod write returned: %i!\n", ret);

	return ret;
}

static int
cimod_ioctl(int request, void *data)
{
	dprintf("cimod ioctl called!\n");

	return 0;
}
