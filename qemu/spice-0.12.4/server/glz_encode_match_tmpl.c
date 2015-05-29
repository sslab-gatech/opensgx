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

#define SHORT_PIX_IMAGE_DIST_LEVEL_1 64 //(1 << 6)
#define SHORT_PIX_IMAGE_DIST_LEVEL_2 16384 // (1 << 14)
#define SHORT_PIX_IMAGE_DIST_LEVEL_3 4194304 // (1 << 22)
#define FAR_PIX_IMAGE_DIST_LEVEL_1 256 // (1 << 8)
#define FAR_PIX_IMAGE_DIST_LEVEL_2 65536 // (1 << 16)
#define FAR_PIX_IMAGE_DIST_LEVEL_3 16777216 // (1 << 24)

/* if image_distance = 0, pixel_distance is the distance between the matching pixels.
  Otherwise, it is the offset from the beginning of the referred image */
#if defined(GLZ_ENCODE_MATCH) /* actually performing the encoding */
static INLINE void encode_match(Encoder *encoder, uint32_t image_distance,
                                size_t pixel_distance, size_t len)
#elif defined(GLZ_ENCODE_SIZE) /* compute the size of the encoding except for the match length*/
static INLINE int get_encode_ref_size(uint32_t image_distance, size_t pixel_distance)
#endif
{
#if defined(GLZ_ENCODE_SIZE)
    int encode_size;
#endif

#if defined(GLZ_ENCODE_MATCH)
    /* encoding the match length + Long/Short dist bit +  12 LSB pixels of pixel_distance*/
    if (len < 7) {
        if (pixel_distance < MAX_PIXEL_SHORT_DISTANCE) {
            encode(encoder, (uint8_t)((len << 5) + (pixel_distance & 0x0f)));
        } else {
            encode(encoder, (uint8_t)((len << 5) + 16 + (pixel_distance & 0x0f)));
        }
        encode(encoder, (uint8_t)((pixel_distance >> 4) & 255));
    } else {
        if (pixel_distance < MAX_PIXEL_SHORT_DISTANCE) {
            encode(encoder, (uint8_t)((7 << 5) + (pixel_distance & 0x0f)));
        } else {
            encode(encoder, (uint8_t)((7 << 5) + 16 + (pixel_distance & 0x0f)));
        }
        for (len -= 7; len >= 255; len -= 255) {
            encode(encoder, 255);
        }
        encode(encoder, (uint8_t)len);
        encode(encoder, (uint8_t)((pixel_distance >> 4) & 255));
    }
#endif


    /* encoding the rest of the pixel distance and the image_dist and its 2 control bits */

    /* The first 2 MSB bits indicate how many more bytes should be read for image dist */
    if (pixel_distance < MAX_PIXEL_SHORT_DISTANCE) {
        if (image_distance < SHORT_PIX_IMAGE_DIST_LEVEL_1) {
#if defined(GLZ_ENCODE_MATCH)
            encode(encoder, (uint8_t)(image_distance & 0x3f));
#elif defined(GLZ_ENCODE_SIZE)
            encode_size = 3;
#endif
        } else if (image_distance < SHORT_PIX_IMAGE_DIST_LEVEL_2) {
#if defined(GLZ_ENCODE_MATCH)
            encode(encoder, (uint8_t)((1 << 6) + (image_distance & 0x3f)));
            encode(encoder, (uint8_t)((image_distance >> 6) & 255));
#elif defined(GLZ_ENCODE_SIZE)
            encode_size = 4;
#endif
        } else if (image_distance < SHORT_PIX_IMAGE_DIST_LEVEL_3) {
#if defined(GLZ_ENCODE_MATCH)
            encode(encoder, (uint8_t)((1 << 7) + (image_distance & 0x3f)));
            encode(encoder, (uint8_t)((image_distance >> 6) & 255));
            encode(encoder, (uint8_t)((image_distance >> 14) & 255));
#elif defined(GLZ_ENCODE_SIZE)
            encode_size = 5;
#endif
        } else {
#if defined(GLZ_ENCODE_MATCH)
            encode(encoder, (uint8_t)((1 << 7) + (1 << 6) + (image_distance & 0x3f)));
            encode(encoder, (uint8_t)((image_distance >> 6) & 255));
            encode(encoder, (uint8_t)((image_distance >> 14) & 255));
            encode(encoder, (uint8_t)((image_distance >> 22) & 255));
#elif defined(GLZ_ENCODE_SIZE)
            encode_size = 6;
#endif
        }
    } else {
        /* the third MSB bit indicates if the pixel_distance is medium/long*/
        uint8_t long_dist_control = (pixel_distance < MAX_PIXEL_MEDIUM_DISTANCE) ? 0 : 32;
        if (image_distance == 0) {
#if defined(GLZ_ENCODE_MATCH)
            encode(encoder, (uint8_t)(long_dist_control + ((pixel_distance >> 12) & 31)));
#elif defined(GLZ_ENCODE_SIZE)
            encode_size = 3;
#endif
        } else if (image_distance < FAR_PIX_IMAGE_DIST_LEVEL_1) {
#if defined(GLZ_ENCODE_MATCH)
            encode(encoder,
                   (uint8_t)(long_dist_control + (1 << 6) + ((pixel_distance >> 12) & 31)));
            encode(encoder, (uint8_t)(image_distance & 255));
#elif defined(GLZ_ENCODE_SIZE)
            encode_size = 4;
#endif
        } else if (image_distance < FAR_PIX_IMAGE_DIST_LEVEL_2) {
#if defined(GLZ_ENCODE_MATCH)
            encode(encoder,
                   (uint8_t)(long_dist_control + (1 << 7) + ((pixel_distance >> 12) & 31)));
            encode(encoder, (uint8_t)(image_distance & 255));
            encode(encoder, (uint8_t)((image_distance >> 8) & 255));
#elif defined(GLZ_ENCODE_SIZE)
            encode_size = 5;
#endif
        } else {
#if defined(GLZ_ENCODE_MATCH)
            encode(encoder,
                   (uint8_t)(long_dist_control + (1 << 7) + (1 << 6) +
                                                                    ((pixel_distance >> 12) & 31)));
            encode(encoder, (uint8_t)(image_distance & 255));
            encode(encoder, (uint8_t)((image_distance >> 8) & 255));
            encode(encoder, (uint8_t)((image_distance >> 16) & 255));
#elif defined(GLZ_ENCODE_SIZE)
            encode_size = 6;
#endif
        }

        if (long_dist_control) {
#if defined(GLZ_ENCODE_MATCH)
            encode(encoder, (uint8_t)((pixel_distance >> 17) & 255));
#elif defined(GLZ_ENCODE_SIZE)
            encode_size++;
#endif
        }
    }

#if defined(GLZ_ENCODE_SIZE)
    return encode_size;
#endif
}

#undef GLZ_ENCODE_SIZE
#undef GLZ_ENCODE_MATCH
