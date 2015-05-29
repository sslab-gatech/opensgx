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


    Author:
        yhalperi@redhat.com
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "common/generated_server_marshallers.h"
#include "common/ring.h"

#include "stat.h"
#include "red_channel.h"
#include "reds.h"
#include "main_dispatcher.h"

typedef struct EmptyMsgPipeItem {
    PipeItem base;
    int msg;
} EmptyMsgPipeItem;

#define PING_TEST_TIMEOUT_MS 15000
#define PING_TEST_IDLE_NET_TIMEOUT_MS 100

enum QosPingState {
    PING_STATE_NONE,
    PING_STATE_TIMER,
    PING_STATE_WARMUP,
    PING_STATE_LATENCY,
};

static void red_channel_client_start_ping_timer(RedChannelClient *rcc, uint32_t timeout);
static void red_channel_client_cancel_ping_timer(RedChannelClient *rcc);
static void red_channel_client_restart_ping_timer(RedChannelClient *rcc);

static void red_channel_client_event(int fd, int event, void *data);
static void red_client_add_channel(RedClient *client, RedChannelClient *rcc);
static void red_client_remove_channel(RedChannelClient *rcc);
static RedChannelClient *red_client_get_channel(RedClient *client, int type, int id);
static void red_channel_client_restore_main_sender(RedChannelClient *rcc);

/*
 * Lifetime of RedChannel, RedChannelClient and RedClient:
 * RedChannel is created and destroyed by the calls to
 * red_channel_create.* and red_channel_destroy. The RedChannel resources
 * are deallocated only after red_channel_destroy is called and no RedChannelClient
 * refers to the channel.
 * RedChannelClient is created and destroyed by the calls to red_channel_client_create
 * and red_channel_client_destroy. RedChannelClient resources are deallocated only when
 * its refs == 0. The reference count of RedChannelClient can be increased by routines
 * that include calls that might destroy the red_channel_client. For example,
 * red_peer_handle_incoming calls the handle_message proc of the channel, which
 * might lead to destroying the client. However, after the call to handle_message,
 * there is a call to the channel's release_msg_buf proc.
 *
 * Once red_channel_client_destroy is called, the RedChannelClient is disconnected and
 * removed from the RedChannel clients list, but if rcc->refs != 0, it will still hold
 * a reference to the Channel. The reason for this is that on the one hand RedChannel holds
 * callbacks that may be still in use by RedChannel, and on the other hand,
 * when an operation is performed on the list of clients that belongs to the channel,
 * we don't want to execute it on the "to be destroyed" channel client.
 *
 * RedClient is created and destroyed by the calls to red_client_new and red_client_destroy.
 * When it is destroyed, it also disconnects and destroys all the RedChannelClients that
 * are associated with it. However, since part of these channel clients may still have
 * other references, they will not be completely released, until they are dereferenced.
 *
 * Note: red_channel_client_destroy is not thread safe, and still it is called from
 * red_client_destroy (from the client's thread). However, since before this call,
 * red_client_destroy calls rcc->channel->client_cbs.disconnect(rcc), which is synchronous,
 * we assume that if the channel is in another thread, it does no longer have references to
 * this channel client.
 * If a call to red_channel_client_destroy is made from another location, it must be called
 * from the channel's thread.
*/
static void red_channel_ref(RedChannel *channel);
static void red_channel_unref(RedChannel *channel);
static void red_channel_client_ref(RedChannelClient *rcc);
static void red_channel_client_unref(RedChannelClient *rcc);

static uint32_t full_header_get_msg_size(SpiceDataHeaderOpaque *header)
{
    return ((SpiceDataHeader *)header->data)->size;
}

static uint32_t mini_header_get_msg_size(SpiceDataHeaderOpaque *header)
{
    return ((SpiceMiniDataHeader *)header->data)->size;
}

static uint16_t full_header_get_msg_type(SpiceDataHeaderOpaque *header)
{
    return ((SpiceDataHeader *)header->data)->type;
}

static uint16_t mini_header_get_msg_type(SpiceDataHeaderOpaque *header)
{
    return ((SpiceMiniDataHeader *)header->data)->type;
}

static void full_header_set_msg_type(SpiceDataHeaderOpaque *header, uint16_t type)
{
    ((SpiceDataHeader *)header->data)->type = type;
}

static void mini_header_set_msg_type(SpiceDataHeaderOpaque *header, uint16_t type)
{
    ((SpiceMiniDataHeader *)header->data)->type = type;
}

static void full_header_set_msg_size(SpiceDataHeaderOpaque *header, uint32_t size)
{
    ((SpiceDataHeader *)header->data)->size = size;
}

static void mini_header_set_msg_size(SpiceDataHeaderOpaque *header, uint32_t size)
{
    ((SpiceMiniDataHeader *)header->data)->size = size;
}

static void full_header_set_msg_serial(SpiceDataHeaderOpaque *header, uint64_t serial)
{
    ((SpiceDataHeader *)header->data)->serial = serial;
}

static void mini_header_set_msg_serial(SpiceDataHeaderOpaque *header, uint64_t serial)
{
    spice_error("attempt to set header serial on mini header");
}

static void full_header_set_msg_sub_list(SpiceDataHeaderOpaque *header, uint32_t sub_list)
{
    ((SpiceDataHeader *)header->data)->sub_list = sub_list;
}

static void mini_header_set_msg_sub_list(SpiceDataHeaderOpaque *header, uint32_t sub_list)
{
    spice_error("attempt to set header sub list on mini header");
}

static SpiceDataHeaderOpaque full_header_wrapper = {NULL, sizeof(SpiceDataHeader),
                                                    full_header_set_msg_type,
                                                    full_header_set_msg_size,
                                                    full_header_set_msg_serial,
                                                    full_header_set_msg_sub_list,
                                                    full_header_get_msg_type,
                                                    full_header_get_msg_size};

static SpiceDataHeaderOpaque mini_header_wrapper = {NULL, sizeof(SpiceMiniDataHeader),
                                                    mini_header_set_msg_type,
                                                    mini_header_set_msg_size,
                                                    mini_header_set_msg_serial,
                                                    mini_header_set_msg_sub_list,
                                                    mini_header_get_msg_type,
                                                    mini_header_get_msg_size};

/* return the number of bytes read. -1 in case of error */
static int red_peer_receive(RedsStream *stream, uint8_t *buf, uint32_t size)
{
    uint8_t *pos = buf;
    while (size) {
        int now;
        if (stream->shutdown) {
            return -1;
        }
        now = reds_stream_read(stream, pos, size);
        if (now <= 0) {
            if (now == 0) {
                return -1;
            }
            spice_assert(now == -1);
            if (errno == EAGAIN) {
                break;
            } else if (errno == EINTR) {
                continue;
            } else if (errno == EPIPE) {
                return -1;
            } else {
                spice_printerr("%s", strerror(errno));
                return -1;
            }
        } else {
            size -= now;
            pos += now;
        }
    }
    return pos - buf;
}

// TODO: this implementation, as opposed to the old implementation in red_worker,
// does many calls to red_peer_receive and through it cb_read, and thus avoids pointer
// arithmetic for the case where a single cb_read could return multiple messages. But
// this is suboptimal potentially. Profile and consider fixing.
static void red_peer_handle_incoming(RedsStream *stream, IncomingHandler *handler)
{
    int bytes_read;
    uint8_t *parsed;
    size_t parsed_size;
    message_destructor_t parsed_free;
    uint16_t msg_type;
    uint32_t msg_size;

    /* XXX: This needs further investigation as to the underlying cause, it happened
     * after spicec disconnect (but not with spice-gtk) repeatedly. */
    if (!stream) {
        return;
    }

    for (;;) {
        int ret_handle;
        if (handler->header_pos < handler->header.header_size) {
            bytes_read = red_peer_receive(stream,
                                          handler->header.data + handler->header_pos,
                                          handler->header.header_size - handler->header_pos);
            if (bytes_read == -1) {
                handler->cb->on_error(handler->opaque);
                return;
            }
            handler->header_pos += bytes_read;

            if (handler->header_pos != handler->header.header_size) {
                return;
            }
        }

        msg_size = handler->header.get_msg_size(&handler->header);
        msg_type = handler->header.get_msg_type(&handler->header);
        if (handler->msg_pos < msg_size) {
            if (!handler->msg) {
                handler->msg = handler->cb->alloc_msg_buf(handler->opaque, msg_type, msg_size);
                if (handler->msg == NULL) {
                    spice_printerr("ERROR: channel refused to allocate buffer.");
                    handler->cb->on_error(handler->opaque);
                    return;
                }
            }

            bytes_read = red_peer_receive(stream,
                                          handler->msg + handler->msg_pos,
                                          msg_size - handler->msg_pos);
            if (bytes_read == -1) {
                handler->cb->release_msg_buf(handler->opaque, msg_type, msg_size, handler->msg);
                handler->cb->on_error(handler->opaque);
                return;
            }
            handler->msg_pos += bytes_read;
            if (handler->msg_pos != msg_size) {
                return;
            }
        }

        if (handler->cb->parser) {
            parsed = handler->cb->parser(handler->msg,
                handler->msg + msg_size, msg_type,
                SPICE_VERSION_MINOR, &parsed_size, &parsed_free);
            if (parsed == NULL) {
                spice_printerr("failed to parse message type %d", msg_type);
                handler->cb->release_msg_buf(handler->opaque, msg_type, msg_size, handler->msg);
                handler->cb->on_error(handler->opaque);
                return;
            }
            ret_handle = handler->cb->handle_parsed(handler->opaque, parsed_size,
                                                    msg_type, parsed);
            parsed_free(parsed);
        } else {
            ret_handle = handler->cb->handle_message(handler->opaque, msg_type, msg_size,
                                                     handler->msg);
        }
        handler->msg_pos = 0;
        handler->cb->release_msg_buf(handler->opaque, msg_type, msg_size, handler->msg);
        handler->msg = NULL;
        handler->header_pos = 0;

        if (!ret_handle) {
            handler->cb->on_error(handler->opaque);
            return;
        }
    }
}

