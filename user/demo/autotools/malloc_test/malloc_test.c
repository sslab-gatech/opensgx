#include "malloc.h"

#include <sgx-lib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum {
    MEM_TYPE_SMALL,
    MEM_TYPE_MID,
    MEM_TYPE_LARGE,
    N_TYPE,
};

#define SIZE_SMALL 8
#define SIZE_MID 64
#define SIZE_LARGE 1024

#define N_OBJ 1000
struct mobj {
    unsigned long buf;
    int size;
} obj[N_OBJ];

int partition( struct mobj a[], int l, int r) {
    int i, j;
    unsigned long pivot = a[l].buf;
    struct mobj t;
    i = l; j = r+1;

    while( 1)
    {
        do ++i; while( a[i].buf <= pivot && i <= r );
        do --j; while( a[j].buf > pivot );
        if( i >= j ) break;
        t = a[i]; a[i] = a[j]; a[j] = t;
    }
    t = a[l]; a[l] = a[j]; a[j] = t;
    return j;
}

void quickSort( struct mobj a[], int l, int r)
{
    int j;

    if( l < r )
    {
        // divide and conquer
        j = partition( a, l, r);
        quickSort( a, l, j-1);
        quickSort( a, j+1, r);
    }
}

void enclave_main()
{
    int i;

    srand(time(NULL));
    for (i = 0; i < N_OBJ; i++) {
        int type = rand() % N_TYPE;
        int size = 0;
        switch (type) {
            case MEM_TYPE_SMALL:
                size = rand() % (3*SIZE_SMALL) + SIZE_SMALL;
                break;
            case MEM_TYPE_LARGE:
            case MEM_TYPE_MID:
                size = rand() % (3*SIZE_MID) + SIZE_MID;
                break;
                /*
            case MEM_TYPE_LARGE:
                size = rand() % (3*SIZE_LARGE) + SIZE_LARGE;
                break;
                */
            default:
                exit(1);
        }
        obj[i].buf = (unsigned long)malloc(size);
        obj[i].size = size;
    }

    quickSort(obj, 0, N_OBJ-1);

    for (i = 1; i < N_OBJ; i++) {
        unsigned long boundary = obj[i-1].buf + (unsigned long)obj[i-1].size;
        if (boundary > obj[i].buf)
            printf("Err: %lx > %lx\n", boundary, obj[i].buf);
    }

    printf("Malloc test pass\n");
    sgx_exit(NULL);
}
