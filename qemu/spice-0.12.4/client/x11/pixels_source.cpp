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
#include "x_platform.h"
#include "pixels_source.h"
#include "pixels_source_p.h"
#include "utils.h"
#include "debug.h"
#include "res.h"


static void create_pixmap(const PixmapHeader* pixmap, PixelsSource_p& pixels_source,
                         pixman_format_code_t format)
{
    pixman_image_t *pixman_image;

    pixman_image = pixman_image_create_bits(format,
					    pixmap->width, pixmap->height,
					    (uint32_t *)pixmap->data,
					    pixmap->stride);
    if (pixman_image == NULL) {
      THROW("surf create failed");
    }

    pixels_source.type = PIXELS_SOURCE_TYPE_PIXMAP;
    pixels_source.pixmap.pixman_image = pixman_image;
    pixels_source.pixmap.x_image = NULL;
    pixels_source.pixmap.shminfo = NULL;
    if (format == PIXMAN_a8r8g8b8) {
        pixels_source.pixmap.format = RedDrawable::ARGB32;
    } else {
        pixels_source.pixmap.format = RedDrawable::RGB32;
    }
}

PixelsSource::PixelsSource()
{
    _origin.x = _origin.y = 0;
    memset(_opaque, 0, sizeof(_opaque));
}

PixelsSource::~PixelsSource()
{
}

ImageFromRes::ImageFromRes(int res_id)
{
    const PixmapHeader* pixmap = res_get_image(res_id);
    if (!pixmap) {
        THROW("no image %d", res_id);
    }
    create_pixmap(pixmap, *(PixelsSource_p*)get_opaque(), PIXMAN_x8r8g8b8);
}

ImageFromRes::~ImageFromRes()
{
    pixman_image_unref(((PixelsSource_p*)get_opaque())->pixmap.pixman_image);
}

SpicePoint ImageFromRes::get_size()
{
    pixman_image_t *image = ((PixelsSource_p*)get_opaque())->pixmap.pixman_image;
    SpicePoint pt;
    pt.x = pixman_image_get_width(image);
    pt.y = pixman_image_get_height(image);
    return pt;
}

AlphaImageFromRes::AlphaImageFromRes(int res_id)
{
    const PixmapHeader* pixmap = res_get_image(res_id);
    if (!pixmap) {
        THROW("no image %d", res_id);
    }
    create_pixmap(pixmap, *(PixelsSource_p*)get_opaque(), PIXMAN_a8r8g8b8);
}

AlphaImageFromRes::~AlphaImageFromRes()
{
    pixman_image_unref(((PixelsSource_p*)get_opaque())->pixmap.pixman_image);
}

SpicePoint AlphaImageFromRes::get_size()
{
    pixman_image_t *image = ((PixelsSource_p*)get_opaque())->pixmap.pixman_image;
    SpicePoint pt;
    pt.x = pixman_image_get_width(image);
    pt.y = pixman_image_get_height(image);
    return pt;
}
