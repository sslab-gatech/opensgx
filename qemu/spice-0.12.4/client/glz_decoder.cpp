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
#include "glz_decoder_config.h"
#include "glz_decoder.h"

static void op_decode (SpiceGlzDecoder *decoder,
                       uint8_t *data,
                       SpicePalette *plt,
                       void *usr_data)
{
    GlzDecoder* _decoder = static_cast<GlzDecoder*>(decoder);
    _decoder->decode(data, plt, usr_data);
}

GlzDecoder::GlzDecoder(GlzDecoderWindow &images_window,
                       GlzDecodeHandler &usr_handler, GlzDecoderDebug &debug_calls)
    : _images_window (images_window)
    , _usr_handler (usr_handler)
    , _debug_calls (debug_calls)
{
    static SpiceGlzDecoderOps decoder_ops = {
        op_decode,
    };
    ops = &decoder_ops;
}

GlzDecoder::~GlzDecoder()
{
}

void GlzDecoder::decode_header()
{
    uint32_t magic;
    int version;
    uint8_t tmp;
    int stride;

    magic = decode_32();
    if (magic != LZ_MAGIC) {
        _debug_calls.warn(std::string("bad magic\n"));
    }

    version = decode_32();
    if (version != LZ_VERSION) {
        _debug_calls.warn(std::string("bad version\n"));
    }

    tmp = *(_in_now++);

    _image.type = (LzImageType)(tmp & LZ_IMAGE_TYPE_MASK);
    _image.top_down = (tmp >> LZ_IMAGE_TYPE_LOG) ? true : false;
    _image.width = decode_32();
    _image.height = decode_32();
    stride = decode_32();

    if (IS_IMAGE_TYPE_PLT[_image.type]) {
        _image.gross_pixels = stride * PLT_PIXELS_PER_BYTE[_image.type] * _image.height;
    } else {
        _image.gross_pixels = _image.width * _image.height;
    }

    _image.id = decode_64();
    _image.win_head_dist = decode_32();
}

inline uint32_t GlzDecoder::decode_32()
{
    uint32_t word = 0;
    word |= *(_in_now++);
    word <<= 8;
    word |= *(_in_now++);
    word <<= 8;
    word |= *(_in_now++);
    word <<= 8;
    word |= *(_in_now++);
    return word;
}

inline uint64_t GlzDecoder::decode_64()
{
    uint64_t long_word = decode_32();
    long_word <<= 32;
    long_word |= decode_32();
    return long_word;
}

// TODO: the code is historically c based. Consider transforming to c++ and use templates
//      - but be sure it won't make it slower!

/*
 * Give hints to the compiler for branch prediction optimization.
 */
#if defined(__GNUC__) && (__GNUC__ > 2)
#define LZ_EXPECT_CONDITIONAL(c) (__builtin_expect((c), 1))
#define LZ_UNEXPECT_CONDITIONAL(c) (__builtin_expect((c), 0))
#else
#define LZ_EXPECT_CONDITIONAL(c) (c)
#define LZ_UNEXPECT_CONDITIONAL(c) (c)
#endif


#ifdef __GNUC__
#define ATTR_PACKED __attribute__ ((__packed__))
#else
#define ATTR_PACKED
#pragma pack(push)
#pragma pack(1)
#endif


/* the palette images will be treated as one byte pixels. Their width should be transformed
   accordingly.
*/
typedef struct ATTR_PACKED one_byte_pixel_t {
    uint8_t a;
} one_byte_pixel_t;

typedef struct ATTR_PACKED rgb32_pixel_t {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t pad;
} rgb32_pixel_t;

typedef struct ATTR_PACKED rgb24_pixel_t {
    uint8_t b;
    uint8_t g;
    uint8_t r;
} rgb24_pixel_t;

typedef uint16_t rgb16_pixel_t;

#ifndef __GNUC__
#pragma pack(pop)
#endif

#undef ATTR_PACKED

#define LZ_PLT
#include "glz_decode_tmpl.c"

#define LZ_PLT
#define PLT8
#define TO_RGB32
#include "glz_decode_tmpl.c"

#define LZ_PLT
#define PLT4_BE
#define TO_RGB32
#include "glz_decode_tmpl.c"

#define LZ_PLT
#define PLT4_LE
#define TO_RGB32
#include "glz_decode_tmpl.c"

