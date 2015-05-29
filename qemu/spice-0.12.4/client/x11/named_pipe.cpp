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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "named_pipe.h"
#include "utils.h"
#include "debug.h"

Session::Session(int fd, ProcessLoop& events_loop)
    : _fd_client(fd)
    , _events_loop(events_loop)
{
}

void Session::on_event()
{
    _conn_interface->on_data();
}

void Session::bind(NamedPipe::ConnectionInterface* conn_interface)
{
    _conn_interface = conn_interface;
    _events_loop.add_socket(*this);
}

Session::~Session()
{
    _events_loop.remove_socket(*this);
    close(_fd_client);
}

int32_t Session::write(const uint8_t *buf, int32_t size)
{
    const uint8_t *pos = buf;

    while (size) {
        int now;
        if ((now = send(_fd_client, (char *)pos, size, 0)) == -1) {
            if (errno == EAGAIN) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            DBG(0, "send error errno=%d, %s", errno, strerror(errno));
            return -1;
        }
        size -= now;
        pos += now;
    }
    return (pos - buf);
}

int32_t Session::read(uint8_t *buf, int32_t size)
{
    uint8_t *pos = buf;
    while (size) {
        int now;
        if ((now = recv(_fd_client, (char *)pos, size, 0)) <= 0) {
            if (now == 0) {
                DBG(0, "read error, connection shutdown");
                return -1;
            }
            if (errno == EAGAIN) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            DBG(0, "read error errno=%d, %s", errno, strerror(errno));
            return -1;
        }
        size -= now;
        pos += now;
    }
    return (pos - buf);
}

int LinuxListener::create_socket(const char *socket_name)
{
    int listen_socket;
    struct sockaddr_un local;

    if ((listen_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        DBG(0, "create socket error, errno=%d, %s", errno, strerror(errno));
        return -1;
    }

    _name = socket_name;

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, socket_name);
    unlink(local.sun_path);
    if (bind(listen_socket, (struct sockaddr *)&local,
             strlen(local.sun_path) + sizeof(local.sun_family)) == -1) {
        DBG(0, "bind error, errno=%d, %s", errno, strerror(errno));
        return -1;
    }
    if (listen(listen_socket, 10) == -1) {
        DBG(0, "listen error, errno=%d, %s", errno, strerror(errno));
        return -1;
    }
    return listen_socket;
}

LinuxListener::LinuxListener(const char *name, NamedPipe::ListenerInterface &listener_interface,
                             ProcessLoop& events_loop)
    : _listener_interface (listener_interface)
    , _events_loop (events_loop)
{
    _listen_socket = create_socket(name);
    if (_listen_socket <= 0) {
        THROW("Listener creation failed %d", _listen_socket);
    }
    _events_loop.add_socket(*this);

    DBG(0, "listening socket - %s, added to events_loop", name);
}

LinuxListener::~LinuxListener()
{
    _events_loop.remove_socket(*this);
    close(_listen_socket);
    unlink(_name.c_str());
}

void LinuxListener::on_event()
{
    for (;;) {
        int fd_client;
        Session *conn;
        struct sockaddr_un remote;
        socklen_t len = sizeof(remote);

        if ((fd_client = accept(_listen_socket, (struct sockaddr *)&remote, &len)) == -1) {
            if (errno == EAGAIN) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            THROW("errno=%d, %s", errno, strerror(errno));
        }

        conn = new Session(fd_client, _events_loop);
        DBG(0, "New connection created, fd: %d", fd_client);
        NamedPipe::ConnectionInterface &conn_interface = _listener_interface.create();
        conn->bind(&conn_interface);
        conn_interface.bind((NamedPipe::ConnectionRef)conn);
    }
}
