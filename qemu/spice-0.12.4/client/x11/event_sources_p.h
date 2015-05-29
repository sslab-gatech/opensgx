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

#ifndef _H_EVENT_SOURCES_P
#define _H_EVENT_SOURCES_P

#include "common.h"
#include "threads.h"

#define INFINITE -1

class EventSource;

class EventSources_p {
protected:
    void add_event(int fd, EventSource* source);
    void remove_event(EventSource* source);

public:
    std::vector<EventSource*> _events;
    std::vector<int> _fds;
};

class Trigger_p {
public:
    Trigger_p() : _pending_int (false) {}
    int get_fd() { return _event_fd;}
    bool reset_event();

public:
    int _event_fd;
    int _event_write_fd;
    bool _pending_int;
    Mutex _lock;
};

class Handle_p {
};

#endif
