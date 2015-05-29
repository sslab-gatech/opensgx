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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "spice.h"
#include "spice-experimental.h"
#include "red_tunnel_worker.h"
#include "red_common.h"
#include <spice/protocol.h>
#include "reds.h"
#include "net_slirp.h"
#include "red_channel.h"


//#define DEBUG_NETWORK

#ifdef DEBUG_NETWORK
#define PRINT_SCKT(sckt) spice_printerr("TUNNEL_DBG SOCKET(connection_id=%d port=%d, service=%d)",\
                                    sckt->connection_id, ntohs(sckt->local_port),             \
                                    sckt->far_service->id)
#endif

#define MAX_SOCKETS_NUM 20

#define MAX_SOCKET_DATA_SIZE (1024 * 2)

#define SOCKET_WINDOW_SIZE 80
#define SOCKET_TOKENS_TO_SEND 20
#define SOCKET_TOKENS_TO_SEND_FOR_PROCESS 5 // sent in case the all the tokens were used by
                                            // the client but they weren't consumed by slirp
                                            // due to missing data for processing them and
                                            // turning them into 'ready chunks'

/* the number of buffer might exceed the window size when the analysis of the buffers in the
   process queue need more data in order to be able to move them to the ready queue */
#define MAX_SOCKET_IN_BUFFERS (int)(SOCKET_WINDOW_SIZE * 1.5)
#define MAX_SOCKET_OUT_BUFFERS (int)(SOCKET_WINDOW_SIZE * 1.5)

#define CONTROL_MSG_RECV_BUF_SIZE 1024

typedef struct TunnelWorker TunnelWorker;

enum {
    PIPE_ITEM_TYPE_MIGRATE_DATA = PIPE_ITEM_TYPE_CHANNEL_BASE,
    PIPE_ITEM_TYPE_TUNNEL_INIT,
    PIPE_ITEM_TYPE_SERVICE_IP_MAP,
    PIPE_ITEM_TYPE_SOCKET_OPEN,
    PIPE_ITEM_TYPE_SOCKET_FIN,
    PIPE_ITEM_TYPE_SOCKET_CLOSE,
    PIPE_ITEM_TYPE_SOCKET_CLOSED_ACK,
    PIPE_ITEM_TYPE_SOCKET_DATA,
    PIPE_ITEM_TYPE_SOCKET_TOKEN,
};

typedef struct RawTunneledBuffer RawTunneledBuffer;
typedef void (*release_tunneled_buffer_proc_t)(RawTunneledBuffer *buf);

struct RawTunneledBuffer {
    uint8_t *data;
    int size;
    int max_size;
    int refs;
    RawTunneledBuffer *next;
    void *usr_opaque;
    release_tunneled_buffer_proc_t release_proc;
};

static inline RawTunneledBuffer *tunneled_buffer_ref(RawTunneledBuffer *buf)
{
    buf->refs++;
    return buf;
}

static inline void tunneled_buffer_unref(RawTunneledBuffer *buf)
{
    if (!(--buf->refs)) {
        buf->release_proc(buf);
    }
}

typedef struct RedSocket RedSocket;

/* data received from the quest through slirp */
typedef struct RedSocketRawSndBuf {
    RawTunneledBuffer base;
    uint8_t buf[MAX_SOCKET_DATA_SIZE];
} RedSocketRawSndBuf;

/* data received from the client */
typedef struct RedSocketRawRcvBuf {
    RawTunneledBuffer base;
    uint8_t buf[MAX_SOCKET_DATA_SIZE + sizeof(SpiceMsgcTunnelSocketData)];
    SpiceMsgcTunnelSocketData *msg_info;
} RedSocketRawRcvBuf;

typedef struct ReadyTunneledChunk ReadyTunneledChunk;

enum {
    READY_TUNNELED_CHUNK_TYPE_ORIG,
    READY_TUNNELED_CHUNK_TYPE_SUB, // substitution
};


/* A chunk of data from a RawTunneledBuffer (or a substitution for a part of it)
   that was processed and is ready to be consumed (by slirp or by the client).
   Each chunk has a reference to the RawTunneledBuffer it
   was originated from. When all the reference chunks of one buffer are consumed (i.e. they are out
   of the ready queue and they unrefed the buffer), the buffer is released */
struct ReadyTunneledChunk {
    uint32_t type;
    RawTunneledBuffer *origin;
    uint8_t *data;             // if type == READY_TUNNELED_CHUNK_TYPE_ORIG, it points
                               // directly to the tunneled data. Otherwise, it is a
                               // newly allocated chunk of data
                               // that should be freed after its consumption.
    int size;
    ReadyTunneledChunk *next;
};

typedef struct ReadyTunneledChunkQueue {
    ReadyTunneledChunk *head;
    ReadyTunneledChunk *tail;
    uint32_t offset;           // first byte in the ready queue that wasn't consumed
} ReadyTunneledChunkQueue;

static void ready_queue_add_orig_chunk(ReadyTunneledChunkQueue *queue, RawTunneledBuffer *origin,
                                       uint8_t *data, int size);
static void ready_queue_pop_chunk(ReadyTunneledChunkQueue *queue);


enum {
    PROCESS_DIRECTION_TYPE_REQUEST, //  guest request
    PROCESS_DIRECTION_TYPE_REPLY,   // reply from the service in the client LAN
};

typedef struct TunneledBufferProcessQueue TunneledBufferProcessQueue;

typedef RawTunneledBuffer *(*alloc_tunneled_buffer_proc_t)(TunneledBufferProcessQueue *queue);
/* processing the data. Notice that the buffers can be empty of
 * data (see RedSocketRestoreTokensBuf) */
typedef void (*analyze_new_data_proc_t)(TunneledBufferProcessQueue *queue,
                                        RawTunneledBuffer *start_buf, int offset, int len);

// migrating specific queue data (not the buffers themselves)
typedef int (*get_migrate_data_proc_t)(TunneledBufferProcessQueue *queue, void **migrate_data);
typedef void (*release_migrate_data_proc_t)(TunneledBufferProcessQueue *queue, void *migrate_data);
typedef void (*restore_proc_t)(TunneledBufferProcessQueue *queue, uint8_t *migrate_data);

struct TunneledBufferProcessQueue {
    uint32_t service_type; // which kind of processing is performed.
    uint32_t direction;    // reply/request
    RawTunneledBuffer *head;
    RawTunneledBuffer *tail;
    int head_offset;

    ReadyTunneledChunkQueue *ready_chunks_queue; // the queue to push the post-process data to

    void *usr_opaque;

    alloc_tunneled_buffer_proc_t alloc_buf_proc; // for appending data to the queue
    analyze_new_data_proc_t analysis_proc;       // service dependent. should create the
                                                 // post-process chunks and remove buffers
                                                 // from the queue.
    get_migrate_data_proc_t get_migrate_data_proc;
    release_migrate_data_proc_t release_migrate_data_proc;
    restore_proc_t restore_proc;
};

/* push and append routines are the ones that call to the analysis_proc */
static void process_queue_push(TunneledBufferProcessQueue *queue, RawTunneledBuffer *buf);
static void process_queue_append(TunneledBufferProcessQueue *queue, uint8_t *data, size_t size);
static void process_queue_pop(TunneledBufferProcessQueue *queue);

static void process_queue_clear(TunneledBufferProcessQueue *queue);


typedef struct RedSocketOutData {
    // Note that this pipe items can appear only once in the pipe
    PipeItem status_pipe_item;
    PipeItem data_pipe_item;
    PipeItem token_pipe_item;

    TunneledBufferProcessQueue *process_queue;  // service type dependent
    ReadyTunneledChunkQueue ready_chunks_queue;
    ReadyTunneledChunk *push_tail;              // last chunk in the ready queue that was pushed
    uint32_t push_tail_size;                    // the subset of the push_tail that was sent

    uint32_t num_buffers; // total count of buffers in process_queue + references from ready queue
    uint32_t data_size;   // total size of data that is waiting to be sent.

    uint32_t num_tokens;
    uint32_t window_size;
} RedSocketOutData;

typedef struct RedSocketInData {
    TunneledBufferProcessQueue *process_queue; // service type dependent
    ReadyTunneledChunkQueue ready_chunks_queue;

    uint32_t num_buffers;

    int32_t num_tokens;   // No. tokens consumed by slirp since the last token msg sent to the
                          // client. can be negative if we loaned some to the client (when the
                          // ready queue is empty)
    uint32_t client_total_num_tokens;
} RedSocketInData;

typedef enum {
    SLIRP_SCKT_STATUS_OPEN,
    SLIRP_SCKT_STATUS_SHUTDOWN_SEND, // FIN was issued from guest
    SLIRP_SCKT_STATUS_SHUTDOWN_RECV, // Triggered when FIN is received from client
    SLIRP_SCKT_STATUS_DELAY_ABORT,   // when out buffers overflow, we wait for client to
                                     // close before we close slirp socket. see
                                     //tunnel_socket_force_close
    SLIRP_SCKT_STATUS_WAIT_CLOSE,    // when shutdown_send was called after shut_recv
                                     // and vice versa
    SLIRP_SCKT_STATUS_CLOSED,
} SlirpSocketStatus;

typedef enum {
    CLIENT_SCKT_STATUS_WAIT_OPEN,
    CLIENT_SCKT_STATUS_OPEN,
    CLIENT_SCKT_STATUS_SHUTDOWN_SEND, // FIN was issued from client
    CLIENT_SCKT_STATUS_CLOSED,
} ClientSocketStatus;

typedef struct TunnelService TunnelService;
struct RedSocket {
    int allocated;

    TunnelWorker *worker;

    uint16_t connection_id;

    uint16_t local_port;
    TunnelService *far_service;

    ClientSocketStatus client_status;
    SlirpSocketStatus slirp_status;

    int pushed_close;
    int client_waits_close_ack;

    SlirpSocket *slirp_sckt;

    RedSocketOutData out_data;
    RedSocketInData in_data;

    int in_slirp_send;

    uint32_t mig_client_status_msg;         // the last status change msg that was received from
                                            //the client during migration, and thus was unhandled.
                                            // It is 0 if the status didn't change during migration
    uint32_t mig_open_ack_tokens;           // if SPICE_MSGC_TUNNEL_SOCKET_OPEN_ACK was received during
                                            // migration, we store the tokens we received in the
                                            // msg.
};

/********** managing send buffers ***********/
static RawTunneledBuffer *tunnel_socket_alloc_snd_buf(RedSocket *sckt);
static inline RedSocketRawSndBuf *__tunnel_worker_alloc_socket_snd_buf(TunnelWorker *worker);
static RawTunneledBuffer *process_queue_alloc_snd_tunneled_buffer(
                                                          TunneledBufferProcessQueue *queue);

static void tunnel_socket_free_snd_buf(RedSocket *sckt, RedSocketRawSndBuf *snd_buf);
static inline void __tunnel_worker_free_socket_snd_buf(TunnelWorker *worker,
                                                       RedSocketRawSndBuf *snd_buf);
static void snd_tunnled_buffer_release(RawTunneledBuffer *buf);

/********** managing recv buffers ***********/
// receive buffers are allocated before we know to which socket they are directed.
static inline void tunnel_socket_assign_rcv_buf(RedSocket *sckt,
                                                RedSocketRawRcvBuf *recv_buf, int buf_size);
static inline RedSocketRawRcvBuf *__tunnel_worker_alloc_socket_rcv_buf(TunnelWorker *worker);

static void tunnel_socket_free_rcv_buf(RedSocket *sckt, RedSocketRawRcvBuf *rcv_buf);
static inline void __tunnel_worker_free_socket_rcv_buf(TunnelWorker *worker,
                                                       RedSocketRawRcvBuf *rcv_buf);
static void rcv_tunnled_buffer_release(RawTunneledBuffer *buf);

/********* managing buffers' queues ***********/

static void process_queue_simple_analysis(TunneledBufferProcessQueue *queue,
                                          RawTunneledBuffer *start_last_added,
                                          int offset, int len);
static inline TunneledBufferProcessQueue *__tunnel_socket_alloc_simple_process_queue(
                                                                        RedSocket *sckt,
                                                                        uint32_t service_type,
                                                                        uint32_t direction_type);
static TunneledBufferProcessQueue *tunnel_socket_alloc_simple_print_request_process_queue(
                                                                                RedSocket *sckt);
static TunneledBufferProcessQueue *tunnel_socket_alloc_simple_print_reply_process_queue(
                                                                                RedSocket *sckt);
static void free_simple_process_queue(TunneledBufferProcessQueue *queue);

typedef struct ServiceCallback {
    /* allocating the queue & setting the analysis proc by service type */
    TunneledBufferProcessQueue *(*alloc_process_queue)(RedSocket * sckt);
    void (*free_process_queue)(TunneledBufferProcessQueue *queue);
} ServiceCallback;

/* Callbacks for process queue manipulation according to the service type and
   the direction of the data.
   The access is performed by [service_type][direction] */
static const ServiceCallback SERVICES_CALLBACKS[3][2] = {
    {{NULL, NULL},
     {NULL, NULL}},
    {{tunnel_socket_alloc_simple_print_request_process_queue, free_simple_process_queue},
     {tunnel_socket_alloc_simple_print_reply_process_queue, free_simple_process_queue}},
    {{tunnel_socket_alloc_simple_print_request_process_queue, free_simple_process_queue},
     {tunnel_socket_alloc_simple_print_reply_process_queue, free_simple_process_queue}}
};

/****************************************************
*   Migration data
****************************************************/
typedef struct TunnelChannelClient TunnelChannelClient;

#define TUNNEL_MIGRATE_DATA_MAGIC (*(uint32_t *)"TMDA")
#define TUNNEL_MIGRATE_DATA_VERSION 1

#define TUNNEL_MIGRATE_NULL_OFFSET = ~0;

typedef struct __attribute__ ((__packed__)) TunnelMigrateSocketOutData {
    uint32_t num_tokens;
    uint32_t window_size;

    uint32_t process_buf_size;
    uint32_t process_buf;

    uint32_t process_queue_size;
    uint32_t process_queue;

    uint32_t ready_buf_size;
    uint32_t ready_buf;
} TunnelMigrateSocketOutData;

typedef struct __attribute__ ((__packed__)) TunnelMigrateSocketInData {
    int32_t num_tokens;
    uint32_t client_total_num_tokens;

    uint32_t process_buf_size;
    uint32_t process_buf;

    uint32_t process_queue_size;
    uint32_t process_queue;

    uint32_t ready_buf_size;
    uint32_t ready_buf;
} TunnelMigrateSocketInData;


typedef struct __attribute__ ((__packed__)) TunnelMigrateSocket {
    uint16_t connection_id;
    uint16_t local_port;
    uint32_t far_service_id;

    uint16_t client_status;
    uint16_t slirp_status;

    uint8_t pushed_close;
    uint8_t client_waits_close_ack;

    TunnelMigrateSocketOutData out_data;
    TunnelMigrateSocketInData in_data;

    uint32_t slirp_sckt;

    uint32_t mig_client_status_msg;
    uint32_t mig_open_ack_tokens;
} TunnelMigrateSocket;

typedef struct __attribute__ ((__packed__)) TunnelMigrateSocketList {
    uint16_t num_sockets;
    uint32_t sockets[0]; // offsets in TunnelMigrateData.data to TunnelMigrateSocket
} TunnelMigrateSocketList;

typedef struct __attribute__ ((__packed__)) TunnelMigrateService {
    uint32_t type;
    uint32_t id;
    uint32_t group;
    uint32_t port;
    uint32_t name;
    uint32_t description;
    uint8_t virt_ip[4];
} TunnelMigrateService;

typedef struct __attribute__ ((__packed__)) TunnelMigratePrintService {
    TunnelMigrateService base;
    uint8_t ip[4];
} TunnelMigratePrintService;

typedef struct __attribute__ ((__packed__)) TunnelMigrateServicesList {
    uint32_t num_services;
    uint32_t services[0];
} TunnelMigrateServicesList;

//todo: add ack_generation
typedef struct __attribute__ ((__packed__)) TunnelMigrateData {
    uint32_t magic;
    uint32_t version;
    uint64_t message_serial;

    uint32_t slirp_state;  // offset in data to slirp state
    uint32_t sockets_list; // offset in data to TunnelMigrateSocketList
    uint32_t services_list;

    uint8_t data[0];
} TunnelMigrateData;

typedef struct TunnelMigrateSocketItem {
    RedSocket *socket;
    TunnelMigrateSocket mig_socket;
    void *out_process_queue;
    void *in_process_queue; // queue data specific for service
    void *slirp_socket;
    uint32_t slirp_socket_size;
} TunnelMigrateSocketItem;

typedef struct TunnelMigrateServiceItem {
    TunnelService *service;
    union {
        TunnelMigrateService generic_service;
        TunnelMigratePrintService print_service;
    } u;
} TunnelMigrateServiceItem;

typedef struct TunnelMigrateItem {
    PipeItem base;

    void *slirp_state;
    uint64_t slirp_state_size;

    TunnelMigrateServicesList *services_list;
    uint32_t services_list_size;

    TunnelMigrateServiceItem *services;

    TunnelMigrateSocketList *sockets_list;
    uint32_t sockets_list_size;

    TunnelMigrateSocketItem sockets_data[MAX_SOCKETS_NUM];
} TunnelMigrateItem;

static inline void tunnel_channel_activate_migrated_sockets(TunnelChannelClient *channel);

