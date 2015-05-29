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

#if defined(CLIENT_PIXMAPS_CACHE)

#define CACHE PixmapCache

#define CACHE_NAME bits_cache
#define CACHE_HASH_KEY BITS_CACHE_HASH_KEY
#define CACHE_HASH_SIZE BITS_CACHE_HASH_SIZE
#define PIPE_ITEM_TYPE PIPE_ITEM_TYPE_INVAL_PIXMAP
#define FUNC_NAME(name) pixmap_cache_##name
#define PRIVATE_FUNC_NAME(name) __pixmap_cache_##name
#define CHANNEL DisplayChannel
#define CACH_GENERATION pixmap_cache_generation
#define INVAL_ALL_VERB SPICE_MSG_DISPLAY_INVAL_ALL_PIXMAPS
#else

#error "no cache type."

#endif

#define CHANNEL_FROM_RCC(rcc) SPICE_CONTAINEROF((rcc)->channel, CHANNEL, common.base);

static int FUNC_NAME(hit)(CACHE *cache, uint64_t id, int *lossy, DisplayChannelClient *dcc)
{
    NewCacheItem *item;
    uint64_t serial;

    serial = red_channel_client_get_message_serial(&dcc->common.base);
    pthread_mutex_lock(&cache->lock);
    item = cache->hash_table[CACHE_HASH_KEY(id)];

    while (item) {
        if (item->id == id) {
            ring_remove(&item->lru_link);
            ring_add(&cache->lru, &item->lru_link);
            spice_assert(dcc->common.id < MAX_CACHE_CLIENTS);
            item->sync[dcc->common.id] = serial;
            cache->sync[dcc->common.id] = serial;
            *lossy = item->lossy;
            break;
        }
        item = item->next;
    }
    pthread_mutex_unlock(&cache->lock);

    return !!item;
}

static int FUNC_NAME(set_lossy)(CACHE *cache, uint64_t id, int lossy)
{
    NewCacheItem *item;
    pthread_mutex_lock(&cache->lock);

    item = cache->hash_table[CACHE_HASH_KEY(id)];

    while (item) {
        if (item->id == id) {
            item->lossy = lossy;
            break;
        }
        item = item->next;
    }
    pthread_mutex_unlock(&cache->lock);
    return !!item;
}

static int FUNC_NAME(add)(CACHE *cache, uint64_t id, uint32_t size, int lossy, DisplayChannelClient *dcc)
{
    NewCacheItem *item;
    uint64_t serial;
    int key;

    spice_assert(size > 0);

    item = spice_new(NewCacheItem, 1);
    serial = red_channel_client_get_message_serial(&dcc->common.base);

    pthread_mutex_lock(&cache->lock);

    if (cache->generation != dcc->CACH_GENERATION) {
        if (!dcc->pending_pixmaps_sync) {
            red_channel_client_pipe_add_type(
                &dcc->common.base, PIPE_ITEM_TYPE_PIXMAP_SYNC);
            dcc->pending_pixmaps_sync = TRUE;
        }
        pthread_mutex_unlock(&cache->lock);
        free(item);
        return FALSE;
    }

    cache->available -= size;
    while (cache->available < 0) {
        NewCacheItem *tail;
        NewCacheItem **now;

        if (!(tail = (NewCacheItem *)ring_get_tail(&cache->lru)) ||
                                                   tail->sync[dcc->common.id] == serial) {
            cache->available += size;
            pthread_mutex_unlock(&cache->lock);
            free(item);
            return FALSE;
        }

        now = &cache->hash_table[CACHE_HASH_KEY(tail->id)];
        for (;;) {
            spice_assert(*now);
            if (*now == tail) {
                *now = tail->next;
                break;
            }
            now = &(*now)->next;
        }
        ring_remove(&tail->lru_link);
        cache->items--;
        cache->available += tail->size;
        cache->sync[dcc->common.id] = serial;
        display_channel_push_release(dcc, SPICE_RES_TYPE_PIXMAP, tail->id, tail->sync);
        free(tail);
    }
    ++cache->items;
    item->next = cache->hash_table[(key = CACHE_HASH_KEY(id))];
    cache->hash_table[key] = item;
    ring_item_init(&item->lru_link);
    ring_add(&cache->lru, &item->lru_link);
    item->id = id;
    item->size = size;
    item->lossy = lossy;
    memset(item->sync, 0, sizeof(item->sync));
    item->sync[dcc->common.id] = serial;
    cache->sync[dcc->common.id] = serial;
    pthread_mutex_unlock(&cache->lock);
    return TRUE;
}

static void PRIVATE_FUNC_NAME(clear)(CACHE *cache)
{
    NewCacheItem *item;

    if (cache->freezed) {
        cache->lru.next = cache->freezed_head;
        cache->lru.prev = cache->freezed_tail;
        cache->freezed = FALSE;
    }

    while ((item = (NewCacheItem *)ring_get_head(&cache->lru))) {
        ring_remove(&item->lru_link);
        free(item);
    }
    memset(cache->hash_table, 0, sizeof(*cache->hash_table) * CACHE_HASH_SIZE);

    cache->available = cache->size;
    cache->items = 0;
}

static void FUNC_NAME(reset)(CACHE *cache, DisplayChannelClient *dcc, SpiceMsgWaitForChannels* sync_data)
{
    uint8_t wait_count;
    uint64_t serial;
    uint32_t i;

    serial = red_channel_client_get_message_serial(&dcc->common.base);
    pthread_mutex_lock(&cache->lock);
    PRIVATE_FUNC_NAME(clear)(cache);

    dcc->CACH_GENERATION = ++cache->generation;
    cache->generation_initiator.client = dcc->common.id;
    cache->generation_initiator.message = serial;
    cache->sync[dcc->common.id] = serial;

    wait_count = 0;
    for (i = 0; i < MAX_CACHE_CLIENTS; i++) {
        if (cache->sync[i] && i != dcc->common.id) {
            sync_data->wait_list[wait_count].channel_type = SPICE_CHANNEL_DISPLAY;
            sync_data->wait_list[wait_count].channel_id = i;
            sync_data->wait_list[wait_count++].message_serial = cache->sync[i];
        }
    }
    sync_data->wait_count = wait_count;
    pthread_mutex_unlock(&cache->lock);
}

static int FUNC_NAME(freeze)(CACHE *cache)
{
    pthread_mutex_lock(&cache->lock);

    if (cache->freezed) {
        pthread_mutex_unlock(&cache->lock);
        return FALSE;
    }

    cache->freezed_head = cache->lru.next;
    cache->freezed_tail = cache->lru.prev;
    ring_init(&cache->lru);
    memset(cache->hash_table, 0, sizeof(*cache->hash_table) * CACHE_HASH_SIZE);
    cache->available = -1;
    cache->freezed = TRUE;

    pthread_mutex_unlock(&cache->lock);
    return TRUE;
}

static void FUNC_NAME(destroy)(CACHE *cache)
{
    spice_assert(cache);

    pthread_mutex_lock(&cache->lock);
    PRIVATE_FUNC_NAME(clear)(cache);
    pthread_mutex_unlock(&cache->lock);
}

#undef CACHE_NAME
#undef CACHE_HASH_KEY
#undef CACHE_HASH_SIZE
#undef CACHE_INVAL_TYPE
#undef CACHE_MAX_CLIENT_SIZE
#undef FUNC_NAME
#undef VAR_NAME
#undef CHANNEL
#undef CHANNEL_FROM_RCC
