/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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

#include <arpa/inet.h>
#include <vscard_common.h>

#include "reds.h"
#include "char_device.h"
#include "red_channel.h"
#include "smartcard.h"
#include "migration_protocol.h"

/*
 * TODO: the code doesn't really support multiple readers.
 * For example: smartcard_char_device_add_to_readers calls smartcard_init,
 * which can be called only once.
 * We should allow different readers, at least one reader per client.
 * In addition the implementation should be changed: instead of one channel for
 * all readers, we need to have different channles for different readers,
 * similarly to spicevmc.
 *
 */
#define SMARTCARD_MAX_READERS 10

typedef struct SmartCardDeviceState SmartCardDeviceState;

typedef struct SmartCardChannelClient {
    RedChannelClient base;
    SmartCardDeviceState *smartcard_state;

    /* read_from_client/write_to_device buffer.
     * The beginning of the buffer should always be VSCMsgHeader*/
    SpiceCharDeviceWriteBuffer *write_buf;
    int msg_in_write_buf; /* was the client msg received into a SpiceCharDeviceWriteBuffer
                           * or was it explicitly malloced */
} SmartCardChannelClient;

struct SmartCardDeviceState {
    SpiceCharDeviceState *chardev_st;
    uint32_t             reader_id;
    /* read_from_device buffer */
    uint8_t             *buf;
    uint32_t             buf_size;
    uint8_t             *buf_pos;
    uint32_t             buf_used;

    SmartCardChannelClient    *scc; // client providing the remote card
    int                  reader_added; // has reader_add been sent to the device
};

enum {
    PIPE_ITEM_TYPE_ERROR = PIPE_ITEM_TYPE_CHANNEL_BASE,
    PIPE_ITEM_TYPE_SMARTCARD_DATA,
    PIPE_ITEM_TYPE_SMARTCARD_MIGRATE_DATA,
};

typedef struct ErrorItem {
    PipeItem base;
    VSCMsgHeader vheader;
    VSCMsgError  error;
} ErrorItem;

typedef struct MsgItem {
    PipeItem base;
    uint32_t refs;

    VSCMsgHeader* vheader;
} MsgItem;

static MsgItem *smartcard_get_vsc_msg_item(RedChannelClient *rcc, VSCMsgHeader *vheader);
static MsgItem *smartcard_ref_vsc_msg_item(MsgItem *item);
static void smartcard_unref_vsc_msg_item(MsgItem *item);
static void smartcard_channel_client_pipe_add_push(RedChannelClient *rcc, PipeItem *item);

typedef struct SmartCardChannel {
    RedChannel base;
} SmartCardChannel;

static struct Readers {
    uint32_t num;
    SpiceCharDeviceInstance* sin[SMARTCARD_MAX_READERS];
} g_smartcard_readers = {0, {NULL}};

static SpiceCharDeviceInstance* smartcard_readers_get_unattached(void);
static SpiceCharDeviceInstance* smartcard_readers_get(uint32_t reader_id);
static int smartcard_char_device_add_to_readers(SpiceCharDeviceInstance *sin);
static void smartcard_char_device_attach_client(
    SpiceCharDeviceInstance *char_device, SmartCardChannelClient *scc);
static void smartcard_channel_write_to_reader(SpiceCharDeviceWriteBuffer *write_buf);

static MsgItem *smartcard_char_device_on_message_from_device(
    SmartCardDeviceState *state, VSCMsgHeader *header);
static SmartCardDeviceState *smartcard_device_state_new(SpiceCharDeviceInstance *sin);
static void smartcard_device_state_free(SmartCardDeviceState* st);
static void smartcard_init(void);

static void smartcard_read_buf_prepare(SmartCardDeviceState *state, VSCMsgHeader *vheader)
{
    uint32_t msg_len;

    msg_len = ntohl(vheader->length);
    if (msg_len > state->buf_size) {
        state->buf_size = MAX(state->buf_size * 2, msg_len + sizeof(VSCMsgHeader));
        state->buf = spice_realloc(state->buf, state->buf_size);
    }
}

