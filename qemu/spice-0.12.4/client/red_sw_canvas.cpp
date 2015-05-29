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

#include <stdint.h>
#ifdef WIN32
#include <winsock2.h>
#endif

#include "common/region.h"
#define SPICE_CANVAS_INTERNAL
#define SW_CANVAS_CACHE
#include "common/sw_canvas.c"
#undef SW_CANVAS_CACHE
#undef SPICE_CANVAS_INTERNAL

#include "common.h"
#include "red_window.h"
#include "red_sw_canvas.h"
#include "utils.h"
#include "debug.h"
#include "red_pixmap_sw.h"

SCanvas::SCanvas(bool onscreen,
                 int width, int height, uint32_t format, RedWindow *win,
                 PixmapCache& pixmap_cache, PaletteCache& palette_cache,
                 GlzDecoderWindow &glz_decoder_window, SurfacesCache& csurfaces)
    : Canvas (pixmap_cache, palette_cache, glz_decoder_window, csurfaces)
    , _pixmap (0)
{
    if (onscreen) {
        _pixmap = new RedPixmapSw(width, height,
                                  RedDrawable::format_from_surface(format),
                                  true, win);
        _canvas = canvas_create_for_data(width, height, format,
                                         _pixmap->get_data(),
                                         _pixmap->get_stride(),
                                         &pixmap_cache.base,
                                         &palette_cache.base,
                                         &csurfaces,
                                         &glz_decoder(),
                                         &jpeg_decoder(),
                                         &zlib_decoder());
    } else {
        _canvas = canvas_create(width, height, format,
                                &pixmap_cache.base,
                                &palette_cache.base,
                                &csurfaces,
                                &glz_decoder(),
                                &jpeg_decoder(),
                                &zlib_decoder());
    }
    if (_canvas == NULL) {
        THROW("create canvas failed");
    }
}

SCanvas::~SCanvas()
{
    _canvas->ops->destroy(_canvas);
    _canvas = NULL;
    if (_pixmap) {
        delete _pixmap;
        _pixmap = NULL;
    }
}

void SCanvas::copy_pixels(const QRegion& region, RedDrawable& dest_dc)
{
    pixman_box32_t *rects;
    int num_rects;

    ASSERT(_pixmap != NULL);

    rects = pixman_region32_rectangles((pixman_region32_t *)&region, &num_rects);
    for (int i = 0; i < num_rects; i++) {
        SpiceRect r;

        r.left = rects[i].x1;
        r.top = rects[i].y1;
        r.right = rects[i].x2;
        r.bottom = rects[i].y2;
        dest_dc.copy_pixels(*_pixmap, r.left, r.top, r);
    }
}

void SCanvas::copy_pixels(const QRegion& region, RedDrawable* dest_dc, const PixmapHeader* pixmap)
{
    copy_pixels(region, *dest_dc);
}

CanvasType SCanvas::get_pixmap_type()
{
    return CANVAS_TYPE_SW;
}
