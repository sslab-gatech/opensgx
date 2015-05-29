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

#include <netinet/in.h> // IPPROTO_TCP
#include <netinet/tcp.h> // TCP_NODELAY
#include <fcntl.h>
#include <stddef.h> // NULL
#include <errno.h>
#include <spice/macros.h>
#include <spice/vd_agent.h>
#include <spice/protocol.h>

#include "common/marshaller.h"
#include "common/messages.h"
#include "common/generated_server_marshallers.h"

#include "demarshallers.h"
#include "spice.h"
#include "red_common.h"
#include "reds.h"
#include "red_channel.h"
#include "main_channel.h"
#include "inputs_channel.h"
#include "migration_protocol.h"

// TODO: RECEIVE_BUF_SIZE used to be the same for inputs_channel and main_channel
// since it was defined once in reds.c which contained both.
// Now that they are split we can give a more fitting value for inputs - what
// should it be?
#define REDS_AGENT_WINDOW_SIZE 10
#define REDS_NUM_INTERNAL_AGENT_MESSAGES 1

// approximate max receive message size
#define RECEIVE_BUF_SIZE \
    (4096 + (REDS_AGENT_WINDOW_SIZE + REDS_NUM_INTERNAL_AGENT_MESSAGES) * SPICE_AGENT_MAX_DATA_SIZE)

struct SpiceKbdState {
    int dummy;
};

struct SpiceMouseState {
    int dummy;
};

struct SpiceTabletState {
    int dummy;
};

typedef struct InputsChannelClient {
    RedChannelClient base;
    uint16_t motion_count;
} InputsChannelClient;

typedef struct InputsChannel {
    RedChannel base;
    uint8_t recv_buf[RECEIVE_BUF_SIZE];
    VDAgentMouseState mouse_state;
    int src_during_migrate;
} InputsChannel;

enum {
    PIPE_ITEM_INPUTS_INIT = PIPE_ITEM_TYPE_CHANNEL_BASE,
    PIPE_ITEM_MOUSE_MOTION_ACK,
    PIPE_ITEM_KEY_MODIFIERS,
    PIPE_ITEM_MIGRATE_DATA,
};

typedef struct InputsPipeItem {
    PipeItem base;
} InputsPipeItem;

typedef struct KeyModifiersPipeItem {
    PipeItem base;
    uint8_t modifiers;
} KeyModifiersPipeItem;

typedef struct InputsInitPipeItem {
    PipeItem base;
    uint8_t modifiers;
} InputsInitPipeItem;

static SpiceKbdInstance *keyboard = NULL;
static SpiceMouseInstance *mouse = NULL;
static SpiceTabletInstance *tablet = NULL;

static SpiceTimer *key_modifiers_timer;

static InputsChannel *g_inputs_channel = NULL;

#define KEY_MODIFIERS_TTL (1000 * 2) /*2sec*/

#define SCROLL_LOCK_SCAN_CODE 0x46
#define NUM_LOCK_SCAN_CODE 0x45
#define CAPS_LOCK_SCAN_CODE 0x3a

int inputs_inited(void)
{
    return !!g_inputs_channel;
}

int inputs_set_keyboard(SpiceKbdInstance *_keyboard)
{
    if (keyboard) {
        spice_printerr("already have keyboard");
        return -1;
    }
    keyboard = _keyboard;
    keyboard->st = spice_new0(SpiceKbdState, 1);
    return 0;
}

int inputs_set_mouse(SpiceMouseInstance *_mouse)
{
    if (mouse) {
        spice_printerr("already have mouse");
        return -1;
    }
    mouse = _mouse;
    mouse->st = spice_new0(SpiceMouseState, 1);
    return 0;
}

int inputs_set_tablet(SpiceTabletInstance *_tablet)
{
    if (tablet) {
        spice_printerr("already have tablet");
        return -1;
    }
    tablet = _tablet;
    tablet->st = spice_new0(SpiceTabletState, 1);
    return 0;
}

int inputs_has_tablet(void)
{
    return !!tablet;
}

void inputs_detach_tablet(SpiceTabletInstance *_tablet)
{
    spice_printerr("");
    tablet = NULL;
}

