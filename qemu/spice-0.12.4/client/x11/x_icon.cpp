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

#include "x_icon.h"
#include "platform.h"
#include "x_platform.h"
#include "res.h"
#include "utils.h"
#include "debug.h"

typedef std::map<int, XIcon*> IconsMap;
static IconsMap res_icons;

XIcon::XIcon(int id, const IconHeader *icon)
    : _id (id)
    , _raw_icon (icon)
{
    res_icons[id] = this;
}

XIcon::~XIcon()
{
    Display* x_display = XPlatform::get_display();
    while (!_screen_icons.empty()) {
        std::map<int, ScreenIcon>::iterator iter = _screen_icons.begin();
        XFreePixmap(x_display, (*iter).second.pixmap);
        XFreePixmap(x_display, (*iter).second.mask);
        _screen_icons.erase(iter);
    }
    IconsMap::iterator iter = res_icons.find(_id);
    res_icons.erase(iter);
}

void XIcon::get_pixmaps(int screen, Pixmap& out_pixmap, Pixmap& out_mask)
{
    std::map<int, ScreenIcon>::iterator iter = _screen_icons.find(screen);
    if (iter != _screen_icons.end()) {
        out_pixmap = (*iter).second.pixmap;
        out_mask = (*iter).second.mask;
        return;
    }

    Display* x_display = XPlatform::get_display();
    Window root_window = RootWindow(x_display, screen);

    Pixmap pixmap = None;
    Pixmap mask = None;
    GC gc = NULL;
    try {
        XLockDisplay(x_display);
        pixmap = XCreatePixmap(x_display, root_window, _raw_icon->width, _raw_icon->height, 24);
        XUnlockDisplay(x_display);
        if (pixmap == None) {
            THROW("create pixmap failed");
        }

        XWindowAttributes attr;

        XLockDisplay(x_display);
        XGetWindowAttributes(x_display, root_window, &attr);
        XUnlockDisplay(x_display);

        XImage image;
        memset(&image, 0, sizeof(image));
        image.width = _raw_icon->width;
        image.height = _raw_icon->height;
        image.data = (char*)_raw_icon->pixmap;
        image.byte_order = LSBFirst;
        image.bitmap_unit = 32;
        image.bitmap_bit_order = LSBFirst;
        image.bitmap_pad = 32;
        image.bytes_per_line = _raw_icon->width * 4;
        image.depth = 24;
        image.format = ZPixmap;
        image.bits_per_pixel = 32;
        image.red_mask = 0x00ff0000;
        image.green_mask = 0x0000ff00;
        image.blue_mask = 0x000000ff;

        if (!XInitImage(&image)) {
            THROW("init image failed");
        }

        XGCValues gc_vals;
        gc_vals.function = GXcopy;
        gc_vals.foreground = ~0;
        gc_vals.background = 0;
        gc_vals.plane_mask = AllPlanes;

        XLockDisplay(x_display);
        gc = XCreateGC(x_display, pixmap, GCFunction | GCForeground | GCBackground | GCPlaneMask,
                       &gc_vals);
        XPutImage(x_display, pixmap, gc, &image, 0, 0, 0, 0, image.width, image.height);
        // HDG: why ?? XFlush should suffice
        XSync(x_display, False);
        XFreeGC(x_display, gc);
        gc = NULL;

        mask = XCreatePixmap(x_display, root_window, _raw_icon->width, _raw_icon->height, 1);
        XUnlockDisplay(x_display);
        if (mask == None) {
            THROW("create mask failed");
        }

        memset(&image, 0, sizeof(image));
        image.width = _raw_icon->width;
        image.height = _raw_icon->height;
        image.data = (char*)_raw_icon->mask;
        image.byte_order = LSBFirst;
        image.bitmap_unit = 8;
        image.bitmap_bit_order = MSBFirst;
        image.bitmap_pad = 8;
        image.bytes_per_line = _raw_icon->width / 8;
        image.depth = 1;
        image.format = XYBitmap;
        if (!XInitImage(&image)) {
            THROW("init image failed");
        }

        XLockDisplay(x_display);
        gc = XCreateGC(x_display, mask, GCFunction | GCForeground | GCBackground | GCPlaneMask,
                       &gc_vals);
        XPutImage(x_display, mask, gc, &image, 0, 0, 0, 0, image.width, image.height);
        // HDG: why ?? XFlush should suffice
        XSync(x_display, False);
        XUnlockDisplay(x_display);
        XFreeGC(x_display, gc);
    } catch (...) {
        if (gc) {
            XFreeGC(x_display, gc);
        }
        if (mask) {
            XFreePixmap(x_display, mask);
        }
        if (pixmap) {
            XFreePixmap(x_display, pixmap);
        }
        throw;
    }
    _screen_icons[screen] = ScreenIcon(pixmap, mask);
    out_pixmap = pixmap;
    out_mask = mask;
}

Icon* Platform::load_icon(int id)
{
    IconsMap::iterator iter = res_icons.find(id);
    if (iter != res_icons.end()) {
        return (*iter).second->ref();
    }

    const IconHeader *icon = res_get_icon(id);
    if (!icon) {
        return NULL;
    }
    XIcon *xicon = new XIcon(id, icon);
    return xicon->ref();
}