/*******************************************************************************************/

/* use for signaling that 1) subroutines failed 2)routines in the interface for slirp
   failed (which triggered from a call to slirp) */
#define SET_TUNNEL_ERROR(channel,format, ...) {   \
    channel->tunnel_error = TRUE;                 \
    spice_printerr(format, ## __VA_ARGS__);           \
}

/* should be checked after each subroutine that may cause error or after calls to slirp routines */
#define CHECK_TUNNEL_ERROR(channel) (channel->tunnel_error)

struct TunnelChannelClient {
    RedChannelClient base;
    TunnelWorker *worker;
    int mig_inprogress;

    int tunnel_error;

    /* TODO: this needs to be RCC specific (or bad things will happen) */
    struct {
        union {
            SpiceMsgTunnelInit init;
            SpiceMsgTunnelServiceIpMap service_ip;
            SpiceMsgTunnelSocketOpen socket_open;
            SpiceMsgTunnelSocketFin socket_fin;
            SpiceMsgTunnelSocketClose socket_close;
            SpiceMsgTunnelSocketClosedAck socket_close_ack;
            SpiceMsgTunnelSocketData socket_data;
            SpiceMsgTunnelSocketTokens socket_token;
            TunnelMigrateData migrate_data;
            SpiceMsgMigrate migrate;
        } u;
    } send_data;

    uint8_t control_rcv_buf[CONTROL_MSG_RECV_BUF_SIZE];
};

typedef struct RedSlirpNetworkInterface {
    SlirpUsrNetworkInterface base;
    TunnelWorker *worker;
} RedSlirpNetworkInterface;

struct TunnelService {
    RingItem ring_item;
    PipeItem pipe_item;
    uint32_t type;
    uint32_t id;
    uint32_t group;
    uint32_t port;
    char *name;
    char *description;

    struct in_addr virt_ip;
};

typedef struct TunnelPrintService {
    TunnelService base;
    uint8_t ip[4];
} TunnelPrintService;

struct TunnelWorker {
    RedChannel *channel;
    TunnelChannelClient *channel_client;

    SpiceCoreInterface *core_interface;
    SpiceNetWireInstance *sin;
    SpiceNetWireInterface *sif;
    RedSlirpNetworkInterface tunnel_interface;
    RedSlirpNetworkInterface null_interface;

    RedSocket sockets[MAX_SOCKETS_NUM];         // the sockets are in the worker and not
                                                // in the channel since the slirp sockets
                                                // can be still alive (but during close) after
                                                // the channel was disconnected

    int num_sockets;

    RedSocketRawSndBuf *free_snd_buf;
    RedSocketRawRcvBuf *free_rcv_buf;

    Ring services;
    int num_services;
};


/*********************************************************************
 * Tunnel interface
 *********************************************************************/
static void tunnel_channel_on_disconnect(RedChannel *channel);

/* networking interface for slirp */
static int  qemu_can_output(SlirpUsrNetworkInterface *usr_interface);
static void qemu_output(SlirpUsrNetworkInterface *usr_interface, const uint8_t *pkt, int pkt_len);
static int null_tunnel_socket_connect(SlirpUsrNetworkInterface *usr_interface,
                                      struct in_addr src_addr, uint16_t src_port,
                                      struct in_addr dst_addr, uint16_t dst_port,
                                      SlirpSocket *slirp_s, UserSocket **o_usr_s);
static int tunnel_socket_connect(SlirpUsrNetworkInterface *usr_interface,
                                 struct in_addr src_addr, uint16_t src_port,
                                 struct in_addr dst_addr, uint16_t dst_port,
                                 SlirpSocket *slirp_s, UserSocket **o_usr_s);
static void null_tunnel_socket_close(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque);
static void tunnel_socket_close(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque);
static int null_tunnel_socket_send(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque,
                                   uint8_t *buf, size_t len, uint8_t urgent);
static int tunnel_socket_send(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque,
                              uint8_t *buf, size_t len, uint8_t urgent);
static int null_tunnel_socket_recv(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque,
                                   uint8_t *buf, size_t len);
static int tunnel_socket_recv(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque,
                              uint8_t *buf, size_t len);
static void null_tunnel_socket_shutdown_send(SlirpUsrNetworkInterface *usr_interface,
                                             UserSocket *opaque);
static void tunnel_socket_shutdown_send(SlirpUsrNetworkInterface *usr_interface,
                                        UserSocket *opaque);
static void null_tunnel_socket_shutdown_recv(SlirpUsrNetworkInterface *usr_interface,
                                             UserSocket *opaque);
static void tunnel_socket_shutdown_recv(SlirpUsrNetworkInterface *usr_interface,
                                        UserSocket *opaque);

static UserTimer *create_timer(SlirpUsrNetworkInterface *usr_interface,
                               timer_proc_t proc, void *opaque);
static void arm_timer(SlirpUsrNetworkInterface *usr_interface, UserTimer *timer, uint32_t ms);


/* RedChannel interface */

static void handle_tunnel_channel_link(RedChannel *channel, RedClient *client,
                                       RedsStream *stream, int migration,
                                       int num_common_caps,
                                       uint32_t *common_caps, int num_caps,
                                       uint32_t *caps);
static void handle_tunnel_channel_client_migrate(RedChannelClient *rcc);
static void red_tunnel_channel_create(TunnelWorker *worker);

static void tunnel_shutdown(TunnelWorker *worker)
{
    int i;
    spice_printerr("");
    /* shutdown input from channel */
    if (worker->channel_client) {
        red_channel_client_shutdown(&worker->channel_client->base);
    }

    /* shutdown socket pipe items */
    for (i = 0; i < MAX_SOCKETS_NUM; i++) {
        RedSocket *sckt = worker->sockets + i;
        if (sckt->allocated) {
            sckt->client_status = CLIENT_SCKT_STATUS_CLOSED;
            sckt->client_waits_close_ack = FALSE;
        }
    }

    /* shutdown input from slirp */
    net_slirp_set_net_interface(&worker->null_interface.base);
}

/*****************************************************************
* Managing raw tunneled buffers storage
******************************************************************/

/********** send buffers ***********/
static RawTunneledBuffer *tunnel_socket_alloc_snd_buf(RedSocket *sckt)
{
    RedSocketRawSndBuf *ret = __tunnel_worker_alloc_socket_snd_buf(sckt->worker);
    ret->base.usr_opaque = sckt;
    ret->base.release_proc = snd_tunnled_buffer_release;
    sckt->out_data.num_buffers++;
    return &ret->base;
}

static inline RedSocketRawSndBuf *__tunnel_worker_alloc_socket_snd_buf(TunnelWorker *worker)
{
    RedSocketRawSndBuf *ret;
    if (worker->free_snd_buf) {
        ret = worker->free_snd_buf;
        worker->free_snd_buf = (RedSocketRawSndBuf *)worker->free_snd_buf->base.next;
    } else {
        ret = spice_new(RedSocketRawSndBuf, 1);
    }
    ret->base.data = ret->buf;
    ret->base.size = 0;
    ret->base.max_size = MAX_SOCKET_DATA_SIZE;
    ret->base.usr_opaque = NULL;
    ret->base.refs = 1;
    ret->base.next = NULL;

    return ret;
}

static void tunnel_socket_free_snd_buf(RedSocket *sckt, RedSocketRawSndBuf *snd_buf)
{
    sckt->out_data.num_buffers--;
    __tunnel_worker_free_socket_snd_buf(sckt->worker, snd_buf);
}

static inline void __tunnel_worker_free_socket_snd_buf(TunnelWorker *worker,
                                                       RedSocketRawSndBuf *snd_buf)
{
    snd_buf->base.size = 0;
    snd_buf->base.next = &worker->free_snd_buf->base;
    worker->free_snd_buf = snd_buf;
}

static RawTunneledBuffer *process_queue_alloc_snd_tunneled_buffer(TunneledBufferProcessQueue *queue)
{
    return tunnel_socket_alloc_snd_buf((RedSocket *)queue->usr_opaque);
}

static void snd_tunnled_buffer_release(RawTunneledBuffer *buf)
{
    tunnel_socket_free_snd_buf((RedSocket *)buf->usr_opaque, (RedSocketRawSndBuf *)buf);
}

/********** recv buffers ***********/

static inline void tunnel_socket_assign_rcv_buf(RedSocket *sckt,
                                                RedSocketRawRcvBuf *recv_buf, int buf_size)
{
    spice_assert(!recv_buf->base.usr_opaque);
    // the rcv buffer was allocated by tunnel_channel_alloc_msg_rcv_buf
    // before we could know which of the sockets it belongs to, so the
    // assignment to the socket is performed now
    recv_buf->base.size = buf_size;
    recv_buf->base.usr_opaque = sckt;
    recv_buf->base.release_proc = rcv_tunnled_buffer_release;
    sckt->in_data.num_buffers++;
    process_queue_push(sckt->in_data.process_queue, &recv_buf->base);
}

static inline RedSocketRawRcvBuf *__tunnel_worker_alloc_socket_rcv_buf(TunnelWorker *worker)
{
    RedSocketRawRcvBuf *ret;
    if (worker->free_rcv_buf) {
        ret = worker->free_rcv_buf;
        worker->free_rcv_buf = (RedSocketRawRcvBuf *)worker->free_rcv_buf->base.next;
    } else {
        ret = spice_new(RedSocketRawRcvBuf, 1);
    }
    ret->msg_info = (SpiceMsgcTunnelSocketData *)ret->buf;
    ret->base.usr_opaque = NULL;
    ret->base.data = ret->msg_info->data;
    ret->base.size = 0;
    ret->base.max_size = MAX_SOCKET_DATA_SIZE;
    ret->base.refs = 1;
    ret->base.next = NULL;

    return ret;
}

static inline void __process_rcv_buf_tokens(TunnelChannelClient *channel, RedSocket *sckt)
{
    if ((sckt->client_status != CLIENT_SCKT_STATUS_OPEN) || red_channel_client_pipe_item_is_linked(
            &channel->base, &sckt->out_data.token_pipe_item) || channel->mig_inprogress) {
        return;
    }

    if ((sckt->in_data.num_tokens >= SOCKET_TOKENS_TO_SEND) ||
        (!sckt->in_data.client_total_num_tokens && !sckt->in_data.ready_chunks_queue.head)) {
        sckt->out_data.token_pipe_item.type = PIPE_ITEM_TYPE_SOCKET_TOKEN;
        red_channel_client_pipe_add(&channel->base, &sckt->out_data.token_pipe_item);
    }
}

static void tunnel_socket_free_rcv_buf(RedSocket *sckt, RedSocketRawRcvBuf *rcv_buf)
{
    --sckt->in_data.num_buffers;
    __tunnel_worker_free_socket_rcv_buf(sckt->worker, rcv_buf);
    ++sckt->in_data.num_tokens;
    __process_rcv_buf_tokens(sckt->worker->channel_client, sckt);
}

static inline void __tunnel_worker_free_socket_rcv_buf(TunnelWorker *worker,
                                                       RedSocketRawRcvBuf *rcv_buf)
{
    rcv_buf->base.next = &worker->free_rcv_buf->base;
    worker->free_rcv_buf = rcv_buf;
}

static void rcv_tunnled_buffer_release(RawTunneledBuffer *buf)
{
    tunnel_socket_free_rcv_buf((RedSocket *)buf->usr_opaque,
                               (RedSocketRawRcvBuf *)buf);
}

/************************
*  Process & Ready queue
*************************/

static inline void __process_queue_push(TunneledBufferProcessQueue *queue, RawTunneledBuffer *buf)
{
    buf->next = NULL;
    if (!queue->head) {
        queue->head = buf;
        queue->tail = buf;
    } else {
        queue->tail->next = buf;
        queue->tail = buf;
    }
}

static void process_queue_push(TunneledBufferProcessQueue *queue, RawTunneledBuffer *buf)
{
    __process_queue_push(queue, buf);
    queue->analysis_proc(queue, buf, 0, buf->size);
}

static void process_queue_append(TunneledBufferProcessQueue *queue, uint8_t *data, size_t size)
{
    RawTunneledBuffer *start_buf = NULL;
    int start_offset = 0;
    int copied = 0;

    if (queue->tail) {
        RawTunneledBuffer *buf = queue->tail;
        int space = buf->max_size - buf->size;
        if (space) {
            int copy_count = MIN(size, space);
            start_buf = buf;
            start_offset = buf->size;
            memcpy(buf->data + buf->size, data, copy_count);
            copied += copy_count;
            buf->size += copy_count;
        }
    }


    while (copied < size) {
        RawTunneledBuffer *buf = queue->alloc_buf_proc(queue);
        int copy_count = MIN(size - copied, buf->max_size);
        memcpy(buf->data, data + copied, copy_count);
        copied += copy_count;
        buf->size = copy_count;

        __process_queue_push(queue, buf);

        if (!start_buf) {
            start_buf = buf;
            start_offset = 0;
        }
    }

    queue->analysis_proc(queue, start_buf, start_offset, size);
}

static void process_queue_pop(TunneledBufferProcessQueue *queue)
{
    RawTunneledBuffer *prev_head;
    spice_assert(queue->head && queue->tail);
    prev_head = queue->head;
    queue->head = queue->head->next;
    if (!queue->head) {
        queue->tail = NULL;
    }

    tunneled_buffer_unref(prev_head);
}

static void process_queue_clear(TunneledBufferProcessQueue *queue)
{
    while (queue->head) {
        process_queue_pop(queue);
    }
}

static void __ready_queue_push(ReadyTunneledChunkQueue *queue, ReadyTunneledChunk *chunk)
{
    chunk->next = NULL;
    if (queue->tail) {
        queue->tail->next = chunk;
        queue->tail = chunk;
    } else {
        queue->head = chunk;
        queue->tail = chunk;
    }
}

static void ready_queue_add_orig_chunk(ReadyTunneledChunkQueue *queue, RawTunneledBuffer *origin,
                                       uint8_t *data, int size)
{
    ReadyTunneledChunk *chunk = spice_new(ReadyTunneledChunk, 1);
    chunk->type = READY_TUNNELED_CHUNK_TYPE_ORIG;
    chunk->origin = tunneled_buffer_ref(origin);
    chunk->data = data;
    chunk->size = size;

    __ready_queue_push(queue, chunk);
}

static void ready_queue_pop_chunk(ReadyTunneledChunkQueue *queue)
{
    ReadyTunneledChunk *chunk = queue->head;
    spice_assert(queue->head);
    queue->head = queue->head->next;

    if (!queue->head) {
        queue->tail = NULL;
    }

    tunneled_buffer_unref(chunk->origin);
    if (chunk->type != READY_TUNNELED_CHUNK_TYPE_ORIG) {
        free(chunk->data);
    }
    free(chunk);
}

static void ready_queue_clear(ReadyTunneledChunkQueue *queue)
{
    while (queue->head) {
        ready_queue_pop_chunk(queue);
    }
}

static void process_queue_simple_analysis(TunneledBufferProcessQueue *queue,
                                          RawTunneledBuffer *start_last_added, int offset, int len)
{
    spice_assert(offset == 0);
    spice_assert(start_last_added == queue->head);

    while (queue->head) {
        ready_queue_add_orig_chunk(queue->ready_chunks_queue, queue->head, queue->head->data,
                                   queue->head->size);
        process_queue_pop(queue);
    }
}

static int process_queue_simple_get_migrate_data(TunneledBufferProcessQueue *queue,
                                                 void **migrate_data)
{
    *migrate_data = NULL;
    return 0;
}

static void process_queue_simple_release_migrate_data(TunneledBufferProcessQueue *queue,
                                                      void *migrate_data)
{
    spice_assert(!migrate_data);
}

static void process_queue_simple_restore(TunneledBufferProcessQueue *queue, uint8_t *migrate_data)
{
}

static inline TunneledBufferProcessQueue *__tunnel_socket_alloc_simple_process_queue(
                                                                            RedSocket *sckt,
                                                                            uint32_t service_type,
                                                                            uint32_t direction_type)
{
    TunneledBufferProcessQueue *ret_queue = spice_new0(TunneledBufferProcessQueue, 1);
    ret_queue->service_type = service_type;
    ret_queue->direction = direction_type;
    ret_queue->usr_opaque = sckt;
    // NO need for allocations by the process queue when getting replies. The buffer is created
    // when the msg is received
    if (direction_type == PROCESS_DIRECTION_TYPE_REQUEST) {
        ret_queue->alloc_buf_proc = process_queue_alloc_snd_tunneled_buffer;
        ret_queue->ready_chunks_queue = &sckt->out_data.ready_chunks_queue;
    } else {
        ret_queue->ready_chunks_queue = &sckt->in_data.ready_chunks_queue;
    }

    ret_queue->analysis_proc = process_queue_simple_analysis;

    ret_queue->get_migrate_data_proc = process_queue_simple_get_migrate_data;
    ret_queue->release_migrate_data_proc = process_queue_simple_release_migrate_data;
    ret_queue->restore_proc = process_queue_simple_restore;
    return ret_queue;
}

static void free_simple_process_queue(TunneledBufferProcessQueue *queue)
{
    process_queue_clear(queue);
    free(queue);
}

static TunneledBufferProcessQueue *tunnel_socket_alloc_simple_print_request_process_queue(
                                                                                 RedSocket *sckt)
{
    return __tunnel_socket_alloc_simple_process_queue(sckt,
                                                      SPICE_TUNNEL_SERVICE_TYPE_IPP,
                                                      PROCESS_DIRECTION_TYPE_REQUEST);
}

