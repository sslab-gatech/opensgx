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

#ifndef _H_RED_CHANNEL
#define _H_RED_CHANNEL

#include <pthread.h>
#include <limits.h>

#include "common/ring.h"
#include "common/marshaller.h"

#include "spice.h"
#include "red_common.h"
#include "demarshallers.h"

#define MAX_SEND_BUFS 1000
#define CLIENT_ACK_WINDOW 20

#ifndef IOV_MAX
#define IOV_MAX 1024
#endif

#define MAX_HEADER_SIZE sizeof(SpiceDataHeader)

/* Basic interface for channels, without using the RedChannel interface.
   The intention is to move towards one channel interface gradually.
   At the final stage, this interface shouldn't be exposed. Only RedChannel will use it. */

typedef struct SpiceDataHeaderOpaque SpiceDataHeaderOpaque;

typedef uint16_t (*get_msg_type_proc)(SpiceDataHeaderOpaque *header);
typedef uint32_t (*get_msg_size_proc)(SpiceDataHeaderOpaque *header);
typedef void (*set_msg_type_proc)(SpiceDataHeaderOpaque *header, uint16_t type);
typedef void (*set_msg_size_proc)(SpiceDataHeaderOpaque *header, uint32_t size);
typedef void (*set_msg_serial_proc)(SpiceDataHeaderOpaque *header, uint64_t serial);
typedef void (*set_msg_sub_list_proc)(SpiceDataHeaderOpaque *header, uint32_t sub_list);

struct SpiceDataHeaderOpaque {
    uint8_t *data;
    uint16_t header_size;

    set_msg_type_proc set_msg_type;
    set_msg_size_proc set_msg_size;
    set_msg_serial_proc set_msg_serial;
    set_msg_sub_list_proc set_msg_sub_list;

    get_msg_type_proc get_msg_type;
    get_msg_size_proc get_msg_size;
};

typedef int (*handle_message_proc)(void *opaque,
                                   uint16_t type, uint32_t size, uint8_t *msg);
typedef int (*handle_parsed_proc)(void *opaque, uint32_t size, uint16_t type, void *message);
typedef uint8_t *(*alloc_msg_recv_buf_proc)(void *opaque, uint16_t type, uint32_t size);
typedef void (*release_msg_recv_buf_proc)(void *opaque,
                                          uint16_t type, uint32_t size, uint8_t *msg);
typedef void (*on_incoming_error_proc)(void *opaque);

typedef struct IncomingHandlerInterface {
    handle_message_proc handle_message;
    alloc_msg_recv_buf_proc alloc_msg_buf;
    on_incoming_error_proc on_error; // recv error or handle_message error
    release_msg_recv_buf_proc release_msg_buf; // for errors
    // The following is an optional alternative to handle_message, used if not null
    spice_parse_channel_func_t parser;
    handle_parsed_proc handle_parsed;
} IncomingHandlerInterface;

typedef struct IncomingHandler {
    IncomingHandlerInterface *cb;
    void *opaque;
    uint8_t header_buf[MAX_HEADER_SIZE];
    SpiceDataHeaderOpaque header;
    uint32_t header_pos;
    uint8_t *msg; // data of the msg following the header. allocated by alloc_msg_buf.
    uint32_t msg_pos;
    uint64_t serial;
} IncomingHandler;

typedef int (*get_outgoing_msg_size_proc)(void *opaque);
typedef void (*prepare_outgoing_proc)(void *opaque, struct iovec *vec, int *vec_size, int pos);
typedef void (*on_outgoing_error_proc)(void *opaque);
typedef void (*on_outgoing_block_proc)(void *opaque);
typedef void (*on_outgoing_msg_done_proc)(void *opaque);
typedef void (*on_output_proc)(void *opaque, int n);

