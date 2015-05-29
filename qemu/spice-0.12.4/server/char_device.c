/* spice-server char device flow control code

   Copyright (C) 2012 Red Hat, Inc.

   Red Hat Authors:
   Yonit Halperin <yhalperi@redhat.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http:www.gnu.org/licenses/>.
*/


#include <config.h>
#include "char_device.h"
#include "red_channel.h"
#include "reds.h"

#define CHAR_DEVICE_WRITE_TO_TIMEOUT 100
#define SPICE_CHAR_DEVICE_WAIT_TOKENS_TIMEOUT 30000

typedef struct SpiceCharDeviceClientState SpiceCharDeviceClientState;
struct SpiceCharDeviceClientState {
    RingItem link;
    SpiceCharDeviceState *dev;
    RedClient *client;
    int do_flow_control;
    uint64_t num_client_tokens;
    uint64_t num_client_tokens_free; /* client messages that were consumed by the device */
    uint64_t num_send_tokens; /* send to client */
    SpiceTimer *wait_for_tokens_timer;
    int wait_for_tokens_started;
    Ring send_queue;
    uint32_t send_queue_size;
    uint32_t max_send_queue_size;
};

struct SpiceCharDeviceState {
    int running;
    int active; /* has read/write been performed since the device was started */
    int wait_for_migrate_data;
    uint32_t refs;

    Ring write_queue;
    Ring write_bufs_pool;
    SpiceCharDeviceWriteBuffer *cur_write_buf;
    uint8_t *cur_write_buf_pos;
    SpiceTimer *write_to_dev_timer;
    uint64_t num_self_tokens;

    Ring clients; /* list of SpiceCharDeviceClientState */
    uint32_t num_clients;

    uint64_t client_tokens_interval; /* frequency of returning tokens to the client */
    SpiceCharDeviceInstance *sin;

    int during_read_from_device;

    SpiceCharDeviceCallbacks cbs;
    void *opaque;
};

enum {
    WRITE_BUFFER_ORIGIN_NONE,
    WRITE_BUFFER_ORIGIN_CLIENT,
    WRITE_BUFFER_ORIGIN_SERVER,
    WRITE_BUFFER_ORIGIN_SERVER_NO_TOKEN,
};

/* Holding references for avoiding access violation if the char device was
 * destroyed during a callback */
static void spice_char_device_state_ref(SpiceCharDeviceState *char_dev);
static void spice_char_device_state_unref(SpiceCharDeviceState *char_dev);

static void spice_char_dev_write_retry(void *opaque);

typedef struct SpiceCharDeviceMsgToClientItem {
    RingItem link;
    SpiceCharDeviceMsgToClient *msg;
} SpiceCharDeviceMsgToClientItem;

static void spice_char_device_write_buffer_free(SpiceCharDeviceWriteBuffer *buf)
{
    if (--buf->refs == 0) {
        free(buf->buf);
        free(buf);
    }
}

static void write_buffers_queue_free(Ring *write_queue)
{
    while (!ring_is_empty(write_queue)) {
        RingItem *item = ring_get_tail(write_queue);
        SpiceCharDeviceWriteBuffer *buf;

        ring_remove(item);
        buf = SPICE_CONTAINEROF(item, SpiceCharDeviceWriteBuffer, link);
        spice_char_device_write_buffer_free(buf);
    }
}

static void spice_char_device_write_buffer_pool_add(SpiceCharDeviceState *dev,
                                                    SpiceCharDeviceWriteBuffer *buf)
{
    if (buf->refs == 1) {
        buf->buf_used = 0;
        buf->origin = WRITE_BUFFER_ORIGIN_NONE;
        buf->client = NULL;
        ring_add(&dev->write_bufs_pool, &buf->link);
    } else {
        --buf->refs;
    }
}

static void spice_char_device_client_send_queue_free(SpiceCharDeviceState *dev,
                                                     SpiceCharDeviceClientState *dev_client)
{
    spice_debug("send_queue_empty %d", ring_is_empty(&dev_client->send_queue));
    while (!ring_is_empty(&dev_client->send_queue)) {
        RingItem *item = ring_get_tail(&dev_client->send_queue);
        SpiceCharDeviceMsgToClientItem *msg_item = SPICE_CONTAINEROF(item,
                                                                     SpiceCharDeviceMsgToClientItem,
                                                                     link);

        ring_remove(item);
        dev->cbs.unref_msg_to_client(msg_item->msg, dev->opaque);
        free(msg_item);
    }
    dev_client->num_send_tokens += dev_client->send_queue_size;
    dev_client->send_queue_size = 0;
}

