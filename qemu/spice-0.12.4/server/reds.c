/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#if HAVE_SASL
#include <sasl/sasl.h>
#endif

#include <glib.h>

#include <spice/protocol.h>
#include <spice/vd_agent.h>
#include <spice/stats.h>

#include "common/generated_server_marshallers.h"
#include "common/ring.h"

#include "spice.h"
#include "spice-experimental.h"
#include "reds.h"
#include "agent-msg-filter.h"
#include "inputs_channel.h"
#include "main_channel.h"
#include "red_common.h"
#include "red_dispatcher.h"
#include "main_dispatcher.h"
#include "snd_worker.h"
#include "stat.h"
#include "demarshallers.h"
#include "char_device.h"
#include "migration_protocol.h"
#ifdef USE_TUNNEL
#include "red_tunnel_worker.h"
#endif
#ifdef USE_SMARTCARD
#include "smartcard.h"
#endif

#include "reds-private.h"

SpiceCoreInterface *core = NULL;
static SpiceCharDeviceInstance *vdagent = NULL;
static SpiceMigrateInstance *migration_interface = NULL;

/* Debugging only variable: allow multiple client connections to the spice
 * server */
#define SPICE_DEBUG_ALLOW_MC_ENV "SPICE_DEBUG_ALLOW_MC"

#define MIGRATION_NOTIFY_SPICE_KEY "spice_mig_ext"

#define REDS_MIG_VERSION 3
#define REDS_MIG_CONTINUE 1
#define REDS_MIG_ABORT 2
#define REDS_MIG_DIFF_VERSION 3

#define REDS_TOKENS_TO_SEND 5
#define REDS_VDI_PORT_NUM_RECEIVE_BUFFS 5

static TicketAuthentication taTicket;

static int spice_port = -1;
static int spice_secure_port = -1;
static int spice_listen_socket_fd = -1;
static char spice_addr[256];
static int spice_family = PF_UNSPEC;
static const char *default_renderer = "sw";
static int sasl_enabled = 0; // sasl disabled by default
#if HAVE_SASL
static char *sasl_appname = NULL; // default to "spice" if NULL
#endif
static char *spice_name = NULL;
static bool spice_uuid_is_set = FALSE;
static uint8_t spice_uuid[16] = { 0, };

static int ticketing_enabled = 1; //Ticketing is enabled by default
static pthread_mutex_t *lock_cs;
static long *lock_count;
uint32_t streaming_video = STREAM_VIDEO_FILTER;
spice_image_compression_t image_compression = SPICE_IMAGE_COMPRESS_AUTO_GLZ;
spice_wan_compression_t jpeg_state = SPICE_WAN_COMPRESSION_AUTO;
spice_wan_compression_t zlib_glz_state = SPICE_WAN_COMPRESSION_AUTO;
#ifdef USE_TUNNEL
void *red_tunnel = NULL;
#endif
int agent_mouse = TRUE;
int agent_copypaste = TRUE;
int agent_file_xfer = TRUE;
static bool exit_on_disconnect = FALSE;

static RedsState *reds = NULL;

typedef struct AsyncRead {
    RedsStream *stream;
    void *opaque;
    uint8_t *now;
    uint8_t *end;
    void (*done)(void *opaque);
    void (*error)(void *opaque, int err);
} AsyncRead;

typedef struct RedLinkInfo {
    RedsStream *stream;
    AsyncRead asyc_read;
    SpiceLinkHeader link_header;
    SpiceLinkMess *link_mess;
    int mess_pos;
    TicketInfo tiTicketing;
    SpiceLinkAuthMechanism auth_mechanism;
    int skip_auth;
} RedLinkInfo;

typedef struct RedSSLParameters {
    char keyfile_password[256];
    char certs_file[256];
    char private_key_file[256];
    char ca_certificate_file[256];
    char dh_key_file[256];
    char ciphersuite[256];
} RedSSLParameters;

typedef struct ChannelSecurityOptions ChannelSecurityOptions;
struct ChannelSecurityOptions {
    uint32_t channel_id;
    uint32_t options;
    ChannelSecurityOptions *next;
};

static void migrate_timeout(void *opaque);
static RedsMigTargetClient* reds_mig_target_client_find(RedClient *client);
static void reds_mig_target_client_free(RedsMigTargetClient *mig_client);
static void reds_mig_cleanup_wait_disconnect(void);
static void reds_mig_remove_wait_disconnect_client(RedClient *client);
static void reds_char_device_add_state(SpiceCharDeviceState *st);
static void reds_char_device_remove_state(SpiceCharDeviceState *st);
static void reds_send_mm_time(void);

static ChannelSecurityOptions *channels_security = NULL;
static int default_channel_security =
    SPICE_CHANNEL_SECURITY_NONE | SPICE_CHANNEL_SECURITY_SSL;

static RedSSLParameters ssl_parameters;

static ChannelSecurityOptions *find_channel_security(int id)
{
    ChannelSecurityOptions *now = channels_security;
    while (now && now->channel_id != id) {
        now = now->next;
    }
    return now;
}

static void reds_stream_push_channel_event(RedsStream *s, int event)
{
    main_dispatcher_channel_event(event, s->info);
}

void reds_handle_channel_event(int event, SpiceChannelEventInfo *info)
{
    if (core->base.minor_version >= 3 && core->channel_event != NULL)
        core->channel_event(event, info);

    if (event == SPICE_CHANNEL_EVENT_DISCONNECTED) {
        free(info);
    }
}

static ssize_t stream_write_cb(RedsStream *s, const void *buf, size_t size)
{
    return write(s->socket, buf, size);
}

static ssize_t stream_writev_cb(RedsStream *s, const struct iovec *iov, int iovcnt)
{
    ssize_t ret = 0;
    do {
        int tosend;
        ssize_t n, expected = 0;
        int i;
#ifdef IOV_MAX
        tosend = MIN(iovcnt, IOV_MAX);
#else
        tosend = iovcnt;
#endif
        for (i = 0; i < tosend; i++) {
            expected += iov[i].iov_len;
        }
        n = writev(s->socket, iov, tosend);
        if (n <= expected) {
            if (n > 0)
                ret += n;
            return ret == 0 ? n : ret;
        }
        ret += n;
        iov += tosend;
        iovcnt -= tosend;
    } while(iovcnt > 0);

    return ret;
}

static ssize_t stream_read_cb(RedsStream *s, void *buf, size_t size)
{
    return read(s->socket, buf, size);
}

static ssize_t stream_ssl_write_cb(RedsStream *s, const void *buf, size_t size)
{
    int return_code;
    SPICE_GNUC_UNUSED int ssl_error;

    return_code = SSL_write(s->ssl, buf, size);

    if (return_code < 0) {
        ssl_error = SSL_get_error(s->ssl, return_code);
    }

    return return_code;
}

static ssize_t stream_ssl_read_cb(RedsStream *s, void *buf, size_t size)
{
    int return_code;
    SPICE_GNUC_UNUSED int ssl_error;

    return_code = SSL_read(s->ssl, buf, size);

    if (return_code < 0) {
        ssl_error = SSL_get_error(s->ssl, return_code);
    }

    return return_code;
}

static void reds_stream_remove_watch(RedsStream* s)
{
    if (s->watch) {
        core->watch_remove(s->watch);
        s->watch = NULL;
    }
}

static void reds_link_free(RedLinkInfo *link)
{
    reds_stream_free(link->stream);
    link->stream = NULL;

    free(link->link_mess);
    link->link_mess = NULL;

    BN_free(link->tiTicketing.bn);
    link->tiTicketing.bn = NULL;

    if (link->tiTicketing.rsa) {
        RSA_free(link->tiTicketing.rsa);
        link->tiTicketing.rsa = NULL;
    }

    free(link);
}

#ifdef RED_STATISTICS

static void insert_stat_node(StatNodeRef parent, StatNodeRef ref)
{
    SpiceStatNode *node = &reds->stat->nodes[ref];
    uint32_t pos = INVALID_STAT_REF;
    uint32_t node_index;
    uint32_t *head;
    SpiceStatNode *n;

    node->first_child_index = INVALID_STAT_REF;
    head = (parent == INVALID_STAT_REF ? &reds->stat->root_index :
                                         &reds->stat->nodes[parent].first_child_index);
    node_index = *head;
    while (node_index != INVALID_STAT_REF && (n = &reds->stat->nodes[node_index]) &&
                                                     strcmp(node->name, n->name) > 0) {
        pos = node_index;
        node_index = n->next_sibling_index;
    }
    if (pos == INVALID_STAT_REF) {
        node->next_sibling_index = *head;
        *head = ref;
    } else {
        n = &reds->stat->nodes[pos];
        node->next_sibling_index = n->next_sibling_index;
        n->next_sibling_index = ref;
    }
}

StatNodeRef stat_add_node(StatNodeRef parent, const char *name, int visible)
{
    StatNodeRef ref;
    SpiceStatNode *node;

    spice_assert(name && strlen(name) > 0);
    if (strlen(name) >= sizeof(node->name)) {
        return INVALID_STAT_REF;
    }
    pthread_mutex_lock(&reds->stat_lock);
    ref = (parent == INVALID_STAT_REF ? reds->stat->root_index :
                                        reds->stat->nodes[parent].first_child_index);
    while (ref != INVALID_STAT_REF) {
        node = &reds->stat->nodes[ref];
        if (strcmp(name, node->name)) {
            ref = node->next_sibling_index;
        } else {
            pthread_mutex_unlock(&reds->stat_lock);
            return ref;
        }
    }
    if (reds->stat->num_of_nodes >= REDS_MAX_STAT_NODES || reds->stat == NULL) {
        pthread_mutex_unlock(&reds->stat_lock);
        return INVALID_STAT_REF;
    }
    reds->stat->generation++;
    reds->stat->num_of_nodes++;
    for (ref = 0; ref <= REDS_MAX_STAT_NODES; ref++) {
        node = &reds->stat->nodes[ref];
        if (!(node->flags & SPICE_STAT_NODE_FLAG_ENABLED)) {
            break;
        }
    }
    spice_assert(!(node->flags & SPICE_STAT_NODE_FLAG_ENABLED));
    node->value = 0;
    node->flags = SPICE_STAT_NODE_FLAG_ENABLED | (visible ? SPICE_STAT_NODE_FLAG_VISIBLE : 0);
    g_strlcpy(node->name, name, sizeof(node->name));
    insert_stat_node(parent, ref);
    pthread_mutex_unlock(&reds->stat_lock);
    return ref;
}

static void stat_remove(SpiceStatNode *node)
{
    pthread_mutex_lock(&reds->stat_lock);
    node->flags &= ~SPICE_STAT_NODE_FLAG_ENABLED;
    reds->stat->generation++;
    reds->stat->num_of_nodes--;
    pthread_mutex_unlock(&reds->stat_lock);
}

void stat_remove_node(StatNodeRef ref)
{
    stat_remove(&reds->stat->nodes[ref]);
}

uint64_t *stat_add_counter(StatNodeRef parent, const char *name, int visible)
{
    StatNodeRef ref = stat_add_node(parent, name, visible);
    SpiceStatNode *node;

    if (ref == INVALID_STAT_REF) {
        return NULL;
    }
    node = &reds->stat->nodes[ref];
    node->flags |= SPICE_STAT_NODE_FLAG_VALUE;
    return &node->value;
}

void stat_remove_counter(uint64_t *counter)
{
    stat_remove((SpiceStatNode *)(counter - offsetof(SpiceStatNode, value)));
}

void reds_update_stat_value(uint32_t value)
{
    RedsStatValue *stat_value = &reds->roundtrip_stat;

    stat_value->value = value;
    stat_value->min = (stat_value->count ? MIN(stat_value->min, value) : value);
    stat_value->max = MAX(stat_value->max, value);
    stat_value->average = (stat_value->average * stat_value->count + value) /
                          (stat_value->count + 1);
    stat_value->count++;
}

#endif

void reds_register_channel(RedChannel *channel)
{
    spice_assert(reds);
    ring_add(&reds->channels, &channel->link);
    reds->num_of_channels++;
}

void reds_unregister_channel(RedChannel *channel)
{
    if (ring_item_is_linked(&channel->link)) {
        ring_remove(&channel->link);
        reds->num_of_channels--;
    } else {
        spice_warning("not found");
    }
}

static RedChannel *reds_find_channel(uint32_t type, uint32_t id)
{
    RingItem *now;

    RING_FOREACH(now, &reds->channels) {
        RedChannel *channel = SPICE_CONTAINEROF(now, RedChannel, link);
        if (channel->type == type && channel->id == id) {
            return channel;
        }
    }
    return NULL;
}

static void reds_mig_cleanup(void)
{
    if (reds->mig_inprogress) {

        if (reds->mig_wait_connect || reds->mig_wait_disconnect) {
            SpiceMigrateInterface *sif;
            spice_assert(migration_interface);
            sif = SPICE_CONTAINEROF(migration_interface->base.sif, SpiceMigrateInterface, base);
            if (reds->mig_wait_connect) {
                sif->migrate_connect_complete(migration_interface);
            } else {
                if (sif->migrate_end_complete) {
                    sif->migrate_end_complete(migration_interface);
                }
            }
        }
        reds->mig_inprogress = FALSE;
        reds->mig_wait_connect = FALSE;
        reds->mig_wait_disconnect = FALSE;
        core->timer_cancel(reds->mig_timer);
        reds_mig_cleanup_wait_disconnect();
    }
}

static void reds_reset_vdp(void)
{
    VDIPortState *state = &reds->agent_state;
    SpiceCharDeviceInterface *sif;

    state->read_state = VDI_PORT_READ_STATE_READ_HEADER;
    state->recive_pos = (uint8_t *)&state->vdi_chunk_header;
    state->recive_len = sizeof(state->vdi_chunk_header);
    state->message_recive_len = 0;
    if (state->current_read_buf) {
        vdi_port_read_buf_unref(state->current_read_buf);
        state->current_read_buf = NULL;
    }
    /* Reset read filter to start with clean state when the agent reconnects */
    agent_msg_filter_init(&state->read_filter, agent_copypaste,
                          agent_file_xfer, TRUE);
    /* Throw away pending chunks from the current (if any) and future
     * messages written by the client.
     * TODO: client should clear its agent messages queue when the agent
     * is disconnect. Currently, when and agent gets disconnected and reconnected,
     * messeges that were directed to the previous instance of the agent continues
     * to be sent from the client. This TODO will require server, protocol, and client changes */
    state->write_filter.result = AGENT_MSG_FILTER_DISCARD;
    state->write_filter.discard_all = TRUE;
    state->client_agent_started = FALSE;

    /* reseting and not destroying the state as a workaround for a bad
     * tokens management in the vdagent protocol:
     *  The client tokens' are set only once, when the main channel is initialized.
     *  Instead, it would have been more appropriate to reset them upon AGEN_CONNECT.
     *  The client tokens are tracked as part of the SpiceCharDeviceClientState. Thus,
     *  in order to be backward compatible with the client, we need to track the tokens
     *  even if the agent is detached. We don't destroy the char_device state, and
     *  instead we just reset it.
     *  In addition, there used to be a misshandling of AGENT_TOKENS message in spice-gtk: it
     *  overrides the amount of tokens, instead of adding the given amount.
     */
    if (red_channel_test_remote_cap(&reds->main_channel->base,
                                    SPICE_MAIN_CAP_AGENT_CONNECTED_TOKENS)) {
        spice_char_device_state_destroy(state->base);
        state->base = NULL;
    } else {
        spice_char_device_reset(state->base);
    }

    sif = SPICE_CONTAINEROF(vdagent->base.sif, SpiceCharDeviceInterface, base);
    if (sif->state) {
        sif->state(vdagent, 0);
    }
}

static int reds_main_channel_connected(void)
{
    return main_channel_is_connected(reds->main_channel);
}

void reds_client_disconnect(RedClient *client)
{
    RedsMigTargetClient *mig_client;

    if (exit_on_disconnect)
    {
        spice_info("Exiting server because of client disconnect.\n");
        exit(0);
    }

    if (!client || client->disconnecting) {
        return;
    }

    spice_info(NULL);
    /* disconnecting is set to prevent recursion because of the following:
     * main_channel_client_on_disconnect->
     *  reds_client_disconnect->red_client_destroy->main_channel...
     */
    client->disconnecting = TRUE;

    // TODO: we need to handle agent properly for all clients!!!! (e.g., cut and paste, how?)
    // We shouldn't initialize the agent when there are still clients connected

    mig_client = reds_mig_target_client_find(client);
    if (mig_client) {
        reds_mig_target_client_free(mig_client);
    }

    if (reds->mig_wait_disconnect) {
        reds_mig_remove_wait_disconnect_client(client);
    }

    if (reds->agent_state.base) {
        /* note that vdagent might be NULL, if the vdagent was once
         * up and than was removed */
        if (spice_char_device_client_exists(reds->agent_state.base, client)) {
            spice_char_device_client_remove(reds->agent_state.base, client);
        }
    }

    ring_remove(&client->link);
    reds->num_clients--;
    red_client_destroy(client);

   // TODO: we need to handle agent properly for all clients!!!! (e.g., cut and paste, how? Maybe throw away messages
   // if we are in the middle of one from another client)
    if (reds->num_clients == 0) {
        /* Let the agent know the client is disconnected */
        if (reds->agent_state.base) {
            SpiceCharDeviceWriteBuffer *char_dev_buf;
            VDInternalBuf *internal_buf;
            uint32_t total_msg_size;

            total_msg_size = sizeof(VDIChunkHeader) + sizeof(VDAgentMessage);
            char_dev_buf = spice_char_device_write_buffer_get_server_no_token(
                               reds->agent_state.base, total_msg_size);
            char_dev_buf->buf_used = total_msg_size;
            internal_buf = (VDInternalBuf *)char_dev_buf->buf;
            internal_buf->chunk_header.port = VDP_SERVER_PORT;
            internal_buf->chunk_header.size = sizeof(VDAgentMessage);
            internal_buf->header.protocol = VD_AGENT_PROTOCOL;
            internal_buf->header.type = VD_AGENT_CLIENT_DISCONNECTED;
            internal_buf->header.opaque = 0;
            internal_buf->header.size = 0;

            spice_char_device_write_buffer_add(reds->agent_state.base,
                                               char_dev_buf);
        }

        /* Reset write filter to start with clean state on client reconnect */
        agent_msg_filter_init(&reds->agent_state.write_filter, agent_copypaste,
                              agent_file_xfer, TRUE);

        /* Throw away pending chunks from the current (if any) and future
         *  messages read from the agent */
        reds->agent_state.read_filter.result = AGENT_MSG_FILTER_DISCARD;
        reds->agent_state.read_filter.discard_all = TRUE;
        free(reds->agent_state.mig_data);
        reds->agent_state.mig_data = NULL;

        reds_mig_cleanup();
    }
}

// TODO: go over all usage of reds_disconnect, most/some of it should be converted to
// reds_client_disconnect
static void reds_disconnect(void)
{
    RingItem *link, *next;

    spice_info(NULL);
    RING_FOREACH_SAFE(link, next, &reds->clients) {
        reds_client_disconnect(SPICE_CONTAINEROF(link, RedClient, link));
    }
    reds_mig_cleanup();
}

