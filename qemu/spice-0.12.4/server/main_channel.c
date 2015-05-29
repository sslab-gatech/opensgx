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

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
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

#include "common/generated_server_marshallers.h"
#include "common/messages.h"
#include "common/ring.h"

#include "demarshallers.h"
#include "main_channel.h"
#include "red_channel.h"
#include "red_common.h"
#include "reds.h"
#include "migration_protocol.h"

#define ZERO_BUF_SIZE 4096

#define NET_TEST_WARMUP_BYTES 0
#define NET_TEST_BYTES (1024 * 250)

#define PING_INTERVAL (1000 * 10)

static uint8_t zero_page[ZERO_BUF_SIZE] = {0};

enum {
    PIPE_ITEM_TYPE_MAIN_CHANNELS_LIST = PIPE_ITEM_TYPE_CHANNEL_BASE,
    PIPE_ITEM_TYPE_MAIN_PING,
    PIPE_ITEM_TYPE_MAIN_MOUSE_MODE,
    PIPE_ITEM_TYPE_MAIN_AGENT_DISCONNECTED,
    PIPE_ITEM_TYPE_MAIN_AGENT_TOKEN,
    PIPE_ITEM_TYPE_MAIN_AGENT_DATA,
    PIPE_ITEM_TYPE_MAIN_MIGRATE_DATA,
    PIPE_ITEM_TYPE_MAIN_INIT,
    PIPE_ITEM_TYPE_MAIN_NOTIFY,
    PIPE_ITEM_TYPE_MAIN_MIGRATE_BEGIN,
    PIPE_ITEM_TYPE_MAIN_MIGRATE_BEGIN_SEAMLESS,
    PIPE_ITEM_TYPE_MAIN_MIGRATE_SWITCH_HOST,
    PIPE_ITEM_TYPE_MAIN_MULTI_MEDIA_TIME,
    PIPE_ITEM_TYPE_MAIN_NAME,
    PIPE_ITEM_TYPE_MAIN_UUID,
    PIPE_ITEM_TYPE_MAIN_AGENT_CONNECTED_TOKENS,
};

typedef struct RedsOutItem RedsOutItem;
struct RedsOutItem {
    PipeItem base;
};

typedef struct RefsPipeItem {
    PipeItem base;
    int *refs;
} RefsPipeItem;

typedef struct PingPipeItem {
    PipeItem base;
    int size;
} PingPipeItem;

typedef struct MouseModePipeItem {
    PipeItem base;
    int current_mode;
    int is_client_mouse_allowed;
} MouseModePipeItem;

typedef struct TokensPipeItem {
    PipeItem base;
    int tokens;
} TokensPipeItem;

typedef struct AgentDataPipeItem {
    PipeItem base;
    uint8_t* data;
    size_t len;
    spice_marshaller_item_free_func free_data;
    void *opaque;
} AgentDataPipeItem;

typedef struct InitPipeItem {
    PipeItem base;
    int connection_id;
    int display_channels_hint;
    int current_mouse_mode;
    int is_client_mouse_allowed;
    int multi_media_time;
    int ram_hint;
} InitPipeItem;

typedef struct NamePipeItem {
    PipeItem base;
    SpiceMsgMainName msg;
} NamePipeItem;

typedef struct UuidPipeItem {
    PipeItem base;
    SpiceMsgMainUuid msg;
} UuidPipeItem;

typedef struct NotifyPipeItem {
    PipeItem base;
    char *msg;
} NotifyPipeItem;

typedef struct MultiMediaTimePipeItem {
    PipeItem base;
    int time;
} MultiMediaTimePipeItem;

struct MainChannelClient {
    RedChannelClient base;
    uint32_t connection_id;
    uint32_t ping_id;
    uint32_t net_test_id;
    int net_test_stage;
    uint64_t latency;
    uint64_t bitrate_per_sec;
#ifdef RED_STATISTICS
    SpiceTimer *ping_timer;
    int ping_interval;
#endif
    int mig_wait_connect;
    int mig_connect_ok;
    int mig_wait_prev_complete;
    int mig_wait_prev_try_seamless;
    int init_sent;
    int seamless_mig_dst;
};

enum NetTestStage {
    NET_TEST_STAGE_INVALID,
    NET_TEST_STAGE_WARMUP,
    NET_TEST_STAGE_LATENCY,
    NET_TEST_STAGE_RATE,
    NET_TEST_STAGE_COMPLETE,
};

static void main_channel_release_pipe_item(RedChannelClient *rcc,
                                           PipeItem *base, int item_pushed);

int main_channel_is_connected(MainChannel *main_chan)
{
    return red_channel_is_connected(&main_chan->base);
}

// when disconnection occurs, let reds shutdown all channels. This will trigger the
// real disconnection of main channel
static void main_channel_client_on_disconnect(RedChannelClient *rcc)
{
    spice_printerr("rcc=%p", rcc);
    reds_client_disconnect(rcc->client);
//    red_channel_client_disconnect(rcc);
}

RedClient *main_channel_get_client_by_link_id(MainChannel *main_chan, uint32_t connection_id)
{
    RingItem *link;
    MainChannelClient *mcc;

    RING_FOREACH(link, &main_chan->base.clients) {
        mcc = SPICE_CONTAINEROF(link, MainChannelClient, base.channel_link);
        if (mcc->connection_id == connection_id) {
            return mcc->base.client;
        }
    }
    return NULL;
}

static int main_channel_client_push_ping(MainChannelClient *mcc, int size);

void main_channel_client_start_net_test(MainChannelClient *mcc)
{
    if (!mcc || mcc->net_test_id) {
        return;
    }
    if (main_channel_client_push_ping(mcc, NET_TEST_WARMUP_BYTES)
        && main_channel_client_push_ping(mcc, 0)
        && main_channel_client_push_ping(mcc, NET_TEST_BYTES)) {
        mcc->net_test_id = mcc->ping_id - 2;
        mcc->net_test_stage = NET_TEST_STAGE_WARMUP;
    }
}

typedef struct MainMouseModeItemInfo {
    int current_mode;
    int is_client_mouse_allowed;
} MainMouseModeItemInfo;

static PipeItem *main_mouse_mode_item_new(RedChannelClient *rcc, void *data, int num)
{
    MouseModePipeItem *item = spice_malloc(sizeof(MouseModePipeItem));
    MainMouseModeItemInfo *info = data;

    red_channel_pipe_item_init(rcc->channel, &item->base,
                               PIPE_ITEM_TYPE_MAIN_MOUSE_MODE);
    item->current_mode = info->current_mode;
    item->is_client_mouse_allowed = info->is_client_mouse_allowed;
    return &item->base;
}