SpiceCharDeviceMsgToClient *smartcard_read_msg_from_device(SpiceCharDeviceInstance *sin,
                                                           void *opaque)
{
    SmartCardDeviceState *state = opaque;
    SpiceCharDeviceInterface *sif = SPICE_CONTAINEROF(sin->base.sif, SpiceCharDeviceInterface, base);
    VSCMsgHeader *vheader = (VSCMsgHeader*)state->buf;
    int n;
    int remaining;
    int actual_length;

    while ((n = sif->read(sin, state->buf_pos, state->buf_size - state->buf_used)) > 0) {
        MsgItem *msg_to_client;

        state->buf_pos += n;
        state->buf_used += n;
        if (state->buf_used < sizeof(VSCMsgHeader)) {
            continue;
        }
        smartcard_read_buf_prepare(state, vheader);
        actual_length = ntohl(vheader->length);
        if (state->buf_used - sizeof(VSCMsgHeader) < actual_length) {
            continue;
        }
        msg_to_client = smartcard_char_device_on_message_from_device(state, vheader);
        remaining = state->buf_used - sizeof(VSCMsgHeader) - actual_length;
        if (remaining > 0) {
            memcpy(state->buf, state->buf_pos, remaining);
        }
        state->buf_pos = state->buf;
        state->buf_used = remaining;
        if (msg_to_client) {
            return msg_to_client;
        }
    }
    return NULL;
}

static SpiceCharDeviceMsgToClient *smartcard_ref_msg_to_client(SpiceCharDeviceMsgToClient *msg,
                                                               void *opaque)
{
    return smartcard_ref_vsc_msg_item((MsgItem *)msg);
}

static void smartcard_unref_msg_to_client(SpiceCharDeviceMsgToClient *msg,
                                          void *opaque)
{
    smartcard_unref_vsc_msg_item((MsgItem *)msg);
}

static void smartcard_send_msg_to_client(SpiceCharDeviceMsgToClient *msg,
                                         RedClient *client,
                                         void *opaque)
{
    SmartCardDeviceState *dev = opaque;
    spice_assert(dev->scc && dev->scc->base.client == client);
    smartcard_channel_client_pipe_add_push(&dev->scc->base, &((MsgItem *)msg)->base);

}

static void smartcard_send_tokens_to_client(RedClient *client, uint32_t tokens, void *opaque)
{
    spice_error("not implemented");
}

static void smartcard_remove_client(RedClient *client, void *opaque)
{
    SmartCardDeviceState *dev = opaque;

    spice_printerr("smartcard  state %p, client %p", dev, client);
    spice_assert(dev->scc && dev->scc->base.client == client);
    red_channel_client_shutdown(&dev->scc->base);
}

MsgItem *smartcard_char_device_on_message_from_device(SmartCardDeviceState *state,
                                                      VSCMsgHeader *vheader)
{
    VSCMsgHeader *sent_header;

    vheader->type = ntohl(vheader->type);
    vheader->length = ntohl(vheader->length);
    vheader->reader_id = ntohl(vheader->reader_id);

    switch (vheader->type) {
        case VSC_Init:
            return NULL;
        default:
            break;
    }
    /* We pass any VSC_Error right now - might need to ignore some? */
    if (state->reader_id == VSCARD_UNDEFINED_READER_ID && vheader->type != VSC_Init) {
        spice_printerr("error: reader_id not assigned for message of type %d", vheader->type);
    }
    if (state->scc) {
        sent_header = spice_memdup(vheader, sizeof(*vheader) + vheader->length);
        /* We patch the reader_id, since the device only knows about itself, and
         * we know about the sum of readers. */
        sent_header->reader_id = state->reader_id;
        return smartcard_get_vsc_msg_item(&state->scc->base, sent_header);
    }
    return NULL;
}

