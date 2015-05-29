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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "debug.h"
#include "utils.h"
#include "mjpeg_decoder.h"

#if !defined(jpeg_boolean)
#define jpeg_boolean boolean
#endif

enum {
    STATE_READ_HEADER,
    STATE_START_DECOMPRESS,
    STATE_READ_SCANLINES,
    STATE_FINISH_DECOMPRESS
};

extern "C" {

    static void init_source(j_decompress_ptr cinfo)
    {
    }

    static jpeg_boolean fill_input_buffer(j_decompress_ptr cinfo)
    {
        return FALSE;
    }

    void mjpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
    {
        MJpegDecoder *decoder = (MJpegDecoder *)cinfo;
        if (num_bytes > 0) {
            if (cinfo->src->bytes_in_buffer >= (size_t)num_bytes) {
                cinfo->src->next_input_byte += (size_t) num_bytes;
                cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
            } else {
                decoder->_extra_skip = num_bytes - cinfo->src->bytes_in_buffer;
                cinfo->src->bytes_in_buffer = 0;
            }
        }
    }

    static void term_source (j_decompress_ptr cinfo)
    {
        return;
    }
}

MJpegDecoder::MJpegDecoder(int width, int height,
                           int stride,
                           uint8_t *frame,
                           bool back_compat) :
    _data(NULL)
    , _data_size(0)
    , _data_start(0)
    , _data_end(0)
    , _extra_skip(0)
    , _width(width)
    , _height(height)
    , _stride(stride)
    , _frame(frame)
    , _back_compat(back_compat)
    , _y(0)
    , _state(0)
{
    memset(&_cinfo, 0, sizeof(_cinfo));
    _cinfo.err = jpeg_std_error (&_jerr);
    jpeg_create_decompress (&_cinfo);

    _cinfo.src = &_jsrc;
    _cinfo.src->init_source = init_source;
    _cinfo.src->fill_input_buffer = fill_input_buffer;
    _cinfo.src->skip_input_data = mjpeg_skip_input_data;
    _cinfo.src->resync_to_restart = jpeg_resync_to_restart;
    _cinfo.src->term_source = term_source;

    _scanline = new uint8_t[width * 3];
}

MJpegDecoder::~MJpegDecoder()
{
    jpeg_destroy_decompress(&_cinfo);
    delete [] _scanline;
    if (_data) {
        delete [] _data;
    }
}

void MJpegDecoder::convert_scanline(void)
{
    uint32_t *row;
    uint32_t c;
    uint8_t *s;
    unsigned x;

    ASSERT(_width % 2 == 0);
    ASSERT(_height % 2 == 0);

   row = (uint32_t *)(_frame + _y * _stride);
    s = _scanline;


    if (_back_compat) {
        /* We need to check for the old major and for backwards compat
           a) swap r and b (done)
           b) to-yuv with right values and then from-yuv with old wrong values (TODO)
        */
        for (x = 0; x < _width; x++) {
            c = s[2] << 16 | s[1] << 8 | s[0];
            s += 3;
            *row++ = c;
        }
    } else {
        for (x = 0; x < _width; x++) {
            c = s[0] << 16 | s[1] << 8 | s[2];
            s += 3;
            *row++ = c;
        }
    }
}

void MJpegDecoder::append_data(uint8_t *data, size_t length)
{
    uint8_t *new_data;
    size_t data_len;

    if (length == 0) {
        return;
    }

    if (_data_size - _data_end < length) {
        /* Can't fits in tail, need to make space */

        data_len = _data_end - _data_start;
        if (_data_size - data_len < length) {
            /* Can't fit at all, grow a bit */
            _data_size = _data_size + length * 2;
            new_data = new uint8_t[_data_size];
            memcpy (new_data, _data + _data_start, data_len);
            delete [] _data;
            _data = new_data;
        } else {
            /* Just needs to compact */
            memmove (_data, _data + _data_start, data_len);
        }
        _data_start = 0;
        _data_end = data_len;
    }

    memcpy (_data + _data_end, data, length);
    _data_end += length;
}

bool MJpegDecoder::decode_data(uint8_t *data, size_t length)
{
    bool got_picture;
    int res;

    got_picture = false;

    if (_extra_skip > 0) {
        if (_extra_skip >= length) {
            _extra_skip -= length;
            return false;
        } else {
            data += _extra_skip;
            length -= _extra_skip;
            _extra_skip = 0;
        }
    }

    if (_data_end - _data_start == 0) {
        /* No current data, pass in without copy */

        _jsrc.next_input_byte = data;
        _jsrc.bytes_in_buffer = length;
    } else {
        /* Need to combine the new and old data */
        append_data(data, length);

        _jsrc.next_input_byte = _data + _data_start;
        _jsrc.bytes_in_buffer = _data_end - _data_start;
    }

    switch (_state) {
    case STATE_READ_HEADER:
        res = jpeg_read_header(&_cinfo, TRUE);
        if (res == JPEG_SUSPENDED) {
            break;
        }

        _cinfo.do_fancy_upsampling = FALSE;
        _cinfo.do_block_smoothing = FALSE;
        _cinfo.out_color_space = JCS_RGB;

        PANIC_ON(_cinfo.image_width != _width);
        PANIC_ON(_cinfo.image_height != _height);

        _state = STATE_START_DECOMPRESS;

        /* fall through */
    case STATE_START_DECOMPRESS:
        res = jpeg_start_decompress (&_cinfo);

        if (!res) {
            break;
        }

        _state = STATE_READ_SCANLINES;

        /* fall through */
    case STATE_READ_SCANLINES:
        res = 0;
        while (_y < _height) {
            res = jpeg_read_scanlines(&_cinfo, &_scanline, 1);

            if (res == 0) {
                break;
            }

            convert_scanline();
            _y++;
        }
        if (res == 0) {
            break;
        }

        _state = STATE_FINISH_DECOMPRESS;

        /* fall through */
    case STATE_FINISH_DECOMPRESS:
        res = jpeg_finish_decompress (&_cinfo);

        if (!res) {
            break;
        }

        _y = 0;
        _state = STATE_READ_HEADER;
        got_picture = true;

        break;
    }

    if (_jsrc.next_input_byte == data) {
        /* We read directly from the user, store remaining data in
           buffer for next time */
        size_t read_size = _jsrc.next_input_byte - data;

        append_data(data + read_size, length - read_size);
    } else {
        _data_start = _jsrc.next_input_byte - _data;
        _data_end = _data_start + _jsrc.bytes_in_buffer;
    }

    return got_picture;
}