void red_channel_client_receive(RedChannelClient *rcc)
{
    red_channel_client_ref(rcc);
    red_peer_handle_incoming(rcc->stream, &rcc->incoming);
    red_channel_client_unref(rcc);
}

void red_channel_receive(RedChannel *channel)
{
    RingItem *link;
    RingItem *next;
    RedChannelClient *rcc;

    RING_FOREACH_SAFE(link, next, &channel->clients) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);
        red_channel_client_receive(rcc);
    }
}

static void red_peer_handle_outgoing(RedsStream *stream, OutgoingHandler *handler)
{
    ssize_t n;

    if (!stream) {
        return;
    }

    if (handler->size == 0) {
        handler->vec = handler->vec_buf;
        handler->size = handler->cb->get_msg_size(handler->opaque);
        if (!handler->size) {  // nothing to be sent
            return;
        }
    }

    for (;;) {
        handler->cb->prepare(handler->opaque, handler->vec, &handler->vec_size, handler->pos);
        n = reds_stream_writev(stream, handler->vec, handler->vec_size);
        if (n == -1) {
            switch (errno) {
            case EAGAIN:
                handler->cb->on_block(handler->opaque);
                return;
            case EINTR:
                continue;
            case EPIPE:
                handler->cb->on_error(handler->opaque);
                return;
            default:
                spice_printerr("%s", strerror(errno));
                handler->cb->on_error(handler->opaque);
                return;
            }
        } else {
            handler->pos += n;
            handler->cb->on_output(handler->opaque, n);
            if (handler->pos == handler->size) { // finished writing data
                /* reset handler before calling on_msg_done, since it
                 * can trigger another call to red_peer_handle_outgoing (when
                 * switching from the urgent marshaller to the main one */
                handler->vec = handler->vec_buf;
                handler->pos = 0;
                handler->size = 0;
                handler->cb->on_msg_done(handler->opaque);
                return;
            }
        }
    }
}

static void red_channel_client_on_output(void *opaque, int n)
{
    RedChannelClient *rcc = opaque;

    stat_inc_counter(rcc->channel->out_bytes_counter, n);
}

static void red_channel_client_default_peer_on_error(RedChannelClient *rcc)
{
    red_channel_client_disconnect(rcc);
}

static int red_channel_client_peer_get_out_msg_size(void *opaque)
{
    RedChannelClient *rcc = (RedChannelClient *)opaque;

    return rcc->send_data.size;
}

static void red_channel_client_peer_prepare_out_msg(
    void *opaque, struct iovec *vec, int *vec_size, int pos)
{
    RedChannelClient *rcc = (RedChannelClient *)opaque;

    *vec_size = spice_marshaller_fill_iovec(rcc->send_data.marshaller,
                                            vec, IOV_MAX, pos);
}

static void red_channel_client_peer_on_out_block(void *opaque)
{
    RedChannelClient *rcc = (RedChannelClient *)opaque;

    rcc->send_data.blocked = TRUE;
    rcc->channel->core->watch_update_mask(rcc->stream->watch,
                                     SPICE_WATCH_EVENT_READ |
                                     SPICE_WATCH_EVENT_WRITE);
}

static inline int red_channel_client_urgent_marshaller_is_active(RedChannelClient *rcc)
{
    return (rcc->send_data.marshaller == rcc->send_data.urgent.marshaller);
}

static void red_channel_client_reset_send_data(RedChannelClient *rcc)
{
    spice_marshaller_reset(rcc->send_data.marshaller);
    rcc->send_data.header.data = spice_marshaller_reserve_space(rcc->send_data.marshaller,
                                                                rcc->send_data.header.header_size);
    spice_marshaller_set_base(rcc->send_data.marshaller, rcc->send_data.header.header_size);
    rcc->send_data.header.set_msg_type(&rcc->send_data.header, 0);
    rcc->send_data.header.set_msg_size(&rcc->send_data.header, 0);

    /* Keeping the serial consecutive: reseting it if reset_send_data
     * has been called before, but no message has been sent since then.
     */
    if (rcc->send_data.last_sent_serial != rcc->send_data.serial) {
        spice_assert(rcc->send_data.serial - rcc->send_data.last_sent_serial == 1);
        /*  When the urgent marshaller is active, the serial was incremented by
         *  the call to reset_send_data that was made for the main marshaller.
         *  The urgent msg receives this serial, and the main msg serial is
         *  the following one. Thus, (rcc->send_data.serial - rcc->send_data.last_sent_serial)
         *  should be 1 in this case*/
        if (!red_channel_client_urgent_marshaller_is_active(rcc)) {
            rcc->send_data.serial = rcc->send_data.last_sent_serial;
        }
    }
    rcc->send_data.serial++;

    if (!rcc->is_mini_header) {
        spice_assert(rcc->send_data.marshaller != rcc->send_data.urgent.marshaller);
        rcc->send_data.header.set_msg_sub_list(&rcc->send_data.header, 0);
        rcc->send_data.header.set_msg_serial(&rcc->send_data.header, rcc->send_data.serial);
    }
}

void red_channel_client_push_set_ack(RedChannelClient *rcc)
{
    red_channel_client_pipe_add_type(rcc, PIPE_ITEM_TYPE_SET_ACK);
}

void red_channel_push_set_ack(RedChannel *channel)
{
    red_channel_pipes_add_type(channel, PIPE_ITEM_TYPE_SET_ACK);
}

static void red_channel_client_send_set_ack(RedChannelClient *rcc)
{
    SpiceMsgSetAck ack;

    spice_assert(rcc);
    red_channel_client_init_send_data(rcc, SPICE_MSG_SET_ACK, NULL);
    ack.generation = ++rcc->ack_data.generation;
    ack.window = rcc->ack_data.client_window;
    rcc->ack_data.messages_window = 0;

    spice_marshall_msg_set_ack(rcc->send_data.marshaller, &ack);

    red_channel_client_begin_send_message(rcc);
}

static void red_channel_client_send_migrate(RedChannelClient *rcc)
{
    SpiceMsgMigrate migrate;

    red_channel_client_init_send_data(rcc, SPICE_MSG_MIGRATE, NULL);
    migrate.flags = rcc->channel->migration_flags;
    spice_marshall_msg_migrate(rcc->send_data.marshaller, &migrate);
    if (rcc->channel->migration_flags & SPICE_MIGRATE_NEED_FLUSH) {
        rcc->wait_migrate_flush_mark = TRUE;
    }

    red_channel_client_begin_send_message(rcc);
}


static void red_channel_client_send_empty_msg(RedChannelClient *rcc, PipeItem *base)
{
    EmptyMsgPipeItem *msg_pipe_item = SPICE_CONTAINEROF(base, EmptyMsgPipeItem, base);

    red_channel_client_init_send_data(rcc, msg_pipe_item->msg, NULL);
    red_channel_client_begin_send_message(rcc);
}

static void red_channel_client_send_ping(RedChannelClient *rcc)
{
    SpiceMsgPing ping;
    struct timespec ts;

    if (!rcc->latency_monitor.warmup_was_sent) { // latency test start
        int delay_val;
        socklen_t opt_size = sizeof(delay_val);

        rcc->latency_monitor.warmup_was_sent = TRUE;
        /*
         * When testing latency, TCP_NODELAY must be switched on, otherwise,
         * sending the ping message is delayed by Nagle algorithm, and the
         * roundtrip measurment is less accurate (bigger).
         */
        rcc->latency_monitor.tcp_nodelay = 1;
        if (getsockopt(rcc->stream->socket, IPPROTO_TCP, TCP_NODELAY, &delay_val,
                       &opt_size) == -1) {
            spice_warning("getsockopt failed, %s", strerror(errno));
        }  else {
            rcc->latency_monitor.tcp_nodelay = delay_val;
            if (!delay_val) {
                delay_val = 1;
                if (setsockopt(rcc->stream->socket, IPPROTO_TCP, TCP_NODELAY, &delay_val,
                               sizeof(delay_val)) == -1) {
                   if (errno != ENOTSUP) {
                        spice_warning("setsockopt failed, %s", strerror(errno));
                    }
                }
            }
        }
    }

    red_channel_client_init_send_data(rcc, SPICE_MSG_PING, NULL);
    ping.id = rcc->latency_monitor.id;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ping.timestamp = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    spice_marshall_msg_ping(rcc->send_data.marshaller, &ping);
    red_channel_client_begin_send_message(rcc);
}

