#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>

#include "spice/macros.h"
#include "common/ring.h"
#include "test_util.h"
#include "basic_event_loop.h"

int debug = 0;

#define DPRINTF(x, format, ...) { \
    if (x <= debug) { \
        printf("%s: " format "\n" , __FUNCTION__, ## __VA_ARGS__); \
    } \
}

#define NOT_IMPLEMENTED printf("%s not implemented\n", __func__);

static SpiceCoreInterface core;

typedef struct SpiceTimer {
    RingItem link;
    SpiceTimerFunc func;
    struct timeval tv_start;
    int ms;
    void *opaque;
} Timer;

Ring timers;

static SpiceTimer* timer_add(SpiceTimerFunc func, void *opaque)
{
    SpiceTimer *timer = calloc(sizeof(SpiceTimer), 1);

    timer->func = func;
    timer->opaque = opaque;
    ring_add(&timers, &timer->link);
    return timer;
}

static void add_ms_to_timeval(struct timeval *tv, int ms)
{
    tv->tv_usec += 1000 * ms;
    while (tv->tv_usec >= 1000000) {
        tv->tv_sec++;
        tv->tv_usec -= 1000000;
    }
}

static void timer_start(SpiceTimer *timer, uint32_t ms)
{
    ASSERT(ms);
    gettimeofday(&timer->tv_start, NULL);
    timer->ms = ms;
    // already add ms to timer value
    add_ms_to_timeval(&timer->tv_start, ms);
}

static void timer_cancel(SpiceTimer *timer)
{
    timer->ms = 0;
}

static void timer_remove(SpiceTimer *timer)
{
    ring_remove(&timer->link);
}

struct SpiceWatch {
    RingItem link;
    int fd;
    int event_mask;
    SpiceWatchFunc func;
    int removed;
    void *opaque;
};

Ring watches;

int watch_count = 0;

static SpiceWatch *watch_add(int fd, int event_mask, SpiceWatchFunc func, void *opaque)
{
    SpiceWatch *watch = malloc(sizeof(SpiceWatch));

    DPRINTF(0, "adding %p, fd=%d at %d", watch,
        fd, watch_count);
    watch->fd = fd;
    watch->event_mask = event_mask;
    watch->func = func;
    watch->opaque = opaque;
    watch->removed = FALSE;
    ring_item_init(&watch->link);
    ring_add(&watches, &watch->link);
    watch_count++;
    return watch;
}

static void watch_update_mask(SpiceWatch *watch, int event_mask)
{
    DPRINTF(0, "fd %d to %d", watch->fd, event_mask);
    watch->event_mask = event_mask;
}

static void watch_remove(SpiceWatch *watch)
{
    DPRINTF(0, "remove %p (fd %d)", watch, watch->fd);
    watch_count--;
    watch->removed = TRUE;
}

static void channel_event(int event, SpiceChannelEventInfo *info)
{
    DPRINTF(0, "channel event con, type, id, event: %ld, %d, %d, %d",
            info->connection_id, info->type, info->id, event);
}

SpiceTimer *get_next_timer(void)
{
    SpiceTimer *next, *min;

    if (ring_is_empty(&timers)) {
        return NULL;
    }
    min = next = (SpiceTimer*)ring_get_head(&timers);
    while ((next=(SpiceTimer*)ring_next(&timers, &next->link)) != NULL) {
        if (next->ms &&
            (next->tv_start.tv_sec < min->tv_start.tv_sec ||
             (next->tv_start.tv_sec == min->tv_start.tv_sec &&
              next->tv_start.tv_usec < min->tv_start.tv_usec))) {
             min = next;
        }
    }
    return min;
}

struct timeval now;

void tv_b_minus_a_return_le_zero(struct timeval *a, struct timeval *b, struct timeval *dest)
{
    dest->tv_usec = b->tv_usec - a->tv_usec;
    dest->tv_sec = b->tv_sec - a->tv_sec;
    while (dest->tv_usec < 0) {
        dest->tv_usec += 1000000;
        dest->tv_sec--;
    }
    if (dest->tv_sec < 0) {
        dest->tv_sec = 0;
        dest->tv_usec = 0;
    }
}