static int smartcard_char_device_add_to_readers(SpiceCharDeviceInstance *char_device)
{
    SmartCardDeviceState *state = spice_char_device_state_opaque_get(char_device->st);

    if (g_smartcard_readers.num >= SMARTCARD_MAX_READERS) {
        return -1;
    }
    state->reader_id = g_smartcard_readers.num;
    g_smartcard_readers.sin[g_smartcard_readers.num++] = char_device;
    smartcard_init();
    return 0;
}

static SpiceCharDeviceInstance *smartcard_readers_get(uint32_t reader_id)
{
    spice_assert(reader_id < g_smartcard_readers.num);
    return g_smartcard_readers.sin[reader_id];
}

/* TODO: fix implementation for multiple readers. Each reader should have a separated
 * channel */
static SpiceCharDeviceInstance *smartcard_readers_get_unattached(void)
{
    int i;
    SmartCardDeviceState* state;

    for (i = 0; i < g_smartcard_readers.num; ++i) {
        state = spice_char_device_state_opaque_get(g_smartcard_readers.sin[i]->st);
        if (!state->scc) {
            return g_smartcard_readers.sin[i];
        }
    }
    return NULL;
}

static SmartCardDeviceState *smartcard_device_state_new(SpiceCharDeviceInstance *sin)
{
    SmartCardDeviceState *st;
    SpiceCharDeviceCallbacks chardev_cbs = { NULL, };

    chardev_cbs.read_one_msg_from_device = smartcard_read_msg_from_device;
    chardev_cbs.ref_msg_to_client = smartcard_ref_msg_to_client;
    chardev_cbs.unref_msg_to_client = smartcard_unref_msg_to_client;
    chardev_cbs.send_msg_to_client = smartcard_send_msg_to_client;
    chardev_cbs.send_tokens_to_client = smartcard_send_tokens_to_client;
    chardev_cbs.remove_client = smartcard_remove_client;

    st = spice_new0(SmartCardDeviceState, 1);
    st->chardev_st = spice_char_device_state_create(sin,
                                                    0, /* tokens interval */
                                                    ~0, /* self tokens */
                                                    &chardev_cbs,
                                                    st);
    st->reader_id = VSCARD_UNDEFINED_READER_ID;
    st->reader_added = FALSE;
    st->buf_size = APDUBufSize + sizeof(VSCMsgHeader);
    st->buf = spice_malloc(st->buf_size);
    st->buf_pos = st->buf;
    st->buf_used = 0;
    st->scc = NULL;
    return st;
}

static void smartcard_device_state_free(SmartCardDeviceState* st)
{
    if (st->scc) {
        st->scc->smartcard_state = NULL;
    }
    free(st->buf);
    spice_char_device_state_destroy(st->chardev_st);
    free(st);
}

void smartcard_device_disconnect(SpiceCharDeviceInstance *char_device)
{
    SmartCardDeviceState *st = spice_char_device_state_opaque_get(char_device->st);

    smartcard_device_state_free(st);
}

SpiceCharDeviceState *smartcard_device_connect(SpiceCharDeviceInstance *char_device)
{
    SmartCardDeviceState *st;

    st = smartcard_device_state_new(char_device);
    if (smartcard_char_device_add_to_readers(char_device) == -1) {
        smartcard_device_state_free(st);
        return NULL;
    }
    return st->chardev_st;
}

static void smartcard_char_device_notify_reader_add(SmartCardDeviceState *st)
{
    SpiceCharDeviceWriteBuffer *write_buf;
    VSCMsgHeader *vheader;

    write_buf = spice_char_device_write_buffer_get(st->chardev_st, NULL, sizeof(vheader));
    if (!write_buf) {
        spice_error("failed to allocate write buffer");
        return;
    }
    st->reader_added = TRUE;
    vheader = (VSCMsgHeader *)write_buf->buf;
    vheader->type = VSC_ReaderAdd;
    vheader->reader_id = st->reader_id;
    vheader->length = 0;
    smartcard_channel_write_to_reader(write_buf);
}