static void red_channel_client_send_item(RedChannelClient *rcc, PipeItem *item)
{
    int handled = TRUE;

    spice_assert(red_channel_client_no_item_being_sent(rcc));
    red_channel_client_reset_send_data(rcc);
    switch (item->type) {
        case PIPE_ITEM_TYPE_SET_ACK:
            red_channel_client_send_set_ack(rcc);
            free(item);
            break;
        case PIPE_ITEM_TYPE_MIGRATE:
            red_channel_client_send_migrate(rcc);
            free(item);
            break;
        case PIPE_ITEM_TYPE_EMPTY_MSG:
            red_channel_client_send_empty_msg(rcc, item);
            free(item);
            break;
        case PIPE_ITEM_TYPE_PING:
            red_channel_client_send_ping(rcc);
            free(item);
            break;
        default:
            handled = FALSE;
    }
    if (!handled) {
        rcc->channel->channel_cbs.send_item(rcc, item);
    }
}

static void red_channel_client_release_item(RedChannelClient *rcc, PipeItem *item, int item_pushed)
{
    int handled = TRUE;

    switch (item->type) {
        case PIPE_ITEM_TYPE_SET_ACK:
        case PIPE_ITEM_TYPE_EMPTY_MSG:
        case PIPE_ITEM_TYPE_MIGRATE:
        case PIPE_ITEM_TYPE_PING:
            free(item);
            break;
        default:
            handled = FALSE;
    }
    if (!handled) {
        rcc->channel->channel_cbs.release_item(rcc, item, item_pushed);
    }
}

static inline void red_channel_client_release_sent_item(RedChannelClient *rcc)
{
    if (rcc->send_data.item) {
        red_channel_client_release_item(rcc,
                                        rcc->send_data.item, TRUE);
        rcc->send_data.item = NULL;
    }
}

static void red_channel_peer_on_out_msg_done(void *opaque)
{
    RedChannelClient *rcc = (RedChannelClient *)opaque;

    rcc->send_data.size = 0;
    red_channel_client_release_sent_item(rcc);
    if (rcc->send_data.blocked) {
        rcc->send_data.blocked = FALSE;
        rcc->channel->core->watch_update_mask(rcc->stream->watch,
                                         SPICE_WATCH_EVENT_READ);
    }

    if (red_channel_client_urgent_marshaller_is_active(rcc)) {
        red_channel_client_restore_main_sender(rcc);
        spice_assert(rcc->send_data.header.data != NULL);
        red_channel_client_begin_send_message(rcc);
    } else {
        if (rcc->latency_monitor.timer && !rcc->send_data.blocked && rcc->pipe_size == 0) {
            /* It is possible that the socket will become idle, so we may be able to test latency */
            red_channel_client_restart_ping_timer(rcc);
        }
    }

}

static void red_channel_client_pipe_remove(RedChannelClient *rcc, PipeItem *item)
{
    rcc->pipe_size--;
    ring_remove(&item->link);
}

static void red_channel_add_client(RedChannel *channel, RedChannelClient *rcc)
{
    spice_assert(rcc);
    ring_add(&channel->clients, &rcc->channel_link);
    channel->clients_num++;
}

static void red_channel_client_set_remote_caps(RedChannelClient* rcc,
                                               int num_common_caps, uint32_t *common_caps,
                                               int num_caps, uint32_t *caps)
{
    rcc->remote_caps.num_common_caps = num_common_caps;
    rcc->remote_caps.common_caps = spice_memdup(common_caps, num_common_caps * sizeof(uint32_t));

    rcc->remote_caps.num_caps = num_caps;
    rcc->remote_caps.caps = spice_memdup(caps, num_caps * sizeof(uint32_t));
}

static void red_channel_client_destroy_remote_caps(RedChannelClient* rcc)
{
    rcc->remote_caps.num_common_caps = 0;
    free(rcc->remote_caps.common_caps);
    rcc->remote_caps.num_caps = 0;
    free(rcc->remote_caps.caps);
}

int red_channel_client_test_remote_common_cap(RedChannelClient *rcc, uint32_t cap)
{
    return test_capabilty(rcc->remote_caps.common_caps,
                          rcc->remote_caps.num_common_caps,
                          cap);
}

int red_channel_client_test_remote_cap(RedChannelClient *rcc, uint32_t cap)
{
    return test_capabilty(rcc->remote_caps.caps,
                          rcc->remote_caps.num_caps,
                          cap);
}

int red_channel_test_remote_common_cap(RedChannel *channel, uint32_t cap)
{
    RingItem *link;

    RING_FOREACH(link, &channel->clients) {
        RedChannelClient *rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);

        if (!red_channel_client_test_remote_common_cap(rcc, cap)) {
            return FALSE;
        }
    }
    return TRUE;
}

int red_channel_test_remote_cap(RedChannel *channel, uint32_t cap)
{
    RingItem *link;

    RING_FOREACH(link, &channel->clients) {
        RedChannelClient *rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);

        if (!red_channel_client_test_remote_cap(rcc, cap)) {
            return FALSE;
        }
    }
    return TRUE;
}

static int red_channel_client_pre_create_validate(RedChannel *channel, RedClient  *client)
{
    if (red_client_get_channel(client, channel->type, channel->id)) {
        spice_printerr("Error client %p: duplicate channel type %d id %d",
                       client, channel->type, channel->id);
        return FALSE;
    }
    return TRUE;
}

static void red_channel_client_push_ping(RedChannelClient *rcc)
{
    spice_assert(rcc->latency_monitor.state == PING_STATE_NONE);
    rcc->latency_monitor.state = PING_STATE_WARMUP;
    rcc->latency_monitor.warmup_was_sent = FALSE;
    rcc->latency_monitor.id = rand();
    red_channel_client_pipe_add_type(rcc, PIPE_ITEM_TYPE_PING);
    red_channel_client_pipe_add_type(rcc, PIPE_ITEM_TYPE_PING);
}

static void red_channel_client_ping_timer(void *opaque)
{
    int so_unsent_size = 0;
    RedChannelClient *rcc = opaque;

    spice_assert(rcc->latency_monitor.state == PING_STATE_TIMER);
    red_channel_client_cancel_ping_timer(rcc);
    /* retrieving the occupied size of the socket's tcp snd buffer (unacked + unsent) */
    if (ioctl(rcc->stream->socket, TIOCOUTQ, &so_unsent_size) == -1) {
        spice_printerr("ioctl(TIOCOUTQ) failed, %s", strerror(errno));
    }
    if (so_unsent_size > 0) {
        /* tcp snd buffer is still occupied. rescheduling ping */
        red_channel_client_start_ping_timer(rcc, PING_TEST_IDLE_NET_TIMEOUT_MS);
    } else {
        red_channel_client_push_ping(rcc);
    }
}

RedChannelClient *red_channel_client_create(int size, RedChannel *channel, RedClient  *client,
                                            RedsStream *stream,
                                            int monitor_latency,
                                            int num_common_caps, uint32_t *common_caps,
                                            int num_caps, uint32_t *caps)
{
    RedChannelClient *rcc = NULL;

    pthread_mutex_lock(&client->lock);
    if (!red_channel_client_pre_create_validate(channel, client)) {
        goto error;
    }
    spice_assert(stream && channel && size >= sizeof(RedChannelClient));
    rcc = spice_malloc0(size);
    rcc->stream = stream;
    rcc->channel = channel;
    rcc->client = client;
    rcc->refs = 1;
    rcc->ack_data.messages_window = ~0;  // blocks send message (maybe use send_data.blocked +
                                             // block flags)
    rcc->ack_data.client_generation = ~0;
    rcc->ack_data.client_window = CLIENT_ACK_WINDOW;
    rcc->send_data.main.marshaller = spice_marshaller_new();
    rcc->send_data.urgent.marshaller = spice_marshaller_new();

    rcc->send_data.marshaller = rcc->send_data.main.marshaller;

    rcc->incoming.opaque = rcc;
    rcc->incoming.cb = &channel->incoming_cb;

    rcc->outgoing.opaque = rcc;
    rcc->outgoing.cb = &channel->outgoing_cb;
    rcc->outgoing.pos = 0;
    rcc->outgoing.size = 0;

    red_channel_client_set_remote_caps(rcc, num_common_caps, common_caps, num_caps, caps);
    if (red_channel_client_test_remote_common_cap(rcc, SPICE_COMMON_CAP_MINI_HEADER)) {
        rcc->incoming.header = mini_header_wrapper;
        rcc->send_data.header = mini_header_wrapper;
        rcc->is_mini_header = TRUE;
    } else {
        rcc->incoming.header = full_header_wrapper;
        rcc->send_data.header = full_header_wrapper;
        rcc->is_mini_header = FALSE;
    }

    rcc->incoming.header.data = rcc->incoming.header_buf;
    rcc->incoming.serial = 1;

    if (!channel->channel_cbs.config_socket(rcc)) {
        goto error;
    }

    ring_init(&rcc->pipe);
    rcc->pipe_size = 0;

    stream->watch = channel->core->watch_add(stream->socket,
                                           SPICE_WATCH_EVENT_READ,
                                           red_channel_client_event, rcc);
    rcc->id = channel->clients_num;
    red_channel_add_client(channel, rcc);
    red_client_add_channel(client, rcc);
    red_channel_ref(channel);
    pthread_mutex_unlock(&client->lock);

    if (monitor_latency) {
        rcc->latency_monitor.timer = channel->core->timer_add(
            red_channel_client_ping_timer, rcc);
        if (!client->during_target_migrate) {
            red_channel_client_start_ping_timer(rcc, PING_TEST_IDLE_NET_TIMEOUT_MS);
        }
        rcc->latency_monitor.roundtrip = -1;
    }

    return rcc;
error:
    free(rcc);
    reds_stream_free(stream);
    pthread_mutex_unlock(&client->lock);
    return NULL;
}

