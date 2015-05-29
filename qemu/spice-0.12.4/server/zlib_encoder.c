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

#include "red_common.h"
#include "zlib_encoder.h"
#include <zlib.h>

struct ZlibEncoder {
    ZlibEncoderUsrContext *usr;

    z_stream strm;
    int last_level;
};

ZlibEncoder* zlib_encoder_create(ZlibEncoderUsrContext *usr, int level)
{
    ZlibEncoder *enc;
    int z_ret;

    if (!usr->more_space || !usr->more_input) {
        return NULL;
    }

    enc = spice_new0(ZlibEncoder, 1);

    enc->usr = usr;

    enc->strm.zalloc = Z_NULL;
    enc->strm.zfree = Z_NULL;
    enc->strm.opaque = Z_NULL;

    z_ret = deflateInit(&enc->strm, level);
    enc->last_level = level;
    if (z_ret != Z_OK) {
        spice_printerr("zlib error");
        free(enc);
        return NULL;
    }

    return enc;
}

void zlib_encoder_destroy(ZlibEncoder *encoder)
{
    deflateEnd(&encoder->strm);
    free(encoder);
}

/* returns the total size of the encoded data */
int zlib_encode(ZlibEncoder *zlib, int level, int input_size,
                uint8_t *io_ptr, unsigned int num_io_bytes)
{
    int flush;
    int enc_size = 0;
    int out_size = 0;
    int z_ret;

    z_ret = deflateReset(&zlib->strm);

    if (z_ret != Z_OK) {
        spice_error("deflateReset failed");
    }

    zlib->strm.next_out = io_ptr;
    zlib->strm.avail_out = num_io_bytes;

    if (level != zlib->last_level) {
        if (zlib->strm.avail_out == 0) {
            zlib->strm.avail_out = zlib->usr->more_space(zlib->usr, &zlib->strm.next_out);
            if (zlib->strm.avail_out == 0) {
                spice_error("not enough space");
            }
        }
        z_ret = deflateParams(&zlib->strm, level, Z_DEFAULT_STRATEGY);
        if (z_ret != Z_OK) {
            spice_error("deflateParams failed");
        }
        zlib->last_level = level;
    }


    do {
        zlib->strm.avail_in = zlib->usr->more_input(zlib->usr, &zlib->strm.next_in);
        if (zlib->strm.avail_in <= 0) {
            spice_error("more input failed");
        }
        enc_size += zlib->strm.avail_in;
        flush = (enc_size == input_size) ?  Z_FINISH : Z_NO_FLUSH;
        while (1) {
            int deflate_size = zlib->strm.avail_out;
            z_ret = deflate(&zlib->strm, flush);
            spice_assert(z_ret != Z_STREAM_ERROR);
            out_size += deflate_size - zlib->strm.avail_out;
            if (zlib->strm.avail_out) {
                break;
            }

            zlib->strm.avail_out = zlib->usr->more_space(zlib->usr, &zlib->strm.next_out);
            if (zlib->strm.avail_out == 0) {
                spice_error("not enough space");
            }
        }
    } while (flush != Z_FINISH);

    spice_assert(z_ret == Z_STREAM_END);
    return out_size;
}
