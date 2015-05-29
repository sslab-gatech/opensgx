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

#ifndef _H_GLZ_DECODED_IMAGE
#define _H_GLZ_DECODED_IMAGE

#include "common.h"
#include "glz_decoder_config.h"

/*
    This class represents an image the lz window holds. It is created after the decoding of the
    image is completed, and destroyed when it exits the window.
*/

class GlzDecodedImage {
public:

    GlzDecodedImage(uint64_t id, uint64_t win_head_id, uint8_t *data, int size,
                    int bytes_per_pixel)
        : _id (id)
        , _win_head_id (win_head_id)
        , _data (data)
        , _bytes_per_pixel (bytes_per_pixel)
        , _size (size) {}

    virtual ~GlzDecodedImage() {}
    uint8_t *get_data() {return _data;}
    uint8_t *get_pixel_ref(int offset); // palette pix_id = byte count
    uint64_t get_id() {return _id;}
    uint64_t get_window_head_id() {return _win_head_id;}
    int      get_size() {return _size;}

protected:
    uint64_t _id;
    uint64_t _win_head_id;
    uint8_t *_data;
    int _bytes_per_pixel;  // if image is with palette pixel=byte
    int _size;             // number of pixels
};

inline uint8_t* GlzDecodedImage::get_pixel_ref(int offset)
{
    if (!_data) {
        return NULL;
    } else {
        return (_data + (offset * _bytes_per_pixel));
    }
}

#endif
