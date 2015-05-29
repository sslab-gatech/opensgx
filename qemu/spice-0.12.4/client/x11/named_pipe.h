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

#ifndef _H_NAMED_PIPE
#define _H_NAMED_PIPE

#include "platform.h"
#include "x_platform.h"
#include "process_loop.h"

class Session: public EventSources::Socket {
public:
    Session(int fd, ProcessLoop& events_loop);
    virtual ~Session();
    void bind(NamedPipe::ConnectionInterface* conn_interface);

public:
    virtual void on_event();
    virtual int get_socket() {return _fd_client;}
    int32_t write(const uint8_t *buf, int32_t size);
    int32_t read(uint8_t *buf, int32_t size);

private:
    NamedPipe::ConnectionInterface *_conn_interface;
    int _fd_client;
    ProcessLoop &_events_loop;
};

class LinuxListener: public EventSources::Socket {
public:
    LinuxListener(const char *name, NamedPipe::ListenerInterface &listener_interface,
                  ProcessLoop& events_loop);
    virtual ~LinuxListener();
    void on_event();
    virtual int get_socket() {return _listen_socket;}

private:
    int create_socket(const char *socket_name);

private:
    NamedPipe::ListenerInterface &_listener_interface;
    int _listen_socket;
    std::string _name;
    ProcessLoop &_events_loop;
};

#endif