static TunneledBufferProcessQueue *tunnel_socket_alloc_simple_print_reply_process_queue(
                                                                                  RedSocket *sckt)
{
    return __tunnel_socket_alloc_simple_process_queue(sckt,
                                                      SPICE_TUNNEL_SERVICE_TYPE_IPP,
                                                      PROCESS_DIRECTION_TYPE_REPLY);
}

SPICE_GNUC_VISIBLE void spice_server_net_wire_recv_packet(SpiceNetWireInstance *sin,
                                                          const uint8_t *pkt, int pkt_len)
{
    TunnelWorker *worker = sin->st->worker;
    spice_assert(worker);

    if (worker->channel_client && worker->channel_client->mig_inprogress) {
        return; // during migration and the tunnel state hasn't been restored yet.
    }

    net_slirp_input(pkt, pkt_len);
}

void *red_tunnel_attach(SpiceCoreInterface *core_interface,
                        SpiceNetWireInstance *sin)
{
    TunnelWorker *worker = spice_new0(TunnelWorker, 1);

    worker->core_interface = core_interface;
    worker->sin = sin;
    worker->sin->st->worker = worker;
    worker->sif = SPICE_CONTAINEROF(sin->base.sif, SpiceNetWireInterface, base);

    worker->tunnel_interface.base.slirp_can_output = qemu_can_output;
    worker->tunnel_interface.base.slirp_output = qemu_output;
    worker->tunnel_interface.base.connect = tunnel_socket_connect;
    worker->tunnel_interface.base.send = tunnel_socket_send;
    worker->tunnel_interface.base.recv = tunnel_socket_recv;
    worker->tunnel_interface.base.close = tunnel_socket_close;
    worker->tunnel_interface.base.shutdown_recv = tunnel_socket_shutdown_recv;
    worker->tunnel_interface.base.shutdown_send = tunnel_socket_shutdown_send;
    worker->tunnel_interface.base.create_timer = create_timer;
    worker->tunnel_interface.base.arm_timer = arm_timer;

    worker->tunnel_interface.worker = worker;

    worker->null_interface.base.slirp_can_output = qemu_can_output;
    worker->null_interface.base.slirp_output = qemu_output;
    worker->null_interface.base.connect = null_tunnel_socket_connect;
    worker->null_interface.base.send = null_tunnel_socket_send;
    worker->null_interface.base.recv = null_tunnel_socket_recv;
    worker->null_interface.base.close = null_tunnel_socket_close;
    worker->null_interface.base.shutdown_recv = null_tunnel_socket_shutdown_recv;
    worker->null_interface.base.shutdown_send = null_tunnel_socket_shutdown_send;
    worker->null_interface.base.create_timer = create_timer;
    worker->null_interface.base.arm_timer = arm_timer;

    worker->null_interface.worker = worker;

    red_tunnel_channel_create(worker);

   ring_init(&worker->services);

    net_slirp_init(worker->sif->get_ip(worker->sin),
                   TRUE,
                   &worker->null_interface.base);
    return worker;
}

/* returns the first service that has the same group id (NULL if not found) */
static inline TunnelService *__tunnel_worker_find_service_of_group(TunnelWorker *worker,
                                                                   uint32_t group)
{
    TunnelService *service;
    for (service = (TunnelService *)ring_get_head(&worker->services);
         service;
         service = (TunnelService *)ring_next(&worker->services, &service->ring_item)) {
        if (service->group == group) {
            return service;
        }
    }

    return NULL;
}

static inline TunnelService *__tunnel_worker_add_service(TunnelWorker *worker, uint32_t size,
                                                         uint32_t type, uint32_t id,
                                                         uint32_t group, uint32_t port,
                                                         char *name, char *description,
                                                         struct in_addr *virt_ip)
{
    TunnelService *new_service = spice_malloc0(size);

    if (!virt_ip) {
        TunnelService *service_of_same_group;
        if (!(service_of_same_group = __tunnel_worker_find_service_of_group(worker, group))) {
            if (!net_slirp_allocate_virtual_ip(&new_service->virt_ip)) {
                spice_printerr("failed to allocate virtual ip");
                free(new_service);
                return NULL;
            }
        } else {
            if (strcmp(name, service_of_same_group->name) == 0) {
                new_service->virt_ip.s_addr = service_of_same_group->virt_ip.s_addr;
            } else {
                spice_printerr("inconsistent name for service group %d", group);
                free(new_service);
                return NULL;
            }
        }
    } else {
        new_service->virt_ip.s_addr = virt_ip->s_addr;
    }

    ring_item_init(&new_service->ring_item);
    new_service->type = type;
    new_service->id = id;
    new_service->group = group;
    new_service->port = port;

    new_service->name = spice_strdup(name);
    new_service->description = spice_strdup(description);

    ring_add(&worker->services, &new_service->ring_item);
    worker->num_services++;

#ifdef DEBUG_NETWORK
    spice_printerr("TUNNEL_DBG: ==>SERVICE ADDED: id=%d virt ip=%s port=%d name=%s desc=%s",
               new_service->id, inet_ntoa(new_service->virt_ip),
               new_service->port, new_service->name, new_service->description);
#endif
    if (!virt_ip) {
        new_service->pipe_item.type = PIPE_ITEM_TYPE_SERVICE_IP_MAP;
        red_channel_client_pipe_add(&worker->channel_client->base, &new_service->pipe_item);
    }

    return new_service;
}

static TunnelService *tunnel_worker_add_service(TunnelWorker *worker, uint32_t size,
                                                SpiceMsgcTunnelAddGenericService *redc_service)
{
    return __tunnel_worker_add_service(worker, size, redc_service->type,
                                       redc_service->id, redc_service->group,
                                       redc_service->port,
                                       (char *)(((uint8_t *)redc_service) +
                                                redc_service->name),
                                       (char *)(((uint8_t *)redc_service) +
                                                redc_service->description), NULL);
}

static inline void tunnel_worker_free_service(TunnelWorker *worker, TunnelService *service)
{
    ring_remove(&service->ring_item);
    free(service->name);
    free(service->description);
    free(service);
    worker->num_services--;
}

static void tunnel_worker_free_print_service(TunnelWorker *worker, TunnelPrintService *service)
{
    tunnel_worker_free_service(worker, &service->base);
}

static TunnelPrintService *tunnel_worker_add_print_service(TunnelWorker *worker,
                                                           SpiceMsgcTunnelAddGenericService *redc_service)
{
    TunnelPrintService *service;

    service = (TunnelPrintService *)tunnel_worker_add_service(worker, sizeof(TunnelPrintService),
                                                              redc_service);

    if (!service) {
        return NULL;
    }

    if (redc_service->type == SPICE_TUNNEL_IP_TYPE_IPv4) {
        memcpy(service->ip, redc_service->u.ip.data, sizeof(SpiceTunnelIPv4));
    } else {
        spice_printerr("unexpected ip type=%d", redc_service->type);
        tunnel_worker_free_print_service(worker, service);
        return NULL;
    }
#ifdef DEBUG_NETWORK
    spice_printerr("TUNNEL_DBG: ==>PRINT SERVICE ADDED: ip=%d.%d.%d.%d", service->ip[0],
               service->ip[1], service->ip[2], service->ip[3]);
#endif
    return service;
}

static int tunnel_channel_handle_service_add(TunnelChannelClient *channel,
                                             SpiceMsgcTunnelAddGenericService *service_msg)
{
    TunnelService *out_service = NULL;
    if (service_msg->type == SPICE_TUNNEL_SERVICE_TYPE_IPP) {
        out_service = &tunnel_worker_add_print_service(channel->worker,
                                                       service_msg)->base;
    } else if (service_msg->type == SPICE_TUNNEL_SERVICE_TYPE_GENERIC) {
        out_service = tunnel_worker_add_service(channel->worker, sizeof(TunnelService),
                                                service_msg);
    } else {
        spice_printerr("invalid service type");
    }

    free(service_msg);
    return (out_service != NULL);
}

static inline TunnelService *tunnel_worker_find_service_by_id(TunnelWorker *worker, uint32_t id)
{
    TunnelService *service;
    for (service = (TunnelService *)ring_get_head(&worker->services);
         service;
         service = (TunnelService *)ring_next(&worker->services, &service->ring_item)) {
        if (service->id == id) {
            return service;
        }
    }

    return NULL;
}

static inline TunnelService *tunnel_worker_find_service_by_addr(TunnelWorker *worker,
                                                                struct in_addr *virt_ip,
                                                                uint32_t port)
{
    TunnelService *service;
    for (service = (TunnelService *)ring_get_head(&worker->services);
         service;
         service = (TunnelService *)ring_next(&worker->services, &service->ring_item)) {
        if ((virt_ip->s_addr == service->virt_ip.s_addr) && (port == service->port)) {
            return service;
        }
    }

    return NULL;
}

static inline void tunnel_worker_clear_routed_network(TunnelWorker *worker)
{
    while (!ring_is_empty(&worker->services)) {
        TunnelService *service = (TunnelService *)ring_get_head(&worker->services);
        if (service->type == SPICE_TUNNEL_SERVICE_TYPE_GENERIC) {
            tunnel_worker_free_service(worker, service);
        } else if (service->type == SPICE_TUNNEL_SERVICE_TYPE_IPP) {
            tunnel_worker_free_print_service(worker, (TunnelPrintService *)service);
        } else {
            spice_error("unexpected service type");
        }
    }

    net_slirp_clear_virtual_ips();
}

static inline RedSocket *__tunnel_worker_find_free_socket(TunnelWorker *worker)
{
    int i;
    RedSocket *ret = NULL;

    if (worker->num_sockets == MAX_SOCKETS_NUM) {
        return NULL;
    }

    for (i = 0; i < MAX_SOCKETS_NUM; i++) {
        if (!worker->sockets[i].allocated) {
            ret = worker->sockets + i;
            ret->connection_id = i;
            break;
        }
    }

    spice_assert(ret);
    return ret;
}

static inline void __tunnel_worker_add_socket(TunnelWorker *worker, RedSocket *sckt)
{
    spice_assert(!sckt->allocated);
    sckt->allocated = TRUE;
    worker->num_sockets++;
}

static inline void tunnel_worker_alloc_socket(TunnelWorker *worker, RedSocket *sckt,
                                              uint16_t local_port, TunnelService *far_service,
                                              SlirpSocket *slirp_s)
{
    spice_assert(far_service);
    sckt->worker = worker;
    sckt->local_port = local_port;
    sckt->far_service = far_service;
    sckt->out_data.num_tokens = 0;

    sckt->slirp_status = SLIRP_SCKT_STATUS_OPEN;
    sckt->client_status = CLIENT_SCKT_STATUS_WAIT_OPEN;
    sckt->slirp_sckt = slirp_s;

    sckt->out_data.process_queue = SERVICES_CALLBACKS[far_service->type][
            PROCESS_DIRECTION_TYPE_REQUEST].alloc_process_queue(sckt);
    sckt->in_data.process_queue = SERVICES_CALLBACKS[far_service->type][
            PROCESS_DIRECTION_TYPE_REPLY].alloc_process_queue(sckt);
    __tunnel_worker_add_socket(worker, sckt);
}

static inline void __tunnel_worker_free_socket(TunnelWorker *worker, RedSocket *sckt)
{
    memset(sckt, 0, sizeof(*sckt));
    worker->num_sockets--;
}

static RedSocket *tunnel_worker_create_socket(TunnelWorker *worker, uint16_t local_port,
                                              TunnelService *far_service,
                                              SlirpSocket *slirp_s)
{
    RedSocket *new_socket;
    spice_assert(worker);
    new_socket = __tunnel_worker_find_free_socket(worker);

    if (!new_socket) {
        spice_error("creation of RedSocket failed");
    }

    tunnel_worker_alloc_socket(worker, new_socket, local_port, far_service, slirp_s);

    return new_socket;
}

static void tunnel_worker_free_socket(TunnelWorker *worker, RedSocket *sckt)
{
    if (worker->channel_client) {
        if (red_channel_client_pipe_item_is_linked(&worker->channel_client->base,
                                            &sckt->out_data.data_pipe_item)) {
            red_channel_client_pipe_remove_and_release(&worker->channel_client->base,
                                         &sckt->out_data.data_pipe_item);
            return;
        }

        if (red_channel_client_pipe_item_is_linked(&worker->channel_client->base,
                                            &sckt->out_data.status_pipe_item)) {
            red_channel_client_pipe_remove_and_release(&worker->channel_client->base,
                                         &sckt->out_data.status_pipe_item);
            return;
        }

        if (red_channel_client_pipe_item_is_linked(&worker->channel_client->base,
                                            &sckt->out_data.token_pipe_item)) {
            red_channel_client_pipe_remove_and_release(&worker->channel_client->base,
                                         &sckt->out_data.token_pipe_item);
            return;
        }
    }

    SERVICES_CALLBACKS[sckt->far_service->type][
        PROCESS_DIRECTION_TYPE_REQUEST].free_process_queue(sckt->out_data.process_queue);
    SERVICES_CALLBACKS[sckt->far_service->type][
        PROCESS_DIRECTION_TYPE_REPLY].free_process_queue(sckt->in_data.process_queue);

    ready_queue_clear(&sckt->out_data.ready_chunks_queue);
    ready_queue_clear(&sckt->in_data.ready_chunks_queue);

    __tunnel_worker_free_socket(worker, sckt);
}

static inline RedSocket *tunnel_worker_find_socket(TunnelWorker *worker,
                                                   uint16_t local_port,
                                                   uint32_t far_service_id)
{
    RedSocket *sckt;
    int allocated = 0;

    for (sckt = worker->sockets; allocated < worker->num_sockets; sckt++) {
        if (sckt->allocated) {
            allocated++;
            if ((sckt->local_port == local_port) &&
                (sckt->far_service->id == far_service_id)) {
                return sckt;
            }
        }
    }
    return NULL;
}

static inline void __tunnel_socket_add_fin_to_pipe(TunnelChannelClient *channel, RedSocket *sckt)
{
    spice_assert(!red_channel_client_pipe_item_is_linked(&channel->base, &sckt->out_data.status_pipe_item));
    sckt->out_data.status_pipe_item.type = PIPE_ITEM_TYPE_SOCKET_FIN;
    red_channel_client_pipe_add(&channel->base, &sckt->out_data.status_pipe_item);
}

static inline void __tunnel_socket_add_close_to_pipe(TunnelChannelClient *channel, RedSocket *sckt)
{
    spice_assert(!channel->mig_inprogress);

    if (red_channel_client_pipe_item_is_linked(&channel->base, &sckt->out_data.status_pipe_item)) {
        spice_assert(sckt->out_data.status_pipe_item.type == PIPE_ITEM_TYPE_SOCKET_FIN);
        // close is stronger than FIN
        red_channel_client_pipe_remove_and_release(&channel->base,
                                        &sckt->out_data.status_pipe_item);
    }
    sckt->pushed_close = TRUE;
    sckt->out_data.status_pipe_item.type = PIPE_ITEM_TYPE_SOCKET_CLOSE;
    red_channel_client_pipe_add(&channel->base, &sckt->out_data.status_pipe_item);
}

static inline void __tunnel_socket_add_close_ack_to_pipe(TunnelChannelClient *channel, RedSocket *sckt)
{
    spice_assert(!channel->mig_inprogress);

    if (red_channel_client_pipe_item_is_linked(&channel->base, &sckt->out_data.status_pipe_item)) {
        spice_assert(sckt->out_data.status_pipe_item.type == PIPE_ITEM_TYPE_SOCKET_FIN);
        // close is stronger than FIN
        red_channel_client_pipe_remove_and_release(&channel->base,
                                    &sckt->out_data.status_pipe_item);
    }

    sckt->out_data.status_pipe_item.type = PIPE_ITEM_TYPE_SOCKET_CLOSED_ACK;
    red_channel_client_pipe_add(&channel->base, &sckt->out_data.status_pipe_item);
}

/*
    Send close msg to the client.
    If possible, notify slirp to recv data (which will return 0)
    When close ack is received from client, we notify slirp (maybe again) if needed.
*/
static void tunnel_socket_force_close(TunnelChannelClient *channel, RedSocket *sckt)
{
    if (red_channel_client_pipe_item_is_linked(&channel->base, &sckt->out_data.token_pipe_item)) {
        red_channel_client_pipe_remove_and_release(&channel->base, &sckt->out_data.token_pipe_item);
    }

    if (red_channel_client_pipe_item_is_linked(&channel->base, &sckt->out_data.data_pipe_item)) {
        red_channel_client_pipe_remove_and_release(&channel->base, &sckt->out_data.data_pipe_item);
    }


    if ((sckt->client_status != CLIENT_SCKT_STATUS_CLOSED) ||
        !sckt->pushed_close) {
        __tunnel_socket_add_close_to_pipe(channel, sckt);
    }

    // we can't call net_slirp_socket_can_receive_notify if the forced close was initiated by
    // tunnel_socket_send (which was called from slirp). Instead, when
    // we receive the close ack from the client, we call net_slirp_socket_can_receive_notify
    if (sckt->slirp_status != SLIRP_SCKT_STATUS_CLOSED) {
        if (!sckt->in_slirp_send) {
            sckt->slirp_status = SLIRP_SCKT_STATUS_WAIT_CLOSE;
            net_slirp_socket_abort(sckt->slirp_sckt);
        } else {
            sckt->slirp_status = SLIRP_SCKT_STATUS_DELAY_ABORT;
        }
    }
}

