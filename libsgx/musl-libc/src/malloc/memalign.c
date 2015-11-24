#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "libc.h"

/* This function should work with most dlmalloc-like chunk bookkeeping
 * systems, but it's only guaranteed to work with the native implementation
 * used in this library. */

void *__memalign(size_t align, size_t len)
{
    void *mem = malloc(len + (align - 1));
    void *ptr = (void *)(((unsigned long)mem + ((unsigned long)align - 1)) & ~ ((unsigned long)align - 1));

    return ptr;
}

weak_alias(__memalign, memalign);
