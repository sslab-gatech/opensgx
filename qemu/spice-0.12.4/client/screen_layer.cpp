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
#include "screen_layer.h"
#include "utils.h"
#include "screen.h"
#include "application.h"
#include "debug.h"

class AttachLayerEvent: public SyncEvent {
public:
    AttachLayerEvent(ScreenLayer& layer, int screen_id)
        : _layer (layer)
        , _screen_id (screen_id)
    {
    }

    virtual void do_response(AbstractProcessLoop& events_loop);

private:
    ScreenLayer& _layer;
    int _screen_id;
};

void AttachLayerEvent::do_response(AbstractProcessLoop& events_loop)
{
    Application* app = (Application*)(events_loop.get_owner());
    AutoRef<RedScreen> screen(app->get_screen(_screen_id));
    (*screen)->attach_layer(_layer);
}

class DetachLayerEvent: public SyncEvent {
public:
    DetachLayerEvent(ScreenLayer& _layer) : _layer (_layer) {}

    virtual void do_response(AbstractProcessLoop& events_loop);

private:
    ScreenLayer& _layer;
};

void DetachLayerEvent::do_response(AbstractProcessLoop& events_loop)
{
    _layer.screen()->detach_layer(_layer);
}

ScreenLayer::ScreenLayer(int z_order, bool opaque)
    : _screen (NULL)
    , _z_order (z_order)
    , _opaque (opaque)
    , _using_ogl (false)
{
    region_init(&_area);
    region_init(&_direct_area);
    region_init(&_composit_area);
}

ScreenLayer::~ScreenLayer()
{
    ASSERT(!_screen);
    region_destroy(&_area);
    region_destroy(&_direct_area);
    region_destroy(&_composit_area);
}

uint64_t ScreenLayer::invalidate_rect(const SpiceRect& r, bool urgent)
{
    return _screen->invalidate(r, urgent);
}

uint64_t ScreenLayer::invalidate(const SpiceRect& r, bool urgent)
{
    if (!_screen) {
        return 0;
    }
    return invalidate_rect(r, urgent);
}

void ScreenLayer::invalidate(const QRegion& region)
{
    pixman_box32_t *rects, *end;
    int num_rects;

    if (!_screen) {
        return;
    }

    rects = pixman_region32_rectangles((pixman_region32_t *)&region, &num_rects);
    end = rects + num_rects;

    while (rects != end) {
        SpiceRect r;

        r.left = rects->x1;
        r.top = rects->y1;
        r.right = rects->x2;
        r.bottom = rects->y2;
        rects++;

        invalidate_rect(r, false);
    }
}

void ScreenLayer::invalidate()
{
    invalidate(_area);
}

void ScreenLayer::notify_changed()
{
    if (_screen) {
        _screen->on_layer_changed(*this);
    }
}

void ScreenLayer::set_area(const QRegion& area)
{
    Lock lock(_area_lock);
    invalidate();
    region_destroy(&_area);
    region_clone(&_area, &area);
    invalidate();
    notify_changed();
}

void ScreenLayer::clear_area()
{
    Lock lock(_area_lock);
    invalidate();
    region_clear(&_area);
    notify_changed();
}

void ScreenLayer::set_rect_area(const SpiceRect& r)
{
    Lock lock(_area_lock);
    invalidate();
    region_clear(&_area);
    region_add(&_area, &r);
    invalidate();
    notify_changed();
}

void ScreenLayer::offset_area(int dx, int dy)
{
    Lock lock(_area_lock);
    invalidate();
    region_offset(&_area, dx, dy);
    invalidate();
    notify_changed();
}

void ScreenLayer::add_rect_area(const SpiceRect& r)
{
    Lock lock(_area_lock);
    region_add(&_area, &r);
    notify_changed();
}

void ScreenLayer::remove_rect_area(const SpiceRect& r)
{
    Lock lock(_area_lock);
    invalidate();
    region_remove(&_area, &r);
    notify_changed();
}

void ScreenLayer::begin_update(QRegion& direct_rgn, QRegion& composit_rgn)
{
    Lock lock(_area_lock);
    region_destroy(&_direct_area);
    region_clone(&_direct_area, &_area);
    region_and(&_direct_area, &direct_rgn);

    region_destroy(&_composit_area);
    region_clone(&_composit_area, &_area);
    region_and(&_composit_area, &composit_rgn);

    region_exclude(&direct_rgn, &_direct_area);
    if (_opaque) {
        region_exclude(&composit_rgn, &_composit_area);
    } else {
        region_or(&composit_rgn, &_direct_area);
        region_or(&_composit_area, &_direct_area);
        region_clear(&_direct_area);
    }
}

bool ScreenLayer::contains_point(int x, int y)
{
    Lock lock(_area_lock);
    return !!region_contains_point(&_area, x, y);
}

void ScreenLayer::attach_to_screen(Application& application, int screen_id)
{
    if (_screen) {
        return;
    }
    AutoRef<AttachLayerEvent> event(new AttachLayerEvent(*this, screen_id));
    application.push_event(*event);
    (*event)->wait();
    if (!(*event)->success()) {
        THROW("attach failed");
    }
    ASSERT(_screen);
}

void ScreenLayer::detach_from_screen(Application& application)
{
    if (!_screen) {
        return;
    }
    AutoRef<DetachLayerEvent> event(new DetachLayerEvent(*this));
    application.push_event(*event);
    (*event)->wait();
    if (!(*event)->success()) {
        THROW("detach failed");
    }
    ASSERT(!_screen);
}