static void red_channel_client_seamless_migration_done(RedChannelClient *rcc)
{
    rcc->wait_migrate_data = FALSE;

    pthread_mutex_lock(&rcc->client->lock);
    rcc->client->num_migrated_channels--;

    /* we assume we always have at least one channel who has migration data transfer,
     * otherwise, this flag will never be set back to FALSE*/
    if (!rcc->client->num_migrated_channels) {
        rcc->client->during_target_migrate = FALSE;
        rcc->client->seamless_migrate = FALSE;
        /* migration completion might have been triggered from a different thread
         * than the main thread */
        main_dispatcher_seamless_migrate_dst_complete(rcc->client);
        if (rcc->latency_monitor.timer) {
            red_channel_client_start_ping_timer(rcc, PING_TEST_IDLE_NET_TIMEOUT_MS);
        }
    }
    pthread_mutex_unlock(&rcc->client->lock);
}

int red_channel_client_waits_for_migrate_data(RedChannelClient *rcc)
{
    return rcc->wait_migrate_data;
}

int red_channel_waits_for_migrate_data(RedChannel *channel)
{
    RedChannelClient *rcc;

    if (!red_channel_is_connected(channel)) {
        return FALSE;
    }

    if (channel->clients_num > 1) {
        return FALSE;
    }
    spice_assert(channel->clients_num == 1);
    rcc = SPICE_CONTAINEROF(ring_get_head(&channel->clients), RedChannelClient, channel_link);
    return red_channel_client_waits_for_migrate_data(rcc);
}

static void red_channel_client_default_connect(RedChannel *channel, RedClient *client,
                                               RedsStream *stream,
                                               int migration,
                                               int num_common_caps, uint32_t *common_caps,
                                               int num_caps, uint32_t *caps)
{
    spice_error("not implemented");
}

static void red_channel_client_default_disconnect(RedChannelClient *base)
{
    red_channel_client_disconnect(base);
}

void red_channel_client_default_migrate(RedChannelClient *rcc)
{
    if (rcc->latency_monitor.timer) {
        red_channel_client_cancel_ping_timer(rcc);
        rcc->channel->core->timer_remove(rcc->latency_monitor.timer);
        rcc->latency_monitor.timer = NULL;
    }
    red_channel_client_pipe_add_type(rcc, PIPE_ITEM_TYPE_MIGRATE);
}

RedChannel *red_channel_create(int size,
                               SpiceCoreInterface *core,
                               uint32_t type, uint32_t id,
                               int handle_acks,
                               channel_handle_message_proc handle_message,
                               ChannelCbs *channel_cbs,
                               uint32_t migration_flags)
{
    RedChannel *channel;
    ClientCbs client_cbs = { NULL, };

    spice_assert(size >= sizeof(*channel));
    spice_assert(channel_cbs->config_socket && channel_cbs->on_disconnect && handle_message &&
           channel_cbs->alloc_recv_buf && channel_cbs->release_item);
    spice_assert(channel_cbs->handle_migrate_data ||
                 !(migration_flags & SPICE_MIGRATE_NEED_DATA_TRANSFER));
    channel = spice_malloc0(size);
    channel->type = type;
    channel->id = id;
    channel->refs = 1;
    channel->handle_acks = handle_acks;
    channel->migration_flags = migration_flags;
    memcpy(&channel->channel_cbs, channel_cbs, sizeof(ChannelCbs));

    channel->core = core;
    ring_init(&channel->clients);

    // TODO: send incoming_cb as parameters instead of duplicating?
    channel->incoming_cb.alloc_msg_buf = (alloc_msg_recv_buf_proc)channel_cbs->alloc_recv_buf;
    channel->incoming_cb.release_msg_buf = (release_msg_recv_buf_proc)channel_cbs->release_recv_buf;
    channel->incoming_cb.handle_message = (handle_message_proc)handle_message;
    channel->incoming_cb.on_error =
        (on_incoming_error_proc)red_channel_client_default_peer_on_error;
    channel->outgoing_cb.get_msg_size = red_channel_client_peer_get_out_msg_size;
    channel->outgoing_cb.prepare = red_channel_client_peer_prepare_out_msg;
    channel->outgoing_cb.on_block = red_channel_client_peer_on_out_block;
    channel->outgoing_cb.on_error =
        (on_outgoing_error_proc)red_channel_client_default_peer_on_error;
    channel->outgoing_cb.on_msg_done = red_channel_peer_on_out_msg_done;
    channel->outgoing_cb.on_output = red_channel_client_on_output;

    client_cbs.connect = red_channel_client_default_connect;
    client_cbs.disconnect = red_channel_client_default_disconnect;
    client_cbs.migrate = red_channel_client_default_migrate;

    red_channel_register_client_cbs(channel, &client_cbs);
    red_channel_set_common_cap(channel, SPICE_COMMON_CAP_MINI_HEADER);

    channel->thread_id = pthread_self();

    channel->out_bytes_counter = 0;

    spice_debug("channel type %d id %d thread_id 0x%lx",
                channel->type, channel->id, channel->thread_id);
    return channel;
}

// TODO: red_worker can use this one
static void dummy_watch_update_mask(SpiceWatch *watch, int event_mask)
{
}

static SpiceWatch *dummy_watch_add(int fd, int event_mask, SpiceWatchFunc func, void *opaque)
{
    return NULL; // apparently allowed?
}

static void dummy_watch_remove(SpiceWatch *watch)
{
}

// TODO: actually, since I also use channel_client_dummym, no need for core. Can be NULL
SpiceCoreInterface dummy_core = {
    .watch_update_mask = dummy_watch_update_mask,
    .watch_add = dummy_watch_add,
    .watch_remove = dummy_watch_remove,
};

RedChannel *red_channel_create_dummy(int size, uint32_t type, uint32_t id)
{
    RedChannel *channel;
    ClientCbs client_cbs = { NULL, };

    spice_assert(size >= sizeof(*channel));
    channel = spice_malloc0(size);
    channel->type = type;
    channel->id = id;
    channel->refs = 1;
    channel->core = &dummy_core;
    ring_init(&channel->clients);
    client_cbs.connect = red_channel_client_default_connect;
    client_cbs.disconnect = red_channel_client_default_disconnect;
    client_cbs.migrate = red_channel_client_default_migrate;

    red_channel_register_client_cbs(channel, &client_cbs);
    red_channel_set_common_cap(channel, SPICE_COMMON_CAP_MINI_HEADER);

    channel->thread_id = pthread_self();
    spice_debug("channel type %d id %d thread_id 0x%lx",
                channel->type, channel->id, channel->thread_id);

    channel->out_bytes_counter = 0;

    return channel;
}

static int do_nothing_handle_message(RedChannelClient *rcc,
                                     uint16_t type,
                                     uint32_t size,
                                     uint8_t *msg)
{
    return TRUE;
}

RedChannel *red_channel_create_parser(int size,
                               SpiceCoreInterface *core,
                               uint32_t type, uint32_t id,
                               int handle_acks,
                               spice_parse_channel_func_t parser,
                               channel_handle_parsed_proc handle_parsed,
                               ChannelCbs *channel_cbs,
                               uint32_t migration_flags)
{
    RedChannel *channel = red_channel_create(size, core, type, id,
                                             handle_acks,
                                             do_nothing_handle_message,
                                             channel_cbs,
                                             migration_flags);

    if (channel == NULL) {
        return NULL;
    }
    channel->incoming_cb.handle_parsed = (handle_parsed_proc)handle_parsed;
    channel->incoming_cb.parser = parser;
    return channel;
}