static void spice_char_device_client_free(SpiceCharDeviceState *dev,
                                          SpiceCharDeviceClientState *dev_client)
{
    RingItem *item, *next;

    if (dev_client->wait_for_tokens_timer) {
        core->timer_remove(dev_client->wait_for_tokens_timer);
    }

    spice_char_device_client_send_queue_free(dev, dev_client);

    /* remove write buffers that are associated with the client */
    spice_debug("write_queue_is_empty %d", ring_is_empty(&dev->write_queue) && !dev->cur_write_buf);
    RING_FOREACH_SAFE(item, next, &dev->write_queue) {
        SpiceCharDeviceWriteBuffer *write_buf;

        write_buf = SPICE_CONTAINEROF(item, SpiceCharDeviceWriteBuffer, link);
        if (write_buf->origin == WRITE_BUFFER_ORIGIN_CLIENT &&
            write_buf->client == dev_client->client) {
            ring_remove(item);
            spice_char_device_write_buffer_pool_add(dev, write_buf);
        }
    }

    if (dev->cur_write_buf && dev->cur_write_buf->origin == WRITE_BUFFER_ORIGIN_CLIENT &&
        dev->cur_write_buf->client == dev_client->client) {
        dev->cur_write_buf->origin = WRITE_BUFFER_ORIGIN_NONE;
        dev->cur_write_buf->client = NULL;
    }

    dev->num_clients--;
    ring_remove(&dev_client->link);
    free(dev_client);
}

static void spice_char_device_handle_client_overflow(SpiceCharDeviceClientState *dev_client)
{
    SpiceCharDeviceState *dev = dev_client->dev;
    spice_printerr("dev %p client %p ", dev, dev_client);
    dev->cbs.remove_client(dev_client->client, dev->opaque);
}

static SpiceCharDeviceClientState *spice_char_device_client_find(SpiceCharDeviceState *dev,
                                                                 RedClient *client)
{
    RingItem *item;

    RING_FOREACH(item, &dev->clients) {
        SpiceCharDeviceClientState *dev_client;

        dev_client = SPICE_CONTAINEROF(item, SpiceCharDeviceClientState, link);
        if (dev_client->client == client) {
            return dev_client;
        }
    }
    return NULL;
}

/***************************
 * Reading from the device *
 **************************/

static void device_client_wait_for_tokens_timeout(void *opaque)
{
    SpiceCharDeviceClientState *dev_client = opaque;

    spice_char_device_handle_client_overflow(dev_client);
}

static int spice_char_device_can_send_to_client(SpiceCharDeviceClientState *dev_client)
{
    return !dev_client->do_flow_control || dev_client->num_send_tokens;
}

static uint64_t spice_char_device_max_send_tokens(SpiceCharDeviceState *dev)
{
    RingItem *item;
    uint64_t max = 0;

    RING_FOREACH(item, &dev->clients) {
        SpiceCharDeviceClientState *dev_client;

        dev_client = SPICE_CONTAINEROF(item, SpiceCharDeviceClientState, link);

        if (!dev_client->do_flow_control) {
            max = ~0;
            break;
        }

        if (dev_client->num_send_tokens > max) {
            max = dev_client->num_send_tokens;
        }
    }
    return max;
}

static void spice_char_device_add_msg_to_client_queue(SpiceCharDeviceClientState *dev_client,
                                                      SpiceCharDeviceMsgToClient *msg)
{
    SpiceCharDeviceState *dev = dev_client->dev;
    SpiceCharDeviceMsgToClientItem *msg_item;

    if (dev_client->send_queue_size >= dev_client->max_send_queue_size) {
        spice_char_device_handle_client_overflow(dev_client);
        return;
    }

    msg_item = spice_new0(SpiceCharDeviceMsgToClientItem, 1);
    msg_item->msg = dev->cbs.ref_msg_to_client(msg, dev->opaque);
    ring_add(&dev_client->send_queue, &msg_item->link);
    dev_client->send_queue_size++;
    if (!dev_client->wait_for_tokens_started) {
        core->timer_start(dev_client->wait_for_tokens_timer,
                          SPICE_CHAR_DEVICE_WAIT_TOKENS_TIMEOUT);
        dev_client->wait_for_tokens_started = TRUE;
    }
}

