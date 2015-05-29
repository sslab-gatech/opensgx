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

#ifndef _INPUTS_CHANNEL_H_
#define _INPUTS_CHANNEL_H_

// Inputs channel, dealing with keyboard, mouse, tablet.
// This include should only be used by reds.c and inputs_channel.c

#include <stdint.h>
#include <spice/vd_agent.h>

void inputs_init(void);
int inputs_inited(void);
int inputs_has_tablet(void);
const VDAgentMouseState *inputs_get_mouse_state(void);
void inputs_on_keyboard_leds_change(void *opaque, uint8_t leds);
int inputs_set_keyboard(SpiceKbdInstance *_keyboard);
int inputs_set_mouse(SpiceMouseInstance *_mouse);
int inputs_set_tablet(SpiceTabletInstance *_tablet);
void inputs_detach_tablet(SpiceTabletInstance *_tablet);
void inputs_set_tablet_logical_size(int x_res, int y_res);

#endif
