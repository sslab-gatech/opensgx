#include <config.h>
#include <stdlib.h>
#include <string.h>

#include <spice.h>

struct SpiceTimer {
    int a,b;
};

SpiceTimer* timer_add(SpiceTimerFunc func, void *opaque)
{
    static struct SpiceTimer t = {0,};

    return &t;
}

void timer_start(SpiceTimer *timer, uint32_t ms)
{
}

void timer_cancel(SpiceTimer *timer)
{
}

void timer_remove(SpiceTimer *timer)
{
}

SpiceWatch *watch_add(int fd, int event_mask, SpiceWatchFunc func, void *opaque)
{
    return NULL;
}

void watch_update_mask(SpiceWatch *watch, int event_mask)
{
}

void watch_remove(SpiceWatch *watch)
{
}

void channel_event(int event, SpiceChannelEventInfo *info)
{
}

int main(void)
{
    SpiceServer *server = spice_server_new();
    SpiceCoreInterface core;

    memset(&core, 0, sizeof(core));
    core.base.major_version = SPICE_INTERFACE_CORE_MAJOR;
    core.timer_add = timer_add;
    core.timer_start = timer_start;
    core.timer_cancel = timer_cancel;
    core.timer_remove = timer_remove;
    core.watch_add = watch_add;
    core.watch_update_mask = watch_update_mask;
    core.watch_remove = watch_remove;
    core.channel_event = channel_event;

    spice_server_set_port(server, 5911);
    spice_server_init(server, &core);

    spice_server_destroy(server);

    return 0;
}