static void reds_mig_disconnect(void)
{
    if (reds_main_channel_connected()) {
        reds_disconnect();
    } else {
        reds_mig_cleanup();
    }
}

int reds_get_mouse_mode(void)
{
    return reds->mouse_mode;
}

static void reds_set_mouse_mode(uint32_t mode)
{
    if (reds->mouse_mode == mode) {
        return;
    }
    reds->mouse_mode = mode;
    red_dispatcher_set_mouse_mode(reds->mouse_mode);
    main_channel_push_mouse_mode(reds->main_channel, reds->mouse_mode, reds->is_client_mouse_allowed);
}

int reds_get_agent_mouse(void)
{
    return agent_mouse;
}

static void reds_update_mouse_mode(void)
{
    int allowed = 0;
    int qxl_count = red_dispatcher_qxl_count();

    if ((agent_mouse && vdagent) || (inputs_has_tablet() && qxl_count == 1)) {
        allowed = reds->dispatcher_allows_client_mouse;
    }
    if (allowed == reds->is_client_mouse_allowed) {
        return;
    }
    reds->is_client_mouse_allowed = allowed;
    if (reds->mouse_mode == SPICE_MOUSE_MODE_CLIENT && !allowed) {
        reds_set_mouse_mode(SPICE_MOUSE_MODE_SERVER);
        return;
    }
    if (reds->main_channel) {
        main_channel_push_mouse_mode(reds->main_channel, reds->mouse_mode,
                                     reds->is_client_mouse_allowed);
    }
}

static void reds_agent_remove(void)
{
    // TODO: agent is broken with multiple clients. also need to figure out what to do when
    // part of the clients are during target migration.
    reds_reset_vdp();

    vdagent = NULL;
    reds_update_mouse_mode();
    if (reds_main_channel_connected() &&
        !red_channel_waits_for_migrate_data(&reds->main_channel->base)) {
        main_channel_push_agent_disconnected(reds->main_channel);
    }
}

/*******************************
 * Char device state callbacks *
 * *****************************/

static void vdi_port_read_buf_release(uint8_t *data, void *opaque)
{
    VDIReadBuf *buf = (VDIReadBuf *)opaque;

    vdi_port_read_buf_unref(buf);
}

/* returns TRUE if the buffer can be forwarded */
static int vdi_port_read_buf_process(int port, VDIReadBuf *buf)
{
    VDIPortState *state = &reds->agent_state;
    int res;

    switch (port) {
    case VDP_CLIENT_PORT: {
        res = agent_msg_filter_process_data(&state->read_filter,
                                            buf->data, buf->len);
        switch (res) {
        case AGENT_MSG_FILTER_OK:
            return TRUE;
        case AGENT_MSG_FILTER_DISCARD:
            return FALSE;
        case AGENT_MSG_FILTER_PROTO_ERROR:
            reds_agent_remove();
            return FALSE;
        }
    }
    case VDP_SERVER_PORT:
        return FALSE;
    default:
        spice_warning("invalid port");
        reds_agent_remove();
        return FALSE;
    }
}

static VDIReadBuf *vdi_port_read_buf_get(void)
{
    VDIPortState *state = &reds->agent_state;
    RingItem *item;
    VDIReadBuf *buf;

    if (!(item = ring_get_head(&state->read_bufs))) {
        return NULL;
    }

    ring_remove(item);
    buf = SPICE_CONTAINEROF(item, VDIReadBuf, link);

    buf->refs = 1;
    return buf;
}

static VDIReadBuf* vdi_port_read_buf_ref(VDIReadBuf *buf)
{
    buf->refs++;
    return buf;
}

static void vdi_port_read_buf_unref(VDIReadBuf *buf)
{
    if (!--buf->refs) {
        ring_add(&reds->agent_state.read_bufs, &buf->link);

        /* read_one_msg_from_vdi_port may have never completed because the read_bufs
        ring was empty. So we call it again so it can complete its work if
        necessary. Note that since we can be called from spice_char_device_wakeup
        this can cause recursion, but we have protection for that */
        if (reds->agent_state.base) {
            spice_char_device_wakeup(reds->agent_state.base);
        }
    }
}

/* reads from the device till completes reading a message that is addressed to the client,
 * or otherwise, when reading from the device fails */
static SpiceCharDeviceMsgToClient *vdi_port_read_one_msg_from_device(SpiceCharDeviceInstance *sin,
                                                                     void *opaque)
{
    VDIPortState *state = &reds->agent_state;
    SpiceCharDeviceInterface *sif;
    VDIReadBuf *dispatch_buf;
    int n;

    if (!vdagent) {
        return NULL;
    }
    spice_assert(vdagent == sin);
    sif = SPICE_CONTAINEROF(vdagent->base.sif, SpiceCharDeviceInterface, base);
    while (vdagent) {
        switch (state->read_state) {
        case VDI_PORT_READ_STATE_READ_HEADER:
            n = sif->read(vdagent, state->recive_pos, state->recive_len);
            if (!n) {
                return NULL;
            }
            if ((state->recive_len -= n)) {
                state->recive_pos += n;
                return NULL;
            }
            state->message_recive_len = state->vdi_chunk_header.size;
            state->read_state = VDI_PORT_READ_STATE_GET_BUFF;
        case VDI_PORT_READ_STATE_GET_BUFF: {
            if (!(state->current_read_buf = vdi_port_read_buf_get())) {
                return NULL;
            }
            state->recive_pos = state->current_read_buf->data;
            state->recive_len = MIN(state->message_recive_len,
                                    sizeof(state->current_read_buf->data));
            state->current_read_buf->len = state->recive_len;
            state->message_recive_len -= state->recive_len;
            state->read_state = VDI_PORT_READ_STATE_READ_DATA;
        }
        case VDI_PORT_READ_STATE_READ_DATA:
            n = sif->read(vdagent, state->recive_pos, state->recive_len);
            if (!n) {
                return NULL;
            }
            if ((state->recive_len -= n)) {
                state->recive_pos += n;
                break;
            }
            dispatch_buf = state->current_read_buf;
            state->current_read_buf = NULL;
            state->recive_pos = NULL;
            if (state->message_recive_len == 0) {
                state->read_state = VDI_PORT_READ_STATE_READ_HEADER;
                state->recive_pos = (uint8_t *)&state->vdi_chunk_header;
                state->recive_len = sizeof(state->vdi_chunk_header);
            } else {
                state->read_state = VDI_PORT_READ_STATE_GET_BUFF;
            }
            if (vdi_port_read_buf_process(state->vdi_chunk_header.port, dispatch_buf)) {
                return dispatch_buf;
            } else {
                vdi_port_read_buf_unref(dispatch_buf);
            }
        } /* END switch */
    } /* END while */
    return NULL;
}

static SpiceCharDeviceMsgToClient *vdi_port_ref_msg_to_client(SpiceCharDeviceMsgToClient *msg,
                                                       void *opaque)
{
    return vdi_port_read_buf_ref(msg);
}

static void vdi_port_unref_msg_to_client(SpiceCharDeviceMsgToClient *msg,
                                  void *opaque)
{
    vdi_port_read_buf_unref(msg);
}

/* after calling this, we unref the message, and the ref is in the instance side */
static void vdi_port_send_msg_to_client(SpiceCharDeviceMsgToClient *msg,
                                        RedClient *client,
                                        void *opaque)
{
    VDIReadBuf *agent_data_buf = msg;

    main_channel_client_push_agent_data(red_client_get_main(client),
                                        agent_data_buf->data,
                                        agent_data_buf->len,
                                        vdi_port_read_buf_release,
                                        vdi_port_read_buf_ref(agent_data_buf));
}

static void vdi_port_send_tokens_to_client(RedClient *client, uint32_t tokens, void *opaque)
{
    main_channel_client_push_agent_tokens(red_client_get_main(client),
                                          tokens);
}

static void vdi_port_on_free_self_token(void *opaque)
{

    if (inputs_inited() && reds->pending_mouse_event) {
        spice_debug("pending mouse event");
        reds_handle_agent_mouse_event(inputs_get_mouse_state());
    }
}

static void vdi_port_remove_client(RedClient *client, void *opaque)
{
    reds_client_disconnect(client);
}

/****************************************************************************/

int reds_has_vdagent(void)
{
    return !!vdagent;
}

void reds_handle_agent_mouse_event(const VDAgentMouseState *mouse_state)
{
    SpiceCharDeviceWriteBuffer *char_dev_buf;
    VDInternalBuf *internal_buf;
    uint32_t total_msg_size;

    if (!inputs_inited() || !reds->agent_state.base) {
        return;
    }

    total_msg_size = sizeof(VDIChunkHeader) + sizeof(VDAgentMessage) +
                     sizeof(VDAgentMouseState);
    char_dev_buf = spice_char_device_write_buffer_get(reds->agent_state.base,
                                                      NULL,
                                                      total_msg_size);

    if (!char_dev_buf) {
        reds->pending_mouse_event = TRUE;

        return;
    }
    reds->pending_mouse_event = FALSE;

    internal_buf = (VDInternalBuf *)char_dev_buf->buf;
    internal_buf->chunk_header.port = VDP_SERVER_PORT;
    internal_buf->chunk_header.size = sizeof(VDAgentMessage) + sizeof(VDAgentMouseState);
    internal_buf->header.protocol = VD_AGENT_PROTOCOL;
    internal_buf->header.type = VD_AGENT_MOUSE_STATE;
    internal_buf->header.opaque = 0;
    internal_buf->header.size = sizeof(VDAgentMouseState);
    internal_buf->u.mouse_state = *mouse_state;

    char_dev_buf->buf_used = total_msg_size;
    spice_char_device_write_buffer_add(reds->agent_state.base, char_dev_buf);
}

int reds_num_of_channels(void)
{
    return reds ? reds->num_of_channels : 0;
}


int reds_num_of_clients(void)
{
    return reds ? reds->num_clients : 0;
}

SPICE_GNUC_VISIBLE int spice_server_get_num_clients(SpiceServer *s)
{
    spice_assert(reds == s);
    return reds_num_of_clients();
}

static int secondary_channels[] = {
    SPICE_CHANNEL_MAIN, SPICE_CHANNEL_DISPLAY, SPICE_CHANNEL_CURSOR, SPICE_CHANNEL_INPUTS};

