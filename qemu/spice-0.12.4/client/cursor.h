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

#ifndef _H_CURSOR_
#define _H_CURSOR_

#include "common/messages.h"
#include "threads.h"
#include "red_window_p.h"

class CursorOpaque {
public:
    CursorOpaque() {}
    virtual ~CursorOpaque() {}
};

class LocalCursor;

class CursorData {
public:
    CursorData(SpiceCursor& cursor, int data_size);

    CursorData *ref() { ++_atomic; return this;}
    void unref() {if (--_atomic == 0) delete this;}
    void set_opaque(CursorOpaque* opaque) { delete _opaque; _opaque = opaque;}
    CursorOpaque* get_opaque() { return _opaque;}
    void set_local(LocalCursor* local_cursor);
    LocalCursor* get_local() { return _local_cursor;}
    const SpiceCursorHeader& header() const { return _header;}
    const uint8_t* data() const { return _data;}

private:
    ~CursorData();

private:
    AtomicCount _atomic;
    SpiceCursorHeader _header;
    uint8_t* _data;
    CursorOpaque* _opaque;
    LocalCursor* _local_cursor;
};

class LocalCursor {
public:
    LocalCursor(): _atomic (1) {}
    virtual ~LocalCursor() {}
    virtual void set(Window window) {}
    LocalCursor* ref() { ++_atomic; return this;}
    void unref() { if (--_atomic == 0) delete this;}

protected:
    static int get_size_bits(const SpiceCursorHeader& header, int &size);

private:
    AtomicCount _atomic;
};

#endif