static PipeItem *main_ping_item_new(MainChannelClient *mcc, int size)
{
    PingPipeItem *item = spice_malloc(sizeof(PingPipeItem));

    red_channel_pipe_item_init(mcc->base.channel, &item->base, PIPE_ITEM_TYPE_MAIN_PING);
    item->size = size;
    return &item->base;
}

static PipeItem *main_agent_tokens_item_new(RedChannelClient *rcc, uint32_t num_tokens)
{
    TokensPipeItem *item = spice_malloc(sizeof(TokensPipeItem));

    red_channel_pipe_item_init(rcc->channel, &item->base,
                               PIPE_ITEM_TYPE_MAIN_AGENT_TOKEN);
    item->tokens = num_tokens;
    return &item->base;
}

static PipeItem *main_agent_data_item_new(RedChannelClient *rcc, uint8_t* data, size_t len,
                                          spice_marshaller_item_free_func free_data,
                                          void *opaque)
{
    AgentDataPipeItem *item = spice_malloc(sizeof(AgentDataPipeItem));

    red_channel_pipe_item_init(rcc->channel, &item->base,
                               PIPE_ITEM_TYPE_MAIN_AGENT_DATA);
    item->data = data;
    item->len = len;
    item->free_data = free_data;
    item->opaque = opaque;
    return &item->base;
}

static PipeItem *main_init_item_new(MainChannelClient *mcc,
    int connection_id, int display_channels_hint, int current_mouse_mode,
    int is_client_mouse_allowed, int multi_media_time,
    int ram_hint)
{
    InitPipeItem *item = spice_malloc(sizeof(InitPipeItem));

    red_channel_pipe_item_init(mcc->base.channel, &item->base,
                               PIPE_ITEM_TYPE_MAIN_INIT);
    item->connection_id = connection_id;
    item->display_channels_hint = display_channels_hint;
    item->current_mouse_mode = current_mouse_mode;
    item->is_client_mouse_allowed = is_client_mouse_allowed;
    item->multi_media_time = multi_media_time;
    item->ram_hint = ram_hint;
    return &item->base;
}

static PipeItem *main_name_item_new(MainChannelClient *mcc, const char *name)
{
    NamePipeItem *item = spice_malloc(sizeof(NamePipeItem) + strlen(name) + 1);

    red_channel_pipe_item_init(mcc->base.channel, &item->base,
                               PIPE_ITEM_TYPE_MAIN_NAME);
    item->msg.name_len = strlen(name) + 1;
    memcpy(&item->msg.name, name, item->msg.name_len);

    return &item->base;
}

static PipeItem *main_uuid_item_new(MainChannelClient *mcc, const uint8_t uuid[16])
{
    UuidPipeItem *item = spice_malloc(sizeof(UuidPipeItem));

    red_channel_pipe_item_init(mcc->base.channel, &item->base,
                               PIPE_ITEM_TYPE_MAIN_UUID);
    memcpy(item->msg.uuid, uuid, sizeof(item->msg.uuid));

    return &item->base;
}

static PipeItem *main_notify_item_new(RedChannelClient *rcc, void *data, int num)
{
    NotifyPipeItem *item = spice_malloc(sizeof(NotifyPipeItem));
    const char *msg = data;

    red_channel_pipe_item_init(rcc->channel, &item->base,
                               PIPE_ITEM_TYPE_MAIN_NOTIFY);
    item->msg = spice_strdup(msg);
    return &item->base;
}

static PipeItem *main_multi_media_time_item_new(
    RedChannelClient *rcc, void *data, int num)
{
    MultiMediaTimePipeItem *item, *info = data;

    item = spice_malloc(sizeof(MultiMediaTimePipeItem));
    red_channel_pipe_item_init(rcc->channel, &item->base,
                               PIPE_ITEM_TYPE_MAIN_MULTI_MEDIA_TIME);
    item->time = info->time;
    return &item->base;
}

static void main_channel_push_channels(MainChannelClient *mcc)
{
    if (red_client_during_migrate_at_target(mcc->base.client)) {
        spice_printerr("warning: ignoring unexpected SPICE_MSGC_MAIN_ATTACH_CHANNELS"
                   "during migration");
        return;
    }
    red_channel_client_pipe_add_type(&mcc->base, PIPE_ITEM_TYPE_MAIN_CHANNELS_LIST);
}

static void main_channel_marshall_channels(RedChannelClient *rcc,
                                           SpiceMarshaller *m,
                                           PipeItem *item)
{
    SpiceMsgChannels* channels_info;

    red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_CHANNELS_LIST, item);
    channels_info = (SpiceMsgChannels *)spice_malloc(sizeof(SpiceMsgChannels)
                            + reds_num_of_channels() * sizeof(SpiceChannelId));
    reds_fill_channels(channels_info);
    spice_marshall_msg_main_channels_list(m, channels_info);
    free(channels_info);
}

int main_channel_client_push_ping(MainChannelClient *mcc, int size)
{
    PipeItem *item;

    if (mcc == NULL) {
        return FALSE;
    }
    item = main_ping_item_new(mcc, size);
    red_channel_client_pipe_add_push(&mcc->base, item);
    return TRUE;
}

static void main_channel_marshall_ping(RedChannelClient *rcc,
                                       SpiceMarshaller *m,
                                       PingPipeItem *item)
{
    MainChannelClient *mcc = SPICE_CONTAINEROF(rcc, MainChannelClient, base);
    struct timespec time_space;
    SpiceMsgPing ping;
    int size_left = item->size;

    red_channel_client_init_send_data(rcc, SPICE_MSG_PING, &item->base);
    ping.id = ++(mcc->ping_id);
    clock_gettime(CLOCK_MONOTONIC, &time_space);
    ping.timestamp = time_space.tv_sec * 1000000LL + time_space.tv_nsec / 1000LL;
    spice_marshall_msg_ping(m, &ping);

    while (size_left > 0) {
        int now = MIN(ZERO_BUF_SIZE, size_left);
        size_left -= now;
        spice_marshaller_add_ref(m, zero_page, now);
    }
}

void main_channel_push_mouse_mode(MainChannel *main_chan, int current_mode,
                                  int is_client_mouse_allowed)
{
    MainMouseModeItemInfo info = {
        .current_mode=current_mode,
        .is_client_mouse_allowed=is_client_mouse_allowed,
    };

    red_channel_pipes_new_add_push(&main_chan->base,
        main_mouse_mode_item_new, &info);
}

