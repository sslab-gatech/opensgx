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

#include "red_common.h"
#include "jpeg_encoder.h"
#include <jpeglib.h>

typedef struct JpegEncoder {
    JpegEncoderUsrContext *usr;

    struct jpeg_destination_mgr dest_mgr;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    struct {
        JpegEncoderImageType type;
        int width;
        int height;
        int stride;
        unsigned int out_size;
        void (*convert_line_to_RGB24) (uint8_t *line, int width, uint8_t **out_line);
    } cur_image;
} JpegEncoder;

/* jpeg destination manager callbacks */

static void dest_mgr_init_destination(j_compress_ptr cinfo)
{
    JpegEncoder *enc = (JpegEncoder *)cinfo->client_data;
    if (enc->dest_mgr.free_in_buffer == 0) {
        enc->dest_mgr.free_in_buffer = enc->usr->more_space(enc->usr,
                                                            &enc->dest_mgr.next_output_byte);

        if (enc->dest_mgr.free_in_buffer == 0) {
            spice_error("not enough space");
        }
    }

    enc->cur_image.out_size = enc->dest_mgr.free_in_buffer;
}

static boolean dest_mgr_empty_output_buffer(j_compress_ptr cinfo)
{
    JpegEncoder *enc = (JpegEncoder *)cinfo->client_data;
    enc->dest_mgr.free_in_buffer = enc->usr->more_space(enc->usr,
                                                        &enc->dest_mgr.next_output_byte);

    if (enc->dest_mgr.free_in_buffer == 0) {
        spice_error("not enough space");
    }
    enc->cur_image.out_size += enc->dest_mgr.free_in_buffer;
    return TRUE;
}

static void dest_mgr_term_destination(j_compress_ptr cinfo)
{
    JpegEncoder *enc = (JpegEncoder *)cinfo->client_data;
    enc->cur_image.out_size -= enc->dest_mgr.free_in_buffer;
}

JpegEncoderContext* jpeg_encoder_create(JpegEncoderUsrContext *usr)
{
    JpegEncoder *enc;
    if (!usr->more_space || !usr->more_lines) {
        return NULL;
    }

    enc = spice_new0(JpegEncoder, 1);

    enc->usr = usr;

    enc->dest_mgr.init_destination = dest_mgr_init_destination;
    enc->dest_mgr.empty_output_buffer = dest_mgr_empty_output_buffer;
    enc->dest_mgr.term_destination = dest_mgr_term_destination;

    enc->cinfo.err = jpeg_std_error(&enc->jerr);

    jpeg_create_compress(&enc->cinfo);
    enc->cinfo.client_data = enc;
    enc->cinfo.dest = &enc->dest_mgr;
    return (JpegEncoderContext*)enc;
}

void jpeg_encoder_destroy(JpegEncoderContext* encoder)
{
    jpeg_destroy_compress(&((JpegEncoder*)encoder)->cinfo);
    free(encoder);
}

static void convert_RGB16_to_RGB24(uint8_t *line, int width, uint8_t **out_line)
{
    uint16_t *src_line = (uint16_t *)line;
    uint8_t *out_pix;
    int x;

    spice_assert(out_line && *out_line);

    out_pix = *out_line;

    for (x = 0; x < width; x++) {
       uint16_t pixel = *src_line++;
       *out_pix++ = ((pixel >> 7) & 0xf8) | ((pixel >> 12) & 0x7);
       *out_pix++ = ((pixel >> 2) & 0xf8) | ((pixel >> 7) & 0x7);
       *out_pix++ = ((pixel << 3) & 0xf8) | ((pixel >> 2) & 0x7);
   }
}

static void convert_BGR24_to_RGB24(uint8_t *line, int width, uint8_t **out_line)
{
    int x;
    uint8_t *out_pix;

    spice_assert(out_line && *out_line);

    out_pix = *out_line;

    for (x = 0; x < width; x++) {
        *out_pix++ = line[2];
        *out_pix++ = line[1];
        *out_pix++ = line[0];
        line += 3;
    }
}