#define LZ_PLT
#define PLT1_BE
#define TO_RGB32
#include "glz_decode_tmpl.c"

#define LZ_PLT
#define PLT1_LE
#define TO_RGB32
#include "glz_decode_tmpl.c"


#define LZ_RGB16
#include "glz_decode_tmpl.c"
#define LZ_RGB16
#define TO_RGB32
#include "glz_decode_tmpl.c"

#define LZ_RGB24
#include "glz_decode_tmpl.c"

#define LZ_RGB32
#include "glz_decode_tmpl.c"

#define LZ_RGB_ALPHA
#include "glz_decode_tmpl.c"

#undef LZ_UNEXPECT_CONDITIONAL
#undef LZ_EXPECT_CONDITIONAL

typedef size_t (*decode_function)(GlzDecoderWindow &window, uint8_t* in_buf,
                                  uint8_t *out_buf, int size,
                                  DecodedImageWinId image_win_id, SpicePalette *plt,
                                  GlzDecoderDebug  &debug_calls);

// ordered according to LZ_IMAGE_TYPE
const decode_function DECODE_TO_RGB32[] = {
    NULL,
    glz_plt1_le_to_rgb32_decode,
    glz_plt1_be_to_rgb32_decode,
    glz_plt4_le_to_rgb32_decode,
    glz_plt4_be_to_rgb32_decode,
    glz_plt8_to_rgb32_decode,
    glz_rgb16_to_rgb32_decode,
    glz_rgb32_decode,
    glz_rgb32_decode,
    glz_rgb32_decode
};

const decode_function DECODE_TO_SAME[] = {
    NULL,
    glz_plt_decode,
    glz_plt_decode,
    glz_plt_decode,
    glz_plt_decode,
    glz_plt_decode,
    glz_rgb16_decode,
    glz_rgb24_decode,
    glz_rgb32_decode,
    glz_rgb32_decode
};

void GlzDecoder::decode(uint8_t *data, SpicePalette *palette, void *opaque_usr_info)
{
    DecodedImageWinId image_window_id;
    GlzDecodedImage *decoded_image;
    size_t n_in_bytes_decoded;
    int bytes_per_pixel;
    LzImageType decoded_type;

    _in_start = data;
    _in_now = data;

    decode_header();

#ifdef GLZ_DECODE_TO_RGB32
    bytes_per_pixel = 4;

    if (_image.type == LZ_IMAGE_TYPE_RGBA) {
        decoded_type = LZ_IMAGE_TYPE_RGBA;
    } else {
        decoded_type = LZ_IMAGE_TYPE_RGB32;
    }

#else
    if (IS_IMAGE_TYPE_PLT[_image.type]) {
        GLZ_ASSERT(_debug_calls, !(_image.gross_pixels % PLT_PIXELS_PER_BYTE[_image.type]));
    }
    bytes_per_pixel = RGB_BYTES_PER_PIXEL[_image.type];
    decoded_type = _image.type;
#endif


    image_window_id = _images_window.pre_decode(_image.id, _image.id - _image.win_head_dist);

    decoded_image = _usr_handler.alloc_image(opaque_usr_info, _image.id,
                                             _image.id - _image.win_head_dist,
                                             decoded_type, _image.width, _image.height,
                                             _image.gross_pixels, bytes_per_pixel,
                                             _image.top_down);

    _image.data = decoded_image->get_data();

    // decode_by_type
#ifdef GLZ_DECODE_TO_RGB32
    n_in_bytes_decoded = DECODE_TO_RGB32[_image.type](_images_window, _in_now, _image.data,
                                                      _image.gross_pixels, image_window_id,
                                                      palette, _debug_calls);
#else
    n_in_bytes_decoded = DECODE_TO_SAME[_image.type](_images_window, _in_now, _image.data,
                                                     IS_IMAGE_TYPE_PLT[_image.type] ?
                                                     _image.gross_pixels /
                                                     PLT_PIXELS_PER_BYTE[_image.type] :
                                                     _image.gross_pixels,
                                                     image_window_id, palette, _debug_calls);
#endif

    _in_now += n_in_bytes_decoded;

    if (_image.type == LZ_IMAGE_TYPE_RGBA) {
        glz_rgb_alpha_decode(_images_window, _in_now, _image.data,
                             _image.gross_pixels, image_window_id, palette, _debug_calls);
    }

    _images_window.post_decode(decoded_image);
}
