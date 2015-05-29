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

#ifndef _H_SPICE_TIMER_QUEUE
#define _H_SPICE_TIMER_QUEUE

#include  <stdint.h>
#include "spice.h"

typedef struct SpiceTimerQueue SpiceTimerQueue;

/* create/destroy a timer queue for the current thread.
 * In order to execute the timers functions, spice_timer_queue_cb should be called
 * periodically, according to spice_timer_queue_get_timeout_ms */
int spice_timer_queue_create(void);
void spice_timer_queue_destroy(void);

SpiceTimer *spice_timer_queue_add(SpiceTimerFunc func, void *opaque);
void spice_timer_set(SpiceTimer *timer, uint32_t ms);
void spice_timer_cancel(SpiceTimer *timer);
void spice_timer_remove(SpiceTimer *timer);

/* returns the time left till the earliest timer in the queue expires.
 * returns (unsigned)-1 if there are no active timers */
unsigned int spice_timer_queue_get_timeout_ms(void);
/* call the timeout callbacks of all the expired timers */
void spice_timer_queue_cb(void);

#endif