static int channel_is_secondary(RedChannel *channel)
{
    int i;
    for (i = 0 ; i < sizeof(secondary_channels)/sizeof(secondary_channels[0]); ++i) {
        if (channel->type == secondary_channels[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

void reds_fill_channels(SpiceMsgChannels *channels_info)
{
    RingItem *now;
    int used_channels = 0;

    channels_info->num_of_channels = reds->num_of_channels;
    RING_FOREACH(now, &reds->channels) {
        RedChannel *channel = SPICE_CONTAINEROF(now, RedChannel, link);
        if (reds->num_clients > 1 && !channel_is_secondary(channel)) {
            continue;
        }
        channels_info->channels[used_channels].type = channel->type;
        channels_info->channels[used_channels].id = channel->id;
        used_channels++;
    }

    channels_info->num_of_channels = used_channels;
    if (used_channels != reds->num_of_channels) {
        spice_warning("sent %d out of %d", used_channels, reds->num_of_channels);
    }
}

void reds_on_main_agent_start(MainChannelClient *mcc, uint32_t num_tokens)
{
    SpiceCharDeviceState *dev_state = reds->agent_state.base;
    RedChannelClient *rcc;

    if (!vdagent) {
        return;
    }
    spice_assert(vdagent->st && vdagent->st == dev_state);
    rcc = main_channel_client_get_base(mcc);
    reds->agent_state.client_agent_started = TRUE;
    /*
     * Note that in older releases, send_tokens were set to ~0 on both client
     * and server. The server ignored the client given tokens.
     * Thanks to that, when an old client is connected to a new server,
     * and vice versa, the sending from the server to the client won't have
     * flow control, but will have no other problem.
     */
    if (!spice_char_device_client_exists(dev_state, rcc->client)) {
        int client_added;

        client_added = spice_char_device_client_add(dev_state,
                                                    rcc->client,
                                                    TRUE, /* flow control */
                                                    REDS_VDI_PORT_NUM_RECEIVE_BUFFS,
                                                    REDS_AGENT_WINDOW_SIZE,
                                                    num_tokens,
                                                    red_channel_client_waits_for_migrate_data(rcc));

        if (!client_added) {
            spice_warning("failed to add client to agent");
            reds_client_disconnect(rcc->client);
            return;
        }
    } else {
        spice_char_device_send_to_client_tokens_set(dev_state,
                                                    rcc->client,
                                                    num_tokens);
    }
    reds->agent_state.write_filter.discard_all = FALSE;
}

void reds_on_main_agent_tokens(MainChannelClient *mcc, uint32_t num_tokens)
{
    if (!vdagent) {
        return;
    }
    spice_assert(vdagent->st);
    spice_char_device_send_to_client_tokens_add(vdagent->st,
                                                main_channel_client_get_base(mcc)->client,
                                                num_tokens);
}

uint8_t *reds_get_agent_data_buffer(MainChannelClient *mcc, size_t size)
{
    VDIPortState *dev_state = &reds->agent_state;
    RedClient *client;

    if (!dev_state->client_agent_started) {
        /*
         * agent got disconnected, and possibly got reconnected, but we still can receive
         * msgs that are addressed to the agent's old instance, in case they were
         * sent by the client before it received the AGENT_DISCONNECTED msg.
         * In such case, we will receive and discard the msgs (reds_reset_vdp takes care
         * of setting state->write_filter.result = AGENT_MSG_FILTER_DISCARD).
         */
        return spice_malloc(size);
    }

    spice_assert(dev_state->recv_from_client_buf == NULL);
    client = main_channel_client_get_base(mcc)->client;
    dev_state->recv_from_client_buf = spice_char_device_write_buffer_get(dev_state->base,
                                                                         client,
                                                                         size + sizeof(VDIChunkHeader));
    dev_state->recv_from_client_buf_pushed = FALSE;
    return dev_state->recv_from_client_buf->buf + sizeof(VDIChunkHeader);
}

void reds_release_agent_data_buffer(uint8_t *buf)
{
    VDIPortState *dev_state = &reds->agent_state;

    if (!dev_state->recv_from_client_buf) {
        free(buf);
        return;
    }

    spice_assert(buf == dev_state->recv_from_client_buf->buf + sizeof(VDIChunkHeader));
    if (!dev_state->recv_from_client_buf_pushed) {
        spice_char_device_write_buffer_release(reds->agent_state.base,
                                               dev_state->recv_from_client_buf);
    }
    dev_state->recv_from_client_buf = NULL;
    dev_state->recv_from_client_buf_pushed = FALSE;
}

static void reds_client_monitors_config_cleanup(void)
{
    RedsClientMonitorsConfig *cmc = &reds->client_monitors_config;

    cmc->buffer_size = cmc->buffer_pos = 0;
    free(cmc->buffer);
    cmc->buffer = NULL;
    cmc->mcc = NULL;
}

static void reds_on_main_agent_monitors_config(
        MainChannelClient *mcc, void *message, size_t size)
{
    VDAgentMessage *msg_header;
    VDAgentMonitorsConfig *monitors_config;
    RedsClientMonitorsConfig *cmc = &reds->client_monitors_config;

    cmc->buffer_size += size;
    cmc->buffer = realloc(cmc->buffer, cmc->buffer_size);
    spice_assert(cmc->buffer);
    cmc->mcc = mcc;
    memcpy(cmc->buffer + cmc->buffer_pos, message, size);
    cmc->buffer_pos += size;
    msg_header = (VDAgentMessage *)cmc->buffer;
    if (sizeof(VDAgentMessage) > cmc->buffer_size ||
            msg_header->size > cmc->buffer_size - sizeof(VDAgentMessage)) {
        spice_debug("not enough data yet. %d\n", cmc->buffer_size);
        return;
    }
    monitors_config = (VDAgentMonitorsConfig *)(cmc->buffer + sizeof(*msg_header));
    spice_debug("%s: %d\n", __func__, monitors_config->num_of_monitors);
    red_dispatcher_client_monitors_config(monitors_config);
    reds_client_monitors_config_cleanup();
}

void reds_on_main_agent_data(MainChannelClient *mcc, void *message, size_t size)
{
    VDIPortState *dev_state = &reds->agent_state;
    VDIChunkHeader *header;
    int res;

    res = agent_msg_filter_process_data(&reds->agent_state.write_filter,
                                        message, size);
    switch (res) {
    case AGENT_MSG_FILTER_OK:
        break;
    case AGENT_MSG_FILTER_DISCARD:
        return;
    case AGENT_MSG_FILTER_MONITORS_CONFIG:
        reds_on_main_agent_monitors_config(mcc, message, size);
        return;
    case AGENT_MSG_FILTER_PROTO_ERROR:
        reds_disconnect();
        return;
    }

    spice_assert(reds->agent_state.recv_from_client_buf);
    spice_assert(message == reds->agent_state.recv_from_client_buf->buf + sizeof(VDIChunkHeader));
    // TODO - start tracking agent data per channel
    header =  (VDIChunkHeader *)dev_state->recv_from_client_buf->buf;
    header->port = VDP_CLIENT_PORT;
    header->size = size;
    dev_state->recv_from_client_buf->buf_used = sizeof(VDIChunkHeader) + size;

    dev_state->recv_from_client_buf_pushed = TRUE;
    spice_char_device_write_buffer_add(reds->agent_state.base, dev_state->recv_from_client_buf);
}

void reds_on_main_migrate_connected(int seamless)
{
    reds->src_do_seamless_migrate = seamless;
    if (reds->mig_wait_connect) {
        reds_mig_cleanup();
    }
}

void reds_on_main_mouse_mode_request(void *message, size_t size)
{
    switch (((SpiceMsgcMainMouseModeRequest *)message)->mode) {
    case SPICE_MOUSE_MODE_CLIENT:
        if (reds->is_client_mouse_allowed) {
            reds_set_mouse_mode(SPICE_MOUSE_MODE_CLIENT);
        } else {
            spice_info("client mouse is disabled");
        }
        break;
    case SPICE_MOUSE_MODE_SERVER:
        reds_set_mouse_mode(SPICE_MOUSE_MODE_SERVER);
        break;
    default:
        spice_warning("unsupported mouse mode");
    }
}

/*
 * Push partial agent data, even if not all the chunk was consumend,
 * in order to avoid the roundtrip (src-server->client->dest-server)
 */
void reds_on_main_channel_migrate(MainChannelClient *mcc)
{
    VDIPortState *agent_state = &reds->agent_state;
    uint32_t read_data_len;

    spice_assert(reds->num_clients == 1);

    if (agent_state->read_state != VDI_PORT_READ_STATE_READ_DATA) {
        return;
    }
    spice_assert(agent_state->current_read_buf->data &&
                 agent_state->recive_pos > agent_state->current_read_buf->data);
    read_data_len = agent_state->recive_pos - agent_state->current_read_buf->data;

    if (agent_state->read_filter.msg_data_to_read ||
        read_data_len > sizeof(VDAgentMessage)) { /* msg header has been read */
        VDIReadBuf *read_buf = agent_state->current_read_buf;

        spice_debug("push partial read %u (msg first chunk? %d)", read_data_len,
                    !agent_state->read_filter.msg_data_to_read);

        read_buf->len = read_data_len;
        if (vdi_port_read_buf_process(agent_state->vdi_chunk_header.port, read_buf)) {
            main_channel_client_push_agent_data(mcc,
                                                read_buf->data,
                                                read_buf->len,
                                                vdi_port_read_buf_release,
                                                read_buf);
        } else {
            vdi_port_read_buf_unref(read_buf);
        }

        spice_assert(agent_state->recive_len);
        agent_state->message_recive_len += agent_state->recive_len;
        agent_state->read_state = VDI_PORT_READ_STATE_GET_BUFF;
        agent_state->current_read_buf = NULL;
        agent_state->recive_pos = NULL;
    }
}

void reds_marshall_migrate_data(SpiceMarshaller *m)
{
    SpiceMigrateDataMain mig_data;
    VDIPortState *agent_state = &reds->agent_state;
    SpiceMarshaller *m2;

    memset(&mig_data, 0, sizeof(mig_data));
    spice_marshaller_add_uint32(m, SPICE_MIGRATE_DATA_MAIN_MAGIC);
    spice_marshaller_add_uint32(m, SPICE_MIGRATE_DATA_MAIN_VERSION);

    if (!vdagent) {
        uint8_t *null_agent_mig_data;

        spice_assert(!agent_state->base); /* MSG_AGENT_CONNECTED_TOKENS is supported by the client
                                             (see spice_server_migrate_connect), so SpiceCharDeviceState
                                             is destroyed when the agent is disconnected and
                                             there is no need to track the client tokens
                                             (see reds_reset_vdp) */
        spice_char_device_state_migrate_data_marshall_empty(m);
        null_agent_mig_data = spice_marshaller_reserve_space(m,
                                                             sizeof(SpiceMigrateDataMain) -
                                                             sizeof(SpiceMigrateDataCharDevice));
        memset(null_agent_mig_data,
               0,
               sizeof(SpiceMigrateDataMain) - sizeof(SpiceMigrateDataCharDevice));
        return;
    }

    spice_char_device_state_migrate_data_marshall(reds->agent_state.base, m);
    spice_marshaller_add_uint8(m, reds->agent_state.client_agent_started);

    mig_data.agent2client.chunk_header = agent_state->vdi_chunk_header;

    /* agent to client partial msg */
    if (agent_state->read_state == VDI_PORT_READ_STATE_READ_HEADER) {
        mig_data.agent2client.chunk_header_size = agent_state->recive_pos -
            (uint8_t *)&agent_state->vdi_chunk_header;

        mig_data.agent2client.msg_header_done = FALSE;
        mig_data.agent2client.msg_header_partial_len = 0;
        spice_assert(!agent_state->read_filter.msg_data_to_read);
    } else {
        mig_data.agent2client.chunk_header_size = sizeof(VDIChunkHeader);
        mig_data.agent2client.chunk_header.size = agent_state->message_recive_len;
        if (agent_state->read_state == VDI_PORT_READ_STATE_READ_DATA) {
            /* in the middle of reading the message header (see reds_on_main_channel_migrate) */
            mig_data.agent2client.msg_header_done = FALSE;
            mig_data.agent2client.msg_header_partial_len =
                agent_state->recive_pos - agent_state->current_read_buf->data;
            spice_assert(mig_data.agent2client.msg_header_partial_len < sizeof(VDAgentMessage));
            spice_assert(!agent_state->read_filter.msg_data_to_read);
        } else {
            mig_data.agent2client.msg_header_done =  TRUE;
            mig_data.agent2client.msg_remaining = agent_state->read_filter.msg_data_to_read;
            mig_data.agent2client.msg_filter_result = agent_state->read_filter.result;
        }
    }
    spice_marshaller_add_uint32(m, mig_data.agent2client.chunk_header_size);
    spice_marshaller_add(m,
                         (uint8_t *)&mig_data.agent2client.chunk_header,
                         sizeof(VDIChunkHeader));
    spice_marshaller_add_uint8(m, mig_data.agent2client.msg_header_done);
    spice_marshaller_add_uint32(m, mig_data.agent2client.msg_header_partial_len);
    m2 = spice_marshaller_get_ptr_submarshaller(m, 0);
    spice_marshaller_add(m2, agent_state->current_read_buf->data,
                         mig_data.agent2client.msg_header_partial_len);
    spice_marshaller_add_uint32(m, mig_data.agent2client.msg_remaining);
    spice_marshaller_add_uint8(m, mig_data.agent2client.msg_filter_result);

    mig_data.client2agent.msg_remaining = agent_state->write_filter.msg_data_to_read;
    mig_data.client2agent.msg_filter_result = agent_state->write_filter.result;
    spice_marshaller_add_uint32(m, mig_data.client2agent.msg_remaining);
    spice_marshaller_add_uint8(m, mig_data.client2agent.msg_filter_result);
    spice_debug("from agent filter: discard all %d, wait_msg %u, msg_filter_result %d",
                agent_state->read_filter.discard_all,
                agent_state->read_filter.msg_data_to_read,
                 agent_state->read_filter.result);
    spice_debug("to agent filter: discard all %d, wait_msg %u, msg_filter_result %d",
                agent_state->write_filter.discard_all,
                agent_state->write_filter.msg_data_to_read,
                 agent_state->write_filter.result);
}

static int reds_agent_state_restore(SpiceMigrateDataMain *mig_data)
{
    VDIPortState *agent_state = &reds->agent_state;
    uint32_t chunk_header_remaining;

    agent_state->vdi_chunk_header = mig_data->agent2client.chunk_header;
    spice_assert(mig_data->agent2client.chunk_header_size <= sizeof(VDIChunkHeader));
    chunk_header_remaining = sizeof(VDIChunkHeader) - mig_data->agent2client.chunk_header_size;
    if (chunk_header_remaining) {
        agent_state->read_state = VDI_PORT_READ_STATE_READ_HEADER;
        agent_state->recive_pos = (uint8_t *)&agent_state->vdi_chunk_header +
            mig_data->agent2client.chunk_header_size;
        agent_state->recive_len = chunk_header_remaining;
    } else {
        agent_state->message_recive_len = agent_state->vdi_chunk_header.size;
    }

    if (!mig_data->agent2client.msg_header_done) {
        uint8_t *partial_msg_header;

        if (!chunk_header_remaining) {
            uint32_t cur_buf_size;

            agent_state->read_state = VDI_PORT_READ_STATE_READ_DATA;
            agent_state->current_read_buf = vdi_port_read_buf_get();
            spice_assert(agent_state->current_read_buf);
            partial_msg_header = (uint8_t *)mig_data + mig_data->agent2client.msg_header_ptr -
                sizeof(SpiceMiniDataHeader);
            memcpy(agent_state->current_read_buf->data,
                   partial_msg_header,
                   mig_data->agent2client.msg_header_partial_len);
            agent_state->recive_pos = agent_state->current_read_buf->data +
                                      mig_data->agent2client.msg_header_partial_len;
            cur_buf_size = sizeof(agent_state->current_read_buf->data) -
                           mig_data->agent2client.msg_header_partial_len;
            agent_state->recive_len = MIN(agent_state->message_recive_len, cur_buf_size);
            agent_state->current_read_buf->len = agent_state->recive_len +
                                                 mig_data->agent2client.msg_header_partial_len;
            agent_state->message_recive_len -= agent_state->recive_len;
        } else {
            spice_assert(mig_data->agent2client.msg_header_partial_len == 0);
        }
    } else {
            agent_state->read_state = VDI_PORT_READ_STATE_GET_BUFF;
            agent_state->current_read_buf = NULL;
            agent_state->recive_pos = NULL;
            agent_state->read_filter.msg_data_to_read = mig_data->agent2client.msg_remaining;
            agent_state->read_filter.result = mig_data->agent2client.msg_filter_result;
    }

    agent_state->read_filter.discard_all = FALSE;
    agent_state->write_filter.discard_all = !mig_data->client_agent_started;
    agent_state->client_agent_started = mig_data->client_agent_started;

    agent_state->write_filter.msg_data_to_read = mig_data->client2agent.msg_remaining;
    agent_state->write_filter.result = mig_data->client2agent.msg_filter_result;

    spice_debug("to agent filter: discard all %d, wait_msg %u, msg_filter_result %d",
                agent_state->write_filter.discard_all,
                agent_state->write_filter.msg_data_to_read,
                 agent_state->write_filter.result);
    spice_debug("from agent filter: discard all %d, wait_msg %u, msg_filter_result %d",
                agent_state->read_filter.discard_all,
                agent_state->read_filter.msg_data_to_read,
                 agent_state->read_filter.result);
    return spice_char_device_state_restore(agent_state->base, &mig_data->agent_base);
}

/*
 * The agent device is not attached to the dest before migration is completed. It is
 * attached only after the vm is started. It might be attached before or after
 * the migration data has reached the server.
 */
int reds_handle_migrate_data(MainChannelClient *mcc, SpiceMigrateDataMain *mig_data, uint32_t size)
{
    VDIPortState *agent_state = &reds->agent_state;

    /*
     * Now that the client has switched to the target server, if main_channel
     * controls the mm-time, we update the client's mm-time.
     * (MSG_MAIN_INIT is not sent for a migrating connection)
     */
    if (reds->mm_timer_enabled) {
        reds_send_mm_time();
    }
    if (mig_data->agent_base.connected) {
        if (agent_state->base) { // agent was attached before migration data has arrived
            if (!vdagent) {
                spice_assert(agent_state->plug_generation > 0);
                main_channel_push_agent_disconnected(reds->main_channel);
                spice_debug("agent is no longer connected");
            } else {
                if (agent_state->plug_generation > 1) {
                    /* spice_char_device_state_reset takes care of not making the device wait for migration data */
                    spice_debug("agent has been detached and reattached before receiving migration data");
                    main_channel_push_agent_disconnected(reds->main_channel);
                    main_channel_push_agent_connected(reds->main_channel);
                } else {
                    return reds_agent_state_restore(mig_data);
                }
            }
        } else {
            /* restore agent starte when the agent gets attached */
            spice_assert(agent_state->plug_generation == 0);
            agent_state->mig_data = spice_memdup(mig_data, size);
        }
    } else {
        if (vdagent) {
            /* spice_char_device_client_remove disables waiting for migration data */
            spice_char_device_client_remove(agent_state->base,
                                            main_channel_client_get_base(mcc)->client);
            main_channel_push_agent_connected(reds->main_channel);
        }
    }

    return TRUE;
}

static int sync_write(RedsStream *stream, const void *in_buf, size_t n)
{
    const uint8_t *buf = (uint8_t *)in_buf;

    while (n) {
        int now = reds_stream_write(stream, buf, n);
        if (now <= 0) {
            if (now == -1 && (errno == EINTR || errno == EAGAIN)) {
                continue;
            }
            return FALSE;
        }
        n -= now;
        buf += now;
    }
    return TRUE;
}

static void reds_channel_init_auth_caps(RedLinkInfo *link, RedChannel *channel)
{
    if (sasl_enabled && !link->skip_auth) {
        red_channel_set_common_cap(channel, SPICE_COMMON_CAP_AUTH_SASL);
    } else {
        red_channel_set_common_cap(channel, SPICE_COMMON_CAP_AUTH_SPICE);
    }
    red_channel_set_common_cap(channel, SPICE_COMMON_CAP_PROTOCOL_AUTH_SELECTION);
}

static int reds_send_link_ack(RedLinkInfo *link)
{
    SpiceLinkHeader header;
    SpiceLinkReply ack;
    RedChannel *channel;
    RedChannelCapabilities *channel_caps;
    BUF_MEM *bmBuf;
    BIO *bio;
    int ret = FALSE;

    header.magic = SPICE_MAGIC;
    header.size = sizeof(ack);
    header.major_version = SPICE_VERSION_MAJOR;
    header.minor_version = SPICE_VERSION_MINOR;

    ack.error = SPICE_LINK_ERR_OK;

    channel = reds_find_channel(link->link_mess->channel_type, 0);
    if (!channel) {
        spice_assert(link->link_mess->channel_type == SPICE_CHANNEL_MAIN);
        spice_assert(reds->main_channel);
        channel = &reds->main_channel->base;
    }

    reds_channel_init_auth_caps(link, channel); /* make sure common caps are set */

    channel_caps = &channel->local_caps;
    ack.num_common_caps = channel_caps->num_common_caps;
    ack.num_channel_caps = channel_caps->num_caps;
    header.size += (ack.num_common_caps + ack.num_channel_caps) * sizeof(uint32_t);
    ack.caps_offset = sizeof(SpiceLinkReply);

    if (!(link->tiTicketing.rsa = RSA_new())) {
        spice_warning("RSA nes failed");
        return FALSE;
    }

    if (!(bio = BIO_new(BIO_s_mem()))) {
        spice_warning("BIO new failed");
        return FALSE;
    }

    RSA_generate_key_ex(link->tiTicketing.rsa, SPICE_TICKET_KEY_PAIR_LENGTH, link->tiTicketing.bn,
                        NULL);
    link->tiTicketing.rsa_size = RSA_size(link->tiTicketing.rsa);

    i2d_RSA_PUBKEY_bio(bio, link->tiTicketing.rsa);
    BIO_get_mem_ptr(bio, &bmBuf);
    memcpy(ack.pub_key, bmBuf->data, sizeof(ack.pub_key));

    if (!sync_write(link->stream, &header, sizeof(header)))
        goto end;
    if (!sync_write(link->stream, &ack, sizeof(ack)))
        goto end;
    if (!sync_write(link->stream, channel_caps->common_caps, channel_caps->num_common_caps * sizeof(uint32_t)))
        goto end;
    if (!sync_write(link->stream, channel_caps->caps, channel_caps->num_caps * sizeof(uint32_t)))
        goto end;

    ret = TRUE;

end:
    BIO_free(bio);
    return ret;
}

static int reds_send_link_error(RedLinkInfo *link, uint32_t error)
{
    SpiceLinkHeader header;
    SpiceLinkReply reply;

    header.magic = SPICE_MAGIC;
    header.size = sizeof(reply);
    header.major_version = SPICE_VERSION_MAJOR;
    header.minor_version = SPICE_VERSION_MINOR;
    memset(&reply, 0, sizeof(reply));
    reply.error = error;
    return sync_write(link->stream, &header, sizeof(header)) && sync_write(link->stream, &reply,
                                                                         sizeof(reply));
}

static void reds_info_new_channel(RedLinkInfo *link, int connection_id)
{
    spice_info("channel %d:%d, connected successfully, over %s link",
               link->link_mess->channel_type,
               link->link_mess->channel_id,
               link->stream->ssl == NULL ? "Non Secure" : "Secure");
    /* add info + send event */
    if (link->stream->ssl) {
        link->stream->info->flags |= SPICE_CHANNEL_EVENT_FLAG_TLS;
    }
    link->stream->info->connection_id = connection_id;
    link->stream->info->type = link->link_mess->channel_type;
    link->stream->info->id   = link->link_mess->channel_id;
    reds_stream_push_channel_event(link->stream, SPICE_CHANNEL_EVENT_INITIALIZED);
}

static void reds_send_link_result(RedLinkInfo *link, uint32_t error)
{
    sync_write(link->stream, &error, sizeof(error));
}

int reds_expects_link_id(uint32_t connection_id)
{
    spice_info("TODO: keep a list of connection_id's from migration, compare to them");
    return 1;
}

static void reds_mig_target_client_add(RedClient *client)
{
    RedsMigTargetClient *mig_client;

    spice_assert(reds);
    spice_info(NULL);
    mig_client = spice_malloc0(sizeof(RedsMigTargetClient));
    mig_client->client = client;
    ring_init(&mig_client->pending_links);
    ring_add(&reds->mig_target_clients, &mig_client->link);
    reds->num_mig_target_clients++;

}

static RedsMigTargetClient* reds_mig_target_client_find(RedClient *client)
{
    RingItem *item;

    RING_FOREACH(item, &reds->mig_target_clients) {
        RedsMigTargetClient *mig_client;

        mig_client = SPICE_CONTAINEROF(item, RedsMigTargetClient, link);
        if (mig_client->client == client) {
            return mig_client;
        }
    }
    return NULL;
}

static void reds_mig_target_client_add_pending_link(RedsMigTargetClient *client,
                                                    SpiceLinkMess *link_msg,
                                                    RedsStream *stream)
{
    RedsMigPendingLink *mig_link;

    spice_assert(reds);
    spice_assert(client);
    mig_link = spice_malloc0(sizeof(RedsMigPendingLink));
    mig_link->link_msg = link_msg;
    mig_link->stream = stream;

    ring_add(&client->pending_links, &mig_link->ring_link);
}

static void reds_mig_target_client_free(RedsMigTargetClient *mig_client)
{
    RingItem *now, *next;

    ring_remove(&mig_client->link);
    reds->num_mig_target_clients--;

    RING_FOREACH_SAFE(now, next, &mig_client->pending_links) {
        RedsMigPendingLink *mig_link = SPICE_CONTAINEROF(now, RedsMigPendingLink, ring_link);
        ring_remove(now);
        free(mig_link);
    }
    free(mig_client);
}

static void reds_mig_target_client_disconnect_all(void)
{
    RingItem *now, *next;

    RING_FOREACH_SAFE(now, next, &reds->mig_target_clients) {
        RedsMigTargetClient *mig_client = SPICE_CONTAINEROF(now, RedsMigTargetClient, link);
        reds_client_disconnect(mig_client->client);
    }
}

static int reds_find_client(RedClient *client)
{
    RingItem *item;

    RING_FOREACH(item, &reds->clients) {
        RedClient *list_client;

        list_client = SPICE_CONTAINEROF(item, RedClient, link);
        if (list_client == client) {
            return TRUE;
        }
    }
    return FALSE;
}

/* should be used only when there is one client */
static RedClient *reds_get_client(void)
{
    spice_assert(reds->num_clients <= 1);

    if (reds->num_clients == 0) {
        return NULL;
    }

    return SPICE_CONTAINEROF(ring_get_head(&reds->clients), RedClient, link);
}

// TODO: now that main is a separate channel this should
// actually be joined with reds_handle_other_links, become reds_handle_link
static void reds_handle_main_link(RedLinkInfo *link)
{
    RedClient *client;
    RedsStream *stream;
    SpiceLinkMess *link_mess;
    uint32_t *caps;
    uint32_t connection_id;
    MainChannelClient *mcc;
    int mig_target = FALSE;

    spice_info(NULL);
    spice_assert(reds->main_channel);

    link_mess = link->link_mess;
    if (!reds->allow_multiple_clients) {
        reds_disconnect();
    }

    if (link_mess->connection_id == 0) {
        reds_send_link_result(link, SPICE_LINK_ERR_OK);
        while((connection_id = rand()) == 0);
        mig_target = FALSE;
    } else {
        // TODO: make sure link_mess->connection_id is the same
        // connection id the migration src had (use vmstate to store the connection id)
        reds_send_link_result(link, SPICE_LINK_ERR_OK);
        connection_id = link_mess->connection_id;
        mig_target = TRUE;
    }

    reds->mig_inprogress = FALSE;
    reds->mig_wait_connect = FALSE;
    reds->mig_wait_disconnect = FALSE;

    reds_info_new_channel(link, connection_id);
    stream = link->stream;
    reds_stream_remove_watch(stream);
    link->stream = NULL;
    link->link_mess = NULL;
    reds_link_free(link);
    caps = (uint32_t *)((uint8_t *)link_mess + link_mess->caps_offset);
    client = red_client_new(mig_target);
    ring_add(&reds->clients, &client->link);
    reds->num_clients++;
    mcc = main_channel_link(reds->main_channel, client,
                            stream, connection_id, mig_target,
                            link_mess->num_common_caps,
                            link_mess->num_common_caps ? caps : NULL, link_mess->num_channel_caps,
                            link_mess->num_channel_caps ? caps + link_mess->num_common_caps : NULL);
    spice_info("NEW Client %p mcc %p connect-id %d", client, mcc, connection_id);
    free(link_mess);
    red_client_set_main(client, mcc);

    if (vdagent) {
        if (mig_target) {
            spice_warning("unexpected: vdagent attached to destination during migration");
        }
        reds->agent_state.read_filter.discard_all = FALSE;
        reds->agent_state.plug_generation++;
    }

    if (!mig_target) {
        main_channel_push_init(mcc, red_dispatcher_count(),
            reds->mouse_mode, reds->is_client_mouse_allowed,
            reds_get_mm_time() - MM_TIME_DELTA,
            red_dispatcher_qxl_ram_size());
        if (spice_name)
            main_channel_push_name(mcc, spice_name);
        if (spice_uuid_is_set)
            main_channel_push_uuid(mcc, spice_uuid);

        main_channel_client_start_net_test(mcc);
    } else {
        reds_mig_target_client_add(client);
    }
}

#define RED_MOUSE_STATE_TO_LOCAL(state)     \
    ((state & SPICE_MOUSE_BUTTON_MASK_LEFT) |          \
     ((state & SPICE_MOUSE_BUTTON_MASK_MIDDLE) << 1) |   \
     ((state & SPICE_MOUSE_BUTTON_MASK_RIGHT) >> 1))

#define RED_MOUSE_BUTTON_STATE_TO_AGENT(state)                      \
    (((state & SPICE_MOUSE_BUTTON_MASK_LEFT) ? VD_AGENT_LBUTTON_MASK : 0) |    \
     ((state & SPICE_MOUSE_BUTTON_MASK_MIDDLE) ? VD_AGENT_MBUTTON_MASK : 0) |    \
     ((state & SPICE_MOUSE_BUTTON_MASK_RIGHT) ? VD_AGENT_RBUTTON_MASK : 0))

void reds_set_client_mouse_allowed(int is_client_mouse_allowed, int x_res, int y_res)
{
    reds->monitor_mode.x_res = x_res;
    reds->monitor_mode.y_res = y_res;
    reds->dispatcher_allows_client_mouse = is_client_mouse_allowed;
    reds_update_mouse_mode();
    if (reds->is_client_mouse_allowed && inputs_has_tablet()) {
        inputs_set_tablet_logical_size(reds->monitor_mode.x_res, reds->monitor_mode.y_res);
    }
}

static void openssl_init(RedLinkInfo *link)
{
    unsigned long f4 = RSA_F4;
    link->tiTicketing.bn = BN_new();

    if (!link->tiTicketing.bn) {
        spice_error("OpenSSL BIGNUMS alloc failed");
    }

    BN_set_word(link->tiTicketing.bn, f4);
}

static void reds_channel_do_link(RedChannel *channel, RedClient *client,
                                 SpiceLinkMess *link_msg,
                                 RedsStream *stream)
{
    uint32_t *caps;

    spice_assert(channel);
    spice_assert(link_msg);
    spice_assert(stream);

    caps = (uint32_t *)((uint8_t *)link_msg + link_msg->caps_offset);
    channel->client_cbs.connect(channel, client, stream,
                                red_client_during_migrate_at_target(client),
                                link_msg->num_common_caps,
                                link_msg->num_common_caps ? caps : NULL,
                                link_msg->num_channel_caps,
                                link_msg->num_channel_caps ?
                                caps + link_msg->num_common_caps : NULL);
}

/*
 * migration target side:
 * In semi-seamless migration, we activate the channels only
 * after migration is completed.
 * In seamless migration, in order to keep the continuousness, and
 * not lose any data, we activate the target channels before
 * migration completes, as soon as we receive SPICE_MSGC_MAIN_MIGRATE_DST_DO_SEAMLESS
 */
static int reds_link_mig_target_channels(RedClient *client)
{
    RedsMigTargetClient *mig_client;
    RingItem *item;

    spice_info("%p", client);
    mig_client = reds_mig_target_client_find(client);
    if (!mig_client) {
        spice_info("Error: mig target client was not found");
        return FALSE;
    }

    /* Each channel should check if we are during migration, and
     * act accordingly. */
    RING_FOREACH(item, &mig_client->pending_links) {
        RedsMigPendingLink *mig_link;
        RedChannel *channel;

        mig_link = SPICE_CONTAINEROF(item, RedsMigPendingLink, ring_link);
        channel = reds_find_channel(mig_link->link_msg->channel_type,
                                    mig_link->link_msg->channel_id);
        if (!channel) {
            spice_warning("client %p channel (%d, %d) (type, id) wasn't found",
                          client,
                          mig_link->link_msg->channel_type,
                          mig_link->link_msg->channel_id);
            continue;
        }
        reds_channel_do_link(channel, client, mig_link->link_msg, mig_link->stream);
    }

    reds_mig_target_client_free(mig_client);

    return TRUE;
}

int reds_on_migrate_dst_set_seamless(MainChannelClient *mcc, uint32_t src_version)
{
    /* seamless migration is not supported with multiple clients*/
    if (reds->allow_multiple_clients  || src_version > SPICE_MIGRATION_PROTOCOL_VERSION) {
        reds->dst_do_seamless_migrate = FALSE;
    } else {
        RedChannelClient *rcc = main_channel_client_get_base(mcc);

        red_client_set_migration_seamless(rcc->client);
        /* linking all the channels that have been connected before migration handshake */
        reds->dst_do_seamless_migrate = reds_link_mig_target_channels(rcc->client);
    }
    return reds->dst_do_seamless_migrate;
}

void reds_on_client_seamless_migrate_complete(RedClient *client)
{
    spice_debug(NULL);
    if (!reds_find_client(client)) {
        spice_info("client no longer exists");
        return;
    }
    main_channel_migrate_dst_complete(red_client_get_main(client));
}

void reds_on_client_semi_seamless_migrate_complete(RedClient *client)
{
    MainChannelClient *mcc;

    spice_info("%p", client);
    mcc = red_client_get_main(client);

    // TODO: not doing net test. consider doing it on client_migrate_info
    main_channel_push_init(mcc, red_dispatcher_count(),
                           reds->mouse_mode, reds->is_client_mouse_allowed,
                           reds_get_mm_time() - MM_TIME_DELTA,
                           red_dispatcher_qxl_ram_size());
    reds_link_mig_target_channels(client);
    main_channel_migrate_dst_complete(mcc);
}

static void reds_handle_other_links(RedLinkInfo *link)
{
    RedChannel *channel;
    RedClient *client = NULL;
    SpiceLinkMess *link_mess;
    RedsMigTargetClient *mig_client;

    link_mess = link->link_mess;
    if (reds->main_channel) {
        client = main_channel_get_client_by_link_id(reds->main_channel,
                                                    link_mess->connection_id);
    }

    // TODO: MC: broke migration (at least for the dont-drop-connection kind).
    // On migration we should get a connection_id to expect (must be a security measure)
    // where do we store it? on reds, but should be a list (MC).
    if (!client) {
        reds_send_link_result(link, SPICE_LINK_ERR_BAD_CONNECTION_ID);
        reds_link_free(link);
        return;
    }

    // TODO: MC: be less lenient. Tally connections from same connection_id (by same client).
    if (!(channel = reds_find_channel(link_mess->channel_type,
                                      link_mess->channel_id))) {
        reds_send_link_result(link, SPICE_LINK_ERR_CHANNEL_NOT_AVAILABLE);
        reds_link_free(link);
        return;
    }

    reds_send_link_result(link, SPICE_LINK_ERR_OK);
    reds_info_new_channel(link, link_mess->connection_id);
    reds_stream_remove_watch(link->stream);

    mig_client = reds_mig_target_client_find(client);
    /*
     * In semi-seamless migration, we activate the channels only
     * after migration is completed. Since, the session starts almost from
     * scratch we don't mind if we skip some messages in between the src session end and
     * dst session start.
     * In seamless migration, in order to keep the continuousness of the session, and
     * in order not to lose any data, we activate the target channels before
     * migration completes, as soon as we receive SPICE_MSGC_MAIN_MIGRATE_DST_DO_SEAMLESS.
     * If a channel connects before receiving SPICE_MSGC_MAIN_MIGRATE_DST_DO_SEAMLESS,
     * reds_on_migrate_dst_set_seamless will take care of activating it */
    if (red_client_during_migrate_at_target(client) && !reds->dst_do_seamless_migrate) {
        spice_assert(mig_client);
        reds_mig_target_client_add_pending_link(mig_client, link_mess, link->stream);
    } else {
        spice_assert(!mig_client);
        reds_channel_do_link(channel, client, link_mess, link->stream);
        free(link_mess);
    }
    link->stream = NULL;
    link->link_mess = NULL;
    reds_link_free(link);
}

static void reds_handle_link(RedLinkInfo *link)
{
    if (link->link_mess->channel_type == SPICE_CHANNEL_MAIN) {
        reds_handle_main_link(link);
    } else {
        reds_handle_other_links(link);
    }
}

static void reds_handle_ticket(void *opaque)
{
    RedLinkInfo *link = (RedLinkInfo *)opaque;
    char password[SPICE_MAX_PASSWORD_LENGTH];
    time_t ltime;

    //todo: use monotonic time
    time(&ltime);
    RSA_private_decrypt(link->tiTicketing.rsa_size,
                        link->tiTicketing.encrypted_ticket.encrypted_data,
                        (unsigned char *)password, link->tiTicketing.rsa, RSA_PKCS1_OAEP_PADDING);

    if (ticketing_enabled && !link->skip_auth) {
        int expired =  taTicket.expiration_time < ltime;

        if (strlen(taTicket.password) == 0) {
            reds_send_link_result(link, SPICE_LINK_ERR_PERMISSION_DENIED);
            spice_warning("Ticketing is enabled, but no password is set. "
                        "please set a ticket first");
            reds_link_free(link);
            return;
        }

        if (expired || strncmp(password, taTicket.password, SPICE_MAX_PASSWORD_LENGTH) != 0) {
            if (expired) {
                spice_warning("Ticket has expired");
            } else {
                spice_warning("Invalid password");
            }
            reds_send_link_result(link, SPICE_LINK_ERR_PERMISSION_DENIED);
            reds_link_free(link);
            return;
        }
    }

    reds_handle_link(link);
}

static inline void async_read_clear_handlers(AsyncRead *obj)
{
    if (!obj->stream->watch) {
        return;
    }

    reds_stream_remove_watch(obj->stream);
}

#if HAVE_SASL
static int sync_write_u8(RedsStream *s, uint8_t n)
{
    return sync_write(s, &n, sizeof(uint8_t));
}

static int sync_write_u32(RedsStream *s, uint32_t n)
{
    return sync_write(s, &n, sizeof(uint32_t));
}

static ssize_t reds_stream_sasl_write(RedsStream *s, const void *buf, size_t nbyte)
{
    ssize_t ret;

    if (!s->sasl.encoded) {
        int err;
        err = sasl_encode(s->sasl.conn, (char *)buf, nbyte,
                          (const char **)&s->sasl.encoded,
                          &s->sasl.encodedLength);
        if (err != SASL_OK) {
            spice_warning("sasl_encode error: %d", err);
            return -1;
        }

        if (s->sasl.encodedLength == 0) {
            return 0;
        }

        if (!s->sasl.encoded) {
            spice_warning("sasl_encode didn't return a buffer!");
            return 0;
        }

        s->sasl.encodedOffset = 0;
    }

    ret = s->write(s, s->sasl.encoded + s->sasl.encodedOffset,
                   s->sasl.encodedLength - s->sasl.encodedOffset);

    if (ret <= 0) {
        return ret;
    }

    s->sasl.encodedOffset += ret;
    if (s->sasl.encodedOffset == s->sasl.encodedLength) {
        s->sasl.encoded = NULL;
        s->sasl.encodedOffset = s->sasl.encodedLength = 0;
        return nbyte;
    }

    /* we didn't flush the encoded buffer */
    errno = EAGAIN;
    return -1;
}

static ssize_t reds_stream_sasl_read(RedsStream *s, uint8_t *buf, size_t nbyte)
{
    uint8_t encoded[4096];
    const char *decoded;
    unsigned int decodedlen;
    int err;
    int n;

    n = spice_buffer_copy(&s->sasl.inbuffer, buf, nbyte);
    if (n > 0) {
        spice_buffer_remove(&s->sasl.inbuffer, n);
        if (n == nbyte)
            return n;
        nbyte -= n;
        buf += n;
    }

    n = s->read(s, encoded, sizeof(encoded));
    if (n <= 0) {
        return n;
    }

    err = sasl_decode(s->sasl.conn,
                      (char *)encoded, n,
                      &decoded, &decodedlen);
    if (err != SASL_OK) {
        spice_warning("sasl_decode error: %d", err);
        return -1;
    }

    if (decodedlen == 0) {
        errno = EAGAIN;
        return -1;
    }

    n = MIN(nbyte, decodedlen);
    memcpy(buf, decoded, n);
    spice_buffer_append(&s->sasl.inbuffer, decoded + n, decodedlen - n);
    return n;
}
#endif

static void async_read_handler(int fd, int event, void *data)
{
    AsyncRead *obj = (AsyncRead *)data;

    for (;;) {
        int n = obj->end - obj->now;

        spice_assert(n > 0);
        n = reds_stream_read(obj->stream, obj->now, n);
        if (n <= 0) {
            if (n < 0) {
                switch (errno) {
                case EAGAIN:
                    if (!obj->stream->watch) {
                        obj->stream->watch = core->watch_add(obj->stream->socket,
                                                           SPICE_WATCH_EVENT_READ,
                                                           async_read_handler, obj);
                    }
                    return;
                case EINTR:
                    break;
                default:
                    async_read_clear_handlers(obj);
                    obj->error(obj->opaque, errno);
                    return;
                }
            } else {
                async_read_clear_handlers(obj);
                obj->error(obj->opaque, 0);
                return;
            }
        } else {
            obj->now += n;
            if (obj->now == obj->end) {
                async_read_clear_handlers(obj);
                obj->done(obj->opaque);
                return;
            }
        }
    }
}

static void reds_get_spice_ticket(RedLinkInfo *link)
{
    AsyncRead *obj = &link->asyc_read;

    obj->now = (uint8_t *)&link->tiTicketing.encrypted_ticket.encrypted_data;
    obj->end = obj->now + link->tiTicketing.rsa_size;
    obj->done = reds_handle_ticket;
    async_read_handler(0, 0, &link->asyc_read);
}

#if HAVE_SASL
static char *addr_to_string(const char *format,
                            struct sockaddr_storage *sa,
                            socklen_t salen) {
    char *addr;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    int err;
    size_t addrlen;

    if ((err = getnameinfo((struct sockaddr *)sa, salen,
                           host, sizeof(host),
                           serv, sizeof(serv),
                           NI_NUMERICHOST | NI_NUMERICSERV)) != 0) {
        spice_warning("Cannot resolve address %d: %s",
                      err, gai_strerror(err));
        return NULL;
    }

    /* Enough for the existing format + the 2 vars we're
     * substituting in. */
    addrlen = strlen(format) + strlen(host) + strlen(serv);
    addr = spice_malloc(addrlen + 1);
    snprintf(addr, addrlen, format, host, serv);
    addr[addrlen] = '\0';

    return addr;
}

static int auth_sasl_check_ssf(RedsSASL *sasl, int *runSSF)
{
    const void *val;
    int err, ssf;

    *runSSF = 0;
    if (!sasl->wantSSF) {
        return 1;
    }

    err = sasl_getprop(sasl->conn, SASL_SSF, &val);
    if (err != SASL_OK) {
        return 0;
    }

    ssf = *(const int *)val;
    spice_info("negotiated an SSF of %d", ssf);
    if (ssf < 56) {
        return 0; /* 56 is good for Kerberos */
    }

    *runSSF = 1;

    /* We have a SSF that's good enough */
    return 1;
}

/*
 * Step Msg
 *
 * Input from client:
 *
 * u32 clientin-length
 * u8-array clientin-string
 *
 * Output to client:
 *
 * u32 serverout-length
 * u8-array serverout-strin
 * u8 continue
 */
#define SASL_DATA_MAX_LEN (1024 * 1024)

static void reds_handle_auth_sasl_steplen(void *opaque);

static void reds_handle_auth_sasl_step(void *opaque)
{
    const char *serverout;
    unsigned int serveroutlen;
    int err;
    char *clientdata = NULL;
    RedLinkInfo *link = (RedLinkInfo *)opaque;
    RedsSASL *sasl = &link->stream->sasl;
    uint32_t datalen = sasl->len;
    AsyncRead *obj = &link->asyc_read;

    /* NB, distinction of NULL vs "" is *critical* in SASL */
    if (datalen) {
        clientdata = sasl->data;
        clientdata[datalen - 1] = '\0'; /* Wire includes '\0', but make sure */
        datalen--; /* Don't count NULL byte when passing to _start() */
    }

    spice_info("Step using SASL Data %p (%d bytes)",
               clientdata, datalen);
    err = sasl_server_step(sasl->conn,
                           clientdata,
                           datalen,
                           &serverout,
                           &serveroutlen);
    if (err != SASL_OK &&
        err != SASL_CONTINUE) {
        spice_warning("sasl step failed %d (%s)",
                      err, sasl_errdetail(sasl->conn));
        goto authabort;
    }

    if (serveroutlen > SASL_DATA_MAX_LEN) {
        spice_warning("sasl step reply data too long %d",
                      serveroutlen);
        goto authabort;
    }

    spice_info("SASL return data %d bytes, %p", serveroutlen, serverout);

    if (serveroutlen) {
        serveroutlen += 1;
        sync_write(link->stream, &serveroutlen, sizeof(uint32_t));
        sync_write(link->stream, serverout, serveroutlen);
    } else {
        sync_write(link->stream, &serveroutlen, sizeof(uint32_t));
    }

    /* Whether auth is complete */
    sync_write_u8(link->stream, err == SASL_CONTINUE ? 0 : 1);

    if (err == SASL_CONTINUE) {
        spice_info("%s", "Authentication must continue (step)");
        /* Wait for step length */
        obj->now = (uint8_t *)&sasl->len;
        obj->end = obj->now + sizeof(uint32_t);
        obj->done = reds_handle_auth_sasl_steplen;
        async_read_handler(0, 0, &link->asyc_read);
    } else {
        int ssf;

        if (auth_sasl_check_ssf(sasl, &ssf) == 0) {
            spice_warning("Authentication rejected for weak SSF");
            goto authreject;
        }

        spice_info("Authentication successful");
        sync_write_u32(link->stream, SPICE_LINK_ERR_OK); /* Accept auth */

        /*
         * Delay writing in SSF encoded until now
         */
        sasl->runSSF = ssf;
        link->stream->writev = NULL; /* make sure writev isn't called directly anymore */

        reds_handle_link(link);
    }

    return;

authreject:
    sync_write_u32(link->stream, 1); /* Reject auth */
    sync_write_u32(link->stream, sizeof("Authentication failed"));
    sync_write(link->stream, "Authentication failed", sizeof("Authentication failed"));

authabort:
    reds_link_free(link);
    return;
}

static void reds_handle_auth_sasl_steplen(void *opaque)
{
    RedLinkInfo *link = (RedLinkInfo *)opaque;
    AsyncRead *obj = &link->asyc_read;
    RedsSASL *sasl = &link->stream->sasl;

    spice_info("Got steplen %d", sasl->len);
    if (sasl->len > SASL_DATA_MAX_LEN) {
        spice_warning("Too much SASL data %d", sasl->len);
        reds_link_free(link);
        return;
    }

    if (sasl->len == 0) {
        return reds_handle_auth_sasl_step(opaque);
    } else {
        sasl->data = spice_realloc(sasl->data, sasl->len);
        obj->now = (uint8_t *)sasl->data;
        obj->end = obj->now + sasl->len;
        obj->done = reds_handle_auth_sasl_step;
        async_read_handler(0, 0, &link->asyc_read);
    }
}

/*
 * Start Msg
 *
 * Input from client:
 *
 * u32 clientin-length
 * u8-array clientin-string
 *
 * Output to client:
 *
 * u32 serverout-length
 * u8-array serverout-strin
 * u8 continue
 */


static void reds_handle_auth_sasl_start(void *opaque)
{
    RedLinkInfo *link = (RedLinkInfo *)opaque;
    AsyncRead *obj = &link->asyc_read;
    const char *serverout;
    unsigned int serveroutlen;
    int err;
    char *clientdata = NULL;
    RedsSASL *sasl = &link->stream->sasl;
    uint32_t datalen = sasl->len;

    /* NB, distinction of NULL vs "" is *critical* in SASL */
    if (datalen) {
        clientdata = sasl->data;
        clientdata[datalen - 1] = '\0'; /* Should be on wire, but make sure */
        datalen--; /* Don't count NULL byte when passing to _start() */
    }

    spice_info("Start SASL auth with mechanism %s. Data %p (%d bytes)",
               sasl->mechlist, clientdata, datalen);
    err = sasl_server_start(sasl->conn,
                            sasl->mechlist,
                            clientdata,
                            datalen,
                            &serverout,
                            &serveroutlen);
    if (err != SASL_OK &&
        err != SASL_CONTINUE) {
        spice_warning("sasl start failed %d (%s)",
                    err, sasl_errdetail(sasl->conn));
        goto authabort;
    }

    if (serveroutlen > SASL_DATA_MAX_LEN) {
        spice_warning("sasl start reply data too long %d",
                    serveroutlen);
        goto authabort;
    }

    spice_info("SASL return data %d bytes, %p", serveroutlen, serverout);

    if (serveroutlen) {
        serveroutlen += 1;
        sync_write(link->stream, &serveroutlen, sizeof(uint32_t));
        sync_write(link->stream, serverout, serveroutlen);
    } else {
        sync_write(link->stream, &serveroutlen, sizeof(uint32_t));
    }

    /* Whether auth is complete */
    sync_write_u8(link->stream, err == SASL_CONTINUE ? 0 : 1);

    if (err == SASL_CONTINUE) {
        spice_info("%s", "Authentication must continue (start)");
        /* Wait for step length */
        obj->now = (uint8_t *)&sasl->len;
        obj->end = obj->now + sizeof(uint32_t);
        obj->done = reds_handle_auth_sasl_steplen;
        async_read_handler(0, 0, &link->asyc_read);
    } else {
        int ssf;

        if (auth_sasl_check_ssf(sasl, &ssf) == 0) {
            spice_warning("Authentication rejected for weak SSF");
            goto authreject;
        }

        spice_info("Authentication successful");
        sync_write_u32(link->stream, SPICE_LINK_ERR_OK); /* Accept auth */

        /*
         * Delay writing in SSF encoded until now
         */
        sasl->runSSF = ssf;
        link->stream->writev = NULL; /* make sure writev isn't called directly anymore */

        reds_handle_link(link);
    }

    return;

authreject:
    sync_write_u32(link->stream, 1); /* Reject auth */
    sync_write_u32(link->stream, sizeof("Authentication failed"));
    sync_write(link->stream, "Authentication failed", sizeof("Authentication failed"));

authabort:
    reds_link_free(link);
    return;
}

static void reds_handle_auth_startlen(void *opaque)
{
    RedLinkInfo *link = (RedLinkInfo *)opaque;
    AsyncRead *obj = &link->asyc_read;
    RedsSASL *sasl = &link->stream->sasl;

    spice_info("Got client start len %d", sasl->len);
    if (sasl->len > SASL_DATA_MAX_LEN) {
        spice_warning("Too much SASL data %d", sasl->len);
        reds_send_link_error(link, SPICE_LINK_ERR_INVALID_DATA);
        reds_link_free(link);
        return;
    }

    if (sasl->len == 0) {
        reds_handle_auth_sasl_start(opaque);
        return;
    }

    sasl->data = spice_realloc(sasl->data, sasl->len);
    obj->now = (uint8_t *)sasl->data;
    obj->end = obj->now + sasl->len;
    obj->done = reds_handle_auth_sasl_start;
    async_read_handler(0, 0, &link->asyc_read);
}

static void reds_handle_auth_mechname(void *opaque)
{
    RedLinkInfo *link = (RedLinkInfo *)opaque;
    AsyncRead *obj = &link->asyc_read;
    RedsSASL *sasl = &link->stream->sasl;

    sasl->mechname[sasl->len] = '\0';
    spice_info("Got client mechname '%s' check against '%s'",
               sasl->mechname, sasl->mechlist);

    if (strncmp(sasl->mechlist, sasl->mechname, sasl->len) == 0) {
        if (sasl->mechlist[sasl->len] != '\0' &&
            sasl->mechlist[sasl->len] != ',') {
            spice_info("One %d", sasl->mechlist[sasl->len]);
            reds_link_free(link);
            return;
        }
    } else {
        char *offset = strstr(sasl->mechlist, sasl->mechname);
        spice_info("Two %p", offset);
        if (!offset) {
            reds_send_link_error(link, SPICE_LINK_ERR_INVALID_DATA);
            return;
        }
        spice_info("Two '%s'", offset);
        if (offset[-1] != ',' ||
            (offset[sasl->len] != '\0'&&
             offset[sasl->len] != ',')) {
            reds_send_link_error(link, SPICE_LINK_ERR_INVALID_DATA);
            return;
        }
    }

    free(sasl->mechlist);
    sasl->mechlist = spice_strdup(sasl->mechname);

    spice_info("Validated mechname '%s'", sasl->mechname);

    obj->now = (uint8_t *)&sasl->len;
    obj->end = obj->now + sizeof(uint32_t);
    obj->done = reds_handle_auth_startlen;
    async_read_handler(0, 0, &link->asyc_read);

    return;
}

static void reds_handle_auth_mechlen(void *opaque)
{
    RedLinkInfo *link = (RedLinkInfo *)opaque;
    AsyncRead *obj = &link->asyc_read;
    RedsSASL *sasl = &link->stream->sasl;

    if (sasl->len < 1 || sasl->len > 100) {
        spice_warning("Got bad client mechname len %d", sasl->len);
        reds_link_free(link);
        return;
    }

    sasl->mechname = spice_malloc(sasl->len + 1);

    spice_info("Wait for client mechname");
    obj->now = (uint8_t *)sasl->mechname;
    obj->end = obj->now + sasl->len;
    obj->done = reds_handle_auth_mechname;
    async_read_handler(0, 0, &link->asyc_read);
}

static void reds_start_auth_sasl(RedLinkInfo *link)
{
    const char *mechlist = NULL;
    sasl_security_properties_t secprops;
    int err;
    char *localAddr, *remoteAddr;
    int mechlistlen;
    AsyncRead *obj = &link->asyc_read;
    RedsSASL *sasl = &link->stream->sasl;

    /* Get local & remote client addresses in form  IPADDR;PORT */
    if (!(localAddr = addr_to_string("%s;%s", &link->stream->info->laddr_ext,
                                              link->stream->info->llen_ext))) {
        goto error;
    }

    if (!(remoteAddr = addr_to_string("%s;%s", &link->stream->info->paddr_ext,
                                               link->stream->info->plen_ext))) {
        free(localAddr);
        goto error;
    }

    err = sasl_server_new("spice",
                          NULL, /* FQDN - just delegates to gethostname */
                          NULL, /* User realm */
                          localAddr,
                          remoteAddr,
                          NULL, /* Callbacks, not needed */
                          SASL_SUCCESS_DATA,
                          &sasl->conn);
    free(localAddr);
    free(remoteAddr);
    localAddr = remoteAddr = NULL;

    if (err != SASL_OK) {
        spice_warning("sasl context setup failed %d (%s)",
                    err, sasl_errstring(err, NULL, NULL));
        sasl->conn = NULL;
        goto error;
    }

    /* Inform SASL that we've got an external SSF layer from TLS */
    if (link->stream->ssl) {
        sasl_ssf_t ssf;

        ssf = SSL_get_cipher_bits(link->stream->ssl, NULL);
        err = sasl_setprop(sasl->conn, SASL_SSF_EXTERNAL, &ssf);
        if (err != SASL_OK) {
            spice_warning("cannot set SASL external SSF %d (%s)",
                        err, sasl_errstring(err, NULL, NULL));
            goto error_dispose;
        }
    } else {
        sasl->wantSSF = 1;
    }

    memset(&secprops, 0, sizeof secprops);
    /* Inform SASL that we've got an external SSF layer from TLS */
    if (link->stream->ssl) {
        /* If we've got TLS (or UNIX domain sock), we don't care about SSF */
        secprops.min_ssf = 0;
        secprops.max_ssf = 0;
        secprops.maxbufsize = 8192;
        secprops.security_flags = 0;
    } else {
        /* Plain TCP, better get an SSF layer */
        secprops.min_ssf = 56; /* Good enough to require kerberos */
        secprops.max_ssf = 100000; /* Arbitrary big number */
        secprops.maxbufsize = 8192;
        /* Forbid any anonymous or trivially crackable auth */
        secprops.security_flags =
            SASL_SEC_NOANONYMOUS | SASL_SEC_NOPLAINTEXT;
    }

    err = sasl_setprop(sasl->conn, SASL_SEC_PROPS, &secprops);
    if (err != SASL_OK) {
        spice_warning("cannot set SASL security props %d (%s)",
                      err, sasl_errstring(err, NULL, NULL));
        goto error_dispose;
    }

    err = sasl_listmech(sasl->conn,
                        NULL, /* Don't need to set user */
                        "", /* Prefix */
                        ",", /* Separator */
                        "", /* Suffix */
                        &mechlist,
                        NULL,
                        NULL);
    if (err != SASL_OK || mechlist == NULL) {
        spice_warning("cannot list SASL mechanisms %d (%s)",
                      err, sasl_errdetail(sasl->conn));
        goto error_dispose;
    }

    spice_info("Available mechanisms for client: '%s'", mechlist);

    sasl->mechlist = spice_strdup(mechlist);

    mechlistlen = strlen(mechlist);
    if (!sync_write(link->stream, &mechlistlen, sizeof(uint32_t))
        || !sync_write(link->stream, sasl->mechlist, mechlistlen)) {
        spice_warning("SASL mechanisms write error");
        goto error;
    }

    spice_info("Wait for client mechname length");
    obj->now = (uint8_t *)&sasl->len;
    obj->end = obj->now + sizeof(uint32_t);
    obj->done = reds_handle_auth_mechlen;
    async_read_handler(0, 0, &link->asyc_read);

    return;

error_dispose:
    sasl_dispose(&sasl->conn);
    sasl->conn = NULL;
error:
    reds_link_free(link);
    return;
}
#endif

static void reds_handle_auth_mechanism(void *opaque)
{
    RedLinkInfo *link = (RedLinkInfo *)opaque;

    spice_info("Auth method: %d", link->auth_mechanism.auth_mechanism);

    if (link->auth_mechanism.auth_mechanism == SPICE_COMMON_CAP_AUTH_SPICE
        && !sasl_enabled
        ) {
        reds_get_spice_ticket(link);
#if HAVE_SASL
    } else if (link->auth_mechanism.auth_mechanism == SPICE_COMMON_CAP_AUTH_SASL) {
        spice_info("Starting SASL");
        reds_start_auth_sasl(link);
#endif
    } else {
        spice_warning("Unknown auth method, disconnecting");
        if (sasl_enabled) {
            spice_warning("Your client doesn't handle SASL?");
        }
        reds_send_link_error(link, SPICE_LINK_ERR_INVALID_DATA);
        reds_link_free(link);
    }
}

static int reds_security_check(RedLinkInfo *link)
{
    ChannelSecurityOptions *security_option = find_channel_security(link->link_mess->channel_type);
    uint32_t security = security_option ? security_option->options : default_channel_security;
    return (link->stream->ssl && (security & SPICE_CHANNEL_SECURITY_SSL)) ||
        (!link->stream->ssl && (security & SPICE_CHANNEL_SECURITY_NONE));
}

static void reds_handle_read_link_done(void *opaque)
{
    RedLinkInfo *link = (RedLinkInfo *)opaque;
    SpiceLinkMess *link_mess = link->link_mess;
    AsyncRead *obj = &link->asyc_read;
    uint32_t num_caps = link_mess->num_common_caps + link_mess->num_channel_caps;
    uint32_t *caps = (uint32_t *)((uint8_t *)link_mess + link_mess->caps_offset);
    int auth_selection;

    if (num_caps && (num_caps * sizeof(uint32_t) + link_mess->caps_offset >
                     link->link_header.size ||
                     link_mess->caps_offset < sizeof(*link_mess))) {
        reds_send_link_error(link, SPICE_LINK_ERR_INVALID_DATA);
        reds_link_free(link);
        return;
    }

    auth_selection = test_capabilty(caps, link_mess->num_common_caps,
                                    SPICE_COMMON_CAP_PROTOCOL_AUTH_SELECTION);

    if (!reds_security_check(link)) {
        if (link->stream->ssl) {
            spice_warning("spice channels %d should not be encrypted", link_mess->channel_type);
            reds_send_link_error(link, SPICE_LINK_ERR_NEED_UNSECURED);
        } else {
            spice_warning("spice channels %d should be encrypted", link_mess->channel_type);
            reds_send_link_error(link, SPICE_LINK_ERR_NEED_SECURED);
        }
        reds_link_free(link);
        return;
    }

    if (!reds_send_link_ack(link)) {
        reds_link_free(link);
        return;
    }

    if (!auth_selection) {
        if (sasl_enabled && !link->skip_auth) {
            spice_warning("SASL enabled, but peer supports only spice authentication");
            reds_send_link_error(link, SPICE_LINK_ERR_VERSION_MISMATCH);
            return;
        }
        spice_warning("Peer doesn't support AUTH selection");
        reds_get_spice_ticket(link);
    } else {
        obj->now = (uint8_t *)&link->auth_mechanism;
        obj->end = obj->now + sizeof(SpiceLinkAuthMechanism);
        obj->done = reds_handle_auth_mechanism;
        async_read_handler(0, 0, &link->asyc_read);
    }
}

static void reds_handle_link_error(void *opaque, int err)
{
    RedLinkInfo *link = (RedLinkInfo *)opaque;
    switch (err) {
    case 0:
    case EPIPE:
        break;
    default:
        spice_warning("%s", strerror(errno));
        break;
    }
    reds_link_free(link);
}

static void reds_handle_read_header_done(void *opaque)
{
    RedLinkInfo *link = (RedLinkInfo *)opaque;
    SpiceLinkHeader *header = &link->link_header;
    AsyncRead *obj = &link->asyc_read;

    if (header->magic != SPICE_MAGIC) {
        reds_send_link_error(link, SPICE_LINK_ERR_INVALID_MAGIC);
        reds_link_free(link);
        return;
    }

    if (header->major_version != SPICE_VERSION_MAJOR) {
        if (header->major_version > 0) {
            reds_send_link_error(link, SPICE_LINK_ERR_VERSION_MISMATCH);
        }

        spice_warning("version mismatch");
        reds_link_free(link);
        return;
    }

    reds->peer_minor_version = header->minor_version;

    if (header->size < sizeof(SpiceLinkMess)) {
        reds_send_link_error(link, SPICE_LINK_ERR_INVALID_DATA);
        spice_warning("bad size %u", header->size);
        reds_link_free(link);
        return;
    }

    link->link_mess = spice_malloc(header->size);

    obj->now = (uint8_t *)link->link_mess;
    obj->end = obj->now + header->size;
    obj->done = reds_handle_read_link_done;
    async_read_handler(0, 0, &link->asyc_read);
}

static void reds_handle_new_link(RedLinkInfo *link)
{
    AsyncRead *obj = &link->asyc_read;
    obj->opaque = link;
    obj->stream = link->stream;
    obj->now = (uint8_t *)&link->link_header;
    obj->end = (uint8_t *)((SpiceLinkHeader *)&link->link_header + 1);
    obj->done = reds_handle_read_header_done;
    obj->error = reds_handle_link_error;
    async_read_handler(0, 0, &link->asyc_read);
}

static void reds_handle_ssl_accept(int fd, int event, void *data)
{
    RedLinkInfo *link = (RedLinkInfo *)data;
    int return_code;

    if ((return_code = SSL_accept(link->stream->ssl)) != 1) {
        int ssl_error = SSL_get_error(link->stream->ssl, return_code);
        if (ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE) {
            spice_warning("SSL_accept failed, error=%d", ssl_error);
            reds_link_free(link);
        } else {
            if (ssl_error == SSL_ERROR_WANT_READ) {
                core->watch_update_mask(link->stream->watch, SPICE_WATCH_EVENT_READ);
            } else {
                core->watch_update_mask(link->stream->watch, SPICE_WATCH_EVENT_WRITE);
            }
        }
        return;
    }
    reds_stream_remove_watch(link->stream);
    reds_handle_new_link(link);
}

static RedLinkInfo *reds_init_client_connection(int socket)
{
    RedLinkInfo *link;
    RedsStream *stream;
    int delay_val = 1;
    int flags;

    if ((flags = fcntl(socket, F_GETFL)) == -1) {
        spice_warning("accept failed, %s", strerror(errno));
        goto error;
    }

    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
        spice_warning("accept failed, %s", strerror(errno));
        goto error;
    }

    if (setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &delay_val, sizeof(delay_val)) == -1) {
        if (errno != ENOTSUP) {
            spice_warning("setsockopt failed, %s", strerror(errno));
        }
    }

    link = spice_new0(RedLinkInfo, 1);
    stream = spice_new0(RedsStream, 1);
    stream->info = spice_new0(SpiceChannelEventInfo, 1);
    link->stream = stream;

    stream->socket = socket;
    /* gather info + send event */

    /* deprecated fields. Filling them for backward compatibility */
    stream->info->llen = sizeof(stream->info->laddr);
    stream->info->plen = sizeof(stream->info->paddr);
    getsockname(stream->socket, (struct sockaddr*)(&stream->info->laddr), &stream->info->llen);
    getpeername(stream->socket, (struct sockaddr*)(&stream->info->paddr), &stream->info->plen);

    stream->info->flags |= SPICE_CHANNEL_EVENT_FLAG_ADDR_EXT;
    stream->info->llen_ext = sizeof(stream->info->laddr_ext);
    stream->info->plen_ext = sizeof(stream->info->paddr_ext);
    getsockname(stream->socket, (struct sockaddr*)(&stream->info->laddr_ext),
                &stream->info->llen_ext);
    getpeername(stream->socket, (struct sockaddr*)(&stream->info->paddr_ext),
                &stream->info->plen_ext);

    reds_stream_push_channel_event(stream, SPICE_CHANNEL_EVENT_CONNECTED);

    openssl_init(link);

    return link;

error:
    return NULL;
}


static RedLinkInfo *reds_init_client_ssl_connection(int socket)
{
    RedLinkInfo *link;
    int return_code;
    int ssl_error;
    BIO *sbio;

    link = reds_init_client_connection(socket);
    if (link == NULL)
        goto error;

    // Handle SSL handshaking
    if (!(sbio = BIO_new_socket(link->stream->socket, BIO_NOCLOSE))) {
        spice_warning("could not allocate ssl bio socket");
        goto error;
    }

    link->stream->ssl = SSL_new(reds->ctx);
    if (!link->stream->ssl) {
        spice_warning("could not allocate ssl context");
        BIO_free(sbio);
        goto error;
    }

    SSL_set_bio(link->stream->ssl, sbio, sbio);

    link->stream->write = stream_ssl_write_cb;
    link->stream->read = stream_ssl_read_cb;
    link->stream->writev = NULL;

    return_code = SSL_accept(link->stream->ssl);
    if (return_code == 1) {
        reds_handle_new_link(link);
        return link;
    }

    ssl_error = SSL_get_error(link->stream->ssl, return_code);
    if (return_code == -1 && (ssl_error == SSL_ERROR_WANT_READ ||
                              ssl_error == SSL_ERROR_WANT_WRITE)) {
        int eventmask = ssl_error == SSL_ERROR_WANT_READ ?
            SPICE_WATCH_EVENT_READ : SPICE_WATCH_EVENT_WRITE;
        link->stream->watch = core->watch_add(link->stream->socket, eventmask,
                                            reds_handle_ssl_accept, link);
        return link;
    }

    ERR_print_errors_fp(stderr);
    spice_warning("SSL_accept failed, error=%d", ssl_error);
    SSL_free(link->stream->ssl);

error:
    free(link->stream);
    BN_free(link->tiTicketing.bn);
    free(link);
    return NULL;
}

static void reds_accept_ssl_connection(int fd, int event, void *data)
{
    RedLinkInfo *link;
    int socket;

    if ((socket = accept(reds->secure_listen_socket, NULL, 0)) == -1) {
        spice_warning("accept failed, %s", strerror(errno));
        return;
    }

    if (!(link = reds_init_client_ssl_connection(socket))) {
        close(socket);
        return;
    }
}


static void reds_accept(int fd, int event, void *data)
{
    int socket;

    if ((socket = accept(reds->listen_socket, NULL, 0)) == -1) {
        spice_warning("accept failed, %s", strerror(errno));
        return;
    }

    if (spice_server_add_client(reds, socket, 0) < 0)
        close(socket);
}


SPICE_GNUC_VISIBLE int spice_server_add_client(SpiceServer *s, int socket, int skip_auth)
{
    RedLinkInfo *link;
    RedsStream *stream;

    spice_assert(reds == s);
    if (!(link = reds_init_client_connection(socket))) {
        spice_warning("accept failed");
        return -1;
    }

    link->skip_auth = skip_auth;

    stream = link->stream;
    stream->read = stream_read_cb;
    stream->write = stream_write_cb;
    stream->writev = stream_writev_cb;

    reds_handle_new_link(link);
    return 0;
}


SPICE_GNUC_VISIBLE int spice_server_add_ssl_client(SpiceServer *s, int socket, int skip_auth)
{
    RedLinkInfo *link;

    spice_assert(reds == s);
    if (!(link = reds_init_client_ssl_connection(socket))) {
        return -1;
    }

    link->skip_auth = skip_auth;
    return 0;
}


static int reds_init_socket(const char *addr, int portnr, int family)
{
    static const int on=1, off=0;
    struct addrinfo ai,*res,*e;
    char port[33];
    int slisten,rc;

    memset(&ai,0, sizeof(ai));
    ai.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    ai.ai_socktype = SOCK_STREAM;
    ai.ai_family = family;

    snprintf(port, sizeof(port), "%d", portnr);
    rc = getaddrinfo(strlen(addr) ? addr : NULL, port, &ai, &res);
    if (rc != 0) {
        spice_warning("getaddrinfo(%s,%s): %s", addr, port,
                      gai_strerror(rc));
        return -1;
    }

    for (e = res; e != NULL; e = e->ai_next) {
        slisten = socket(e->ai_family, e->ai_socktype, e->ai_protocol);
        if (slisten < 0) {
            continue;
        }

        setsockopt(slisten,SOL_SOCKET,SO_REUSEADDR,(void*)&on,sizeof(on));
#ifdef IPV6_V6ONLY
        if (e->ai_family == PF_INET6) {
            /* listen on both ipv4 and ipv6 */
            setsockopt(slisten,IPPROTO_IPV6,IPV6_V6ONLY,(void*)&off,
                       sizeof(off));
        }
#endif
        if (bind(slisten, e->ai_addr, e->ai_addrlen) == 0) {
            char uaddr[INET6_ADDRSTRLEN+1];
            char uport[33];
            rc = getnameinfo((struct sockaddr*)e->ai_addr,e->ai_addrlen,
                             uaddr,INET6_ADDRSTRLEN, uport,32,
                             NI_NUMERICHOST | NI_NUMERICSERV);
            if (rc == 0) {
                spice_info("bound to %s:%s", uaddr, uport);
            } else {
                spice_info("cannot resolve address spice-server is bound to");
            }
            goto listen;
        }
        close(slisten);
    }
    spice_warning("%s: binding socket to %s:%d failed", __FUNCTION__,
                  addr, portnr);
    freeaddrinfo(res);
    return -1;

listen:
    freeaddrinfo(res);
    if (listen(slisten,1) != 0) {
        spice_warning("listen: %s", strerror(errno));
        close(slisten);
        return -1;
    }
    return slisten;
}

static void reds_send_mm_time(void)
{
    if (!reds_main_channel_connected()) {
        return;
    }
    spice_debug(NULL);
    main_channel_push_multi_media_time(reds->main_channel,
                                       reds_get_mm_time() - reds->mm_time_latency);
}

void reds_set_client_mm_time_latency(RedClient *client, uint32_t latency)
{
    // TODO: multi-client support for mm_time
    if (reds->mm_timer_enabled) {
        // TODO: consider network latency
        if (latency > reds->mm_time_latency) {
            reds->mm_time_latency = latency;
            reds_send_mm_time();
        } else {
            spice_debug("new latency %u is smaller than existing %u",
                        latency, reds->mm_time_latency);
        }
    } else {
        snd_set_playback_latency(client, latency);
    }
}

static int reds_init_net(void)
{
    if (spice_port != -1) {
        reds->listen_socket = reds_init_socket(spice_addr, spice_port, spice_family);
        if (-1 == reds->listen_socket) {
            return -1;
        }
        reds->listen_watch = core->watch_add(reds->listen_socket,
                                             SPICE_WATCH_EVENT_READ,
                                             reds_accept, NULL);
        if (reds->listen_watch == NULL) {
            spice_warning("set fd handle failed");
            return -1;
        }
    }

    if (spice_secure_port != -1) {
        reds->secure_listen_socket = reds_init_socket(spice_addr, spice_secure_port,
                                                      spice_family);
        if (-1 == reds->secure_listen_socket) {
            return -1;
        }
        reds->secure_listen_watch = core->watch_add(reds->secure_listen_socket,
                                                    SPICE_WATCH_EVENT_READ,
                                                    reds_accept_ssl_connection, NULL);
        if (reds->secure_listen_watch == NULL) {
            spice_warning("set fd handle failed");
            return -1;
        }
    }

    if (spice_listen_socket_fd != -1 ) {
        reds->listen_socket = spice_listen_socket_fd;
        reds->listen_watch = core->watch_add(reds->listen_socket,
                                             SPICE_WATCH_EVENT_READ,
                                             reds_accept, NULL);
        if (reds->listen_watch == NULL) {
            spice_warning("set fd handle failed");
            return -1;
        }
    }
    return 0;
}

static int load_dh_params(SSL_CTX *ctx, char *file)
{
    DH *ret = 0;
    BIO *bio;

    if ((bio = BIO_new_file(file, "r")) == NULL) {
        spice_warning("Could not open DH file");
        return -1;
    }

    ret = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (ret == 0) {
        spice_warning("Could not read DH params");
        return -1;
    }


    if (SSL_CTX_set_tmp_dh(ctx, ret) < 0) {
        spice_warning("Could not set DH params");
        return -1;
    }

    return 0;
}

/*The password code is not thread safe*/
static int ssl_password_cb(char *buf, int size, int flags, void *userdata)
{
    char *pass = ssl_parameters.keyfile_password;
    if (size < strlen(pass) + 1) {
        return (0);
    }

    strcpy(buf, pass);
    return (strlen(pass));
}

static unsigned long pthreads_thread_id(void)
{
    unsigned long ret;

    ret = (unsigned long)pthread_self();
    return (ret);
}

static void pthreads_locking_callback(int mode, int type, const char *file, int line)
{
    if (mode & CRYPTO_LOCK) {
        pthread_mutex_lock(&(lock_cs[type]));
        lock_count[type]++;
    } else {
        pthread_mutex_unlock(&(lock_cs[type]));
    }
}

static void openssl_thread_setup(void)
{
    int i;

    lock_cs = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
    lock_count = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));

    for (i = 0; i < CRYPTO_num_locks(); i++) {
        lock_count[i] = 0;
        pthread_mutex_init(&(lock_cs[i]), NULL);
    }

    CRYPTO_set_id_callback(pthreads_thread_id);
    CRYPTO_set_locking_callback(pthreads_locking_callback);
}

