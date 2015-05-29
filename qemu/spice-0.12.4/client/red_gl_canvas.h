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

#ifndef _H_GCANVAS
#define _H_GCANVAS

#include "canvas.h"
#define SPICE_CANVAS_INTERNAL
#define SW_CANVAS_CACHE
#include "common/sw_canvas.h"
#include "common/gl_canvas.h"
#undef SW_CANVAS_CACHE
#undef SPICE_CANVAS_INTERNAL

#include "red_pixmap_gl.h"
#include "red_window.h"

class RedPixmapGL;

class GCanvas: public Canvas {
public:
    GCanvas(int width, int height, uint32_t format, RedWindow *win,
            RenderType rendertype,
            PixmapCache& pixmap_cache, PaletteCache& palette_cache,
            GlzDecoderWindow &glz_decoder_window, SurfacesCache &csurfaces);
    virtual ~GCanvas();

    void set_mode();
    void clear();
    void thread_touch() {}
    void copy_pixels(const QRegion& region, RedDrawable* dc,
                     const PixmapHeader* pixmap);
    void copy_pixels(const QRegion& region, RedDrawable& dc);
    virtual void textures_lost();
    virtual CanvasType get_pixmap_type();
    virtual void touch_context();
    virtual void pre_gl_copy();
    virtual void post_gl_copy();
    void touched_bbox(const SpiceRect *bbox);

private:
    void create_pixmap(int width, int height, RedWindow *win,
                       RenderType rendertype);
    void destroy_pixmap();
    void destroy();

private:
    RedPixmapGL *_pixmap;
    bool _textures_lost;
};

#endif