static void smartcard_char_device_attach_client(SpiceCharDeviceInstance *char_device,
                                                SmartCardChannelClient *scc)
{
    SmartCardDeviceState *st = spice_char_device_state_opaque_get(char_device->st);
    int client_added;

    spice_assert(!scc->smartcard_state && !st->scc);
    st->scc = scc;
    scc->smartcard_state = st;
    client_added = spice_char_device_client_add(st->chardev_st,
                                                scc->base.client,
                                                FALSE, /* no flow control yet */
                                                0, /* send queue size */
                                                ~0,
                                                ~0,
                                                red_channel_client_waits_for_migrate_data(
                                                    &scc->base));
    if (!client_added) {
        spice_warning("failed");
        st->scc = NULL;
        scc->smartcard_state = NULL;
        red_channel_client_disconnect(&scc->base);
    }
}

static void smartcard_char_device_notify_reader_remove(SmartCardDeviceState *st)
{
    SpiceCharDeviceWriteBuffer *write_buf;
    VSCMsgHeader *vheader;

    if (!st->reader_added) {
        spice_debug("reader add was never sent to the device");
        return;
    }
    write_buf = spice_char_device_write_buffer_get(st->chardev_st, NULL, sizeof(vheader));
    if (!write_buf) {
        spice_error("failed to allocate write buffer");
        return;
    }
    st->reader_added = FALSE;
    vheader = (VSCMsgHeader *)write_buf->buf;
    vheader->type = VSC_ReaderRemove;
    vheader->reader_id = st->reader_id;
    vheader->length = 0;
    smartcard_channel_write_to_reader(write_buf);
}

static void smartcard_char_device_detach_client(SmartCardChannelClient *scc)
{
    SmartCardDeviceState *st;

    if (!scc->smartcard_state) {
        return;
    }
    st = scc->smartcard_state;
    spice_assert(st->scc == scc);
    spice_char_device_client_remove(st->chardev_st, scc->base.client);
    scc->smartcard_state = NULL;
    st->scc = NULL;
}

static int smartcard_channel_client_config_socket(RedChannelClient *rcc)
{
    return TRUE;
}

static uint8_t *smartcard_channel_alloc_msg_rcv_buf(RedChannelClient *rcc,
                                                    uint16_t type,
                                                    uint32_t size)
{
    SmartCardChannelClient *scc = SPICE_CONTAINEROF(rcc, SmartCardChannelClient, base);

    /* todo: only one reader is actually supported. When we fix the code to support
     * multiple readers, we will porbably associate different devices to
     * differenc channels */
    if (!scc->smartcard_state) {
        scc->msg_in_write_buf = FALSE;
        return spice_malloc(size);
    } else {
        SmartCardDeviceState *st;

        spice_assert(g_smartcard_readers.num == 1);
        st = scc->smartcard_state;
        spice_assert(st->scc || scc->smartcard_state);
        spice_assert(!scc->write_buf);
        scc->write_buf = spice_char_device_write_buffer_get(st->chardev_st, rcc->client, size);

        if (!scc->write_buf) {
            spice_error("failed to allocate write buffer");
            return NULL;
        }
        scc->msg_in_write_buf = TRUE;
        return scc->write_buf->buf;
    }
}

static void smartcard_channel_release_msg_rcv_buf(RedChannelClient *rcc,
                                                  uint16_t type,
                                                  uint32_t size,
                                                  uint8_t *msg)
{
    SmartCardChannelClient *scc = SPICE_CONTAINEROF(rcc, SmartCardChannelClient, base);

    /* todo: only one reader is actually supported. When we fix the code to support
     * multiple readers, we will porbably associate different devices to
     * differenc channels */

    if (!scc->msg_in_write_buf) {
        spice_assert(!scc->write_buf);
        free(msg);
    } else {
        SpiceCharDeviceState *dev_st;
        if (scc->write_buf) { /* msg hasn't been pushed to the guest */
            spice_assert(scc->write_buf->buf == msg);
            dev_st = scc->smartcard_state ? scc->smartcard_state->chardev_st : NULL;
            spice_char_device_write_buffer_release(dev_st, scc->write_buf);
            scc->write_buf = NULL;
        }
    }
}

