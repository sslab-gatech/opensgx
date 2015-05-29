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

#ifndef _H_XPLATFORM
#define _H_XPLATFORM

#include "red_drawable.h"
#include <X11/Xdefs.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

class XPlatform {
public:
    static Display* get_display();
    static XVisualInfo** get_vinfo();
    static RedDrawable::Format get_screen_format(int screen);
    static XIC get_input_context();
#ifdef USE_OPENGL
    static GLXFBConfig** get_fbconfig();
#endif // USE_OPENGL

    typedef void (*win_proc_t)(XEvent& event);
    static void set_win_proc(Window win, win_proc_t proc);
    static void cleare_win_proc(Window win);

    static void on_focus_in();
    static void on_focus_out();

    static bool is_x_shm_avail();
    static XImage *create_x_shm_image(RedDrawable::Format format,
                                      int width, int height, int depth,
                                      Visual *visual,
                                      XShmSegmentInfo **shminfo_out);
    static XImage *create_x_image(RedDrawable::Format format,
                                  int width, int height, int depth,
                                  Visual *visual,
                                  XShmSegmentInfo **shminfo_out);
    static void free_x_image(XImage *image,
                             XShmSegmentInfo *shminfo);
};

#endif