static void main_channel_marshall_mouse_mode(RedChannelClient *rcc,
                                             SpiceMarshaller *m,
                                             MouseModePipeItem *item)
{
    SpiceMsgMainMouseMode mouse_mode;

    red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_MOUSE_MODE, &item->base);
    mouse_mode.supported_modes = SPICE_MOUSE_MODE_SERVER;
    if (item->is_client_mouse_allowed) {
        mouse_mode.supported_modes |= SPICE_MOUSE_MODE_CLIENT;
    }
    mouse_mode.current_mode = item->current_mode;
    spice_marshall_msg_main_mouse_mode(m, &mouse_mode);
}

void main_channel_push_agent_connected(MainChannel *main_chan)
{
    if (red_channel_test_remote_cap(&main_chan->base, SPICE_MAIN_CAP_AGENT_CONNECTED_TOKENS)) {
        red_channel_pipes_add_type(&main_chan->base, PIPE_ITEM_TYPE_MAIN_AGENT_CONNECTED_TOKENS);
    } else {
        red_channel_pipes_add_empty_msg(&main_chan->base, SPICE_MSG_MAIN_AGENT_CONNECTED);
    }
}

static void main_channel_marshall_agent_connected(SpiceMarshaller *m,
                                                  RedChannelClient *rcc,
                                                  PipeItem *item)
{
    SpiceMsgMainAgentConnectedTokens connected;

    red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_AGENT_CONNECTED_TOKENS, item);
    connected.num_tokens = REDS_AGENT_WINDOW_SIZE;
    spice_marshall_msg_main_agent_connected_tokens(m, &connected);
}

void main_channel_push_agent_disconnected(MainChannel *main_chan)
{
    red_channel_pipes_add_type(&main_chan->base, PIPE_ITEM_TYPE_MAIN_AGENT_DISCONNECTED);
}

static void main_channel_marshall_agent_disconnected(RedChannelClient *rcc,
                                                     SpiceMarshaller *m,
                                                     PipeItem *item)
{
    SpiceMsgMainAgentDisconnect disconnect;

    red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_AGENT_DISCONNECTED, item);
    disconnect.error_code = SPICE_LINK_ERR_OK;
    spice_marshall_msg_main_agent_disconnected(m, &disconnect);
}

void main_channel_client_push_agent_tokens(MainChannelClient *mcc, uint32_t num_tokens)
{
    PipeItem *item = main_agent_tokens_item_new(&mcc->base, num_tokens);

    red_channel_client_pipe_add_push(&mcc->base, item);
}

static void main_channel_marshall_tokens(RedChannelClient *rcc,
                                         SpiceMarshaller *m, TokensPipeItem *item)
{
    SpiceMsgMainAgentTokens tokens;

    red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_AGENT_TOKEN, &item->base);
    tokens.num_tokens = item->tokens;
    spice_marshall_msg_main_agent_token(m, &tokens);
}

void main_channel_client_push_agent_data(MainChannelClient *mcc, uint8_t* data, size_t len,
           spice_marshaller_item_free_func free_data, void *opaque)
{
    PipeItem *item;

    item = main_agent_data_item_new(&mcc->base, data, len, free_data, opaque);
    red_channel_client_pipe_add_push(&mcc->base, item);
}

static void main_channel_marshall_agent_data(RedChannelClient *rcc,
                                             SpiceMarshaller *m,
                                             AgentDataPipeItem *item)
{
    red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_AGENT_DATA, &item->base);
    spice_marshaller_add_ref(m, item->data, item->len);
}

static void main_channel_push_migrate_data_item(MainChannel *main_chan)
{
    red_channel_pipes_add_type(&main_chan->base, PIPE_ITEM_TYPE_MAIN_MIGRATE_DATA);
}

static void main_channel_marshall_migrate_data_item(RedChannelClient *rcc,
                                                    SpiceMarshaller *m, PipeItem *item)
{
    red_channel_client_init_send_data(rcc, SPICE_MSG_MIGRATE_DATA, item);
    reds_marshall_migrate_data(m); // TODO: from reds split. ugly separation.
}

static int main_channel_handle_migrate_data(RedChannelClient *rcc,
    uint32_t size, void *message)
{
    MainChannelClient *mcc = SPICE_CONTAINEROF(rcc, MainChannelClient, base);
    SpiceMigrateDataHeader *header = (SpiceMigrateDataHeader *)message;

    /* not supported with multi-clients */
    spice_assert(rcc->channel->clients_num == 1);

    if (size < sizeof(SpiceMigrateDataHeader) + sizeof(SpiceMigrateDataMain)) {
        spice_printerr("bad message size %u", size);
        return FALSE;
    }
    if (!migration_protocol_validate_header(header,
                                            SPICE_MIGRATE_DATA_MAIN_MAGIC,
                                            SPICE_MIGRATE_DATA_MAIN_VERSION)) {
        spice_error("bad header");
        return FALSE;
    }
    return reds_handle_migrate_data(mcc, (SpiceMigrateDataMain *)(header + 1), size);
}

void main_channel_push_init(MainChannelClient *mcc,
    int display_channels_hint, int current_mouse_mode,
    int is_client_mouse_allowed, int multi_media_time,
    int ram_hint)
{
    PipeItem *item;

    item = main_init_item_new(mcc,
             mcc->connection_id, display_channels_hint, current_mouse_mode,
             is_client_mouse_allowed, multi_media_time, ram_hint);
    red_channel_client_pipe_add_push(&mcc->base, item);
}

static void main_channel_marshall_init(RedChannelClient *rcc,
                                       SpiceMarshaller *m,
                                       InitPipeItem *item)
{
    SpiceMsgMainInit init; // TODO - remove this copy, make InitPipeItem reuse SpiceMsgMainInit


    red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_INIT, &item->base);
    init.session_id = item->connection_id;
    init.display_channels_hint = item->display_channels_hint;
    init.current_mouse_mode = item->current_mouse_mode;
    init.supported_mouse_modes = SPICE_MOUSE_MODE_SERVER;
    if (item->is_client_mouse_allowed) {
        init.supported_mouse_modes |= SPICE_MOUSE_MODE_CLIENT;
    }
    init.agent_connected = reds_has_vdagent();
    init.agent_tokens = REDS_AGENT_WINDOW_SIZE;
    init.multi_media_time = item->multi_media_time;
    init.ram_hint = item->ram_hint;
    spice_marshall_msg_main_init(m, &init);
}

