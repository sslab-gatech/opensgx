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

#ifndef _H_GLZ_DECODER
#define _H_GLZ_DECODER

#include "common/lz_common.h"
#include "glz_decoder_config.h"
#include "glz_decoder_window.h"
#include "red_canvas_base.h"

class GlzDecodeHandler {
public:
    GlzDecodeHandler() {}
    virtual ~GlzDecodeHandler() {}

    /* Called by the decoder before the image decoding is starts. The
       user of the decoder should create GlzDecodedImage instance.
       If resources should be released when the image exits the Glz window,
       it should be handled in the instance dtor.

       opaque_usr_info: the data sent when GlzDecoder::decode was called
       gross_pixels   : number of pixels when considering the whole stride*/
    virtual GlzDecodedImage *alloc_image(void *opaque_usr_info, uint64_t image_id,
                                         uint64_t image_win_head_id, LzImageType type,
                                         int width, int height, int gross_pixels,
                                         int n_bytes_per_pixel, bool top_down) = 0;
};

/*
    This class implements the lz decoding algorithm
*/

class GlzDecoder : public SpiceGlzDecoder
{
public:
    GlzDecoder(GlzDecoderWindow &images_window, GlzDecodeHandler &usr_handler,
               GlzDecoderDebug &debug_calls);
    virtual ~GlzDecoder();

    /* Decodes the data and afterwards  calls GlzDecodeHandler::handle_decoded_image */
    void decode(uint8_t *data, SpicePalette *palette, void *opaque_usr_info);

private:
    void decode_header();
    uint32_t decode_32();
    uint64_t decode_64();

private:
    GlzDecoderWindow &_images_window;
    GlzDecodeHandler &_usr_handler;
    GlzDecoderDebug  &_debug_calls;

    uint8_t *_in_now;
    uint8_t *_in_start;

    struct {
        uint64_t id;
        LzImageType type;
        int width;
        int height;
        int gross_pixels;
        bool top_down;
        int win_head_dist;
        uint8_t *data;
    } _image;
};

#endif // _H_GLZ_DECODER
