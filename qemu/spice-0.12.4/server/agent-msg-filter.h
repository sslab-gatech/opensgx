/*
   Copyright (C) 2011 Red Hat, Inc.

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

   Red Hat Authors:
        hdegoede@redhat.com
*/

#ifndef _H_AGENT_MSG_FILTER
#define _H_AGENT_MSG_FILTER

#include <spice/vd_agent.h>

/* Possible return values for agent_msg_filter_process_data */
enum {
    AGENT_MSG_FILTER_OK,
    AGENT_MSG_FILTER_DISCARD,
    AGENT_MSG_FILTER_PROTO_ERROR,
    AGENT_MSG_FILTER_MONITORS_CONFIG,
    AGENT_MSG_FILTER_END
};

typedef struct AgentMsgFilter {
    int msg_data_to_read;
    int result;
    int copy_paste_enabled;
    int file_xfer_enabled;
    int discard_all;
} AgentMsgFilter;

void agent_msg_filter_init(struct AgentMsgFilter *filter,
                           int copy_paste, int file_xfer, int discard_all);
int agent_msg_filter_process_data(struct AgentMsgFilter *filter,
                                  uint8_t *data, uint32_t len);

#endif