void main_channel_push_name(MainChannelClient *mcc, const char *name)
{
    PipeItem *item;

    if (!red_channel_client_test_remote_cap(&mcc->base,
                                            SPICE_MAIN_CAP_NAME_AND_UUID))
        return;

    item = main_name_item_new(mcc, name);
    red_channel_client_pipe_add_push(&mcc->base, item);
}

void main_channel_push_uuid(MainChannelClient *mcc, const uint8_t uuid[16])
{
    PipeItem *item;

    if (!red_channel_client_test_remote_cap(&mcc->base,
                                            SPICE_MAIN_CAP_NAME_AND_UUID))
        return;

    item = main_uuid_item_new(mcc, uuid);
    red_channel_client_pipe_add_push(&mcc->base, item);
}

void main_channel_push_notify(MainChannel *main_chan, const char *msg)
{
    red_channel_pipes_new_add_push(&main_chan->base,
        main_notify_item_new, (void *)msg);
}

void main_channel_client_push_notify(MainChannelClient *mcc, const char *msg)
{
    PipeItem *item = main_notify_item_new(&mcc->base, (void *)msg, 1);
    red_channel_client_pipe_add_push(&mcc->base, item);
}

static uint64_t get_time_stamp(void)
{
    struct timespec time_space;
    clock_gettime(CLOCK_MONOTONIC, &time_space);
    return time_space.tv_sec * 1000 * 1000 * 1000 + time_space.tv_nsec;
}

static void main_channel_marshall_notify(RedChannelClient *rcc,
                                         SpiceMarshaller *m, NotifyPipeItem *item)
{
    SpiceMsgNotify notify;

    red_channel_client_init_send_data(rcc, SPICE_MSG_NOTIFY, &item->base);
    notify.time_stamp = get_time_stamp(); // TODO - move to main_new_notify_item
    notify.severity = SPICE_NOTIFY_SEVERITY_WARN;
    notify.visibilty = SPICE_NOTIFY_VISIBILITY_HIGH;
    notify.what = SPICE_WARN_GENERAL;
    notify.message_len = strlen(item->msg);
    spice_marshall_msg_notify(m, &notify);
    spice_marshaller_add(m, (uint8_t *)item->msg, notify.message_len + 1);
}

static void main_channel_fill_migrate_dst_info(MainChannel *main_channel,
                                               SpiceMigrationDstInfo *dst_info)
{
    RedsMigSpice *mig_dst = &main_channel->mig_target;
    dst_info->port = mig_dst->port;
    dst_info->sport = mig_dst->sport;
    dst_info->host_size = strlen(mig_dst->host) + 1;
    dst_info->host_data = (uint8_t *)mig_dst->host;
    if (mig_dst->cert_subject) {
        dst_info->cert_subject_size = strlen(mig_dst->cert_subject) + 1;
        dst_info->cert_subject_data = (uint8_t *)mig_dst->cert_subject;
    } else {
        dst_info->cert_subject_size = 0;
        dst_info->cert_subject_data = NULL;
    }
}

static void main_channel_marshall_migrate_begin(SpiceMarshaller *m, RedChannelClient *rcc,
                                                PipeItem *item)
{
    SpiceMsgMainMigrationBegin migrate;
    MainChannel *main_ch;

    red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_MIGRATE_BEGIN, item);
    main_ch = SPICE_CONTAINEROF(rcc->channel, MainChannel, base);
    main_channel_fill_migrate_dst_info(main_ch, &migrate.dst_info);
    spice_marshall_msg_main_migrate_begin(m, &migrate);
}

static void main_channel_marshall_migrate_begin_seamless(SpiceMarshaller *m,
                                                         RedChannelClient *rcc,
                                                         PipeItem *item)
{
    SpiceMsgMainMigrateBeginSeamless migrate_seamless;
    MainChannel *main_ch;

    red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_MIGRATE_BEGIN_SEAMLESS, item);
    main_ch = SPICE_CONTAINEROF(rcc->channel, MainChannel, base);
    main_channel_fill_migrate_dst_info(main_ch, &migrate_seamless.dst_info);
    migrate_seamless.src_mig_version = SPICE_MIGRATION_PROTOCOL_VERSION;
    spice_marshall_msg_main_migrate_begin_seamless(m, &migrate_seamless);
}

void main_channel_push_multi_media_time(MainChannel *main_chan, int time)
{
    MultiMediaTimePipeItem info = {
        .time = time,
    };

    red_channel_pipes_new_add_push(&main_chan->base,
        main_multi_media_time_item_new, &info);
}

static void main_channel_fill_mig_target(MainChannel *main_channel, RedsMigSpice *mig_target)
{
    spice_assert(mig_target);
    free(main_channel->mig_target.host);
    main_channel->mig_target.host = spice_strdup(mig_target->host);
    free(main_channel->mig_target.cert_subject);
    if (mig_target->cert_subject) {
        main_channel->mig_target.cert_subject = spice_strdup(mig_target->cert_subject);
    } else {
        main_channel->mig_target.cert_subject = NULL;
    }
    main_channel->mig_target.port = mig_target->port;
    main_channel->mig_target.sport = mig_target->sport;
}

void main_channel_migrate_switch(MainChannel *main_chan, RedsMigSpice *mig_target)
{
    main_channel_fill_mig_target(main_chan, mig_target);
    red_channel_pipes_add_type(&main_chan->base, PIPE_ITEM_TYPE_MAIN_MIGRATE_SWITCH_HOST);
}

static void main_channel_marshall_migrate_switch(SpiceMarshaller *m, RedChannelClient *rcc,
                                                 PipeItem *item)
{
    SpiceMsgMainMigrationSwitchHost migrate;
    MainChannel *main_ch;

    spice_printerr("");
    red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_MIGRATE_SWITCH_HOST, item);
    main_ch = SPICE_CONTAINEROF(rcc->channel, MainChannel, base);
    migrate.port = main_ch->mig_target.port;
    migrate.sport = main_ch->mig_target.sport;
    migrate.host_size = strlen(main_ch->mig_target.host) + 1;
    migrate.host_data = (uint8_t *)main_ch->mig_target.host;
    if (main_ch->mig_target.cert_subject) {
        migrate.cert_subject_size = strlen(main_ch->mig_target.cert_subject) + 1;
        migrate.cert_subject_data = (uint8_t *)main_ch->mig_target.cert_subject;
    } else {
        migrate.cert_subject_size = 0;
        migrate.cert_subject_data = NULL;
    }
    spice_marshall_msg_main_migrate_switch_host(m, &migrate);
}