static void spice_char_device_send_msg_to_clients(SpiceCharDeviceState *dev,
                                                  SpiceCharDeviceMsgToClient *msg)
{
    RingItem *item, *next;

    RING_FOREACH_SAFE(item, next, &dev->clients) {
        SpiceCharDeviceClientState *dev_client;

        dev_client = SPICE_CONTAINEROF(item, SpiceCharDeviceClientState, link);
        if (spice_char_device_can_send_to_client(dev_client)) {
            dev_client->num_send_tokens--;
            spice_assert(ring_is_empty(&dev_client->send_queue));
            dev->cbs.send_msg_to_client(msg, dev_client->client, dev->opaque);

            /* don't refer to dev_client anymore, it may have been released */
        } else {
            spice_char_device_add_msg_to_client_queue(dev_client, msg);
        }
    }
}

static int spice_char_device_read_from_device(SpiceCharDeviceState *dev)
{
    uint64_t max_send_tokens;
    int did_read = FALSE;

    if (!dev->running || dev->wait_for_migrate_data) {
        return FALSE;
    }

    /* There are 2 scenarios where we can get called recursively:
     * 1) spice-vmc vmc_read triggering flush of throttled data, recalling wakeup
     * (virtio)
     * 2) in case of sending messages to the client, and unreferencing the
     * msg, we trigger another read.
     */
    if (dev->during_read_from_device++ > 0) {
        return FALSE;
    }

    max_send_tokens = spice_char_device_max_send_tokens(dev);
    spice_char_device_state_ref(dev);
    /*
     * Reading from the device only in case at least one of the clients have a free token.
     * All messages will be discarded if no client is attached to the device
     */
    while ((max_send_tokens || ring_is_empty(&dev->clients)) && dev->running) {
        SpiceCharDeviceMsgToClient *msg;

        msg = dev->cbs.read_one_msg_from_device(dev->sin, dev->opaque);
        if (!msg) {
            if (dev->during_read_from_device > 1) {
                dev->during_read_from_device = 1;
                continue; /* a wakeup might have been called during the read -
                             make sure it doesn't get lost */
            }
            break;
        }
        did_read = TRUE;
        spice_char_device_send_msg_to_clients(dev, msg);
        dev->cbs.unref_msg_to_client(msg, dev->opaque);
        max_send_tokens--;
    }
    dev->during_read_from_device = 0;
    if (dev->running) {
        dev->active = dev->active || did_read;
    }
    spice_char_device_state_unref(dev);
    return did_read;
}

static void spice_char_device_client_send_queue_push(SpiceCharDeviceClientState *dev_client)
{
    RingItem *item;
    while ((item = ring_get_tail(&dev_client->send_queue)) &&
           spice_char_device_can_send_to_client(dev_client)) {
        SpiceCharDeviceMsgToClientItem *msg_item;

        msg_item = SPICE_CONTAINEROF(item, SpiceCharDeviceMsgToClientItem, link);
        ring_remove(item);

        dev_client->num_send_tokens--;
        dev_client->dev->cbs.send_msg_to_client(msg_item->msg,
                                           dev_client->client,
                                           dev_client->dev->opaque);
        dev_client->dev->cbs.unref_msg_to_client(msg_item->msg, dev_client->dev->opaque);
        dev_client->send_queue_size--;
        free(msg_item);
    }
}

static void spice_char_device_send_to_client_tokens_absorb(SpiceCharDeviceClientState *dev_client,
                                                           uint32_t tokens)
{
    dev_client->num_send_tokens += tokens;

    if (dev_client->send_queue_size) {
        spice_assert(dev_client->num_send_tokens == tokens);
        spice_char_device_client_send_queue_push(dev_client);
    }

    if (spice_char_device_can_send_to_client(dev_client)) {
        core->timer_cancel(dev_client->wait_for_tokens_timer);
        dev_client->wait_for_tokens_started = FALSE;
        spice_char_device_read_from_device(dev_client->dev);
    } else if (dev_client->send_queue_size) {
        core->timer_start(dev_client->wait_for_tokens_timer,
                          SPICE_CHAR_DEVICE_WAIT_TOKENS_TIMEOUT);
        dev_client->wait_for_tokens_started = TRUE;
    }
}

void spice_char_device_send_to_client_tokens_add(SpiceCharDeviceState *dev,
                                                 RedClient *client,
                                                 uint32_t tokens)
{
    SpiceCharDeviceClientState *dev_client;

    dev_client = spice_char_device_client_find(dev, client);

    if (!dev_client) {
        spice_error("client wasn't found dev %p client %p", dev, client);
        return;
    }
    spice_char_device_send_to_client_tokens_absorb(dev_client, tokens);
}

