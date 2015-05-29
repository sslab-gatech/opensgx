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

#include <pthread.h>
#include <stdio.h>
#include "glz_encoder.h"
#include "glz_encoder_dictionary_protected.h"


/* Holds a specific data for one encoder, and data that is relevant for the current image encoded */
typedef struct Encoder {
    GlzEncoderUsrContext *usr;
    uint8_t id;
    SharedDictionary     *dict;

    struct {
        LzImageType type;
        uint32_t id;
        uint32_t first_win_seg;
    } cur_image;

    struct {
        uint8_t            *start;
        uint8_t            *now;
        uint8_t            *end;
        size_t bytes_count;
        uint8_t            *last_copy;  // pointer to the last byte in which copy count was written
    } io;
} Encoder;


/**************************************************************************
* Handling writing the encoded image to the output buffer
***************************************************************************/
static INLINE int more_io_bytes(Encoder *encoder)
{
    uint8_t *io_ptr;
    int num_io_bytes = encoder->usr->more_space(encoder->usr, &io_ptr);
    encoder->io.bytes_count += num_io_bytes;
    encoder->io.now = io_ptr;
    encoder->io.end = encoder->io.now + num_io_bytes;
    return num_io_bytes;
}

static INLINE void encode(Encoder *encoder, uint8_t byte)
{
    if (encoder->io.now == encoder->io.end) {
        if (more_io_bytes(encoder) <= 0) {
            encoder->usr->error(encoder->usr, "%s: no more bytes\n", __FUNCTION__);
        }
        GLZ_ASSERT(encoder->usr, encoder->io.now);
    }

    GLZ_ASSERT(encoder->usr, encoder->io.now < encoder->io.end);
    *(encoder->io.now++) = byte;
}

static INLINE void encode_32(Encoder *encoder, unsigned int word)
{
    encode(encoder, (uint8_t)(word >> 24));
    encode(encoder, (uint8_t)(word >> 16) & 0x0000ff);
    encode(encoder, (uint8_t)(word >> 8) & 0x0000ff);
    encode(encoder, (uint8_t)(word & 0x0000ff));
}

static INLINE void encode_64(Encoder *encoder, uint64_t word)
{
    encode_32(encoder, (uint32_t)(word >> 32));
    encode_32(encoder, (uint32_t)(word & 0xffffff));
}

static INLINE void encode_copy_count(Encoder *encoder, uint8_t copy_count)
{
    encode(encoder, copy_count);
    encoder->io.last_copy = encoder->io.now - 1; // io_now cannot be the first byte of the buffer
}

static INLINE void update_copy_count(Encoder *encoder, uint8_t copy_count)
{
    GLZ_ASSERT(encoder->usr, encoder->io.last_copy);
    *(encoder->io.last_copy) = copy_count;
}

// decrease the io ptr by 1
static INLINE void compress_output_prev(Encoder *encoder)
{
    // io_now cannot be the first byte of the buffer
    encoder->io.now--;
    // the function should be called only when copy count is written unnecessarily by glz_compress
    GLZ_ASSERT(encoder->usr, encoder->io.now == encoder->io.last_copy)
}

static int encoder_reset(Encoder *encoder, uint8_t *io_ptr, uint8_t *io_ptr_end)
{
    GLZ_ASSERT(encoder->usr, io_ptr <= io_ptr_end);
    encoder->io.bytes_count = io_ptr_end - io_ptr;
    encoder->io.start = io_ptr;
    encoder->io.now = io_ptr;
    encoder->io.end = io_ptr_end;
    encoder->io.last_copy = NULL;

    return TRUE;
}

/**********************************************************
*           Encoding
***********************************************************/

GlzEncoderContext *glz_encoder_create(uint8_t id, GlzEncDictContext *dictionary,
                                      GlzEncoderUsrContext *usr)
{
    Encoder *encoder;

    if (!usr || !usr->error || !usr->warn || !usr->info || !usr->malloc ||
        !usr->free || !usr->more_space) {
        return NULL;
    }

    if (!(encoder = (Encoder *)usr->malloc(usr, sizeof(Encoder)))) {
        return NULL;
    }

    encoder->id = id;
    encoder->usr = usr;
    encoder->dict = (SharedDictionary *)dictionary;

    return (GlzEncoderContext *)encoder;
}

void glz_encoder_destroy(GlzEncoderContext *opaque_encoder)
{
    Encoder *encoder = (Encoder *)opaque_encoder;

    if (!opaque_encoder) {
        return;
    }

    encoder->usr->free(encoder->usr, encoder);
}

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


typedef uint8_t BYTE;

typedef struct __attribute__ ((__packed__)) one_byte_pixel_t {
    BYTE a;
} one_byte_pixel_t;

typedef struct __attribute__ ((__packed__)) rgb32_pixel_t {
    BYTE b;
    BYTE g;
    BYTE r;
    BYTE pad;
} rgb32_pixel_t;

