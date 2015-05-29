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
#include "cursor.h"
#include "utils.h"
#include "debug.h"

CursorData::CursorData(SpiceCursor& cursor, int data_size)
    : _atomic (1)
    , _header (cursor.header)
    , _data (NULL)
    , _opaque (NULL)
    , _local_cursor (NULL)
{
    int expected_size = 0;

    switch (cursor.header.type) {
    case SPICE_CURSOR_TYPE_ALPHA:
        expected_size = (_header.width << 2) * _header.height;
        break;
    case SPICE_CURSOR_TYPE_MONO:
        expected_size = (SPICE_ALIGN(_header.width, 8) >> 2) * _header.height;
        break;
    case SPICE_CURSOR_TYPE_COLOR4:
        expected_size = (SPICE_ALIGN(_header.width, 2) >> 1) * _header.height;
        expected_size += (SPICE_ALIGN(_header.width, 8) >> 3) * _header.height;
        expected_size += 16 * sizeof(uint32_t);
        break;
    case SPICE_CURSOR_TYPE_COLOR8:
        expected_size = _header.width * _header.height;
        expected_size += (SPICE_ALIGN(_header.width, 8) >> 3) * _header.height;
        expected_size += 256 * sizeof(uint32_t);
        break;
    case SPICE_CURSOR_TYPE_COLOR16:
        expected_size = (_header.width << 1) * _header.height;
        expected_size += (SPICE_ALIGN(_header.width, 8) >> 3) * _header.height;
        break;
    case SPICE_CURSOR_TYPE_COLOR24:
        expected_size = (_header.width * 3) * _header.height;
        expected_size += (SPICE_ALIGN(_header.width, 8) >> 3) * _header.height;
        break;
    case SPICE_CURSOR_TYPE_COLOR32:
        expected_size = (_header.width << 2) * _header.height;
        expected_size += (SPICE_ALIGN(_header.width, 8) >> 3) * _header.height;
        break;
    }

    if (data_size < expected_size) {
        THROW("access violation 0x%" PRIuPTR " %u", (uintptr_t)cursor.data, expected_size);
    }
    _data = new uint8_t[expected_size];
    memcpy(_data, cursor.data, expected_size);
}

void CursorData::set_local(LocalCursor* local_cursor)
{
    ASSERT(!_local_cursor);
    if (local_cursor) {
        _local_cursor = local_cursor->ref();
    }
}

CursorData::~CursorData()
{
    if (_local_cursor) {
        _local_cursor->unref();
    }
    delete _opaque;
    delete[] _data;
}

int LocalCursor::get_size_bits(const SpiceCursorHeader& header, int& size)
{
    switch (header.type) {
    case SPICE_CURSOR_TYPE_ALPHA:
    case SPICE_CURSOR_TYPE_COLOR32:
        size = (header.width << 2) * header.height;
        return 32;
    case SPICE_CURSOR_TYPE_MONO:
        size = (SPICE_ALIGN(header.width, 8) >> 3) * header.height;
        return 1;
    case SPICE_CURSOR_TYPE_COLOR4:
        size = (SPICE_ALIGN(header.width, 2) >> 1) * header.height;
        return 4;
    case SPICE_CURSOR_TYPE_COLOR8:
        size = header.width * header.height;
        return 8;
    case SPICE_CURSOR_TYPE_COLOR16:
        size = (header.width << 1) * header.height;
        return 16;
    case SPICE_CURSOR_TYPE_COLOR24:
        size = (header.width * 3) * header.height;
        return 24;
    default:
        return 0;
    }
}
