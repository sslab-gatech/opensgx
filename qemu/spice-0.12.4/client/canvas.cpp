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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "canvas.h"
#include "utils.h"
#include "debug.h"

static SpiceCanvas* surfaces_cache_op_get(SpiceImageSurfaces *surfaces, uint32_t surface_id)
{
    SurfacesCache* surfaces_cache = static_cast<SurfacesCache*>(surfaces);
    if (!surfaces_cache->exist(surface_id)) {
        return NULL;
    }
    return (*surfaces_cache)[surface_id]->get_internal_canvas();
}

SurfacesCache::SurfacesCache()
{
    static SpiceImageSurfacesOps surfaces_ops = {
        surfaces_cache_op_get,
    };
    ops = &surfaces_ops;
}

bool SurfacesCache::exist(uint32_t surface_id)
{
    return (this->count(surface_id) != 0);
}

Canvas::Canvas(PixmapCache& pixmap_cache, PaletteCache& palette_cache,
               GlzDecoderWindow &glz_decoder_window, SurfacesCache &csurfaces)
    : _canvas (NULL)
    , _pixmap_cache (pixmap_cache)
    , _palette_cache (palette_cache)
    , _glz_decoder(glz_decoder_window, _glz_handler, _glz_debug)
    , _surfaces_cache(csurfaces)
{
}

Canvas::~Canvas()
{
    /* _canvas is both set and destroyed by derived class */
}

void Canvas::clear()
{
    if (_canvas) {
        _canvas->ops->clear(_canvas);
    }
}

void Canvas::begin_draw(SpiceMsgDisplayBase& base, int size, size_t min_size)
{
}

void Canvas::draw_fill(SpiceMsgDisplayDrawFill& fill, int size)
{
    begin_draw(fill.base, size, sizeof(SpiceMsgDisplayDrawFill));
    _canvas->ops->draw_fill(_canvas, &fill.base.box, &fill.base.clip, &fill.data);
    touched_bbox(&fill.base.box);
}

void Canvas::draw_text(SpiceMsgDisplayDrawText& text, int size)
{
    begin_draw(text.base, size, sizeof(SpiceMsgDisplayDrawText));
    _canvas->ops->draw_text(_canvas, &text.base.box, &text.base.clip, &text.data);
    touched_bbox(&text.base.box);
}

void Canvas::draw_opaque(SpiceMsgDisplayDrawOpaque& opaque, int size)
{
    begin_draw(opaque.base, size, sizeof(SpiceMsgDisplayDrawOpaque));
    _canvas->ops->draw_opaque(_canvas, &opaque.base.box, &opaque.base.clip, &opaque.data);
    touched_bbox(&opaque.base.box);
}

void Canvas::draw_copy(SpiceMsgDisplayDrawCopy& copy, int size)
{
    begin_draw(copy.base, size, sizeof(SpiceMsgDisplayDrawCopy));
    _canvas->ops->draw_copy(_canvas, &copy.base.box, &copy.base.clip, &copy.data);
    touched_bbox(&copy.base.box);
}

void Canvas::draw_transparent(SpiceMsgDisplayDrawTransparent& transparent, int size)
{
    begin_draw(transparent.base, size, sizeof(SpiceMsgDisplayDrawTransparent));
    _canvas->ops->draw_transparent(_canvas, &transparent.base.box, &transparent.base.clip, &transparent.data);
    touched_bbox(&transparent.base.box);
}

void Canvas::draw_alpha_blend(SpiceMsgDisplayDrawAlphaBlend& alpha_blend, int size)
{
    begin_draw(alpha_blend.base, size, sizeof(SpiceMsgDisplayDrawAlphaBlend));
    _canvas->ops->draw_alpha_blend(_canvas, &alpha_blend.base.box, &alpha_blend.base.clip, &alpha_blend.data);
    touched_bbox(&alpha_blend.base.box);
}

void Canvas::draw_composite(SpiceMsgDisplayDrawComposite& composite, int size)
{
    begin_draw(composite.base, size, sizeof(SpiceMsgDisplayDrawComposite));
    _canvas->ops->draw_composite(_canvas, &composite.base.box, &composite.base.clip, &composite.data);
    touched_bbox(&composite.base.box);
}

void Canvas::copy_bits(SpiceMsgDisplayCopyBits& copy, int size)
{
    begin_draw(copy.base, size, sizeof(SpiceMsgDisplayCopyBits));
    _canvas->ops->copy_bits(_canvas, &copy.base.box, &copy.base.clip, &copy.src_pos);
    touched_bbox(&copy.base.box);
}

void Canvas::draw_blend(SpiceMsgDisplayDrawBlend& blend, int size)
{
    begin_draw(blend.base, size, sizeof(SpiceMsgDisplayDrawBlend));
    _canvas->ops->draw_blend(_canvas, &blend.base.box, &blend.base.clip, &blend.data);
    touched_bbox(&blend.base.box);
}

void Canvas::draw_blackness(SpiceMsgDisplayDrawBlackness& blackness, int size)
{
    begin_draw(blackness.base, size, sizeof(SpiceMsgDisplayDrawBlackness));
    _canvas->ops->draw_blackness(_canvas, &blackness.base.box, &blackness.base.clip, &blackness.data);
    touched_bbox(&blackness.base.box);
}

void Canvas::draw_whiteness(SpiceMsgDisplayDrawWhiteness& whiteness, int size)
{
    begin_draw(whiteness.base, size, sizeof(SpiceMsgDisplayDrawWhiteness));
    _canvas->ops->draw_whiteness(_canvas, &whiteness.base.box, &whiteness.base.clip, &whiteness.data);
    touched_bbox(&whiteness.base.box);
}

void Canvas::draw_invers(SpiceMsgDisplayDrawInvers& invers, int size)
{
    begin_draw(invers.base, size, sizeof(SpiceMsgDisplayDrawInvers));
    _canvas->ops->draw_invers(_canvas, &invers.base.box, &invers.base.clip, &invers.data);
    touched_bbox(&invers.base.box);
}

void Canvas::draw_rop3(SpiceMsgDisplayDrawRop3& rop3, int size)
{
    begin_draw(rop3.base, size, sizeof(SpiceMsgDisplayDrawRop3));
    _canvas->ops->draw_rop3(_canvas, &rop3.base.box, &rop3.base.clip, &rop3.data);
    touched_bbox(&rop3.base.box);
}

void Canvas::draw_stroke(SpiceMsgDisplayDrawStroke& stroke, int size)
{
    begin_draw(stroke.base, size, sizeof(SpiceMsgDisplayDrawStroke));
    _canvas->ops->draw_stroke(_canvas, &stroke.base.box, &stroke.base.clip, &stroke.data);
    touched_bbox(&stroke.base.box);
}

void Canvas::put_image(
#ifdef WIN32
                        HDC dc,
#endif
                        const PixmapHeader& image, const SpiceRect& dest, const QRegion* clip)
{
    _canvas->ops->put_image(_canvas,
#ifdef WIN32
                            dc,
#endif
                            &dest, image.data, image.width, image.height, image.stride,
                            clip);
    touched_bbox(&dest);
}
