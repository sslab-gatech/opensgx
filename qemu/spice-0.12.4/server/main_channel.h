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

#ifndef __MAIN_CHANNEL_H__
#define __MAIN_CHANNEL_H__

#include <stdint.h>
#include <spice/vd_agent.h>
#include "common/marshaller.h"
#include "reds.h"
#include "red_channel.h"

// TODO: Defines used to calculate receive buffer size, and also by reds.c
// other options: is to make a reds_main_consts.h, to duplicate defines.
#define REDS_AGENT_WINDOW_SIZE 10
#define REDS_NUM_INTERNAL_AGENT_MESSAGES 1

// approximate max receive message size for main channel
#define RECEIVE_BUF_SIZE \
    (4096 + (REDS_AGENT_WINDOW_SIZE + REDS_NUM_INTERNAL_AGENT_MESSAGES) * SPICE_AGENT_MAX_DATA_SIZE)

typedef struct MainChannel {
    RedChannel base;
    uint8_t recv_buf[RECEIVE_BUF_SIZE];
    RedsMigSpice mig_target; // TODO: add refs and release (afrer all clients completed migration in one way or the other?)
    int num_clients_mig_wait;
} MainChannel;


MainChannel *main_channel_init(void);
RedClient *main_channel_get_client_by_link_id(MainChannel *main_chan, uint32_t link_id);
/* This is a 'clone' from the reds.h Channel.link callback to allow passing link_id */
MainChannelClient *main_channel_link(MainChannel *, RedClient *client,
     RedsStream *stream, uint32_t link_id, int migration, int num_common_caps,
     uint32_t *common_caps, int num_caps, uint32_t *caps);
void main_channel_close(MainChannel *main_chan); // not destroy, just socket close
void main_channel_push_mouse_mode(MainChannel *main_chan, int current_mode, int is_client_mouse_allowed);
void main_channel_push_agent_connected(MainChannel *main_chan);
void main_channel_push_agent_disconnected(MainChannel *main_chan);
void main_channel_client_push_agent_tokens(MainChannelClient *mcc, uint32_t num_tokens);
void main_channel_client_push_agent_data(MainChannelClient *mcc, uint8_t* data, size_t len,
                                         spice_marshaller_item_free_func free_data, void *opaque);
void main_channel_client_start_net_test(MainChannelClient *mcc);
// TODO: huge. Consider making a reds_* interface for these functions
// and calling from main.
void main_channel_push_init(MainChannelClient *mcc, int display_channels_hint,
    int current_mouse_mode, int is_client_mouse_allowed, int multi_media_time,
    int ram_hint);
void main_channel_push_notify(MainChannel *main_chan, const char *msg);
void main_channel_client_push_notify(MainChannelClient *mcc, const char *msg);
void main_channel_push_multi_media_time(MainChannel *main_chan, int time);
int main_channel_getsockname(MainChannel *main_chan, struct sockaddr *sa, socklen_t *salen);
int main_channel_getpeername(MainChannel *main_chan, struct sockaddr *sa, socklen_t *salen);
uint32_t main_channel_client_get_link_id(MainChannelClient *mcc);

/*
 * return TRUE if network test had been completed successfully.
 * If FALSE, bitrate_per_sec is set to MAX_UINT64 and the roundtrip is set to 0
 */
int main_channel_client_is_network_info_initialized(MainChannelClient *mcc);
int main_channel_client_is_low_bandwidth(MainChannelClient *mcc);
uint64_t main_channel_client_get_bitrate_per_sec(MainChannelClient *mcc);
uint64_t main_channel_client_get_roundtrip_ms(MainChannelClient *mcc);

int main_channel_is_connected(MainChannel *main_chan);
RedChannelClient* main_channel_client_get_base(MainChannelClient* mcc);

/* switch host migration */
void main_channel_migrate_switch(MainChannel *main_chan, RedsMigSpice *mig_target);

/* semi seamless migration */

/* returns the number of clients that we are waiting for their connection.
 * try_seamless = 'true' when the seamless-migration=on in qemu command line */
int main_channel_migrate_connect(MainChannel *main_channel, RedsMigSpice *mig_target,
                                 int try_seamless);
void main_channel_migrate_cancel_wait(MainChannel *main_chan);
/* returns the number of clients for which SPICE_MSG_MAIN_MIGRATE_END was sent*/
int main_channel_migrate_src_complete(MainChannel *main_chan, int success);
void main_channel_migrate_dst_complete(MainChannelClient *mcc);
void main_channel_push_name(MainChannelClient *mcc, const char *name);
void main_channel_push_uuid(MainChannelClient *mcc, const uint8_t uuid[16]);

#endif