static void smartcard_channel_send_data(RedChannelClient *rcc, SpiceMarshaller *m,
                                        PipeItem *item, VSCMsgHeader *vheader)
{
    spice_assert(rcc);
    spice_assert(vheader);
    red_channel_client_init_send_data(rcc, SPICE_MSG_SMARTCARD_DATA, item);
    spice_marshaller_add_ref(m, (uint8_t*)vheader, sizeof(VSCMsgHeader));
    if (vheader->length > 0) {
        spice_marshaller_add_ref(m, (uint8_t*)(vheader+1), vheader->length);
    }
}

static void smartcard_channel_send_error(
    RedChannelClient *rcc, SpiceMarshaller *m, PipeItem *item)
{
    ErrorItem* error_item = (ErrorItem*)item;

    smartcard_channel_send_data(rcc, m, item, &error_item->vheader);
}

static void smartcard_channel_send_msg(RedChannelClient *rcc,
                                       SpiceMarshaller *m, PipeItem *item)
{
    MsgItem* msg_item = (MsgItem*)item;

    smartcard_channel_send_data(rcc, m, item, msg_item->vheader);
}

static void smartcard_channel_send_migrate_data(RedChannelClient *rcc,
                                                SpiceMarshaller *m, PipeItem *item)
{
    SmartCardChannelClient *scc;
    SmartCardDeviceState *state;
    SpiceMarshaller *m2;

    scc = SPICE_CONTAINEROF(rcc, SmartCardChannelClient, base);
    state = scc->smartcard_state;
    red_channel_client_init_send_data(rcc, SPICE_MSG_MIGRATE_DATA, item);
    spice_marshaller_add_uint32(m, SPICE_MIGRATE_DATA_SMARTCARD_MAGIC);
    spice_marshaller_add_uint32(m, SPICE_MIGRATE_DATA_SMARTCARD_VERSION);

    if (!state) {
        spice_char_device_state_migrate_data_marshall_empty(m);
        spice_marshaller_add_uint8(m, 0);
        spice_marshaller_add_uint32(m, 0);
        spice_marshaller_add_uint32(m, 0);
        spice_debug("null char dev state");
    } else {
        spice_char_device_state_migrate_data_marshall(state->chardev_st, m);
        spice_marshaller_add_uint8(m, state->reader_added);
        spice_marshaller_add_uint32(m, state->buf_used);
        m2 = spice_marshaller_get_ptr_submarshaller(m, 0);
        spice_marshaller_add(m2, state->buf, state->buf_used);
        spice_debug("reader added %d partial read size %u", state->reader_added, state->buf_used);
    }
}

static void smartcard_channel_send_item(RedChannelClient *rcc, PipeItem *item)
{
    SpiceMarshaller *m = red_channel_client_get_marshaller(rcc);

    switch (item->type) {
    case PIPE_ITEM_TYPE_ERROR:
        smartcard_channel_send_error(rcc, m, item);
        break;
    case PIPE_ITEM_TYPE_SMARTCARD_DATA:
        smartcard_channel_send_msg(rcc, m, item);
        break;
    case PIPE_ITEM_TYPE_SMARTCARD_MIGRATE_DATA:
        smartcard_channel_send_migrate_data(rcc, m, item);
        break;
    default:
        spice_error("bad pipe item %d", item->type);
        free(item);
        return;
    }
    red_channel_client_begin_send_message(rcc);
}

static void smartcard_channel_release_pipe_item(RedChannelClient *rcc,
                                      PipeItem *item, int item_pushed)
{
    if (item->type == PIPE_ITEM_TYPE_SMARTCARD_DATA) {
        smartcard_unref_vsc_msg_item((MsgItem *)item);
    } else {
        free(item);
    }
}

