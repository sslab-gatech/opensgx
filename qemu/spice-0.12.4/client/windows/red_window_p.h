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

#include <map>
#include <red_drawable.h>

class RedWindow;
class Menu;
struct PixelsSource_p;

typedef HWND Window;

class CommandInfo {
public:
    CommandInfo() : menu (0), command (0) {}
    CommandInfo(Menu* in_menu, int in_command) : menu (in_menu), command (in_command) {}

    Menu* menu;
    int command;
};

typedef std::map<int, CommandInfo> CommandMap;

class RedWindow_p {
public:
    RedWindow_p();

    void create(RedWindow& red_window, PixelsSource_p& pixels_source);
    void destroy(PixelsSource_p& pixels_source);
    void release_menu(Menu* menu);
    void on_minimized();
    void on_restored();
    void on_pos_changing(RedWindow& red_window);
    bool prossec_menu_commands(int cmd);

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

protected:
    HWND _win;
    RedDrawable::Format _format;
    uint32_t _modal_refs;
    HMODULE _no_taskmgr_dll;
    HHOOK _no_taskmgr_hook;
    bool _focused;
    bool _pointer_in_window;
    bool _minimized;
    bool _valid_pos;
    int _x;
    int _y;
    CommandMap _commands_map;
    HMENU _sys_menu;
};

#endif
