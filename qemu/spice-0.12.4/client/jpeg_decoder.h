/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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

#ifndef _H_JPEG_DECODER
#define _H_JPEG_DECODER

#include "common.h"
#include "red_canvas_base.h"

#if defined(WIN32) && !defined(__MINGW32__)
/* We need some hacks to avoid warnings from the jpeg headers */
#define XMD_H
#undef FAR
#endif

extern "C" {
#include <jpeglib.h>
#ifdef HAVE_STDLIB_H
/* on mingw, there is a hack,
  and we also include config.h from spice-common, which redefine it */
#undef HAVE_STDLIB_H
#endif
}

class RGBConverter {
public:
    virtual ~RGBConverter() {}
    virtual void convert(uint8_t* src, uint8_t* dest, int width) = 0;
};

class RGBToBGRConverter : public RGBConverter {
public:
    void convert(uint8_t* src, uint8_t* dest, int width)
    {
        for (int x = 0; x < width; x++) {
            *dest++ = src[2];
            *dest++ = src[1];
            *dest++ = src[0];
            src += 3;
        }
    }
};

class RGBToBGRXConverter : public RGBConverter {
public:
    void convert(uint8_t* src, uint8_t* dest, int width)
    {
        for (int x = 0; x < width; x++) {
            *dest++ = src[2];
            *dest++ = src[1];
            *dest++ = src[0];
            *dest++ = 0;
            src += 3;
        }
    }
};

class JpegDecoder : public SpiceJpegDecoder {
public:
    JpegDecoder();
    ~JpegDecoder();

    void begin_decode(uint8_t* data, int data_size, int& out_width, int& out_height);
    /* format is SPICE_BITMAP_FMT_<X> for the dest; currently, only
       x=32BIT and x=24BIT are supported */
    void decode(uint8_t* dest, int stride, int format);

private:
    struct jpeg_decompress_struct _cinfo;
    struct jpeg_error_mgr _jerr;
    struct jpeg_source_mgr _jsrc;

    uint8_t* _data;
    int _data_size;
    int _width;
    int _height;

    RGBToBGRConverter _rgb2bgr;
    RGBToBGRXConverter _rgb2bgrx;
};
#endif
