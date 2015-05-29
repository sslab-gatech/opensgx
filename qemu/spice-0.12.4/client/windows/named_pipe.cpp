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
#include "named_pipe.h"
#include "utils.h"
#include "debug.h"

#define PIPE_TIMEOUT 5000
#define PIPE_MAX_NAME_LEN 256
#define PIPE_PREFIX TEXT("\\\\.\\pipe\\")

PipeBuffer::PipeBuffer(HANDLE pipe, ProcessLoop& process_loop)
    : _handler (NULL)
    , _pipe (pipe)
    , _start (0)
    , _end (0)
    , _pending (false)
    , _process_loop(process_loop)
{
    ZeroMemory(&_overlap, sizeof(_overlap));
    _overlap.hEvent = this->get_handle();
    _process_loop.add_handle(*this);
}

PipeBuffer::~PipeBuffer()
{
    _process_loop.remove_handle(*this);
}

DWORD PipeBuffer::get_overlapped_bytes()
{
    DWORD bytes = 0;

    if (!GetOverlappedResult(_pipe, &_overlap, &bytes, FALSE) || bytes == 0) {
        _pending = false;
        _handler->on_data();
    }
    return bytes;
}

int32_t PipeReader::read(uint8_t *buf, int32_t size)
{
    ASSERT(buf && size >= 0);

    if (_start < _end) {
        int32_t bytes_read = 0;
        bytes_read = MIN(_end - _start, (uint32_t)size);
        CopyMemory(buf, _data + _start, bytes_read);
        _start += bytes_read;
        if (_start == _end) {
            _start = _end = 0;
        }
        return bytes_read;
    }
    if (_pending) {
        return 0;
    }
    if (!ReadFile(_pipe, _data + _end, sizeof(_data) - _end, NULL, &_overlap) &&
                                              GetLastError() != ERROR_IO_PENDING) {
        DBG(0, "ReadFile() failed %u", GetLastError());
        return -1;
    }
    _pending = true;
    return 0;
}

void PipeReader::on_event()
{
    ASSERT(_pending);
    DWORD bytes = get_overlapped_bytes();

    if (!bytes) {
        return;
    }
    _end += bytes;
    _pending = false;
    _handler->on_data();
}

int32_t PipeWriter::write(const uint8_t *buf, int32_t size)
{
    int32_t bytes_written = 0;
    ASSERT(buf && size >= 0);

    if (!_pending && _start == _end) {
        _start = _end = 0;
    }
    if (_end < sizeof(_data)) {
        bytes_written = MIN(sizeof(_data) - _end, (uint32_t)size);
        CopyMemory(_data + _end, buf, bytes_written);
        _end += bytes_written;
    }
    if (!_pending && _start < _end) {
        if (!WriteFile(_pipe, _data + _start, _end - _start, NULL, &_overlap) &&
                                              GetLastError() != ERROR_IO_PENDING) {
            DBG(0, "WriteFile() failed %u", GetLastError());
            return -1;
        }
        _pending = true;
    }
    return bytes_written;
}

void PipeWriter::on_event()
{
    ASSERT(_pending);
    DWORD bytes = get_overlapped_bytes();
    if (!bytes) {
        return;
    }
    _start += bytes;
    _pending = false;
    if (_start == sizeof(_data)) {
        _handler->on_data();
    }
}

WinConnection::WinConnection(HANDLE pipe, ProcessLoop& process_loop)
    : _pipe (pipe)
    , _writer (pipe, process_loop)
    , _reader (pipe, process_loop)
{
}

WinConnection::~WinConnection()
{
    if (!DisconnectNamedPipe(_pipe)) {
        DBG(0, "DisconnectNamedPipe failed %d", GetLastError());
    }
    CloseHandle(_pipe);
}

int32_t WinConnection::read(uint8_t *buf, int32_t size)
{
    return _reader.read(buf, size);
}

int32_t WinConnection::write(const uint8_t *buf, int32_t size)
{
    return _writer.write(buf, size);
}

void WinConnection::set_handler(NamedPipe::ConnectionInterface* handler)
{
    _reader.set_handler(handler);
    _writer.set_handler(handler);
}

WinListener::WinListener(const char *name, NamedPipe::ListenerInterface &listener_interface,
                         ProcessLoop& process_loop)
    : _listener_interface (listener_interface)
    , _pipe (0)
    , _process_loop (process_loop)
{
    _pipename = new TCHAR[PIPE_MAX_NAME_LEN];
    swprintf_s(_pipename, PIPE_MAX_NAME_LEN, L"%s%S", PIPE_PREFIX, name);
    ZeroMemory(&_overlap, sizeof(_overlap));
    _overlap.hEvent = this->get_handle();
    _process_loop.add_handle(*this);
    create_pipe();
}

WinListener::~WinListener()
{
    CancelIo(_pipe);
    _process_loop.remove_handle(*this);
    delete[] _pipename;
}

void WinListener::on_event()
{
    DWORD bytes;

    if (!GetOverlappedResult(_pipe, &_overlap, &bytes, FALSE)) {
        DBG(0, "GetOverlappedResult() failed %u", GetLastError());
        return;
    }
    DBG(0, "Pipe connected 0x%p", _pipe);
    WinConnection *con = new WinConnection(_pipe, _process_loop);
    NamedPipe::ConnectionInterface &con_interface = _listener_interface.create();
    con->set_handler(&con_interface);
    con_interface.bind((NamedPipe::ConnectionRef)con);
    create_pipe();
}

void WinListener::create_pipe()
{
    _pipe = CreateNamedPipe(_pipename, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                            PIPE_UNLIMITED_INSTANCES, PIPE_BUF_SIZE, PIPE_BUF_SIZE,
                            PIPE_TIMEOUT, NULL);
    if (_pipe == INVALID_HANDLE_VALUE) {
        THROW("CreateNamedPipe() failed %u", GetLastError());
    }
    if (ConnectNamedPipe(_pipe, &_overlap)) {
        THROW("ConnectNamedPipe() is not pending");
    }
    switch (GetLastError()) {
    case ERROR_IO_PENDING:
        DBG(0, "Pipe waits for connection");
        break;
    case ERROR_PIPE_CONNECTED: {
        DBG(0, "Pipe already connected");
        WinConnection *con = new WinConnection(_pipe, _process_loop);
        NamedPipe::ConnectionInterface &con_interface = _listener_interface.create();
        con->set_handler(&con_interface);
        con_interface.bind((NamedPipe::ConnectionRef)con);
        create_pipe();
        break;
    }
    default:
        THROW("ConnectNamedPipe() failed %u", GetLastError());
    }
}
