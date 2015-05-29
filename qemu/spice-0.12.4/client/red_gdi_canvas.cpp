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
#ifdef __MINGW32__
#undef HAVE_STDLIB_H
#endif
#include <config.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#endif
#include <stdint.h>

#include "common/region.h"
#define SPICE_CANVAS_INTERNAL
#define SW_CANVAS_CACHE
#include "common/gdi_canvas.c"
#undef SW_CANVAS_CACHE
#undef SPICE_CANVAS_INTERNAL

#include "common.h"
#include "red_gdi_canvas.h"
#include "utils.h"
#include "debug.h"
#include "red_pixmap_gdi.h"

GDICanvas::GDICanvas(int width, int height, uint32_t format,
		     PixmapCache& pixmap_cache, PaletteCache& palette_cache,
                     GlzDecoderWindow &glz_decoder_window, SurfacesCache &csurfaces)
    : Canvas (pixmap_cache, palette_cache, glz_decoder_window, csurfaces)
    , _pixmap (0)
{
    _pixmap = new RedPixmapGdi(width, height,
                               RedDrawable::format_from_surface(format),
                               true);
    if (!(_canvas = gdi_canvas_create(width, height, _pixmap->get_dc(),
                                      &_pixmap->get_mutex(),
                                      format, &pixmap_cache.base,
                                      &palette_cache.base,
                                      &csurfaces,
                                      &glz_decoder(),
                                      &jpeg_decoder(),
                                      &zlib_decoder()))) {
        THROW("create canvas failed");
    }
}

GDICanvas::~GDICanvas()
{
    _canvas->ops->destroy(_canvas);
    _canvas = NULL;
    delete _pixmap;
    _pixmap = NULL;
}

void GDICanvas::copy_pixels(const QRegion& region, RedDrawable& dest_dc)
{
    pixman_box32_t *rects;
    int num_rects;

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

void GDICanvas::copy_pixels(const QRegion& region, RedDrawable* dest_dc, const PixmapHeader* pixmap)
{
    copy_pixels(region, *dest_dc);
}


CanvasType GDICanvas::get_pixmap_type()
{
    return CANVAS_TYPE_GDI;
}
