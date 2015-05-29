#ifndef __SPICE_EXPERIMENTAL_H__
#define __SPICE_EXPERIMENTAL_H__

#include "spice.h"

/* tunnel interface */

#define SPICE_INTERFACE_NET_WIRE "net_wire"
#define SPICE_INTERFACE_NET_WIRE_MAJOR 1
#define SPICE_INTERFACE_NET_WIRE_MINOR 1
typedef struct SpiceNetWireInterface SpiceNetWireInterface;
typedef struct SpiceNetWireInstance SpiceNetWireInstance;
typedef struct SpiceNetWireState SpiceNetWireState;

struct SpiceNetWireInterface {
    SpiceBaseInterface base;

    struct in_addr (*get_ip)(SpiceNetWireInstance *sin);
    int (*can_send_packet)(SpiceNetWireInstance *sin);
    void (*send_packet)(SpiceNetWireInstance *sin, const uint8_t *pkt, int len);
};

struct SpiceNetWireInstance {
    SpiceBaseInstance base;
    SpiceNetWireState *st;
};

void spice_server_net_wire_recv_packet(SpiceNetWireInstance *sin,
                                       const uint8_t *pkt, int len);

/* spice seamless client migration (broken) */
enum {
    SPICE_MIGRATE_CLIENT_NONE = 1,
    SPICE_MIGRATE_CLIENT_WAITING,
    SPICE_MIGRATE_CLIENT_READY,
};

int spice_server_migrate_client_state(SpiceServer *s);

#endif // __SPICE_EXPERIMENTAL_H__
