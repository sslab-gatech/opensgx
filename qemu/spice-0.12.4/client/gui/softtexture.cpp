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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"

#include "softtexture.h"
#include "softrenderer.h"

#include "CEGUIImageCodec.h"
#include "CEGUISystem.h"
#include "CEGUIExceptions.h"

namespace CEGUI
{


SoftTexture::SoftTexture(Renderer* owner)
    : Texture (owner)
    , _surf (NULL)
    , _width (0)
    , _height (0)
{

}

SoftTexture::SoftTexture(Renderer* owner, uint size)
    : Texture (owner)
    , _surf (new uint32[size * size])
    , _width (size)
    , _height (size)
{

}

SoftTexture::SoftTexture(Renderer* owner, const String& filename,
                         const String& resourceGroup)
    : Texture (owner)
    , _surf (NULL)
    , _width (0)
    , _height (0)
{
    loadFromFile(filename, resourceGroup);
}


SoftTexture::~SoftTexture()
{
    freeSurf();
}

void SoftTexture::freeSurf()
{
    _width = _height = 0;
    delete[] _surf;
    _surf = NULL;
}

void SoftTexture::loadFromFile(const String& filename, const String& resourceGroup)
{
    freeSurf();
    SoftRenderer* renderer = static_cast<SoftRenderer*>(getRenderer());
    RawDataContainer texture_file;
    ResourceProvider* resource_provider = System::getSingleton().getResourceProvider();
    resource_provider->loadRawDataContainer(filename, texture_file, resourceGroup);
    ImageCodec *codec = renderer->getImageCodec();
    Texture* res = codec->load(texture_file, this);
    resource_provider->unloadRawDataContainer(texture_file);
    if (!res) {
        throw RendererException("load from file failed");
    }
}

void SoftTexture::loadFromMemory(const void* buffPtr, uint buffWidth,
                                 uint buffHeight,
                                 PixelFormat pixelFormat)
{
    freeSurf();
    _surf = new uint32[buffWidth * buffHeight];
    _width = buffWidth;
    _height = buffHeight;

    switch (pixelFormat) {
    case PF_RGBA: {
        const uint32_t *src = static_cast<const uint32_t *>(buffPtr);
        uint32* line = _surf;
        uint32* end_line = _surf + _width * _height;
        for (int i = 0; line != end_line; line += _width, i++) {
            uint32* pixel = line;
            uint32* end_pixel = pixel + _width;
            for (; pixel != end_pixel; pixel++, src++) {
                ((uint8_t*)pixel)[0] = ((uint8_t*)src)[2];
                ((uint8_t*)pixel)[1] = ((uint8_t*)src)[1];
                ((uint8_t*)pixel)[2] = ((uint8_t*)src)[0];
                ((uint8_t*)pixel)[3] = ((uint8_t*)src)[3];
            }
        }
        break;
    }
    case PF_RGB: {
        const uint8_t *src = static_cast<const uint8_t *>(buffPtr);
        uint32* line = _surf;
        uint32* end_line = _surf + _width * _height;
        for (int i = 0; line != end_line; line += _width, i++) {
            uint8* pixel = (uint8*)line;
            uint8* end_pixel = (uint8*)(line + _width);
            for (; pixel != end_pixel; pixel += 4, src += 3) {
                pixel[2] = src[0];
                pixel[1] = src[1];
                pixel[0] = src[2];
                pixel[3] = 0xff;
            }
        }
        break;
    }
    default:
        throw RendererException("invalid pixel format");
    }
}


}
