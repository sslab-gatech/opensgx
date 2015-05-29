#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <sgx.h>
#include <sgx-user.h>
#include <sgx-kern.h>
#include <sgx-lib.h>

#define is_aligned(addr, bytes) \
    ((((uintptr_t)(const void *)(addr)) & (bytes - 1)) == 0)
