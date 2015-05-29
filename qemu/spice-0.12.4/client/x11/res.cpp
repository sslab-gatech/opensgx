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
#include "resource.h"

#include "images/alt_image.c"

static const PixmapHeader alt_image = {
    (uint8_t *)_alt_image.pixel_data,
    _alt_image.width,
    _alt_image.height,
    _alt_image.width * 4,
};

typedef struct ResImage {
    int id;
    const PixmapHeader* image;
} ResImage;

static const ResImage res_image_map[] = {
    { ALT_IMAGE_RES_ID, &alt_image},
    {0, NULL},
};

const PixmapHeader *res_get_image(int id)
{
    const ResImage *now = res_image_map;
    for (; now->image; now++) {
        if (now->id == id) {
            return now->image;
        }
    }
    return NULL;
}

#include "images/red_icon.c"

static const IconHeader red_icon = {
    _red_icon.width,
    _red_icon.height,
    (uint8_t *)_red_icon.pixmap,
    (uint8_t *)_red_icon.mask,
};

typedef struct ResIcon {
    int id;
    const IconHeader* icon;
} ResIcon;

static const ResIcon res_icon_map[] = {
    { RED_ICON_RES_ID, &red_icon},
    {0, NULL},
};

const IconHeader *res_get_icon(int id)
{
    const ResIcon *now = res_icon_map;
    for (; now->icon; now++) {
        if (now->id == id) {
            return now->icon;
        }
    }
    return NULL;
}