typedef struct __attribute__ ((__packed__)) rgb24_pixel_t {
    BYTE b;
    BYTE g;
    BYTE r;
} rgb24_pixel_t;

typedef uint16_t rgb16_pixel_t;

#define BOUND_OFFSET 2
#define LIMIT_OFFSET 6
#define MIN_FILE_SIZE 4

#define MAX_PIXEL_SHORT_DISTANCE 4096       // (1 << 12)
#define MAX_PIXEL_MEDIUM_DISTANCE 131072    // (1 << 17)  2 ^ (12 + 5)
#define MAX_PIXEL_LONG_DISTANCE 33554432    // (1 << 25)  2 ^ (12 + 5 + 8)
#define MAX_IMAGE_DIST 16777215             // (1 << 24 - 1)


//#define DEBUG_ENCODE


#define GLZ_ENCODE_SIZE
#include "glz_encode_match_tmpl.c"
#define GLZ_ENCODE_MATCH
#include "glz_encode_match_tmpl.c"

#define LZ_PLT
#include "glz_encode_tmpl.c"

#define LZ_RGB16
#include "glz_encode_tmpl.c"

#define LZ_RGB24
#include "glz_encode_tmpl.c"

#define LZ_RGB32
#include "glz_encode_tmpl.c"

#define LZ_RGB_ALPHA
#include "glz_encode_tmpl.c"


int glz_encode(GlzEncoderContext *opaque_encoder,
               LzImageType type, int width, int height, int top_down,
               uint8_t *lines, unsigned int num_lines, int stride,
               uint8_t *io_ptr, unsigned int num_io_bytes,
               GlzUsrImageContext *usr_context, GlzEncDictImageContext **o_enc_dict_context)
{
    Encoder *encoder = (Encoder *)opaque_encoder;
    WindowImage *dict_image;
    uint8_t *io_ptr_end = io_ptr + num_io_bytes;
    uint32_t win_head_image_dist;

    if (IS_IMAGE_TYPE_PLT[type]) {
        if (stride > (width / PLT_PIXELS_PER_BYTE[type])) {
            if (((width % PLT_PIXELS_PER_BYTE[type]) == 0) || (
                    (stride - (width / PLT_PIXELS_PER_BYTE[type])) > 1)) {
                encoder->usr->error(encoder->usr, "stride overflows (plt)\n");
            }
        }
    } else {
        if (stride != width * RGB_BYTES_PER_PIXEL[type]) {
            encoder->usr->error(encoder->usr, "stride != width*bytes_per_pixel (rgb)\n");
        }
    }

    // assign the output buffer
    if (!encoder_reset(encoder, io_ptr, io_ptr_end)) {
        encoder->usr->error(encoder->usr, "lz encoder io reset failed\n");
    }

    // first read the list of the image segments into the dictionary window
    dict_image = glz_dictionary_pre_encode(encoder->id, encoder->usr,
                                           encoder->dict, type, width, height, stride,
                                           lines, num_lines, usr_context, &win_head_image_dist);
    *o_enc_dict_context = (GlzEncDictImageContext *)dict_image;

    encoder->cur_image.type = type;
    encoder->cur_image.id = dict_image->id;
    encoder->cur_image.first_win_seg = dict_image->first_seg;

    encode_32(encoder, LZ_MAGIC);
    encode_32(encoder, LZ_VERSION);
    if (top_down) {
        encode(encoder, (type & LZ_IMAGE_TYPE_MASK) | (1 << LZ_IMAGE_TYPE_LOG));
    } else {
        encode(encoder, (type & LZ_IMAGE_TYPE_MASK));
    }

    encode_32(encoder, width);
    encode_32(encoder, height);
    encode_32(encoder, stride);
    encode_64(encoder, dict_image->id);
    encode_32(encoder, win_head_image_dist);

    switch (encoder->cur_image.type) {
    case LZ_IMAGE_TYPE_PLT1_BE:
    case LZ_IMAGE_TYPE_PLT1_LE:
    case LZ_IMAGE_TYPE_PLT4_BE:
    case LZ_IMAGE_TYPE_PLT4_LE:
    case LZ_IMAGE_TYPE_PLT8:
        glz_plt_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_RGB16:
        glz_rgb16_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_RGB24:
        glz_rgb24_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_RGB32:
        glz_rgb32_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_RGBA:
        glz_rgb32_compress(encoder);
        glz_rgb_alpha_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_INVALID:
    default:
        encoder->usr->error(encoder->usr, "bad image type\n");
    }

    glz_dictionary_post_encode(encoder->id, encoder->usr, encoder->dict);

    // move all the used segments to the free ones
    encoder->io.bytes_count -= (encoder->io.end - encoder->io.now);

    return encoder->io.bytes_count;
}