static int tunnel_channel_handle_socket_connect_ack(TunnelChannelClient *channel, RedSocket *sckt,
                                                    uint32_t tokens)
{
#ifdef DEBUG_NETWORK
    spice_printerr("TUNNEL_DBG");
#endif
    if (channel->mig_inprogress) {
        sckt->mig_client_status_msg = SPICE_MSGC_TUNNEL_SOCKET_OPEN_ACK;
        sckt->mig_open_ack_tokens = tokens;
        return TRUE;
    }

    if (sckt->client_status != CLIENT_SCKT_STATUS_WAIT_OPEN) {
        spice_printerr("unexpected SPICE_MSGC_TUNNEL_SOCKET_OPEN_ACK status=%d", sckt->client_status);
        return FALSE;
    }
    sckt->client_status = CLIENT_SCKT_STATUS_OPEN;

    // SLIRP_SCKT_STATUS_CLOSED is possible after waiting for a connection has timed out
    if (sckt->slirp_status == SLIRP_SCKT_STATUS_CLOSED) {
        spice_assert(!sckt->pushed_close);
        __tunnel_socket_add_close_to_pipe(channel, sckt);
    } else if (sckt->slirp_status == SLIRP_SCKT_STATUS_OPEN) {
        sckt->out_data.window_size = tokens;
        sckt->out_data.num_tokens = tokens;
        net_slirp_socket_connected_notify(sckt->slirp_sckt);
    } else {
        spice_printerr("unexpected slirp status status=%d", sckt->slirp_status);
        return FALSE;
    }

    return (!CHECK_TUNNEL_ERROR(channel));
}

static int tunnel_channel_handle_socket_connect_nack(TunnelChannelClient *channel, RedSocket *sckt)
{
#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif
    if (channel->mig_inprogress) {
        sckt->mig_client_status_msg = SPICE_MSGC_TUNNEL_SOCKET_OPEN_NACK;
        return TRUE;
    }

    if (sckt->client_status != CLIENT_SCKT_STATUS_WAIT_OPEN) {
        spice_printerr("unexpected SPICE_MSGC_TUNNEL_SOCKET_OPEN_NACK status=%d", sckt->client_status);
        return FALSE;
    }
    sckt->client_status = CLIENT_SCKT_STATUS_CLOSED;

    if (sckt->slirp_status != SLIRP_SCKT_STATUS_CLOSED) {
        net_slirp_socket_connect_failed_notify(sckt->slirp_sckt);
    } else {
        tunnel_worker_free_socket(channel->worker, sckt);
    }

    return (!CHECK_TUNNEL_ERROR(channel));
}

static int tunnel_channel_handle_socket_fin(TunnelChannelClient *channel, RedSocket *sckt)
{
#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif
    if (channel->mig_inprogress) {
        sckt->mig_client_status_msg = SPICE_MSGC_TUNNEL_SOCKET_FIN;
        return TRUE;
    }

    if (sckt->client_status != CLIENT_SCKT_STATUS_OPEN) {
        spice_printerr("unexpected SPICE_MSGC_TUNNEL_SOCKET_FIN status=%d", sckt->client_status);
        return FALSE;
    }
    sckt->client_status = CLIENT_SCKT_STATUS_SHUTDOWN_SEND;

    if ((sckt->slirp_status == SLIRP_SCKT_STATUS_CLOSED) ||
        (sckt->slirp_status == SLIRP_SCKT_STATUS_DELAY_ABORT)) {
        return TRUE;
    }

    if ((sckt->slirp_status == SLIRP_SCKT_STATUS_OPEN) ||
        (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_SEND)) {
        // After slirp will receive all the data buffers, the next recv
        // will return an error and shutdown_recv should be called.
        net_slirp_socket_can_receive_notify(sckt->slirp_sckt);
    } else if (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_RECV) {
        // it already received the FIN
        spice_printerr("unexpected slirp status=%d", sckt->slirp_status);
        return FALSE;
    }

    return (!CHECK_TUNNEL_ERROR(channel));
}

static int tunnel_channel_handle_socket_closed(TunnelChannelClient *channel, RedSocket *sckt)
{
    int prev_client_status = sckt->client_status;

#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif

    if (channel->mig_inprogress) {
        sckt->mig_client_status_msg = SPICE_MSGC_TUNNEL_SOCKET_CLOSED;
        return TRUE;
    }

    sckt->client_status = CLIENT_SCKT_STATUS_CLOSED;

    if (sckt->slirp_status == SLIRP_SCKT_STATUS_CLOSED) {
        // if we already pushed close to the client, we expect it to send us ack.
        // Otherwise, we will send it an ack.
        if (!sckt->pushed_close) {
            sckt->client_waits_close_ack = TRUE;
            __tunnel_socket_add_close_ack_to_pipe(channel, sckt);
        }

        return (!CHECK_TUNNEL_ERROR(channel));
    }

    // close was initiated by client
    sckt->client_waits_close_ack = TRUE;

    if (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_SEND) {
        // guest waits for fin: after slirp will receive all the data buffers,
        // the next recv will return an error and shutdown_recv should be called.
        net_slirp_socket_can_receive_notify(sckt->slirp_sckt);
    } else if ((sckt->slirp_status == SLIRP_SCKT_STATUS_OPEN) ||
               (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_RECV)) {
        sckt->slirp_status = SLIRP_SCKT_STATUS_WAIT_CLOSE;
        net_slirp_socket_abort(sckt->slirp_sckt);
    } else if ((sckt->slirp_status != SLIRP_SCKT_STATUS_WAIT_CLOSE) ||
               (prev_client_status != CLIENT_SCKT_STATUS_SHUTDOWN_SEND)) {
        // slirp can be in wait close if both slirp and client sent fin previously
        // otherwise, the prev client status would also have been wait close, and this
        // case was handled above
        spice_printerr("unexpected slirp_status=%d", sckt->slirp_status);
        return FALSE;
    }

    return (!CHECK_TUNNEL_ERROR(channel));
}

static int tunnel_channel_handle_socket_closed_ack(TunnelChannelClient *channel, RedSocket *sckt)
{
#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif
    if (channel->mig_inprogress) {
        sckt->mig_client_status_msg = SPICE_MSGC_TUNNEL_SOCKET_CLOSED_ACK;
        return TRUE;
    }

    sckt->client_status = CLIENT_SCKT_STATUS_CLOSED;
    if (sckt->slirp_status == SLIRP_SCKT_STATUS_DELAY_ABORT) {
        sckt->slirp_status = SLIRP_SCKT_STATUS_WAIT_CLOSE;
        net_slirp_socket_abort(sckt->slirp_sckt);
        return (!CHECK_TUNNEL_ERROR(channel));
    }

    if (sckt->slirp_status != SLIRP_SCKT_STATUS_CLOSED) {
        spice_printerr("unexpected SPICE_MSGC_TUNNEL_SOCKET_CLOSED_ACK slirp_status=%d",
                   sckt->slirp_status);
        return FALSE;
    }

    tunnel_worker_free_socket(channel->worker, sckt);
    return (!CHECK_TUNNEL_ERROR(channel));
}

static int tunnel_channel_handle_socket_receive_data(TunnelChannelClient *channel, RedSocket *sckt,
                                                     RedSocketRawRcvBuf *recv_data, int buf_size)
{
    if ((sckt->client_status == CLIENT_SCKT_STATUS_SHUTDOWN_SEND) ||
        (sckt->client_status == CLIENT_SCKT_STATUS_CLOSED)) {
        spice_printerr("unexpected SPICE_MSGC_TUNNEL_SOCKET_DATA client_status=%d",
                   sckt->client_status);
        return FALSE;
    }

    // handling a case where the client sent data before it received the close msg
    if ((sckt->slirp_status != SLIRP_SCKT_STATUS_OPEN) &&
            (sckt->slirp_status != SLIRP_SCKT_STATUS_SHUTDOWN_SEND)) {
        __tunnel_worker_free_socket_rcv_buf(sckt->worker, recv_data);
        return (!CHECK_TUNNEL_ERROR(channel));
    } else if ((sckt->in_data.num_buffers == MAX_SOCKET_IN_BUFFERS) &&
               !channel->mig_inprogress) {
        spice_printerr("socket in buffers overflow, socket will be closed"
                   " (local_port=%d, service_id=%d)",
                   ntohs(sckt->local_port), sckt->far_service->id);
        __tunnel_worker_free_socket_rcv_buf(sckt->worker, recv_data);
        tunnel_socket_force_close(channel, sckt);
        return (!CHECK_TUNNEL_ERROR(channel));
    }

    tunnel_socket_assign_rcv_buf(sckt, recv_data, buf_size);
    if (!sckt->in_data.client_total_num_tokens) {
        spice_printerr("token violation");
        return FALSE;
    }

    --sckt->in_data.client_total_num_tokens;
    __process_rcv_buf_tokens(channel, sckt);

    if (sckt->in_data.ready_chunks_queue.head && !channel->mig_inprogress) {
        net_slirp_socket_can_receive_notify(sckt->slirp_sckt);
    }

    return (!CHECK_TUNNEL_ERROR(channel));
}

static inline int __client_socket_can_receive(RedSocket *sckt)
{
    return (((sckt->client_status == CLIENT_SCKT_STATUS_OPEN) ||
             (sckt->client_status == CLIENT_SCKT_STATUS_SHUTDOWN_SEND)) &&
            !sckt->worker->channel_client->mig_inprogress);
}

static int tunnel_channel_handle_socket_token(TunnelChannelClient *channel, RedSocket *sckt,
                                              SpiceMsgcTunnelSocketTokens *message)
{
    sckt->out_data.num_tokens += message->num_tokens;

    if (__client_socket_can_receive(sckt) && sckt->out_data.ready_chunks_queue.head &&
        !red_channel_client_pipe_item_is_linked(&channel->base, &sckt->out_data.data_pipe_item)) {
        // data is pending to be sent
        sckt->out_data.data_pipe_item.type = PIPE_ITEM_TYPE_SOCKET_DATA;
        red_channel_client_pipe_add(&channel->base, &sckt->out_data.data_pipe_item);
    }

    return TRUE;
}

static uint8_t *tunnel_channel_alloc_msg_rcv_buf(RedChannelClient *rcc,
                                                 uint16_t type, uint32_t size)
{
    TunnelChannelClient *tunnel_channel = (TunnelChannelClient *)rcc->channel;

    if (type == SPICE_MSGC_TUNNEL_SOCKET_DATA) {
        return (__tunnel_worker_alloc_socket_rcv_buf(tunnel_channel->worker)->buf);
    } else if ((type == SPICE_MSGC_MIGRATE_DATA) ||
               (type == SPICE_MSGC_TUNNEL_SERVICE_ADD)) {
        return spice_malloc(size);
    } else {
        return (tunnel_channel->control_rcv_buf);
    }
}

// called by the receive routine of the channel, before the buffer was assigned to a socket
static void tunnel_channel_release_msg_rcv_buf(RedChannelClient *rcc, uint16_t type, uint32_t size,
                                               uint8_t *msg)
{
    TunnelChannelClient *tunnel_channel = (TunnelChannelClient *)rcc->channel;

    if (type == SPICE_MSGC_TUNNEL_SOCKET_DATA) {
        spice_assert(!(SPICE_CONTAINEROF(msg, RedSocketRawRcvBuf, buf)->base.usr_opaque));
        __tunnel_worker_free_socket_rcv_buf(tunnel_channel->worker,
                                            SPICE_CONTAINEROF(msg, RedSocketRawRcvBuf, buf));
    }
}

static void __tunnel_channel_fill_service_migrate_item(TunnelChannelClient *channel,
                                                       TunnelService *service,
                                                       TunnelMigrateServiceItem *migrate_item)
{
    migrate_item->service = service;
    TunnelMigrateService *general_data;
    if (service->type == SPICE_TUNNEL_SERVICE_TYPE_GENERIC) {
        general_data = &migrate_item->u.generic_service;
    } else if (service->type == SPICE_TUNNEL_SERVICE_TYPE_IPP) {
        general_data = &migrate_item->u.print_service.base;
        memcpy(migrate_item->u.print_service.ip, ((TunnelPrintService *)service)->ip, 4);
    } else {
        spice_error("unexpected service type");
        abort();
    }

    general_data->type = service->type;
    general_data->id = service->id;
    general_data->group = service->group;
    general_data->port = service->port;
    memcpy(general_data->virt_ip, &service->virt_ip.s_addr, 4);
}

static void __tunnel_channel_fill_socket_migrate_item(TunnelChannelClient *channel, RedSocket *sckt,
                                                      TunnelMigrateSocketItem *migrate_item)
{
    TunnelMigrateSocket *mig_sckt = &migrate_item->mig_socket;
    migrate_item->socket = sckt;
    mig_sckt->connection_id = sckt->connection_id;
    mig_sckt->local_port = sckt->local_port;
    mig_sckt->far_service_id = sckt->far_service->id;
    mig_sckt->client_status = sckt->client_status;
    mig_sckt->slirp_status = sckt->slirp_status;

    mig_sckt->pushed_close = sckt->pushed_close;
    mig_sckt->client_waits_close_ack = sckt->client_waits_close_ack;

    mig_sckt->mig_client_status_msg = sckt->mig_client_status_msg;
    mig_sckt->mig_open_ack_tokens = sckt->mig_open_ack_tokens;

    mig_sckt->out_data.num_tokens = sckt->out_data.num_tokens;
    mig_sckt->out_data.window_size = sckt->out_data.window_size;

    // checking if there is a need to save the queues
    if ((sckt->client_status != CLIENT_SCKT_STATUS_CLOSED) &&
        (sckt->mig_client_status_msg != SPICE_MSGC_TUNNEL_SOCKET_CLOSED) &&
        (sckt->mig_client_status_msg != SPICE_MSGC_TUNNEL_SOCKET_CLOSED_ACK)) {
        mig_sckt->out_data.process_queue_size =
            sckt->out_data.process_queue->get_migrate_data_proc(sckt->out_data.process_queue,
                                                                &migrate_item->out_process_queue);
    }

    mig_sckt->in_data.num_tokens = sckt->in_data.num_tokens;
    mig_sckt->in_data.client_total_num_tokens = sckt->in_data.client_total_num_tokens;

    if ((sckt->slirp_status == SLIRP_SCKT_STATUS_OPEN) ||
        (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_SEND)) {
        mig_sckt->in_data.process_queue_size =
            sckt->in_data.process_queue->get_migrate_data_proc(sckt->in_data.process_queue,
                                                               &migrate_item->in_process_queue);
    }

    if (sckt->slirp_status != SLIRP_SCKT_STATUS_CLOSED) {
        migrate_item->slirp_socket_size = net_slirp_tcp_socket_export(sckt->slirp_sckt,
                                                                      &migrate_item->slirp_socket);
        if (!migrate_item->slirp_socket) {
            SET_TUNNEL_ERROR(channel, "failed export slirp socket");
        }
    } else {
        migrate_item->slirp_socket_size = 0;
        migrate_item->slirp_socket = NULL;
    }
}

static void release_migrate_item(TunnelMigrateItem *item);
static int tunnel_channel_handle_migrate_mark(RedChannelClient *base)
{
    TunnelChannelClient *channel = SPICE_CONTAINEROF(base->channel, TunnelChannelClient, base);
    TunnelMigrateItem *migrate_item = NULL;
    TunnelService *service;
    TunnelMigrateServiceItem *mig_service;
    int num_sockets_saved = 0;
    RedSocket *sckt;

    migrate_item = spice_new0(TunnelMigrateItem, 1);
    migrate_item->base.type = PIPE_ITEM_TYPE_MIGRATE_DATA;

    migrate_item->slirp_state_size = net_slirp_state_export(&migrate_item->slirp_state);
    if (!migrate_item->slirp_state) {
        spice_printerr("failed export slirp state");
        goto error;
    }

    migrate_item->services_list_size = sizeof(TunnelMigrateServicesList) +
        (sizeof(uint32_t)*channel->worker->num_services);
    migrate_item->services_list =
        (TunnelMigrateServicesList *)spice_malloc(migrate_item->services_list_size);
    migrate_item->services_list->num_services = channel->worker->num_services;

    migrate_item->services = (TunnelMigrateServiceItem *)spice_malloc(
            channel->worker->num_services * sizeof(TunnelMigrateServiceItem));

    for (mig_service = migrate_item->services,
         service = (TunnelService *)ring_get_head(&channel->worker->services);
         service;
         mig_service++,
         service = (TunnelService *)ring_next(&channel->worker->services, &service->ring_item)) {
        __tunnel_channel_fill_service_migrate_item(channel, service, mig_service);
        if (CHECK_TUNNEL_ERROR(channel)) {
            goto error;
        }
    }

    migrate_item->sockets_list_size = sizeof(TunnelMigrateSocketList) +
        (sizeof(uint32_t)*channel->worker->num_sockets);
    migrate_item->sockets_list =
        (TunnelMigrateSocketList *) spice_malloc(migrate_item->sockets_list_size);

    migrate_item->sockets_list->num_sockets = channel->worker->num_sockets;

    for (sckt = channel->worker->sockets; num_sockets_saved < channel->worker->num_sockets;
                                                                                       sckt++) {
        if (sckt->allocated) {
            __tunnel_channel_fill_socket_migrate_item(channel, sckt,
                                                      &migrate_item->sockets_data[
                                                        num_sockets_saved++]);
            if (CHECK_TUNNEL_ERROR(channel)) {
                goto error;
            }
        }
    }

    red_channel_client_pipe_add(&channel->base, &migrate_item->base);

    return TRUE;
error:
    release_migrate_item(migrate_item);
    return FALSE;
}