void spice_char_device_send_to_client_tokens_set(SpiceCharDeviceState *dev,
                                                 RedClient *client,
                                                 uint32_t tokens)
{
    SpiceCharDeviceClientState *dev_client;

    dev_client = spice_char_device_client_find(dev, client);

    if (!dev_client) {
        spice_error("client wasn't found dev %p client %p", dev, client);
        return;
    }

    dev_client->num_send_tokens = 0;
    spice_char_device_send_to_client_tokens_absorb(dev_client, tokens);
}

/**************************
 * Writing to the device  *
***************************/

static void spice_char_device_client_tokens_add(SpiceCharDeviceState *dev,
                                                SpiceCharDeviceClientState *dev_client,
                                                uint32_t num_tokens)
{
    if (!dev_client->do_flow_control) {
        return;
    }
    if (num_tokens > 1) {
        spice_debug("#tokens > 1 (=%u)", num_tokens);
    }
    dev_client->num_client_tokens_free += num_tokens;
    if (dev_client->num_client_tokens_free >= dev->client_tokens_interval) {
        uint32_t tokens = dev_client->num_client_tokens_free;

        dev_client->num_client_tokens += dev_client->num_client_tokens_free;
        dev_client->num_client_tokens_free = 0;
        dev->cbs.send_tokens_to_client(dev_client->client,
                                       tokens,
                                       dev->opaque);
    }
}

static int spice_char_device_write_to_device(SpiceCharDeviceState *dev)
{
    SpiceCharDeviceInterface *sif;
    int total = 0;
    int n;

    if (!dev->running || dev->wait_for_migrate_data) {
        return 0;
    }

    spice_char_device_state_ref(dev);
    core->timer_cancel(dev->write_to_dev_timer);

    sif = SPICE_CONTAINEROF(dev->sin->base.sif, SpiceCharDeviceInterface, base);
    while (dev->running) {
        uint32_t write_len;

        if (!dev->cur_write_buf) {
            RingItem *item = ring_get_tail(&dev->write_queue);
            if (!item) {
                break;
            }
            dev->cur_write_buf = SPICE_CONTAINEROF(item, SpiceCharDeviceWriteBuffer, link);
            dev->cur_write_buf_pos = dev->cur_write_buf->buf;
            ring_remove(item);
        }

        write_len = dev->cur_write_buf->buf + dev->cur_write_buf->buf_used -
                    dev->cur_write_buf_pos;
        n = sif->write(dev->sin, dev->cur_write_buf_pos, write_len);
        if (n <= 0) {
            break;
        }
        total += n;
        write_len -= n;
        if (!write_len) {
            SpiceCharDeviceWriteBuffer *release_buf = dev->cur_write_buf;
            dev->cur_write_buf = NULL;
            spice_char_device_write_buffer_release(dev, release_buf);
            continue;
        }
        dev->cur_write_buf_pos += n;
    }
    /* retry writing as long as the write queue is not empty */
    if (dev->running) {
        if (dev->cur_write_buf) {
            core->timer_start(dev->write_to_dev_timer,
                              CHAR_DEVICE_WRITE_TO_TIMEOUT);
        } else {
            spice_assert(ring_is_empty(&dev->write_queue));
        }
        dev->active = dev->active || total;
    }
    spice_char_device_state_unref(dev);
    return total;
}

static void spice_char_dev_write_retry(void *opaque)
{
    SpiceCharDeviceState *dev = opaque;

    core->timer_cancel(dev->write_to_dev_timer);
    spice_char_device_write_to_device(dev);
}

static SpiceCharDeviceWriteBuffer *__spice_char_device_write_buffer_get(
    SpiceCharDeviceState *dev, RedClient *client,
    int size, int origin, int migrated_data_tokens)
{
    RingItem *item;
    SpiceCharDeviceWriteBuffer *ret;

    if (origin == WRITE_BUFFER_ORIGIN_SERVER && !dev->num_self_tokens) {
        return NULL;
    }

    if ((item = ring_get_tail(&dev->write_bufs_pool))) {
        ret = SPICE_CONTAINEROF(item, SpiceCharDeviceWriteBuffer, link);
        ring_remove(item);
    } else {
        ret = spice_new0(SpiceCharDeviceWriteBuffer, 1);
    }

    spice_assert(!ret->buf_used);

    if (ret->buf_size < size) {
        ret->buf = spice_realloc(ret->buf, size);
        ret->buf_size = size;
    }
    ret->origin = origin;

    if (origin == WRITE_BUFFER_ORIGIN_CLIENT) {
       spice_assert(client);
       SpiceCharDeviceClientState *dev_client = spice_char_device_client_find(dev, client);
       if (dev_client) {
            if (!migrated_data_tokens &&
                dev_client->do_flow_control && !dev_client->num_client_tokens) {
                spice_printerr("token violation: dev %p client %p", dev, client);
                spice_char_device_handle_client_overflow(dev_client);
                goto error;
            }
            ret->client = client;
            if (!migrated_data_tokens && dev_client->do_flow_control) {
                dev_client->num_client_tokens--;
            }
        } else {
            /* it is possible that the client was removed due to send tokens underflow, but
             * the caller still receive messages from the client */
            spice_printerr("client not found: dev %p client %p", dev, client);
            goto error;
        }
    } else if (origin == WRITE_BUFFER_ORIGIN_SERVER) {
        dev->num_self_tokens--;
    }

    ret->token_price = migrated_data_tokens ? migrated_data_tokens : 1;
    ret->refs = 1;
    return ret;
error:
    ring_add(&dev->write_bufs_pool, &ret->link);
    return NULL;
}

