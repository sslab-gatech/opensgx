#ifndef MAIN_DISPATCHER_H
#define MAIN_DISPATCHER_H

#include <spice.h>
#include "red_channel.h"

void main_dispatcher_channel_event(int event, SpiceChannelEventInfo *info);
void main_dispatcher_seamless_migrate_dst_complete(RedClient *client);
void main_dispatcher_set_mm_time_latency(RedClient *client, uint32_t latency);
void main_dispatcher_init(SpiceCoreInterface *core);

#endif //MAIN_DISPATCHER_H