void inputs_set_tablet_logical_size(int x_res, int y_res)
{
    SpiceTabletInterface *sif;

    sif = SPICE_CONTAINEROF(tablet->base.sif, SpiceTabletInterface, base);
    sif->set_logical_size(tablet, x_res, y_res);
}

const VDAgentMouseState *inputs_get_mouse_state(void)
{
    spice_assert(g_inputs_channel);
    return &g_inputs_channel->mouse_state;
}

static uint8_t *inputs_channel_alloc_msg_rcv_buf(RedChannelClient *rcc,
                                                 uint16_t type,
                                                 uint32_t size)
{
    InputsChannel *inputs_channel = SPICE_CONTAINEROF(rcc->channel, InputsChannel, base);

    if (size > RECEIVE_BUF_SIZE) {
        spice_printerr("error: too large incoming message");
        return NULL;
    }
    return inputs_channel->recv_buf;
}

static void inputs_channel_release_msg_rcv_buf(RedChannelClient *rcc,
                                               uint16_t type,
                                               uint32_t size,
                                               uint8_t *msg)
{
}

#define OUTGOING_OK 0
#define OUTGOING_FAILED -1
#define OUTGOING_BLOCKED 1

#define RED_MOUSE_STATE_TO_LOCAL(state)     \
    ((state & SPICE_MOUSE_BUTTON_MASK_LEFT) |          \
     ((state & SPICE_MOUSE_BUTTON_MASK_MIDDLE) << 1) |   \
     ((state & SPICE_MOUSE_BUTTON_MASK_RIGHT) >> 1))

#define RED_MOUSE_BUTTON_STATE_TO_AGENT(state)                      \
    (((state & SPICE_MOUSE_BUTTON_MASK_LEFT) ? VD_AGENT_LBUTTON_MASK : 0) |    \
     ((state & SPICE_MOUSE_BUTTON_MASK_MIDDLE) ? VD_AGENT_MBUTTON_MASK : 0) |    \
     ((state & SPICE_MOUSE_BUTTON_MASK_RIGHT) ? VD_AGENT_RBUTTON_MASK : 0))

static void activate_modifiers_watch(void)
{
    core->timer_start(key_modifiers_timer, KEY_MODIFIERS_TTL);
}

static void kbd_push_scan(SpiceKbdInstance *sin, uint8_t scan)
{
    SpiceKbdInterface *sif;

    if (!sin) {
        return;
    }
    sif = SPICE_CONTAINEROF(sin->base.sif, SpiceKbdInterface, base);
    sif->push_scan_freg(sin, scan);
}

static uint8_t kbd_get_leds(SpiceKbdInstance *sin)
{
    SpiceKbdInterface *sif;

    if (!sin) {
        return 0;
    }
    sif = SPICE_CONTAINEROF(sin->base.sif, SpiceKbdInterface, base);
    return sif->get_leds(sin);
}

static PipeItem *inputs_key_modifiers_item_new(
    RedChannelClient *rcc, void *data, int num)
{
    KeyModifiersPipeItem *item = spice_malloc(sizeof(KeyModifiersPipeItem));

    red_channel_pipe_item_init(rcc->channel, &item->base,
                               PIPE_ITEM_KEY_MODIFIERS);
    item->modifiers = *(uint8_t *)data;
    return &item->base;
}

static void inputs_channel_send_migrate_data(RedChannelClient *rcc,
                                             SpiceMarshaller *m,
                                             PipeItem *item)
{
    InputsChannelClient *icc = SPICE_CONTAINEROF(rcc, InputsChannelClient, base);

    g_inputs_channel->src_during_migrate = FALSE;
    red_channel_client_init_send_data(rcc, SPICE_MSG_MIGRATE_DATA, item);

    spice_marshaller_add_uint32(m, SPICE_MIGRATE_DATA_INPUTS_MAGIC);
    spice_marshaller_add_uint32(m, SPICE_MIGRATE_DATA_INPUTS_VERSION);
    spice_marshaller_add_uint16(m, icc->motion_count);
}

static void inputs_channel_release_pipe_item(RedChannelClient *rcc,
    PipeItem *base, int item_pushed)
{
    free(base);
}

