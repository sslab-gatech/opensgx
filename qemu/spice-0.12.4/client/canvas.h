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

#ifndef _H_CANVAS
#define _H_CANVAS

#include <map>

#include "common/region.h"
#include "common/messages.h"
#include "common/canvas_utils.h"

#include "common.h"
#include "debug.h"
#include "cache.hpp"
#include "shared_cache.hpp"
#include "glz_decoded_image.h"
#include "glz_decoder.h"
#include "jpeg_decoder.h"
#include "zlib_decoder.h"

enum CanvasType {
    CANVAS_TYPE_INVALID,
    CANVAS_TYPE_SW,
    CANVAS_TYPE_GL,
    CANVAS_TYPE_GDI,
};

class PixmapCacheTreat {
public:
    static inline pixman_image_t *get(pixman_image_t *surf)
    {
        return pixman_image_ref(surf);
    }

    static inline void release(pixman_image_t *surf)
    {
        pixman_image_unref(surf);
    }

    static const char* name() { return "pixmap";}
};

class SpiceImageCacheBase;

typedef SharedCache<pixman_image_t, PixmapCacheTreat, 1024, SpiceImageCacheBase> PixmapCache;

class SpiceImageCacheBase {
public:
    SpiceImageCache base;

    static void op_put(SpiceImageCache *c, uint64_t id, pixman_image_t *surface)
    {
        PixmapCache* cache = reinterpret_cast<PixmapCache*>(c);
        cache->add(id, surface);
    }

    static void op_put_lossy(SpiceImageCache *c, uint64_t id, pixman_image_t *surface)
    {
        PixmapCache* cache = reinterpret_cast<PixmapCache*>(c);
        cache->add(id, surface, TRUE);
    }

    static void op_replace_lossy(SpiceImageCache *c, uint64_t id, pixman_image_t *surface)
    {
        PixmapCache* cache = reinterpret_cast<PixmapCache*>(c);
        cache->replace(id, surface);
    }

    static pixman_image_t* op_get(SpiceImageCache *c, uint64_t id)
    {
        PixmapCache* cache = reinterpret_cast<PixmapCache*>(c);
        return cache->get(id);
    }

    static pixman_image_t* op_get_lossless(SpiceImageCache *c, uint64_t id)
    {
        PixmapCache* cache = reinterpret_cast<PixmapCache*>(c);
        return cache->get_lossless(id);
    }

    SpiceImageCacheBase()
    {
        static SpiceImageCacheOps cache_ops = {
            op_put,
            op_get,
            op_put_lossy,
            op_replace_lossy,
            op_get_lossless
        };
        base.ops = &cache_ops;
    }
};


class CachedPalette {
public:
    CachedPalette(SpicePalette* palette)
        : _refs(1)
    {
        int size = sizeof(SpicePalette) + palette->num_ents * sizeof(uint32_t);
        CachedPalette **ptr = (CachedPalette **)new uint8_t[size + sizeof(CachedPalette *)];
        *ptr = this;
        _palette = (SpicePalette*)(ptr + 1);
        memcpy(_palette, palette, size);
    }

    CachedPalette* ref()
    {
        _refs++;
        return this;
    }

    void unref()
    {
        if (--_refs == 0) {
            delete this;
        }
    }

    static void unref(SpicePalette *pal)
    {
        CachedPalette **ptr = (CachedPalette **)pal;
        (*(ptr - 1))->unref();
    }

    SpicePalette* palette() { return _palette;}

private:
    ~CachedPalette()
    {
        delete[] (uint8_t *)((CachedPalette **)_palette - 1);
    }

private:
    int _refs;
    SpicePalette* _palette;
};

class PaletteCacheTreat {
public:
    static inline CachedPalette* get(CachedPalette* palette)
    {
        return palette->ref();
    }

    static inline void release(CachedPalette* palette)
    {
        palette->unref();
    }

    static const char* name() { return "palette";}
};

class SpicePaletteCacheBase;
typedef Cache<CachedPalette, PaletteCacheTreat, 1024, SpicePaletteCacheBase> PaletteCache;

class SpicePaletteCacheBase {
public:
    SpicePaletteCache base;

    static void op_put(SpicePaletteCache *c, SpicePalette *palette)
    {
        PaletteCache* cache = reinterpret_cast<PaletteCache*>(c);
        AutoRef<CachedPalette> cached_palette(new CachedPalette(palette));
        cache->add(palette->unique, *cached_palette);
    }

    static SpicePalette* op_get(SpicePaletteCache *c, uint64_t id)
    {
        PaletteCache* cache = reinterpret_cast<PaletteCache*>(c);
        return cache->get(id)->palette();
    }

    static void op_release (SpicePaletteCache *c,
                            SpicePalette *palette)
    {
        CachedPalette::unref(palette);
    }

    SpicePaletteCacheBase()
    {
        static SpicePaletteCacheOps cache_ops = {
            op_put,
            op_get,
            op_release
        };
        base.ops = &cache_ops;
    }
};


/* Lz decoder related classes */

class GlzDecodedSurface: public GlzDecodedImage {
public:
    GlzDecodedSurface(uint64_t id, uint64_t win_head_id, uint8_t *data, int size,
                      int bytes_per_pixel, pixman_image_t *surface)
        : GlzDecodedImage(id, win_head_id, data, size, bytes_per_pixel)
        , _surface (surface)
    {
        pixman_image_ref(_surface);
    }

