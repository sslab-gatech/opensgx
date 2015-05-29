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

#ifndef _H_SPICE
#define _H_SPICE

#include <stdint.h>
#include <sys/socket.h>
#include <spice/qxl_dev.h>
#include <spice/vd_agent.h>

#define SPICE_SERVER_VERSION 0x000c04 /* release 0.12.4 */

/* interface base type */

typedef struct SpiceBaseInterface SpiceBaseInterface;
typedef struct SpiceBaseInstance SpiceBaseInstance;

struct SpiceBaseInterface {
    const char *type;
    const char *description;
    uint32_t major_version;
    uint32_t minor_version;
};
struct SpiceBaseInstance {
    const SpiceBaseInterface *sif;
};

/* core interface */

#define SPICE_INTERFACE_CORE "core"
#define SPICE_INTERFACE_CORE_MAJOR 1
#define SPICE_INTERFACE_CORE_MINOR 3
typedef struct SpiceCoreInterface SpiceCoreInterface;

#define SPICE_WATCH_EVENT_READ  (1 << 0)
#define SPICE_WATCH_EVENT_WRITE (1 << 1)

#define SPICE_CHANNEL_EVENT_CONNECTED     1
#define SPICE_CHANNEL_EVENT_INITIALIZED   2
#define SPICE_CHANNEL_EVENT_DISCONNECTED  3

#define SPICE_CHANNEL_EVENT_FLAG_TLS      (1 << 0)
#define SPICE_CHANNEL_EVENT_FLAG_ADDR_EXT (1 << 1)

typedef struct SpiceWatch SpiceWatch;
typedef void (*SpiceWatchFunc)(int fd, int event, void *opaque);

typedef struct SpiceTimer SpiceTimer;
typedef void (*SpiceTimerFunc)(void *opaque);

typedef struct SpiceChannelEventInfo {
    int connection_id;
    int type;
    int id;
    int flags;
    /* deprecated, can't hold ipv6 addresses, kept for backward compatibility */
    struct sockaddr laddr;
    struct sockaddr paddr;
    socklen_t llen, plen;
    /* should be used if (flags & SPICE_CHANNEL_EVENT_FLAG_ADDR_EXT) */
    struct sockaddr_storage laddr_ext;
    struct sockaddr_storage paddr_ext;
    socklen_t llen_ext, plen_ext;
} SpiceChannelEventInfo;

struct SpiceCoreInterface {
    SpiceBaseInterface base;

    SpiceTimer *(*timer_add)(SpiceTimerFunc func, void *opaque);
    void (*timer_start)(SpiceTimer *timer, uint32_t ms);
    void (*timer_cancel)(SpiceTimer *timer);
    void (*timer_remove)(SpiceTimer *timer);

    SpiceWatch *(*watch_add)(int fd, int event_mask, SpiceWatchFunc func, void *opaque);
    void (*watch_update_mask)(SpiceWatch *watch, int event_mask);
    void (*watch_remove)(SpiceWatch *watch);

    void (*channel_event)(int event, SpiceChannelEventInfo *info);
};

/* qxl interface */

