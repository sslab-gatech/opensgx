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

#ifndef _H_PIXELS_SOURCE
#define _H_PIXELS_SOURCE

#include "common/draw.h"

#define PIXELES_SOURCE_OPAQUE_SIZE (20 * sizeof(void*))

class PixelsSource {
public:
    PixelsSource();
    virtual ~PixelsSource();

    virtual SpicePoint get_size() = 0;
    void set_origin(int x, int y) { _origin.x = x; _origin.y = y;}
    const SpicePoint& get_origin() { return _origin;}

protected:
    const uint8_t* get_opaque() const { return _opaque;}

private:
    SpicePoint _origin;
    uint8_t _opaque[PIXELES_SOURCE_OPAQUE_SIZE];

    friend class RedDrawable;
};

class ImageFromRes: public PixelsSource {
public:
    ImageFromRes(int res_id);
    virtual ~ImageFromRes();
    virtual SpicePoint get_size();
};

class AlphaImageFromRes: public PixelsSource {
public:
    AlphaImageFromRes(int res_id);
    virtual ~AlphaImageFromRes();
    virtual SpicePoint get_size();
};

#endif