static int reds_init_ssl(void)
{
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
    const SSL_METHOD *ssl_method;
#else
    SSL_METHOD *ssl_method;
#endif
    int return_code;
    long ssl_options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;

    /* Global system initialization*/
    SSL_library_init();
    SSL_load_error_strings();

    /* Create our context*/
    ssl_method = TLSv1_method();
    reds->ctx = SSL_CTX_new(ssl_method);
    if (!reds->ctx) {
        spice_warning("Could not allocate new SSL context");
        return -1;
    }

    /* Limit connection to TLSv1 only */
#ifdef SSL_OP_NO_COMPRESSION
    ssl_options |= SSL_OP_NO_COMPRESSION;
#endif
    SSL_CTX_set_options(reds->ctx, ssl_options);

    /* Load our keys and certificates*/
    return_code = SSL_CTX_use_certificate_chain_file(reds->ctx, ssl_parameters.certs_file);
    if (return_code == 1) {
        spice_info("Loaded certificates from %s", ssl_parameters.certs_file);
    } else {
        spice_warning("Could not load certificates from %s", ssl_parameters.certs_file);
        return -1;
    }

    SSL_CTX_set_default_passwd_cb(reds->ctx, ssl_password_cb);

    return_code = SSL_CTX_use_PrivateKey_file(reds->ctx, ssl_parameters.private_key_file,
                                              SSL_FILETYPE_PEM);
    if (return_code == 1) {
        spice_info("Using private key from %s", ssl_parameters.private_key_file);
    } else {
        spice_warning("Could not use private key file");
        return -1;
    }

    /* Load the CAs we trust*/
    return_code = SSL_CTX_load_verify_locations(reds->ctx, ssl_parameters.ca_certificate_file, 0);
    if (return_code == 1) {
        spice_info("Loaded CA certificates from %s", ssl_parameters.ca_certificate_file);
    } else {
        spice_warning("Could not use CA file %s", ssl_parameters.ca_certificate_file);
        return -1;
    }

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
    SSL_CTX_set_verify_depth(reds->ctx, 1);
#endif

    if (strlen(ssl_parameters.dh_key_file) > 0) {
        if (load_dh_params(reds->ctx, ssl_parameters.dh_key_file) < 0) {
            return -1;
        }
    }

    SSL_CTX_set_session_id_context(reds->ctx, (const unsigned char *)"SPICE", 5);
    if (strlen(ssl_parameters.ciphersuite) > 0) {
        if (!SSL_CTX_set_cipher_list(reds->ctx, ssl_parameters.ciphersuite)) {
            return -1;
        }
    }

    openssl_thread_setup();

#ifndef SSL_OP_NO_COMPRESSION
    STACK *cmp_stack = SSL_COMP_get_compression_methods();
    sk_zero(cmp_stack);
#endif

    return 0;
}

