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

#include <windows.h>
#include "process_loop.h"
#include "event_sources.h"
#include "platform.h"

#define PIPE_BUF_SIZE 8192

class WinConnection;

class PipeBuffer: public EventSources::Handle {
public:
    PipeBuffer(HANDLE pipe, ProcessLoop& process_loop);
    ~PipeBuffer();
    void set_handler(NamedPipe::ConnectionInterface* handler) { _handler = handler;}
    DWORD get_overlapped_bytes();

protected:
    NamedPipe::ConnectionInterface *_handler;
    OVERLAPPED _overlap;
    HANDLE _pipe;
    uint32_t _start;
    uint32_t _end;
    uint8_t _data[PIPE_BUF_SIZE];
    bool _pending;
    ProcessLoop& _process_loop;
};

class PipeReader: public PipeBuffer {
public:
    PipeReader(HANDLE pipe, ProcessLoop& process_loop) : PipeBuffer(pipe, process_loop) {}
    int32_t read(uint8_t *buf, int32_t size);
    void on_event();
};

class PipeWriter: public PipeBuffer {
public:
    PipeWriter(HANDLE pipe, ProcessLoop& process_loop) : PipeBuffer(pipe, process_loop) {}
    int32_t write(const uint8_t *buf, int32_t size);
    void on_event();
};

class WinConnection {
public:
    WinConnection(HANDLE pipe, ProcessLoop& process_loop);
    ~WinConnection();
    int32_t read(uint8_t *buf, int32_t size);
    int32_t write(const uint8_t *buf, int32_t size);
    void set_handler(NamedPipe::ConnectionInterface* handler);

private:
    HANDLE _pipe;
    PipeWriter _writer;
    PipeReader _reader;
};

class WinListener: public EventSources::Handle {
public:
    WinListener(const char *name, NamedPipe::ListenerInterface &listener_interface,
                ProcessLoop& process_loop);
    ~WinListener();
    void on_event();

private:
    void create_pipe();

private:
    TCHAR *_pipename;
    NamedPipe::ListenerInterface &_listener_interface;
    OVERLAPPED _overlap;
    HANDLE _pipe;
    ProcessLoop& _process_loop;
};

#endif