static void inputs_channel_send_item(RedChannelClient *rcc, PipeItem *base)
{
    SpiceMarshaller *m = red_channel_client_get_marshaller(rcc);

    switch (base->type) {
        case PIPE_ITEM_KEY_MODIFIERS:
        {
            SpiceMsgInputsKeyModifiers key_modifiers;

            red_channel_client_init_send_data(rcc, SPICE_MSG_INPUTS_KEY_MODIFIERS, base);
            key_modifiers.modifiers =
                SPICE_CONTAINEROF(base, KeyModifiersPipeItem, base)->modifiers;
            spice_marshall_msg_inputs_key_modifiers(m, &key_modifiers);
            break;
        }
        case PIPE_ITEM_INPUTS_INIT:
        {
            SpiceMsgInputsInit inputs_init;

            red_channel_client_init_send_data(rcc, SPICE_MSG_INPUTS_INIT, base);
            inputs_init.keyboard_modifiers =
                SPICE_CONTAINEROF(base, InputsInitPipeItem, base)->modifiers;
            spice_marshall_msg_inputs_init(m, &inputs_init);
            break;
        }
        case PIPE_ITEM_MOUSE_MOTION_ACK:
            red_channel_client_init_send_data(rcc, SPICE_MSG_INPUTS_MOUSE_MOTION_ACK, base);
            break;
        case PIPE_ITEM_MIGRATE_DATA:
            inputs_channel_send_migrate_data(rcc, m, base);
            break;
        default:
            spice_warning("invalid pipe iten %d", base->type);
            break;
    }
    red_channel_client_begin_send_message(rcc);
}

