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
#include "event_sources.h"
#include "debug.h"
#include "utils.h"

bool EventSources_p::process_system_events()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

        if (msg.message == WM_QUIT) {
            return true;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return false;
}

void EventSources_p::add_event(HANDLE event, EventSource* source)
{
    int size = _events.size();
    _events.resize(size + 1);
    _handles.resize(size + 1);
    _events[size] = source;
    _handles[size] = event;
}

void EventSources_p::remove_event(EventSource* source)
{
    int size = _events.size();
    for (int i = 0; i < size; i++) {
        if (_events[i] == source) {
            for (i++; i < size; i++) {
                _events[i - 1] = _events[i];
                _handles[i - 1] = _handles[i];
            }
            _events.resize(size - 1);
            _handles.resize(size - 1);
            return;
        }
    }
    THROW("event not found");
}

EventSources::EventSources()
{
}

EventSources::~EventSources()
{
}

bool EventSources::wait_events(int timeout_ms)
{
    if (_handles.empty()) {
        if (WaitMessage()) {
            return process_system_events();
        } else {
            THROW("wait failed %d", GetLastError());
        }
    }

    DWORD wait_res = MsgWaitForMultipleObjectsEx(_handles.size(),  &_handles[0], timeout_ms,
                                                 QS_ALLINPUT, 0);
    if (wait_res == WAIT_TIMEOUT) {
        return false;
    }

    if (wait_res == WAIT_FAILED) {
        THROW("wait failed %d", GetLastError());
    }

    size_t event_index = wait_res - WAIT_OBJECT_0;
    if (event_index == _handles.size()) {
        return process_system_events();
    } else if ((event_index >= 0) && (event_index < _handles.size())) {
        _events[event_index]->action();
        return false;
    } else {
        THROW("invalid event id");
    }
}

void EventSources::add_socket(Socket& socket)
{
    HANDLE event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!event) {
        THROW("create event failed");
    }
    if (WSAEventSelect(socket.get_socket(), event,
                       FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR) {
        CloseHandle(event);
        THROW("event select failed");
    }
    add_event(event, &socket);
}

void EventSources::remove_socket(Socket& socket)
{
    int size = _events.size();
    for (int i = 0; i < size; i++) {
        if (_events[i] == &socket) {
            if (WSAEventSelect(socket.get_socket(), NULL, 0) == SOCKET_ERROR) {
                THROW("event select failed");
            }
            u_long arg = 0;
            if (ioctlsocket(socket.get_socket(), FIONBIO, &arg) == SOCKET_ERROR) {
                THROW("set blocking mode failed");
            }
            CloseHandle(_handles[i]);
            for (i++; i < size; i++) {
                _events[i - 1] = _events[i];
                _handles[i - 1] = _handles[i];
            }
            _events.resize(size - 1);
            _handles.resize(size - 1);
            return;
        }
    }
    THROW("socket not found");
}

void EventSources::add_handle(Handle& handle)
{
    add_event(handle.get_handle(), &handle);
}

void EventSources::remove_handle(Handle& handle)
{
    remove_event(&handle);
}

Handle_p::Handle_p()
{
    if (!(_event = CreateEvent(NULL, FALSE, FALSE, NULL))) {
        THROW("create event failed");
    }
}

Handle_p::~Handle_p()
{
    CloseHandle(_event);
}

void EventSources::add_trigger(Trigger& trigger)
{
    add_event(trigger.get_handle(), &trigger);
}

void EventSources::remove_trigger(Trigger& trigger)
{
    remove_event(&trigger);
}


EventSources::Trigger::Trigger()
{
}

EventSources::Trigger::~Trigger()
{
}


void EventSources::Trigger::trigger()
{
    if (!SetEvent(_event)) {
        THROW("set event failed");
    }
}

void EventSources::Trigger::reset()
{
    if (!ResetEvent(_event)) {
        THROW("set event failed");
    }
}

void EventSources::Trigger::action()
{
    on_event();
}

void EventSources::add_file(File& file)
{
}

void EventSources::remove_file(File& file)
{
}