static void smartcard_channel_on_disconnect(RedChannelClient *rcc)
{
    SmartCardChannelClient *scc = SPICE_CONTAINEROF(rcc, SmartCardChannelClient, base);

    if (scc->smartcard_state) {
        SmartCardDeviceState *st = scc->smartcard_state;

        smartcard_char_device_detach_client(scc);
        smartcard_char_device_notify_reader_remove(st);
    }
}

/* this is called from both device input and client input. since the device is
 * a usb device, the context is still the main thread (kvm_main_loop, timers)
 * so no mutex is required. */
static void smartcard_channel_client_pipe_add_push(RedChannelClient *rcc, PipeItem *item)
{
    red_channel_client_pipe_add_push(rcc, item);
}

static void smartcard_push_error(RedChannelClient *rcc, uint32_t reader_id, VSCErrorCode error)
{
    ErrorItem *error_item = spice_new0(ErrorItem, 1);

    red_channel_pipe_item_init(rcc->channel, &error_item->base,
                               PIPE_ITEM_TYPE_ERROR);

    error_item->base.type = PIPE_ITEM_TYPE_ERROR;
    error_item->vheader.reader_id = reader_id;
    error_item->vheader.type = VSC_Error;
    error_item->vheader.length = sizeof(error_item->error);
    error_item->error.code = error;
    smartcard_channel_client_pipe_add_push(rcc, &error_item->base);
}

static MsgItem *smartcard_get_vsc_msg_item(RedChannelClient *rcc, VSCMsgHeader *vheader)
{
    MsgItem *msg_item = spice_new0(MsgItem, 1);

    red_channel_pipe_item_init(rcc->channel, &msg_item->base,
                               PIPE_ITEM_TYPE_SMARTCARD_DATA);
    msg_item->refs = 1;
    msg_item->vheader = vheader;
    return msg_item;
}

static MsgItem *smartcard_ref_vsc_msg_item(MsgItem *item)
{
    item->refs++;
    return item;
}

static void smartcard_unref_vsc_msg_item(MsgItem *item)
{
    if (!--item->refs) {
        free(item->vheader);
        free(item);
    }
}

static void smartcard_remove_reader(SmartCardChannelClient *scc, uint32_t reader_id)
{
    SpiceCharDeviceInstance *char_device = smartcard_readers_get(reader_id);
    SmartCardDeviceState *state;

    if (char_device == NULL) {
        smartcard_push_error(&scc->base, reader_id,
            VSC_GENERAL_ERROR);
        return;
    }

    state = spice_char_device_state_opaque_get(char_device->st);
    if (state->reader_added == FALSE) {
        smartcard_push_error(&scc->base, reader_id,
            VSC_GENERAL_ERROR);
        return;
    }
    spice_assert(scc->smartcard_state == state);
    smartcard_char_device_notify_reader_remove(state);
}

static void smartcard_add_reader(SmartCardChannelClient *scc, uint8_t *name)
{
    if (!scc->smartcard_state) { /* we already tried to attach a reader to the client
                                    when it connected */
        SpiceCharDeviceInstance *char_device = smartcard_readers_get_unattached();

        if (!char_device) {
            smartcard_push_error(&scc->base, VSCARD_UNDEFINED_READER_ID,
                                VSC_CANNOT_ADD_MORE_READERS);
            return;
        }
        smartcard_char_device_attach_client(char_device, scc);
    }
    smartcard_char_device_notify_reader_add(scc->smartcard_state);
    // The device sends a VSC_Error message, we will let it through, no
    // need to send our own. We already set the correct reader_id, from
    // our SmartCardDeviceState.
}

