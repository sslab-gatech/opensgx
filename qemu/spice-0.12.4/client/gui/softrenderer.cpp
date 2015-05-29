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
#include "utils.h"
#include "debug.h"

#include "softrenderer.h"
#include "softtexture.h"

#include "CEGUIExceptions.h"
#include "CEGUIImageCodec.h"
#include "CEGUIDynamicModule.h"
#include "CEGUIEventArgs.h"

#define S_(X) #X
#define STRINGIZE(X) S_(X)

namespace CEGUI {

SoftRenderer::SoftRenderer(uint8_t* surface, uint width, uint height, uint stride,
                           ImageCodec* codec)
    : _surface (surface)
    , _width (width)
    , _height (height)
    , _image_codec (codec)
    , _image_codec_module (NULL)
    , _queueing(true)
{
    assert(stride == width * 4); //for now
    if (!_image_codec) {
        setupImageCodec();
    }
}

SoftRenderer::~SoftRenderer()
{
    destroyAllTextures();
    cleanupImageCodec();
}


void SoftRenderer::reset_surface(uint8_t* surface, uint width, uint height, uint stride)
{
    assert(stride == width * 4); //for now
    _surface = surface;
    _width = width;
    _height = height;

    EventArgs args;
    fireEvent(EventDisplaySizeChanged, args, EventNamespace);
}

#if defined(CEGUI_STATIC)
extern "C" CEGUI::ImageCodec* createImageCodec(void);
extern "C" void destroyImageCodec(CEGUI::ImageCodec*);
#endif

void SoftRenderer::setupImageCodec()
{
#if defined(CEGUI_STATIC)
    _destroy_image_codec = destroyImageCodec;
    _image_codec = createImageCodec();
#else
    String _default_codec_name(STRINGIZE(TGAImageCodec/*CEGUI_DEFAULT_IMAGE_CODEC*/));
    DynamicModule* module = NULL;

    try {
        DynamicModule* module = new DynamicModule(String("CEGUI") + _default_codec_name);

        _destroy_image_codec = (void(*)(ImageCodec*))module->getSymbolAddress("destroyImageCodec");

        if (!_destroy_image_codec) {
            throw GenericException("Missing destroyImageCodec symbol");
        }

        ImageCodec* (*create_f)(void);
        create_f = (ImageCodec* (*)(void))module->getSymbolAddress("createImageCodec");

        if (!create_f) {
            throw GenericException("Missing createImageCodec symbol");
        }

        _image_codec = create_f();
    } catch (...) {
        delete module;
        throw;
    }
    _image_codec_module = module;
#endif
}

void SoftRenderer::cleanupImageCodec()
{
    _destroy_image_codec(_image_codec);
    delete _image_codec_module;
}

static inline uint8_t calac_pixel(uint64_t c1, uint64_t c2, uint64_t c3, uint64_t a_mul)
{
    //(c' * c" * a' * a" + c"' * 255 ^ 3 - c"' * a' * a" * 255) / 255^4

    return uint8_t((c1 * c2 * a_mul + c3 * 255 * 255 * 255 - c3 * a_mul * 255) / (255 * 255 * 255));
}

inline void SoftRenderer::componnentAtPoint(int x_pos, int y_pos,
                                            int top_left, int top_right,
                                            int bottom_left, int bottom_right,
                                            uint64_t& comp)
{
    int a = top_left + (((x_pos * (top_right - top_left)) + (1 << 15)) >> 16);
    int b = bottom_left + (((x_pos * (bottom_right - bottom_left)) + (1 << 15)) >> 16);
    comp = a + (((b - a) * y_pos + (1 << 15)) >> 16);
}

void SoftRenderer::colourAtPoint(int x, int x_max, int y, int y_max,
                                 const ColourIRect& colours,
                                 uint64_t& r, uint64_t& g,
                                 uint64_t& b, uint64_t& a)
{
    int x_pos = (x << 16) / x_max;
    int y_pos = (y << 16) / y_max;


    componnentAtPoint(x_pos, y_pos, colours.top_left.r, colours.top_right.r,
                      colours.bottom_left.r, colours.bottom_right.r, r);
    componnentAtPoint(x_pos, y_pos, colours.top_left.g, colours.top_right.g,
                      colours.bottom_left.g, colours.bottom_right.g, g);
    componnentAtPoint(x_pos, y_pos, colours.top_left.b, colours.top_right.b,
                      colours.bottom_left.b, colours.bottom_right.b, b);
    componnentAtPoint(x_pos, y_pos, colours.top_left.a, colours.top_right.a,
                      colours.bottom_left.a, colours.bottom_right.a, a);
}

void SoftRenderer::renderQuadWithColourRect(const QuadInfo& quad)
{
    uint32_t* src = quad.tex->_surf + quad.tex_src.top * (int)quad.tex->getWidth();
    src += quad.tex_src.left;


    int src_width = quad.tex_src.right - quad.tex_src.left;
    int src_height = quad.tex_src.bottom - quad.tex_src.top;

    int dest_width = quad.dest.right - quad.dest.left;
    int dest_height = quad.dest.bottom - quad.dest.top;


    uint32_t x_scale = (src_width << 16) / dest_width;
    uint32_t y_scale = (src_height << 16) / dest_height;

    uint32_t* line = (uint32_t*)_surface + quad.dest.top * _width;
    line += quad.dest.left;

    for (int i = 0; i < dest_height; line += _width, i++) {
        uint32_t* pix = line;
        uint32_t* src_line = src + (((i * y_scale) + (1 << 15)) >> 16) * (int)quad.tex->getWidth();

        for (int j = 0; j < dest_width; pix++, j++) {
            uint64_t r;
            uint64_t g;
            uint64_t b;
            uint64_t a;

            colourAtPoint(j, dest_width, i, dest_height, quad.colors, r, g, b, a);

            uint8_t* tex_pix = (uint8_t*)&src_line[(((j * x_scale)+ (1 << 15)) >> 16)];
            uint64_t a_mul = a * tex_pix[3];

            ((uint8_t *)pix)[0] = calac_pixel(tex_pix[0], b, ((uint8_t *)pix)[0], a_mul);
            ((uint8_t *)pix)[1] = calac_pixel(tex_pix[1], g, ((uint8_t *)pix)[1], a_mul);
            ((uint8_t *)pix)[2] = calac_pixel(tex_pix[2], r, ((uint8_t *)pix)[2], a_mul);
        }
    }
}

void SoftRenderer::renderQuad(const QuadInfo& quad)
{
    if (!quad.colors.top_left.isSameColour(quad.colors.top_right) ||
        !quad.colors.top_left.isSameColour(quad.colors.bottom_left) ||
        !quad.colors.top_left.isSameColour(quad.colors.bottom_right)) {
        renderQuadWithColourRect(quad);
        return;
    }


    uint32_t* src = quad.tex->_surf + quad.tex_src.top * (int)quad.tex->getWidth();
    src += quad.tex_src.left;


    int src_width = quad.tex_src.right - quad.tex_src.left;
    int src_height = quad.tex_src.bottom - quad.tex_src.top;

    int dest_width = quad.dest.right - quad.dest.left;
    int dest_height = quad.dest.bottom - quad.dest.top;


    uint32_t x_scale = (src_width << 16) / dest_width;
    uint32_t y_scale = (src_height << 16) / dest_height;

    uint32_t* line = (uint32_t*)_surface + quad.dest.top * _width;
    line += quad.dest.left;

    uint64_t r = quad.colors.top_left.r;
    uint64_t g = quad.colors.top_left.g;
    uint64_t b = quad.colors.top_left.b;
    uint64_t a = quad.colors.top_left.a;

    for (int i = 0; i < dest_height; line += _width, i++) {
        uint32_t* pix = line;
        uint32_t* src_line = src + (((i * y_scale) + (1 << 15)) >> 16) * (int)quad.tex->getWidth();

        for (int j = 0; j < dest_width; pix++, j++) {
            uint8_t* tex_pix = (uint8_t*)&src_line[(((j * x_scale)+ (1 << 15)) >> 16)];
            uint64_t a_mul = a * tex_pix[3];

            ((uint8_t *)pix)[0] = calac_pixel(tex_pix[0], b, ((uint8_t *)pix)[0], a_mul);
            ((uint8_t *)pix)[1] = calac_pixel(tex_pix[1], g, ((uint8_t *)pix)[1], a_mul);
            ((uint8_t *)pix)[2] = calac_pixel(tex_pix[2], r, ((uint8_t *)pix)[2], a_mul);
        }
    }
}

inline void SoftRenderer::setRGB(ColourI& dest, const colour& src)
{
    dest.r = uint8_t(src.getRed()* 255);
    dest.g = uint8_t(src.getGreen() * 255);
    dest.b = uint8_t(src.getBlue() * 255);
    dest.a = uint8_t(src.getAlpha() * 255);
}

void SoftRenderer::addQuad(const Rect& dest_rect, float z, const Texture* texture,
                   const Rect& texture_rect, const ColourRect& colours,
                   QuadSplitMode quad_split_mode)
{
    if (dest_rect.d_right <= dest_rect.d_left || dest_rect.d_bottom <= dest_rect.d_top) {
        return;
    }

    if (texture_rect.d_right <= texture_rect.d_left ||
                                           texture_rect.d_bottom <= texture_rect.d_top) {
        return;
    }

    QuadInfo quad;
    quad.dest.top = (int)dest_rect.d_top;
    quad.dest.left = (int)dest_rect.d_left;
    quad.dest.bottom = (int)dest_rect.d_bottom;
    quad.dest.right = (int)dest_rect.d_right;

    quad.tex = (const SoftTexture*)texture;

    quad.tex_src.top = int(texture_rect.d_top * texture->getHeight());
    quad.tex_src.bottom = int(texture_rect.d_bottom * texture->getHeight());
    quad.tex_src.left = int(texture_rect.d_left * texture->getWidth());
    quad.tex_src.right = int(texture_rect.d_right * texture->getWidth());

    setRGB(quad.colors.top_left, colours.d_top_left);
    setRGB(quad.colors.top_right, colours.d_top_right);
    setRGB(quad.colors.bottom_left, colours.d_bottom_left);
    setRGB(quad.colors.bottom_right, colours.d_bottom_right);

    quad.z = z;

    if (!_queueing) {
        renderQuad(quad);
        return;
    }

    _queue.insert(quad);
}

void SoftRenderer::doRender()
{
    QuadQueue::iterator iter = _queue.begin();

    for (; iter != _queue.end(); ++iter) {
        renderQuad(*iter);
    }
}

void SoftRenderer::clearRenderList()
{
    _queue.clear();
}

void SoftRenderer::setQueueingEnabled(bool val)
{
    _queueing = val;
}

bool SoftRenderer::isQueueingEnabled() const
{
    return _queueing;
}

Texture* SoftRenderer::createTexture()
{
    SoftTexture* texture = new SoftTexture(this);
    _textures.push_back(texture);
    return texture;
}

Texture* SoftRenderer::createTexture(const String& filename,
                                     const String& resourceGroup)
{
    SoftTexture* texture = new SoftTexture(this, filename, resourceGroup);
    _textures.push_back(texture);
    return texture;
}

Texture* SoftRenderer::createTexture(float size)
{
    SoftTexture* texture = new SoftTexture(this, (uint)size);
    _textures.push_back(texture);
    return texture;
}

void SoftRenderer::destroyTexture(Texture* texture)
{
    if (!texture) {
        return;
    }
    SoftTexture* soft_texture = (SoftTexture*)texture;
    _textures.remove(soft_texture);
    delete soft_texture;
}

void SoftRenderer::destroyAllTextures()
{
    while (!_textures.empty()) {
        SoftTexture* texture = *_textures.begin();
        _textures.pop_front();
        delete texture;
    }
}

uint SoftRenderer::getMaxTextureSize() const
{
    return 1 << 16;
}

float SoftRenderer::getWidth() const
{
    return (float)_width;
}

float SoftRenderer::getHeight() const
{
    return (float)_height;
}

Size SoftRenderer::getSize() const
{
    return Size((float)_width, (float)_height);
}

Rect SoftRenderer::getRect() const
{
    return Rect(0, 0, (float)_width, (float)_height);
}

uint SoftRenderer::getHorzScreenDPI() const
{
    return 96;
}

uint SoftRenderer::getVertScreenDPI() const
{
    return 96;
}

}
