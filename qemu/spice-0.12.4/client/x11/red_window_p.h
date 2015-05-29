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

#ifndef _H_RED_WINDOW_P
#define _H_RED_WINDOW_P

#ifdef USE_OPENGL
#include <GL/glx.h>
#endif // USE_OPENGL
#include <X11/Xdefs.h>
#include <X11/Xlib.h>

typedef Window Win;
#ifdef USE_OPENGL
typedef GLXContext RedGlContext;
typedef GLXPbuffer RedPbuffer;
#endif // USE_OPENGL

class RedWindow;
class Icon;
struct PixelsSource_p;

class RedWindow_p {
public:
    RedWindow_p();

    void migrate(RedWindow& red_window, PixelsSource_p& pix_source, int dest_screen);
    void create(RedWindow& red_window, PixelsSource_p& pix_source,
                int x, int y, int in_screen);
    void destroy(RedWindow& red_window, PixelsSource_p& pix_source);
    void set_minmax(PixelsSource_p& pix_source);
    void wait_for_reparent();
    void wait_for_map();
    void wait_for_unmap();
    void sync(bool shadowed = false);
    void set_visibale(bool vis) { _visibale = vis;}
    void move_to_current_desktop();
    Window get_window() {return _win;}

    static void win_proc(XEvent& event);
    static Cursor create_invisible_cursor(Window window);

#ifdef USE_OPENGL
    void set_glx(int width, int height);
#endif // USE_OPENGL
    static void handle_key_press_event(RedWindow& red_window, XKeyEvent* event);

protected:
    int _screen;
    Window _win;
    Cursor _invisible_cursor;
    bool _visibale;
    bool _expect_parent;
    SpicePoint _show_pos;
    bool _show_pos_valid;
#ifdef USE_OPENGL
    GLXContext _glcont_copy;
#endif // USE_OPENGL
    Icon* _icon;
    bool _focused;
    bool _ignore_foucs;
    bool _shadow_foucs_state;
    XEvent _shadow_focus_event;
    bool _pointer_in_window;
    bool _ignore_pointer;
    bool _shadow_pointer_state;
    XEvent _shadow_pointer_event;
    Colormap _colormap;
    RedWindow *_red_window;
    int _width;
    int _height;
    Time _last_event_time;
};

#endif