SpiceCharDeviceWriteBuffer *spice_char_device_write_buffer_get(SpiceCharDeviceState *dev,
                                                               RedClient *client,
                                                               int size)
{
   return  __spice_char_device_write_buffer_get(dev, client, size,
             client ? WRITE_BUFFER_ORIGIN_CLIENT : WRITE_BUFFER_ORIGIN_SERVER,
             0);
}

SpiceCharDeviceWriteBuffer *spice_char_device_write_buffer_get_server_no_token(
    SpiceCharDeviceState *dev, int size)
{
   return  __spice_char_device_write_buffer_get(dev, NULL, size,
             WRITE_BUFFER_ORIGIN_SERVER_NO_TOKEN, 0);
}

static SpiceCharDeviceWriteBuffer *spice_char_device_write_buffer_ref(SpiceCharDeviceWriteBuffer *write_buf)
{
    spice_assert(write_buf);

    write_buf->refs++;
    return write_buf;
}

void spice_char_device_write_buffer_add(SpiceCharDeviceState *dev,
                                        SpiceCharDeviceWriteBuffer *write_buf)
{
    spice_assert(dev);
    /* caller shouldn't add buffers for client that was removed */
    if (write_buf->origin == WRITE_BUFFER_ORIGIN_CLIENT &&
        !spice_char_device_client_find(dev, write_buf->client)) {
        spice_printerr("client not found: dev %p client %p", dev, write_buf->client);
        spice_char_device_write_buffer_pool_add(dev, write_buf);
        return;
    }

    ring_add(&dev->write_queue, &write_buf->link);
    spice_char_device_write_to_device(dev);
}

void spice_char_device_write_buffer_release(SpiceCharDeviceState *dev,
                                            SpiceCharDeviceWriteBuffer *write_buf)
{
    int buf_origin = write_buf->origin;
    uint32_t buf_token_price = write_buf->token_price;
    RedClient *client = write_buf->client;

    spice_assert(!ring_item_is_linked(&write_buf->link));
    if (!dev) {
        spice_printerr("no device. write buffer is freed");
        free(write_buf->buf);
        free(write_buf);
        return;
    }

    spice_assert(dev->cur_write_buf != write_buf);

    spice_char_device_write_buffer_pool_add(dev, write_buf);
    if (buf_origin == WRITE_BUFFER_ORIGIN_CLIENT) {
        SpiceCharDeviceClientState *dev_client;

        spice_assert(client);
        dev_client = spice_char_device_client_find(dev, client);
        /* when a client is removed, we remove all the buffers that are associated with it */
        spice_assert(dev_client);
        spice_char_device_client_tokens_add(dev, dev_client, buf_token_price);
    } else if (buf_origin == WRITE_BUFFER_ORIGIN_SERVER) {
        dev->num_self_tokens++;
        if (dev->cbs.on_free_self_token) {
            dev->cbs.on_free_self_token(dev->opaque);
        }
    }
}

/********************************
 * char_device_state management *
 ********************************/

SpiceCharDeviceState *spice_char_device_state_create(SpiceCharDeviceInstance *sin,
                                                     uint32_t client_tokens_interval,
                                                     uint32_t self_tokens,
                                                     SpiceCharDeviceCallbacks *cbs,
                                                     void *opaque)
{
    SpiceCharDeviceState *char_dev;

    spice_assert(sin);
    spice_assert(cbs->read_one_msg_from_device && cbs->ref_msg_to_client &&
                 cbs->unref_msg_to_client && cbs->send_msg_to_client &&
                 cbs->send_tokens_to_client && cbs->remove_client);

    char_dev = spice_new0(SpiceCharDeviceState, 1);
    char_dev->sin = sin;
    char_dev->cbs = *cbs;
    char_dev->opaque = opaque;
    char_dev->client_tokens_interval = client_tokens_interval;
    char_dev->num_self_tokens = self_tokens;

    ring_init(&char_dev->write_queue);
    ring_init(&char_dev->write_bufs_pool);
    ring_init(&char_dev->clients);

    char_dev->write_to_dev_timer = core->timer_add(spice_char_dev_write_retry, char_dev);
    if (!char_dev->write_to_dev_timer) {
        spice_error("failed creating char dev write timer");
    }
    char_dev->refs = 1;
    sin->st = char_dev;
    spice_debug("sin %p dev_state %p", sin, char_dev);
    return char_dev;
}