#define SPICE_INTERFACE_QXL "qxl"
#define SPICE_INTERFACE_QXL_MAJOR 3
#define SPICE_INTERFACE_QXL_MINOR 3
typedef struct QXLInterface QXLInterface;
typedef struct QXLInstance QXLInstance;
typedef struct QXLState QXLState;
typedef struct QXLWorker QXLWorker;
typedef struct QXLDevMemSlot QXLDevMemSlot;
typedef struct QXLDevSurfaceCreate QXLDevSurfaceCreate;
union QXLReleaseInfo;
struct QXLReleaseInfoExt;
struct QXLCommand;
struct QXLCommandExt;
struct QXLRect;
struct QXLWorker {
    uint32_t minor_version;
    uint32_t major_version;
    /* These calls are deprecated. Please use the spice_qxl_* calls instead */
    void (*wakeup)(QXLWorker *worker);
    void (*oom)(QXLWorker *worker);
    void (*start)(QXLWorker *worker);
    void (*stop)(QXLWorker *worker);
    void (*update_area)(QXLWorker *qxl_worker, uint32_t surface_id,
                       struct QXLRect *area, struct QXLRect *dirty_rects,
                       uint32_t num_dirty_rects, uint32_t clear_dirty_region);
    void (*add_memslot)(QXLWorker *worker, QXLDevMemSlot *slot);
    void (*del_memslot)(QXLWorker *worker, uint32_t slot_group_id, uint32_t slot_id);
    void (*reset_memslots)(QXLWorker *worker);
    void (*destroy_surfaces)(QXLWorker *worker);
    void (*destroy_primary_surface)(QXLWorker *worker, uint32_t surface_id);
    void (*create_primary_surface)(QXLWorker *worker, uint32_t surface_id,
                                   QXLDevSurfaceCreate *surface);
    void (*reset_image_cache)(QXLWorker *worker);
    void (*reset_cursor)(QXLWorker *worker);
    void (*destroy_surface_wait)(QXLWorker *worker, uint32_t surface_id);
    void (*loadvm_commands)(QXLWorker *worker, struct QXLCommandExt *ext, uint32_t count);
};

void spice_qxl_wakeup(QXLInstance *instance);
void spice_qxl_oom(QXLInstance *instance);
void spice_qxl_start(QXLInstance *instance); /* deprecated since 0.11.2
                                                spice_server_vm_start replaces it */
void spice_qxl_stop(QXLInstance *instance);  /* deprecated since 0.11.2
                                                spice_server_vm_stop replaces it */
void spice_qxl_update_area(QXLInstance *instance, uint32_t surface_id,
                   struct QXLRect *area, struct QXLRect *dirty_rects,
                   uint32_t num_dirty_rects, uint32_t clear_dirty_region);
void spice_qxl_add_memslot(QXLInstance *instance, QXLDevMemSlot *slot);
void spice_qxl_del_memslot(QXLInstance *instance, uint32_t slot_group_id, uint32_t slot_id);
void spice_qxl_reset_memslots(QXLInstance *instance);
void spice_qxl_destroy_surfaces(QXLInstance *instance);
void spice_qxl_destroy_primary_surface(QXLInstance *instance, uint32_t surface_id);
void spice_qxl_create_primary_surface(QXLInstance *instance, uint32_t surface_id,
                               QXLDevSurfaceCreate *surface);
void spice_qxl_reset_image_cache(QXLInstance *instance);
void spice_qxl_reset_cursor(QXLInstance *instance);
void spice_qxl_destroy_surface_wait(QXLInstance *instance, uint32_t surface_id);
void spice_qxl_loadvm_commands(QXLInstance *instance, struct QXLCommandExt *ext, uint32_t count);
/* async versions of commands. when complete spice calls async_complete */
void spice_qxl_update_area_async(QXLInstance *instance, uint32_t surface_id, QXLRect *qxl_area,
                                 uint32_t clear_dirty_region, uint64_t cookie);
void spice_qxl_add_memslot_async(QXLInstance *instance, QXLDevMemSlot *slot, uint64_t cookie);
void spice_qxl_destroy_surfaces_async(QXLInstance *instance, uint64_t cookie);
void spice_qxl_destroy_primary_surface_async(QXLInstance *instance, uint32_t surface_id, uint64_t cookie);
void spice_qxl_create_primary_surface_async(QXLInstance *instance, uint32_t surface_id,
                                QXLDevSurfaceCreate *surface, uint64_t cookie);
void spice_qxl_destroy_surface_async(QXLInstance *instance, uint32_t surface_id, uint64_t cookie);
/* suspend and resolution change on windows drivers */
void spice_qxl_flush_surfaces_async(QXLInstance *instance, uint64_t cookie);
/* since spice 0.12.0 */
void spice_qxl_monitors_config_async(QXLInstance *instance, QXLPHYSICAL monitors_config,
                                     int group_id, uint64_t cookie);
/* since spice 0.12.3 */
void spice_qxl_driver_unload(QXLInstance *instance);

