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
#include "red_drawable.h"
#include "pixels_source_p.h"
#include "utils.h"
#include "threads.h"

static const uint64_t lock_timout = 1000 * 1000 * 10; /*10ms*/

void RedDrawable::copy_pixels(const PixelsSource& src, int src_x, int src_y, const SpiceRect& dest)
{
    PixelsSource_p* dest_p_data = (PixelsSource_p*)get_opaque();
    PixelsSource_p* src_p_data = (PixelsSource_p*)src.get_opaque();

    for (;;) {
        Lock lock(*dest_p_data->_mutex);
        Lock timed_lock(*src_p_data->_mutex, lock_timout);
        if (!timed_lock.is_locked()) {
            continue;
        }
        BitBlt(dest_p_data->dc, dest.left + _origin.x, dest.top + _origin.y,
               dest.right - dest.left, dest.bottom - dest.top,
               src_p_data->dc, src_x + src._origin.x,
               src_y + src._origin.y, SRCCOPY);
        return;
    }
}

void RedDrawable::blend_pixels(const PixelsSource& src, int src_x, int src_y, const SpiceRect& dest)
{
    static BLENDFUNCTION blend_func = { AC_SRC_OVER, 0, 0xff, AC_SRC_ALPHA};

    int width = dest.right - dest.left;
    int height = dest.bottom - dest.top;
    PixelsSource_p* dest_p_data = (PixelsSource_p*)get_opaque();
    PixelsSource_p* src_p_data = (PixelsSource_p*)src.get_opaque();
    for (;;) {
        RecurciveLock lock(*dest_p_data->_mutex);
        RecurciveLock timed_lock(*src_p_data->_mutex, lock_timout);
        if (!timed_lock.is_locked()) {
            continue;
        }
        AlphaBlend(dest_p_data->dc, dest.left + _origin.x, dest.top + _origin.y, width, height,
                   src_p_data->dc, src_x + src._origin.x, src_y + src._origin.y, width, height,
                   blend_func);
        return;
    }
}

void RedDrawable::combine_pixels(const PixelsSource& src, int src_x, int src_y, const SpiceRect& dest,
                                 CombineOP op)
{
    DWORD rop;
    switch (op) {
    case OP_COPY:
        rop = SRCCOPY;
        break;
    case OP_AND:
        rop = SRCAND;
        break;
    case OP_XOR:
        rop = SRCINVERT;
        break;
    default:
        THROW("invalid op %d", op);
    }

    PixelsSource_p* dest_p_data = (PixelsSource_p*)get_opaque();
    PixelsSource_p* src_p_data = (PixelsSource_p*)src.get_opaque();
    for (;;) {
        RecurciveLock lock(*dest_p_data->_mutex);
        RecurciveLock timed_lock(*src_p_data->_mutex, lock_timout);
        if (!timed_lock.is_locked()) {
            continue;
        }
        BitBlt(dest_p_data->dc, dest.left + _origin.x, dest.top + _origin.y,
               dest.right - dest.left, dest.bottom - dest.top,
               src_p_data->dc, src_x + src._origin.x,
               src_y + src._origin.y, rop);
        return;
    }
}

void RedDrawable::erase_rect(const SpiceRect& rect, rgb32_t color)
{
    RECT r;
    r.left = rect.left + _origin.x;
    r.right = rect.right + _origin.x;
    r.top = rect.top + _origin.y;
    r.bottom = rect.bottom + _origin.y;

    PixelsSource_p* dest_p_data = (PixelsSource_p*)get_opaque();
    RecurciveLock lock(*dest_p_data->_mutex);
    FillRect(dest_p_data->dc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
}

void RedDrawable::fill_rect(const SpiceRect& rect, rgb32_t color)
{
    RECT r;
    r.left = rect.left + _origin.x;
    r.right = rect.right + _origin.x;
    r.top = rect.top + _origin.y;
    r.bottom = rect.bottom + _origin.y;

    HBRUSH brush = CreateSolidBrush(RGB(rgb32_get_red(color),
                                        rgb32_get_green(color),
                                        rgb32_get_blue(color)));
    for (;;) {
        PixelsSource_p* dest_p_data = (PixelsSource_p*)get_opaque();
        RecurciveLock lock(*dest_p_data->_mutex);
        FillRect(dest_p_data->dc, &r, brush);
        break;
    }
    DeleteObject(brush);
}

void RedDrawable::frame_rect(const SpiceRect& rect, rgb32_t color)
{
    RECT r;
    r.left = rect.left + _origin.x;
    r.right = rect.right + _origin.x;
    r.top = rect.top + _origin.y;
    r.bottom = rect.bottom + _origin.y;
    HBRUSH brush = CreateSolidBrush(RGB(rgb32_get_red(color),
                                        rgb32_get_green(color),
                                        rgb32_get_blue(color)));
    for (;;) {
        PixelsSource_p* dest_p_data = (PixelsSource_p*)get_opaque();
        RecurciveLock lock(*dest_p_data->_mutex);
        FrameRect(dest_p_data->dc, &r, brush);
        break;
    }
    DeleteObject(brush);
}