void spice_char_device_state_reset_dev_instance(SpiceCharDeviceState *state,
                                                SpiceCharDeviceInstance *sin)
{
    spice_debug("sin %p dev_state %p", sin, state);
    state->sin = sin;
    sin->st = state;
}

void *spice_char_device_state_opaque_get(SpiceCharDeviceState *dev)
{
    return dev->opaque;
}

static void spice_char_device_state_ref(SpiceCharDeviceState *char_dev)
{
    char_dev->refs++;
}

static void spice_char_device_state_unref(SpiceCharDeviceState *char_dev)
{
    /* The refs field protects the char_dev from being deallocated in
     * case spice_char_device_state_destroy has been called
     * during a callabck, and we might still access the char_dev afterwards.
     * spice_char_device_state_unref is always coupled with a preceding
     * spice_char_device_state_ref. Here, refs can turn 0
     * only when spice_char_device_state_destroy is called in between
     * the calls to spice_char_device_state_ref and spice_char_device_state_unref.*/
    if (!--char_dev->refs) {
        free(char_dev);
    }
}

void spice_char_device_state_destroy(SpiceCharDeviceState *char_dev)
{
    reds_on_char_device_state_destroy(char_dev);
    core->timer_remove(char_dev->write_to_dev_timer);
    write_buffers_queue_free(&char_dev->write_queue);
    write_buffers_queue_free(&char_dev->write_bufs_pool);
    if (char_dev->cur_write_buf) {
        spice_char_device_write_buffer_free(char_dev->cur_write_buf);
    }

    while (!ring_is_empty(&char_dev->clients)) {
        RingItem *item = ring_get_tail(&char_dev->clients);
        SpiceCharDeviceClientState *dev_client;

        dev_client = SPICE_CONTAINEROF(item, SpiceCharDeviceClientState, link);
        spice_char_device_client_free(char_dev, dev_client);
    }
    char_dev->running = FALSE;

    spice_char_device_state_unref(char_dev);
}

int spice_char_device_client_add(SpiceCharDeviceState *dev,
                                 RedClient *client,
                                 int do_flow_control,
                                 uint32_t max_send_queue_size,
                                 uint32_t num_client_tokens,
                                 uint32_t num_send_tokens,
                                 int wait_for_migrate_data)
{
    SpiceCharDeviceClientState *dev_client;

    spice_assert(dev);
    spice_assert(client);

    if (wait_for_migrate_data && (dev->num_clients > 0 || dev->active)) {
        spice_warning("can't restore device %p from migration data. The device "
                      "has already been active", dev);
        return FALSE;
    }

    dev->wait_for_migrate_data = wait_for_migrate_data;

    spice_debug("dev_state %p client %p", dev, client);
    dev_client = spice_new0(SpiceCharDeviceClientState, 1);
    dev_client->dev = dev;
    dev_client->client = client;
    ring_init(&dev_client->send_queue);
    dev_client->send_queue_size = 0;
    dev_client->max_send_queue_size = max_send_queue_size;
    dev_client->do_flow_control = do_flow_control;
    if (do_flow_control) {
        dev_client->wait_for_tokens_timer = core->timer_add(device_client_wait_for_tokens_timeout,
                                                            dev_client);
        if (!dev_client->wait_for_tokens_timer) {
            spice_error("failed to create wait for tokens timer");
        }
        dev_client->num_client_tokens = num_client_tokens;
        dev_client->num_send_tokens = num_send_tokens;
    } else {
        dev_client->num_client_tokens = ~0;
        dev_client->num_send_tokens = ~0;
    }
    ring_add(&dev->clients, &dev_client->link);
    dev->num_clients++;
    /* Now that we have a client, forward any pending device data */
    spice_char_device_wakeup(dev);
    return TRUE;
}