    virtual ~GlzDecodedSurface()
    {
        pixman_image_unref(_surface);
    }

private:
    pixman_image_t *_surface;
};

class GlzDecodeSurfaceHandler: public GlzDecodeHandler {
public:
    virtual GlzDecodedImage *alloc_image(void *opaque_usr_info, uint64_t image_id,
                                         uint64_t image_win_head_id, LzImageType type,
                                         int width, int height, int gross_pixels,
                                         int n_bytes_per_pixel, bool top_down)
    {
        ASSERT(type == LZ_IMAGE_TYPE_RGB32 || type == LZ_IMAGE_TYPE_RGBA);

        pixman_image_t *surface =
            alloc_lz_image_surface((LzDecodeUsrData *)opaque_usr_info,
                                   type == LZ_IMAGE_TYPE_RGBA ? PIXMAN_a8r8g8b8 : PIXMAN_x8r8g8b8,
                                   width, height, gross_pixels, top_down);
        uint8_t *data = (uint8_t *)pixman_image_get_data(surface);
        if (!top_down) {
            data = data - (gross_pixels / height) * n_bytes_per_pixel * (height - 1);
        }

        return (new GlzDecodedSurface(image_id, image_win_head_id, data,
                                      gross_pixels, n_bytes_per_pixel, surface));
    }
};

/* TODO: unite with the window debug callbacks? */
class GlzDecoderCanvasDebug: public GlzDecoderDebug {
public:
    virtual SPICE_GNUC_NORETURN void error(const std::string& str)
    {
        throw Exception(str);
    }

    virtual void warn(const std::string& str)
    {
        LOG_WARN("%s", str.c_str());
    }

    virtual void info(const std::string& str)
    {
        LOG_INFO("%s", str.c_str());
    }
};

class Canvas;

typedef std::map<uint32_t, Canvas*> SurfacesCanvasesMap;

class SurfacesCache: public SpiceImageSurfaces, public SurfacesCanvasesMap {
public:
    SurfacesCache();
    bool exist(uint32_t surface_id);
};

class Canvas {
public:
    Canvas(PixmapCache& bits_cache, PaletteCache& palette_cache,
           GlzDecoderWindow &glz_decoder_window, SurfacesCache& csurfaces);
    virtual ~Canvas();

    virtual void copy_pixels(const QRegion& region, RedDrawable* dc,
                             const PixmapHeader* pixmap) = 0;
    virtual void copy_pixels(const QRegion& region, RedDrawable& dc) = 0;
    virtual void thread_touch() = 0;

    void clear();

    void draw_fill(SpiceMsgDisplayDrawFill& fill, int size);
    void draw_text(SpiceMsgDisplayDrawText& text, int size);
    void draw_opaque(SpiceMsgDisplayDrawOpaque& opaque, int size);
    void draw_copy(SpiceMsgDisplayDrawCopy& copy, int size);
    void draw_transparent(SpiceMsgDisplayDrawTransparent& transparent, int size);
    void draw_alpha_blend(SpiceMsgDisplayDrawAlphaBlend& alpha_blend, int size);
    void copy_bits(SpiceMsgDisplayCopyBits& copy_bits, int size);
    void draw_blend(SpiceMsgDisplayDrawBlend& blend, int size);
    void draw_blackness(SpiceMsgDisplayDrawBlackness& blackness, int size);
    void draw_whiteness(SpiceMsgDisplayDrawWhiteness& whiteness, int size);
    void draw_invers(SpiceMsgDisplayDrawInvers& invers, int size);
    void draw_rop3(SpiceMsgDisplayDrawRop3& rop3, int size);
    void draw_stroke(SpiceMsgDisplayDrawStroke& stroke, int size);
    void draw_composite(SpiceMsgDisplayDrawComposite& composite, int size);

    void put_image(
#ifdef WIN32
                   HDC dc,
#endif
                   const PixmapHeader& image,
                   const SpiceRect& dest, const QRegion* clip);

    virtual CanvasType get_pixmap_type() { return CANVAS_TYPE_INVALID; }

    virtual SpiceCanvas *get_internal_canvas() { return _canvas; }

protected:
    virtual void touched_bbox(const SpiceRect *bbox) {};

    PixmapCache& pixmap_cache() { return _pixmap_cache;}
    PaletteCache& palette_cache() { return _palette_cache;}
    SurfacesCache& surfaces_cache() { return _surfaces_cache;}

    GlzDecoder& glz_decoder() {return _glz_decoder;}
    JpegDecoder& jpeg_decoder() { return _jpeg_decoder;}
    ZlibDecoder& zlib_decoder() { return _zlib_decoder;}

private:
    void begin_draw(SpiceMsgDisplayBase& base, int size, size_t min_size);

protected:
    SpiceCanvas* _canvas;

private:
    PixmapCache& _pixmap_cache;
    PaletteCache& _palette_cache;

    GlzDecodeSurfaceHandler _glz_handler;
    GlzDecoderCanvasDebug _glz_debug;
    GlzDecoder _glz_decoder;

    JpegDecoder _jpeg_decoder;
    ZlibDecoder _zlib_decoder;

    SurfacesCache& _surfaces_cache;
};


#endif
