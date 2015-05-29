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
#include "red_pixmap.h"
#include "debug.h"
#include "utils.h"

RedPixmap::RedPixmap(int width, int height, RedPixmap::Format format,
                     bool top_bottom)
    : _format (format)
    , _width (width)
    , _height (height)
    , _stride (SPICE_ALIGN(width * format_to_bpp(format), 32) / 8)
    , _top_bottom (top_bottom)
    , _data (NULL)
{
}

RedPixmap::~RedPixmap()
{
}

bool RedPixmap::is_big_endian_bits()
{
    return false;
}