static void smartcard_channel_write_to_reader(SpiceCharDeviceWriteBuffer *write_buf)
{
    SpiceCharDeviceInstance *sin;
    SmartCardDeviceState *st;
    VSCMsgHeader *vheader;
    uint32_t actual_length;

    vheader = (VSCMsgHeader *)write_buf->buf;
    actual_length = vheader->length;

    spice_assert(vheader->reader_id <= g_smartcard_readers.num);
    sin = g_smartcard_readers.sin[vheader->reader_id];
    st = (SmartCardDeviceState *)spice_char_device_state_opaque_get(sin->st);
    spice_assert(!st->scc || st == st->scc->smartcard_state);
    /* protocol requires messages to be in network endianess */
    vheader->type = htonl(vheader->type);
    vheader->length = htonl(vheader->length);
    vheader->reader_id = htonl(vheader->reader_id);
    write_buf->buf_used = actual_length + sizeof(VSCMsgHeader);
    /* pushing the buffer to the write queue; It will be released
     * when it will be fully consumed by the device */
    spice_char_device_write_buffer_add(sin->st, write_buf);
    if (st->scc && write_buf == st->scc->write_buf) {
        st->scc->write_buf = NULL;
    }
}

static int smartcard_channel_client_handle_migrate_flush_mark(RedChannelClient *rcc)
{
    red_channel_client_pipe_add_type(rcc, PIPE_ITEM_TYPE_SMARTCARD_MIGRATE_DATA);
    return TRUE;
}

static void smartcard_device_state_restore_partial_read(SmartCardDeviceState *state,
                                                        SpiceMigrateDataSmartcard *mig_data)
{
    uint8_t *read_data;

    spice_debug("read_size  %u", mig_data->read_size);
    read_data = (uint8_t *)mig_data + mig_data->read_data_ptr - sizeof(SpiceMigrateDataHeader);
    if (mig_data->read_size < sizeof(VSCMsgHeader)) {
        spice_assert(state->buf_size >= mig_data->read_size);
    } else {
        smartcard_read_buf_prepare(state, (VSCMsgHeader *)read_data);
    }
    memcpy(state->buf, read_data, mig_data->read_size);
    state->buf_used = mig_data->read_size;
    state->buf_pos = state->buf + mig_data->read_size;
}

static int smartcard_channel_client_handle_migrate_data(RedChannelClient *rcc,
                                                        uint32_t size, void *message)
{
    SmartCardChannelClient *scc;
    SpiceMigrateDataHeader *header;
    SpiceMigrateDataSmartcard *mig_data;

    scc = SPICE_CONTAINEROF(rcc, SmartCardChannelClient, base);
    header = (SpiceMigrateDataHeader *)message;
    mig_data = (SpiceMigrateDataSmartcard *)(header + 1);
    if (size < sizeof(SpiceMigrateDataHeader) + sizeof(SpiceMigrateDataSmartcard)) {
        spice_error("bad message size");
        return FALSE;
    }
    if (!migration_protocol_validate_header(header,
                                            SPICE_MIGRATE_DATA_SMARTCARD_MAGIC,
                                            SPICE_MIGRATE_DATA_SMARTCARD_VERSION)) {
        spice_error("bad header");
        return FALSE;
    }

    if (!mig_data->base.connected) { /* client wasn't attached to a smartcard */
        return TRUE;
    }

    if (!scc->smartcard_state) {
        SpiceCharDeviceInstance *char_device = smartcard_readers_get_unattached();

        if (!char_device) {
            spice_warning("no unattached device available");
            return TRUE;
        } else {
            smartcard_char_device_attach_client(char_device, scc);
        }
    }
    spice_debug("reader added %d partial read_size %u", mig_data->reader_added, mig_data->read_size);
    scc->smartcard_state->reader_added = mig_data->reader_added;

    smartcard_device_state_restore_partial_read(scc->smartcard_state, mig_data);
    return spice_char_device_state_restore(scc->smartcard_state->chardev_st, &mig_data->base);
}