static void release_migrate_item(TunnelMigrateItem *item)
{
    if (!item) {
        return;
    }

    int i;
    if (item->sockets_list) {
        int num_sockets = item->sockets_list->num_sockets;
        for (i = 0; i < num_sockets; i++) {
            if (item->sockets_data[i].socket) { // handling errors in the middle of
                                                // __tunnel_channel_fill_socket_migrate_item
                if (item->sockets_data[i].out_process_queue) {
                    item->sockets_data[i].socket->out_data.process_queue->release_migrate_data_proc(
                        item->sockets_data[i].socket->out_data.process_queue,
                        item->sockets_data[i].out_process_queue);
                }
                if (item->sockets_data[i].in_process_queue) {
                    item->sockets_data[i].socket->in_data.process_queue->release_migrate_data_proc(
                        item->sockets_data[i].socket->in_data.process_queue,
                        item->sockets_data[i].in_process_queue);
                }
            }

            free(item->sockets_data[i].slirp_socket);
        }
        free(item->sockets_list);
    }

    free(item->services);
    free(item->services_list);
    free(item->slirp_state);
    free(item);
}

typedef RawTunneledBuffer *(*socket_alloc_buffer_proc_t)(RedSocket *sckt);

typedef struct RedSocketRestoreTokensBuf {
    RedSocketRawRcvBuf base;
    int num_tokens;
} RedSocketRestoreTokensBuf;

// not updating tokens
static void restored_rcv_buf_release(RawTunneledBuffer *buf)
{
    RedSocket *sckt = (RedSocket *)buf->usr_opaque;
    --sckt->in_data.num_buffers;
    __tunnel_worker_free_socket_rcv_buf(sckt->worker, (RedSocketRawRcvBuf *)buf);
    // for case that ready queue is empty and the client has no tokens
    __process_rcv_buf_tokens(sckt->worker->channel_client, sckt);
}

RawTunneledBuffer *tunnel_socket_alloc_restored_rcv_buf(RedSocket *sckt)
{
    RedSocketRawRcvBuf *buf = __tunnel_worker_alloc_socket_rcv_buf(sckt->worker);
    buf->base.usr_opaque = sckt;
    buf->base.release_proc = restored_rcv_buf_release;

    sckt->in_data.num_buffers++;
    return &buf->base;
}

static void restore_tokens_buf_release(RawTunneledBuffer *buf)
{
    RedSocketRestoreTokensBuf *tokens_buf = (RedSocketRestoreTokensBuf *)buf;
    RedSocket *sckt = (RedSocket *)buf->usr_opaque;

    sckt->in_data.num_tokens += tokens_buf->num_tokens;
    __process_rcv_buf_tokens(sckt->worker->channel_client, sckt);

    free(tokens_buf);
}

RawTunneledBuffer *__tunnel_socket_alloc_restore_tokens_buf(RedSocket *sckt, int num_tokens)
{
    RedSocketRestoreTokensBuf *buf = spice_new0(RedSocketRestoreTokensBuf, 1);

    buf->base.base.usr_opaque = sckt;
    buf->base.base.refs = 1;
    buf->base.base.release_proc = restore_tokens_buf_release;
    buf->num_tokens = num_tokens;
#ifdef DEBUG_NETWORK
    spice_printerr("TUNNEL DBG: num_tokens=%d", num_tokens);
#endif
    return &buf->base.base;
}

static void __restore_ready_chunks_queue(RedSocket *sckt, ReadyTunneledChunkQueue *queue,
                                         uint8_t *data, int size,
                                         socket_alloc_buffer_proc_t alloc_buf)
{
    int copied = 0;

    while (copied < size) {
        RawTunneledBuffer *buf = alloc_buf(sckt);
        int copy_count = MIN(size - copied, buf->max_size);
        memcpy(buf->data, data + copied, copy_count);
        copied += copy_count;
        buf->size = copy_count;
        ready_queue_add_orig_chunk(queue, buf, buf->data, buf->size);
        tunneled_buffer_unref(buf);
    }
}

// not using the alloc_buf cb of the queue, since we may want to create the migrated buffers
// with other properties (e.g., not releasing token)
static void __restore_process_queue(RedSocket *sckt, TunneledBufferProcessQueue *queue,
                                    uint8_t *data, int size,
                                    socket_alloc_buffer_proc_t alloc_buf)
{
    int copied = 0;

    while (copied < size) {
        RawTunneledBuffer *buf = alloc_buf(sckt);
        int copy_count = MIN(size - copied, buf->max_size);
        memcpy(buf->data, data + copied, copy_count);
        copied += copy_count;
        buf->size = copy_count;
        __process_queue_push(queue, buf);
    }
}

static void tunnel_channel_restore_migrated_service(TunnelChannelClient *channel,
                                                    TunnelMigrateService *mig_service,
                                                    uint8_t *data_buf)
{
    int service_size;
    TunnelService *service;
    struct in_addr virt_ip;
    if (mig_service->type == SPICE_TUNNEL_SERVICE_TYPE_GENERIC) {
        service_size = sizeof(TunnelService);
    } else if (mig_service->type == SPICE_TUNNEL_SERVICE_TYPE_IPP) {
        service_size = sizeof(TunnelPrintService);
    } else {
        SET_TUNNEL_ERROR(channel, "unexpected service type");
        return;
    }

    memcpy(&virt_ip.s_addr, mig_service->virt_ip, 4);
    service = __tunnel_worker_add_service(channel->worker, service_size,
                                          mig_service->type, mig_service->id,
                                          mig_service->group, mig_service->port,
                                          (char *)(data_buf + mig_service->name),
                                          (char *)(data_buf + mig_service->description), &virt_ip);
    if (!service) {
        SET_TUNNEL_ERROR(channel, "failed creating service");
        return;
    }

    if (service->type == SPICE_TUNNEL_SERVICE_TYPE_IPP) {
        TunnelMigratePrintService *mig_print_service = (TunnelMigratePrintService *)mig_service;
        TunnelPrintService *print_service = (TunnelPrintService *)service;

        memcpy(print_service->ip, mig_print_service->ip, 4);
    }
}

static void tunnel_channel_restore_migrated_socket(TunnelChannelClient *channel,
                                                   TunnelMigrateSocket *mig_socket,
                                                   uint8_t *data_buf)
{
    RedSocket *sckt;
    SlirpSocket *slirp_sckt;
    RawTunneledBuffer *tokens_buf;
    TunnelService *service;
    sckt = channel->worker->sockets + mig_socket->connection_id;
    sckt->connection_id = mig_socket->connection_id;
    spice_assert(!sckt->allocated);

    /* Services must be restored before sockets */
    service = tunnel_worker_find_service_by_id(channel->worker, mig_socket->far_service_id);
    if (!service) {
        SET_TUNNEL_ERROR(channel, "service not found");
        return;
    }

    tunnel_worker_alloc_socket(channel->worker, sckt, mig_socket->local_port, service, NULL);

    sckt->client_status = mig_socket->client_status;
    sckt->slirp_status = mig_socket->slirp_status;

    sckt->mig_client_status_msg = mig_socket->mig_client_status_msg;
    sckt->mig_open_ack_tokens = mig_socket->mig_open_ack_tokens;

    sckt->pushed_close = mig_socket->pushed_close;
    sckt->client_waits_close_ack = mig_socket->client_waits_close_ack;

    if (sckt->slirp_status != SLIRP_SCKT_STATUS_CLOSED) {
        slirp_sckt = net_slirp_tcp_socket_restore(data_buf + mig_socket->slirp_sckt, sckt);
        if (!slirp_sckt) {
            SET_TUNNEL_ERROR(channel, "failed restoring slirp socket");
            return;
        }
        sckt->slirp_sckt = slirp_sckt;
    }
    // out data
    sckt->out_data.num_tokens = mig_socket->out_data.num_tokens;
    sckt->out_data.window_size = mig_socket->out_data.window_size;
    sckt->out_data.data_size = mig_socket->out_data.process_buf_size +
        mig_socket->out_data.ready_buf_size;

    __restore_ready_chunks_queue(sckt, &sckt->out_data.ready_chunks_queue,
                                 data_buf + mig_socket->out_data.ready_buf,
                                 mig_socket->out_data.ready_buf_size,
                                 tunnel_socket_alloc_snd_buf);

    sckt->out_data.process_queue->restore_proc(sckt->out_data.process_queue,
                                               data_buf + mig_socket->out_data.process_queue);

    __restore_process_queue(sckt, sckt->out_data.process_queue,
                            data_buf + mig_socket->out_data.process_buf,
                            mig_socket->out_data.process_buf_size,
                            tunnel_socket_alloc_snd_buf);

    sckt->in_data.client_total_num_tokens = mig_socket->in_data.client_total_num_tokens;
    sckt->in_data.num_tokens = mig_socket->in_data.num_tokens;

    __restore_ready_chunks_queue(sckt, &sckt->in_data.ready_chunks_queue,
                                 data_buf + mig_socket->in_data.ready_buf,
                                 mig_socket->in_data.ready_buf_size,
                                 tunnel_socket_alloc_restored_rcv_buf);

    sckt->in_data.process_queue->restore_proc(sckt->in_data.process_queue,
                                              data_buf + mig_socket->in_data.process_queue);

    __restore_process_queue(sckt, sckt->in_data.process_queue,
                            data_buf + mig_socket->in_data.process_buf,
                            mig_socket->in_data.process_buf_size,
                            tunnel_socket_alloc_restored_rcv_buf);

    tokens_buf = __tunnel_socket_alloc_restore_tokens_buf(sckt,
                                                          SOCKET_WINDOW_SIZE -
                                                          (sckt->in_data.client_total_num_tokens +
                                                           sckt->in_data.num_tokens));
    if (sckt->in_data.process_queue->head) {
        __process_queue_push(sckt->in_data.process_queue, tokens_buf);
    } else {
        ready_queue_add_orig_chunk(&sckt->in_data.ready_chunks_queue, tokens_buf,
                                   tokens_buf->data, tokens_buf->size);
        tunneled_buffer_unref(tokens_buf);
    }
}

static void tunnel_channel_restore_socket_state(TunnelChannelClient *channel, RedSocket *sckt)
{
    int ret = TRUE;
    spice_printerr("");
    // handling client status msgs that were received during migration
    switch (sckt->mig_client_status_msg) {
    case 0:
        break;
    case SPICE_MSGC_TUNNEL_SOCKET_OPEN_ACK:
        ret = tunnel_channel_handle_socket_connect_ack(channel, sckt,
                                                       sckt->mig_open_ack_tokens);
        break;
    case SPICE_MSGC_TUNNEL_SOCKET_OPEN_NACK:
        ret = tunnel_channel_handle_socket_connect_nack(channel, sckt);
        break;
    case SPICE_MSGC_TUNNEL_SOCKET_FIN:
        if (sckt->client_status == CLIENT_SCKT_STATUS_WAIT_OPEN) {
            ret = tunnel_channel_handle_socket_connect_ack(channel, sckt,
                                                           sckt->mig_open_ack_tokens);
        }
        if (ret) {
            ret = tunnel_channel_handle_socket_fin(channel, sckt);
        }
        break;
    case SPICE_MSGC_TUNNEL_SOCKET_CLOSED:
        // can't just send nack since we need to send close ack to client
        if (sckt->client_status == CLIENT_SCKT_STATUS_WAIT_OPEN) {
            ret = tunnel_channel_handle_socket_connect_ack(channel, sckt,
                                                           sckt->mig_open_ack_tokens);
        }
        ret = ret & tunnel_channel_handle_socket_closed(channel, sckt);

        break;
    case SPICE_MSGC_TUNNEL_SOCKET_CLOSED_ACK:
        ret = tunnel_channel_handle_socket_closed_ack(channel, sckt);
        break;
    default:
        SET_TUNNEL_ERROR(channel, "invalid message type %u", sckt->mig_client_status_msg);
        return;
    }

    if (!ret) {
        SET_TUNNEL_ERROR(channel, "failed restoring socket state");
        return;
    }
    sckt->mig_client_status_msg = 0;
    sckt->mig_open_ack_tokens = 0;

    // handling data transfer
    if (__client_socket_can_receive(sckt) && sckt->out_data.ready_chunks_queue.head) {
        if (!red_channel_client_pipe_item_is_linked(
                &channel->base, &sckt->out_data.data_pipe_item)) {
            sckt->out_data.data_pipe_item.type = PIPE_ITEM_TYPE_SOCKET_DATA;
            red_channel_client_pipe_add(&channel->base, &sckt->out_data.data_pipe_item);
        }
    }

    if (((sckt->slirp_status == SLIRP_SCKT_STATUS_OPEN) ||
         (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_SEND)) &&
        sckt->in_data.ready_chunks_queue.head) {
        net_slirp_socket_can_receive_notify(sckt->slirp_sckt);
    }

    if (CHECK_TUNNEL_ERROR(channel)) {
        return;
    }

    if ((sckt->slirp_status == SLIRP_SCKT_STATUS_OPEN) ||
        (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_RECV)) {
        net_slirp_socket_can_send_notify(sckt->slirp_sckt);
    }

    if (CHECK_TUNNEL_ERROR(channel)) {
        return;
    }
    // for cases where the client has no tokens left, but all the data is in the process queue.
    __process_rcv_buf_tokens(channel, sckt);
}

static inline void tunnel_channel_activate_migrated_sockets(TunnelChannelClient *channel)
{
    // if we are overgoing migration again, no need to restore the state, we will wait
    // for the next host.
    if (!channel->mig_inprogress) {
        int num_activated = 0;
        RedSocket *sckt = channel->worker->sockets;

        for (; num_activated < channel->worker->num_sockets; sckt++) {
            if (sckt->allocated) {
                tunnel_channel_restore_socket_state(channel, sckt);

                if (CHECK_TUNNEL_ERROR(channel)) {
                    return;
                }

                num_activated++;
            }
        }
        net_slirp_unfreeze();
    }
}

static uint64_t tunnel_channel_handle_migrate_data_get_serial(RedChannelClient *base,
                                              uint32_t size, void *msg)
{
    TunnelMigrateData *migrate_data = msg;

    if (size < sizeof(TunnelMigrateData)
        || migrate_data->magic != TUNNEL_MIGRATE_DATA_MAGIC
        || migrate_data->version != TUNNEL_MIGRATE_DATA_VERSION) {
        return 0;
    }
    return migrate_data->message_serial;
}

static int tunnel_channel_handle_migrate_data(RedChannelClient *base,
                                              uint32_t size, void *msg)
{
    TunnelChannelClient *channel = SPICE_CONTAINEROF(base, TunnelChannelClient, base);
    TunnelMigrateSocketList *sockets_list;
    TunnelMigrateServicesList *services_list;
    TunnelMigrateData *migrate_data = msg;
    int i;

    if (size < sizeof(TunnelMigrateData)) {
        spice_printerr("bad message size");
        goto error;
    }

    if (migrate_data->magic != TUNNEL_MIGRATE_DATA_MAGIC ||
        migrate_data->version != TUNNEL_MIGRATE_DATA_VERSION) {
        spice_printerr("invalid content");
        goto error;
    }

    net_slirp_state_restore(migrate_data->data + migrate_data->slirp_state);

    services_list = (TunnelMigrateServicesList *)(migrate_data->data +
                                                  migrate_data->services_list);
    for (i = 0; i < services_list->num_services; i++) {
        tunnel_channel_restore_migrated_service(channel,
                                                (TunnelMigrateService *)(migrate_data->data +
                                                                        services_list->services[i]),
                                                migrate_data->data);
        if (CHECK_TUNNEL_ERROR(channel)) {
            spice_printerr("failed restoring service");
            goto error;
        }
    }

    sockets_list = (TunnelMigrateSocketList *)(migrate_data->data + migrate_data->sockets_list);

    for (i = 0; i < sockets_list->num_sockets; i++) {
        tunnel_channel_restore_migrated_socket(channel,
                                               (TunnelMigrateSocket *)(migrate_data->data +
                                                                       sockets_list->sockets[i]),
                                               migrate_data->data);
        if (CHECK_TUNNEL_ERROR(channel)) {
            spice_printerr("failed restoring socket");
            goto error;
        }
    }

    // activate channel
    channel->mig_inprogress = FALSE;
    red_channel_init_outgoing_messages_window(channel->base.channel);

    tunnel_channel_activate_migrated_sockets(channel);

    if (CHECK_TUNNEL_ERROR(channel)) {
        goto error;
    }
    free(migrate_data);
    return TRUE;
error:
    free(migrate_data);
    return FALSE;
}