static int inputs_channel_handle_parsed(RedChannelClient *rcc, uint32_t size, uint16_t type,
                                        void *message)
{
    InputsChannel *inputs_channel = (InputsChannel *)rcc->channel;
    InputsChannelClient *icc = (InputsChannelClient *)rcc;
    uint8_t *buf = (uint8_t *)message;
    uint32_t i;

    spice_assert(g_inputs_channel == inputs_channel);
    switch (type) {
    case SPICE_MSGC_INPUTS_KEY_DOWN: {
        SpiceMsgcKeyDown *key_up = (SpiceMsgcKeyDown *)buf;
        if (key_up->code == CAPS_LOCK_SCAN_CODE || key_up->code == NUM_LOCK_SCAN_CODE ||
            key_up->code == SCROLL_LOCK_SCAN_CODE) {
            activate_modifiers_watch();
        }
    }
    case SPICE_MSGC_INPUTS_KEY_UP: {
        SpiceMsgcKeyDown *key_down = (SpiceMsgcKeyDown *)buf;
        for (i = 0; i < 4; i++) {
            uint8_t code = (key_down->code >> (i * 8)) & 0xff;
            if (code == 0) {
                break;
            }
            kbd_push_scan(keyboard, code);
        }
        break;
    }
    case SPICE_MSGC_INPUTS_KEY_SCANCODE: {
        uint8_t *code = (uint8_t *)buf;
        for (i = 0; i < size; i++) {
            kbd_push_scan(keyboard, code[i]);
        }
        break;
    }
    case SPICE_MSGC_INPUTS_MOUSE_MOTION: {
        SpiceMsgcMouseMotion *mouse_motion = (SpiceMsgcMouseMotion *)buf;

        if (++icc->motion_count % SPICE_INPUT_MOTION_ACK_BUNCH == 0 &&
            !g_inputs_channel->src_during_migrate) {
            red_channel_client_pipe_add_type(rcc, PIPE_ITEM_MOUSE_MOTION_ACK);
            icc->motion_count = 0;
        }
        if (mouse && reds_get_mouse_mode() == SPICE_MOUSE_MODE_SERVER) {
            SpiceMouseInterface *sif;
            sif = SPICE_CONTAINEROF(mouse->base.sif, SpiceMouseInterface, base);
            sif->motion(mouse,
                        mouse_motion->dx, mouse_motion->dy, 0,
                        RED_MOUSE_STATE_TO_LOCAL(mouse_motion->buttons_state));
        }
        break;
    }
    case SPICE_MSGC_INPUTS_MOUSE_POSITION: {
        SpiceMsgcMousePosition *pos = (SpiceMsgcMousePosition *)buf;

        if (++icc->motion_count % SPICE_INPUT_MOTION_ACK_BUNCH == 0 &&
            !g_inputs_channel->src_during_migrate) {
            red_channel_client_pipe_add_type(rcc, PIPE_ITEM_MOUSE_MOTION_ACK);
            icc->motion_count = 0;
        }
        if (reds_get_mouse_mode() != SPICE_MOUSE_MODE_CLIENT) {
            break;
        }
        spice_assert((reds_get_agent_mouse() && reds_has_vdagent()) || tablet);
        if (!reds_get_agent_mouse() || !reds_has_vdagent()) {
            SpiceTabletInterface *sif;
            sif = SPICE_CONTAINEROF(tablet->base.sif, SpiceTabletInterface, base);
            sif->position(tablet, pos->x, pos->y, RED_MOUSE_STATE_TO_LOCAL(pos->buttons_state));
            break;
        }
        VDAgentMouseState *mouse_state = &inputs_channel->mouse_state;
        mouse_state->x = pos->x;
        mouse_state->y = pos->y;
        mouse_state->buttons = RED_MOUSE_BUTTON_STATE_TO_AGENT(pos->buttons_state);
        mouse_state->display_id = pos->display_id;
        reds_handle_agent_mouse_event(mouse_state);
        break;
    }
    case SPICE_MSGC_INPUTS_MOUSE_PRESS: {
        SpiceMsgcMousePress *mouse_press = (SpiceMsgcMousePress *)buf;
        int dz = 0;
        if (mouse_press->button == SPICE_MOUSE_BUTTON_UP) {
            dz = -1;
        } else if (mouse_press->button == SPICE_MOUSE_BUTTON_DOWN) {
            dz = 1;
        }
        if (reds_get_mouse_mode() == SPICE_MOUSE_MODE_CLIENT) {
            if (reds_get_agent_mouse() && reds_has_vdagent()) {
                inputs_channel->mouse_state.buttons =
                    RED_MOUSE_BUTTON_STATE_TO_AGENT(mouse_press->buttons_state) |
                    (dz == -1 ? VD_AGENT_UBUTTON_MASK : 0) |
                    (dz == 1 ? VD_AGENT_DBUTTON_MASK : 0);
                reds_handle_agent_mouse_event(&inputs_channel->mouse_state);
            } else if (tablet) {
                SpiceTabletInterface *sif;
                sif = SPICE_CONTAINEROF(tablet->base.sif, SpiceTabletInterface, base);
                sif->wheel(tablet, dz, RED_MOUSE_STATE_TO_LOCAL(mouse_press->buttons_state));
            }
        } else if (mouse) {
            SpiceMouseInterface *sif;
            sif = SPICE_CONTAINEROF(mouse->base.sif, SpiceMouseInterface, base);
            sif->motion(mouse, 0, 0, dz,
                        RED_MOUSE_STATE_TO_LOCAL(mouse_press->buttons_state));
        }
        break;
    }
    case SPICE_MSGC_INPUTS_MOUSE_RELEASE: {
        SpiceMsgcMouseRelease *mouse_release = (SpiceMsgcMouseRelease *)buf;
        if (reds_get_mouse_mode() == SPICE_MOUSE_MODE_CLIENT) {
            if (reds_get_agent_mouse() && reds_has_vdagent()) {
                inputs_channel->mouse_state.buttons =
                    RED_MOUSE_BUTTON_STATE_TO_AGENT(mouse_release->buttons_state);
                reds_handle_agent_mouse_event(&inputs_channel->mouse_state);
            } else if (tablet) {
                SpiceTabletInterface *sif;
                sif = SPICE_CONTAINEROF(tablet->base.sif, SpiceTabletInterface, base);
                sif->buttons(tablet, RED_MOUSE_STATE_TO_LOCAL(mouse_release->buttons_state));
            }
        } else if (mouse) {
            SpiceMouseInterface *sif;
            sif = SPICE_CONTAINEROF(mouse->base.sif, SpiceMouseInterface, base);
            sif->buttons(mouse,
                         RED_MOUSE_STATE_TO_LOCAL(mouse_release->buttons_state));
        }
        break;
    }
    case SPICE_MSGC_INPUTS_KEY_MODIFIERS: {
        SpiceMsgcKeyModifiers *modifiers = (SpiceMsgcKeyModifiers *)buf;
        uint8_t leds;

        if (!keyboard) {
            break;
        }
        leds = kbd_get_leds(keyboard);
        if ((modifiers->modifiers & SPICE_KEYBOARD_MODIFIER_FLAGS_SCROLL_LOCK) !=
            (leds & SPICE_KEYBOARD_MODIFIER_FLAGS_SCROLL_LOCK)) {
            kbd_push_scan(keyboard, SCROLL_LOCK_SCAN_CODE);
            kbd_push_scan(keyboard, SCROLL_LOCK_SCAN_CODE | 0x80);
        }
        if ((modifiers->modifiers & SPICE_KEYBOARD_MODIFIER_FLAGS_NUM_LOCK) !=
            (leds & SPICE_KEYBOARD_MODIFIER_FLAGS_NUM_LOCK)) {
            kbd_push_scan(keyboard, NUM_LOCK_SCAN_CODE);
            kbd_push_scan(keyboard, NUM_LOCK_SCAN_CODE | 0x80);
        }
        if ((modifiers->modifiers & SPICE_KEYBOARD_MODIFIER_FLAGS_CAPS_LOCK) !=
            (leds & SPICE_KEYBOARD_MODIFIER_FLAGS_CAPS_LOCK)) {
            kbd_push_scan(keyboard, CAPS_LOCK_SCAN_CODE);
            kbd_push_scan(keyboard, CAPS_LOCK_SCAN_CODE | 0x80);
        }
        activate_modifiers_watch();
        break;
    }
    case SPICE_MSGC_DISCONNECTING:
        break;
    default:
        return red_channel_client_handle_message(rcc, size, type, message);
    }
    return TRUE;
}

