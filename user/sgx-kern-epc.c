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

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <malloc.h>
#include <err.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdint.h>

#include <sgx-kern-epc.h>

//
// NOTE.
//   bitmap can contain more useful info (e.g., eid, contiguous region &c)
//
static epc_t *g_epc;
static epc_info_t *g_epc_info;
static int g_num_epc;

void init_epc(int nepc) {
    g_num_epc = nepc;

    //toward making g_num_epc configurable
    //g_epc = memalign(PAGE_SIZE, g_num_epc * sizeof(epc_t));

    g_epc = (epc_t *)mmap((void *)EPC_ADDR, g_num_epc * sizeof(epc_t),
                          PROT_READ|PROT_WRITE,
                          MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(g_epc == MAP_FAILED) {
        perror("EPC ALLOC FAIL");
        exit(EXIT_FAILURE);
    }


    sgx_dbg(kern, "g_epc: %p", (void *)g_epc);


    if (!g_epc)
        err(1, "failed to allocate EPC");

    g_epc_info = malloc(g_num_epc * sizeof(epc_info_t));
    if (!g_epc_info)
        err(1, "failed to allocate EPC map in kernel");

    memset(g_epc, 0, g_num_epc * sizeof(epc_t));
    memset(g_epc_info, 0, g_num_epc * sizeof(epc_info_t));
}

static
int get_epc_index(int key, epc_type_t pt)
{
    static int last = 0;
    for (int i = 0; i < g_num_epc; i++) {
        int idx = (i + last) % g_num_epc;
        if (g_epc_info[idx].key == key
            && g_epc_info[idx].type == RESERVED) {
            g_epc_info[idx].type = pt;
            return idx;
        }
    }
    return -1;
}

static
void put_epc_index(int index)
{
    assert(0 <= index && index < g_num_epc);
    assert(g_epc_info[index].type != FREE_PAGE);

    g_epc_info[index].type = FREE_PAGE;
}

epc_t *get_epc(int key, epc_type_t pt)
{
    int idx = get_epc_index(key, pt);
    if (idx != -1)
        return &g_epc[idx];
    return NULL;
}

epc_t *get_epc_region_beg(void)
{
    return &g_epc[0];
}

epc_t *get_epc_region_end(void)
{
    return &g_epc[g_num_epc];
}

static
const char *epc_bitmap_to_str(epc_type_t type)
{
    switch (type) {
        case FREE_PAGE: return "FREE";
        case SECS_PAGE: return "SECS";
        case TCS_PAGE : return "TCS ";
        case REG_PAGE : return "REG ";
        case RESERVED : return "RERV";
        default:
        {
            sgx_dbg(err, "unknown epc page type (%d)", type);
            assert(false);
        }
    }
}

void dbg_dump_epc(void)
{
    for (int i = 0; i < g_num_epc; i++) {
        fprintf(stderr, "[%02d] %p (%02d/%s)\n",
                i, g_epc[i],
                g_epc_info[i].key,
                epc_bitmap_to_str(g_epc_info[i].type));
    }
    fprintf(stderr, "\n");
}

// XXX. convert addr to index like free_epc_pages
int find_epc_type(void *addr)
{
    for (int i = 0; i < g_num_epc; i++) {
        if (addr == &g_epc[i])
            return g_epc_info[i].type;
    }

    return -1;
}

static
int reserve_epc_index(int key)
{
    static int last = 0;
    for (int i = 0; i < g_num_epc; i++) {
        int idx = (i + last) % g_num_epc;
        if (g_epc_info[idx].type == FREE_PAGE) {
            g_epc_info[idx].key = key;
            g_epc_info[idx].type = RESERVED;
            return idx;
        }
    }
    return -1;
}

static
int alloc_epc_index_pages(int npages, int key)
{
    int beg = reserve_epc_index(key);
    if (beg == -1)
        return -1;

    // request too many pages
    if (beg + npages >= g_num_epc) {
        put_epc_index(beg);
        return -1;
    }

    // check if we have npages
    int i;
    for (i = beg + 1; i < beg + npages; i++) {
        if (g_epc_info[i].type != FREE_PAGE) {
            // restore and return
            for (int j = beg; j < i; j ++) {
                put_epc_index(i);
            }
            return -1;
        }
        g_epc_info[i].key = key;
        g_epc_info[i].type = RESERVED;
    }

    // npages epcs allocated
    return beg;
}

epc_t *alloc_epc_pages(int npages, int key)
{
    int idx = alloc_epc_index_pages(npages, key);
    if (idx != -1)
        return &g_epc[idx];
    return NULL;
}

epc_t *alloc_epc_page(int key)
{
    int idx = reserve_epc_index(key);
    if (idx != -1)
        return &g_epc[idx];
    return NULL;
}

void free_reserved_epc_pages(epc_t *epc)
{
    int beg = ((unsigned long)epc - (unsigned long)&g_epc[0]) / sizeof(epc_t);
    int key = g_epc_info[beg].key;

    for (int i = beg; i < g_num_epc; i ++) {
        if (g_epc_info[i].key == key && g_epc_info[i].type == RESERVED) {
            g_epc_info[i].key = 0;
            g_epc_info[i].type = FREE_PAGE;
        }
    }
}

void free_epc_pages(epc_t *epc)
{
    int beg = ((unsigned long)epc - (unsigned long)&g_epc[0]) / sizeof(epc_t);
    int key = g_epc_info[beg].key;

    for (int i = beg; i < g_num_epc; i ++) {
        if (g_epc_info[i].key == key) {
            g_epc_info[i].key = 0;
            g_epc_info[i].type = FREE_PAGE;
        }
    }
}

#ifdef UNITTEST
int count_epc(int key)
{
    int cnt = 0;
    for (int i = 0; i < g_num_epc; i ++) {
        if (g_epc_info[i].type != FREE_PAGE
            && g_epc_info[i].key == key) {
            cnt ++;
        }
    }
    return cnt;
}

int main(int argc, char *argv[])
{
    init_epc(NUM_EPC);

    epc_t *epc = alloc_epc_pages(NUM_EPC/2, 1);
    assert(count_epc(1) == NUM_EPC/2);
    dbg_dump_epc();
    free_epc_pages(epc);
    assert(count_epc(1) == 0);

    (void) alloc_epc_pages(2, 2);
    epc =  alloc_epc_pages(3, 3);
    (void) alloc_epc_pages(4, 4);

    assert(count_epc(2) + count_epc(3) + count_epc(4) == 9);

    assert(get_epc(2, SECS_PAGE) != 0);
    assert(get_epc(2, SECS_PAGE) != 0);
    assert(get_epc(2, SECS_PAGE) == 0);

    dbg_dump_epc();

    free_epc_pages(epc);

    dbg_dump_epc();

    return 0;
}
#endif
