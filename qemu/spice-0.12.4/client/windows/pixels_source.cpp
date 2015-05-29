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
#include "pixels_source.h"
#include "pixels_source_p.h"
#include "platform_utils.h"
#include "threads.h"
#include "debug.h"


static SpicePoint get_bitmap_size(HDC dc)
{
    BITMAP bitmap_info;
    SpicePoint size;

    GetObject(GetCurrentObject(dc, OBJ_BITMAP), sizeof(bitmap_info), &bitmap_info);

    size.x = bitmap_info.bmWidth;
    size.y = bitmap_info.bmHeight;
    return size;
}

PixelsSource::PixelsSource()
{
    ASSERT(sizeof(_opaque) >= sizeof(PixelsSource_p));
    _origin.x = _origin.y = 0;
    memset(_opaque, 0, sizeof(_opaque));
    PixelsSource_p* p_data = (PixelsSource_p*)_opaque;
    p_data->_mutex = new RecurciveMutex();
}

PixelsSource::~PixelsSource()
{
    PixelsSource_p* p_data = (PixelsSource_p*)_opaque;
    delete p_data->_mutex;
}

struct ResImage_p {
    PixelsSource_p source_p;
    HBITMAP prev_bitmap;
};


ImageFromRes::ImageFromRes(int res_id)
{
    AutoDC dc(create_compatible_dc());
    ((ResImage_p*)get_opaque())->prev_bitmap = (HBITMAP)SelectObject(dc.get(),
                                                                     get_bitmap_res(res_id));
    ((ResImage_p*)get_opaque())->source_p.dc = dc.release();
}

ImageFromRes::~ImageFromRes()
{
    HDC dc = ((ResImage_p*)get_opaque())->source_p.dc;
    if (dc) {
        HGDIOBJ bitmap = SelectObject(dc, ((ResImage_p*)get_opaque())->prev_bitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }
}

SpicePoint ImageFromRes::get_size()
{
    ResImage_p* p_data = (ResImage_p*)get_opaque();
    Lock lock(*p_data->source_p._mutex);
    return get_bitmap_size(p_data->source_p.dc);
}

AlphaImageFromRes::AlphaImageFromRes(int res_id)
{
    AutoDC dc(create_compatible_dc());
    ((ResImage_p*)get_opaque())->prev_bitmap = (HBITMAP)SelectObject(dc.get(),
                                                                     get_alpha_bitmap_res(res_id));
    ((ResImage_p*)get_opaque())->source_p.dc = dc.release();
}

AlphaImageFromRes::~AlphaImageFromRes()
{
    HDC dc = ((ResImage_p*)get_opaque())->source_p.dc;
    if (dc) {
        HGDIOBJ bitmap = SelectObject(dc, ((ResImage_p*)get_opaque())->prev_bitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }
}

SpicePoint AlphaImageFromRes::get_size()
{
    ResImage_p* p_data = (ResImage_p*)get_opaque();
    Lock lock(*p_data->source_p._mutex);
    return get_bitmap_size(p_data->source_p.dc);
}