//  msg was allocated by tunnel_channel_alloc_msg_rcv_buf
static int tunnel_channel_handle_message(RedChannelClient *rcc, uint16_t type,
                                         uint32_t size, uint8_t *msg)
{
    TunnelChannelClient *tunnel_channel = (TunnelChannelClient *)rcc->channel;
    RedSocket *sckt = NULL;
    // retrieve the sckt
    switch (type) {
    case SPICE_MSGC_MIGRATE_FLUSH_MARK:
    case SPICE_MSGC_MIGRATE_DATA:
    case SPICE_MSGC_TUNNEL_SERVICE_ADD:
    case SPICE_MSGC_TUNNEL_SERVICE_REMOVE:
        break;
    case SPICE_MSGC_TUNNEL_SOCKET_OPEN_ACK:
    case SPICE_MSGC_TUNNEL_SOCKET_OPEN_NACK:
    case SPICE_MSGC_TUNNEL_SOCKET_DATA:
    case SPICE_MSGC_TUNNEL_SOCKET_FIN:
    case SPICE_MSGC_TUNNEL_SOCKET_CLOSED:
    case SPICE_MSGC_TUNNEL_SOCKET_CLOSED_ACK:
    case SPICE_MSGC_TUNNEL_SOCKET_TOKEN:
        // the first field in these messages is connection id
        sckt = tunnel_channel->worker->sockets + (*((uint16_t *)msg));
        if (!sckt->allocated) {
            spice_printerr("red socket not found");
            return FALSE;
        }
        break;
    default:
        return red_channel_client_handle_message(rcc, size, type, msg);
    }

    switch (type) {
    case SPICE_MSGC_TUNNEL_SERVICE_ADD:
        if (size < sizeof(SpiceMsgcTunnelAddGenericService)) {
            spice_printerr("bad message size");
            free(msg);
            return FALSE;
        }
        return tunnel_channel_handle_service_add(tunnel_channel,
                                                 (SpiceMsgcTunnelAddGenericService *)msg);
    case SPICE_MSGC_TUNNEL_SERVICE_REMOVE:
        spice_printerr("REDC_TUNNEL_REMOVE_SERVICE not supported yet");
        return FALSE;
    case SPICE_MSGC_TUNNEL_SOCKET_OPEN_ACK:
        if (size != sizeof(SpiceMsgcTunnelSocketOpenAck)) {
            spice_printerr("bad message size");
            return FALSE;
        }

        return tunnel_channel_handle_socket_connect_ack(tunnel_channel, sckt,
                                                        ((SpiceMsgcTunnelSocketOpenAck *)msg)->tokens);

    case SPICE_MSGC_TUNNEL_SOCKET_OPEN_NACK:
        if (size != sizeof(SpiceMsgcTunnelSocketOpenNack)) {
            spice_printerr("bad message size");
            return FALSE;
        }

        return tunnel_channel_handle_socket_connect_nack(tunnel_channel, sckt);
    case SPICE_MSGC_TUNNEL_SOCKET_DATA:
    {
        if (size < sizeof(SpiceMsgcTunnelSocketData)) {
            spice_printerr("bad message size");
            return FALSE;
        }

        return tunnel_channel_handle_socket_receive_data(tunnel_channel, sckt,
                                                    SPICE_CONTAINEROF(msg, RedSocketRawRcvBuf, buf),
                                                    size - sizeof(SpiceMsgcTunnelSocketData));
    }
    case SPICE_MSGC_TUNNEL_SOCKET_FIN:
        if (size != sizeof(SpiceMsgcTunnelSocketFin)) {
            spice_printerr("bad message size");
            return FALSE;
        }
        return tunnel_channel_handle_socket_fin(tunnel_channel, sckt);
    case SPICE_MSGC_TUNNEL_SOCKET_CLOSED:
        if (size != sizeof(SpiceMsgcTunnelSocketClosed)) {
            spice_printerr("bad message size");
            return FALSE;
        }
        return tunnel_channel_handle_socket_closed(tunnel_channel, sckt);
    case SPICE_MSGC_TUNNEL_SOCKET_CLOSED_ACK:
        if (size != sizeof(SpiceMsgcTunnelSocketClosedAck)) {
            spice_printerr("bad message size");
            return FALSE;
        }
        return tunnel_channel_handle_socket_closed_ack(tunnel_channel, sckt);
    case SPICE_MSGC_TUNNEL_SOCKET_TOKEN:
        if (size != sizeof(SpiceMsgcTunnelSocketTokens)) {
            spice_printerr("bad message size");
            return FALSE;
        }

        return tunnel_channel_handle_socket_token(tunnel_channel, sckt,
                                                  (SpiceMsgcTunnelSocketTokens *)msg);
    default:
        return red_channel_client_handle_message(rcc, size, type, msg);
    }
    return TRUE;
}

/********************************/
/* outgoing msgs
********************************/

static int __tunnel_channel_marshall_process_bufs_migrate_data(TunnelChannelClient *channel,
                                    SpiceMarshaller *m, TunneledBufferProcessQueue *queue)
{
    int buf_offset = queue->head_offset;
    RawTunneledBuffer *buf = queue->head;
    int size = 0;

    while (buf) {
        spice_marshaller_add_ref(m, (uint8_t*)buf->data + buf_offset, buf->size - buf_offset);
        size += buf->size - buf_offset;
        buf_offset = 0;
        buf = buf->next;
    }

    return size;
}

static int __tunnel_channel_marshall_ready_bufs_migrate_data(TunnelChannelClient *channel,
                                    SpiceMarshaller *m, ReadyTunneledChunkQueue *queue)
{
    int offset = queue->offset;
    ReadyTunneledChunk *chunk = queue->head;
    int size = 0;

    while (chunk) {
        spice_marshaller_add_ref(m, (uint8_t*)chunk->data + offset, chunk->size - offset);
        size += chunk->size - offset;
        offset = 0;
        chunk = chunk->next;
    }
    return size;
}

// returns the size to send
static int __tunnel_channel_marshall_service_migrate_data(TunnelChannelClient *channel,
                                                      SpiceMarshaller *m,
                                                      TunnelMigrateServiceItem *item,
                                                      int offset)
{
    TunnelService *service = item->service;
    int cur_offset = offset;
    TunnelMigrateService *generic_data;

    if (service->type == SPICE_TUNNEL_SERVICE_TYPE_GENERIC) {
        generic_data = &item->u.generic_service;
        spice_marshaller_add_ref(m, (uint8_t*)&item->u.generic_service,
                            sizeof(item->u.generic_service));
        cur_offset += sizeof(item->u.generic_service);
    } else if (service->type == SPICE_TUNNEL_SERVICE_TYPE_IPP) {
        generic_data = &item->u.print_service.base;
        spice_marshaller_add_ref(m, (uint8_t*)&item->u.print_service,
                            sizeof(item->u.print_service));
        cur_offset += sizeof(item->u.print_service);
    } else {
        spice_error("unexpected service type");
        abort();
    }

    generic_data->name = cur_offset;
    spice_marshaller_add_ref(m, (uint8_t*)service->name, strlen(service->name) + 1);
    cur_offset += strlen(service->name) + 1;

    generic_data->description = cur_offset;
    spice_marshaller_add_ref(m, (uint8_t*)service->description, strlen(service->description) + 1);
    cur_offset += strlen(service->description) + 1;

    return (cur_offset - offset);
}

// returns the size to send
static int __tunnel_channel_marshall_socket_migrate_data(TunnelChannelClient *channel,
                                SpiceMarshaller *m, TunnelMigrateSocketItem *item, int offset)
{
    RedSocket *sckt = item->socket;
    TunnelMigrateSocket *mig_sckt = &item->mig_socket;
    int cur_offset = offset;
    spice_marshaller_add_ref(m, (uint8_t*)mig_sckt, sizeof(*mig_sckt));
    cur_offset += sizeof(*mig_sckt);

    if ((sckt->client_status != CLIENT_SCKT_STATUS_CLOSED) &&
        (sckt->mig_client_status_msg != SPICE_MSGC_TUNNEL_SOCKET_CLOSED) &&
        (sckt->mig_client_status_msg != SPICE_MSGC_TUNNEL_SOCKET_CLOSED_ACK)) {
        mig_sckt->out_data.process_buf = cur_offset;
        mig_sckt->out_data.process_buf_size =
            __tunnel_channel_marshall_process_bufs_migrate_data(channel, m,
                                                            sckt->out_data.process_queue);
        cur_offset += mig_sckt->out_data.process_buf_size;
        if (mig_sckt->out_data.process_queue_size) {
            mig_sckt->out_data.process_queue = cur_offset;
            spice_marshaller_add_ref(m, (uint8_t*)item->out_process_queue,
                                mig_sckt->out_data.process_queue_size);
            cur_offset += mig_sckt->out_data.process_queue_size;
        }
        mig_sckt->out_data.ready_buf = cur_offset;
        mig_sckt->out_data.ready_buf_size =
            __tunnel_channel_marshall_ready_bufs_migrate_data(channel, m,
                                                          &sckt->out_data.ready_chunks_queue);
        cur_offset += mig_sckt->out_data.ready_buf_size;
    } else {
        mig_sckt->out_data.process_buf_size = 0;
        mig_sckt->out_data.ready_buf_size = 0;
    }

    // notice that we migrate the received buffers without the msg headers.
    if ((sckt->slirp_status == SLIRP_SCKT_STATUS_OPEN) ||
        (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_SEND)) {
        mig_sckt->in_data.process_buf = cur_offset;
        mig_sckt->in_data.process_buf_size =
            __tunnel_channel_marshall_process_bufs_migrate_data(channel, m,
                                                            sckt->in_data.process_queue);
        cur_offset += mig_sckt->in_data.process_buf_size;
        if (mig_sckt->in_data.process_queue_size) {
            mig_sckt->in_data.process_queue = cur_offset;
            spice_marshaller_add_ref(m, (uint8_t*)item->in_process_queue,
                                mig_sckt->in_data.process_queue_size);
            cur_offset += mig_sckt->in_data.process_queue_size;
        }
        mig_sckt->in_data.ready_buf = cur_offset;
        mig_sckt->in_data.ready_buf_size =
            __tunnel_channel_marshall_ready_bufs_migrate_data(channel, m,
                                                          &sckt->in_data.ready_chunks_queue);
        cur_offset += mig_sckt->in_data.ready_buf_size;
    } else {
        mig_sckt->in_data.process_buf_size = 0;
        mig_sckt->in_data.ready_buf_size = 0;
    }

    if (item->slirp_socket_size) { // zero if socket is closed
        spice_marshaller_add_ref(m, (uint8_t*)item->slirp_socket, item->slirp_socket_size);
        mig_sckt->slirp_sckt = cur_offset;
        cur_offset += item->slirp_socket_size;
    }
    return (cur_offset - offset);
}

static void tunnel_channel_marshall_migrate_data(RedChannelClient *rcc,
                                        SpiceMarshaller *m, PipeItem *item)
{
    TunnelChannelClient *tunnel_channel;
    TunnelMigrateData *migrate_data;
    TunnelMigrateItem *migrate_item = (TunnelMigrateItem *)item;
    int i;

    uint32_t data_buf_offset = 0; // current location in data[0] field
    spice_assert(rcc);
    tunnel_channel = SPICE_CONTAINEROF(rcc->channel, TunnelChannelClient, base);
    migrate_data = &tunnel_channel->send_data.u.migrate_data;

    migrate_data->magic = TUNNEL_MIGRATE_DATA_MAGIC;
    migrate_data->version = TUNNEL_MIGRATE_DATA_VERSION;
    migrate_data->message_serial = red_channel_client_get_message_serial(rcc);
    red_channel_client_init_send_data(rcc, SPICE_MSG_MIGRATE_DATA, item);
    spice_marshaller_add_ref(m, (uint8_t*)migrate_data, sizeof(*migrate_data));

    migrate_data->slirp_state = data_buf_offset;
    spice_marshaller_add_ref(m, (uint8_t*)migrate_item->slirp_state, migrate_item->slirp_state_size);
    data_buf_offset += migrate_item->slirp_state_size;

    migrate_data->services_list = data_buf_offset;
    spice_marshaller_add_ref(m, (uint8_t*)migrate_item->services_list,
                        migrate_item->services_list_size);
    data_buf_offset += migrate_item->services_list_size;

    for (i = 0; i < migrate_item->services_list->num_services; i++) {
        migrate_item->services_list->services[i] = data_buf_offset;
        data_buf_offset += __tunnel_channel_marshall_service_migrate_data(tunnel_channel, m,
                                                                      migrate_item->services + i,
                                                                      data_buf_offset);
    }


    migrate_data->sockets_list = data_buf_offset;
    spice_marshaller_add_ref(m, (uint8_t*)migrate_item->sockets_list,
                        migrate_item->sockets_list_size);
    data_buf_offset += migrate_item->sockets_list_size;

    for (i = 0; i < migrate_item->sockets_list->num_sockets; i++) {
        migrate_item->sockets_list->sockets[i] = data_buf_offset;
        data_buf_offset += __tunnel_channel_marshall_socket_migrate_data(tunnel_channel, m,
                                                                     migrate_item->sockets_data + i,
                                                                     data_buf_offset);
    }
}

static void tunnel_channel_marshall_init(RedChannelClient *rcc, SpiceMarshaller *m, PipeItem *item)
{
    TunnelChannelClient *channel;

    spice_assert(rcc);
    channel = SPICE_CONTAINEROF(rcc->channel, TunnelChannelClient, base);
    channel->send_data.u.init.max_socket_data_size = MAX_SOCKET_DATA_SIZE;
    channel->send_data.u.init.max_num_of_sockets = MAX_SOCKETS_NUM;

    red_channel_client_init_send_data(rcc, SPICE_MSG_TUNNEL_INIT, item);
    spice_marshaller_add_ref(m, (uint8_t*)&channel->send_data.u.init, sizeof(SpiceMsgTunnelInit));
}

static void tunnel_channel_marshall_service_ip_map(RedChannelClient *rcc, SpiceMarshaller *m, PipeItem *item)
{
    TunnelChannelClient *tunnel_channel;
    TunnelService *service = SPICE_CONTAINEROF(item, TunnelService, pipe_item);

    tunnel_channel = SPICE_CONTAINEROF(rcc->channel, TunnelChannelClient, base);
    tunnel_channel->send_data.u.service_ip.service_id = service->id;
    tunnel_channel->send_data.u.service_ip.virtual_ip.type = SPICE_TUNNEL_IP_TYPE_IPv4;

    red_channel_client_init_send_data(rcc, SPICE_MSG_TUNNEL_SERVICE_IP_MAP, item);
    spice_marshaller_add_ref(m, (uint8_t*)&tunnel_channel->send_data.u.service_ip,
                        sizeof(SpiceMsgTunnelServiceIpMap));
    spice_marshaller_add_ref(m, (uint8_t*)&service->virt_ip.s_addr, sizeof(SpiceTunnelIPv4));
}

static void tunnel_channel_marshall_socket_open(RedChannelClient *rcc, SpiceMarshaller *m, PipeItem *item)
{
    TunnelChannelClient *tunnel_channel;
    RedSocketOutData *sckt_out_data = SPICE_CONTAINEROF(item, RedSocketOutData, status_pipe_item);
    RedSocket *sckt = SPICE_CONTAINEROF(sckt_out_data, RedSocket, out_data);

    tunnel_channel = SPICE_CONTAINEROF(rcc->channel, TunnelChannelClient, base);
    tunnel_channel->send_data.u.socket_open.connection_id = sckt->connection_id;
    tunnel_channel->send_data.u.socket_open.service_id = sckt->far_service->id;
    tunnel_channel->send_data.u.socket_open.tokens = SOCKET_WINDOW_SIZE;

    sckt->in_data.client_total_num_tokens = SOCKET_WINDOW_SIZE;
    sckt->in_data.num_tokens = 0;
    red_channel_client_init_send_data(rcc, SPICE_MSG_TUNNEL_SOCKET_OPEN, item);
    spice_marshaller_add_ref(m, (uint8_t*)&tunnel_channel->send_data.u.socket_open,
                        sizeof(tunnel_channel->send_data.u.socket_open));
#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif
}

static void tunnel_channel_marshall_socket_fin(RedChannelClient *rcc, SpiceMarshaller *m, PipeItem *item)
{
    TunnelChannelClient *tunnel_channel;
    RedSocketOutData *sckt_out_data = SPICE_CONTAINEROF(item, RedSocketOutData, status_pipe_item);
    RedSocket *sckt = SPICE_CONTAINEROF(sckt_out_data, RedSocket, out_data);

    spice_assert(!sckt->out_data.ready_chunks_queue.head);

    tunnel_channel = SPICE_CONTAINEROF(rcc->channel, TunnelChannelClient, base);
    if (sckt->out_data.process_queue->head) {
        spice_printerr("socket sent FIN but there are still buffers in outgoing process queue"
                   "(local_port=%d, service_id=%d)",
                   ntohs(sckt->local_port), sckt->far_service->id);
    }

    tunnel_channel->send_data.u.socket_fin.connection_id = sckt->connection_id;

    red_channel_client_init_send_data(rcc, SPICE_MSG_TUNNEL_SOCKET_FIN, item);
    spice_marshaller_add_ref(m, (uint8_t*)&tunnel_channel->send_data.u.socket_fin,
                        sizeof(tunnel_channel->send_data.u.socket_fin));
#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif
}

