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

#ifndef _H_READ_WRITE_MUTEX
#define _H_READ_WRITE_MUTEX

#include "threads.h"

class ReadWriteMutex {
public:
    ReadWriteMutex()
    {
        _state.read_count = 0;
        _state.write = false;
        _state.write_waiting = false;
    }

    virtual ~ReadWriteMutex()
    {
    }

    void read_lock()
    {
        Lock lock(_state_mutex);

        while (_state.write || _state.write_waiting) {
            _read_cond.wait(lock);
        }

        ++_state.read_count;
    }

    bool try_read_lock()
    {
        Lock lock(_state_mutex);
        if (_state.write || _state.write_waiting) {
            return false;
        } else {
            ++_state.read_count;
            return true;
        }
    }

    void read_unlock()
    {
        Lock lock(_state_mutex);
        --_state.read_count;

        if (!_state.read_count) { // last reader
            _state.write_waiting = false;
            release_waiters();
        }
    }

    void write_lock()
    {
        Lock lock(_state_mutex);

        while (_state.read_count || _state.write) {
            _state.write_waiting = true;
            _write_cond.wait(lock);
        }
        _state.write = true;
    }

    bool try_write_lock()
    {
        Lock lock(_state_mutex);

        if (_state.read_count || _state.write) {
            return false;
        } else {
            _state.write = true;
            return true;
        }
    }

    void write_unlock()
    {
        Lock lock(_state_mutex);
        _state.write = false;
        _state.write_waiting = false;
        release_waiters();
    }

private:
    void release_waiters()
    {
        _write_cond.notify_one();
        _read_cond.notify_all();
    }

private:
    struct {
        unsigned int read_count;
        bool write;
        bool write_waiting;
    } _state;

    Mutex _state_mutex;
    Condition _read_cond;
    Condition _write_cond;
};

#endif