static void convert_BGRX32_to_RGB24(uint8_t *line, int width, uint8_t **out_line)
{
    uint32_t *src_line = (uint32_t *)line;
    uint8_t *out_pix;
    int x;

    spice_assert(out_line && *out_line);

    out_pix = *out_line;

    for (x = 0; x < width; x++) {
        uint32_t pixel = *src_line++;
        *out_pix++ = (pixel >> 16) & 0xff;
        *out_pix++ = (pixel >> 8) & 0xff;
        *out_pix++ = pixel & 0xff;
    }
}

static void convert_RGB24_to_RGB24(uint8_t *line, int width, uint8_t **out_line)
{
    *out_line = line;
}


#define FILL_LINES() {                                                  \
    if (lines == lines_end) {                                           \
        int n = jpeg->usr->more_lines(jpeg->usr, &lines);               \
        if (n <= 0) {                                                   \
            spice_error("more lines failed");                           \
        }                                                               \
        lines_end = lines + n * stride;                                 \
    }                                                                   \
}

static void do_jpeg_encode(JpegEncoder *jpeg, uint8_t *lines, unsigned int num_lines)
{
    uint8_t *lines_end;
    uint8_t *RGB24_line;
    int stride, width;
    JSAMPROW row_pointer[1];
    width = jpeg->cur_image.width;
    stride = jpeg->cur_image.stride;

    if (jpeg->cur_image.type != JPEG_IMAGE_TYPE_RGB24) {
        RGB24_line = (uint8_t *)spice_malloc(width*3);
    }

    lines_end = lines + (stride * num_lines);

    for (;jpeg->cinfo.next_scanline < jpeg->cinfo.image_height; lines += stride) {
        FILL_LINES();
        jpeg->cur_image.convert_line_to_RGB24(lines, width, &RGB24_line);
        row_pointer[0] = RGB24_line;
        jpeg_write_scanlines(&jpeg->cinfo, row_pointer, 1);
    }

    if (jpeg->cur_image.type != JPEG_IMAGE_TYPE_RGB24) {
        free(RGB24_line);
    }
}

int jpeg_encode(JpegEncoderContext *jpeg, int quality, JpegEncoderImageType type,
                int width, int height, uint8_t *lines, unsigned int num_lines, int stride,
                uint8_t *io_ptr, unsigned int num_io_bytes)
{
    JpegEncoder *enc = (JpegEncoder *)jpeg;

    enc->cur_image.type = type;
    enc->cur_image.width = width;
    enc->cur_image.height = height;
    enc->cur_image.stride = stride;
    enc->cur_image.out_size = 0;

    switch (type) {
    case JPEG_IMAGE_TYPE_RGB16:
        enc->cur_image.convert_line_to_RGB24 = convert_RGB16_to_RGB24;
        break;
    case JPEG_IMAGE_TYPE_RGB24:
        enc->cur_image.convert_line_to_RGB24 = convert_RGB24_to_RGB24;
        break;
    case JPEG_IMAGE_TYPE_BGR24:
        enc->cur_image.convert_line_to_RGB24 = convert_BGR24_to_RGB24;
        break;
    case JPEG_IMAGE_TYPE_BGRX32:
        enc->cur_image.convert_line_to_RGB24 = convert_BGRX32_to_RGB24;
        break;
    default:
        spice_error("bad image type");
    }

    enc->cinfo.image_width = width;
    enc->cinfo.image_height = height;
    enc->cinfo.input_components = 3;
    enc->cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&enc->cinfo);
    jpeg_set_quality(&enc->cinfo, quality, TRUE);

    enc->dest_mgr.next_output_byte = io_ptr;
    enc->dest_mgr.free_in_buffer = num_io_bytes;

    jpeg_start_compress(&enc->cinfo, TRUE);

    do_jpeg_encode(enc, lines, num_lines);

    jpeg_finish_compress(&enc->cinfo);
    return enc->cur_image.out_size;
}