typedef struct QXLDrawArea {
    uint8_t *buf;
    uint32_t size;
    uint8_t *line_0;
    uint32_t width;
    uint32_t heigth;
    int stride;
} QXLDrawArea;

typedef struct QXLDevInfo {
    uint32_t x_res;
    uint32_t y_res;
    uint32_t bits;
    uint32_t use_hardware_cursor;

    QXLDrawArea draw_area;

    uint32_t ram_size;
} QXLDevInfo;

typedef struct QXLDevInitInfo {
    uint32_t num_memslots_groups;
    uint32_t num_memslots;
    uint8_t memslot_gen_bits;
    uint8_t memslot_id_bits;
    uint32_t qxl_ram_size;
    uint8_t internal_groupslot_id;
    uint32_t n_surfaces;
} QXLDevInitInfo;

struct QXLDevMemSlot {
    uint32_t slot_group_id;
    uint32_t slot_id;
    uint32_t generation;
    unsigned long virt_start;
    unsigned long virt_end;
    uint64_t addr_delta;
    uint32_t qxl_ram_size;
};

struct QXLDevSurfaceCreate {
    uint32_t width;
    uint32_t height;
    int32_t stride;
    uint32_t format;
    uint32_t position;
    uint32_t mouse_mode;
    uint32_t flags;
    uint32_t type;
    uint64_t mem;
    uint32_t group_id;
};

struct QXLInterface {
    SpiceBaseInterface base;

    void (*attache_worker)(QXLInstance *qin, QXLWorker *qxl_worker);
    void (*set_compression_level)(QXLInstance *qin, int level);
    void (*set_mm_time)(QXLInstance *qin, uint32_t mm_time);

    void (*get_init_info)(QXLInstance *qin, QXLDevInitInfo *info);
    int (*get_command)(QXLInstance *qin, struct QXLCommandExt *cmd);
    int (*req_cmd_notification)(QXLInstance *qin);
    void (*release_resource)(QXLInstance *qin, struct QXLReleaseInfoExt release_info);
    int (*get_cursor_command)(QXLInstance *qin, struct QXLCommandExt *cmd);
    int (*req_cursor_notification)(QXLInstance *qin);
    void (*notify_update)(QXLInstance *qin, uint32_t update_id);
    int (*flush_resources)(QXLInstance *qin);
    void (*async_complete)(QXLInstance *qin, uint64_t cookie);
    void (*update_area_complete)(QXLInstance *qin, uint32_t surface_id,
                                 struct QXLRect *updated_rects,
                                 uint32_t num_updated_rects);
    void (*set_client_capabilities)(QXLInstance *qin,
                                    uint8_t client_present,
                                    uint8_t caps[58]);
    /* returns 1 if the interface is supported, 0 otherwise.
     * if monitors_config is NULL nothing is done except reporting the
     * return code. */
    int (*client_monitors_config)(QXLInstance *qin,
                                  VDAgentMonitorsConfig *monitors_config);
};

struct QXLInstance {
    SpiceBaseInstance  base;
    int                id;
    QXLState           *st;
};

/* input interfaces */

#define SPICE_INTERFACE_KEYBOARD "keyboard"
#define SPICE_INTERFACE_KEYBOARD_MAJOR 1
#define SPICE_INTERFACE_KEYBOARD_MINOR 1
typedef struct SpiceKbdInterface SpiceKbdInterface;
typedef struct SpiceKbdInstance SpiceKbdInstance;
typedef struct SpiceKbdState SpiceKbdState;

struct SpiceKbdInterface {
    SpiceBaseInterface base;

    void (*push_scan_freg)(SpiceKbdInstance *sin, uint8_t frag);
    uint8_t (*get_leds)(SpiceKbdInstance *sin);
};

struct SpiceKbdInstance {
    SpiceBaseInstance base;
    SpiceKbdState     *st;
};

int spice_server_kbd_leds(SpiceKbdInstance *sin, int leds);

