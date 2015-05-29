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

#ifndef _H_RED_COMMON
#define _H_RED_COMMON

#include <sys/uio.h>
#include <openssl/ssl.h>
#include <spice/macros.h>

#include "common/mem.h"
#include "common/spice_common.h"
#include "common/messages.h"
#include "common/lz_common.h"

#include "spice.h"

enum {
    STREAM_VIDEO_INVALID,
    STREAM_VIDEO_OFF,
    STREAM_VIDEO_ALL,
    STREAM_VIDEO_FILTER
};

static const LzImageType MAP_BITMAP_FMT_TO_LZ_IMAGE_TYPE[] = {
    LZ_IMAGE_TYPE_INVALID,
    LZ_IMAGE_TYPE_PLT1_LE,
    LZ_IMAGE_TYPE_PLT1_BE,
    LZ_IMAGE_TYPE_PLT4_LE,
    LZ_IMAGE_TYPE_PLT4_BE,
    LZ_IMAGE_TYPE_PLT8,
    LZ_IMAGE_TYPE_RGB16,
    LZ_IMAGE_TYPE_RGB24,
    LZ_IMAGE_TYPE_RGB32,
    LZ_IMAGE_TYPE_RGBA,
    LZ_IMAGE_TYPE_A8
};

static inline int bitmap_fmt_is_rgb(uint8_t fmt)
{
    static const int BITMAP_FMT_IS_RGB[SPICE_BITMAP_FMT_ENUM_END] =
                                        {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1};

    if (fmt >= SPICE_BITMAP_FMT_ENUM_END) {
        spice_warning("fmt >= SPICE_BITMAP_FMT_ENUM_END; %d >= %d",
                      fmt, SPICE_BITMAP_FMT_ENUM_END);
        return 0;
    }
    return BITMAP_FMT_IS_RGB[fmt];
}

#endif
