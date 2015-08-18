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

#include <string.h>
#include <sgx-kern.h>
#include <sgx-user.h>
#include <sgx-utils.h>
#include <sgx-crypto.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <sgx-malloc.h>

#include <sgx-lib.h>

extern void ENCT_START;
extern void ENCT_END;
extern void ENCD_START;
extern void ENCD_END;

void enclave_start() \
    __attribute__((section(".enc_text")));
void enclave_start()
{
    enclave_main();
    sgx_exit(NULL);
}

int main(int argc, char **argv)
{
    return 0;
}