static void inputs_relase_keys(void)
{
    kbd_push_scan(keyboard, 0x2a | 0x80); //LSHIFT
    kbd_push_scan(keyboard, 0x36 | 0x80); //RSHIFT
    kbd_push_scan(keyboard, 0xe0); kbd_push_scan(keyboard, 0x1d | 0x80); //RCTRL
    kbd_push_scan(keyboard, 0x1d | 0x80); //LCTRL
    kbd_push_scan(keyboard, 0xe0); kbd_push_scan(keyboard, 0x38 | 0x80); //RALT
    kbd_push_scan(keyboard, 0x38 | 0x80); //LALT
}

static void inputs_channel_on_disconnect(RedChannelClient *rcc)
{
    if (!rcc) {
        return;
    }
    inputs_relase_keys();
}

static void inputs_pipe_add_init(RedChannelClient *rcc)
{
    InputsInitPipeItem *item = spice_malloc(sizeof(InputsInitPipeItem));

    red_channel_pipe_item_init(rcc->channel, &item->base,
                               PIPE_ITEM_INPUTS_INIT);
    item->modifiers = kbd_get_leds(keyboard);
    red_channel_client_pipe_add_push(rcc, &item->base);
}

static int inputs_channel_config_socket(RedChannelClient *rcc)
{
    int delay_val = 1;
    RedsStream *stream = red_channel_client_get_stream(rcc);

    if (setsockopt(stream->socket, IPPROTO_TCP, TCP_NODELAY,
            &delay_val, sizeof(delay_val)) == -1) {
        if (errno != ENOTSUP && errno != ENOPROTOOPT) {
            spice_printerr("setsockopt failed, %s", strerror(errno));
            return FALSE;
        }
    }

    return TRUE;
}

static void inputs_channel_hold_pipe_item(RedChannelClient *rcc, PipeItem *item)
{
}

static void inputs_connect(RedChannel *channel, RedClient *client,
                           RedsStream *stream, int migration,
                           int num_common_caps, uint32_t *common_caps,
                           int num_caps, uint32_t *caps)
{
    InputsChannelClient *icc;

    spice_assert(g_inputs_channel);
    spice_assert(channel == &g_inputs_channel->base);

    if (!stream->ssl && !red_client_during_migrate_at_target(client)) {
        main_channel_client_push_notify(red_client_get_main(client),
                                        "keyboard channel is insecure");
    }

    spice_printerr("inputs channel client create");
    icc = (InputsChannelClient*)red_channel_client_create(sizeof(InputsChannelClient),
                                                          channel,
                                                          client,
                                                          stream,
                                                          FALSE,
                                                          num_common_caps, common_caps,
                                                          num_caps, caps);
    if (!icc) {
        return;
    }
    icc->motion_count = 0;
    inputs_pipe_add_init(&icc->base);
}

