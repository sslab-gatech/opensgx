/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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
#ifndef _softtexture_h_
#define _softtexture_h_

#include <stdint.h>
/* CEGUI 0.6 bug, CEGUITexture.h doesn't include this, we need to */
#include <cstddef>

#include "CEGUIBase.h"
#include "CEGUITexture.h"

namespace CEGUI
{
    class SoftTexture : public Texture
    {
    public:
        SoftTexture(Renderer* owner);
        SoftTexture(Renderer* owner, uint size);
        SoftTexture(Renderer* owner, const String& filename,
                    const String& resourceGroup);
        virtual ~SoftTexture();

        virtual ushort getWidth(void) const { return _width;}
        virtual ushort getHeight(void) const { return _height;}

        virtual void loadFromFile(const String& filename, const String& resourceGroup);
        virtual void loadFromMemory(const void* buffPtr, uint buffWidth, uint buffHeight,
                                    PixelFormat pixelFormat);

    private:
        void freeSurf();

    private:
        uint32_t* _surf;
        ushort _width;
        ushort _height;

        friend class SoftRenderer;
    };
}

#endif