void calc_next_timeout(SpiceTimer *next, struct timeval *timeout)
{
    gettimeofday(&now, NULL);
    tv_b_minus_a_return_le_zero(&now, &next->tv_start, timeout);
}

void timeout_timers(void)
{
    SpiceTimer *next;
    struct timeval left;
    int count = 0;

    next = (SpiceTimer*)ring_get_head(&timers);
    while (next != NULL) {
        tv_b_minus_a_return_le_zero(&now, &next->tv_start, &left);
        if (next->ms && left.tv_usec == 0 && left.tv_sec == 0) {
            count++;
            DPRINTF(2, "calling timer");
            next->ms = 0;
            next->func(next->opaque);
        }
        next = (SpiceTimer*)ring_next(&timers, &next->link);
    }
    DPRINTF(2, "called %d timers", count);
}

void basic_event_loop_mainloop(void)
{
    fd_set rfds, wfds;
    int max_fd = -1;
    int i;
    int retval;
    SpiceWatch *watch;
    SpiceTimer *next_timer;
    RingItem *link;
    RingItem *next;
    struct timeval next_timer_timeout;
    struct timeval *timeout;

    while (1) {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        i = 0;
        RING_FOREACH_SAFE(link, next, &watches) {
            watch = (SpiceWatch*)link;
            if (watch->removed) {
                continue;
            }
            if (watch->event_mask & SPICE_WATCH_EVENT_READ) {
                FD_SET(watch->fd, &rfds);
                max_fd = watch->fd > max_fd ? watch->fd : max_fd;
            }
            if (watch->event_mask & SPICE_WATCH_EVENT_WRITE) {
                FD_SET(watch->fd, &wfds);
                max_fd = watch->fd > max_fd ? watch->fd : max_fd;
            }
            i++;
        }
        if ((next_timer = get_next_timer()) != NULL) {
            calc_next_timeout(next_timer, &next_timer_timeout);
            timeout = &next_timer_timeout;
            DPRINTF(2, "timeout of %zd.%06zd",
                    timeout->tv_sec, timeout->tv_usec);
        } else {
            timeout = NULL;
        }
        DPRINTF(1, "watching %d fds", i);
        retval = select(max_fd + 1, &rfds, &wfds, NULL, timeout);
        if (timeout != NULL) {
            calc_next_timeout(next_timer, &next_timer_timeout);
            if (next_timer_timeout.tv_sec == 0 &&
                next_timer_timeout.tv_usec == 0) {
                timeout_timers();
            }
        }
        if (retval == -1) {
            printf("error in select - exiting\n");
            abort();
        }
        if (retval) {
            RING_FOREACH_SAFE(link, next, &watches) {
                watch = SPICE_CONTAINEROF(link, SpiceWatch, link);
                if (!watch->removed && (watch->event_mask & SPICE_WATCH_EVENT_READ)
                     && FD_ISSET(watch->fd, &rfds)) {
                    watch->func(watch->fd, SPICE_WATCH_EVENT_READ, watch->opaque);
                }
                if (!watch->removed && (watch->event_mask & SPICE_WATCH_EVENT_WRITE)
                     && FD_ISSET(watch->fd, &wfds)) {
                    watch->func(watch->fd, SPICE_WATCH_EVENT_WRITE, watch->opaque);
                }
                if (watch->removed) {
                    printf("freeing watch %p\n", watch);
                    ring_remove(&watch->link);
                    free(watch);
                }
            }
        }
    }
}

static void ignore_sigpipe(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    sigfillset(&act.sa_mask);
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);
}

SpiceCoreInterface *basic_event_loop_init(void)
{
    ring_init(&watches);
    ring_init(&timers);
    memset(&core, 0, sizeof(core));
    core.base.major_version = SPICE_INTERFACE_CORE_MAJOR;
    core.base.minor_version = SPICE_INTERFACE_CORE_MINOR; // anything less then 3 and channel_event isn't called
    core.timer_add = timer_add;
    core.timer_start = timer_start;
    core.timer_cancel = timer_cancel;
    core.timer_remove = timer_remove;
    core.watch_add = watch_add;
    core.watch_update_mask = watch_update_mask;
    core.watch_remove = watch_remove;
    core.channel_event = channel_event;
    ignore_sigpipe();
    return &core;
}