void red_channel_register_client_cbs(RedChannel *channel, ClientCbs *client_cbs)
{
    spice_assert(client_cbs->connect || channel->type == SPICE_CHANNEL_MAIN);
    channel->client_cbs.connect = client_cbs->connect;

    if (client_cbs->disconnect) {
        channel->client_cbs.disconnect = client_cbs->disconnect;
    }

    if (client_cbs->migrate) {
        channel->client_cbs.migrate = client_cbs->migrate;
    }
}

int test_capabilty(uint32_t *caps, int num_caps, uint32_t cap)
{
    uint32_t index = cap / 32;
    if (num_caps < index + 1) {
        return FALSE;
    }

    return (caps[index] & (1 << (cap % 32))) != 0;
}

static void add_capability(uint32_t **caps, int *num_caps, uint32_t cap)
{
    int nbefore, n;

    nbefore = *num_caps;
    n = cap / 32;
    *num_caps = MAX(*num_caps, n + 1);
    *caps = spice_renew(uint32_t, *caps, *num_caps);
    memset(*caps + nbefore, 0, (*num_caps - nbefore) * sizeof(uint32_t));
    (*caps)[n] |= (1 << (cap % 32));
}

void red_channel_set_common_cap(RedChannel *channel, uint32_t cap)
{
    add_capability(&channel->local_caps.common_caps, &channel->local_caps.num_common_caps, cap);
}

void red_channel_set_cap(RedChannel *channel, uint32_t cap)
{
    add_capability(&channel->local_caps.caps, &channel->local_caps.num_caps, cap);
}

void red_channel_set_data(RedChannel *channel, void *data)
{
    spice_assert(channel);
    channel->data = data;
}

static void red_channel_ref(RedChannel *channel)
{
    channel->refs++;
}

static void red_channel_unref(RedChannel *channel)
{
    if (!--channel->refs) {
        if (channel->local_caps.num_common_caps) {
            free(channel->local_caps.common_caps);
        }

        if (channel->local_caps.num_caps) {
            free(channel->local_caps.caps);
        }

        free(channel);
    }
}

static void red_channel_client_ref(RedChannelClient *rcc)
{
    rcc->refs++;
}

static void red_channel_client_unref(RedChannelClient *rcc)
{
    if (!--rcc->refs) {
        if (rcc->send_data.main.marshaller) {
            spice_marshaller_destroy(rcc->send_data.main.marshaller);
        }

        if (rcc->send_data.urgent.marshaller) {
            spice_marshaller_destroy(rcc->send_data.urgent.marshaller);
        }

        red_channel_client_destroy_remote_caps(rcc);
        if (rcc->channel) {
            red_channel_unref(rcc->channel);
        }
        free(rcc);
    }
}

void red_channel_client_destroy(RedChannelClient *rcc)
{
    rcc->destroying = 1;
    red_channel_client_disconnect(rcc);
    red_client_remove_channel(rcc);
    red_channel_client_unref(rcc);
}

void red_channel_destroy(RedChannel *channel)
{
    RingItem *link;
    RingItem *next;

    if (!channel) {
        return;
    }
    RING_FOREACH_SAFE(link, next, &channel->clients) {
        red_channel_client_destroy(
            SPICE_CONTAINEROF(link, RedChannelClient, channel_link));
    }

    red_channel_unref(channel);
}

void red_channel_client_shutdown(RedChannelClient *rcc)
{
    if (rcc->stream && !rcc->stream->shutdown) {
        rcc->channel->core->watch_remove(rcc->stream->watch);
        rcc->stream->watch = NULL;
        shutdown(rcc->stream->socket, SHUT_RDWR);
        rcc->stream->shutdown = TRUE;
    }
}

void red_channel_client_send(RedChannelClient *rcc)
{
    red_channel_client_ref(rcc);
    red_peer_handle_outgoing(rcc->stream, &rcc->outgoing);
    red_channel_client_unref(rcc);
}

void red_channel_send(RedChannel *channel)
{
    RingItem *link;
    RingItem *next;

    RING_FOREACH_SAFE(link, next, &channel->clients) {
        red_channel_client_send(SPICE_CONTAINEROF(link, RedChannelClient, channel_link));
    }
}

static inline int red_channel_client_waiting_for_ack(RedChannelClient *rcc)
{
    return (rcc->channel->handle_acks &&
            (rcc->ack_data.messages_window > rcc->ack_data.client_window * 2));
}

static inline PipeItem *red_channel_client_pipe_item_get(RedChannelClient *rcc)
{
    PipeItem *item;

    if (!rcc || rcc->send_data.blocked
             || red_channel_client_waiting_for_ack(rcc)
             || !(item = (PipeItem *)ring_get_tail(&rcc->pipe))) {
        return NULL;
    }
    red_channel_client_pipe_remove(rcc, item);
    return item;
}

void red_channel_client_push(RedChannelClient *rcc)
{
    PipeItem *pipe_item;

    if (!rcc->during_send) {
        rcc->during_send = TRUE;
    } else {
        return;
    }
    red_channel_client_ref(rcc);
    if (rcc->send_data.blocked) {
        red_channel_client_send(rcc);
    }

    if (!red_channel_client_no_item_being_sent(rcc) && !rcc->send_data.blocked) {
        rcc->send_data.blocked = TRUE;
        spice_printerr("ERROR: an item waiting to be sent and not blocked");
    }

    while ((pipe_item = red_channel_client_pipe_item_get(rcc))) {
        red_channel_client_send_item(rcc, pipe_item);
    }
    rcc->during_send = FALSE;
    red_channel_client_unref(rcc);
}

void red_channel_push(RedChannel *channel)
{
    RingItem *link;
    RingItem *next;
    RedChannelClient *rcc;

    if (!channel) {
        return;
    }
    RING_FOREACH_SAFE(link, next, &channel->clients) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);
        red_channel_client_push(rcc);
    }
}

int red_channel_client_get_roundtrip_ms(RedChannelClient *rcc)
{
    if (rcc->latency_monitor.roundtrip < 0) {
        return rcc->latency_monitor.roundtrip;
    }
    return rcc->latency_monitor.roundtrip / 1000 / 1000;
}

static void red_channel_client_init_outgoing_messages_window(RedChannelClient *rcc)
{
    rcc->ack_data.messages_window = 0;
    red_channel_client_push(rcc);
}

// TODO: this function doesn't make sense because the window should be client (WAN/LAN)
// specific
void red_channel_init_outgoing_messages_window(RedChannel *channel)
{
    RingItem *link;
    RingItem *next;

    RING_FOREACH_SAFE(link, next, &channel->clients) {
        red_channel_client_init_outgoing_messages_window(
            SPICE_CONTAINEROF(link, RedChannelClient, channel_link));
    }
}

static void red_channel_handle_migrate_flush_mark(RedChannelClient *rcc)
{
    if (rcc->channel->channel_cbs.handle_migrate_flush_mark) {
        rcc->channel->channel_cbs.handle_migrate_flush_mark(rcc);
    }
}

// TODO: the whole migration is broken with multiple clients. What do we want to do?
// basically just
//  1) source send mark to all
//  2) source gets at various times the data (waits for all)
//  3) source migrates to target
//  4) target sends data to all
// So need to make all the handlers work with per channel/client data (what data exactly?)
static void red_channel_handle_migrate_data(RedChannelClient *rcc, uint32_t size, void *message)
{
    spice_debug("channel type %d id %d rcc %p size %u",
                rcc->channel->type, rcc->channel->id, rcc, size);
    if (!rcc->channel->channel_cbs.handle_migrate_data) {
        return;
    }
    if (!red_channel_client_waits_for_migrate_data(rcc)) {
        spice_channel_client_error(rcc, "unexpected");
        return;
    }
    if (rcc->channel->channel_cbs.handle_migrate_data_get_serial) {
        red_channel_client_set_message_serial(rcc,
            rcc->channel->channel_cbs.handle_migrate_data_get_serial(rcc, size, message));
    }
    if (!rcc->channel->channel_cbs.handle_migrate_data(rcc, size, message)) {
        spice_channel_client_error(rcc, "handle_migrate_data failed");
        return;
    }
    red_channel_client_seamless_migration_done(rcc);
}

static void red_channel_client_restart_ping_timer(RedChannelClient *rcc)
{
    struct timespec ts;
    uint64_t passed, timeout;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    passed = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    passed = passed - rcc->latency_monitor.last_pong_time;
    passed /= 1000*1000;
    timeout = PING_TEST_IDLE_NET_TIMEOUT_MS;
    if (passed  < PING_TEST_TIMEOUT_MS) {
        timeout += PING_TEST_TIMEOUT_MS - passed;
    }

    red_channel_client_start_ping_timer(rcc, timeout);
}

static void red_channel_client_start_ping_timer(RedChannelClient *rcc, uint32_t timeout)
{
    if (!rcc->latency_monitor.timer) {
        return;
    }
    if (rcc->latency_monitor.state != PING_STATE_NONE) {
        return;
    }
    rcc->latency_monitor.state = PING_STATE_TIMER;
    rcc->channel->core->timer_start(rcc->latency_monitor.timer, timeout);
}