static int smartcard_channel_handle_message(RedChannelClient *rcc,
                                            uint16_t type,
                                            uint32_t size,
                                            uint8_t *msg)
{
    VSCMsgHeader* vheader = (VSCMsgHeader*)msg;
    SmartCardChannelClient *scc = SPICE_CONTAINEROF(rcc, SmartCardChannelClient, base);

    if (type != SPICE_MSGC_SMARTCARD_DATA) {
        /* Handles seamless migration protocol. Also handles ack's,
         * spicy sends them while spicec does not */
        return red_channel_client_handle_message(rcc, size, type, msg);
    }

    spice_assert(size == vheader->length + sizeof(VSCMsgHeader));
    switch (vheader->type) {
        case VSC_ReaderAdd:
            smartcard_add_reader(scc, msg + sizeof(VSCMsgHeader));
            return TRUE;
            break;
        case VSC_ReaderRemove:
            smartcard_remove_reader(scc, vheader->reader_id);
            return TRUE;
            break;
        case VSC_Init:
            // ignore - we should never get this anyway
            return TRUE;
            break;
        case VSC_Error:
        case VSC_ATR:
        case VSC_CardRemove:
        case VSC_APDU:
            break; // passed on to device
        default:
            printf("ERROR: unexpected message on smartcard channel\n");
            return TRUE;
    }

    /* todo: fix */
    if (vheader->reader_id >= g_smartcard_readers.num) {
        spice_printerr("ERROR: received message for non existing reader: %d, %d, %d", vheader->reader_id,
            vheader->type, vheader->length);
        return FALSE;
    }
    spice_assert(scc->write_buf->buf == msg);
    smartcard_channel_write_to_reader(scc->write_buf);

    return TRUE;
}

static void smartcard_channel_hold_pipe_item(RedChannelClient *rcc, PipeItem *item)
{
}

static void smartcard_connect_client(RedChannel *channel, RedClient *client,
                                     RedsStream *stream, int migration,
                                     int num_common_caps, uint32_t *common_caps,
                                     int num_caps, uint32_t *caps)
{
    SpiceCharDeviceInstance *char_device =
            smartcard_readers_get_unattached();

    SmartCardChannelClient *scc;

    scc = (SmartCardChannelClient *)red_channel_client_create(sizeof(SmartCardChannelClient),
                                                              channel,
                                                              client,
                                                              stream,
                                                              FALSE,
                                                              num_common_caps, common_caps,
                                                              num_caps, caps);

    if (!scc) {
        return;
    }
    red_channel_client_ack_zero_messages_window(&scc->base);

    if (char_device) {
        smartcard_char_device_attach_client(char_device, scc);
    } else {
        spice_printerr("char dev unavailable");
    }
}

SmartCardChannel *g_smartcard_channel;

static void smartcard_init(void)
{
    ChannelCbs channel_cbs = { NULL, };
    ClientCbs client_cbs = { NULL, };
    uint32_t migration_flags = SPICE_MIGRATE_NEED_FLUSH | SPICE_MIGRATE_NEED_DATA_TRANSFER;

    spice_assert(!g_smartcard_channel);

    channel_cbs.config_socket = smartcard_channel_client_config_socket;
    channel_cbs.on_disconnect = smartcard_channel_on_disconnect;
    channel_cbs.send_item = smartcard_channel_send_item;
    channel_cbs.hold_item = smartcard_channel_hold_pipe_item;
    channel_cbs.release_item = smartcard_channel_release_pipe_item;
    channel_cbs.alloc_recv_buf = smartcard_channel_alloc_msg_rcv_buf;
    channel_cbs.release_recv_buf = smartcard_channel_release_msg_rcv_buf;
    channel_cbs.handle_migrate_flush_mark = smartcard_channel_client_handle_migrate_flush_mark;
    channel_cbs.handle_migrate_data = smartcard_channel_client_handle_migrate_data;

    g_smartcard_channel = (SmartCardChannel*)red_channel_create(sizeof(SmartCardChannel),
                                             core, SPICE_CHANNEL_SMARTCARD, 0,
                                             FALSE /* handle_acks */,
                                             smartcard_channel_handle_message,
                                             &channel_cbs,
                                             migration_flags);

    if (!g_smartcard_channel) {
        spice_error("failed to allocate Smartcard Channel");
    }

    client_cbs.connect = smartcard_connect_client;
    red_channel_register_client_cbs(&g_smartcard_channel->base, &client_cbs);

    reds_register_channel(&g_smartcard_channel->base);
}
