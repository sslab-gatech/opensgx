/**
 * Test vdagent guest to server messages
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <spice/vd_agent.h>

#include "test_display_base.h"

SpiceCoreInterface *core;
SpiceTimer *ping_timer;

int ping_ms = 100;

#ifndef MIN
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#endif

void pinger(void *opaque)
{
    // show_channels is not thread safe - fails if disconnections / connections occur
    //show_channels(server);

    core->timer_start(ping_timer, ping_ms);
}


static int vmc_write(SpiceCharDeviceInstance *sin, const uint8_t *buf, int len)
{
    return len;
}

static int vmc_read(SpiceCharDeviceInstance *sin, uint8_t *buf, int len)
{
    static uint8_t c = 0;
    static uint8_t message[2048];
    static unsigned pos = 0;
    static unsigned message_size;
    int ret;

    if (pos == 0) {
        VDIChunkHeader *hdr = (VDIChunkHeader *)message;
        VDAgentMessage *msg = (VDAgentMessage *)&hdr[1];
        uint8_t *p = message;
        int size = sizeof(message);
        message_size = size;
        /* fill in message */
        hdr->port = VDP_SERVER_PORT;
        hdr->size = message_size - sizeof(VDIChunkHeader);
        msg->protocol = VD_AGENT_PROTOCOL;
        msg->type = VD_AGENT_END_MESSAGE;
        msg->opaque = 0;
        msg->size = message_size - sizeof(VDIChunkHeader) - sizeof(VDAgentMessage);
        size -= sizeof(VDIChunkHeader) + sizeof(VDAgentMessage);
        p += sizeof(VDIChunkHeader) + sizeof(VDAgentMessage);
        for (; size; --size, ++p, ++c)
            *p = c;
    }
    ret = MIN(message_size - pos, len);
    memcpy(buf, &message[pos], ret);
    pos += ret;
    if (pos == message_size) {
        pos = 0;
    }
    //printf("vmc_read %d (ret %d)\n", len, ret);
    return ret;
}

static void vmc_state(SpiceCharDeviceInstance *sin, int connected)
{
}

static SpiceCharDeviceInterface vmc_interface = {
    .base.type          = SPICE_INTERFACE_CHAR_DEVICE,
    .base.description   = "test spice virtual channel char device",
    .base.major_version = SPICE_INTERFACE_CHAR_DEVICE_MAJOR,
    .base.minor_version = SPICE_INTERFACE_CHAR_DEVICE_MINOR,
    .state              = vmc_state,
    .write              = vmc_write,
    .read               = vmc_read,
};

SpiceCharDeviceInstance vmc_instance = {
    .subtype = "vdagent",
};

int main(void)
{
    Test *test;

    core = basic_event_loop_init();
    test = test_new(core);

    vmc_instance.base.sif = &vmc_interface.base;
    spice_server_add_interface(test->server, &vmc_instance.base);

    ping_timer = core->timer_add(pinger, NULL);
    core->timer_start(ping_timer, ping_ms);

    basic_event_loop_mainloop();

    return 0;
}
