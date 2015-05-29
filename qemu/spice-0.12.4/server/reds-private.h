#ifndef REDS_PRIVATE_H
#define REDS_PRIVATE_H

#include <time.h>

#include <spice/protocol.h>

#define MIGRATE_TIMEOUT (1000 * 10) /* 10sec */
#define MM_TIMER_GRANULARITY_MS (1000 / 30)
#define MM_TIME_DELTA 400 /*ms*/

typedef struct TicketAuthentication {
    char password[SPICE_MAX_PASSWORD_LENGTH];
    time_t expiration_time;
} TicketAuthentication;

typedef struct TicketInfo {
    RSA *rsa;
    int rsa_size;
    BIGNUM *bn;
    SpiceLinkEncryptedTicket encrypted_ticket;
} TicketInfo;

typedef struct MonitorMode {
    uint32_t x_res;
    uint32_t y_res;
} MonitorMode;

typedef struct VDIReadBuf {
    RingItem link;
    uint32_t refs;

    int len;
    uint8_t data[SPICE_AGENT_MAX_DATA_SIZE];
} VDIReadBuf;

static VDIReadBuf *vdi_port_read_buf_get(void);
static VDIReadBuf *vdi_port_read_buf_ref(VDIReadBuf *buf);
static void vdi_port_read_buf_unref(VDIReadBuf *buf);

enum {
    VDI_PORT_READ_STATE_READ_HEADER,
    VDI_PORT_READ_STATE_GET_BUFF,
    VDI_PORT_READ_STATE_READ_DATA,
};

typedef struct VDIPortState {
    SpiceCharDeviceState *base;
    uint32_t plug_generation;
    int client_agent_started;

    /* write to agent */
    SpiceCharDeviceWriteBuffer *recv_from_client_buf;
    int recv_from_client_buf_pushed;
    AgentMsgFilter write_filter;

    /* read from agent */
    Ring read_bufs;
    uint32_t read_state;
    uint32_t message_recive_len;
    uint8_t *recive_pos;
    uint32_t recive_len;
    VDIReadBuf *current_read_buf;
    AgentMsgFilter read_filter;

    VDIChunkHeader vdi_chunk_header;

    SpiceMigrateDataMain *mig_data; /* storing it when migration data arrives
                                       before agent is attached */
} VDIPortState;

/* messages that are addressed to the agent and are created in the server */
typedef struct __attribute__ ((__packed__)) VDInternalBuf {
    VDIChunkHeader chunk_header;
    VDAgentMessage header;
    union {
        VDAgentMouseState mouse_state;
    }
    u;
} VDInternalBuf;

#ifdef RED_STATISTICS

#define REDS_MAX_STAT_NODES 100
#define REDS_STAT_SHM_SIZE (sizeof(SpiceStat) + REDS_MAX_STAT_NODES * sizeof(SpiceStatNode))

typedef struct RedsStatValue {
    uint32_t value;
    uint32_t min;
    uint32_t max;
    uint32_t average;
    uint32_t count;
} RedsStatValue;

#endif

typedef struct RedsMigPendingLink {
    RingItem ring_link; // list of links that belongs to the same client
    SpiceLinkMess *link_msg;
    RedsStream *stream;
} RedsMigPendingLink;

typedef struct RedsMigTargetClient {
    RingItem link;
    RedClient *client;
    Ring pending_links;
} RedsMigTargetClient;

typedef struct RedsMigWaitDisconnectClient {
    RingItem link;
    RedClient *client;
} RedsMigWaitDisconnectClient;

typedef struct SpiceCharDeviceStateItem {
    RingItem link;
    SpiceCharDeviceState *st;
} SpiceCharDeviceStateItem;

/* Intermediate state for on going monitors config message from a single
 * client, being passed to the guest */
typedef struct RedsClientMonitorsConfig {
    MainChannelClient *mcc;
    uint8_t *buffer;
    int buffer_size;
    int buffer_pos;
} RedsClientMonitorsConfig;

typedef struct RedsState {
    int listen_socket;
    int secure_listen_socket;
    SpiceWatch *listen_watch;
    SpiceWatch *secure_listen_watch;
    VDIPortState agent_state;
    int pending_mouse_event;
    Ring clients;
    int num_clients;
    MainChannel *main_channel;

    int mig_wait_connect; /* src waits for clients to establish connection to dest
                             (before migration starts) */
    int mig_wait_disconnect; /* src waits for clients to disconnect (after migration completes) */
    Ring mig_wait_disconnect_clients; /* List of RedsMigWaitDisconnectClient. Holds the clients
                                         which the src waits for their disconnection */

    int mig_inprogress;
    int expect_migrate;
    int src_do_seamless_migrate; /* per migration. Updated after the migration handshake
                                    between the 2 servers */
    int dst_do_seamless_migrate; /* per migration. Updated after the migration handshake
                                    between the 2 servers */
    Ring mig_target_clients;
    int num_mig_target_clients;
    RedsMigSpice *mig_spice;

    int num_of_channels;
    Ring channels;
    int mouse_mode;
    int is_client_mouse_allowed;
    int dispatcher_allows_client_mouse;
    MonitorMode monitor_mode;
    SpiceTimer *mig_timer;
    SpiceTimer *mm_timer;

    int vm_running;
    Ring char_devs_states; /* list of SpiceCharDeviceStateItem */
    int seamless_migration_enabled; /* command line arg */

    SSL_CTX *ctx;

#ifdef RED_STATISTICS
    char *stat_shm_name;
    SpiceStat *stat;
    pthread_mutex_t stat_lock;
    RedsStatValue roundtrip_stat;
#endif
    int peer_minor_version;
    int allow_multiple_clients;

    RedsClientMonitorsConfig client_monitors_config;
    int mm_timer_enabled;
    uint32_t mm_time_latency;
} RedsState;

#endif
