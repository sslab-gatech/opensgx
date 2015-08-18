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

// test openssl api

#include <sgx-lib.h>
#include <sgx.h>

#include "tp-lib.h"

int compare(const void *a, const void *b)
{
    return (*(int *)a - *(int *)b);
}

int values[] = { 40, 10, 100, 90, 20, 25 };

void enclave_main()
{
    sgx_debug("qsort test\n");

    sgx_qsort(values, 6, sizeof(int), compare);

    sgx_printf("%d %d %d %d %d %d\n", values[0], values[1], values[2], values[3], values[4], values[5]);

    sgx_exit(NULL);
}