static void reds_exit(void)
{
    if (reds->main_channel) {
        main_channel_close(reds->main_channel);
    }
#ifdef RED_STATISTICS
    shm_unlink(reds->stat_shm_name);
    free(reds->stat_shm_name);
#endif
}

enum {
    SPICE_OPTION_INVALID,
    SPICE_OPTION_PORT,
    SPICE_OPTION_SPORT,
    SPICE_OPTION_HOST,
    SPICE_OPTION_IMAGE_COMPRESSION,
    SPICE_OPTION_PASSWORD,
    SPICE_OPTION_DISABLE_TICKET,
    SPICE_OPTION_RENDERER,
    SPICE_OPTION_SSLKEY,
    SPICE_OPTION_SSLCERTS,
    SPICE_OPTION_SSLCAFILE,
    SPICE_OPTION_SSLDHFILE,
    SPICE_OPTION_SSLPASSWORD,
    SPICE_OPTION_SSLCIPHERSUITE,
    SPICE_SECURED_CHANNELS,
    SPICE_UNSECURED_CHANNELS,
    SPICE_OPTION_STREAMING_VIDEO,
    SPICE_OPTION_AGENT_MOUSE,
    SPICE_OPTION_PLAYBACK_COMPRESSION,
};

typedef struct OptionsMap {
    const char *name;
    int val;
} OptionsMap;

