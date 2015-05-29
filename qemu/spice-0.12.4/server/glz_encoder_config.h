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

#ifndef _H_GLZ_ENCODER_CONFIG
#define _H_GLZ_ENCODER_CONFIG

#include <spice/macros.h>
#include "common/lz_common.h"

typedef void GlzUsrImageContext;
typedef struct GlzEncoderUsrContext GlzEncoderUsrContext;

struct GlzEncoderUsrContext {
    SPICE_GNUC_PRINTF(2, 3) void (*error)(GlzEncoderUsrContext *usr, const char *fmt, ...);
    SPICE_GNUC_PRINTF(2, 3) void (*warn)(GlzEncoderUsrContext *usr, const char *fmt, ...);
    SPICE_GNUC_PRINTF(2, 3) void (*info)(GlzEncoderUsrContext *usr, const char *fmt, ...);
    void    *(*malloc)(GlzEncoderUsrContext *usr, int size);
    void (*free)(GlzEncoderUsrContext *usr, void *ptr);

    // get the next chunk of the image which is entered to the dictionary. If the image is down to
    // top, return it from the last line to the first one (stride should always be positive)
    int (*more_lines)(GlzEncoderUsrContext *usr, uint8_t **lines);

    // get the next chunk of the compressed buffer.return number of bytes in the chunk.
    int (*more_space)(GlzEncoderUsrContext *usr, uint8_t **io_ptr);

    // called when an image is removed from the dictionary, due to the window size limit
    void (*free_image)(GlzEncoderUsrContext *usr, GlzUsrImageContext *image);

};


#ifdef DEBUG

#define GLZ_ASSERT(usr, x) \
    if (!(x)) (usr)->error(usr, "%s: ASSERT %s failed\n", __FUNCTION__, #x);

#else

#define GLZ_ASSERT(usr, x)

#endif

#define INLINE inline


#endif
