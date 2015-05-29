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

#ifndef _H_INPUTS_HANDLER
#define _H_INPUTS_HANDLER

#include "red_key.h"

class KeyHandler {
public:
    virtual ~KeyHandler() {}
    virtual void on_key_down(RedKey key) {}
    virtual void on_key_up(RedKey key) {}
    virtual void on_char(uint32_t ch) {}
    virtual void on_focus_in() {}
    virtual void on_focus_out() {}
    virtual bool permit_focus_loss() { return true;}
};

class MouseHandler {
public:
    virtual ~MouseHandler() {}
    virtual void on_mouse_motion(int dx, int dy, int buttons_state) {}
    virtual void on_mouse_down(int button, int buttons_state) {}
    virtual void on_mouse_up(int button, int buttons_state) {}
};

#endif
