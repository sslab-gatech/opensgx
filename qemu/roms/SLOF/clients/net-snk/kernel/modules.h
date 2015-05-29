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

extern void modules_init(void);
extern void modules_term(void);
extern snk_module_t *insmod_by_type(int);
extern void rmmod_by_type(int);
extern snk_module_t *get_module_by_type(int type);