static void main_channel_marshall_multi_media_time(RedChannelClient *rcc,
                                                   SpiceMarshaller *m,
                                                   MultiMediaTimePipeItem *item)
{
    SpiceMsgMainMultiMediaTime time_mes;

    red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_MULTI_MEDIA_TIME, &item->base);
    time_mes.time = item->time;
    spice_marshall_msg_main_multi_media_time(m, &time_mes);
}

static void main_channel_send_item(RedChannelClient *rcc, PipeItem *base)
{
    MainChannelClient *mcc = SPICE_CONTAINEROF(rcc, MainChannelClient, base);
    SpiceMarshaller *m = red_channel_client_get_marshaller(rcc);

    /* In semi-seamless migration (dest side), the connection is started from scratch, and
     * we ignore any pipe item that arrives before the INIT msg is sent.
     * For seamless we don't send INIT, and the connection continues from the same place
     * it stopped on the src side. */
    if (!mcc->init_sent && !mcc->seamless_mig_dst && base->type != PIPE_ITEM_TYPE_MAIN_INIT) {
        spice_printerr("Init msg for client %p was not sent yet "
                       "(client is probably during semi-seamless migration). Ignoring msg type %d",
                   rcc->client, base->type);
        main_channel_release_pipe_item(rcc, base, FALSE);
        return;
    }
    switch (base->type) {
        case PIPE_ITEM_TYPE_MAIN_CHANNELS_LIST:
            main_channel_marshall_channels(rcc, m, base);
            break;
        case PIPE_ITEM_TYPE_MAIN_PING:
            main_channel_marshall_ping(rcc, m,
                SPICE_CONTAINEROF(base, PingPipeItem, base));
            break;
        case PIPE_ITEM_TYPE_MAIN_MOUSE_MODE:
            {
                MouseModePipeItem *item =
                    SPICE_CONTAINEROF(base, MouseModePipeItem, base);
                main_channel_marshall_mouse_mode(rcc, m, item);
                break;
            }
        case PIPE_ITEM_TYPE_MAIN_AGENT_DISCONNECTED:
            main_channel_marshall_agent_disconnected(rcc, m, base);
            break;
        case PIPE_ITEM_TYPE_MAIN_AGENT_TOKEN:
            main_channel_marshall_tokens(rcc, m,
                SPICE_CONTAINEROF(base, TokensPipeItem, base));
            break;
        case PIPE_ITEM_TYPE_MAIN_AGENT_DATA:
            main_channel_marshall_agent_data(rcc, m,
                SPICE_CONTAINEROF(base, AgentDataPipeItem, base));
            break;
        case PIPE_ITEM_TYPE_MAIN_MIGRATE_DATA:
            main_channel_marshall_migrate_data_item(rcc, m, base);
            break;
        case PIPE_ITEM_TYPE_MAIN_INIT:
            mcc->init_sent = TRUE;
            main_channel_marshall_init(rcc, m,
                SPICE_CONTAINEROF(base, InitPipeItem, base));
            break;
        case PIPE_ITEM_TYPE_MAIN_NOTIFY:
            main_channel_marshall_notify(rcc, m,
                SPICE_CONTAINEROF(base, NotifyPipeItem, base));
            break;
        case PIPE_ITEM_TYPE_MAIN_MIGRATE_BEGIN:
            main_channel_marshall_migrate_begin(m, rcc, base);
            break;
        case PIPE_ITEM_TYPE_MAIN_MIGRATE_BEGIN_SEAMLESS:
            main_channel_marshall_migrate_begin_seamless(m, rcc, base);
            break;
        case PIPE_ITEM_TYPE_MAIN_MULTI_MEDIA_TIME:
            main_channel_marshall_multi_media_time(rcc, m,
                SPICE_CONTAINEROF(base, MultiMediaTimePipeItem, base));
            break;
        case PIPE_ITEM_TYPE_MAIN_MIGRATE_SWITCH_HOST:
            main_channel_marshall_migrate_switch(m, rcc, base);
            break;
        case PIPE_ITEM_TYPE_MAIN_NAME:
            red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_NAME, base);
            spice_marshall_msg_main_name(m, &SPICE_CONTAINEROF(base, NamePipeItem, base)->msg);
            break;
        case PIPE_ITEM_TYPE_MAIN_UUID:
            red_channel_client_init_send_data(rcc, SPICE_MSG_MAIN_UUID, base);
            spice_marshall_msg_main_uuid(m, &SPICE_CONTAINEROF(base, UuidPipeItem, base)->msg);
            break;
        case PIPE_ITEM_TYPE_MAIN_AGENT_CONNECTED_TOKENS:
            main_channel_marshall_agent_connected(m, rcc, base);
            break;
        default:
            break;
    };
    red_channel_client_begin_send_message(rcc);
}

static void main_channel_release_pipe_item(RedChannelClient *rcc,
    PipeItem *base, int item_pushed)
{
    switch (base->type) {
        case PIPE_ITEM_TYPE_MAIN_AGENT_DATA: {
                AgentDataPipeItem *data = (AgentDataPipeItem *)base;

                data->free_data(data->data, data->opaque);
                break;
        }
        case PIPE_ITEM_TYPE_MAIN_NOTIFY: {
                NotifyPipeItem *data = (NotifyPipeItem *)base;
                free(data->msg);
                break;
        }
        default:
            break;
    }
    free(base);
}

static void main_channel_client_handle_migrate_connected(MainChannelClient *mcc,
                                                         int success,
                                                         int seamless)
{
    spice_printerr("client %p connected: %d seamless %d", mcc->base.client, success, seamless);
    if (mcc->mig_wait_connect) {
        MainChannel *main_channel = SPICE_CONTAINEROF(mcc->base.channel, MainChannel, base);

        mcc->mig_wait_connect = FALSE;
        mcc->mig_connect_ok = success;
        spice_assert(main_channel->num_clients_mig_wait);
        spice_assert(!seamless || main_channel->num_clients_mig_wait == 1);
        if (!--main_channel->num_clients_mig_wait) {
            reds_on_main_migrate_connected(seamless && success);
        }
    } else {
        if (success) {
            spice_printerr("client %p MIGRATE_CANCEL", mcc->base.client);
            red_channel_client_pipe_add_empty_msg(&mcc->base, SPICE_MSG_MAIN_MIGRATE_CANCEL);
        }
    }
}

void main_channel_client_handle_migrate_dst_do_seamless(MainChannelClient *mcc,
                                                        uint32_t src_version)
{
    if (reds_on_migrate_dst_set_seamless(mcc, src_version)) {
        mcc->seamless_mig_dst = TRUE;
        red_channel_client_pipe_add_empty_msg(&mcc->base,
                                             SPICE_MSG_MAIN_MIGRATE_DST_SEAMLESS_ACK);
    } else {
        red_channel_client_pipe_add_empty_msg(&mcc->base,
                                              SPICE_MSG_MAIN_MIGRATE_DST_SEAMLESS_NACK);
    }
}

