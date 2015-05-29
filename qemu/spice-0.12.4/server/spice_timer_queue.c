/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2013 Red Hat, Inc.

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
#include <config.h>
#include <pthread.h>
#include "red_common.h"
#include "spice_timer_queue.h"
#include "common/ring.h"

static Ring timer_queue_list;
static int queue_count = 0;
static pthread_mutex_t queue_list_lock = PTHREAD_MUTEX_INITIALIZER;

static void spice_timer_queue_init(void)
{
    ring_init(&timer_queue_list);
}

struct SpiceTimer {
    RingItem link;
    RingItem active_link;

    SpiceTimerFunc func;
    void *opaque;

    SpiceTimerQueue *queue;

    int is_active;
    uint32_t ms;
    uint64_t expiry_time;
};

struct SpiceTimerQueue {
    RingItem link;
    pthread_t thread;
    Ring timers;
    Ring active_timers;
};

static SpiceTimerQueue *spice_timer_queue_find(void)
{
    pthread_t self = pthread_self();
    RingItem *queue_item;

    RING_FOREACH(queue_item, &timer_queue_list) {
         SpiceTimerQueue *queue = SPICE_CONTAINEROF(queue_item, SpiceTimerQueue, link);

         if (pthread_equal(self, queue->thread) != 0) {
            return queue;
         }
    }

    return NULL;
}

static SpiceTimerQueue *spice_timer_queue_find_with_lock(void)
{
    SpiceTimerQueue *queue;

    pthread_mutex_lock(&queue_list_lock);
    queue = spice_timer_queue_find();
    pthread_mutex_unlock(&queue_list_lock);
    return queue;
}

int spice_timer_queue_create(void)
{
    SpiceTimerQueue *queue;

    pthread_mutex_lock(&queue_list_lock);
    if (queue_count == 0) {
        spice_timer_queue_init();
    }

    if (spice_timer_queue_find() != NULL) {
        spice_printerr("timer queue was already created for the thread");
        return FALSE;
    }

    queue = spice_new0(SpiceTimerQueue, 1);
    queue->thread = pthread_self();
    ring_init(&queue->timers);
    ring_init(&queue->active_timers);

    ring_add(&timer_queue_list, &queue->link);
    queue_count++;

    pthread_mutex_unlock(&queue_list_lock);

    return TRUE;
}

void spice_timer_queue_destroy(void)
{
    RingItem *item;
    SpiceTimerQueue *queue;

    pthread_mutex_lock(&queue_list_lock);
    queue = spice_timer_queue_find();

    spice_assert(queue != NULL);

    while ((item = ring_get_head(&queue->timers))) {
        SpiceTimer *timer;

        timer = SPICE_CONTAINEROF(item, SpiceTimer, link);
        spice_timer_remove(timer);
    }

    ring_remove(&queue->link);
    free(queue);
    queue_count--;

    pthread_mutex_unlock(&queue_list_lock);
}

SpiceTimer *spice_timer_queue_add(SpiceTimerFunc func, void *opaque)
{
    SpiceTimer *timer = spice_new0(SpiceTimer, 1);
    SpiceTimerQueue *queue = spice_timer_queue_find_with_lock();

    spice_assert(queue != NULL);

    ring_item_init(&timer->link);
    ring_item_init(&timer->active_link);

    timer->opaque = opaque;
    timer->func = func;
    timer->queue = queue;

    ring_add(&queue->timers, &timer->link);

    return timer;
}

static void _spice_timer_set(SpiceTimer *timer, uint32_t ms, uint32_t now)
{
    RingItem *next_item;
    SpiceTimerQueue *queue;

    if (timer->is_active) {
        spice_timer_cancel(timer);
    }

    queue = timer->queue;
    timer->expiry_time = now + ms;
    timer->ms = ms;

    RING_FOREACH(next_item, &queue->active_timers) {
        SpiceTimer *next_timer = SPICE_CONTAINEROF(next_item, SpiceTimer, active_link);

        if (timer->expiry_time <= next_timer->expiry_time) {
            break;
        }
    }

    if (next_item) {
        ring_add_before(&timer->active_link, next_item);
    } else {
        ring_add_before(&timer->active_link, &queue->active_timers);
    }
    timer->is_active = TRUE;
}

void spice_timer_set(SpiceTimer *timer, uint32_t ms)
{
    struct timespec now;

    spice_assert(pthread_equal(timer->queue->thread, pthread_self()) != 0);

    clock_gettime(CLOCK_MONOTONIC, &now);
    _spice_timer_set(timer, ms, now.tv_sec * 1000 + (now.tv_nsec / 1000 / 1000));
}

void spice_timer_cancel(SpiceTimer *timer)
{
    spice_assert(pthread_equal(timer->queue->thread, pthread_self()) != 0);

    if (!ring_item_is_linked(&timer->active_link)) {
        spice_assert(!timer->is_active);
        return;
    }

    spice_assert(timer->is_active);
    ring_remove(&timer->active_link);
    timer->is_active = FALSE;
}

void spice_timer_remove(SpiceTimer *timer)
{
    spice_assert(timer->queue);
    spice_assert(ring_item_is_linked(&timer->link));
    spice_assert(pthread_equal(timer->queue->thread, pthread_self()) != 0);

    if (timer->is_active) {
        spice_assert(ring_item_is_linked(&timer->active_link));
        ring_remove(&timer->active_link);
    }
    ring_remove(&timer->link);
    free(timer);
}

unsigned int spice_timer_queue_get_timeout_ms(void)
{
    struct timespec now;
    int now_ms;
    RingItem *head;
    SpiceTimer *head_timer;
    SpiceTimerQueue *queue = spice_timer_queue_find_with_lock();

    spice_assert(queue != NULL);

    if (ring_is_empty(&queue->active_timers)) {
        return -1;
    }

    head = ring_get_head(&queue->active_timers);
    head_timer = SPICE_CONTAINEROF(head, SpiceTimer, active_link);

    clock_gettime(CLOCK_MONOTONIC, &now);
    now_ms = (now.tv_sec * 1000) - (now.tv_nsec / 1000 / 1000);

    return MAX(0, ((int)head_timer->expiry_time - now_ms));
}


void spice_timer_queue_cb(void)
{
    struct timespec now;
    uint64_t now_ms;
    RingItem *head;
    SpiceTimerQueue *queue = spice_timer_queue_find_with_lock();

    spice_assert(queue != NULL);

    if (ring_is_empty(&queue->active_timers)) {
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &now);
    now_ms = (now.tv_sec * 1000) + (now.tv_nsec / 1000 / 1000);

    while ((head = ring_get_head(&queue->active_timers))) {
        SpiceTimer *timer = SPICE_CONTAINEROF(head, SpiceTimer, active_link);

        if (timer->expiry_time > now_ms) {
            break;
        } else {
            timer->func(timer->opaque);
            if (timer->is_active) {
                _spice_timer_set(timer, timer->ms, now_ms);
            }
        }
    }
}