static void tunnel_channel_marshall_socket_close(RedChannelClient *rcc, SpiceMarshaller *m, PipeItem *item)
{
    TunnelChannelClient *tunnel_channel;
    RedSocketOutData *sckt_out_data = SPICE_CONTAINEROF(item, RedSocketOutData, status_pipe_item);
    RedSocket *sckt = SPICE_CONTAINEROF(sckt_out_data, RedSocket, out_data);

    tunnel_channel = SPICE_CONTAINEROF(rcc->channel, TunnelChannelClient, base);
    // can happen when it is a forced close
    if (sckt->out_data.ready_chunks_queue.head) {
        spice_printerr("socket closed but there are still buffers in outgoing ready queue"
                   "(local_port=%d, service_id=%d)",
                   ntohs(sckt->local_port),
                   sckt->far_service->id);
    }

    if (sckt->out_data.process_queue->head) {
        spice_printerr("socket closed but there are still buffers in outgoing process queue"
                   "(local_port=%d, service_id=%d)",
                   ntohs(sckt->local_port), sckt->far_service->id);
    }

    tunnel_channel->send_data.u.socket_close.connection_id = sckt->connection_id;

    red_channel_client_init_send_data(rcc, SPICE_MSG_TUNNEL_SOCKET_CLOSE, item);
    spice_marshaller_add_ref(m, (uint8_t*)&tunnel_channel->send_data.u.socket_close,
                        sizeof(tunnel_channel->send_data.u.socket_close));
#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif
}

static void tunnel_channel_marshall_socket_closed_ack(RedChannelClient *rcc, SpiceMarshaller *m, PipeItem *item)
{
    TunnelChannelClient *tunnel_channel;
    RedSocketOutData *sckt_out_data = SPICE_CONTAINEROF(item, RedSocketOutData, status_pipe_item);
    RedSocket *sckt = SPICE_CONTAINEROF(sckt_out_data, RedSocket, out_data);

    tunnel_channel = SPICE_CONTAINEROF(rcc->channel, TunnelChannelClient, base);
    tunnel_channel->send_data.u.socket_close_ack.connection_id = sckt->connection_id;

    // pipe item is null because we free the sckt.
    red_channel_client_init_send_data(rcc, SPICE_MSG_TUNNEL_SOCKET_CLOSED_ACK, NULL);
    spice_marshaller_add_ref(m, (uint8_t*)&tunnel_channel->send_data.u.socket_close_ack,
                        sizeof(tunnel_channel->send_data.u.socket_close_ack));
#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif

    spice_assert(sckt->client_waits_close_ack && (sckt->client_status == CLIENT_SCKT_STATUS_CLOSED));
    tunnel_worker_free_socket(tunnel_channel->worker, sckt);
    if (CHECK_TUNNEL_ERROR(tunnel_channel)) {
        tunnel_shutdown(tunnel_channel->worker);
    }
}

static void tunnel_channel_marshall_socket_token(RedChannelClient *rcc, SpiceMarshaller *m, PipeItem *item)
{
    TunnelChannelClient *tunnel_channel;
    RedSocketOutData *sckt_out_data = SPICE_CONTAINEROF(item, RedSocketOutData, token_pipe_item);
    RedSocket *sckt = SPICE_CONTAINEROF(sckt_out_data, RedSocket, out_data);

    /* notice that the num of tokens sent can be > SOCKET_TOKENS_TO_SEND, since
       the sending is performed after the pipe item was pushed */

    tunnel_channel = SPICE_CONTAINEROF(rcc->channel, TunnelChannelClient, base);
    tunnel_channel->send_data.u.socket_token.connection_id = sckt->connection_id;

    if (sckt->in_data.num_tokens > 0) {
        tunnel_channel->send_data.u.socket_token.num_tokens = sckt->in_data.num_tokens;
    } else {
        spice_assert(!sckt->in_data.client_total_num_tokens && !sckt->in_data.ready_chunks_queue.head);
        tunnel_channel->send_data.u.socket_token.num_tokens = SOCKET_TOKENS_TO_SEND_FOR_PROCESS;
    }
    sckt->in_data.num_tokens -= tunnel_channel->send_data.u.socket_token.num_tokens;
    sckt->in_data.client_total_num_tokens += tunnel_channel->send_data.u.socket_token.num_tokens;
    spice_assert(sckt->in_data.client_total_num_tokens <= SOCKET_WINDOW_SIZE);

    red_channel_client_init_send_data(rcc, SPICE_MSG_TUNNEL_SOCKET_TOKEN, item);
    spice_marshaller_add_ref(m, (uint8_t*)&tunnel_channel->send_data.u.socket_token,
                        sizeof(tunnel_channel->send_data.u.socket_token));
}

static void tunnel_channel_marshall_socket_out_data(RedChannelClient *rcc, SpiceMarshaller *m, PipeItem *item)
{
    TunnelChannelClient *tunnel_channel;
    tunnel_channel = SPICE_CONTAINEROF(rcc->channel, TunnelChannelClient, base);
    RedSocketOutData *sckt_out_data = SPICE_CONTAINEROF(item, RedSocketOutData, data_pipe_item);
    RedSocket *sckt = SPICE_CONTAINEROF(sckt_out_data, RedSocket, out_data);
    ReadyTunneledChunk *chunk;
    uint32_t total_push_size = 0;
    uint32_t pushed_bufs_num = 0;

    spice_assert(!sckt->pushed_close);
    if (sckt->client_status == CLIENT_SCKT_STATUS_CLOSED) {
        return;
    }

    if (!sckt->out_data.num_tokens) {
        return; // only when an we will receive tokens, data will be sent again.
    }

    spice_assert(sckt->out_data.ready_chunks_queue.head);
    spice_assert(!sckt->out_data.push_tail);
    spice_assert(sckt->out_data.ready_chunks_queue.head->size <= MAX_SOCKET_DATA_SIZE);

    tunnel_channel->send_data.u.socket_data.connection_id = sckt->connection_id;

    red_channel_client_init_send_data(rcc, SPICE_MSG_TUNNEL_SOCKET_DATA, item);
    spice_marshaller_add_ref(m, (uint8_t*)&tunnel_channel->send_data.u.socket_data,
                        sizeof(tunnel_channel->send_data.u.socket_data));
    pushed_bufs_num++;

    // the first chunk is in a valid size
    chunk = sckt->out_data.ready_chunks_queue.head;
    total_push_size = chunk->size - sckt->out_data.ready_chunks_queue.offset;
    spice_marshaller_add_ref(m, (uint8_t*)chunk->data + sckt->out_data.ready_chunks_queue.offset,
                        total_push_size);
    pushed_bufs_num++;
    sckt->out_data.push_tail = chunk;
    sckt->out_data.push_tail_size = chunk->size; // all the chunk was sent

    chunk = chunk->next;

    while (chunk && (total_push_size < MAX_SOCKET_DATA_SIZE) && (pushed_bufs_num < MAX_SEND_BUFS)) {
        uint32_t cur_push_size = MIN(chunk->size, MAX_SOCKET_DATA_SIZE - total_push_size);
        spice_marshaller_add_ref(m, (uint8_t*)chunk->data, cur_push_size);
        pushed_bufs_num++;

        sckt->out_data.push_tail = chunk;
        sckt->out_data.push_tail_size = cur_push_size;
        total_push_size += cur_push_size;

        chunk = chunk->next;
    }

    sckt->out_data.num_tokens--;
}

static void tunnel_worker_release_socket_out_data(TunnelWorker *worker, PipeItem *item)
{
    RedSocketOutData *sckt_out_data = SPICE_CONTAINEROF(item, RedSocketOutData, data_pipe_item);
    RedSocket *sckt = SPICE_CONTAINEROF(sckt_out_data, RedSocket, out_data);

    spice_assert(sckt_out_data->ready_chunks_queue.head);

    while (sckt_out_data->ready_chunks_queue.head != sckt_out_data->push_tail) {
        sckt_out_data->data_size -= sckt_out_data->ready_chunks_queue.head->size;
        ready_queue_pop_chunk(&sckt_out_data->ready_chunks_queue);
    }

    sckt_out_data->data_size -= sckt_out_data->push_tail_size;

    // compensation. was subtracted in the previous lines
    sckt_out_data->data_size += sckt_out_data->ready_chunks_queue.offset;

    if (sckt_out_data->push_tail_size == sckt_out_data->push_tail->size) {
        ready_queue_pop_chunk(&sckt_out_data->ready_chunks_queue);
        sckt_out_data->ready_chunks_queue.offset = 0;
    } else {
        sckt_out_data->ready_chunks_queue.offset = sckt_out_data->push_tail_size;
    }

    sckt_out_data->push_tail = NULL;
    sckt_out_data->push_tail_size = 0;

    if (worker->channel_client) {
        // can still send data to socket
        if (__client_socket_can_receive(sckt)) {
            if (sckt_out_data->ready_chunks_queue.head) {
                // the pipe item may already be linked, if for example the send was
                // blocked and before it finished and called release, tunnel_socket_send was called
                if (!red_channel_client_pipe_item_is_linked(
                        &worker->channel_client->base, &sckt_out_data->data_pipe_item)) {
                    sckt_out_data->data_pipe_item.type = PIPE_ITEM_TYPE_SOCKET_DATA;
                    red_channel_client_pipe_add(&worker->channel_client->base, &sckt_out_data->data_pipe_item);
                }
            } else if ((sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_SEND) ||
                       (sckt->slirp_status == SLIRP_SCKT_STATUS_WAIT_CLOSE)) {
                __tunnel_socket_add_fin_to_pipe(worker->channel_client, sckt);
            } else if (sckt->slirp_status == SLIRP_SCKT_STATUS_CLOSED) {
                __tunnel_socket_add_close_to_pipe(worker->channel_client, sckt);
            }
        }
    }


    if (((sckt->slirp_status == SLIRP_SCKT_STATUS_OPEN) ||
         (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_RECV)) &&
        !sckt->in_slirp_send && !worker->channel_client->mig_inprogress) {
        // for cases that slirp couldn't write whole it data to our socket buffer
        net_slirp_socket_can_send_notify(sckt->slirp_sckt);
    }
}

static void tunnel_channel_send_item(RedChannelClient *rcc, PipeItem *item)
{
    SpiceMarshaller *m = red_channel_client_get_marshaller(rcc);

    switch (item->type) {
    case PIPE_ITEM_TYPE_TUNNEL_INIT:
        tunnel_channel_marshall_init(rcc, m, item);
        break;
    case PIPE_ITEM_TYPE_SERVICE_IP_MAP:
        tunnel_channel_marshall_service_ip_map(rcc, m, item);
        break;
    case PIPE_ITEM_TYPE_SOCKET_OPEN:
        tunnel_channel_marshall_socket_open(rcc, m, item);
        break;
    case PIPE_ITEM_TYPE_SOCKET_DATA:
        tunnel_channel_marshall_socket_out_data(rcc, m, item);
        break;
    case PIPE_ITEM_TYPE_SOCKET_FIN:
        tunnel_channel_marshall_socket_fin(rcc, m, item);
        break;
    case PIPE_ITEM_TYPE_SOCKET_CLOSE:
        tunnel_channel_marshall_socket_close(rcc, m, item);
        break;
    case PIPE_ITEM_TYPE_SOCKET_CLOSED_ACK:
        tunnel_channel_marshall_socket_closed_ack(rcc, m, item);
        break;
    case PIPE_ITEM_TYPE_SOCKET_TOKEN:
        tunnel_channel_marshall_socket_token(rcc, m, item);
        break;
    case PIPE_ITEM_TYPE_MIGRATE_DATA:
        tunnel_channel_marshall_migrate_data(rcc, m, item);
        break;
    default:
        spice_error("invalid pipe item type");
    }
    red_channel_client_begin_send_message(rcc);
}

/* param item_pushed: distinguishes between a pipe item that was pushed for sending, and
   a pipe item that is still in the pipe and is released due to disconnection.
   see red_pipe_item_clear */
static void tunnel_channel_release_pipe_item(RedChannelClient *rcc, PipeItem *item, int item_pushed)
{
    if (!item) { // e.g. when acking closed socket
        return;
    }
    switch (item->type) {
    case PIPE_ITEM_TYPE_TUNNEL_INIT:
        free(item);
        break;
    case PIPE_ITEM_TYPE_SERVICE_IP_MAP:
    case PIPE_ITEM_TYPE_SOCKET_OPEN:
    case PIPE_ITEM_TYPE_SOCKET_CLOSE:
    case PIPE_ITEM_TYPE_SOCKET_FIN:
    case PIPE_ITEM_TYPE_SOCKET_TOKEN:
        break;
    case PIPE_ITEM_TYPE_SOCKET_DATA:
        if (item_pushed) {
            tunnel_worker_release_socket_out_data(
                SPICE_CONTAINEROF(rcc, TunnelChannelClient, base)->worker, item);
        }
        break;
    case PIPE_ITEM_TYPE_MIGRATE:
        free(item);
        break;
    case PIPE_ITEM_TYPE_MIGRATE_DATA:
        release_migrate_item((TunnelMigrateItem *)item);
        break;
    default:
        spice_error("invalid pipe item type");
    }
}

/***********************************************************
*                   interface for slirp
************************************************************/

static int qemu_can_output(SlirpUsrNetworkInterface *usr_interface)
{
    TunnelWorker *worker = ((RedSlirpNetworkInterface *)usr_interface)->worker;
    return worker->sif->can_send_packet(worker->sin);
}

static void qemu_output(SlirpUsrNetworkInterface *usr_interface, const uint8_t *pkt, int pkt_len)
{
    TunnelWorker *worker = ((RedSlirpNetworkInterface *)usr_interface)->worker;
    worker->sif->send_packet(worker->sin, pkt, pkt_len);
}

static int null_tunnel_socket_connect(SlirpUsrNetworkInterface *usr_interface,
                                      struct in_addr src_addr, uint16_t src_port,
                                      struct in_addr dst_addr, uint16_t dst_port,
                                      SlirpSocket *slirp_s, UserSocket **o_usr_s)
{
    errno = ENETUNREACH;
    return -1;
}

static int tunnel_socket_connect(SlirpUsrNetworkInterface *usr_interface,
                                 struct in_addr src_addr, uint16_t src_port,
                                 struct in_addr dst_addr, uint16_t dst_port,
                                 SlirpSocket *slirp_s, UserSocket **o_usr_s)
{
    TunnelWorker *worker;
    RedSocket *sckt;
    TunnelService *far_service;

    spice_assert(usr_interface);

#ifdef DEBUG_NETWORK
    spice_printerr("TUNNEL_DBG");
#endif
    worker = ((RedSlirpNetworkInterface *)usr_interface)->worker;
    spice_assert(worker->channel_client);
    spice_assert(!worker->channel_client->mig_inprogress);

    far_service = tunnel_worker_find_service_by_addr(worker, &dst_addr, (uint32_t)ntohs(dst_port));

    if (!far_service) {
        errno = EADDRNOTAVAIL;
        return -1;
    }

    if (tunnel_worker_find_socket(worker, src_port, far_service->id)) {
        spice_printerr("slirp tried to open a socket that is still opened");
        errno = EADDRINUSE;
        return -1;
    }

    if (worker->num_sockets == MAX_SOCKETS_NUM) {
        spice_printerr("number of tunneled sockets exceeds the limit");
        errno = ENFILE;
        return -1;
    }

    sckt = tunnel_worker_create_socket(worker, src_port, far_service, slirp_s);

#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif
    *o_usr_s = sckt;
    sckt->out_data.status_pipe_item.type = PIPE_ITEM_TYPE_SOCKET_OPEN;
    red_channel_client_pipe_add(&worker->channel_client->base, &sckt->out_data.status_pipe_item);

    errno = EINPROGRESS;
    return -1;
}

static int null_tunnel_socket_send(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque,
                                   uint8_t *buf, size_t len, uint8_t urgent)
{
    errno = ECONNRESET;
    return -1;
}

static int tunnel_socket_send(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque,
                              uint8_t *buf, size_t len, uint8_t urgent)
{
    TunnelWorker *worker;
    RedSocket *sckt;
    size_t size_to_send;

    spice_assert(usr_interface);
    spice_assert(opaque);

    worker = ((RedSlirpNetworkInterface *)usr_interface)->worker;

    spice_assert(!worker->channel_client->mig_inprogress);

    sckt = (RedSocket *)opaque;

    if (sckt->slirp_status == SLIRP_SCKT_STATUS_DELAY_ABORT) {
        errno = EAGAIN;
        return -1;
    }

    if ((sckt->client_status != CLIENT_SCKT_STATUS_OPEN) &&
        (sckt->client_status != CLIENT_SCKT_STATUS_SHUTDOWN_SEND)) {
        spice_printerr("client socket is unable to receive data");
        errno = ECONNRESET;
        return -1;
    }


    if ((sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_SEND) ||
        (sckt->slirp_status == SLIRP_SCKT_STATUS_WAIT_CLOSE)) {
        spice_printerr("send was shutdown");
        errno = EPIPE;
        return -1;
    }

    if (urgent) {
        SET_TUNNEL_ERROR(worker->channel_client, "urgent msgs not supported");
        tunnel_shutdown(worker);
        errno = ECONNRESET;
        return -1;
    }

    sckt->in_slirp_send = TRUE;

    if (sckt->out_data.data_size < (sckt->out_data.window_size) * MAX_SOCKET_DATA_SIZE) {
        // the current data in the queues doesn't fill all the tokens
        size_to_send = len;
    } else {
        if (sckt->out_data.ready_chunks_queue.head) {
            // there are no tokens for future data, but once the data will be sent
            // and buffers will be released, we will try to send again.
            size_to_send = 0;
        } else {
            spice_assert(sckt->out_data.process_queue->head);
            if ((sckt->out_data.data_size + len) >
                                                  (MAX_SOCKET_OUT_BUFFERS * MAX_SOCKET_DATA_SIZE)) {
                spice_printerr("socket out buffers overflow, socket will be closed"
                           " (local_port=%d, service_id=%d)",
                           ntohs(sckt->local_port), sckt->far_service->id);
                tunnel_socket_force_close(worker->channel_client, sckt);
                size_to_send = 0;
            } else {
                size_to_send = len;
            }
        }
    }

    if (size_to_send) {
        process_queue_append(sckt->out_data.process_queue, buf, size_to_send);
        sckt->out_data.data_size += size_to_send;

        if (sckt->out_data.ready_chunks_queue.head &&
            !red_channel_client_pipe_item_is_linked(&worker->channel_client->base,
                                             &sckt->out_data.data_pipe_item)) {
            sckt->out_data.data_pipe_item.type = PIPE_ITEM_TYPE_SOCKET_DATA;
            red_channel_client_pipe_add(&worker->channel_client->base, &sckt->out_data.data_pipe_item);
        }
    }

    sckt->in_slirp_send = FALSE;

    if (!size_to_send) {
        errno = EAGAIN;
        return -1;
    } else {
        return size_to_send;
    }
}

