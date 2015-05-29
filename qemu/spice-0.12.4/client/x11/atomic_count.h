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

#ifndef _H_ATOMIC_COUNT
#define _H_ATOMIC_COUNT

#include <stdint.h>

class AtomicCount {
public:
    AtomicCount(uint32_t count = 0) : _count (count) {}

    uint32_t operator ++ ()
    {
        return __sync_add_and_fetch(&_count, 1);
    }

    uint32_t operator -- ()
    {
        return __sync_sub_and_fetch(&_count, 1);
    }

    operator uint32_t () { return _count;}

private:
    uint32_t _count;
};

#endif