typedef struct OutgoingHandlerInterface {
    get_outgoing_msg_size_proc get_msg_size;
    prepare_outgoing_proc prepare;
    on_outgoing_error_proc on_error;
    on_outgoing_block_proc on_block;
    on_outgoing_msg_done_proc on_msg_done;
    on_output_proc on_output;
} OutgoingHandlerInterface;

typedef struct OutgoingHandler {
    OutgoingHandlerInterface *cb;
    void *opaque;
    struct iovec vec_buf[IOV_MAX];
    int vec_size;
    struct iovec *vec;
    int pos;
    int size;
} OutgoingHandler;

/* Red Channel interface */

typedef struct BufDescriptor {
    uint32_t size;
    uint8_t *data;
} BufDescriptor;

typedef struct RedsStream RedsStream;
typedef struct RedChannel RedChannel;
typedef struct RedChannelClient RedChannelClient;
typedef struct RedClient RedClient;
typedef struct MainChannelClient MainChannelClient;

/* Messages handled by red_channel
 * SET_ACK - sent to client on channel connection
 * Note that the numbers don't have to correspond to spice message types,
 * but we keep the 100 first allocated for base channel approach.
 * */
enum {
    PIPE_ITEM_TYPE_SET_ACK=1,
    PIPE_ITEM_TYPE_MIGRATE,
    PIPE_ITEM_TYPE_EMPTY_MSG,
    PIPE_ITEM_TYPE_PING,

    PIPE_ITEM_TYPE_CHANNEL_BASE=101,
};

typedef struct PipeItem {
    RingItem link;
    int type;
} PipeItem;

typedef uint8_t *(*channel_alloc_msg_recv_buf_proc)(RedChannelClient *channel,
                                                    uint16_t type, uint32_t size);
typedef int (*channel_handle_parsed_proc)(RedChannelClient *rcc, uint32_t size, uint16_t type,
                                        void *message);
typedef int (*channel_handle_message_proc)(RedChannelClient *rcc,
                                           uint16_t type, uint32_t size, uint8_t *msg);
typedef void (*channel_release_msg_recv_buf_proc)(RedChannelClient *channel,
                                                  uint16_t type, uint32_t size, uint8_t *msg);
typedef void (*channel_disconnect_proc)(RedChannelClient *rcc);
typedef int (*channel_configure_socket_proc)(RedChannelClient *rcc);
typedef void (*channel_send_pipe_item_proc)(RedChannelClient *rcc, PipeItem *item);
typedef void (*channel_hold_pipe_item_proc)(RedChannelClient *rcc, PipeItem *item);
typedef void (*channel_release_pipe_item_proc)(RedChannelClient *rcc,
                                               PipeItem *item, int item_pushed);
typedef void (*channel_on_incoming_error_proc)(RedChannelClient *rcc);
typedef void (*channel_on_outgoing_error_proc)(RedChannelClient *rcc);

typedef int (*channel_handle_migrate_flush_mark_proc)(RedChannelClient *base);
typedef int (*channel_handle_migrate_data_proc)(RedChannelClient *base,
                                                uint32_t size, void *message);
typedef uint64_t (*channel_handle_migrate_data_get_serial_proc)(RedChannelClient *base,
                                            uint32_t size, void *message);


typedef void (*channel_client_connect_proc)(RedChannel *channel, RedClient *client, RedsStream *stream,
                                            int migration, int num_common_cap, uint32_t *common_caps,
                                            int num_caps, uint32_t *caps);
typedef void (*channel_client_disconnect_proc)(RedChannelClient *base);
typedef void (*channel_client_migrate_proc)(RedChannelClient *base);

// TODO: add ASSERTS for thread_id  in client and channel calls
//
/*
 * callbacks that are triggered from channel client stream events.
 * They are called from the thread that listen to the stream events.
 */
