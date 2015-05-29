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

#ifndef _H_X_ICON
#define _H_X_ICON

#include <X11/Xlib.h>
#include <map>

#include "common.h"

#include "icon.h"

class XIcon: public Icon {
public:
    XIcon(int id, const IconHeader *icon);

    void get_pixmaps(int screen, Pixmap& pixmap, Pixmap& mask);

protected:
    virtual ~XIcon();

private:
    int _id;
    const IconHeader* _raw_icon;

    class ScreenIcon {
    public:
        ScreenIcon(Pixmap in_pixmap, Pixmap in_mask) : pixmap (in_pixmap), mask (in_mask) {}
        ScreenIcon() : pixmap (None), mask (None) {}

        Pixmap pixmap;
        Pixmap mask;
    };
    std::map<int, ScreenIcon> _screen_icons;
};

#endif
