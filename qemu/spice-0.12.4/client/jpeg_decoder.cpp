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
#include "jpeg_decoder.h"
#include "debug.h"
#include "utils.h"

#if !defined(jpeg_boolean)
#define jpeg_boolean boolean
#endif

static void op_begin_decode(SpiceJpegDecoder *decoder,
                            uint8_t* data,
                            int data_size,
                            int* out_width,
                            int* out_height)
{
    JpegDecoder* _decoder = static_cast<JpegDecoder*>(decoder);
    _decoder->begin_decode(data, data_size, *out_width, *out_height);
}

static void op_decode(SpiceJpegDecoder *decoder,
                      uint8_t* dest,
                      int stride,
                      int format)
{
    JpegDecoder* _decoder = static_cast<JpegDecoder*>(decoder);
    _decoder->decode(dest, stride, format);
}

extern "C" {

    static void jpeg_decoder_init_source(j_decompress_ptr cinfo)
    {
    }

    static SPICE_GNUC_NORETURN jpeg_boolean jpeg_decoder_fill_input_buffer(j_decompress_ptr cinfo)
    {
        PANIC("no more data for jpeg");
    }

    static void jpeg_decoder_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
    {
        ASSERT(num_bytes < (long)cinfo->src->bytes_in_buffer);
        cinfo->src->next_input_byte += num_bytes;
        cinfo->src->bytes_in_buffer -= num_bytes;
    }

    static void jpeg_decoder_term_source (j_decompress_ptr cinfo)
    {
        return;
    }
}


JpegDecoder::JpegDecoder()
    : _data (NULL)
    , _data_size (0)
{
    _cinfo.err = jpeg_std_error(&_jerr);
    jpeg_create_decompress(&_cinfo);

    _cinfo.src = &_jsrc;
    _cinfo.src->init_source = jpeg_decoder_init_source;
    _cinfo.src->fill_input_buffer = jpeg_decoder_fill_input_buffer;
    _cinfo.src->skip_input_data = jpeg_decoder_skip_input_data;
    _cinfo.src->resync_to_restart = jpeg_resync_to_restart;
    _cinfo.src->term_source = jpeg_decoder_term_source;

    static SpiceJpegDecoderOps decoder_ops = {
        op_begin_decode,
        op_decode,
    };

    ops = &decoder_ops;
}

JpegDecoder::~JpegDecoder()
{
    jpeg_destroy_decompress(&_cinfo);
}

void JpegDecoder::begin_decode(uint8_t* data, int data_size, int& out_width, int& out_height)
{
    ASSERT(data);
    ASSERT(data_size);

    if (_data) {
        jpeg_abort_decompress(&_cinfo);
    }

    _data = data;
    _data_size = data_size;

    _cinfo.src->next_input_byte = _data;
    _cinfo.src->bytes_in_buffer = _data_size;

    jpeg_read_header(&_cinfo, TRUE);

    _cinfo.out_color_space = JCS_RGB;
    _width = _cinfo.image_width;
    _height = _cinfo.image_height;

    out_width = _width;
    out_height = _height;
}

void JpegDecoder::decode(uint8_t *dest, int stride, int format)
{
    uint8_t* scan_line = new uint8_t[_width*3];
    RGBConverter* rgb_converter;

    switch (format) {
    case SPICE_BITMAP_FMT_24BIT:
        rgb_converter = &_rgb2bgr;
        break;
    case SPICE_BITMAP_FMT_32BIT:
        rgb_converter = &_rgb2bgrx;
        break;
    default:
        THROW("bad bitmap format, %d", format);
    }

    jpeg_start_decompress(&_cinfo);

    for (int row = 0; row < _height; row++) {
        jpeg_read_scanlines(&_cinfo, &scan_line, 1);
        rgb_converter->convert(scan_line, dest, _width);
        dest += stride;
    }

    delete [] scan_line;

    jpeg_finish_decompress(&_cinfo);
}
