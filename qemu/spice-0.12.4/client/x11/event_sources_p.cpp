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

#include <sys/select.h>
#include <sys/fcntl.h>

#include "event_sources.h"
#include "debug.h"
#include "utils.h"

static void set_non_blocking(int fd)
{
    int flags;
    if ((flags = ::fcntl(fd, F_GETFL)) == -1) {
        THROW("failed to set socket non block: %s", strerror(errno));
    }

    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        THROW("failed to set socket non block: %s", strerror(errno));
    }
}

static void set_blocking(int fd)
{
    int flags;
    if ((flags = ::fcntl(fd, F_GETFL)) == -1) {
        THROW("failed to clear socket non block: %s", strerror(errno));
    }

    if (::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        THROW("failed to clear socket non block: %s", strerror(errno));
    }
}

EventSources::EventSources()
{
}

EventSources::~EventSources()
{
}

void EventSources_p::add_event(int fd, EventSource* source)
{
    int size = _events.size();
    _events.resize(size + 1);
    _fds.resize(size + 1);
    _events[size] = source;
    _fds[size] = fd;
}

void EventSources_p::remove_event(EventSource* source)
{
    int size = _events.size();
    for (int i = 0; i < size; i++) {
        if (_events[i] == source) {
            for (i++; i < size; i++) {
                _events[i - 1] = _events[i];
                _fds[i - 1] = _fds[i];
            }
            _events.resize(size - 1);
            _fds.resize(size - 1);
            return;
        }
    }
    THROW("event not found");
}

bool EventSources::wait_events(int timeout_msec)
{
    int maxfd = 0;
    fd_set rfds;
    struct timeval tv;
    struct timeval *tvp;
    int ready;

    FD_ZERO(&rfds);

    int size = _events.size();
    for (int i = 0; i < size; i++) {
        maxfd = MAX(maxfd, _fds[i]);
        FD_SET(_fds[i], &rfds);
    }

    if (timeout_msec == INFINITE) {
        tvp = NULL;
    } else {
        tv.tv_sec = timeout_msec / 1000;
        tv.tv_usec = (timeout_msec % 1000) * 1000;
        tvp = &tv;
    }

    /* Right now we only use read polling in spice */
    ready = ::select(maxfd+1, &rfds, NULL, NULL, tvp);

    if (ready == -1) {
        if (errno == EINTR) {
            return false;
        }
        THROW("wait error select failed");
    } else if (ready == 0) {
        return false;
    }

    for (unsigned int i = 0; i < _events.size(); i++) {
        if (FD_ISSET(_fds[i], &rfds)) {
            _events[i]->action();
            /* The action may have removed / added event sources changing
               our array, so leave the loop and handle other events the next
               time we are called */
            return false;
        }
    }
    return false;
}

void EventSources::add_trigger(Trigger& trigger)
{
    add_event(trigger.get_fd(), &trigger);
}

void EventSources::remove_trigger(Trigger& trigger)
{
    remove_event(&trigger);
}

EventSources::Trigger::Trigger()
{
    int fd[2];
    if (::pipe(fd) == -1) {
        THROW("create pipe failed");
    }
    _event_fd = fd[0];
    _event_write_fd = fd[1];
    set_non_blocking(_event_fd);
}

EventSources::Trigger::~Trigger()
{
    close(_event_fd);
    close(_event_write_fd);
}

void EventSources::Trigger::trigger()
{
    Lock lock(_lock);
    if (_pending_int) {
        return;
    }
    _pending_int = true;
    const uint8_t val = 1;
    if (::write(_event_write_fd, &val, sizeof(val)) != sizeof(val)) {
        THROW("write event failed");
    }
}

bool Trigger_p::reset_event()
{
    Lock lock(_lock);
    if (!_pending_int) {
        return false;
    }
    uint8_t val;
    if (::read(_event_fd, &val, sizeof(val)) != sizeof(val)) {
        THROW("event read error");
    }
    _pending_int = false;
    return true;
}

void EventSources::Trigger::reset()
{
    reset_event();
}

void EventSources::Trigger::action()
{
    if (reset_event()) {
        on_event();
    }
}

void EventSources::add_socket(Socket& socket)
{
    add_event(socket.get_socket(), &socket);
    set_non_blocking(socket.get_socket());
}

void EventSources::remove_socket(Socket& socket)
{
    remove_event(&socket);
    int fd = socket.get_socket();
    set_blocking(fd);
}

void EventSources::add_file(File& file)
{
    add_event(file.get_fd(), &file);
}

void EventSources::remove_file(File& file)
{
    remove_event(&file);
}

void EventSources::add_handle(Handle& file)
{
}

void EventSources::remove_handle(Handle& file)
{
}
