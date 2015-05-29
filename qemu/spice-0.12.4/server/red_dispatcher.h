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

#ifndef _H_RED_DISPATCHER
#define _H_RED_DISPATCHER

#include "red_channel.h"

struct RedChannelClient;
typedef struct AsyncCommand AsyncCommand;

struct RedDispatcher *red_dispatcher_init(QXLInstance *qxl);

void red_dispatcher_set_mm_time(uint32_t);
void red_dispatcher_on_ic_change(void);
void red_dispatcher_on_sv_change(void);
void red_dispatcher_set_mouse_mode(uint32_t mode);
void red_dispatcher_on_vm_stop(void);
void red_dispatcher_on_vm_start(void);
int red_dispatcher_count(void);
int red_dispatcher_add_renderer(const char *name);
uint32_t red_dispatcher_qxl_ram_size(void);
int red_dispatcher_qxl_count(void);
void red_dispatcher_async_complete(struct RedDispatcher *, AsyncCommand *);
struct Dispatcher *red_dispatcher_get_dispatcher(struct RedDispatcher *);
int red_dispatcher_use_client_monitors_config(void);
void red_dispatcher_client_monitors_config(VDAgentMonitorsConfig *monitors_config);

typedef struct RedWorkerMessageDisplayConnect {
    RedClient * client;
    RedsStream * stream;
    uint32_t *common_caps; // red_worker should free
    uint32_t *caps;        // red_worker should free
    int migration;
    int num_common_caps;
    int num_caps;
} RedWorkerMessageDisplayConnect;

typedef struct RedWorkerMessageDisplayDisconnect {
    RedChannelClient *rcc;
} RedWorkerMessageDisplayDisconnect;

typedef struct RedWorkerMessageDisplayMigrate {
    RedChannelClient *rcc;
} RedWorkerMessageDisplayMigrate;

typedef struct RedWorkerMessageCursorConnect {
    RedClient *client;
    RedsStream *stream;
    int migration;
    uint32_t *common_caps; // red_worker should free
    int num_common_caps;
    uint32_t *caps;        // red_worker should free
    int num_caps;
} RedWorkerMessageCursorConnect;

typedef struct RedWorkerMessageCursorDisconnect {
    RedChannelClient *rcc;
} RedWorkerMessageCursorDisconnect;

typedef struct RedWorkerMessageCursorMigrate {
    RedChannelClient *rcc;
} RedWorkerMessageCursorMigrate;

typedef struct RedWorkerMessageUpdate {
    uint32_t surface_id;
    QXLRect * qxl_area;
    QXLRect * qxl_dirty_rects;
    uint32_t num_dirty_rects;
    uint32_t clear_dirty_region;
} RedWorkerMessageUpdate;

typedef struct RedWorkerMessageAsync {
    AsyncCommand *cmd;
} RedWorkerMessageAsync;

typedef struct RedWorkerMessageUpdateAsync {
    RedWorkerMessageAsync base;
    uint32_t surface_id;
    QXLRect qxl_area;
    uint32_t clear_dirty_region;
} RedWorkerMessageUpdateAsync;

typedef struct RedWorkerMessageAddMemslot {
    QXLDevMemSlot mem_slot;
} RedWorkerMessageAddMemslot;

typedef struct RedWorkerMessageAddMemslotAsync {
    RedWorkerMessageAsync base;
    QXLDevMemSlot mem_slot;
} RedWorkerMessageAddMemslotAsync;

typedef struct RedWorkerMessageDelMemslot {
    uint32_t slot_group_id;
    uint32_t slot_id;
} RedWorkerMessageDelMemslot;

typedef struct RedWorkerMessageDestroySurfaces {
} RedWorkerMessageDestroySurfaces;

typedef struct RedWorkerMessageDestroySurfacesAsync {
    RedWorkerMessageAsync base;
} RedWorkerMessageDestroySurfacesAsync;


typedef struct RedWorkerMessageDestroyPrimarySurface {
    uint32_t surface_id;
} RedWorkerMessageDestroyPrimarySurface;

typedef struct RedWorkerMessageDestroyPrimarySurfaceAsync {
    RedWorkerMessageAsync base;
    uint32_t surface_id;
} RedWorkerMessageDestroyPrimarySurfaceAsync;

typedef struct RedWorkerMessageCreatePrimarySurfaceAsync {
    RedWorkerMessageAsync base;
    uint32_t surface_id;
    QXLDevSurfaceCreate surface;
} RedWorkerMessageCreatePrimarySurfaceAsync;

typedef struct RedWorkerMessageCreatePrimarySurface {
    uint32_t surface_id;
    QXLDevSurfaceCreate surface;
} RedWorkerMessageCreatePrimarySurface;

typedef struct RedWorkerMessageResetImageCache {
} RedWorkerMessageResetImageCache;

typedef struct RedWorkerMessageResetCursor {
} RedWorkerMessageResetCursor;

typedef struct RedWorkerMessageWakeup {
} RedWorkerMessageWakeup;

typedef struct RedWorkerMessageOom {
} RedWorkerMessageOom;

typedef struct RedWorkerMessageStart {
} RedWorkerMessageStart;

typedef struct RedWorkerMessageFlushSurfacesAsync {
    RedWorkerMessageAsync base;
} RedWorkerMessageFlushSurfacesAsync;

typedef struct RedWorkerMessageStop {
} RedWorkerMessageStop;

/* this command is sync, so it's ok to pass a pointer */
typedef struct RedWorkerMessageLoadvmCommands {
    uint32_t count;
    QXLCommandExt *ext;
} RedWorkerMessageLoadvmCommands;

typedef struct RedWorkerMessageSetCompression {
    spice_image_compression_t image_compression;
} RedWorkerMessageSetCompression;

typedef struct RedWorkerMessageSetStreamingVideo {
    uint32_t streaming_video;
} RedWorkerMessageSetStreamingVideo;

typedef struct RedWorkerMessageSetMouseMode {
    uint32_t mode;
} RedWorkerMessageSetMouseMode;

typedef struct RedWorkerMessageDisplayChannelCreate {
} RedWorkerMessageDisplayChannelCreate;

typedef struct RedWorkerMessageCursorChannelCreate {
} RedWorkerMessageCursorChannelCreate;

typedef struct RedWorkerMessageDestroySurfaceWait {
    uint32_t surface_id;
} RedWorkerMessageDestroySurfaceWait;

typedef struct RedWorkerMessageDestroySurfaceWaitAsync {
    RedWorkerMessageAsync base;
    uint32_t surface_id;
} RedWorkerMessageDestroySurfaceWaitAsync;

typedef struct RedWorkerMessageResetMemslots {
} RedWorkerMessageResetMemslots;

typedef struct RedWorkerMessageMonitorsConfigAsync {
    RedWorkerMessageAsync base;
    QXLPHYSICAL monitors_config;
    int group_id;
} RedWorkerMessageMonitorsConfigAsync;

typedef struct RedWorkerMessageDriverUnload {
} RedWorkerMessageDriverUnload;

#endif
