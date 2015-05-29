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
#ifndef _directfbrenderer_h_
#define _directfbrenderer_h_

#include <stdint.h>
#include <list>
#include <set>
/* CEGUI 0.6 bug, CEGUITexture.h doesn't include this, we need to */
#include <cstddef>

#include "CEGUIRenderer.h"
#include "CEGUIColourRect.h"
#include "CEGUIRect.h"


namespace CEGUI
{
    class SoftTexture;
    class ImageCodec;

    class SoftRenderer : public Renderer
    {
    public:
        SoftRenderer(uint8_t* surface, uint width, uint height, uint stride,
                     ImageCodec* codec = NULL);
        virtual ~SoftRenderer();

        void reset_surface(uint8_t* surface, uint width, uint height, uint stride);

        virtual void addQuad(const Rect& dest_rect, float z, const Texture* tex,
                            const Rect& texture_rect, const ColourRect& colours,
                            QuadSplitMode quad_split_mode);
        virtual void doRender();
        virtual void clearRenderList();
        virtual void setQueueingEnabled(bool setting);
        virtual bool isQueueingEnabled() const;

        virtual Texture* createTexture();
        virtual Texture* createTexture(const String& filename,
                                       const String& resourceGroup);
        virtual Texture* createTexture(float size);
        virtual void destroyTexture(Texture* texture);
        virtual void destroyAllTextures();
        virtual uint getMaxTextureSize() const;

        virtual float getWidth() const;
        virtual float getHeight() const;
        virtual Size getSize() const;
        virtual Rect getRect() const;

        virtual uint getHorzScreenDPI() const;
        virtual uint getVertScreenDPI() const;

        ImageCodec* getImageCodec() { return _image_codec;}

    private:
        void setupImageCodec();
        void cleanupImageCodec();
        struct QuadInfo;
        void renderQuad(const QuadInfo& quad);
        void renderQuadWithColourRect(const QuadInfo& quad);

        class ColourI {
        public:

            bool isSameColour(const ColourI& other) const
            {
                return other.r == r && other.g == g && other.b == b && other.a == a;
            }

            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        };

        static inline void setRGB(ColourI& dest, const colour& src);
        static inline void componnentAtPoint(int x_pos, int y_pos,
                                             int top_left, int top_right,
                                             int bottom_left, int bottom_right,
                                             uint64_t& comp);

        struct ColourIRect {
            ColourI top_left;
            ColourI top_right;
            ColourI bottom_left;
            ColourI bottom_right;
        };

        static void colourAtPoint(int x, int x_max, int y, int y_max,
                                  const ColourIRect& colours,
                                  uint64_t& r, uint64_t& g,
                                  uint64_t& b, uint64_t& a);

    private:
        uint8_t* _surface;
        int _width;
        int _height;
        ImageCodec* _image_codec;
        DynamicModule* _image_codec_module;
        void (*_destroy_image_codec)(ImageCodec*);
        bool _queueing;

        struct RectI {
            int left;
            int top;
            int right;
            int bottom;
        };

        struct QuadInfo {
            RectI dest;
            const SoftTexture* tex;
            RectI tex_src;
            ColourIRect colors;
            float z;

            bool operator < (const QuadInfo& other) const
            {
                return z > other.z;
            }
        };

        typedef std::multiset<QuadInfo> QuadQueue;
        QuadQueue _queue;

        typedef std::list<SoftTexture *> TexturesList;
        TexturesList _textures;
   };
}

#endif