static void red_channel_client_cancel_ping_timer(RedChannelClient *rcc)
{
    if (!rcc->latency_monitor.timer) {
        return;
    }
    if (rcc->latency_monitor.state != PING_STATE_TIMER) {
        return;
    }

    rcc->channel->core->timer_cancel(rcc->latency_monitor.timer);
    rcc->latency_monitor.state = PING_STATE_NONE;
}

static void red_channel_client_handle_pong(RedChannelClient *rcc, SpiceMsgPing *ping)
{
    uint64_t now;
    struct timespec ts;

    /* ignoring unexpected pongs, or post-migration pongs for pings that
     * started just before migration */
    if (ping->id != rcc->latency_monitor.id) {
        spice_warning("ping-id (%u)!= pong-id %u",
                      rcc->latency_monitor.id, ping->id);
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);
    now =  ts.tv_sec * 1000000000LL + ts.tv_nsec;

    if (rcc->latency_monitor.state == PING_STATE_WARMUP) {
        rcc->latency_monitor.state = PING_STATE_LATENCY;
        return;
    } else if (rcc->latency_monitor.state != PING_STATE_LATENCY) {
        spice_warning("unexpected");
        return;
    }

    /* set TCO_NODELAY=0, in case we reverted it for the test*/
    if (!rcc->latency_monitor.tcp_nodelay) {
        int delay_val = 0;

        if (setsockopt(rcc->stream->socket, IPPROTO_TCP, TCP_NODELAY, &delay_val,
                       sizeof(delay_val)) == -1) {
            if (errno != ENOTSUP) {
                spice_warning("setsockopt failed, %s", strerror(errno));
            }
        }
    }

    /*
     * The real network latency shouldn't change during the connection. However,
     *  the measurements can be bigger than the real roundtrip due to other
     *  threads or processes that are utilizing the network. We update the roundtrip
     *  measurement with the minimal value we encountered till now.
     */
    if (rcc->latency_monitor.roundtrip < 0 ||
        now - ping->timestamp < rcc->latency_monitor.roundtrip) {
        rcc->latency_monitor.roundtrip = now - ping->timestamp;
        spice_debug("update roundtrip %.2f(ms)", rcc->latency_monitor.roundtrip/1000.0/1000.0);
    }

    rcc->latency_monitor.last_pong_time = now;
    rcc->latency_monitor.state = PING_STATE_NONE;
    red_channel_client_start_ping_timer(rcc, PING_TEST_TIMEOUT_MS);
}

int red_channel_client_handle_message(RedChannelClient *rcc, uint32_t size,
                                      uint16_t type, void *message)
{
    switch (type) {
    case SPICE_MSGC_ACK_SYNC:
        if (size != sizeof(uint32_t)) {
            spice_printerr("bad message size");
            return FALSE;
        }
        rcc->ack_data.client_generation = *(uint32_t *)(message);
        break;
    case SPICE_MSGC_ACK:
        if (rcc->ack_data.client_generation == rcc->ack_data.generation) {
            rcc->ack_data.messages_window -= rcc->ack_data.client_window;
            red_channel_client_push(rcc);
        }
        break;
    case SPICE_MSGC_DISCONNECTING:
        break;
    case SPICE_MSGC_MIGRATE_FLUSH_MARK:
        if (!rcc->wait_migrate_flush_mark) {
            spice_error("unexpected flush mark");
            return FALSE;
        }
        red_channel_handle_migrate_flush_mark(rcc);
        rcc->wait_migrate_flush_mark = FALSE;
        break;
    case SPICE_MSGC_MIGRATE_DATA:
        red_channel_handle_migrate_data(rcc, size, message);
        break;
    case SPICE_MSGC_PONG:
        red_channel_client_handle_pong(rcc, message);
        break;
    default:
        spice_printerr("invalid message type %u", type);
        return FALSE;
    }
    return TRUE;
}

static void red_channel_client_event(int fd, int event, void *data)
{
    RedChannelClient *rcc = (RedChannelClient *)data;

    red_channel_client_ref(rcc);
    if (event & SPICE_WATCH_EVENT_READ) {
        red_channel_client_receive(rcc);
    }
    if (event & SPICE_WATCH_EVENT_WRITE) {
        red_channel_client_push(rcc);
    }
    red_channel_client_unref(rcc);
}

void red_channel_client_init_send_data(RedChannelClient *rcc, uint16_t msg_type, PipeItem *item)
{
    spice_assert(red_channel_client_no_item_being_sent(rcc));
    spice_assert(msg_type != 0);
    rcc->send_data.header.set_msg_type(&rcc->send_data.header, msg_type);
    rcc->send_data.item = item;
    if (item) {
        rcc->channel->channel_cbs.hold_item(rcc, item);
    }
}

void red_channel_client_begin_send_message(RedChannelClient *rcc)
{
    SpiceMarshaller *m = rcc->send_data.marshaller;

    // TODO - better check: type in channel_allowed_types. Better: type in channel_allowed_types(channel_state)
    if (rcc->send_data.header.get_msg_type(&rcc->send_data.header) == 0) {
        spice_printerr("BUG: header->type == 0");
        return;
    }

    /* canceling the latency test timer till the nework is idle */
    red_channel_client_cancel_ping_timer(rcc);

    spice_marshaller_flush(m);
    rcc->send_data.size = spice_marshaller_get_total_size(m);
    rcc->send_data.header.set_msg_size(&rcc->send_data.header,
                                       rcc->send_data.size - rcc->send_data.header.header_size);
    rcc->ack_data.messages_window++;
    rcc->send_data.last_sent_serial = rcc->send_data.serial;
    rcc->send_data.header.data = NULL; /* avoid writing to this until we have a new message */
    red_channel_client_send(rcc);
}

SpiceMarshaller *red_channel_client_switch_to_urgent_sender(RedChannelClient *rcc)
{
    spice_assert(red_channel_client_no_item_being_sent(rcc));
    spice_assert(rcc->send_data.header.data != NULL);
    rcc->send_data.main.header_data = rcc->send_data.header.data;
    rcc->send_data.main.item = rcc->send_data.item;

    rcc->send_data.marshaller = rcc->send_data.urgent.marshaller;
    rcc->send_data.item = NULL;
    red_channel_client_reset_send_data(rcc);
    return rcc->send_data.marshaller;
}

static void red_channel_client_restore_main_sender(RedChannelClient *rcc)
{
    spice_marshaller_reset(rcc->send_data.urgent.marshaller);
    rcc->send_data.marshaller = rcc->send_data.main.marshaller;
    rcc->send_data.header.data = rcc->send_data.main.header_data;
    if (!rcc->is_mini_header) {
        rcc->send_data.header.set_msg_serial(&rcc->send_data.header, rcc->send_data.serial);
    }
    rcc->send_data.item = rcc->send_data.main.item;
}

uint64_t red_channel_client_get_message_serial(RedChannelClient *rcc)
{
    return rcc->send_data.serial;
}

void red_channel_client_set_message_serial(RedChannelClient *rcc, uint64_t serial)
{
    rcc->send_data.last_sent_serial = serial;
    rcc->send_data.serial = serial;
}

void red_channel_pipe_item_init(RedChannel *channel, PipeItem *item, int type)
{
    ring_item_init(&item->link);
    item->type = type;
}

void red_channel_client_pipe_add(RedChannelClient *rcc, PipeItem *item)
{
    spice_assert(rcc && item);
    rcc->pipe_size++;
    ring_add(&rcc->pipe, &item->link);
}

void red_channel_client_pipe_add_push(RedChannelClient *rcc, PipeItem *item)
{
    red_channel_client_pipe_add(rcc, item);
    red_channel_client_push(rcc);
}

void red_channel_client_pipe_add_after(RedChannelClient *rcc,
                                       PipeItem *item, PipeItem *pos)
{
    spice_assert(rcc);
    spice_assert(pos);
    spice_assert(item);

    rcc->pipe_size++;
    ring_add_after(&item->link, &pos->link);
}

int red_channel_client_pipe_item_is_linked(RedChannelClient *rcc,
                                           PipeItem *item)
{
    return ring_item_is_linked(&item->link);
}

void red_channel_client_pipe_add_tail_no_push(RedChannelClient *rcc,
                                              PipeItem *item)
{
    spice_assert(rcc);
    rcc->pipe_size++;
    ring_add_before(&item->link, &rcc->pipe);
}

void red_channel_client_pipe_add_tail(RedChannelClient *rcc, PipeItem *item)
{
    spice_assert(rcc);
    rcc->pipe_size++;
    ring_add_before(&item->link, &rcc->pipe);
    red_channel_client_push(rcc);
}

void red_channel_client_pipe_add_type(RedChannelClient *rcc, int pipe_item_type)
{
    PipeItem *item = spice_new(PipeItem, 1);

    red_channel_pipe_item_init(rcc->channel, item, pipe_item_type);
    red_channel_client_pipe_add(rcc, item);
    red_channel_client_push(rcc);
}