void spice_char_device_client_remove(SpiceCharDeviceState *dev,
                                     RedClient *client)
{
    SpiceCharDeviceClientState *dev_client;

    spice_debug("dev_state %p client %p", dev, client);
    dev_client = spice_char_device_client_find(dev, client);

    if (!dev_client) {
        spice_error("client wasn't found");
        return;
    }
    spice_char_device_client_free(dev, dev_client);
    if (dev->wait_for_migrate_data) {
        spice_assert(dev->num_clients == 0);
        dev->wait_for_migrate_data  = FALSE;
        spice_char_device_read_from_device(dev);
    }
}

int spice_char_device_client_exists(SpiceCharDeviceState *dev,
                                    RedClient *client)
{
    return (spice_char_device_client_find(dev, client) != NULL);
}

void spice_char_device_start(SpiceCharDeviceState *dev)
{
    spice_debug("dev_state %p", dev);
    dev->running = TRUE;
    spice_char_device_state_ref(dev);
    while (spice_char_device_write_to_device(dev) ||
           spice_char_device_read_from_device(dev));
    spice_char_device_state_unref(dev);
}

void spice_char_device_stop(SpiceCharDeviceState *dev)
{
    spice_debug("dev_state %p", dev);
    dev->running = FALSE;
    dev->active = FALSE;
    core->timer_cancel(dev->write_to_dev_timer);
}

void spice_char_device_reset(SpiceCharDeviceState *dev)
{
    RingItem *client_item;

    spice_char_device_stop(dev);
    dev->wait_for_migrate_data = FALSE;
    spice_debug("dev_state %p", dev);
    while (!ring_is_empty(&dev->write_queue)) {
        RingItem *item = ring_get_tail(&dev->write_queue);
        SpiceCharDeviceWriteBuffer *buf;

        ring_remove(item);
        buf = SPICE_CONTAINEROF(item, SpiceCharDeviceWriteBuffer, link);
        /* tracking the tokens */
        spice_char_device_write_buffer_release(dev, buf);
    }
    if (dev->cur_write_buf) {
        SpiceCharDeviceWriteBuffer *release_buf = dev->cur_write_buf;

        dev->cur_write_buf = NULL;
        spice_char_device_write_buffer_release(dev, release_buf);
    }

    RING_FOREACH(client_item, &dev->clients) {
        SpiceCharDeviceClientState *dev_client;

        dev_client = SPICE_CONTAINEROF(client_item, SpiceCharDeviceClientState, link);
        spice_char_device_client_send_queue_free(dev, dev_client);
    }
    dev->sin = NULL;
}

void spice_char_device_wakeup(SpiceCharDeviceState *dev)
{
    spice_char_device_read_from_device(dev);
}

/*************
 * Migration *
 * **********/

void spice_char_device_state_migrate_data_marshall_empty(SpiceMarshaller *m)
{
    SpiceMigrateDataCharDevice *mig_data;

    spice_debug(NULL);
    mig_data = (SpiceMigrateDataCharDevice *)spice_marshaller_reserve_space(m,
                                                                            sizeof(*mig_data));
    memset(mig_data, 0, sizeof(*mig_data));
    mig_data->version = SPICE_MIGRATE_DATA_CHAR_DEVICE_VERSION;
    mig_data->connected = FALSE;
}

static void migrate_data_marshaller_write_buffer_free(uint8_t *data, void *opaque)
{
    SpiceCharDeviceWriteBuffer *write_buf = (SpiceCharDeviceWriteBuffer *)opaque;

    spice_char_device_write_buffer_free(write_buf);
}