void main_channel_client_handle_migrate_end(MainChannelClient *mcc)
{
    if (!red_client_during_migrate_at_target(mcc->base.client)) {
        spice_printerr("unexpected SPICE_MSGC_MIGRATE_END");
        return;
    }
    if (!red_channel_client_test_remote_cap(&mcc->base,
                                            SPICE_MAIN_CAP_SEMI_SEAMLESS_MIGRATE)) {
        spice_printerr("unexpected SPICE_MSGC_MIGRATE_END, "
                   "client does not support semi-seamless migration");
            return;
    }
    red_client_semi_seamless_migrate_complete(mcc->base.client);
}

void main_channel_migrate_dst_complete(MainChannelClient *mcc)
{
    if (mcc->mig_wait_prev_complete) {
        if (mcc->mig_wait_prev_try_seamless) {
            spice_assert(mcc->base.channel->clients_num == 1);
            red_channel_client_pipe_add_type(&mcc->base,
                                             PIPE_ITEM_TYPE_MAIN_MIGRATE_BEGIN_SEAMLESS);
        } else {
            red_channel_client_pipe_add_type(&mcc->base, PIPE_ITEM_TYPE_MAIN_MIGRATE_BEGIN);
        }
        mcc->mig_wait_connect = TRUE;
        mcc->mig_wait_prev_complete = FALSE;
    }
}

static int main_channel_handle_parsed(RedChannelClient *rcc, uint32_t size, uint16_t type,
                                      void *message)
{
    MainChannel *main_chan = SPICE_CONTAINEROF(rcc->channel, MainChannel, base);
    MainChannelClient *mcc = SPICE_CONTAINEROF(rcc, MainChannelClient, base);

    switch (type) {
    case SPICE_MSGC_MAIN_AGENT_START: {
        SpiceMsgcMainAgentStart *tokens;

        spice_printerr("agent start");
        if (!main_chan) {
            return FALSE;
        }
        tokens = (SpiceMsgcMainAgentStart *)message;
        reds_on_main_agent_start(mcc, tokens->num_tokens);
        break;
    }
    case SPICE_MSGC_MAIN_AGENT_DATA: {
        reds_on_main_agent_data(mcc, message, size);
        break;
    }
    case SPICE_MSGC_MAIN_AGENT_TOKEN: {
        SpiceMsgcMainAgentTokens *tokens;

        tokens = (SpiceMsgcMainAgentTokens *)message;
        reds_on_main_agent_tokens(mcc, tokens->num_tokens);
        break;
    }
    case SPICE_MSGC_MAIN_ATTACH_CHANNELS:
        main_channel_push_channels(mcc);
        break;
    case SPICE_MSGC_MAIN_MIGRATE_CONNECTED:
        main_channel_client_handle_migrate_connected(mcc,
                                                     TRUE /* success */,
                                                     FALSE /* seamless */);
        break;
    case SPICE_MSGC_MAIN_MIGRATE_CONNECTED_SEAMLESS:
        main_channel_client_handle_migrate_connected(mcc,
                                                     TRUE /* success */,
                                                     TRUE /* seamless */);
        break;
    case SPICE_MSGC_MAIN_MIGRATE_CONNECT_ERROR:
        main_channel_client_handle_migrate_connected(mcc, FALSE, FALSE);
        break;
    case SPICE_MSGC_MAIN_MIGRATE_DST_DO_SEAMLESS:
        main_channel_client_handle_migrate_dst_do_seamless(mcc,
            ((SpiceMsgcMainMigrateDstDoSeamless *)message)->src_version);
        break;
    case SPICE_MSGC_MAIN_MOUSE_MODE_REQUEST:
        reds_on_main_mouse_mode_request(message, size);
        break;
    case SPICE_MSGC_PONG: {
        SpiceMsgPing *ping = (SpiceMsgPing *)message;
        uint64_t roundtrip;
        struct timespec ts;

        clock_gettime(CLOCK_MONOTONIC, &ts);
        roundtrip = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL - ping->timestamp;

        if (ping->id == mcc->net_test_id) {
            switch (mcc->net_test_stage) {
            case NET_TEST_STAGE_WARMUP:
                mcc->net_test_id++;
                mcc->net_test_stage = NET_TEST_STAGE_LATENCY;
                break;
            case NET_TEST_STAGE_LATENCY:
                mcc->net_test_id++;
                mcc->net_test_stage = NET_TEST_STAGE_RATE;
                mcc->latency = roundtrip;
                break;
            case NET_TEST_STAGE_RATE:
                mcc->net_test_id = 0;
                if (roundtrip <= mcc->latency) {
                    // probably high load on client or server result with incorrect values
                    mcc->latency = 0;
                    mcc->net_test_stage = NET_TEST_STAGE_INVALID;
                    spice_printerr("net test: invalid values, latency %" PRIu64
                               " roundtrip %" PRIu64 ". assuming high"
                               "bandwidth", mcc->latency, roundtrip);
                    break;
                }
                mcc->bitrate_per_sec = (uint64_t)(NET_TEST_BYTES * 8) * 1000000
                                        / (roundtrip - mcc->latency);
                mcc->net_test_stage = NET_TEST_STAGE_COMPLETE;
                spice_printerr("net test: latency %f ms, bitrate %"PRIu64" bps (%f Mbps)%s",
                           (double)mcc->latency / 1000,
                           mcc->bitrate_per_sec,
                           (double)mcc->bitrate_per_sec / 1024 / 1024,
                           main_channel_client_is_low_bandwidth(mcc) ? " LOW BANDWIDTH" : "");
                break;
            default:
                spice_printerr("invalid net test stage, ping id %d test id %d stage %d",
                           ping->id,
                           mcc->net_test_id,
                           mcc->net_test_stage);
                mcc->net_test_stage = NET_TEST_STAGE_INVALID;
            }
            break;
        }
#ifdef RED_STATISTICS
        reds_update_stat_value(roundtrip);
#endif
        break;
    }
    case SPICE_MSGC_DISCONNECTING:
        break;
    case SPICE_MSGC_MAIN_MIGRATE_END:
        main_channel_client_handle_migrate_end(mcc);
        break;
    default:
        return red_channel_client_handle_message(rcc, size, type, message);
    }
    return TRUE;
}

