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

#ifndef _H_SCREEN_LAYER
#define _H_SCREEN_LAYER

#include "common/region.h"
#include "threads.h"

class RedScreen;
class Application;

class ScreenLayer {
public:
    ScreenLayer(int z_order, bool opaque);
    virtual ~ScreenLayer();

    void attach_to_screen(Application& application, int screen_id);
    void detach_from_screen(Application& application);

    void set_screen(RedScreen* screen) { _screen = screen;}
    RedScreen* screen() { return _screen; }
    int z_order() { return _z_order;}
    void set_area(const QRegion& area);
    void offset_area(int dx, int dy);
    void clear_area();
    void set_rect_area(const SpiceRect& r);
    void add_rect_area(const SpiceRect& r);
    void remove_rect_area(const SpiceRect& r);
    void begin_update(QRegion& direct_rgn, QRegion& composit_rgn);
    void invalidate();
    uint64_t invalidate(const SpiceRect& r, bool urgent = false);
    void invalidate(const QRegion& r);
    bool contains_point(int x, int y);

    virtual void copy_pixels(const QRegion& dest_region, RedDrawable& dest_dc) {}

    void set_using_ogl(bool val) {_using_ogl = val;}
    bool using_ogl() {return _using_ogl;}
    virtual void on_size_changed() {}

    virtual void pre_migrate() { }
    virtual void post_migrate() { }

    QRegion& area() { return _area;}
    QRegion& direct_area() { return _direct_area;}
    QRegion& composit_area() { return _composit_area;}

    virtual void on_update_completion(uint64_t mark) {}

    virtual bool pointer_test(int x, int y) { return false;}
    virtual void on_pointer_enter(int x, int y, unsigned int buttons_state) {}
    virtual void on_pointer_motion(int x, int y, unsigned int buttons_state) {}
    virtual void on_pointer_leave() {}
    virtual void on_mouse_button_press(int button, int buttons_state) {}
    virtual void on_mouse_button_release(int button, int buttons_state) {}

private:
    uint64_t invalidate_rect(const SpiceRect& r, bool urgent);
    void notify_changed();

private:
    RedScreen* _screen;
    int _z_order;
    bool _opaque;
    bool _using_ogl;
    Mutex _area_lock;
    QRegion _area;
    QRegion _direct_area;
    QRegion _composit_area;
};

#endif
