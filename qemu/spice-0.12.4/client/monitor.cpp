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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "monitor.h"
#include "debug.h"
#include "platform.h"

uint32_t Monitor::self_monitors_change = 0;


Monitor::Monitor(int id)
    : _id (id)
    , _free (true)
{
}

bool Monitor::is_self_change()
{
    return self_monitors_change != 0;
}

void Monitor::set_mode(int width, int height)
{
    do_set_mode(width, height);
    Platform::reset_cursor_pos();
}
void Monitor::restore()
{
    do_restore();
    Platform::reset_cursor_pos();
}