static uint8_t *main_channel_alloc_msg_rcv_buf(RedChannelClient *rcc,
                                               uint16_t type,
                                               uint32_t size)
{
    MainChannel *main_chan = SPICE_CONTAINEROF(rcc->channel, MainChannel, base);
    MainChannelClient *mcc = SPICE_CONTAINEROF(rcc, MainChannelClient, base);

    if (type == SPICE_MSGC_MAIN_AGENT_DATA) {
        return reds_get_agent_data_buffer(mcc, size);
    } else {
        return main_chan->recv_buf;
    }
}

static void main_channel_release_msg_rcv_buf(RedChannelClient *rcc,
                                               uint16_t type,
                                               uint32_t size,
                                               uint8_t *msg)
{
    if (type == SPICE_MSGC_MAIN_AGENT_DATA) {
        reds_release_agent_data_buffer(msg);
    }
}

static int main_channel_config_socket(RedChannelClient *rcc)
{
    return TRUE;
}

static void main_channel_hold_pipe_item(RedChannelClient *rcc, PipeItem *item)
{
}

static int main_channel_handle_migrate_flush_mark(RedChannelClient *rcc)
{
    spice_debug(NULL);
    main_channel_push_migrate_data_item(SPICE_CONTAINEROF(rcc->channel,
                                        MainChannel, base));
    return TRUE;
}

#ifdef RED_STATISTICS
static void do_ping_client(MainChannelClient *mcc,
    const char *opt, int has_interval, int interval)
{
    spice_printerr("");
    if (!opt) {
        main_channel_client_push_ping(mcc, 0);
    } else if (!strcmp(opt, "on")) {
        if (has_interval && interval > 0) {
            mcc->ping_interval = interval * 1000;
        }
        core->timer_start(mcc->ping_timer, mcc->ping_interval);
    } else if (!strcmp(opt, "off")) {
        core->timer_cancel(mcc->ping_timer);
    } else {
        return;
    }
}

static void ping_timer_cb(void *opaque)
{
    MainChannelClient *mcc = opaque;

    if (!red_channel_client_is_connected(&mcc->base)) {
        spice_printerr("not connected to peer, ping off");
        core->timer_cancel(mcc->ping_timer);
        return;
    }
    do_ping_client(mcc, NULL, 0, 0);
    core->timer_start(mcc->ping_timer, mcc->ping_interval);
}
#endif /* RED_STATISTICS */

uint32_t main_channel_client_get_link_id(MainChannelClient *mcc)
{
    return mcc->connection_id;
}

static MainChannelClient *main_channel_client_create(MainChannel *main_chan, RedClient *client,
                                                     RedsStream *stream, uint32_t connection_id,
                                                     int num_common_caps, uint32_t *common_caps,
                                                     int num_caps, uint32_t *caps)
{
    MainChannelClient *mcc = (MainChannelClient*)
                             red_channel_client_create(sizeof(MainChannelClient), &main_chan->base,
                                                       client, stream, FALSE, num_common_caps,
                                                       common_caps, num_caps, caps);
    spice_assert(mcc != NULL);
    mcc->connection_id = connection_id;
    mcc->bitrate_per_sec = ~0;
#ifdef RED_STATISTICS
    if (!(mcc->ping_timer = core->timer_add(ping_timer_cb, NULL))) {
        spice_error("ping timer create failed");
    }
    mcc->ping_interval = PING_INTERVAL;
#endif
    return mcc;
}

MainChannelClient *main_channel_link(MainChannel *channel, RedClient *client,
                                     RedsStream *stream, uint32_t connection_id, int migration,
                                     int num_common_caps, uint32_t *common_caps, int num_caps,
                                     uint32_t *caps)
{
    MainChannelClient *mcc;

    spice_assert(channel);

    // TODO - migration - I removed it from channel creation, now put it
    // into usage somewhere (not an issue until we return migration to it's
    // former glory)
    spice_printerr("add main channel client");
    mcc = main_channel_client_create(channel, client, stream, connection_id,
                                     num_common_caps, common_caps,
                                     num_caps, caps);
    return mcc;
}

int main_channel_getsockname(MainChannel *main_chan, struct sockaddr *sa, socklen_t *salen)
{
    return main_chan ? getsockname(red_channel_get_first_socket(&main_chan->base), sa, salen) : -1;
}

int main_channel_getpeername(MainChannel *main_chan, struct sockaddr *sa, socklen_t *salen)
{
    return main_chan ? getpeername(red_channel_get_first_socket(&main_chan->base), sa, salen) : -1;
}

// TODO: ? shouldn't it disonnect all clients? or shutdown all main_channels?
void main_channel_close(MainChannel *main_chan)
{
    int socketfd;

    if (main_chan && (socketfd = red_channel_get_first_socket(&main_chan->base)) != -1) {
        close(socketfd);
    }
}

int main_channel_client_is_network_info_initialized(MainChannelClient *mcc)
{
    return mcc->net_test_stage == NET_TEST_STAGE_COMPLETE;
}

int main_channel_client_is_low_bandwidth(MainChannelClient *mcc)
{
    // TODO: configurable?
    return mcc->bitrate_per_sec < 10 * 1024 * 1024;
}

uint64_t main_channel_client_get_bitrate_per_sec(MainChannelClient *mcc)
{
    return mcc->bitrate_per_sec;
}

uint64_t main_channel_client_get_roundtrip_ms(MainChannelClient *mcc)
{
    return mcc->latency / 1000;
}

static void main_channel_client_migrate(RedChannelClient *rcc)
{
    reds_on_main_channel_migrate(SPICE_CONTAINEROF(rcc, MainChannelClient, base));
    red_channel_client_default_migrate(rcc);
}

MainChannel* main_channel_init(void)
{
    RedChannel *channel;
    ChannelCbs channel_cbs = { NULL, };
    ClientCbs client_cbs = {NULL, };

    channel_cbs.config_socket = main_channel_config_socket;
    channel_cbs.on_disconnect = main_channel_client_on_disconnect;
    channel_cbs.send_item = main_channel_send_item;
    channel_cbs.hold_item = main_channel_hold_pipe_item;
    channel_cbs.release_item = main_channel_release_pipe_item;
    channel_cbs.alloc_recv_buf = main_channel_alloc_msg_rcv_buf;
    channel_cbs.release_recv_buf = main_channel_release_msg_rcv_buf;
    channel_cbs.handle_migrate_flush_mark = main_channel_handle_migrate_flush_mark;
    channel_cbs.handle_migrate_data = main_channel_handle_migrate_data;

    // TODO: set the migration flag of the channel
    channel = red_channel_create_parser(sizeof(MainChannel), core,
                                        SPICE_CHANNEL_MAIN, 0,
                                        FALSE, /* handle_acks */
                                        spice_get_client_channel_parser(SPICE_CHANNEL_MAIN, NULL),
                                        main_channel_handle_parsed,
                                        &channel_cbs,
                                        SPICE_MIGRATE_NEED_FLUSH | SPICE_MIGRATE_NEED_DATA_TRANSFER);
    spice_assert(channel);
    red_channel_set_cap(channel, SPICE_MAIN_CAP_SEMI_SEAMLESS_MIGRATE);
    red_channel_set_cap(channel, SPICE_MAIN_CAP_SEAMLESS_MIGRATE);

    client_cbs.migrate = main_channel_client_migrate;
    red_channel_register_client_cbs(channel, &client_cbs);

    return (MainChannel *)channel;
}

