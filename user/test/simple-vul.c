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

// Hello world enclave program.
// The simplest case which uses opensgx ABI.
// See sgx/user/sgxLib.c and sgx/user/sgx-user.c for detail.

#include "test.h"

void vul(char *buf)
{
    char target[20];

    memcpy(target, buf, strlen(buf));

    sgx_enclave_write("Normal Run", 10);
    //sgx_enclave_write("Attack Success", 15);
}

void hack()
{
    puts("hack!!!");
}

void enclave_main()
{
    char buf[100];
    sgx_enclave_read(buf, 100);

    vul(buf);

    sgx_exit(NULL);
}
