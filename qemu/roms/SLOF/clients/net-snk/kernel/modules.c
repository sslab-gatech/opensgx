/******************************************************************************
 * Copyright (c) 2004, 2011 IBM Corporation
 * All rights reserved.
 * This program and the accompanying materials
 * are made available under the terms of the BSD License
 * which accompanies this distribution, and is available at
 * http://www.opensource.org/licenses/bsd-license.php
 *
 * Contributors:
 *     IBM Corporation - initial implementation
 *****************************************************************************/

#include <netdriver_int.h>
#include <kernel.h>
#include <of.h>
#include <rtas.h> 
#include <libelf.h>
#include <cpu.h> /* flush_cache */
#include <unistd.h> /* open, close, read, write */
#include <stdio.h>
#include <pci.h>
#include "modules.h"

snk_module_t * cimod_check_and_install(void);

extern snk_module_t of_module, ci_module;

extern char __client_start[];


typedef snk_module_t *(*module_init_t) (snk_kernel_t *, pci_config_t *);

snk_module_t *snk_modules[MODULES_MAX];

extern snk_kernel_t snk_kernel_interface;

/* Load module and call init code.
   Init code will check, if module is responsible for device.
   Returns -1, if not responsible for device, 0 otherwise.
*/

void
modules_init(void)
{
	int i;

	snk_kernel_interface.io_read  = read_io;
	snk_kernel_interface.io_write = write_io;

	snk_modules[0] = &of_module;

	/* Setup Module List */
	for(i=1; i<MODULES_MAX; ++i) {
		snk_modules[i] = 0;
	}

	/* Try to init client-interface module (it's built-in, not loadable) */
	for(i=0; i<MODULES_MAX; ++i) {
		if(snk_modules[i] == 0) {
			snk_modules[i] = cimod_check_and_install();
			break;
		}
	}
}

void
modules_term(void)
{
	int i;

	/* remove all modules */
	for(i=0; i<MODULES_MAX; ++i) {
		if(snk_modules[i] && snk_modules[i]->running != 0) {
			snk_modules[i]->term();
		}
		snk_modules[i] = 0;
	}
}

snk_module_t *
get_module_by_type(int type) {
	int i;

	for(i=0; i<MODULES_MAX; ++i) {
		if(snk_modules[i] && snk_modules[i]->type == type) {
			return snk_modules[i];
		}
	}
	return 0;
}

/**
 * insmod_by_type - Load first module of given type
 *
 * @param type  Type of module that we want to load
 * @return      module descriptor on success
 *              NULL              if not successful
 */
snk_module_t *
insmod_by_type(int type) {
	return 0;
}

/**
 * rmmod_by_type - Remove all module of given type
 *
 * @param type  Type of module that we want to load
 */
void
rmmod_by_type(int type) {
	int i;

	for (i = 0; i < MODULES_MAX; ++i) {
		if (snk_modules[i] && snk_modules[i]->type == type) {
			if (snk_modules[i]->running)
				snk_modules[i]->term();
			snk_modules[i] = 0;
		}
	}
}
