/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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
#include "red_pixmap_gdi.h"
#include "red_pixmap.h"
#include "debug.h"
#include "utils.h"
#include "pixels_source_p.h"
#include "platform_utils.h"

struct RedPixmap_p {
    PixelsSource_p pixels_source_p;
    HBITMAP prev_bitmap;
};

RedPixmapGdi::RedPixmapGdi(int width, int height, RedDrawable::Format format, bool top_bottom)
    : RedPixmap(width, height, format, top_bottom)
{
    DWORD *pixel_format;
    ASSERT(format == RedDrawable::ARGB32 || format == RedDrawable::RGB32
           || format == RedDrawable::RGB16_555 || format == RedDrawable::RGB16_565
           || format == RedDrawable::A1);
    ASSERT(sizeof(RedPixmap_p) <= PIXELES_SOURCE_OPAQUE_SIZE);

    struct {
        BITMAPINFO inf;
        RGBQUAD palette[255];
    } bitmap_info;

    memset(&bitmap_info, 0, sizeof(bitmap_info));
    bitmap_info.inf.bmiHeader.biSize = sizeof(bitmap_info.inf.bmiHeader);
    bitmap_info.inf.bmiHeader.biWidth = _width;
    bitmap_info.inf.bmiHeader.biHeight = top_bottom ? -_height : _height;

    bitmap_info.inf.bmiHeader.biPlanes = 1;
    bitmap_info.inf.bmiHeader.biBitCount = RedDrawable::format_to_bpp(format);
    if (format == RedDrawable::RGB16_565) {
        bitmap_info.inf.bmiHeader.biCompression = BI_BITFIELDS;

    } else {
        bitmap_info.inf.bmiHeader.biCompression = BI_RGB;
    }
    switch (format) {
    case RedDrawable::A1:
        bitmap_info.inf.bmiColors[0].rgbRed = 0;
        bitmap_info.inf.bmiColors[0].rgbGreen = 0;
        bitmap_info.inf.bmiColors[0].rgbBlue = 0;
#ifndef __MINGW32__
        // inf.bmiColors is [1] in mingw/include/wingdi.h
        bitmap_info.inf.bmiColors[1].rgbRed = 0xff;
        bitmap_info.inf.bmiColors[1].rgbGreen = 0xff;
        bitmap_info.inf.bmiColors[1].rgbBlue = 0xff;
#endif
        break;
     case RedDrawable::RGB16_565:
        pixel_format = (DWORD *)bitmap_info.inf.bmiColors;
        pixel_format[0] = 0xf800;
        pixel_format[1] = 0x07e0;
        pixel_format[2] = 0x001f;
        break;
     case RedDrawable::ARGB32:
     case RedDrawable::RGB32:
     case RedDrawable::RGB16_555:
        break;
    }
    AutoDC dc(create_compatible_dc());
    AutoGDIObject bitmap(CreateDIBSection(dc.get(), &bitmap_info.inf, 0,
                                          (VOID **)&_data, NULL, 0));
    if (!bitmap.valid()) {
        THROW("create compatible bitmap failed");
    }
    memset(_data, 1, 1);
    ((RedPixmap_p*)get_opaque())->prev_bitmap = (HBITMAP)SelectObject(dc.get(), bitmap.release());
    ((RedPixmap_p*)get_opaque())->pixels_source_p.dc = dc.release();
}

HDC RedPixmapGdi::get_dc()
{
    return ((RedPixmap_p*)get_opaque())->pixels_source_p.dc;
}

void *RedPixmapGdi::get_memptr()
{
    return _data;
}

RedPixmapGdi::~RedPixmapGdi()
{
    HDC dc = ((RedPixmap_p*)get_opaque())->pixels_source_p.dc;
    if (dc) {
        HBITMAP prev_bitmap = ((RedPixmap_p*)get_opaque())->prev_bitmap;
        HBITMAP bitmap = (HBITMAP)SelectObject(dc, prev_bitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }
}

RecurciveMutex& RedPixmapGdi::get_mutex()
{
    RedPixmap_p* p_data = (RedPixmap_p*)get_opaque();
    return *p_data->pixels_source_p._mutex;
}