static void inputs_migrate(RedChannelClient *rcc)
{
    g_inputs_channel->src_during_migrate = TRUE;
    red_channel_client_default_migrate(rcc);
}

static void inputs_push_keyboard_modifiers(uint8_t modifiers)
{
    if (!g_inputs_channel || !red_channel_is_connected(&g_inputs_channel->base) ||
        g_inputs_channel->src_during_migrate) {
        return;
    }
    red_channel_pipes_new_add_push(&g_inputs_channel->base,
        inputs_key_modifiers_item_new, (void*)&modifiers);
}

void inputs_on_keyboard_leds_change(void *opaque, uint8_t leds)
{
    inputs_push_keyboard_modifiers(leds);
}

static void key_modifiers_sender(void *opaque)
{
    inputs_push_keyboard_modifiers(kbd_get_leds(keyboard));
}

static int inputs_channel_handle_migrate_flush_mark(RedChannelClient *rcc)
{
    red_channel_client_pipe_add_type(rcc, PIPE_ITEM_MIGRATE_DATA);
    return TRUE;
}

static int inputs_channel_handle_migrate_data(RedChannelClient *rcc,
                                              uint32_t size,
                                              void *message)
{
    InputsChannelClient *icc = SPICE_CONTAINEROF(rcc, InputsChannelClient, base);
    SpiceMigrateDataHeader *header;
    SpiceMigrateDataInputs *mig_data;

    header = (SpiceMigrateDataHeader *)message;
    mig_data = (SpiceMigrateDataInputs *)(header + 1);

    if (!migration_protocol_validate_header(header,
                                            SPICE_MIGRATE_DATA_INPUTS_MAGIC,
                                            SPICE_MIGRATE_DATA_INPUTS_VERSION)) {
        spice_error("bad header");
        return FALSE;
    }
    key_modifiers_sender(NULL);
    icc->motion_count = mig_data->motion_count;

    for (; icc->motion_count >= SPICE_INPUT_MOTION_ACK_BUNCH;
           icc->motion_count -= SPICE_INPUT_MOTION_ACK_BUNCH) {
        red_channel_client_pipe_add_type(rcc, PIPE_ITEM_MOUSE_MOTION_ACK);
    }
    return TRUE;
}

void inputs_init(void)
{
    ChannelCbs channel_cbs = { NULL, };
    ClientCbs client_cbs = { NULL, };

    spice_assert(!g_inputs_channel);

    channel_cbs.config_socket = inputs_channel_config_socket;
    channel_cbs.on_disconnect = inputs_channel_on_disconnect;
    channel_cbs.send_item = inputs_channel_send_item;
    channel_cbs.hold_item = inputs_channel_hold_pipe_item;
    channel_cbs.release_item = inputs_channel_release_pipe_item;
    channel_cbs.alloc_recv_buf = inputs_channel_alloc_msg_rcv_buf;
    channel_cbs.release_recv_buf = inputs_channel_release_msg_rcv_buf;
    channel_cbs.handle_migrate_data = inputs_channel_handle_migrate_data;
    channel_cbs.handle_migrate_flush_mark = inputs_channel_handle_migrate_flush_mark;

    g_inputs_channel = (InputsChannel *)red_channel_create_parser(
                                    sizeof(InputsChannel),
                                    core,
                                    SPICE_CHANNEL_INPUTS, 0,
                                    FALSE, /* handle_acks */
                                    spice_get_client_channel_parser(SPICE_CHANNEL_INPUTS, NULL),
                                    inputs_channel_handle_parsed,
                                    &channel_cbs,
                                    SPICE_MIGRATE_NEED_FLUSH | SPICE_MIGRATE_NEED_DATA_TRANSFER);

    if (!g_inputs_channel) {
        spice_error("failed to allocate Inputs Channel");
    }

    client_cbs.connect = inputs_connect;
    client_cbs.migrate = inputs_migrate;
    red_channel_register_client_cbs(&g_inputs_channel->base, &client_cbs);

    red_channel_set_cap(&g_inputs_channel->base, SPICE_INPUTS_CAP_KEY_SCANCODE);
    reds_register_channel(&g_inputs_channel->base);

    if (!(key_modifiers_timer = core->timer_add(key_modifiers_sender, NULL))) {
        spice_error("key modifiers timer create failed");
    }
}