enum {
    SPICE_TICKET_OPTION_INVALID,
    SPICE_TICKET_OPTION_EXPIRATION,
    SPICE_TICKET_OPTION_CONNECTED,
};

static inline void on_activating_ticketing(void)
{
    if (!ticketing_enabled && reds_main_channel_connected()) {
        spice_warning("disconnecting");
        reds_disconnect();
    }
}

static void set_image_compression(spice_image_compression_t val)
{
    if (val == image_compression) {
        return;
    }
    image_compression = val;
    red_dispatcher_on_ic_change();
}

static void set_one_channel_security(int id, uint32_t security)
{
    ChannelSecurityOptions *security_options;

    if ((security_options = find_channel_security(id))) {
        security_options->options = security;
        return;
    }
    security_options = spice_new(ChannelSecurityOptions, 1);
    security_options->channel_id = id;
    security_options->options = security;
    security_options->next = channels_security;
    channels_security = security_options;
}

#define REDS_SAVE_VERSION 1

typedef struct RedsMigSpiceMessage {
    uint32_t connection_id;
} RedsMigSpiceMessage;

typedef struct RedsMigCertPubKeyInfo {
    uint16_t type;
    uint32_t len;
} RedsMigCertPubKeyInfo;

static void reds_mig_release(void)
{
    if (reds->mig_spice) {
        free(reds->mig_spice->cert_subject);
        free(reds->mig_spice->host);
        free(reds->mig_spice);
        reds->mig_spice = NULL;
    }
}

static void reds_mig_started(void)
{
    spice_info(NULL);
    spice_assert(reds->mig_spice);

    reds->mig_inprogress = TRUE;
    reds->mig_wait_connect = TRUE;
    core->timer_start(reds->mig_timer, MIGRATE_TIMEOUT);
}

static void reds_mig_fill_wait_disconnect(void)
{
    RingItem *client_item;

    spice_assert(reds->num_clients > 0);
    /* tracking the clients, in order to ignore disconnection
     * of clients that got connected to the src after migration completion.*/
    RING_FOREACH(client_item, &reds->clients) {
        RedClient *client = SPICE_CONTAINEROF(client_item, RedClient, link);
        RedsMigWaitDisconnectClient *wait_client;

        wait_client = spice_new0(RedsMigWaitDisconnectClient, 1);
        wait_client->client = client;
        ring_add(&reds->mig_wait_disconnect_clients, &wait_client->link);
    }
    reds->mig_wait_disconnect = TRUE;
    core->timer_start(reds->mig_timer, MIGRATE_TIMEOUT);
}

static void reds_mig_cleanup_wait_disconnect(void)
{
    RingItem *wait_client_item;

    while ((wait_client_item = ring_get_tail(&reds->mig_wait_disconnect_clients))) {
        RedsMigWaitDisconnectClient *wait_client;

        wait_client = SPICE_CONTAINEROF(wait_client_item, RedsMigWaitDisconnectClient, link);
        ring_remove(wait_client_item);
        free(wait_client);
    }
    reds->mig_wait_disconnect = FALSE;
}

static void reds_mig_remove_wait_disconnect_client(RedClient *client)
{
    RingItem *wait_client_item;

    RING_FOREACH(wait_client_item, &reds->mig_wait_disconnect_clients) {
        RedsMigWaitDisconnectClient *wait_client;

        wait_client = SPICE_CONTAINEROF(wait_client_item, RedsMigWaitDisconnectClient, link);
        if (wait_client->client == client) {
            ring_remove(wait_client_item);
            free(wait_client);
            if (ring_is_empty(&reds->mig_wait_disconnect_clients)) {
                reds_mig_cleanup();
            }
            return;
        }
    }
    spice_warning("client not found %p", client);
}

static void reds_migrate_channels_seamless(void)
{
    RedClient *client;

    /* seamless migration is supported for only one client for now */
    client = reds_get_client();
    red_client_migrate(client);
}

static void reds_mig_finished(int completed)
{
    spice_info(NULL);

    reds->mig_inprogress = TRUE;

    if (reds->src_do_seamless_migrate && completed) {
        reds_migrate_channels_seamless();
    } else {
        main_channel_migrate_src_complete(reds->main_channel, completed);
    }

    if (completed) {
        reds_mig_fill_wait_disconnect();
    } else {
        reds_mig_cleanup();
    }
    reds_mig_release();
}

static void reds_mig_switch(void)
{
    if (!reds->mig_spice) {
        spice_warning("reds_mig_switch called without migrate_info set");
        return;
    }
    main_channel_migrate_switch(reds->main_channel, reds->mig_spice);
    reds_mig_release();
}