RedChannelClient* main_channel_client_get_base(MainChannelClient* mcc)
{
    spice_assert(mcc);
    return &mcc->base;
}

static int main_channel_connect_semi_seamless(MainChannel *main_channel)
{
    RingItem *client_link;

    RING_FOREACH(client_link, &main_channel->base.clients) {
        MainChannelClient * mcc = SPICE_CONTAINEROF(client_link, MainChannelClient,
                                                    base.channel_link);
        if (red_channel_client_test_remote_cap(&mcc->base,
                                               SPICE_MAIN_CAP_SEMI_SEAMLESS_MIGRATE)) {
            if (red_client_during_migrate_at_target(mcc->base.client)) {
                spice_printerr("client %p: wait till previous migration completes", mcc->base.client);
                mcc->mig_wait_prev_complete = TRUE;
                mcc->mig_wait_prev_try_seamless = FALSE;
            } else {
                red_channel_client_pipe_add_type(&mcc->base,
                                                 PIPE_ITEM_TYPE_MAIN_MIGRATE_BEGIN);
                mcc->mig_wait_connect = TRUE;
            }
            mcc->mig_connect_ok = FALSE;
            main_channel->num_clients_mig_wait++;
        }
    }
    return main_channel->num_clients_mig_wait;
}

static int main_channel_connect_seamless(MainChannel *main_channel)
{
    RingItem *client_link;

    spice_assert(main_channel->base.clients_num == 1);

    RING_FOREACH(client_link, &main_channel->base.clients) {
        MainChannelClient * mcc = SPICE_CONTAINEROF(client_link, MainChannelClient,
                                                    base.channel_link);
        spice_assert(red_channel_client_test_remote_cap(&mcc->base,
                                                        SPICE_MAIN_CAP_SEAMLESS_MIGRATE));
        if (red_client_during_migrate_at_target(mcc->base.client)) {
           spice_printerr("client %p: wait till previous migration completes", mcc->base.client);
           mcc->mig_wait_prev_complete = TRUE;
           mcc->mig_wait_prev_try_seamless = TRUE;
        } else {
            red_channel_client_pipe_add_type(&mcc->base,
                                             PIPE_ITEM_TYPE_MAIN_MIGRATE_BEGIN_SEAMLESS);
            mcc->mig_wait_connect = TRUE;
        }
        mcc->mig_connect_ok = FALSE;
        main_channel->num_clients_mig_wait++;
    }
    return main_channel->num_clients_mig_wait;
}

int main_channel_migrate_connect(MainChannel *main_channel, RedsMigSpice *mig_target,
                                 int try_seamless)
{
    main_channel_fill_mig_target(main_channel, mig_target);
    main_channel->num_clients_mig_wait = 0;

    if (!main_channel_is_connected(main_channel)) {
        return 0;
    }

    if (!try_seamless) {
        return main_channel_connect_semi_seamless(main_channel);
    } else {
        RingItem *client_item;
        MainChannelClient *mcc;

        client_item = ring_get_head(&main_channel->base.clients);
        mcc = SPICE_CONTAINEROF(client_item, MainChannelClient, base.channel_link);

        if (!red_channel_client_test_remote_cap(&mcc->base,
                                                SPICE_MAIN_CAP_SEAMLESS_MIGRATE)) {
            return main_channel_connect_semi_seamless(main_channel);
        } else {
            return main_channel_connect_seamless(main_channel);
        }
    }

}

void main_channel_migrate_cancel_wait(MainChannel *main_chan)
{
    RingItem *client_link;

    RING_FOREACH(client_link, &main_chan->base.clients) {
        MainChannelClient *mcc;

        mcc = SPICE_CONTAINEROF(client_link, MainChannelClient, base.channel_link);
        if (mcc->mig_wait_connect) {
            spice_printerr("client %p cancel wait connect", mcc->base.client);
            mcc->mig_wait_connect = FALSE;
            mcc->mig_connect_ok = FALSE;
        }
        mcc->mig_wait_prev_complete = FALSE;
    }
    main_chan->num_clients_mig_wait = 0;
}

int main_channel_migrate_src_complete(MainChannel *main_chan, int success)
{
    RingItem *client_link;
    int semi_seamless_count = 0;

    spice_printerr("");

    if (ring_is_empty(&main_chan->base.clients)) {
        spice_printerr("no peer connected");
        return 0;
    }

    RING_FOREACH(client_link, &main_chan->base.clients) {
        MainChannelClient *mcc;
        int semi_seamless_support;

        mcc = SPICE_CONTAINEROF(client_link, MainChannelClient, base.channel_link);
        semi_seamless_support = red_channel_client_test_remote_cap(&mcc->base,
                                                   SPICE_MAIN_CAP_SEMI_SEAMLESS_MIGRATE);
        if (semi_seamless_support && mcc->mig_connect_ok) {
            if (success) {
                spice_printerr("client %p MIGRATE_END", mcc->base.client);
                red_channel_client_pipe_add_empty_msg(&mcc->base, SPICE_MSG_MAIN_MIGRATE_END);
                semi_seamless_count++;
            } else {
                spice_printerr("client %p MIGRATE_CANCEL", mcc->base.client);
                red_channel_client_pipe_add_empty_msg(&mcc->base, SPICE_MSG_MAIN_MIGRATE_CANCEL);
            }
        } else {
            if (success) {
                spice_printerr("client %p SWITCH_HOST", mcc->base.client);
                red_channel_client_pipe_add_type(&mcc->base, PIPE_ITEM_TYPE_MAIN_MIGRATE_SWITCH_HOST);
            }
        }
        mcc->mig_connect_ok = FALSE;
        mcc->mig_wait_connect = FALSE;
   }
   return semi_seamless_count;
}
