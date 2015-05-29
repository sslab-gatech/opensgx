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

#ifndef _H_RED_PIXMAP_GDI
#define _H_RED_PIXMAP_GDI

#include "red_pixmap.h"

class RecurciveMutex;

class RedPixmapGdi: public RedPixmap {
public:
    RedPixmapGdi(int width, int height, Format format, bool top_bottom);
    HDC get_dc();
    void *get_memptr();
    ~RedPixmapGdi();
    RecurciveMutex& get_mutex();
};

#endif