void red_channel_pipes_add_type(RedChannel *channel, int pipe_item_type)
{
    RingItem *link, *next;

    RING_FOREACH_SAFE(link, next, &channel->clients) {
        red_channel_client_pipe_add_type(
            SPICE_CONTAINEROF(link, RedChannelClient, channel_link),
            pipe_item_type);
    }
}

void red_channel_client_pipe_add_empty_msg(RedChannelClient *rcc, int msg_type)
{
    EmptyMsgPipeItem *item = spice_new(EmptyMsgPipeItem, 1);

    red_channel_pipe_item_init(rcc->channel, &item->base, PIPE_ITEM_TYPE_EMPTY_MSG);
    item->msg = msg_type;
    red_channel_client_pipe_add(rcc, &item->base);
    red_channel_client_push(rcc);
}

void red_channel_pipes_add_empty_msg(RedChannel *channel, int msg_type)
{
    RingItem *link, *next;

    RING_FOREACH_SAFE(link, next, &channel->clients) {
        red_channel_client_pipe_add_empty_msg(
            SPICE_CONTAINEROF(link, RedChannelClient, channel_link),
            msg_type);
    }
}

int red_channel_client_is_connected(RedChannelClient *rcc)
{
    if (!rcc->dummy) {
        return rcc->stream != NULL;
    } else {
        return rcc->dummy_connected;
    }
}

int red_channel_is_connected(RedChannel *channel)
{
    return channel && (channel->clients_num > 0);
}

void red_channel_client_clear_sent_item(RedChannelClient *rcc)
{
    if (rcc->send_data.item) {
        red_channel_client_release_item(rcc, rcc->send_data.item, TRUE);
        rcc->send_data.item = NULL;
    }
    rcc->send_data.blocked = FALSE;
    rcc->send_data.size = 0;
}

void red_channel_client_pipe_clear(RedChannelClient *rcc)
{
    PipeItem *item;

    if (rcc) {
        red_channel_client_clear_sent_item(rcc);
    }
    while ((item = (PipeItem *)ring_get_head(&rcc->pipe))) {
        ring_remove(&item->link);
        red_channel_client_release_item(rcc, item, FALSE);
    }
    rcc->pipe_size = 0;
}

void red_channel_client_ack_zero_messages_window(RedChannelClient *rcc)
{
    rcc->ack_data.messages_window = 0;
}

void red_channel_client_ack_set_client_window(RedChannelClient *rcc, int client_window)
{
    rcc->ack_data.client_window = client_window;
}

static void red_channel_remove_client(RedChannelClient *rcc)
{
    if (!pthread_equal(pthread_self(), rcc->channel->thread_id)) {
        spice_warning("channel type %d id %d - "
                      "channel->thread_id (0x%lx) != pthread_self (0x%lx)."
                      "If one of the threads is != io-thread && != vcpu-thread, "
                      "this might be a BUG",
                      rcc->channel->type, rcc->channel->id,
                      rcc->channel->thread_id, pthread_self());
    }
    ring_remove(&rcc->channel_link);
    spice_assert(rcc->channel->clients_num > 0);
    rcc->channel->clients_num--;
    // TODO: should we set rcc->channel to NULL???
}

static void red_client_remove_channel(RedChannelClient *rcc)
{
    pthread_mutex_lock(&rcc->client->lock);
    ring_remove(&rcc->client_link);
    rcc->client->channels_num--;
    pthread_mutex_unlock(&rcc->client->lock);
}

static void red_channel_client_disconnect_dummy(RedChannelClient *rcc)
{
    spice_assert(rcc->dummy);
    if (ring_item_is_linked(&rcc->channel_link)) {
        red_channel_remove_client(rcc);
    }
    rcc->dummy_connected = FALSE;
}

void red_channel_client_disconnect(RedChannelClient *rcc)
{
    spice_printerr("%p (channel %p type %d id %d)", rcc, rcc->channel,
                                                rcc->channel->type, rcc->channel->id);
    if (rcc->dummy) {
        red_channel_client_disconnect_dummy(rcc);
        return;
    }
    if (!red_channel_client_is_connected(rcc)) {
        return;
    }
    red_channel_client_pipe_clear(rcc);
    if (rcc->stream->watch) {
        rcc->channel->core->watch_remove(rcc->stream->watch);
        rcc->stream->watch = NULL;
    }
    reds_stream_free(rcc->stream);
    rcc->stream = NULL;
    if (rcc->latency_monitor.timer) {
        rcc->channel->core->timer_remove(rcc->latency_monitor.timer);
        rcc->latency_monitor.timer = NULL;
    }
    red_channel_remove_client(rcc);
    rcc->channel->channel_cbs.on_disconnect(rcc);
}

void red_channel_disconnect(RedChannel *channel)
{
    RingItem *link;
    RingItem *next;

    RING_FOREACH_SAFE(link, next, &channel->clients) {
        red_channel_client_disconnect(
            SPICE_CONTAINEROF(link, RedChannelClient, channel_link));
    }
}

RedChannelClient *red_channel_client_create_dummy(int size,
                                                  RedChannel *channel,
                                                  RedClient  *client,
                                                  int num_common_caps, uint32_t *common_caps,
                                                  int num_caps, uint32_t *caps)
{
    RedChannelClient *rcc = NULL;

    spice_assert(size >= sizeof(RedChannelClient));

    pthread_mutex_lock(&client->lock);
    if (!red_channel_client_pre_create_validate(channel, client)) {
        goto error;
    }
    rcc = spice_malloc0(size);
    rcc->refs = 1;
    rcc->client = client;
    rcc->channel = channel;
    red_channel_ref(channel);
    red_channel_client_set_remote_caps(rcc, num_common_caps, common_caps, num_caps, caps);
    if (red_channel_client_test_remote_common_cap(rcc, SPICE_COMMON_CAP_MINI_HEADER)) {
        rcc->incoming.header = mini_header_wrapper;
        rcc->send_data.header = mini_header_wrapper;
        rcc->is_mini_header = TRUE;
    } else {
        rcc->incoming.header = full_header_wrapper;
        rcc->send_data.header = full_header_wrapper;
        rcc->is_mini_header = FALSE;
    }

    rcc->incoming.header.data = rcc->incoming.header_buf;
    rcc->incoming.serial = 1;
    ring_init(&rcc->pipe);

    rcc->dummy = TRUE;
    rcc->dummy_connected = TRUE;
    red_channel_add_client(channel, rcc);
    red_client_add_channel(client, rcc);
    pthread_mutex_unlock(&client->lock);
    return rcc;
error:
    pthread_mutex_unlock(&client->lock);
    return NULL;
}

void red_channel_apply_clients(RedChannel *channel, channel_client_callback cb)
{
    RingItem *link;
    RingItem *next;
    RedChannelClient *rcc;

    RING_FOREACH_SAFE(link, next, &channel->clients) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);
        cb(rcc);
    }
}

void red_channel_apply_clients_data(RedChannel *channel, channel_client_callback_data cb, void *data)
{
    RingItem *link;
    RingItem *next;
    RedChannelClient *rcc;

    RING_FOREACH_SAFE(link, next, &channel->clients) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);
        cb(rcc, data);
    }
}

int red_channel_all_blocked(RedChannel *channel)
{
    RingItem *link;
    RedChannelClient *rcc;

    if (!channel || channel->clients_num == 0) {
        return FALSE;
    }
    RING_FOREACH(link, &channel->clients) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);
        if (!rcc->send_data.blocked) {
            return FALSE;
        }
    }
    return TRUE;
}

int red_channel_any_blocked(RedChannel *channel)
{
    RingItem *link;
    RedChannelClient *rcc;

    RING_FOREACH(link, &channel->clients) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);
        if (rcc->send_data.blocked) {
            return TRUE;
        }
    }
    return FALSE;
}

int red_channel_client_blocked(RedChannelClient *rcc)
{
    return rcc && rcc->send_data.blocked;
}

int red_channel_client_send_message_pending(RedChannelClient *rcc)
{
    return rcc->send_data.header.get_msg_type(&rcc->send_data.header) != 0;
}

/* accessors for RedChannelClient */
SpiceMarshaller *red_channel_client_get_marshaller(RedChannelClient *rcc)
{
    return rcc->send_data.marshaller;
}

RedsStream *red_channel_client_get_stream(RedChannelClient *rcc)
{
    return rcc->stream;
}

RedClient *red_channel_client_get_client(RedChannelClient *rcc)
{
    return rcc->client;
}

void red_channel_client_set_header_sub_list(RedChannelClient *rcc, uint32_t sub_list)
{
    rcc->send_data.header.set_msg_sub_list(&rcc->send_data.header, sub_list);
}

/* end of accessors */

int red_channel_get_first_socket(RedChannel *channel)
{
    if (!channel || channel->clients_num == 0) {
        return -1;
    }
    return SPICE_CONTAINEROF(ring_get_head(&channel->clients),
                             RedChannelClient, channel_link)->stream->socket;
}

