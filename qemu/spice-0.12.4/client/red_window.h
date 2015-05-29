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

#ifndef _H_RED_WINDOW
#define _H_RED_WINDOW

#include "red_drawable.h"
#include <spice/protocol.h>
#include "red_key.h"
#include "red_window_p.h"
#include "cursor.h"

class Menu;
class Icon;

class RedWindow: public RedDrawable, private RedWindow_p {
public:
    class Listener;

    enum {
        DEFAULT_SCREEN = -1,
    };

    RedWindow(RedWindow::Listener& listener, int screen_id = DEFAULT_SCREEN);
    virtual ~RedWindow();

    void move_and_resize(int x, int y, int width, int height);
    void resize(int width, int height);
    void move(int x, int y);
    void position_after(RedWindow *after);
    void raise();
    void show(int screen_id);
    void external_show();
    void hide();
    void minimize();
    void activate();
    void set_title(std::string& title);
    void set_icon(Icon *icon);
    virtual RedDrawable::Format get_format();

    enum Type {
        TYPE_INVALID,
        TYPE_NORMAL,
        TYPE_FULLSCREEN,
    };
    void set_type(Type type) { _type = type;}
    SpicePoint get_position();
    virtual SpicePoint get_size();
    bool get_mouse_anchor_point(SpicePoint& pt);

    void set_mouse_position(int x, int y);
    void set_cursor(LocalCursor* local_cursor);
    void hide_cursor();
    void show_cursor();
    void capture_mouse();
    void release_mouse();
    void start_key_interception();
    void stop_key_interception();
    int set_menu(Menu* menu);

#ifdef USE_OPENGL
    void untouch_context();
    RedGlContext create_context_gl();
    RedPbuffer create_pbuff(int width, int height);
    void set_render_pbuff(RedPbuffer pbuff);
    void set_render_fbo(GLuint fbo);
    void set_gl_context(RedGlContext context);

    void set_type_gl();
    void unset_type_gl();
#endif

    int get_screen_num();

    static void init();
    static void cleanup();

    Listener& get_listener() { return _listener;}

private:
    void on_focus_in();
    void on_focus_out();
    void on_pointer_enter(int x, int y, unsigned int buttons_state);
    void on_pointer_leave();

    void do_start_key_interception();
    void do_stop_key_interception();

private:
    Listener& _listener;
    SpicePoint _window_size;
    Type _type;
    LocalCursor* _local_cursor;
    bool _cursor_visible;
    bool _trace_key_interception;
    bool _key_interception_on;
    Menu* _menu;

    friend class RedWindow_p;
};

class RedWindow::Listener {
public:
    virtual ~Listener() {}
    virtual void on_exposed_rect(const SpiceRect& area) = 0;

    virtual void on_pointer_enter(int x, int y, unsigned int buttons_state) = 0;
    virtual void on_pointer_motion(int x, int y, unsigned int buttons_state) = 0;
    virtual void on_pointer_leave() = 0;
    virtual void on_mouse_button_press(SpiceMouseButton button, unsigned int buttons_state) = 0;
    virtual void on_mouse_button_release(SpiceMouseButton button, unsigned int buttons_state) = 0;

    virtual void on_key_press(RedKey key) = 0;
    virtual void on_key_release(RedKey key) = 0;
    virtual void on_char(uint32_t ch) = 0;

    virtual void on_deactivate() = 0;
    virtual void on_activate() = 0;

    virtual void on_start_key_interception() = 0;
    virtual void on_stop_key_interception() = 0;
    virtual void enter_modal_loop() = 0;
    virtual void exit_modal_loop() = 0;

    virtual void pre_migrate() { }
    virtual void post_migrate() { }
};

/*class REGION {
    void get_bbox(SpiceRect& bbox) const;
    bool contains_point(int x, int y) const;

};*/

template <class REGION>
static bool find_anchor_point(const REGION& region, SpicePoint& pt)
{
    static const unsigned int lookup_size = 20;
    unsigned int width;
    unsigned int height;
    SpiceRect bbox;

    region.get_bbox(bbox);
    width = bbox.right - bbox.left;
    height = bbox.bottom - bbox.top;

    int div = 2;

    for (;;) {
        unsigned int h_unit;
        unsigned int v_unit;

        if ((v_unit = height / div) * 2 < lookup_size || (h_unit = width / div) * 2 < lookup_size) {
            return false;
        }

        for (unsigned int y = v_unit; y < v_unit * div; y += v_unit * 2) {
            for (unsigned int x = h_unit; x < h_unit * div; x += h_unit * 2) {
                if (!region.contains_point(bbox.left + x, bbox.top + y)) {
                    continue;
                }

                SpiceRect r;
                r.left = bbox.left + x - lookup_size / 2;
                r.right = r.left + lookup_size;
                r.top = bbox.top + y - lookup_size / 2;
                r.bottom = r.top + lookup_size;

                if (!region.contains_point(r.left, r.top) ||
                    !region.contains_point(r.right, r.top) ||
                    !region.contains_point(r.left, r.bottom) ||
                    !region.contains_point(r.right, r.bottom)) {
                    continue;
                }
                pt.x = bbox.left + x;
                pt.y = bbox.top + y;
                return true;
            }
        }
        div *= 2;
    }
}

#endif