static void migrate_timeout(void *opaque)
{
    spice_info(NULL);
    spice_assert(reds->mig_wait_connect || reds->mig_wait_disconnect);
    if (reds->mig_wait_connect) {
        /* we will fall back to the switch host scheme when migration completes */
        main_channel_migrate_cancel_wait(reds->main_channel);
        /* in case part of the client haven't yet completed the previous migration, disconnect them */
        reds_mig_target_client_disconnect_all();
        reds_mig_cleanup();
    } else {
        reds_mig_disconnect();
    }
}

uint32_t reds_get_mm_time(void)
{
    struct timespec time_space;
    clock_gettime(CLOCK_MONOTONIC, &time_space);
    return time_space.tv_sec * 1000 + time_space.tv_nsec / 1000 / 1000;
}

void reds_update_mm_timer(uint32_t mm_time)
{
    red_dispatcher_set_mm_time(mm_time);
}

void reds_enable_mm_timer(void)
{
    core->timer_start(reds->mm_timer, MM_TIMER_GRANULARITY_MS);
    reds->mm_timer_enabled = TRUE;
    reds->mm_time_latency = MM_TIME_DELTA;
    reds_send_mm_time();
}

void reds_disable_mm_timer(void)
{
    core->timer_cancel(reds->mm_timer);
    reds->mm_timer_enabled = FALSE;
}

static void mm_timer_proc(void *opaque)
{
    red_dispatcher_set_mm_time(reds_get_mm_time());
    core->timer_start(reds->mm_timer, MM_TIMER_GRANULARITY_MS);
}

static SpiceCharDeviceState *attach_to_red_agent(SpiceCharDeviceInstance *sin)
{
    VDIPortState *state = &reds->agent_state;
    SpiceCharDeviceInterface *sif;
    SpiceCharDeviceCallbacks char_dev_state_cbs;

    if (!state->base) {
        char_dev_state_cbs.read_one_msg_from_device = vdi_port_read_one_msg_from_device;
        char_dev_state_cbs.ref_msg_to_client = vdi_port_ref_msg_to_client;
        char_dev_state_cbs.unref_msg_to_client = vdi_port_unref_msg_to_client;
        char_dev_state_cbs.send_msg_to_client = vdi_port_send_msg_to_client;
        char_dev_state_cbs.send_tokens_to_client = vdi_port_send_tokens_to_client;
        char_dev_state_cbs.remove_client = vdi_port_remove_client;
        char_dev_state_cbs.on_free_self_token = vdi_port_on_free_self_token;

        state->base = spice_char_device_state_create(sin,
                                                     REDS_TOKENS_TO_SEND,
                                                     REDS_NUM_INTERNAL_AGENT_MESSAGES,
                                                     &char_dev_state_cbs,
                                                     NULL);
    } else {
        spice_char_device_state_reset_dev_instance(state->base, sin);
    }

    vdagent = sin;
    reds_update_mouse_mode();

    sif = SPICE_CONTAINEROF(vdagent->base.sif, SpiceCharDeviceInterface, base);
    if (sif->state) {
        sif->state(vdagent, 1);
    }

    if (!reds_main_channel_connected()) {
        return state->base;
    }

    state->read_filter.discard_all = FALSE;
    reds->agent_state.plug_generation++;

    if (reds->agent_state.mig_data) {
        spice_assert(reds->agent_state.plug_generation == 1);
        reds_agent_state_restore(reds->agent_state.mig_data);
        free(reds->agent_state.mig_data);
        reds->agent_state.mig_data = NULL;
    } else if (!red_channel_waits_for_migrate_data(&reds->main_channel->base)) {
        /* we will assoicate the client with the char device, upon reds_on_main_agent_start,
         * in response to MSGC_AGENT_START */
        main_channel_push_agent_connected(reds->main_channel);
    } else {
       spice_debug("waiting for migration data");
        if (!spice_char_device_client_exists(reds->agent_state.base, reds_get_client())) {
            int client_added;

            client_added = spice_char_device_client_add(reds->agent_state.base,
                                                        reds_get_client(),
                                                        TRUE, /* flow control */
                                                        REDS_VDI_PORT_NUM_RECEIVE_BUFFS,
                                                        REDS_AGENT_WINDOW_SIZE,
                                                        ~0,
                                                        TRUE);

            if (!client_added) {
                spice_warning("failed to add client to agent");
                reds_disconnect();
            }

        }
    }
    return state->base;
}

SPICE_GNUC_VISIBLE void spice_server_char_device_wakeup(SpiceCharDeviceInstance* sin)
{
    if (!sin->st) {
        spice_warning("no SpiceCharDeviceState attached to instance %p", sin);
        return;
    }
    spice_char_device_wakeup(sin->st);
}

#define SUBTYPE_VDAGENT "vdagent"
#define SUBTYPE_SMARTCARD "smartcard"
#define SUBTYPE_USBREDIR "usbredir"
#define SUBTYPE_PORT "port"

const char *spice_server_char_device_recognized_subtypes_list[] = {
    SUBTYPE_VDAGENT,
#ifdef USE_SMARTCARD
    SUBTYPE_SMARTCARD,
#endif
    SUBTYPE_USBREDIR,
    NULL,
};

SPICE_GNUC_VISIBLE const char** spice_server_char_device_recognized_subtypes(void)
{
    return spice_server_char_device_recognized_subtypes_list;
}

static void reds_char_device_add_state(SpiceCharDeviceState *st)
{
    SpiceCharDeviceStateItem *item = spice_new0(SpiceCharDeviceStateItem, 1);

    item->st = st;

    ring_add(&reds->char_devs_states, &item->link);
}

static void reds_char_device_remove_state(SpiceCharDeviceState *st)
{
    RingItem *item;

    RING_FOREACH(item, &reds->char_devs_states) {
        SpiceCharDeviceStateItem *st_item;

        st_item = SPICE_CONTAINEROF(item, SpiceCharDeviceStateItem, link);
        if (st_item->st == st) {
            ring_remove(item);
            free(st_item);
            return;
        }
    }
    spice_error("char dev state not found %p", st);
}

void reds_on_char_device_state_destroy(SpiceCharDeviceState *dev)
{
    reds_char_device_remove_state(dev);
}

static int spice_server_char_device_add_interface(SpiceServer *s,
                                           SpiceBaseInstance *sin)
{
    SpiceCharDeviceInstance* char_device =
            SPICE_CONTAINEROF(sin, SpiceCharDeviceInstance, base);
    SpiceCharDeviceState *dev_state = NULL;

    spice_assert(s == reds);

    spice_info("CHAR_DEVICE %s", char_device->subtype);
    if (strcmp(char_device->subtype, SUBTYPE_VDAGENT) == 0) {
        if (vdagent) {
            spice_warning("vdagent already attached");
            return -1;
        }
        dev_state = attach_to_red_agent(char_device);
    }
#ifdef USE_SMARTCARD
    else if (strcmp(char_device->subtype, SUBTYPE_SMARTCARD) == 0) {
        if (!(dev_state = smartcard_device_connect(char_device))) {
            return -1;
        }
    }
#endif
    else if (strcmp(char_device->subtype, SUBTYPE_USBREDIR) == 0) {
        dev_state = spicevmc_device_connect(char_device, SPICE_CHANNEL_USBREDIR);
    }
    else if (strcmp(char_device->subtype, SUBTYPE_PORT) == 0) {
        dev_state = spicevmc_device_connect(char_device, SPICE_CHANNEL_PORT);
    }

    if (dev_state) {
        spice_assert(char_device->st);
        /* setting the char_device state to "started" for backward compatibily with
         * qemu releases that don't call spice api for start/stop (not implemented yet) */
        if (reds->vm_running) {
            spice_char_device_start(char_device->st);
        }
        reds_char_device_add_state(char_device->st);
    } else {
        spice_warning("failed to create device state for %s", char_device->subtype);
        return -1;
    }
    return 0;
}

static void spice_server_char_device_remove_interface(SpiceBaseInstance *sin)
{
    SpiceCharDeviceInstance* char_device =
            SPICE_CONTAINEROF(sin, SpiceCharDeviceInstance, base);

    spice_info("remove CHAR_DEVICE %s", char_device->subtype);
    if (strcmp(char_device->subtype, SUBTYPE_VDAGENT) == 0) {
        if (vdagent) {
            reds_agent_remove();
        }
    }
#ifdef USE_SMARTCARD
    else if (strcmp(char_device->subtype, SUBTYPE_SMARTCARD) == 0) {
        smartcard_device_disconnect(char_device);
    }
#endif
    else if (strcmp(char_device->subtype, SUBTYPE_USBREDIR) == 0 ||
             strcmp(char_device->subtype, SUBTYPE_PORT) == 0) {
        spicevmc_device_disconnect(char_device);
    } else {
        spice_warning("failed to remove char device %s", char_device->subtype);
    }

    char_device->st = NULL;
}