static inline int __should_send_fin_to_guest(RedSocket *sckt)
{
    return (((sckt->client_status == CLIENT_SCKT_STATUS_SHUTDOWN_SEND) ||
            ((sckt->client_status == CLIENT_SCKT_STATUS_CLOSED) &&
            (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_SEND))) &&
            !sckt->in_data.ready_chunks_queue.head);
}

static int null_tunnel_socket_recv(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque,
                                   uint8_t *buf, size_t len)
{
    errno = ECONNRESET;
    return -1;
}

static int tunnel_socket_recv(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque,
                              uint8_t *buf, size_t len)
{
    TunnelWorker *worker;
    RedSocket *sckt;
    int copied = 0;

    spice_assert(usr_interface);
    spice_assert(opaque);
    worker = ((RedSlirpNetworkInterface *)usr_interface)->worker;

    spice_assert(!worker->channel_client->mig_inprogress);

    sckt = (RedSocket *)opaque;

    if (sckt->slirp_status == SLIRP_SCKT_STATUS_DELAY_ABORT) {
        errno = EAGAIN;
        return -1;
    }

    if ((sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_RECV) ||
        (sckt->slirp_status == SLIRP_SCKT_STATUS_WAIT_CLOSE)) {
        SET_TUNNEL_ERROR(worker->channel_client, "receive was shutdown");
        tunnel_shutdown(worker);
        errno = ECONNRESET;
        return -1;
    }

    if (sckt->slirp_status == SLIRP_SCKT_STATUS_CLOSED) {
        SET_TUNNEL_ERROR(worker->channel_client, "slirp socket not connected");
        tunnel_shutdown(worker);
        errno = ECONNRESET;
        return -1;
    }

    spice_assert((sckt->client_status == CLIENT_SCKT_STATUS_OPEN) ||
           (sckt->client_status == CLIENT_SCKT_STATUS_SHUTDOWN_SEND) ||
           ((sckt->client_status == CLIENT_SCKT_STATUS_CLOSED) &&
            (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_SEND)));


    // if there is data in ready queue, when it is acked, slirp will call recv and get 0
    if (__should_send_fin_to_guest(sckt)) {
        if (sckt->in_data.process_queue->head) {
            spice_printerr("client socket sent FIN but there are still buffers in incoming process"
                       "queue (local_port=%d, service_id=%d)",
                       ntohs(sckt->local_port), sckt->far_service->id);
        }
        return 0; // slirp will call shutdown recv now and it will also send FIN to the guest.
    }

    while (sckt->in_data.ready_chunks_queue.head && (copied < len)) {
        ReadyTunneledChunk *cur_chunk = sckt->in_data.ready_chunks_queue.head;
        int copy_count = MIN(cur_chunk->size - sckt->in_data.ready_chunks_queue.offset,
                             len - copied);

        memcpy(buf + copied, cur_chunk->data + sckt->in_data.ready_chunks_queue.offset, copy_count);
        copied += copy_count;
        if ((sckt->in_data.ready_chunks_queue.offset + copy_count) == cur_chunk->size) {
            ready_queue_pop_chunk(&sckt->in_data.ready_chunks_queue);
            sckt->in_data.ready_chunks_queue.offset = 0;
        } else {
            spice_assert(copied == len);
            sckt->in_data.ready_chunks_queue.offset += copy_count;
        }
    }

    if (!copied) {
        errno = EAGAIN;
        return -1;
    } else {
        return copied;
    }
}

static void null_tunnel_socket_shutdown_send(SlirpUsrNetworkInterface *usr_interface,
                                             UserSocket *opaque)
{
}

// can be called : 1) when a FIN is requested from the guest 2) after shutdown rcv that was called
//                  after received failed because the client socket was sent FIN
static void tunnel_socket_shutdown_send(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque)
{
    TunnelWorker *worker;
    RedSocket *sckt;

    spice_assert(usr_interface);
    spice_assert(opaque);
    worker = ((RedSlirpNetworkInterface *)usr_interface)->worker;
    sckt = (RedSocket *)opaque;

#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif
    spice_assert(!worker->channel_client->mig_inprogress);

    if (sckt->slirp_status == SLIRP_SCKT_STATUS_DELAY_ABORT) {
        return;
    }

    if (sckt->slirp_status == SLIRP_SCKT_STATUS_OPEN) {
        sckt->slirp_status = SLIRP_SCKT_STATUS_SHUTDOWN_SEND;
    } else if (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_RECV) {
        spice_assert(sckt->client_status == CLIENT_SCKT_STATUS_SHUTDOWN_SEND);
        sckt->slirp_status = SLIRP_SCKT_STATUS_WAIT_CLOSE;
    } else {
        SET_TUNNEL_ERROR(worker->channel_client, "unexpected tunnel_socket_shutdown_send slirp_status=%d",
                         sckt->slirp_status);
        tunnel_shutdown(worker);
        return;
    }

    if ((sckt->client_status == CLIENT_SCKT_STATUS_OPEN) ||
        (sckt->client_status == CLIENT_SCKT_STATUS_SHUTDOWN_SEND)) {
        // check if there is still data to send. the fin will be sent after data is released
        // channel is alive, otherwise the sockets would have been aborted
        if (!sckt->out_data.ready_chunks_queue.head) {
            __tunnel_socket_add_fin_to_pipe(worker->channel_client, sckt);
        }
    } else { // if client is closed, it means the connection was aborted since we didn't
             // received fin from guest
        SET_TUNNEL_ERROR(worker->channel_client,
                         "unexpected tunnel_socket_shutdown_send client_status=%d",
                         sckt->client_status);
        tunnel_shutdown(worker);
    }
}

static void null_tunnel_socket_shutdown_recv(SlirpUsrNetworkInterface *usr_interface,
                                             UserSocket *opaque)
{
}

static void tunnel_socket_shutdown_recv(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque)
{
    TunnelWorker *worker;
    RedSocket *sckt;

    spice_assert(usr_interface);
    spice_assert(opaque);
    worker = ((RedSlirpNetworkInterface *)usr_interface)->worker;
    sckt = (RedSocket *)opaque;

#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif
    spice_assert(!worker->channel_client->mig_inprogress);

    /* failure in recv can happen after the client sckt was shutdown
      (after client sent FIN, or after slirp sent FIN and client socket was closed */
    if (!__should_send_fin_to_guest(sckt)) {
        SET_TUNNEL_ERROR(worker->channel_client,
                         "unexpected tunnel_socket_shutdown_recv client_status=%d slirp_status=%d",
                         sckt->client_status, sckt->slirp_status);
        tunnel_shutdown(worker);
        return;
    }

    if (sckt->slirp_status == SLIRP_SCKT_STATUS_OPEN) {
        sckt->slirp_status = SLIRP_SCKT_STATUS_SHUTDOWN_RECV;
    } else if (sckt->slirp_status == SLIRP_SCKT_STATUS_SHUTDOWN_SEND) {
        sckt->slirp_status = SLIRP_SCKT_STATUS_WAIT_CLOSE;
    } else {
        SET_TUNNEL_ERROR(worker->channel_client,
                         "unexpected tunnel_socket_shutdown_recv slirp_status=%d",
                         sckt->slirp_status);
        tunnel_shutdown(worker);
    }
}

static void null_tunnel_socket_close(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque)
{
    TunnelWorker *worker;
    RedSocket *sckt;

    spice_assert(usr_interface);
    spice_assert(opaque);

    worker = ((RedSlirpNetworkInterface *)usr_interface)->worker;

    sckt = (RedSocket *)opaque;
#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif
    sckt->slirp_status = SLIRP_SCKT_STATUS_CLOSED;

    if (sckt->client_status == CLIENT_SCKT_STATUS_CLOSED) {
        tunnel_worker_free_socket(worker, sckt);
    } // else, it will be closed when disconnect will be called (because this callback is
      // set if the channel is disconnect or when we are in the middle of disconnection that
      // was caused by an error
}

// can be called during migration due to the channel disconnect. But it does not affect the
// migrate data
static void tunnel_socket_close(SlirpUsrNetworkInterface *usr_interface, UserSocket *opaque)
{
    TunnelWorker *worker;
    RedSocket *sckt;

    spice_assert(usr_interface);
    spice_assert(opaque);

    worker = ((RedSlirpNetworkInterface *)usr_interface)->worker;

    sckt = (RedSocket *)opaque;

#ifdef DEBUG_NETWORK
    PRINT_SCKT(sckt);
#endif

    sckt->slirp_status = SLIRP_SCKT_STATUS_CLOSED;

    // if sckt is not opened yet, close will be sent when we receive connect ack
    if ((sckt->client_status == CLIENT_SCKT_STATUS_OPEN) ||
        (sckt->client_status == CLIENT_SCKT_STATUS_SHUTDOWN_SEND)) {
        // check if there is still data to send. the close will be sent after data is released.
        // close may already been pushed if it is a forced close
        if (!sckt->out_data.ready_chunks_queue.head && !sckt->pushed_close) {
            __tunnel_socket_add_close_to_pipe(worker->channel_client, sckt);
        }
    } else if (sckt->client_status == CLIENT_SCKT_STATUS_CLOSED) {
        if (sckt->client_waits_close_ack) {
            __tunnel_socket_add_close_ack_to_pipe(worker->channel_client, sckt);
        } else {
            tunnel_worker_free_socket(worker, sckt);
        }
    }
}

static UserTimer *create_timer(SlirpUsrNetworkInterface *usr_interface,
                               timer_proc_t proc, void *opaque)
{
    TunnelWorker *worker;

    spice_assert(usr_interface);

    worker = ((RedSlirpNetworkInterface *)usr_interface)->worker;

    return (void *)worker->core_interface->timer_add(proc, opaque);
}

static void arm_timer(SlirpUsrNetworkInterface *usr_interface, UserTimer *timer, uint32_t ms)
{
    TunnelWorker *worker;

    spice_assert(usr_interface);

    worker = ((RedSlirpNetworkInterface *)usr_interface)->worker;
#ifdef DEBUG_NETWORK
    if (!worker->channel_client) {
        spice_printerr("channel not connected");
    }
#endif
    if (worker->channel_client && worker->channel_client->mig_inprogress) {
        SET_TUNNEL_ERROR(worker->channel_client, "during migration");
        tunnel_shutdown(worker);
        return;
    }

    worker->core_interface->timer_start((SpiceTimer*)timer, ms);
}

/***********************************************
* channel interface and other related procedures
************************************************/

static int tunnel_channel_config_socket(RedChannelClient *rcc)
{
    int flags;
    int delay_val;
    RedsStream *stream = red_channel_client_get_stream(rcc);

    if ((flags = fcntl(stream->socket, F_GETFL)) == -1) {
        spice_printerr("accept failed, %s", strerror(errno)); // can't we just use spice_error?
        return FALSE;
    }

    if (fcntl(stream->socket, F_SETFL, flags | O_NONBLOCK) == -1) {
        spice_printerr("accept failed, %s", strerror(errno));
        return FALSE;
    }

    delay_val = 1;

    if (setsockopt(stream->socket, IPPROTO_TCP, TCP_NODELAY, &delay_val,
                   sizeof(delay_val)) == -1) {
        if (errno != ENOTSUP) {
            spice_printerr("setsockopt failed, %s", strerror(errno));
        }
    }

    return TRUE;
}

static void tunnel_worker_disconnect_slirp(TunnelWorker *worker)
{
    int i;

    net_slirp_set_net_interface(&worker->null_interface.base);
    for (i = 0; i < MAX_SOCKETS_NUM; i++) {
        RedSocket *sckt = worker->sockets + i;
        if (sckt->allocated) {
            sckt->client_status = CLIENT_SCKT_STATUS_CLOSED;
            sckt->client_waits_close_ack = FALSE;
            if (sckt->slirp_status == SLIRP_SCKT_STATUS_CLOSED) {
                tunnel_worker_free_socket(worker, sckt);
            } else {
                sckt->slirp_status = SLIRP_SCKT_STATUS_WAIT_CLOSE;
                net_slirp_socket_abort(sckt->slirp_sckt);
            }
        }
    }
}

/* don't call disconnect from functions that might be called by slirp
   since it closes all its sockets and slirp is not aware of it */
static void tunnel_channel_on_disconnect(RedChannel *channel)
{
    TunnelWorker *worker;
    if (!channel) {
        return;
    }
    spice_printerr("");
    worker = (TunnelWorker *)channel->data;

    tunnel_worker_disconnect_slirp(worker);

    tunnel_worker_clear_routed_network(worker);
    worker->channel_client = NULL;
}

// TODO - not MC friendly, remove
static void tunnel_channel_client_on_disconnect(RedChannelClient *rcc)
{
    tunnel_channel_on_disconnect(rcc->channel);
}

/* interface for reds */

static void on_new_tunnel_channel(TunnelChannelClient *tcc, int migration)
{
    red_channel_client_push_set_ack(&tcc->base);

    if (!migration) {
        red_channel_init_outgoing_messages_window(tcc->base.channel);
        red_channel_client_pipe_add_type(&tcc->base, PIPE_ITEM_TYPE_TUNNEL_INIT);
    }
}

static void tunnel_channel_hold_pipe_item(RedChannelClient *rcc, PipeItem *item)
{
}

static void handle_tunnel_channel_link(RedChannel *channel, RedClient *client,
                                       RedsStream *stream, int migration,
                                       int num_common_caps,
                                       uint32_t *common_caps, int num_caps,
                                       uint32_t *caps)
{
    TunnelChannelClient *tcc;
    TunnelWorker *worker = (TunnelWorker *)channel->data;

    if (worker->channel_client) {
        spice_error("tunnel does not support multiple clients");
    }

    tcc = (TunnelChannelClient*)red_channel_client_create(sizeof(TunnelChannelClient),
                                                          channel, client, stream,
                                                          0, NULL, 0, NULL);
    if (!tcc) {
        return;
    }
    tcc->worker = worker;
    tcc->worker->channel_client = tcc;
    net_slirp_set_net_interface(&worker->tunnel_interface.base);

    on_new_tunnel_channel(tcc, migration);
}

static void handle_tunnel_channel_client_migrate(RedChannelClient *rcc)
{
    TunnelChannelClient *tunnel_channel;

#ifdef DEBUG_NETWORK
    spice_printerr("TUNNEL_DBG: MIGRATE STARTED");
#endif
    tunnel_channel = (TunnelChannelClient *)rcc;
    spice_assert(tunnel_channel == tunnel_channel->worker->channel_client);
    tunnel_channel->mig_inprogress = TRUE;
    net_slirp_freeze();
    red_channel_client_default_migrate(rcc);
}

static void red_tunnel_channel_create(TunnelWorker *worker)
{
    RedChannel *channel;
    ChannelCbs channel_cbs = { NULL, };
    ClientCbs client_cbs = { NULL, };

    channel_cbs.config_socket = tunnel_channel_config_socket;
    channel_cbs.on_disconnect = tunnel_channel_client_on_disconnect;
    channel_cbs.alloc_recv_buf = tunnel_channel_alloc_msg_rcv_buf;
    channel_cbs.release_recv_buf = tunnel_channel_release_msg_rcv_buf;
    channel_cbs.hold_item = tunnel_channel_hold_pipe_item;
    channel_cbs.send_item = tunnel_channel_send_item;
    channel_cbs.release_item = tunnel_channel_release_pipe_item;
    channel_cbs.handle_migrate_flush_mark = tunnel_channel_handle_migrate_mark;
    channel_cbs.handle_migrate_data = tunnel_channel_handle_migrate_data;
    channel_cbs.handle_migrate_data_get_serial = tunnel_channel_handle_migrate_data_get_serial;

    channel = red_channel_create(sizeof(RedChannel),
                                 worker->core_interface,
                                 SPICE_CHANNEL_TUNNEL, 0,
                                 TRUE,
                                 tunnel_channel_handle_message,
                                 &channel_cbs,
                                 SPICE_MIGRATE_NEED_FLUSH | SPICE_MIGRATE_NEED_DATA_TRANSFER);
    if (!channel) {
        return;
    }

    client_cbs.connect = handle_tunnel_channel_link;
    client_cbs.migrate = handle_tunnel_channel_client_migrate;
    red_channel_register_client_cbs(channel, &client_cbs);

    worker->channel = channel;
    red_channel_set_data(channel, worker);
    reds_register_channel(worker->channel);
}