#define SPICE_INTERFACE_MOUSE "mouse"
#define SPICE_INTERFACE_MOUSE_MAJOR 1
#define SPICE_INTERFACE_MOUSE_MINOR 1
typedef struct SpiceMouseInterface SpiceMouseInterface;
typedef struct SpiceMouseInstance SpiceMouseInstance;
typedef struct SpiceMouseState SpiceMouseState;

struct SpiceMouseInterface {
    SpiceBaseInterface base;

    void (*motion)(SpiceMouseInstance *sin, int dx, int dy, int dz,
                   uint32_t buttons_state);
    void (*buttons)(SpiceMouseInstance *sin, uint32_t buttons_state);
};

struct SpiceMouseInstance {
    SpiceBaseInstance base;
    SpiceMouseState   *st;
};

#define SPICE_INTERFACE_TABLET "tablet"
#define SPICE_INTERFACE_TABLET_MAJOR 1
#define SPICE_INTERFACE_TABLET_MINOR 1
typedef struct SpiceTabletInterface SpiceTabletInterface;
typedef struct SpiceTabletInstance SpiceTabletInstance;
typedef struct SpiceTabletState SpiceTabletState;

struct SpiceTabletInterface {
    SpiceBaseInterface base;

    void (*set_logical_size)(SpiceTabletInstance* tablet, int width, int height);
    void (*position)(SpiceTabletInstance* tablet, int x, int y, uint32_t buttons_state);
    void (*wheel)(SpiceTabletInstance* tablet, int wheel_moution, uint32_t buttons_state);
    void (*buttons)(SpiceTabletInstance* tablet, uint32_t buttons_state);
};

struct SpiceTabletInstance {
    SpiceBaseInstance base;
    SpiceTabletState  *st;
};

/* sound interfaces */

#define SPICE_INTERFACE_PLAYBACK "playback"
#define SPICE_INTERFACE_PLAYBACK_MAJOR 1
#define SPICE_INTERFACE_PLAYBACK_MINOR 2
typedef struct SpicePlaybackInterface SpicePlaybackInterface;
typedef struct SpicePlaybackInstance SpicePlaybackInstance;
typedef struct SpicePlaybackState SpicePlaybackState;

enum {
    SPICE_INTERFACE_AUDIO_FMT_S16 = 1,
};

#define SPICE_INTERFACE_PLAYBACK_FREQ  44100
#define SPICE_INTERFACE_PLAYBACK_CHAN  2
#define SPICE_INTERFACE_PLAYBACK_FMT   SPICE_INTERFACE_AUDIO_FMT_S16

struct SpicePlaybackInterface {
    SpiceBaseInterface base;
};

struct SpicePlaybackInstance {
    SpiceBaseInstance  base;
    SpicePlaybackState *st;
};

void spice_server_playback_start(SpicePlaybackInstance *sin);
void spice_server_playback_stop(SpicePlaybackInstance *sin);
void spice_server_playback_get_buffer(SpicePlaybackInstance *sin,
                                      uint32_t **samples, uint32_t *nsamples);
void spice_server_playback_put_samples(SpicePlaybackInstance *sin,
                                       uint32_t *samples);
void spice_server_playback_set_volume(SpicePlaybackInstance *sin,
                                      uint8_t nchannels, uint16_t *volume);
void spice_server_playback_set_mute(SpicePlaybackInstance *sin, uint8_t mute);

#define SPICE_INTERFACE_RECORD "record"
#define SPICE_INTERFACE_RECORD_MAJOR 2
#define SPICE_INTERFACE_RECORD_MINOR 2
typedef struct SpiceRecordInterface SpiceRecordInterface;
typedef struct SpiceRecordInstance SpiceRecordInstance;
typedef struct SpiceRecordState SpiceRecordState;

#define SPICE_INTERFACE_RECORD_FREQ  44100
#define SPICE_INTERFACE_RECORD_CHAN  2
#define SPICE_INTERFACE_RECORD_FMT   SPICE_INTERFACE_AUDIO_FMT_S16

struct SpiceRecordInterface {
    SpiceBaseInterface base;
};

struct SpiceRecordInstance {
    SpiceBaseInstance base;
    SpiceRecordState  *st;
};

