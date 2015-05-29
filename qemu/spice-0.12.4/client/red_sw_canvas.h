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

#ifndef _H_CCANVAS
#define _H_CCANVAS

#include "canvas.h"
#define SPICE_CANVAS_INTERNAL
#define SW_CANVAS_CACHE
#include "common/sw_canvas.h"
#undef SW_CANVAS_CACHE
#undef SPICE_CANVAS_INTERNAL

class RedPixmap;

class SCanvas: public Canvas {
public:
    SCanvas(bool onscreen,
            int width, int height, uint32_t format, RedWindow *win,
            PixmapCache& pixmap_cache, PaletteCache& palette_cache,
            GlzDecoderWindow &glz_decoder_window, SurfacesCache &csurfaces);
    virtual ~SCanvas();

    virtual void thread_touch() {}
    virtual void copy_pixels(const QRegion& region, RedDrawable* dc,
                             const PixmapHeader* pixmap);
    virtual void copy_pixels(const QRegion& region, RedDrawable& dc);

    virtual CanvasType get_pixmap_type();

private:
    RedPixmap *_pixmap;
};

#endif
