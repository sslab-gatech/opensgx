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

#ifndef _H_EVENT_SOURCES
#define _H_EVENT_SOURCES

#include "common.h"
#include "event_sources_p.h"

class EventSource;

// TODO: the class is not thread safe
class EventSources: public EventSources_p {
public:
    class Trigger;
    class Socket;
    class File;
    class Handle;

    EventSources();
    virtual ~EventSources();

    void add_trigger(Trigger& trigger);
    void remove_trigger(Trigger& trigger);
    void add_socket(Socket& socket);
    void remove_socket(Socket& socket);
    void add_file(File& file);
    void remove_file(File& file);
    void add_handle(Handle& handle);
    void remove_handle(Handle& handle);

    /* return true if the events loop should quit */
    bool wait_events(int timeout_ms = INFINITE);
};

class EventSource {
public:
    virtual ~EventSource() {}
    virtual void on_event() = 0;

private:
    virtual void action() {on_event();}

    friend class EventSources;
};

class EventSources::Trigger: public EventSource, private Trigger_p {
public:
    Trigger();
    virtual ~Trigger();
    virtual void trigger();
    virtual void reset();

private:
    virtual void action();

    friend class EventSources;
};

class EventSources::Socket: public EventSource {
protected:
    virtual int get_socket() = 0;

    friend class EventSources;
};


class EventSources::File: public EventSource {
protected:
    virtual int get_fd() = 0;

    friend class EventSources;
};

class EventSources::Handle: public EventSource, public Handle_p {

     friend class EventSources;
};

#endif