void spice_server_record_start(SpiceRecordInstance *sin);
void spice_server_record_stop(SpiceRecordInstance *sin);
uint32_t spice_server_record_get_samples(SpiceRecordInstance *sin,
                                         uint32_t *samples, uint32_t bufsize);
void spice_server_record_set_volume(SpiceRecordInstance *sin,
                                    uint8_t nchannels, uint16_t *volume);
void spice_server_record_set_mute(SpiceRecordInstance *sin, uint8_t mute);

/* char device interfaces */

#define SPICE_INTERFACE_CHAR_DEVICE "char_device"
#define SPICE_INTERFACE_CHAR_DEVICE_MAJOR 1
#define SPICE_INTERFACE_CHAR_DEVICE_MINOR 2
typedef struct SpiceCharDeviceInterface SpiceCharDeviceInterface;
typedef struct SpiceCharDeviceInstance SpiceCharDeviceInstance;
typedef struct SpiceCharDeviceState SpiceCharDeviceState;

struct SpiceCharDeviceInterface {
    SpiceBaseInterface base;

    void (*state)(SpiceCharDeviceInstance *sin, int connected);
    int (*write)(SpiceCharDeviceInstance *sin, const uint8_t *buf, int len);
    int (*read)(SpiceCharDeviceInstance *sin, uint8_t *buf, int len);
    void (*event)(SpiceCharDeviceInstance *sin, uint8_t event);
};

struct SpiceCharDeviceInstance {
    SpiceBaseInstance base;
    const char* subtype;
    SpiceCharDeviceState *st;
    const char* portname;
};

void spice_server_char_device_wakeup(SpiceCharDeviceInstance *sin);
void spice_server_port_event(SpiceCharDeviceInstance *char_device, uint8_t event);
const char** spice_server_char_device_recognized_subtypes(void);

/* spice server setup */

/* Don't use features incompatible with a specific spice
   version, so that migration to/from that version works. */
typedef enum {
    SPICE_COMPAT_VERSION_0_4 = 0,
    SPICE_COMPAT_VERSION_0_6 = 1,
} spice_compat_version_t;

#define SPICE_COMPAT_VERSION_CURRENT SPICE_COMPAT_VERSION_0_6

spice_compat_version_t spice_get_current_compat_version(void);

typedef struct RedsState SpiceServer;
SpiceServer *spice_server_new(void);
int spice_server_init(SpiceServer *s, SpiceCoreInterface *core);
void spice_server_destroy(SpiceServer *s);

#define SPICE_ADDR_FLAG_IPV4_ONLY (1 << 0)
#define SPICE_ADDR_FLAG_IPV6_ONLY (1 << 1)

int spice_server_set_compat_version(SpiceServer *s,
                                    spice_compat_version_t version);
int spice_server_set_port(SpiceServer *s, int port);
void spice_server_set_addr(SpiceServer *s, const char *addr, int flags);
int spice_server_set_listen_socket_fd(SpiceServer *s, int listen_fd);
int spice_server_set_exit_on_disconnect(SpiceServer *s, int flag);
int spice_server_set_noauth(SpiceServer *s);
int spice_server_set_sasl(SpiceServer *s, int enabled);
int spice_server_set_sasl_appname(SpiceServer *s, const char *appname);
int spice_server_set_ticket(SpiceServer *s, const char *passwd, int lifetime,
                            int fail_if_connected, int disconnect_if_connected);
int spice_server_set_tls(SpiceServer *s, int port,
                         const char *ca_cert_file, const char *certs_file,
                         const char *private_key_file, const char *key_passwd,
                         const char *dh_key_file, const char *ciphersuite);

int spice_server_add_client(SpiceServer *s, int socket, int skip_auth);
int spice_server_add_ssl_client(SpiceServer *s, int socket, int skip_auth);

int spice_server_add_interface(SpiceServer *s,
                               SpiceBaseInstance *sin);
int spice_server_remove_interface(SpiceBaseInstance *sin);