void spice_char_device_state_migrate_data_marshall(SpiceCharDeviceState *dev,
                                                   SpiceMarshaller *m)
{
    SpiceCharDeviceClientState *client_state;
    RingItem *item;
    uint32_t *write_to_dev_size_ptr;
    uint32_t *write_to_dev_tokens_ptr;
    SpiceMarshaller *m2;

    /* multi-clients are not supported */
    spice_assert(dev->num_clients == 1);
    client_state = SPICE_CONTAINEROF(ring_get_tail(&dev->clients),
                                     SpiceCharDeviceClientState,
                                     link);
    /* FIXME: if there were more than one client before the marshalling,
     * it is possible that the send_queue_size > 0, and the send data
     * should be migrated as well */
    spice_assert(client_state->send_queue_size == 0);
    spice_marshaller_add_uint32(m, SPICE_MIGRATE_DATA_CHAR_DEVICE_VERSION);
    spice_marshaller_add_uint8(m, 1); /* connected */
    spice_marshaller_add_uint32(m, client_state->num_client_tokens);
    spice_marshaller_add_uint32(m, client_state->num_send_tokens);
    write_to_dev_size_ptr = (uint32_t *)spice_marshaller_reserve_space(m, sizeof(uint32_t));
    write_to_dev_tokens_ptr = (uint32_t *)spice_marshaller_reserve_space(m, sizeof(uint32_t));
    *write_to_dev_size_ptr = 0;
    *write_to_dev_tokens_ptr = 0;

    m2 = spice_marshaller_get_ptr_submarshaller(m, 0);
    if (dev->cur_write_buf) {
        uint32_t buf_remaining = dev->cur_write_buf->buf + dev->cur_write_buf->buf_used -
                                 dev->cur_write_buf_pos;
        spice_marshaller_add_ref_full(m2, dev->cur_write_buf_pos, buf_remaining,
                                      migrate_data_marshaller_write_buffer_free,
                                      spice_char_device_write_buffer_ref(dev->cur_write_buf)
                                      );
        *write_to_dev_size_ptr += buf_remaining;
        if (dev->cur_write_buf->origin == WRITE_BUFFER_ORIGIN_CLIENT) {
            spice_assert(dev->cur_write_buf->client == client_state->client);
            (*write_to_dev_tokens_ptr) += dev->cur_write_buf->token_price;
        }
    }

    RING_FOREACH_REVERSED(item, &dev->write_queue) {
        SpiceCharDeviceWriteBuffer *write_buf;

        write_buf = SPICE_CONTAINEROF(item, SpiceCharDeviceWriteBuffer, link);
        spice_marshaller_add_ref_full(m2, write_buf->buf, write_buf->buf_used,
                                      migrate_data_marshaller_write_buffer_free,
                                      spice_char_device_write_buffer_ref(write_buf)
                                      );
        *write_to_dev_size_ptr += write_buf->buf_used;
        if (write_buf->origin == WRITE_BUFFER_ORIGIN_CLIENT) {
            spice_assert(write_buf->client == client_state->client);
            (*write_to_dev_tokens_ptr) += write_buf->token_price;
        }
    }
    spice_debug("migration data dev %p: write_queue size %u tokens %u",
                dev, *write_to_dev_size_ptr, *write_to_dev_tokens_ptr);
}

int spice_char_device_state_restore(SpiceCharDeviceState *dev,
                                    SpiceMigrateDataCharDevice *mig_data)
{
    SpiceCharDeviceClientState *client_state;
    uint32_t client_tokens_window;

    spice_assert(dev->num_clients == 1 && dev->wait_for_migrate_data);

    client_state = SPICE_CONTAINEROF(ring_get_tail(&dev->clients),
                                     SpiceCharDeviceClientState,
                                     link);
    if (mig_data->version > SPICE_MIGRATE_DATA_CHAR_DEVICE_VERSION) {
        spice_error("dev %p error: migration data version %u is bigger than self %u",
                    dev, mig_data->version, SPICE_MIGRATE_DATA_CHAR_DEVICE_VERSION);
        return FALSE;
    }
    spice_assert(!dev->cur_write_buf && ring_is_empty(&dev->write_queue));
    spice_assert(mig_data->connected);

    client_tokens_window = client_state->num_client_tokens; /* initial state of tokens */
    client_state->num_client_tokens = mig_data->num_client_tokens;
    /* assumption: client_tokens_window stays the same across severs */
    client_state->num_client_tokens_free = client_tokens_window -
                                           mig_data->num_client_tokens -
                                           mig_data->write_num_client_tokens;
    client_state->num_send_tokens = mig_data->num_send_tokens;

    if (mig_data->write_size > 0) {
        if (mig_data->write_num_client_tokens) {
            dev->cur_write_buf =
                __spice_char_device_write_buffer_get(dev, client_state->client,
                    mig_data->write_size, WRITE_BUFFER_ORIGIN_CLIENT,
                    mig_data->write_num_client_tokens);
        } else {
            dev->cur_write_buf =
                __spice_char_device_write_buffer_get(dev, NULL,
                    mig_data->write_size, WRITE_BUFFER_ORIGIN_SERVER, 0);
        }
        /* the first write buffer contains all the data that was saved for migration */
        memcpy(dev->cur_write_buf->buf,
               ((uint8_t *)mig_data) + mig_data->write_data_ptr - sizeof(SpiceMigrateDataHeader),
               mig_data->write_size);
        dev->cur_write_buf->buf_used = mig_data->write_size;
        dev->cur_write_buf_pos = dev->cur_write_buf->buf;
    }
    dev->wait_for_migrate_data = FALSE;
    spice_char_device_write_to_device(dev);
    spice_char_device_read_from_device(dev);
    return TRUE;
}