int red_channel_no_item_being_sent(RedChannel *channel)
{
    RingItem *link;
    RedChannelClient *rcc;

    RING_FOREACH(link, &channel->clients) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);
        if (!red_channel_client_no_item_being_sent(rcc)) {
            return FALSE;
        }
    }
    return TRUE;
}

int red_channel_client_no_item_being_sent(RedChannelClient *rcc)
{
    return !rcc || (rcc->send_data.size == 0);
}

void red_channel_client_pipe_remove_and_release(RedChannelClient *rcc,
                                                PipeItem *item)
{
    red_channel_client_pipe_remove(rcc, item);
    red_channel_client_release_item(rcc, item, FALSE);
}

/*
 * RedClient implementation - kept in red_channel.c because they are
 * pretty tied together.
 */

RedClient *red_client_new(int migrated)
{
    RedClient *client;

    client = spice_malloc0(sizeof(RedClient));
    ring_init(&client->channels);
    pthread_mutex_init(&client->lock, NULL);
    client->thread_id = pthread_self();
    client->during_target_migrate = migrated;

    return client;
}

/* client mutex should be locked before this call */
static void red_channel_client_set_migration_seamless(RedChannelClient *rcc)
{
    spice_assert(rcc->client->during_target_migrate && rcc->client->seamless_migrate);

    if (rcc->channel->migration_flags & SPICE_MIGRATE_NEED_DATA_TRANSFER) {
        rcc->wait_migrate_data = TRUE;
        rcc->client->num_migrated_channels++;
    }
    spice_debug("channel type %d id %d rcc %p wait data %d", rcc->channel->type, rcc->channel->id, rcc,
        rcc->wait_migrate_data);
}

void red_client_set_migration_seamless(RedClient *client) // dest
{
    RingItem *link;
    pthread_mutex_lock(&client->lock);
    client->seamless_migrate = TRUE;
    /* update channel clients that got connected before the migration
     * type was set. red_client_add_channel will handle newer channel clients */
    RING_FOREACH(link, &client->channels) {
        RedChannelClient *rcc = SPICE_CONTAINEROF(link, RedChannelClient, client_link);
        red_channel_client_set_migration_seamless(rcc);
    }
    pthread_mutex_unlock(&client->lock);
}

void red_client_migrate(RedClient *client)
{
    RingItem *link, *next;
    RedChannelClient *rcc;

    spice_printerr("migrate client with #channels %d", client->channels_num);
    if (!pthread_equal(pthread_self(), client->thread_id)) {
        spice_warning("client->thread_id (0x%lx) != pthread_self (0x%lx)."
                      "If one of the threads is != io-thread && != vcpu-thread,"
                      " this might be a BUG",
                      client->thread_id, pthread_self());
    }
    RING_FOREACH_SAFE(link, next, &client->channels) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, client_link);
        if (red_channel_client_is_connected(rcc)) {
            rcc->channel->client_cbs.migrate(rcc);
        }
    }
}

void red_client_destroy(RedClient *client)
{
    RingItem *link, *next;
    RedChannelClient *rcc;

    spice_printerr("destroy client with #channels %d", client->channels_num);
    if (!pthread_equal(pthread_self(), client->thread_id)) {
        spice_warning("client->thread_id (0x%lx) != pthread_self (0x%lx)."
                      "If one of the threads is != io-thread && != vcpu-thread,"
                      " this might be a BUG",
                      client->thread_id,
                      pthread_self());
    }
    RING_FOREACH_SAFE(link, next, &client->channels) {
        // some channels may be in other threads, so disconnection
        // is not synchronous.
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, client_link);
        rcc->destroying = 1;
        // some channels may be in other threads. However we currently
        // assume disconnect is synchronous (we changed the dispatcher
        // to wait for disconnection)
        // TODO: should we go back to async. For this we need to use
        // ref count for channel clients.
        rcc->channel->client_cbs.disconnect(rcc);
        spice_assert(ring_is_empty(&rcc->pipe));
        spice_assert(rcc->pipe_size == 0);
        spice_assert(rcc->send_data.size == 0);
        red_channel_client_destroy(rcc);
    }

    pthread_mutex_destroy(&client->lock);
    free(client);
}

/* client->lock should be locked */
static RedChannelClient *red_client_get_channel(RedClient *client, int type, int id)
{
    RingItem *link;
    RedChannelClient *rcc;
    RedChannelClient *ret = NULL;

    RING_FOREACH(link, &client->channels) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, client_link);
        if (rcc->channel->type == type && rcc->channel->id == id) {
            ret = rcc;
            break;
        }
    }
    return ret;
}

/* client->lock should be locked */
static void red_client_add_channel(RedClient *client, RedChannelClient *rcc)
{
    spice_assert(rcc && client);
    ring_add(&client->channels, &rcc->client_link);
    if (client->during_target_migrate && client->seamless_migrate) {
        red_channel_client_set_migration_seamless(rcc);
    }
    client->channels_num++;
}

MainChannelClient *red_client_get_main(RedClient *client) {
    return client->mcc;
}

void red_client_set_main(RedClient *client, MainChannelClient *mcc) {
    client->mcc = mcc;
}

void red_client_semi_seamless_migrate_complete(RedClient *client)
{
    RingItem *link, *next;

    pthread_mutex_lock(&client->lock);
    if (!client->during_target_migrate || client->seamless_migrate) {
        spice_error("unexpected");
        pthread_mutex_unlock(&client->lock);
        return;
    }
    client->during_target_migrate = FALSE;
    RING_FOREACH_SAFE(link, next, &client->channels) {
        RedChannelClient *rcc = SPICE_CONTAINEROF(link, RedChannelClient, client_link);

        if (rcc->latency_monitor.timer) {
            red_channel_client_start_ping_timer(rcc, PING_TEST_IDLE_NET_TIMEOUT_MS);
        }
    }
    pthread_mutex_unlock(&client->lock);
    reds_on_client_semi_seamless_migrate_complete(client);
}

/* should be called only from the main thread */
int red_client_during_migrate_at_target(RedClient *client)
{
    int ret;
    pthread_mutex_lock(&client->lock);
    ret = client->during_target_migrate;
    pthread_mutex_unlock(&client->lock);
    return ret;
}

/*
 * Functions to push the same item to multiple pipes.
 */

/*
 * TODO: after convinced of correctness, add paths for single client
 * that avoid the whole loop. perhaps even have a function pointer table
 * later.
 * TODO - inline? macro? right now this is the simplest from code amount
 */

typedef void (*rcc_item_t)(RedChannelClient *rcc, PipeItem *item);
typedef int (*rcc_item_cond_t)(RedChannelClient *rcc, PipeItem *item);

static void red_channel_pipes_create_batch(RedChannel *channel,
                                new_pipe_item_t creator, void *data,
                                rcc_item_t callback)
{
    RingItem *link, *next;
    RedChannelClient *rcc;
    PipeItem *item;
    int num = 0;

    RING_FOREACH_SAFE(link, next, &channel->clients) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);
        item = (*creator)(rcc, data, num++);
        if (callback) {
            (*callback)(rcc, item);
        }
    }
}

void red_channel_pipes_new_add_push(RedChannel *channel,
                              new_pipe_item_t creator, void *data)
{
    red_channel_pipes_create_batch(channel, creator, data,
                                     red_channel_client_pipe_add);
    red_channel_push(channel);
}

void red_channel_pipes_new_add(RedChannel *channel, new_pipe_item_t creator, void *data)
{
    red_channel_pipes_create_batch(channel, creator, data,
                                     red_channel_client_pipe_add);
}

void red_channel_pipes_new_add_tail(RedChannel *channel, new_pipe_item_t creator, void *data)
{
    red_channel_pipes_create_batch(channel, creator, data,
                                     red_channel_client_pipe_add_tail_no_push);
}

uint32_t red_channel_max_pipe_size(RedChannel *channel)
{
    RingItem *link;
    RedChannelClient *rcc;
    uint32_t pipe_size = 0;

    RING_FOREACH(link, &channel->clients) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);
        pipe_size = pipe_size > rcc->pipe_size ? pipe_size : rcc->pipe_size;
    }
    return pipe_size;
}

uint32_t red_channel_min_pipe_size(RedChannel *channel)
{
    RingItem *link;
    RedChannelClient *rcc;
    uint32_t pipe_size = ~0;

    RING_FOREACH(link, &channel->clients) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);
        pipe_size = pipe_size < rcc->pipe_size ? pipe_size : rcc->pipe_size;
    }
    return pipe_size == ~0 ? 0 : pipe_size;
}

uint32_t red_channel_sum_pipes_size(RedChannel *channel)
{
    RingItem *link;
    RedChannelClient *rcc;
    uint32_t sum = 0;

    RING_FOREACH(link, &channel->clients) {
        rcc = SPICE_CONTAINEROF(link, RedChannelClient, channel_link);
        sum += rcc->pipe_size;
    }
    return sum;
}