typedef enum {
    SPICE_IMAGE_COMPRESS_INVALID  = 0,
    SPICE_IMAGE_COMPRESS_OFF      = 1,
    SPICE_IMAGE_COMPRESS_AUTO_GLZ = 2,
    SPICE_IMAGE_COMPRESS_AUTO_LZ  = 3,
    SPICE_IMAGE_COMPRESS_QUIC     = 4,
    SPICE_IMAGE_COMPRESS_GLZ      = 5,
    SPICE_IMAGE_COMPRESS_LZ       = 6,
} spice_image_compression_t;

int spice_server_set_image_compression(SpiceServer *s,
                                       spice_image_compression_t comp);
spice_image_compression_t spice_server_get_image_compression(SpiceServer *s);

typedef enum {
    SPICE_WAN_COMPRESSION_INVALID,
    SPICE_WAN_COMPRESSION_AUTO,
    SPICE_WAN_COMPRESSION_ALWAYS,
    SPICE_WAN_COMPRESSION_NEVER,
} spice_wan_compression_t;

int spice_server_set_jpeg_compression(SpiceServer *s, spice_wan_compression_t comp);
int spice_server_set_zlib_glz_compression(SpiceServer *s, spice_wan_compression_t comp);

#define SPICE_CHANNEL_SECURITY_NONE (1 << 0)
#define SPICE_CHANNEL_SECURITY_SSL (1 << 1)

int spice_server_set_channel_security(SpiceServer *s, const char *channel, int security);

int spice_server_add_renderer(SpiceServer *s, const char *name);

enum {
    SPICE_STREAM_VIDEO_INVALID,
    SPICE_STREAM_VIDEO_OFF,
    SPICE_STREAM_VIDEO_ALL,
    SPICE_STREAM_VIDEO_FILTER
};

int spice_server_set_streaming_video(SpiceServer *s, int value);
int spice_server_set_playback_compression(SpiceServer *s, int enable);
int spice_server_set_agent_mouse(SpiceServer *s, int enable);
int spice_server_set_agent_copypaste(SpiceServer *s, int enable);
int spice_server_set_agent_file_xfer(SpiceServer *s, int enable);

int spice_server_get_sock_info(SpiceServer *s, struct sockaddr *sa, socklen_t *salen);
int spice_server_get_peer_info(SpiceServer *s, struct sockaddr *sa, socklen_t *salen);

int spice_server_is_server_mouse(SpiceServer *s);

/* migration interface */
#define SPICE_INTERFACE_MIGRATION "migration"
#define SPICE_INTERFACE_MIGRATION_MAJOR 1
#define SPICE_INTERFACE_MIGRATION_MINOR 1
typedef struct SpiceMigrateInterface SpiceMigrateInterface;
typedef struct SpiceMigrateInstance SpiceMigrateInstance;
typedef struct SpiceMigrateState SpiceMigrateState;

struct SpiceMigrateInterface {
    SpiceBaseInterface base;
    void (*migrate_connect_complete)(SpiceMigrateInstance *sin);
    void (*migrate_end_complete)(SpiceMigrateInstance *sin);
};

struct SpiceMigrateInstance {
    SpiceBaseInstance base;
    SpiceMigrateState *st;
};

/* spice switch-host client migration */
int spice_server_migrate_info(SpiceServer *s, const char* dest,
                              int port, int secure_port,
                              const char* cert_subject);
int spice_server_migrate_switch(SpiceServer *s);

/* server status */
int spice_server_get_num_clients(SpiceServer *s);

/* spice (semi-)seamless client migration */
int spice_server_migrate_connect(SpiceServer *s, const char* dest,
                                 int port, int secure_port,
                                 const char* cert_subject);
int spice_server_migrate_start(SpiceServer *s);
int spice_server_migrate_end(SpiceServer *s, int completed);

void spice_server_set_seamless_migration(SpiceServer *s, int enable);

void spice_server_set_name(SpiceServer *s, const char *name);
void spice_server_set_uuid(SpiceServer *s, const uint8_t uuid[16]);

void spice_server_vm_start(SpiceServer *s);
void spice_server_vm_stop(SpiceServer *s);

#endif