SPICE_GNUC_VISIBLE int spice_server_add_interface(SpiceServer *s,
                                                  SpiceBaseInstance *sin)
{
    const SpiceBaseInterface *interface = sin->sif;

    spice_assert(reds == s);

    if (strcmp(interface->type, SPICE_INTERFACE_KEYBOARD) == 0) {
        spice_info("SPICE_INTERFACE_KEYBOARD");
        if (interface->major_version != SPICE_INTERFACE_KEYBOARD_MAJOR ||
            interface->minor_version > SPICE_INTERFACE_KEYBOARD_MINOR) {
            spice_warning("unsupported keyboard interface");
            return -1;
        }
        if (inputs_set_keyboard(SPICE_CONTAINEROF(sin, SpiceKbdInstance, base)) != 0) {
            return -1;
        }
    } else if (strcmp(interface->type, SPICE_INTERFACE_MOUSE) == 0) {
        spice_info("SPICE_INTERFACE_MOUSE");
        if (interface->major_version != SPICE_INTERFACE_MOUSE_MAJOR ||
            interface->minor_version > SPICE_INTERFACE_MOUSE_MINOR) {
            spice_warning("unsupported mouse interface");
            return -1;
        }
        if (inputs_set_mouse(SPICE_CONTAINEROF(sin, SpiceMouseInstance, base)) != 0) {
            return -1;
        }
    } else if (strcmp(interface->type, SPICE_INTERFACE_QXL) == 0) {
        QXLInstance *qxl;

        spice_info("SPICE_INTERFACE_QXL");
        if (interface->major_version != SPICE_INTERFACE_QXL_MAJOR ||
            interface->minor_version > SPICE_INTERFACE_QXL_MINOR) {
            spice_warning("unsupported qxl interface");
            return -1;
        }

        qxl = SPICE_CONTAINEROF(sin, QXLInstance, base);
        qxl->st = spice_new0(QXLState, 1);
        qxl->st->qif = SPICE_CONTAINEROF(interface, QXLInterface, base);
        qxl->st->dispatcher = red_dispatcher_init(qxl);

    } else if (strcmp(interface->type, SPICE_INTERFACE_TABLET) == 0) {
        spice_info("SPICE_INTERFACE_TABLET");
        if (interface->major_version != SPICE_INTERFACE_TABLET_MAJOR ||
            interface->minor_version > SPICE_INTERFACE_TABLET_MINOR) {
            spice_warning("unsupported tablet interface");
            return -1;
        }
        if (inputs_set_tablet(SPICE_CONTAINEROF(sin, SpiceTabletInstance, base)) != 0) {
            return -1;
        }
        reds_update_mouse_mode();
        if (reds->is_client_mouse_allowed) {
            inputs_set_tablet_logical_size(reds->monitor_mode.x_res, reds->monitor_mode.y_res);
        }

    } else if (strcmp(interface->type, SPICE_INTERFACE_PLAYBACK) == 0) {
        spice_info("SPICE_INTERFACE_PLAYBACK");
        if (interface->major_version != SPICE_INTERFACE_PLAYBACK_MAJOR ||
            interface->minor_version > SPICE_INTERFACE_PLAYBACK_MINOR) {
            spice_warning("unsupported playback interface");
            return -1;
        }
        snd_attach_playback(SPICE_CONTAINEROF(sin, SpicePlaybackInstance, base));

    } else if (strcmp(interface->type, SPICE_INTERFACE_RECORD) == 0) {
        spice_info("SPICE_INTERFACE_RECORD");
        if (interface->major_version != SPICE_INTERFACE_RECORD_MAJOR ||
            interface->minor_version > SPICE_INTERFACE_RECORD_MINOR) {
            spice_warning("unsupported record interface");
            return -1;
        }
        snd_attach_record(SPICE_CONTAINEROF(sin, SpiceRecordInstance, base));

    } else if (strcmp(interface->type, SPICE_INTERFACE_CHAR_DEVICE) == 0) {
        if (interface->major_version != SPICE_INTERFACE_CHAR_DEVICE_MAJOR ||
            interface->minor_version > SPICE_INTERFACE_CHAR_DEVICE_MINOR) {
            spice_warning("unsupported char device interface");
            return -1;
        }
        spice_server_char_device_add_interface(s, sin);

    } else if (strcmp(interface->type, SPICE_INTERFACE_NET_WIRE) == 0) {
#ifdef USE_TUNNEL
        SpiceNetWireInstance *net;
        spice_info("SPICE_INTERFACE_NET_WIRE");
        if (red_tunnel) {
            spice_warning("net wire already attached");
            return -1;
        }
        if (interface->major_version != SPICE_INTERFACE_NET_WIRE_MAJOR ||
            interface->minor_version > SPICE_INTERFACE_NET_WIRE_MINOR) {
            spice_warning("unsupported net wire interface");
            return -1;
        }
        net = SPICE_CONTAINEROF(sin, SpiceNetWireInstance, base);
        net->st = spice_new0(SpiceNetWireState, 1);
        red_tunnel = red_tunnel_attach(core, net);
#else
        spice_warning("unsupported net wire interface");
        return -1;
#endif
    } else if (strcmp(interface->type, SPICE_INTERFACE_MIGRATION) == 0) {
        spice_info("SPICE_INTERFACE_MIGRATION");
        if (migration_interface) {
            spice_warning("already have migration");
            return -1;
        }

        if (interface->major_version != SPICE_INTERFACE_MIGRATION_MAJOR ||
            interface->minor_version > SPICE_INTERFACE_MIGRATION_MINOR) {
            spice_warning("unsupported migration interface");
            return -1;
        }
        migration_interface = SPICE_CONTAINEROF(sin, SpiceMigrateInstance, base);
        migration_interface->st = spice_new0(SpiceMigrateState, 1);
    }

    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_remove_interface(SpiceBaseInstance *sin)
{
    const SpiceBaseInterface *interface = sin->sif;

    if (strcmp(interface->type, SPICE_INTERFACE_TABLET) == 0) {
        spice_info("remove SPICE_INTERFACE_TABLET");
        inputs_detach_tablet(SPICE_CONTAINEROF(sin, SpiceTabletInstance, base));
        reds_update_mouse_mode();
    } else if (strcmp(interface->type, SPICE_INTERFACE_PLAYBACK) == 0) {
        spice_info("remove SPICE_INTERFACE_PLAYBACK");
        snd_detach_playback(SPICE_CONTAINEROF(sin, SpicePlaybackInstance, base));
    } else if (strcmp(interface->type, SPICE_INTERFACE_RECORD) == 0) {
        spice_info("remove SPICE_INTERFACE_RECORD");
        snd_detach_record(SPICE_CONTAINEROF(sin, SpiceRecordInstance, base));
    } else if (strcmp(interface->type, SPICE_INTERFACE_CHAR_DEVICE) == 0) {
        spice_server_char_device_remove_interface(sin);
    } else {
        spice_warning("VD_INTERFACE_REMOVING unsupported");
        return -1;
    }

    return 0;
}

static void init_vd_agent_resources(void)
{
    VDIPortState *state = &reds->agent_state;
    int i;

    ring_init(&state->read_bufs);
    agent_msg_filter_init(&state->write_filter, agent_copypaste,
                          agent_file_xfer, TRUE);
    agent_msg_filter_init(&state->read_filter, agent_copypaste,
                          agent_file_xfer, TRUE);

    state->read_state = VDI_PORT_READ_STATE_READ_HEADER;
    state->recive_pos = (uint8_t *)&state->vdi_chunk_header;
    state->recive_len = sizeof(state->vdi_chunk_header);

    for (i = 0; i < REDS_VDI_PORT_NUM_RECEIVE_BUFFS; i++) {
        VDIReadBuf *buf = spice_new0(VDIReadBuf, 1);
        ring_item_init(&buf->link);
        ring_add(&reds->agent_state.read_bufs, &buf->link);
    }
}

const char *version_string = VERSION;

static int do_spice_init(SpiceCoreInterface *core_interface)
{
    spice_info("starting %s", version_string);

    if (core_interface->base.major_version != SPICE_INTERFACE_CORE_MAJOR) {
        spice_warning("bad core interface version");
        goto err;
    }
    core = core_interface;
    reds->listen_socket = -1;
    reds->secure_listen_socket = -1;
    init_vd_agent_resources();
    ring_init(&reds->clients);
    reds->num_clients = 0;
    main_dispatcher_init(core);
    ring_init(&reds->channels);
    ring_init(&reds->mig_target_clients);
    ring_init(&reds->char_devs_states);
    ring_init(&reds->mig_wait_disconnect_clients);
    reds->vm_running = TRUE; /* for backward compatibility */

    if (!(reds->mig_timer = core->timer_add(migrate_timeout, NULL))) {
        spice_error("migration timer create failed");
    }

#ifdef RED_STATISTICS
    int shm_name_len;
    int fd;

    shm_name_len = strlen(SPICE_STAT_SHM_NAME) + 20;
    reds->stat_shm_name = (char *)spice_malloc(shm_name_len);
    snprintf(reds->stat_shm_name, shm_name_len, SPICE_STAT_SHM_NAME, getpid());
    if ((fd = shm_open(reds->stat_shm_name, O_CREAT | O_RDWR, 0444)) == -1) {
        spice_error("statistics shm_open failed, %s", strerror(errno));
    }
    if (ftruncate(fd, REDS_STAT_SHM_SIZE) == -1) {
        spice_error("statistics ftruncate failed, %s", strerror(errno));
    }
    reds->stat = (SpiceStat *)mmap(NULL, REDS_STAT_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (reds->stat == (SpiceStat *)MAP_FAILED) {
        spice_error("statistics mmap failed, %s", strerror(errno));
    }
    memset(reds->stat, 0, REDS_STAT_SHM_SIZE);
    reds->stat->magic = SPICE_STAT_MAGIC;
    reds->stat->version = SPICE_STAT_VERSION;
    reds->stat->root_index = INVALID_STAT_REF;
    if (pthread_mutex_init(&reds->stat_lock, NULL)) {
        spice_error("mutex init failed");
    }
#endif

    if (!(reds->mm_timer = core->timer_add(mm_timer_proc, NULL))) {
        spice_error("mm timer create failed");
    }
    reds_enable_mm_timer();

    if (reds_init_net() < 0) {
        goto err;
    }
    if (reds->secure_listen_socket != -1) {
        if (reds_init_ssl() < 0) {
            goto err;
        }
    }
#if HAVE_SASL
    int saslerr;
    if ((saslerr = sasl_server_init(NULL, sasl_appname ?
                                    sasl_appname : "spice")) != SASL_OK) {
        spice_error("Failed to initialize SASL auth %s",
                  sasl_errstring(saslerr, NULL, NULL));
        goto err;
    }
#endif

    reds->main_channel = main_channel_init();
    inputs_init();

    reds->mouse_mode = SPICE_MOUSE_MODE_SERVER;

    reds_client_monitors_config_cleanup();

    reds->allow_multiple_clients = getenv(SPICE_DEBUG_ALLOW_MC_ENV) != NULL;
    if (reds->allow_multiple_clients) {
        spice_warning("spice: allowing multiple client connections (crashy)");
    }
    atexit(reds_exit);
    return 0;

err:
    return -1;
}

/* new interface */
SPICE_GNUC_VISIBLE SpiceServer *spice_server_new(void)
{
    /* we can't handle multiple instances (yet) */
    spice_assert(reds == NULL);

    reds = spice_new0(RedsState, 1);
    return reds;
}

SPICE_GNUC_VISIBLE int spice_server_init(SpiceServer *s, SpiceCoreInterface *core)
{
    int ret;

    spice_assert(reds == s);
    ret = do_spice_init(core);
    if (default_renderer) {
        red_dispatcher_add_renderer(default_renderer);
    }
    return ret;
}

SPICE_GNUC_VISIBLE void spice_server_destroy(SpiceServer *s)
{
    spice_assert(reds == s);
    reds_exit();
}

SPICE_GNUC_VISIBLE spice_compat_version_t spice_get_current_compat_version(void)
{
    return SPICE_COMPAT_VERSION_CURRENT;
}

SPICE_GNUC_VISIBLE int spice_server_set_compat_version(SpiceServer *s,
                                                       spice_compat_version_t version)
{
    if (version < SPICE_COMPAT_VERSION_0_6) {
        /* We don't support 0.4 compat mode atm */
        return -1;
    }

    if (version > SPICE_COMPAT_VERSION_CURRENT) {
        /* Not compatible with future versions */
        return -1;
    }
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_port(SpiceServer *s, int port)
{
    spice_assert(reds == s);
    if (port < 0 || port > 0xffff) {
        return -1;
    }
    spice_port = port;
    return 0;
}

SPICE_GNUC_VISIBLE void spice_server_set_addr(SpiceServer *s, const char *addr, int flags)
{
    spice_assert(reds == s);
    g_strlcpy(spice_addr, addr, sizeof(spice_addr));
    if (flags & SPICE_ADDR_FLAG_IPV4_ONLY) {
        spice_family = PF_INET;
    }
    if (flags & SPICE_ADDR_FLAG_IPV6_ONLY) {
        spice_family = PF_INET6;
    }
}

SPICE_GNUC_VISIBLE int spice_server_set_listen_socket_fd(SpiceServer *s, int listen_fd)
{
    spice_assert(reds == s);
    spice_listen_socket_fd = listen_fd;
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_exit_on_disconnect(SpiceServer *s, int flag)
{
    spice_assert(reds == s);
    exit_on_disconnect = !!flag;
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_noauth(SpiceServer *s)
{
    spice_assert(reds == s);
    memset(taTicket.password, 0, sizeof(taTicket.password));
    ticketing_enabled = 0;
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_sasl(SpiceServer *s, int enabled)
{
    spice_assert(reds == s);
#if HAVE_SASL
    sasl_enabled = enabled;
    return 0;
#else
    return -1;
#endif
}

SPICE_GNUC_VISIBLE int spice_server_set_sasl_appname(SpiceServer *s, const char *appname)
{
    spice_assert(reds == s);
#if HAVE_SASL
    free(sasl_appname);
    sasl_appname = spice_strdup(appname);
    return 0;
#else
    return -1;
#endif
}

SPICE_GNUC_VISIBLE void spice_server_set_name(SpiceServer *s, const char *name)
{
    free(spice_name);
    spice_name = spice_strdup(name);
}

SPICE_GNUC_VISIBLE void spice_server_set_uuid(SpiceServer *s, const uint8_t uuid[16])
{
    memcpy(spice_uuid, uuid, sizeof(spice_uuid));
    spice_uuid_is_set = TRUE;
}

SPICE_GNUC_VISIBLE int spice_server_set_ticket(SpiceServer *s,
                                               const char *passwd, int lifetime,
                                               int fail_if_connected,
                                               int disconnect_if_connected)
{
    spice_assert(reds == s);

    if (reds_main_channel_connected()) {
        if (fail_if_connected) {
            return -1;
        }
        if (disconnect_if_connected) {
            reds_disconnect();
        }
    }

    on_activating_ticketing();
    ticketing_enabled = 1;
    if (lifetime == 0) {
        taTicket.expiration_time = INT_MAX;
    } else {
        time_t now = time(NULL);
        taTicket.expiration_time = now + lifetime;
    }
    if (passwd != NULL) {
        g_strlcpy(taTicket.password, passwd, sizeof(taTicket.password));
    } else {
        memset(taTicket.password, 0, sizeof(taTicket.password));
        taTicket.expiration_time = 0;
    }
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_tls(SpiceServer *s, int port,
                                            const char *ca_cert_file, const char *certs_file,
                                            const char *private_key_file, const char *key_passwd,
                                            const char *dh_key_file, const char *ciphersuite)
{
    spice_assert(reds == s);
    if (port == 0 || ca_cert_file == NULL || certs_file == NULL ||
        private_key_file == NULL) {
        return -1;
    }
    if (port < 0 || port > 0xffff) {
        return -1;
    }
    memset(&ssl_parameters, 0, sizeof(ssl_parameters));

    spice_secure_port = port;
    g_strlcpy(ssl_parameters.ca_certificate_file, ca_cert_file,
              sizeof(ssl_parameters.ca_certificate_file));
    g_strlcpy(ssl_parameters.certs_file, certs_file,
              sizeof(ssl_parameters.certs_file));
    g_strlcpy(ssl_parameters.private_key_file, private_key_file,
              sizeof(ssl_parameters.private_key_file));

    if (key_passwd) {
        g_strlcpy(ssl_parameters.keyfile_password, key_passwd,
                  sizeof(ssl_parameters.keyfile_password));
    }
    if (ciphersuite) {
        g_strlcpy(ssl_parameters.ciphersuite, ciphersuite,
                  sizeof(ssl_parameters.ciphersuite));
    }
    if (dh_key_file) {
        g_strlcpy(ssl_parameters.dh_key_file, dh_key_file,
                  sizeof(ssl_parameters.dh_key_file));
    }
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_image_compression(SpiceServer *s,
                                                          spice_image_compression_t comp)
{
    spice_assert(reds == s);
    set_image_compression(comp);
    return 0;
}

SPICE_GNUC_VISIBLE spice_image_compression_t spice_server_get_image_compression(SpiceServer *s)
{
    spice_assert(reds == s);
    return image_compression;
}

SPICE_GNUC_VISIBLE int spice_server_set_jpeg_compression(SpiceServer *s, spice_wan_compression_t comp)
{
    spice_assert(reds == s);
    if (comp == SPICE_WAN_COMPRESSION_INVALID) {
        spice_error("invalid jpeg state");
        return -1;
    }
    // todo: support dynamically changing the state
    jpeg_state = comp;
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_zlib_glz_compression(SpiceServer *s, spice_wan_compression_t comp)
{
    spice_assert(reds == s);
    if (comp == SPICE_WAN_COMPRESSION_INVALID) {
        spice_error("invalid zlib_glz state");
        return -1;
    }
    // todo: support dynamically changing the state
    zlib_glz_state = comp;
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_channel_security(SpiceServer *s, const char *channel, int security)
{
    static const char *names[] = {
        [ SPICE_CHANNEL_MAIN     ] = "main",
        [ SPICE_CHANNEL_DISPLAY  ] = "display",
        [ SPICE_CHANNEL_INPUTS   ] = "inputs",
        [ SPICE_CHANNEL_CURSOR   ] = "cursor",
        [ SPICE_CHANNEL_PLAYBACK ] = "playback",
        [ SPICE_CHANNEL_RECORD   ] = "record",
#ifdef USE_TUNNEL
        [ SPICE_CHANNEL_TUNNEL   ] = "tunnel",
#endif
#ifdef USE_SMARTCARD
        [ SPICE_CHANNEL_SMARTCARD] = "smartcard",
#endif
        [ SPICE_CHANNEL_USBREDIR ] = "usbredir",
    };
    int i;

    spice_assert(reds == s);

    if (channel == NULL) {
        default_channel_security = security;
        return 0;
    }
    for (i = 0; i < SPICE_N_ELEMENTS(names); i++) {
        if (names[i] && strcmp(names[i], channel) == 0) {
            set_one_channel_security(i, security);
            return 0;
        }
    }
    return -1;
}

SPICE_GNUC_VISIBLE int spice_server_get_sock_info(SpiceServer *s, struct sockaddr *sa, socklen_t *salen)
{
    spice_assert(reds == s);
    if (main_channel_getsockname(reds->main_channel, sa, salen) < 0) {
        return -1;
    }
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_get_peer_info(SpiceServer *s, struct sockaddr *sa, socklen_t *salen)
{
    spice_assert(reds == s);
    if (main_channel_getpeername(reds->main_channel, sa, salen) < 0) {
        return -1;
    }
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_is_server_mouse(SpiceServer *s)
{
    spice_assert(reds == s);
    return reds->mouse_mode == SPICE_MOUSE_MODE_SERVER;
}

SPICE_GNUC_VISIBLE int spice_server_add_renderer(SpiceServer *s, const char *name)
{
    spice_assert(reds == s);
    if (!red_dispatcher_add_renderer(name)) {
        return -1;
    }
    default_renderer = NULL;
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_kbd_leds(SpiceKbdInstance *sin, int leds)
{
    inputs_on_keyboard_leds_change(NULL, leds);
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_streaming_video(SpiceServer *s, int value)
{
    spice_assert(reds == s);
    if (value != SPICE_STREAM_VIDEO_OFF &&
        value != SPICE_STREAM_VIDEO_ALL &&
        value != SPICE_STREAM_VIDEO_FILTER)
        return -1;
    streaming_video = value;
    red_dispatcher_on_sv_change();
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_playback_compression(SpiceServer *s, int enable)
{
    spice_assert(reds == s);
    snd_set_playback_compression(enable);
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_agent_mouse(SpiceServer *s, int enable)
{
    spice_assert(reds == s);
    agent_mouse = enable;
    reds_update_mouse_mode();
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_agent_copypaste(SpiceServer *s, int enable)
{
    spice_assert(reds == s);
    agent_copypaste = enable;
    reds->agent_state.write_filter.copy_paste_enabled = agent_copypaste;
    reds->agent_state.read_filter.copy_paste_enabled = agent_copypaste;
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_set_agent_file_xfer(SpiceServer *s, int enable)
{
    spice_assert(reds == s);
    agent_file_xfer = enable;
    reds->agent_state.write_filter.file_xfer_enabled = agent_file_xfer;
    reds->agent_state.read_filter.file_xfer_enabled = agent_file_xfer;
    return 0;
}

/* returns FALSE if info is invalid */
static int reds_set_migration_dest_info(const char* dest,
                                        int port, int secure_port,
                                        const char* cert_subject)
{
    RedsMigSpice *spice_migration = NULL;

    reds_mig_release();
    if ((port == -1 && secure_port == -1) || !dest) {
        return FALSE;
    }

    spice_migration = spice_new0(RedsMigSpice, 1);
    spice_migration->port = port;
    spice_migration->sport = secure_port;
    spice_migration->host = spice_strdup(dest);
    if (cert_subject) {
        spice_migration->cert_subject = spice_strdup(cert_subject);
    }

    reds->mig_spice = spice_migration;

    return TRUE;
}

/* semi-seamless client migration */
SPICE_GNUC_VISIBLE int spice_server_migrate_connect(SpiceServer *s, const char* dest,
                                                    int port, int secure_port,
                                                    const char* cert_subject)
{
    SpiceMigrateInterface *sif;
    int try_seamless;

    spice_info(NULL);
    spice_assert(migration_interface);
    spice_assert(reds == s);

    if (reds->expect_migrate) {
        spice_info("consecutive calls without migration. Canceling previous call");
        main_channel_migrate_src_complete(reds->main_channel, FALSE);
    }

    sif = SPICE_CONTAINEROF(migration_interface->base.sif, SpiceMigrateInterface, base);

    if (!reds_set_migration_dest_info(dest, port, secure_port, cert_subject)) {
        sif->migrate_connect_complete(migration_interface);
        return -1;
    }

    reds->expect_migrate = TRUE;

    /*
     * seamless migration support was added to the client after the support in
     * agent_connect_tokens, so there shouldn't be contradicition - if
     * the client is capable of seamless migration, it is capbable of agent_connected_tokens.
     * The demand for agent_connected_tokens support is in order to assure that if migration
     * occured when the agent was not connected, the tokens state after migration will still
     * be valid (see reds_reset_vdp for more details).
     */
    try_seamless = reds->seamless_migration_enabled &&
                   red_channel_test_remote_cap(&reds->main_channel->base,
                   SPICE_MAIN_CAP_AGENT_CONNECTED_TOKENS);
    /* main channel will take care of clients that are still during migration (at target)*/
    if (main_channel_migrate_connect(reds->main_channel, reds->mig_spice,
                                     try_seamless)) {
        reds_mig_started();
    } else {
        if (reds->num_clients == 0) {
            reds_mig_release();
            spice_info("no client connected");
        }
        sif->migrate_connect_complete(migration_interface);
    }

    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_migrate_info(SpiceServer *s, const char* dest,
                                          int port, int secure_port,
                                          const char* cert_subject)
{
    spice_info(NULL);
    spice_assert(!migration_interface);
    spice_assert(reds == s);

    if (!reds_set_migration_dest_info(dest, port, secure_port, cert_subject)) {
        return -1;
    }
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_migrate_start(SpiceServer *s)
{
    spice_assert(reds == s);
    spice_info(NULL);
    if (!reds->mig_spice) {
        return -1;
    }
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_migrate_client_state(SpiceServer *s)
{
    spice_assert(reds == s);

    if (!reds_main_channel_connected()) {
        return SPICE_MIGRATE_CLIENT_NONE;
    } else if (reds->mig_wait_connect) {
        return SPICE_MIGRATE_CLIENT_WAITING;
    } else {
        return SPICE_MIGRATE_CLIENT_READY;
    }
    return 0;
}

SPICE_GNUC_VISIBLE int spice_server_migrate_end(SpiceServer *s, int completed)
{
    SpiceMigrateInterface *sif;
    int ret = 0;

    spice_info(NULL);

    spice_assert(migration_interface);
    spice_assert(reds == s);

    sif = SPICE_CONTAINEROF(migration_interface->base.sif, SpiceMigrateInterface, base);
    if (completed && !reds->expect_migrate && reds->num_clients) {
        spice_warning("spice_server_migrate_info was not called, disconnecting clients");
        reds_disconnect();
        ret = -1;
        goto complete;
    }

    reds->expect_migrate = FALSE;
    if (!reds_main_channel_connected()) {
        spice_info("no peer connected");
        goto complete;
    }
    reds_mig_finished(completed);
    return 0;
complete:
    if (sif->migrate_end_complete) {
        sif->migrate_end_complete(migration_interface);
    }
    return ret;
}

/* interface for switch-host migration */
SPICE_GNUC_VISIBLE int spice_server_migrate_switch(SpiceServer *s)
{
    spice_assert(reds == s);
    spice_info(NULL);
    if (!reds->num_clients) {
       return 0;
    }
    reds->expect_migrate = FALSE;
    reds_mig_switch();
    return 0;
}

SPICE_GNUC_VISIBLE void spice_server_vm_start(SpiceServer *s)
{
    RingItem *item;

    spice_assert(s == reds);
    reds->vm_running = TRUE;
    RING_FOREACH(item, &reds->char_devs_states) {
        SpiceCharDeviceStateItem *st_item;

        st_item = SPICE_CONTAINEROF(item, SpiceCharDeviceStateItem, link);
        spice_char_device_start(st_item->st);
    }
    red_dispatcher_on_vm_start();
}

SPICE_GNUC_VISIBLE void spice_server_vm_stop(SpiceServer *s)
{
    RingItem *item;

    spice_assert(s == reds);
    reds->vm_running = FALSE;
    RING_FOREACH(item, &reds->char_devs_states) {
        SpiceCharDeviceStateItem *st_item;

        st_item = SPICE_CONTAINEROF(item, SpiceCharDeviceStateItem, link);
        spice_char_device_stop(st_item->st);
    }
    red_dispatcher_on_vm_stop();
}

SPICE_GNUC_VISIBLE void spice_server_set_seamless_migration(SpiceServer *s, int enable)
{
    spice_assert(s == reds);
    /* seamless migration is not supported with multiple clients */
    reds->seamless_migration_enabled = enable && !reds->allow_multiple_clients;
    spice_debug("seamless migration enabled=%d", enable);
}

ssize_t reds_stream_read(RedsStream *s, void *buf, size_t nbyte)
{
    ssize_t ret;

#if HAVE_SASL
    if (s->sasl.conn && s->sasl.runSSF) {
        ret = reds_stream_sasl_read(s, buf, nbyte);
    } else
#endif
        ret = s->read(s, buf, nbyte);

    return ret;
}

ssize_t reds_stream_write(RedsStream *s, const void *buf, size_t nbyte)
{
    ssize_t ret;

#if HAVE_SASL
    if (s->sasl.conn && s->sasl.runSSF) {
        ret = reds_stream_sasl_write(s, buf, nbyte);
    } else
#endif
        ret = s->write(s, buf, nbyte);

    return ret;
}

ssize_t reds_stream_writev(RedsStream *s, const struct iovec *iov, int iovcnt)
{
    int i;
    int n;
    ssize_t ret = 0;

    if (s->writev != NULL) {
        return s->writev(s, iov, iovcnt);
    }

    for (i = 0; i < iovcnt; ++i) {
        n = reds_stream_write(s, iov[i].iov_base, iov[i].iov_len);
        if (n <= 0)
            return ret == 0 ? n : ret;
        ret += n;
    }

    return ret;
}

void reds_stream_free(RedsStream *s)
{
    if (!s) {
        return;
    }

    reds_stream_push_channel_event(s, SPICE_CHANNEL_EVENT_DISCONNECTED);

#if HAVE_SASL
    if (s->sasl.conn) {
        s->sasl.runSSF = s->sasl.wantSSF = 0;
        s->sasl.len = 0;
        s->sasl.encodedLength = s->sasl.encodedOffset = 0;
        s->sasl.encoded = NULL;
        free(s->sasl.mechlist);
        free(s->sasl.mechname);
        s->sasl.mechlist = NULL;
        sasl_dispose(&s->sasl.conn);
        s->sasl.conn = NULL;
    }
#endif

    if (s->ssl) {
        SSL_free(s->ssl);
    }

    reds_stream_remove_watch(s);
    spice_info("close socket fd %d", s->socket);
    close(s->socket);

    free(s);
}