typedef struct {
    channel_configure_socket_proc config_socket;
    channel_disconnect_proc on_disconnect;
    channel_send_pipe_item_proc send_item;
    channel_hold_pipe_item_proc hold_item;
    channel_release_pipe_item_proc release_item;
    channel_alloc_msg_recv_buf_proc alloc_recv_buf;
    channel_release_msg_recv_buf_proc release_recv_buf;
    channel_handle_migrate_flush_mark_proc handle_migrate_flush_mark;
    channel_handle_migrate_data_proc handle_migrate_data;
    channel_handle_migrate_data_get_serial_proc handle_migrate_data_get_serial;
} ChannelCbs;


/*
 * callbacks that are triggered from client events.
 * They should be called from the thread that handles the RedClient
 */
typedef struct {
    channel_client_connect_proc connect;
    channel_client_disconnect_proc disconnect;
    channel_client_migrate_proc migrate;
} ClientCbs;

typedef struct RedChannelCapabilities {
    int num_common_caps;
    uint32_t *common_caps;
    int num_caps;
    uint32_t *caps;
} RedChannelCapabilities;

int test_capabilty(uint32_t *caps, int num_caps, uint32_t cap);

typedef struct RedChannelClientLatencyMonitor {
    int state;
    uint64_t last_pong_time;
    SpiceTimer *timer;
    uint32_t id;
    int tcp_nodelay;
    int warmup_was_sent;

    int64_t roundtrip;
} RedChannelClientLatencyMonitor;

struct RedChannelClient {
    RingItem channel_link;
    RingItem client_link;
    RedChannel *channel;
    RedClient  *client;
    RedsStream *stream;
    int dummy;
    int dummy_connected;

    uint32_t refs;

    struct {
        uint32_t generation;
        uint32_t client_generation;
        uint32_t messages_window;
        uint32_t client_window;
    } ack_data;

    struct {
        SpiceMarshaller *marshaller;
        SpiceDataHeaderOpaque header;
        uint32_t size;
        PipeItem *item;
        int blocked;
        uint64_t serial;
        uint64_t last_sent_serial;

        struct {
            SpiceMarshaller *marshaller;
            uint8_t *header_data;
            PipeItem *item;
        } main;

        struct {
            SpiceMarshaller *marshaller;
        } urgent;
    } send_data;

    OutgoingHandler outgoing;
    IncomingHandler incoming;
    int during_send;
    int id; // debugging purposes
    Ring pipe;
    uint32_t pipe_size;

    RedChannelCapabilities remote_caps;
    int is_mini_header;
    int destroying;

    int wait_migrate_data;
    int wait_migrate_flush_mark;

    RedChannelClientLatencyMonitor latency_monitor;
};

struct RedChannel {
    uint32_t type;
    uint32_t id;

    uint32_t refs;

    RingItem link; // channels link for reds

    SpiceCoreInterface *core;
    int handle_acks;

    // RedChannel will hold only connected channel clients (logic - when pushing pipe item to all channel clients, there
    // is no need to go over disconnect clients)
    // . While client will hold the channel clients till it is destroyed
    // and then it will destroy them as well.
    // However RCC still holds a reference to the Channel.
    // Maybe replace these logic with ref count?
    // TODO: rename to 'connected_clients'?
    Ring clients;
    uint32_t clients_num;

    OutgoingHandlerInterface outgoing_cb;
    IncomingHandlerInterface incoming_cb;

    ChannelCbs channel_cbs;
    ClientCbs client_cbs;

    RedChannelCapabilities local_caps;
    uint32_t migration_flags;

    void *data;

    // TODO: when different channel_clients are in different threads from Channel -> need to protect!
    pthread_t thread_id;
#ifdef RED_STATISTICS
    uint64_t *out_bytes_counter;
#endif
};

/*
 * When an error occurs over a channel, we treat it as a warning
 * for spice-server and shutdown the channel.
 */
