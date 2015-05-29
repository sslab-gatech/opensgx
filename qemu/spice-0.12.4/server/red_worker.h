/*
   Copyright (C) 2009 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _H_REDWORKER
#define _H_REDWORKER

#include <unistd.h>
#include <errno.h>
#include "red_common.h"


static inline void set_bit(int index, uint32_t *addr)
{
    uint32_t mask = 1 << index;
    __sync_or_and_fetch(addr, mask);
}

static inline void clear_bit(int index, uint32_t *addr)
{
    uint32_t mask = ~(1 << index);
    __sync_and_and_fetch(addr, mask);
}

static inline int test_bit(int index, uint32_t val)
{
    return val & (1u << index);
}

enum {
    RED_WORKER_PENDING_WAKEUP,
    RED_WORKER_PENDING_OOM,
};

enum {
    RED_WORKER_MESSAGE_NOP,
    RED_WORKER_MESSAGE_UPDATE,
    RED_WORKER_MESSAGE_WAKEUP,
    RED_WORKER_MESSAGE_OOM,
    RED_WORKER_MESSAGE_READY,
    RED_WORKER_MESSAGE_DISPLAY_CONNECT,
    RED_WORKER_MESSAGE_DISPLAY_DISCONNECT,
    RED_WORKER_MESSAGE_DISPLAY_MIGRATE,
    RED_WORKER_MESSAGE_START,
    RED_WORKER_MESSAGE_STOP,
    RED_WORKER_MESSAGE_CURSOR_CONNECT,
    RED_WORKER_MESSAGE_CURSOR_DISCONNECT,
    RED_WORKER_MESSAGE_CURSOR_MIGRATE,
    RED_WORKER_MESSAGE_SET_COMPRESSION,
    RED_WORKER_MESSAGE_SET_STREAMING_VIDEO,
    RED_WORKER_MESSAGE_SET_MOUSE_MODE,
    RED_WORKER_MESSAGE_ADD_MEMSLOT,
    RED_WORKER_MESSAGE_DEL_MEMSLOT,
    RED_WORKER_MESSAGE_RESET_MEMSLOTS,
    RED_WORKER_MESSAGE_DESTROY_SURFACES,
    RED_WORKER_MESSAGE_CREATE_PRIMARY_SURFACE,
    RED_WORKER_MESSAGE_DESTROY_PRIMARY_SURFACE,
    RED_WORKER_MESSAGE_RESET_CURSOR,
    RED_WORKER_MESSAGE_RESET_IMAGE_CACHE,
    RED_WORKER_MESSAGE_DESTROY_SURFACE_WAIT,
    RED_WORKER_MESSAGE_LOADVM_COMMANDS,
    /* async commands */
    RED_WORKER_MESSAGE_UPDATE_ASYNC,
    RED_WORKER_MESSAGE_ADD_MEMSLOT_ASYNC,
    RED_WORKER_MESSAGE_DESTROY_SURFACES_ASYNC,
    RED_WORKER_MESSAGE_CREATE_PRIMARY_SURFACE_ASYNC,
    RED_WORKER_MESSAGE_DESTROY_PRIMARY_SURFACE_ASYNC,
    RED_WORKER_MESSAGE_DESTROY_SURFACE_WAIT_ASYNC,
    /* suspend/windows resolution change command */
    RED_WORKER_MESSAGE_FLUSH_SURFACES_ASYNC,

    RED_WORKER_MESSAGE_DISPLAY_CHANNEL_CREATE,
    RED_WORKER_MESSAGE_CURSOR_CHANNEL_CREATE,

    RED_WORKER_MESSAGE_MONITORS_CONFIG_ASYNC,
    RED_WORKER_MESSAGE_DRIVER_UNLOAD,

    RED_WORKER_MESSAGE_COUNT // LAST
};

typedef uint32_t RedWorkerMessage;

#define RED_MAX_RENDERERS 4

enum {
    RED_RENDERER_INVALID,
    RED_RENDERER_SW,
    RED_RENDERER_OGL_PBUF,
    RED_RENDERER_OGL_PIXMAP,
};

typedef struct RedDispatcher RedDispatcher;

typedef struct WorkerInitData {
    struct QXLInstance *qxl;
    int id;
    uint32_t *pending;
    uint32_t num_renderers;
    uint32_t renderers[RED_MAX_RENDERERS];
    spice_image_compression_t image_compression;
    spice_wan_compression_t jpeg_state;
    spice_wan_compression_t zlib_glz_state;
    int streaming_video;
    uint32_t num_memslots;
    uint32_t num_memslots_groups;
    uint8_t memslot_gen_bits;
    uint8_t memslot_id_bits;
    uint8_t internal_groupslot_id;
    uint32_t n_surfaces;
    RedDispatcher *red_dispatcher;
} WorkerInitData;

void *red_worker_main(void *arg);

static inline void send_data(int fd, void *in_buf, int n)
{
    uint8_t *buf = in_buf;
    do {
        int now;
        if ((now = write(fd, buf, n)) == -1) {
            if (errno == EINTR) {
                continue;
            }
            spice_error("%s", strerror(errno));
        }
        buf += now;
        n -= now;
    } while (n);
}

static inline void write_message(int fd, RedWorkerMessage *message)
{
    send_data(fd, message, sizeof(RedWorkerMessage));
}

static inline void receive_data(int fd, void *in_buf, int n)
{
    uint8_t *buf = in_buf;
    do {
        int now;
        if ((now = read(fd, buf, n)) == -1) {
            if (errno == EINTR) {
                continue;
            }
            spice_error("%s", strerror(errno));
        }
        buf += now;
        n -= now;
    } while (n);
}

static inline void read_message(int fd, RedWorkerMessage *message)
{
    receive_data(fd, message, sizeof(RedWorkerMessage));
}

#endif
