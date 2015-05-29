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

#ifndef _H_RED_DRAWABLE
#define _H_RED_DRAWABLE

#include "common/pixman_utils.h"
#include "pixels_source.h"
#include "utils.h"

typedef uint32_t rgb32_t;

static inline rgb32_t rgb32_make(uint8_t r, uint8_t g, uint8_t b)
{
    return (rgb32_t(r) << 16) | (rgb32_t(g) << 8) | b;
}

static inline uint8_t rgb32_get_red(rgb32_t color)
{
    return color >> 16;
}

static inline uint8_t rgb32_get_green(rgb32_t color)
{
    return color >> 8;
}

static inline uint8_t rgb32_get_blue(rgb32_t color)
{
    return color;
}

class RedDrawable: public PixelsSource {
public:
    RedDrawable() {}
    virtual ~RedDrawable() {}

    enum Format {
        ARGB32,
        RGB32,
        RGB16_555,
        RGB16_565,
        A1,
    };

    static int format_copy_compatible(Format src, Format dest) {
        return src == dest || (src == ARGB32 && dest == RGB32);
    }

    static int format_to_bpp(Format format) {
        if (format == RedDrawable::A1) {
            return 1;
        } else if (format == RGB16_555 || format == RGB16_565) {
            return 16;
        } else {
            return 32;
        }
    }

    static pixman_format_code_t format_to_pixman(Format format) {
            switch (format) {
            case RedDrawable::ARGB32:
                return PIXMAN_a8r8g8b8;
            case RedDrawable::RGB32:
                return PIXMAN_x8r8g8b8;
            case RedDrawable::RGB16_555:
                return PIXMAN_x1r5g5b5;
            case RedDrawable::RGB16_565:
                return PIXMAN_r5g6b5;
            case RedDrawable::A1:
                return PIXMAN_a1;
            default:
                THROW("unsupported format %d", format);
            }
    }

    static Format format_from_surface(uint32_t format) {
        switch (format) {
        case SPICE_SURFACE_FMT_16_555:
            return RedDrawable::RGB16_555;
        case SPICE_SURFACE_FMT_16_565:
            return RedDrawable::RGB16_565;
        case SPICE_SURFACE_FMT_32_xRGB:
            return RedDrawable::RGB32;
        case SPICE_SURFACE_FMT_32_ARGB:
            return RedDrawable::ARGB32;
        default:
            THROW("Unsupported RedPixman format");
        }
    }

    enum CombineOP {
        OP_COPY,
        OP_AND,
        OP_XOR,
    };

    virtual RedDrawable::Format get_format() = 0;
    void copy_pixels(const PixelsSource& src, int src_x, int src_y, const SpiceRect& dest);
    void blend_pixels(const PixelsSource& src, int src_x, int src_y, const SpiceRect& dest);
    void combine_pixels(const PixelsSource& src, int src_x, int src_y, const SpiceRect& dest,
                        CombineOP op);
    void fill_rect(const SpiceRect& rect, rgb32_t color);
    void frame_rect(const SpiceRect& rect, rgb32_t color);
    void erase_rect(const SpiceRect& rect, rgb32_t color);
};

#endif