#define spice_channel_client_error(rcc, format, ...)                                     \
    do {                                                                                 \
        spice_warning("rcc %p type %u id %u: " format, rcc,                              \
                    rcc->channel->type, rcc->channel->id, ## __VA_ARGS__);               \
        red_channel_client_shutdown(rcc);                                                \
    } while (0)

/* if one of the callbacks should cause disconnect, use red_channel_shutdown and don't
 * explicitly destroy the channel */
RedChannel *red_channel_create(int size,
                               SpiceCoreInterface *core,
                               uint32_t type, uint32_t id,
                               int handle_acks,
                               channel_handle_message_proc handle_message,
                               ChannelCbs *channel_cbs,
                               uint32_t migration_flags);

/* alternative constructor, meant for marshaller based (inputs,main) channels,
 * will become default eventually */
RedChannel *red_channel_create_parser(int size,
                               SpiceCoreInterface *core,
                               uint32_t type, uint32_t id,
                               int handle_acks,
                               spice_parse_channel_func_t parser,
                               channel_handle_parsed_proc handle_parsed,
                               ChannelCbs *channel_cbs,
                               uint32_t migration_flags);

void red_channel_register_client_cbs(RedChannel *channel, ClientCbs *client_cbs);
// caps are freed when the channel is destroyed
void red_channel_set_common_cap(RedChannel *channel, uint32_t cap);
void red_channel_set_cap(RedChannel *channel, uint32_t cap);
void red_channel_set_data(RedChannel *channel, void *data);

RedChannelClient *red_channel_client_create(int size, RedChannel *channel, RedClient *client,
                                            RedsStream *stream,
                                            int monitor_latency,
                                            int num_common_caps, uint32_t *common_caps,
                                            int num_caps, uint32_t *caps);
// TODO: tmp, for channels that don't use RedChannel yet (e.g., snd channel), but
// do use the client callbacks. So the channel clients are not connected (the channel doesn't
// have list of them, but they do have a link to the channel, and the client has a list of them)
RedChannel *red_channel_create_dummy(int size, uint32_t type, uint32_t id);
RedChannelClient *red_channel_client_create_dummy(int size,
                                                  RedChannel *channel,
                                                  RedClient  *client,
                                                  int num_common_caps, uint32_t *common_caps,
                                                  int num_caps, uint32_t *caps);

int red_channel_is_connected(RedChannel *channel);
int red_channel_client_is_connected(RedChannelClient *rcc);

void red_channel_client_default_migrate(RedChannelClient *rcc);
int red_channel_client_waits_for_migrate_data(RedChannelClient *rcc);
/* seamless migration is supported for only one client. This routine
 * checks if the only channel client associated with channel is
 * waiting for migration data */
int red_channel_waits_for_migrate_data(RedChannel *channel);

/*
 * the disconnect callback is called from the channel's thread,
 * i.e., for display channels - red worker thread, for all the other - from the main thread.
 * RedClient is managed from the main thread. red_channel_client_destroy can be called only
 * from red_client_destroy.
 */

void red_channel_client_destroy(RedChannelClient *rcc);
void red_channel_destroy(RedChannel *channel);

int red_channel_client_test_remote_common_cap(RedChannelClient *rcc, uint32_t cap);
int red_channel_client_test_remote_cap(RedChannelClient *rcc, uint32_t cap);

/* return true if all the channel clients support the cap */
int red_channel_test_remote_common_cap(RedChannel *channel, uint32_t cap);
int red_channel_test_remote_cap(RedChannel *channel, uint32_t cap);

/* shutdown is the only safe thing to do out of the client/channel
 * thread. It will not touch the rings, just shutdown the socket.
 * It should be followed by some way to gurantee a disconnection. */
void red_channel_client_shutdown(RedChannelClient *rcc);

/* should be called when a new channel is ready to send messages */
void red_channel_init_outgoing_messages_window(RedChannel *channel);

/* handles general channel msgs from the client */
int red_channel_client_handle_message(RedChannelClient *rcc, uint32_t size,
                                      uint16_t type, void *message);

/* when preparing send_data: should call init and then use marshaller */
void red_channel_client_init_send_data(RedChannelClient *rcc, uint16_t msg_type, PipeItem *item);

uint64_t red_channel_client_get_message_serial(RedChannelClient *channel);
void red_channel_client_set_message_serial(RedChannelClient *channel, uint64_t);

/* When sending a msg. Should first call red_channel_client_begin_send_message.
 * It will first send the pending urgent data, if there is any, and then
 * the rest of the data.
 */
void red_channel_client_begin_send_message(RedChannelClient *rcc);

/*
 * Stores the current send data, and switches to urgent send data.
 * When it begins the actual send, it will send first the urgent data
 * and afterward the rest of the data.
 * Should be called only if during the marshalling of on message,
 * the need to send another message, before, rises.
 * Important: the serial of the non-urgent sent data, will be succeeded.
 * return: the urgent send data marshaller
 */
SpiceMarshaller *red_channel_client_switch_to_urgent_sender(RedChannelClient *rcc);

/* returns -1 if we don't have an estimation */
int red_channel_client_get_roundtrip_ms(RedChannelClient *rcc);

void red_channel_pipe_item_init(RedChannel *channel, PipeItem *item, int type);

// TODO: add back the channel_pipe_add functionality - by adding reference counting
// to the PipeItem.

// helper to push a new item to all channels
typedef PipeItem *(*new_pipe_item_t)(RedChannelClient *rcc, void *data, int num);
void red_channel_pipes_new_add_push(RedChannel *channel, new_pipe_item_t creator, void *data);
void red_channel_pipes_new_add(RedChannel *channel, new_pipe_item_t creator, void *data);
void red_channel_pipes_new_add_tail(RedChannel *channel, new_pipe_item_t creator, void *data);

void red_channel_client_pipe_add_push(RedChannelClient *rcc, PipeItem *item);
void red_channel_client_pipe_add(RedChannelClient *rcc, PipeItem *item);
void red_channel_client_pipe_add_after(RedChannelClient *rcc, PipeItem *item, PipeItem *pos);
int red_channel_client_pipe_item_is_linked(RedChannelClient *rcc, PipeItem *item);
void red_channel_client_pipe_remove_and_release(RedChannelClient *rcc, PipeItem *item);
void red_channel_client_pipe_add_tail(RedChannelClient *rcc, PipeItem *item);
/* for types that use this routine -> the pipe item should be freed */
void red_channel_client_pipe_add_type(RedChannelClient *rcc, int pipe_item_type);
void red_channel_pipes_add_type(RedChannel *channel, int pipe_item_type);

void red_channel_client_pipe_add_empty_msg(RedChannelClient *rcc, int msg_type);
void red_channel_pipes_add_empty_msg(RedChannel *channel, int msg_type);

void red_channel_client_ack_zero_messages_window(RedChannelClient *rcc);
void red_channel_client_ack_set_client_window(RedChannelClient *rcc, int client_window);
void red_channel_client_push_set_ack(RedChannelClient *rcc);
void red_channel_push_set_ack(RedChannel *channel);

int red_channel_get_first_socket(RedChannel *channel);

/* return TRUE if all of the connected clients to this channel are blocked */
int red_channel_all_blocked(RedChannel *channel);

/* return TRUE if any of the connected clients to this channel are blocked */
int red_channel_any_blocked(RedChannel *channel);

int red_channel_client_blocked(RedChannelClient *rcc);

/* helper for channels that have complex logic that can possibly ready a send */
int red_channel_client_send_message_pending(RedChannelClient *rcc);

int red_channel_no_item_being_sent(RedChannel *channel);
int red_channel_client_no_item_being_sent(RedChannelClient *rcc);

void red_channel_pipes_remove(RedChannel *channel, PipeItem *item);

// TODO: unstaticed for display/cursor channels. they do some specific pushes not through
// adding elements or on events. but not sure if this is actually required (only result
// should be that they ""try"" a little harder, but if the event system is correct it
// should not make any difference.
void red_channel_push(RedChannel *channel);
void red_channel_client_push(RedChannelClient *rcc);
// TODO: again - what is the context exactly? this happens in channel disconnect. but our
// current red_channel_shutdown also closes the socket - is there a socket to close?
// are we reading from an fd here? arghh
void red_channel_client_pipe_clear(RedChannelClient *rcc);
// Again, used in various places outside of event handler context (or in other event handler
// contexts):
//  flush_display_commands/flush_cursor_commands
//  display_channel_wait_for_init
//  red_wait_outgoing_item
//  red_wait_pipe_item_sent
//  handle_channel_events - this is the only one that was used before, and was in red_channel.c
void red_channel_receive(RedChannel *channel);
void red_channel_client_receive(RedChannelClient *rcc);
// For red_worker
void red_channel_send(RedChannel *channel);
void red_channel_client_send(RedChannelClient *rcc);
// For red_worker
void red_channel_disconnect(RedChannel *channel);
void red_channel_client_disconnect(RedChannelClient *rcc);

/* accessors for RedChannelClient */
/* Note: the valid times to call red_channel_get_marshaller are just during send_item callback. */
SpiceMarshaller *red_channel_client_get_marshaller(RedChannelClient *rcc);
RedsStream *red_channel_client_get_stream(RedChannelClient *rcc);
RedClient *red_channel_client_get_client(RedChannelClient *rcc);

/* Note that the header is valid only between red_channel_reset_send_data and
 * red_channel_begin_send_message.*/
void red_channel_client_set_header_sub_list(RedChannelClient *rcc, uint32_t sub_list);

/* return the sum of all the rcc pipe size */
uint32_t red_channel_max_pipe_size(RedChannel *channel);
/* return the min size of all the rcc pipe */
uint32_t red_channel_min_pipe_size(RedChannel *channel);
/* return the max size of all the rcc pipe */
uint32_t red_channel_sum_pipes_size(RedChannel *channel);

/* apply given function to all connected clients */
typedef void (*channel_client_callback)(RedChannelClient *rcc);
typedef void (*channel_client_callback_data)(RedChannelClient *rcc, void *data);
void red_channel_apply_clients(RedChannel *channel, channel_client_callback v);
void red_channel_apply_clients_data(RedChannel *channel, channel_client_callback_data v, void *data);

struct RedClient {
    RingItem link;
    Ring channels;
    int channels_num;
    MainChannelClient *mcc;
    pthread_mutex_t lock; // different channels can be in different threads

    pthread_t thread_id;

    int disconnecting;
    /* Note that while semi-seamless migration is conducted by the main thread, seamless migration
     * involves all channels, and thus the related varaibles can be accessed from different
     * threads */
    int during_target_migrate; /* if seamless=TRUE, migration_target is turned off when all
                                  the clients received their migration data. Otherwise (semi-seamless),
                                  it is turned off, when red_client_semi_seamless_migrate_complete
                                  is called */
    int seamless_migrate;
    int num_migrated_channels; /* for seamless - number of channels that wait for migrate data*/
};

RedClient *red_client_new(int migrated);

MainChannelClient *red_client_get_main(RedClient *client);
// main should be set once before all the other channels are created
void red_client_set_main(RedClient *client, MainChannelClient *mcc);

/* called when the migration handshake results in seamless migration (dst side).
 * By default we assume semi-seamless */
void red_client_set_migration_seamless(RedClient *client);
void red_client_semi_seamless_migrate_complete(RedClient *client); /* dst side */
/* TRUE if the migration is seamless and there are still channels that wait from migration data.
 * Or, during semi-seamless migration, and the main channel still waits for MIGRATE_END
 * from the client.
 * Note: Call it only from the main thread */
int red_client_during_migrate_at_target(RedClient *client);

void red_client_migrate(RedClient *client);
// disconnects all the client's channels (should be called from the client's thread)
void red_client_destroy(RedClient *client);

#endif
