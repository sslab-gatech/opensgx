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
#include "red_pixmap_sw.h"
#include "debug.h"
#include "utils.h"
#include "pixels_source_p.h"
#include "x_platform.h"

RedPixmapSw::RedPixmapSw(int width, int height, RedDrawable::Format format,
                         bool top_bottom, RedWindow *win)
    : RedPixmap(width, height, format, top_bottom)
{
    ASSERT(format == RedDrawable::ARGB32 || format == RedDrawable::RGB32 ||
           format == RedDrawable::RGB16_555 || format == RedDrawable::RGB16_565 ||
           format == RedDrawable::A1);
    ASSERT(sizeof(RedDrawable_p) <= PIXELES_SOURCE_OPAQUE_SIZE);
    pixman_image_t *pixman_image;
    XImage *image;
    XShmSegmentInfo *shminfo;
    _data = NULL;
    XVisualInfo *vinfo;
    int screen_num;
    RedDrawable::Format screen_format;

    screen_num = win ? win->get_screen_num() : 0;
    vinfo = XPlatform::get_vinfo()[screen_num];
    screen_format = XPlatform::get_screen_format(screen_num);

    image = NULL;
    shminfo = NULL;

    /* Only create XImage if same format as screen (needs re-verifying at
       draw time!)  */
    if (RedDrawable::format_copy_compatible(format, screen_format) ||
        format == A1) {
        image = XPlatform::create_x_image(format, width, height,
                                          vinfo->depth, vinfo->visual,
                                          &shminfo);
        _stride = image->bytes_per_line;
        _data = (uint8_t *)image->data;
    } else {
        _data = new uint8_t[height * _stride];
    }

    pixman_image = pixman_image_create_bits(RedDrawable::format_to_pixman(format),
                                            _width, _height,
                                            (uint32_t *)_data, _stride);
    if (pixman_image == NULL) {
        THROW("surf create failed");
    }

    ((PixelsSource_p*)get_opaque())->type = PIXELS_SOURCE_TYPE_PIXMAP;
    ((PixelsSource_p*)get_opaque())->pixmap.shminfo = shminfo;
    ((PixelsSource_p*)get_opaque())->pixmap.x_image = image;
    ((PixelsSource_p*)get_opaque())->pixmap.pixman_image = pixman_image;
    ((PixelsSource_p*)get_opaque())->pixmap.format = format;
}

RedPixmapSw::~RedPixmapSw()
{
    ASSERT(((PixelsSource_p*)get_opaque())->type == PIXELS_SOURCE_TYPE_PIXMAP);

    XShmSegmentInfo *shminfo = ((PixelsSource_p*)get_opaque())->pixmap.shminfo;
    XImage *image = ((PixelsSource_p*)get_opaque())->pixmap.x_image;

    pixman_image_unref(((PixelsSource_p*)get_opaque())->pixmap.pixman_image);

    if (image) {
        XPlatform::free_x_image(image, shminfo);
    } else {
        delete[] _data;
    }
}
