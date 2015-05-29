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
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/keysymdef.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#ifdef USE_OPENGL
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <GL/glext.h>
#endif // USE_OPENGL
#include <stdio.h>

#include <spice/protocol.h>
#include "common/region.h"

#ifdef USE_OPENGL
#include "common/gl_utils.h"
#include "red_pixmap_gl.h"
#endif // USE_OPENGL

#include "red_window.h"
#include "utils.h"
#include "debug.h"
#include "platform.h"
#include "x_platform.h"
#include "pixels_source_p.h"
#include "x_icon.h"

#define X_RETRIES 10
#define X_RETRY_DELAY_MICRO (1000 * 100)
#define RAISE_RETRIES X_RETRIES
#define Z_POSITION_RETRIES X_RETRIES
#define GET_POSITION_RETRIES X_RETRIES
#define GET_VIS_REGION_RETRIES X_RETRIES
#define MOUSE_GRAB_RETRIES X_RETRIES

static Display* x_display = NULL;
static XContext user_data_context;
static bool using_evdev = false;
static XIC x_input_context = NULL;

static Atom wm_protocol_atom;
static Atom wm_delete_window_atom;

static Atom wm_desktop;
static Atom wm_current_desktop;

static Atom wm_state;
static Atom wm_state_above;
static Atom wm_state_fullscreen;

static Atom wm_user_time;

static RedWindow* focus_window;
static unsigned long focus_serial = 0;

#define USE_X11_KEYCODE

#ifdef USE_X11_KEYCODE

enum EvdevKeyCode {
    EVDEV_KEYCODE_ESCAPE = 9,
    EVDEV_KEYCODE_1,
    EVDEV_KEYCODE_2,
    EVDEV_KEYCODE_3,
    EVDEV_KEYCODE_4,
    EVDEV_KEYCODE_5,
    EVDEV_KEYCODE_6,
    EVDEV_KEYCODE_7,
    EVDEV_KEYCODE_8,
    EVDEV_KEYCODE_9,
    EVDEV_KEYCODE_0,
    EVDEV_KEYCODE_MINUS,
    EVDEV_KEYCODE_EQUAL,
    EVDEV_KEYCODE_BACK_SPACE,
    EVDEV_KEYCODE_TAB,
    EVDEV_KEYCODE_Q,
    EVDEV_KEYCODE_W,
    EVDEV_KEYCODE_E,
    EVDEV_KEYCODE_R,
    EVDEV_KEYCODE_T,
    EVDEV_KEYCODE_Y,
    EVDEV_KEYCODE_U,
    EVDEV_KEYCODE_I,
    EVDEV_KEYCODE_O,
    EVDEV_KEYCODE_P,
    EVDEV_KEYCODE_L_BRACKET,
    EVDEV_KEYCODE_R_BRACKET,
    EVDEV_KEYCODE_RETURN,
    EVDEV_KEYCODE_L_CONTROL,
    EVDEV_KEYCODE_A,
    EVDEV_KEYCODE_S,
    EVDEV_KEYCODE_D,
    EVDEV_KEYCODE_F,
    EVDEV_KEYCODE_G,
    EVDEV_KEYCODE_H,
    EVDEV_KEYCODE_J,
    EVDEV_KEYCODE_K,
    EVDEV_KEYCODE_L,
    EVDEV_KEYCODE_SEMICOLON,
    EVDEV_KEYCODE_APOSTROPH,
    EVDEV_KEYCODE_BACKQUAT,
    EVDEV_KEYCODE_L_SHIFT,
    EVDEV_KEYCODE_BACKSLASH,
    EVDEV_KEYCODE_Z,
    EVDEV_KEYCODE_X,
    EVDEV_KEYCODE_C,
    EVDEV_KEYCODE_V,
    EVDEV_KEYCODE_B,
    EVDEV_KEYCODE_N,
    EVDEV_KEYCODE_M,
    EVDEV_KEYCODE_COMMA,
    EVDEV_KEYCODE_PERIOD,
    EVDEV_KEYCODE_SLASH,
    EVDEV_KEYCODE_R_SHIFT,
    EVDEV_KEYCODE_PAD_MULTIPLY,
    EVDEV_KEYCODE_L_ALT,
    EVDEV_KEYCODE_SPACE,
    EVDEV_KEYCODE_CAPS_LOCK,
    EVDEV_KEYCODE_F1,
    EVDEV_KEYCODE_F2,
    EVDEV_KEYCODE_F3,
    EVDEV_KEYCODE_F4,
    EVDEV_KEYCODE_F5,
    EVDEV_KEYCODE_F6,
    EVDEV_KEYCODE_F7,
    EVDEV_KEYCODE_F8,
    EVDEV_KEYCODE_F9,
    EVDEV_KEYCODE_F10,
    EVDEV_KEYCODE_NUM_LOCK,
    EVDEV_KEYCODE_SCROLL_LOCK,
    EVDEV_KEYCODE_PAD_7,
    EVDEV_KEYCODE_PAD_8,
    EVDEV_KEYCODE_PAD_9,
    EVDEV_KEYCODE_PAD_SUBTRACT,
    EVDEV_KEYCODE_PAD_4,
    EVDEV_KEYCODE_PAD_5,
    EVDEV_KEYCODE_PAD_6,
    EVDEV_KEYCODE_PAD_ADD,
    EVDEV_KEYCODE_PAD_1,
    EVDEV_KEYCODE_PAD_2,
    EVDEV_KEYCODE_PAD_3,
    EVDEV_KEYCODE_PAD_0,
    EVDEV_KEYCODE_PAD_DEL,
    EVDEV_KEYCODE_EUROPEAN = 94,
    EVDEV_KEYCODE_F11,
    EVDEV_KEYCODE_F12,
    EVDEV_KEYCODE_JAPANESE_BACKSLASH,
    EVDEV_KEYCODE_JAPANESE_HENKAN = 100,
    EVDEV_KEYCODE_JAPANESE_HIRAGANA_KATAKANA,
    EVDEV_KEYCODE_JAPANESE_MUHENKAN,
    EVDEV_KEYCODE_PAD_ENTER = 104,
    EVDEV_KEYCODE_R_CONTROL,
    EVDEV_KEYCODE_PAD_DEVIDE,
    EVDEV_KEYCODE_PRINT,
    EVDEV_KEYCODE_R_ALT,
    EVDEV_KEYCODE_HOME = 110,
    EVDEV_KEYCODE_UP,
    EVDEV_KEYCODE_PAGE_UP,
    EVDEV_KEYCODE_LEFT,
    EVDEV_KEYCODE_RIGHT,
    EVDEV_KEYCODE_END,
    EVDEV_KEYCODE_DOWN,
    EVDEV_KEYCODE_PAGE_DOWN,
    EVDEV_KEYCODE_INSERT,
    EVDEV_KEYCODE_DELETE,
    EVDEV_KEYCODE_MUTE = 121,
    EVDEV_KEYCODE_VOLUME_DOWN = 122,
    EVDEV_KEYCODE_VOLUME_UP = 123,
    EVDEV_KEYCODE_PAUSE = 127,
    EVDEV_KEYCODE_HANGUL = 130,
    EVDEV_KEYCODE_HANGUL_HANJA,
    EVDEV_KEYCODE_YEN,
    EVDEV_KEYCODE_L_COMMAND,
    EVDEV_KEYCODE_R_COMMAND,
    EVDEV_KEYCODE_MENU,
};

enum KbdKeyCode {
    KBD_KEYCODE_ESCAPE = 9,
    KBD_KEYCODE_1,
    KBD_KEYCODE_2,
    KBD_KEYCODE_3,
    KBD_KEYCODE_4,
    KBD_KEYCODE_5,
    KBD_KEYCODE_6,
    KBD_KEYCODE_7,
    KBD_KEYCODE_8,
    KBD_KEYCODE_9,
    KBD_KEYCODE_0,
    KBD_KEYCODE_MINUS,
    KBD_KEYCODE_EQUAL,
    KBD_KEYCODE_BACK_SPACE,
    KBD_KEYCODE_TAB,
    KBD_KEYCODE_Q,
    KBD_KEYCODE_W,
    KBD_KEYCODE_E,
    KBD_KEYCODE_R,
    KBD_KEYCODE_T,
    KBD_KEYCODE_Y,
    KBD_KEYCODE_U,
    KBD_KEYCODE_I,
    KBD_KEYCODE_O,
    KBD_KEYCODE_P,
    KBD_KEYCODE_L_BRACKET,
    KBD_KEYCODE_R_BRACKET,
    KBD_KEYCODE_RETURN,
    KBD_KEYCODE_L_CONTROL,
    KBD_KEYCODE_A,
    KBD_KEYCODE_S,
    KBD_KEYCODE_D,
    KBD_KEYCODE_F,
    KBD_KEYCODE_G,
    KBD_KEYCODE_H,
    KBD_KEYCODE_J,
    KBD_KEYCODE_K,
    KBD_KEYCODE_L,
    KBD_KEYCODE_SEMICOLON,
    KBD_KEYCODE_APOSTROPH,
    KBD_KEYCODE_BACKQUAT,
    KBD_KEYCODE_L_SHIFT,
    KBD_KEYCODE_BACKSLASH,
    KBD_KEYCODE_Z,
    KBD_KEYCODE_X,
    KBD_KEYCODE_C,
    KBD_KEYCODE_V,
    KBD_KEYCODE_B,
    KBD_KEYCODE_N,
    KBD_KEYCODE_M,
    KBD_KEYCODE_COMMA,
    KBD_KEYCODE_PERIOD,
    KBD_KEYCODE_SLASH,
    KBD_KEYCODE_R_SHIFT,
    KBD_KEYCODE_PAD_MULTIPLY,
    KBD_KEYCODE_L_ALT,
    KBD_KEYCODE_SPACE,
    KBD_KEYCODE_CAPS_LOCK,
    KBD_KEYCODE_F1,
    KBD_KEYCODE_F2,
    KBD_KEYCODE_F3,
    KBD_KEYCODE_F4,
    KBD_KEYCODE_F5,
    KBD_KEYCODE_F6,
    KBD_KEYCODE_F7,
    KBD_KEYCODE_F8,
    KBD_KEYCODE_F9,
    KBD_KEYCODE_F10,
    KBD_KEYCODE_NUM_LOCK,
    KBD_KEYCODE_SCROLL_LOCK,
    KBD_KEYCODE_PAD_7,
    KBD_KEYCODE_PAD_8,
    KBD_KEYCODE_PAD_9,
    KBD_KEYCODE_PAD_SUBTRACT,
    KBD_KEYCODE_PAD_4,
    KBD_KEYCODE_PAD_5,
    KBD_KEYCODE_PAD_6,
    KBD_KEYCODE_PAD_ADD,
    KBD_KEYCODE_PAD_1,
    KBD_KEYCODE_PAD_2,
    KBD_KEYCODE_PAD_3,
    KBD_KEYCODE_PAD_0,
    KBD_KEYCODE_PAD_DEL,
    KBD_KEYCODE_EUROPEAN = 94,
    KBD_KEYCODE_F11,
    KBD_KEYCODE_F12,
    KBD_KEYCODE_HOME,
    KBD_KEYCODE_UP,
    KBD_KEYCODE_PAGE_UP,
    KBD_KEYCODE_LEFT,
    KBD_KEYCODE_RIGHT = 102,
    KBD_KEYCODE_END,
    KBD_KEYCODE_DOWN,
    KBD_KEYCODE_PAGE_DOWN,
    KBD_KEYCODE_INSERT,
    KBD_KEYCODE_DELETE,
    KBD_KEYCODE_PAD_ENTER,
    KBD_KEYCODE_R_CONTROL,
    KBD_KEYCODE_PAUSE,
    KBD_KEYCODE_PRINT,
    KBD_KEYCODE_PAD_DEVIDE,
    KBD_KEYCODE_R_ALT,
    KBD_KEYCODE_L_COMMAND = 115,
    KBD_KEYCODE_R_COMMAND,
    KBD_KEYCODE_MENU,
    KBD_KEYCODE_JAPANESE_HENKAN = 129,
    KBD_KEYCODE_JAPANESE_MUHENKAN = 131,
    KBD_KEYCODE_YEN = 133,
    KBD_KEYCODE_JAPANESE_HIRAGANA_KATAKANA = 208,
    KBD_KEYCODE_HANGUL_HANJA,
    KBD_KEYCODE_HANGUL,
    KBD_KEYCODE_JAPANESE_BACKSLASH,
};

static void query_keyboard()
{
    XLockDisplay(x_display);
    XkbDescPtr kbd_desk = XkbGetKeyboard(x_display, XkbAllComponentsMask, XkbUseCoreKbd);
    XUnlockDisplay(x_display);
    if (!kbd_desk) {
        LOG_WARN("get keyboard failed");
        return;
    }

    XLockDisplay(x_display);
    char* keycodes = XGetAtomName(x_display, kbd_desk->names->keycodes);
    XUnlockDisplay(x_display);

    if (keycodes) {
        if (strstr(keycodes, "evdev")) {
            using_evdev = true;
        }
        XFree(keycodes);
    } else {
        LOG_WARN("get name failed");
    }
    XkbFreeClientMap(kbd_desk, XkbAllComponentsMask, True);
}

static RedKey keycode_map[256];

#define INIT_MAP                                                                    \
    KEYMAP(KEYCODE_ESCAPE, REDKEY_ESCAPE);                                          \
    KEYMAP(KEYCODE_1, REDKEY_1);                                                    \
    KEYMAP(KEYCODE_2, REDKEY_2);                                                    \
    KEYMAP(KEYCODE_3, REDKEY_3);                                                    \
    KEYMAP(KEYCODE_4, REDKEY_4);                                                    \
    KEYMAP(KEYCODE_5, REDKEY_5);                                                    \
    KEYMAP(KEYCODE_6, REDKEY_6);                                                    \
    KEYMAP(KEYCODE_7, REDKEY_7);                                                    \
    KEYMAP(KEYCODE_8, REDKEY_8);                                                    \
    KEYMAP(KEYCODE_9, REDKEY_9);                                                    \
    KEYMAP(KEYCODE_0, REDKEY_0);                                                    \
    KEYMAP(KEYCODE_MINUS, REDKEY_MINUS);                                            \
    KEYMAP(KEYCODE_EQUAL, REDKEY_EQUALS);                                           \
    KEYMAP(KEYCODE_BACK_SPACE, REDKEY_BACKSPACE);                                   \
    KEYMAP(KEYCODE_TAB, REDKEY_TAB);                                                \
    KEYMAP(KEYCODE_Q, REDKEY_Q);                                                    \
    KEYMAP(KEYCODE_W, REDKEY_W);                                                    \
    KEYMAP(KEYCODE_E, REDKEY_E);                                                    \
    KEYMAP(KEYCODE_R, REDKEY_R);                                                    \
    KEYMAP(KEYCODE_T, REDKEY_T);                                                    \
    KEYMAP(KEYCODE_Y, REDKEY_Y);                                                    \
    KEYMAP(KEYCODE_U, REDKEY_U);                                                    \
    KEYMAP(KEYCODE_I, REDKEY_I);                                                    \
    KEYMAP(KEYCODE_O, REDKEY_O);                                                    \
    KEYMAP(KEYCODE_P, REDKEY_P);                                                    \
    KEYMAP(KEYCODE_L_BRACKET, REDKEY_L_BRACKET);                                    \
    KEYMAP(KEYCODE_R_BRACKET, REDKEY_R_BRACKET);                                    \
    KEYMAP(KEYCODE_RETURN, REDKEY_ENTER);                                           \
    KEYMAP(KEYCODE_L_CONTROL, REDKEY_L_CTRL);                                       \
    KEYMAP(KEYCODE_A, REDKEY_A);                                                    \
    KEYMAP(KEYCODE_S, REDKEY_S);                                                    \
    KEYMAP(KEYCODE_D, REDKEY_D);                                                    \
    KEYMAP(KEYCODE_F, REDKEY_F);                                                    \
    KEYMAP(KEYCODE_G, REDKEY_G);                                                    \
    KEYMAP(KEYCODE_H, REDKEY_H);                                                    \
    KEYMAP(KEYCODE_J, REDKEY_J);                                                    \
    KEYMAP(KEYCODE_K, REDKEY_K);                                                    \
    KEYMAP(KEYCODE_L, REDKEY_L);                                                    \
    KEYMAP(KEYCODE_SEMICOLON, REDKEY_SEMICOLON);                                    \
    KEYMAP(KEYCODE_APOSTROPH, REDKEY_QUOTE);                                        \
    KEYMAP(KEYCODE_BACKQUAT, REDKEY_BACK_QUOTE);                                    \
    KEYMAP(KEYCODE_L_SHIFT, REDKEY_L_SHIFT);                                        \
    KEYMAP(KEYCODE_BACKSLASH, REDKEY_BACK_SLASH);                                   \
    KEYMAP(KEYCODE_Z, REDKEY_Z);                                                    \
    KEYMAP(KEYCODE_X, REDKEY_X);                                                    \
    KEYMAP(KEYCODE_C, REDKEY_C);                                                    \
    KEYMAP(KEYCODE_V, REDKEY_V);                                                    \
    KEYMAP(KEYCODE_B, REDKEY_B);                                                    \
    KEYMAP(KEYCODE_N, REDKEY_N);                                                    \
    KEYMAP(KEYCODE_M, REDKEY_M);                                                    \
    KEYMAP(KEYCODE_COMMA, REDKEY_COMMA);                                            \
    KEYMAP(KEYCODE_PERIOD, REDKEY_PERIOD);                                          \
    KEYMAP(KEYCODE_SLASH, REDKEY_SLASH);                                            \
    KEYMAP(KEYCODE_R_SHIFT, REDKEY_R_SHIFT);                                        \
    KEYMAP(KEYCODE_PAD_MULTIPLY, REDKEY_PAD_MULTIPLY);                              \
    KEYMAP(KEYCODE_L_ALT, REDKEY_L_ALT);                                            \
    KEYMAP(KEYCODE_SPACE, REDKEY_SPACE);                                            \
    KEYMAP(KEYCODE_CAPS_LOCK, REDKEY_CAPS_LOCK);                                    \
    KEYMAP(KEYCODE_F1, REDKEY_F1);                                                  \
    KEYMAP(KEYCODE_F2, REDKEY_F2);                                                  \
    KEYMAP(KEYCODE_F3, REDKEY_F3);                                                  \
    KEYMAP(KEYCODE_F4, REDKEY_F4);                                                  \
    KEYMAP(KEYCODE_F5, REDKEY_F5);                                                  \
    KEYMAP(KEYCODE_F6, REDKEY_F6);                                                  \
    KEYMAP(KEYCODE_F7, REDKEY_F7);                                                  \
    KEYMAP(KEYCODE_F8, REDKEY_F8);                                                  \
    KEYMAP(KEYCODE_F9, REDKEY_F9);                                                  \
    KEYMAP(KEYCODE_F10, REDKEY_F10);                                                \
    KEYMAP(KEYCODE_NUM_LOCK, REDKEY_NUM_LOCK);                                      \
    KEYMAP(KEYCODE_SCROLL_LOCK, REDKEY_SCROLL_LOCK);                                \
    KEYMAP(KEYCODE_PAD_7, REDKEY_PAD_7);                                            \
    KEYMAP(KEYCODE_PAD_8, REDKEY_PAD_8);                                            \
    KEYMAP(KEYCODE_PAD_9, REDKEY_PAD_9);                                            \
    KEYMAP(KEYCODE_PAD_SUBTRACT, REDKEY_PAD_MINUS);                                 \
    KEYMAP(KEYCODE_PAD_4, REDKEY_PAD_4);                                            \
    KEYMAP(KEYCODE_PAD_5, REDKEY_PAD_5);                                            \
    KEYMAP(KEYCODE_PAD_6, REDKEY_PAD_6);                                            \
    KEYMAP(KEYCODE_PAD_ADD, REDKEY_PAD_PLUS);                                       \
    KEYMAP(KEYCODE_PAD_1, REDKEY_PAD_1);                                            \
    KEYMAP(KEYCODE_PAD_2, REDKEY_PAD_2);                                            \
    KEYMAP(KEYCODE_PAD_3, REDKEY_PAD_3);                                            \
    KEYMAP(KEYCODE_PAD_0, REDKEY_PAD_0);                                            \
    KEYMAP(KEYCODE_PAD_DEL, REDKEY_PAD_POINT);                                      \
    KEYMAP(KEYCODE_EUROPEAN, REDKEY_EUROPEAN);                                      \
    KEYMAP(KEYCODE_F11, REDKEY_F11);                                                \
    KEYMAP(KEYCODE_F12, REDKEY_F12);                                                \
    KEYMAP(KEYCODE_JAPANESE_BACKSLASH, REDKEY_JAPANESE_BACKSLASH);                  \
    KEYMAP(KEYCODE_JAPANESE_HENKAN, REDKEY_JAPANESE_HENKAN);                        \
    KEYMAP(KEYCODE_JAPANESE_HIRAGANA_KATAKANA, REDKEY_JAPANESE_HIRAGANA_KATAKANA);  \
    KEYMAP(KEYCODE_JAPANESE_MUHENKAN, REDKEY_JAPANESE_MUHENKAN);                    \
    KEYMAP(KEYCODE_PAD_ENTER, REDKEY_PAD_ENTER);                                    \
    KEYMAP(KEYCODE_R_CONTROL, REDKEY_R_CTRL);                                       \
    KEYMAP(KEYCODE_PAD_DEVIDE, REDKEY_PAD_DIVIDE);                                  \
    KEYMAP(KEYCODE_PRINT, REDKEY_CTRL_PRINT_SCREEN);                                \
    KEYMAP(KEYCODE_R_ALT, REDKEY_R_ALT);                                            \
    KEYMAP(KEYCODE_HOME, REDKEY_HOME);                                              \
    KEYMAP(KEYCODE_UP, REDKEY_UP);                                                  \
    KEYMAP(KEYCODE_PAGE_UP, REDKEY_PAGEUP);                                         \
    KEYMAP(KEYCODE_LEFT, REDKEY_LEFT);                                              \
    KEYMAP(KEYCODE_RIGHT, REDKEY_RIGHT);                                            \
    KEYMAP(KEYCODE_END, REDKEY_END);                                                \
    KEYMAP(KEYCODE_DOWN, REDKEY_DOWN);                                              \
    KEYMAP(KEYCODE_PAGE_DOWN, REDKEY_PAGEDOWN);                                     \
    KEYMAP(KEYCODE_INSERT, REDKEY_INSERT);                                          \
    KEYMAP(KEYCODE_DELETE, REDKEY_DELETE);                                          \
    KEYMAP(KEYCODE_PAUSE, REDKEY_PAUSE);                                            \
                                                                                    \
    KEYMAP(KEYCODE_YEN, REDKEY_JAPANESE_YEN);                                       \
    KEYMAP(KEYCODE_L_COMMAND, REDKEY_LEFT_CMD);                                     \
    KEYMAP(KEYCODE_R_COMMAND, REDKEY_RIGHT_CMD);                                    \
    KEYMAP(KEYCODE_MENU, REDKEY_MENU);                                              \
    KEYMAP(KEYCODE_HANGUL, REDKEY_KOREAN_HANGUL);                                   \
    KEYMAP(KEYCODE_HANGUL_HANJA, REDKEY_KOREAN_HANGUL_HANJA);

static void init_evdev_map()
{
    #define KEYMAP(key_code, red_key)  keycode_map[EVDEV_##key_code] = red_key
    INIT_MAP;
    KEYMAP(KEYCODE_MUTE, REDKEY_MUTE);
    KEYMAP(KEYCODE_VOLUME_DOWN, REDKEY_VOLUME_DOWN);
    KEYMAP(KEYCODE_VOLUME_UP, REDKEY_VOLUME_UP);
    #undef KEYMAP
}

static void init_kbd_map()
{
    #define KEYMAP(key_code, red_key)  keycode_map[KBD_##key_code] = red_key
    INIT_MAP;
    #undef KEYMAP
}

static void init_key_map()
{
    query_keyboard();
    memset(keycode_map, 0, sizeof(keycode_map));
    if (using_evdev) {
        LOG_INFO("using evdev mapping");
        init_evdev_map();
    } else {
        LOG_INFO("using kbd mapping");
        init_kbd_map();
    }
}

static inline RedKey to_red_key_code(unsigned int keycode)
{
    if (keycode > 255) {
        return REDKEY_INVALID;
    }
    return keycode_map[keycode];
}

#else

static RedKey key_table_0xff[256]; //miscellany

static RedKey key_table_0x00[256]; //Latin 1

static RedKey key_table_0xfe[256]; //Keyboard (XKB) Extension

#define INIT_KEY(x, red) key_table_0xff[x & 0xff] = red;

static void init_key_table_0xff()
{
    for (int i = 0; i < sizeof(key_table_0xff) / sizeof(key_table_0xff[0]); i++) {
        key_table_0xff[i] = REDKEY_INVALID;
    }

    INIT_KEY(XK_Escape, REDKEY_ESCAPE);
    INIT_KEY(XK_BackSpace, REDKEY_BACKSPACE);
    INIT_KEY(XK_Tab, REDKEY_TAB);
    INIT_KEY(XK_Return, REDKEY_ENTER);
    INIT_KEY(XK_Control_L, REDKEY_L_CTRL);
    INIT_KEY(XK_Shift_L, REDKEY_L_SHIFT);
    INIT_KEY(XK_Shift_R, REDKEY_R_SHIFT);
    INIT_KEY(XK_KP_Multiply, REDKEY_PAD_MULTIPLY);
    INIT_KEY(XK_Alt_L, REDKEY_L_ALT);
    INIT_KEY(XK_Caps_Lock, REDKEY_CAPS_LOCK);
    INIT_KEY(XK_F1, REDKEY_F1);
    INIT_KEY(XK_F2, REDKEY_F2);
    INIT_KEY(XK_F3, REDKEY_F3);
    INIT_KEY(XK_F4, REDKEY_F4);
    INIT_KEY(XK_F5, REDKEY_F5);
    INIT_KEY(XK_F6, REDKEY_F6);
    INIT_KEY(XK_F7, REDKEY_F7);
    INIT_KEY(XK_F8, REDKEY_F8);
    INIT_KEY(XK_F9, REDKEY_F9);
    INIT_KEY(XK_F10, REDKEY_F10);

    INIT_KEY(XK_Num_Lock, REDKEY_NUM_LOCK);
    INIT_KEY(XK_Scroll_Lock, REDKEY_SCROLL_LOCK);
    INIT_KEY(XK_KP_7, REDKEY_PAD_7);
    INIT_KEY(XK_KP_Home, REDKEY_PAD_7);
    INIT_KEY(XK_KP_8, REDKEY_PAD_8);
    INIT_KEY(XK_KP_Up, REDKEY_PAD_8);
    INIT_KEY(XK_KP_9, REDKEY_PAD_9);
    INIT_KEY(XK_KP_Page_Up, REDKEY_PAD_9);
    INIT_KEY(XK_KP_Subtract, REDKEY_PAD_MINUS);
    INIT_KEY(XK_KP_4, REDKEY_PAD_4);
    INIT_KEY(XK_KP_Left, REDKEY_PAD_4);
    INIT_KEY(XK_KP_5, REDKEY_PAD_5);
    INIT_KEY(XK_KP_Begin, REDKEY_PAD_5);
    INIT_KEY(XK_KP_6, REDKEY_PAD_6);
    INIT_KEY(XK_KP_Right, REDKEY_PAD_6);
    INIT_KEY(XK_KP_Add, REDKEY_PAD_PLUS);
    INIT_KEY(XK_KP_1, REDKEY_PAD_1);
    INIT_KEY(XK_KP_End, REDKEY_PAD_1);
    INIT_KEY(XK_KP_2, REDKEY_PAD_2);
    INIT_KEY(XK_KP_Down, REDKEY_PAD_2);
    INIT_KEY(XK_KP_3, REDKEY_PAD_3);
    INIT_KEY(XK_KP_Page_Down, REDKEY_PAD_3);
    INIT_KEY(XK_KP_0, REDKEY_PAD_0);
    INIT_KEY(XK_KP_Insert, REDKEY_PAD_0);
    INIT_KEY(XK_KP_Decimal, REDKEY_PAD_POINT);
    INIT_KEY(XK_KP_Delete, REDKEY_PAD_POINT);
    INIT_KEY(XK_F11, REDKEY_F11);
    INIT_KEY(XK_F12, REDKEY_F12);

    INIT_KEY(XK_KP_Enter, REDKEY_PAD_ENTER);
    INIT_KEY(XK_Control_R, REDKEY_R_CTRL);
    INIT_KEY(XK_KP_Divide, REDKEY_PAD_DIVIDE);
    INIT_KEY(XK_Print, REDKEY_CTRL_PRINT_SCREEN);

    INIT_KEY(XK_Home, REDKEY_HOME);
    INIT_KEY(XK_Up, REDKEY_UP);
    INIT_KEY(XK_Page_Up, REDKEY_PAGEUP);
    INIT_KEY(XK_Left, REDKEY_LEFT);
    INIT_KEY(XK_Right, REDKEY_RIGHT);
    INIT_KEY(XK_End, REDKEY_END);

    INIT_KEY(XK_Down, REDKEY_DOWN);
    INIT_KEY(XK_Page_Down, REDKEY_PAGEDOWN);
    INIT_KEY(XK_Insert, REDKEY_INSERT);
    INIT_KEY(XK_Delete, REDKEY_DELETE);
    INIT_KEY(XK_Super_L, REDKEY_LEFT_CMD);
    INIT_KEY(XK_Super_R, REDKEY_RIGHT_CMD);
    INIT_KEY(XK_Menu, REDKEY_MENU);
    INIT_KEY(XK_Pause, REDKEY_CTRL_BREAK);
}

#undef INIT_KEY
#define INIT_KEY(x, red) key_table_0x00[x & 0xff] = red;

static void init_key_table_0x00()
{
    for (int i = 0; i < sizeof(key_table_0x00) / sizeof(key_table_0x00[0]); i++) {
        key_table_0x00[i] = REDKEY_INVALID;
    }

    INIT_KEY(XK_1, REDKEY_1);
    INIT_KEY(XK_2, REDKEY_2);
    INIT_KEY(XK_3, REDKEY_3);
    INIT_KEY(XK_4, REDKEY_4);
    INIT_KEY(XK_5, REDKEY_5);
    INIT_KEY(XK_6, REDKEY_6);
    INIT_KEY(XK_7, REDKEY_7);
    INIT_KEY(XK_8, REDKEY_8);
    INIT_KEY(XK_9, REDKEY_9);
    INIT_KEY(XK_0, REDKEY_0);
    INIT_KEY(XK_minus, REDKEY_MINUS);
    INIT_KEY(XK_equal, REDKEY_EQUALS);
    INIT_KEY(XK_q, REDKEY_Q);
    INIT_KEY(XK_w, REDKEY_W);
    INIT_KEY(XK_e, REDKEY_E);
    INIT_KEY(XK_r, REDKEY_R);
    INIT_KEY(XK_t, REDKEY_T);
    INIT_KEY(XK_y, REDKEY_Y);
    INIT_KEY(XK_u, REDKEY_U);
    INIT_KEY(XK_i, REDKEY_I);
    INIT_KEY(XK_o, REDKEY_O);
    INIT_KEY(XK_p, REDKEY_P);
    INIT_KEY(XK_bracketleft, REDKEY_L_BRACKET);
    INIT_KEY(XK_bracketright, REDKEY_R_BRACKET);
    INIT_KEY(XK_a, REDKEY_A);
    INIT_KEY(XK_s, REDKEY_S);
    INIT_KEY(XK_e, REDKEY_E);
    INIT_KEY(XK_d, REDKEY_D);
    INIT_KEY(XK_f, REDKEY_F);
    INIT_KEY(XK_g, REDKEY_G);
    INIT_KEY(XK_h, REDKEY_H);
    INIT_KEY(XK_j, REDKEY_J);
    INIT_KEY(XK_k, REDKEY_K);
    INIT_KEY(XK_l, REDKEY_L);
    INIT_KEY(XK_semicolon, REDKEY_SEMICOLON);
    INIT_KEY(XK_quoteright, REDKEY_QUOTE);
    INIT_KEY(XK_quoteleft, REDKEY_BACK_QUOTE);
    INIT_KEY(XK_backslash, REDKEY_BACK_SLASH);
    INIT_KEY(XK_z, REDKEY_Z);
    INIT_KEY(XK_x, REDKEY_X);
    INIT_KEY(XK_c, REDKEY_C);
    INIT_KEY(XK_v, REDKEY_V);
    INIT_KEY(XK_b, REDKEY_B);
    INIT_KEY(XK_n, REDKEY_N);
    INIT_KEY(XK_m, REDKEY_M);
    INIT_KEY(XK_comma, REDKEY_COMMA);
    INIT_KEY(XK_period, REDKEY_PERIOD);
    INIT_KEY(XK_slash, REDKEY_SLASH);
    INIT_KEY(XK_space, REDKEY_SPACE);
}

#undef INIT_KEY
#define INIT_KEY(x, red) key_table_0xfe[x & 0xff] = red;

static void init_key_table_0xfe()
{
    for (int i = 0; i < sizeof(key_table_0xfe) / sizeof(key_table_0xfe[0]); i++) {
        key_table_0xfe[i] = REDKEY_INVALID;
    }

    INIT_KEY(XK_ISO_Level3_Shift, REDKEY_R_ALT);
}

static inline RedKey to_red_key_code(unsigned int keycode)
{
    XLockDisplay(x_display);
    KeySym sym = XKeycodeToKeysym(x_display, keycode, 0);
    XUnlockDisplay(x_display);
    RedKey red_key;

    if (sym == NoSymbol) {
        DBG(0, "no symbol for %d", keycode);
    }

    switch (sym >> 8) {
    case 0x00:
        red_key = key_table_0x00[sym & 0xff];
        break;
    case 0xff:
        red_key = key_table_0xff[sym & 0xff];
        break;
    case 0xfe:
        red_key = key_table_0xfe[sym & 0xff];
        break;
    default:
        DBG(0, "unsupported key set %lu", (sym >> 8));
        return REDKEY_INVALID;
    }

    if (red_key == REDKEY_INVALID) {
        DBG(0, "no valid key mapping for keycode %u", keycode);
    }
    return red_key;
}

#endif

static inline int to_red_buttons_state(unsigned int state)
{
    return ((state & Button1Mask) ? SPICE_MOUSE_BUTTON_MASK_LEFT : 0) |
           ((state & Button2Mask) ? SPICE_MOUSE_BUTTON_MASK_MIDDLE : 0) |
           ((state & Button3Mask) ? SPICE_MOUSE_BUTTON_MASK_RIGHT : 0);
}

static inline SpiceMouseButton to_red_button(unsigned int botton, unsigned int& state, bool press)
{
    unsigned int mask = 0;
    SpiceMouseButton ret;

    switch (botton) {
    case Button1:
        mask = SPICE_MOUSE_BUTTON_MASK_LEFT;
        ret = SPICE_MOUSE_BUTTON_LEFT;
        break;
    case Button2:
        mask = SPICE_MOUSE_BUTTON_MASK_MIDDLE;
        ret = SPICE_MOUSE_BUTTON_MIDDLE;
        break;
    case Button3:
        mask = SPICE_MOUSE_BUTTON_MASK_RIGHT;
        ret = SPICE_MOUSE_BUTTON_RIGHT;
        break;
    case Button4:
        ret = SPICE_MOUSE_BUTTON_UP;
        break;
    case Button5:
        ret = SPICE_MOUSE_BUTTON_DOWN;
        break;
    default:
        ret = SPICE_MOUSE_BUTTON_INVALID;
    }
    if (press) {
        state |= mask;
    } else {
        state &= ~mask;
    }
    return ret;
}

void RedWindow_p::handle_key_press_event(RedWindow& window, XKeyEvent* event)
{
    static int buf_size = 0;
    static wchar_t* utf32_buf = NULL;

    KeySym key_sym;
    Status status;
    int len;

    window.get_listener().on_key_press(to_red_key_code(event->keycode));

    if (x_input_context != NULL) {
        for (;;) {
            XLockDisplay(x_display);
            len = XwcLookupString(x_input_context, event, utf32_buf, buf_size, &key_sym, &status);
            XUnlockDisplay(x_display);
            if (status != XBufferOverflow) {
                break;
            }

            if (utf32_buf) {
                delete [] utf32_buf;
            }
            utf32_buf = new wchar_t[len];
            buf_size = len;
        }

        switch (status) {
        case XLookupChars:
        case XLookupBoth: {
            uint32_t* now = (uint32_t*)utf32_buf;
            uint32_t* end = now + len;

            for (; now < end; now++) {
                window.get_listener().on_char(*now);
            }
            break;
        }

        case XLookupNone:
        case XLookupKeySym:
            break;
        default:
            THROW("unexpected status %d", status);
        }
    } else { /* no XIM */
        unsigned char buffer[16];
        int i;

        XLockDisplay(x_display);
        len = XLookupString(event, (char *)buffer, sizeof(buffer), NULL, NULL);
        XUnlockDisplay(x_display);
        for (i = 0; i < len; i++) {
            window.get_listener().on_char((uint32_t)buffer[i]);
        }
    }
}

void RedWindow_p::win_proc(XEvent& event)
{
    XPointer window_pointer;
    RedWindow* red_window;
    int res;

    XLockDisplay(x_display);
    res = XFindContext(x_display, event.xany.window, user_data_context, &window_pointer);
    XUnlockDisplay(x_display);
    if (res) {
        THROW("no user data");
    }
    red_window = (RedWindow*)window_pointer;
    switch (event.type) {
    case MotionNotify: {
        SpicePoint size = red_window->get_size();
        if (event.xmotion.x >= 0 && event.xmotion.y >= 0 &&
            event.xmotion.x < size.x && event.xmotion.y < size.y) {
            SpicePoint origin = red_window->get_origin();
            red_window->get_listener().on_pointer_motion(event.xmotion.x - origin.x,
                                                         event.xmotion.y - origin.y,
                                                         to_red_buttons_state(event.xmotion.state));
        }
        break;
    }
    case KeyPress:
        red_window->handle_key_press_event(*red_window, &event.xkey);
        red_window->_last_event_time = event.xkey.time;
        XChangeProperty(x_display, red_window->_win, wm_user_time,
                        XA_CARDINAL, 32, PropModeReplace,
                        (unsigned char *)&event.xkey.time, 1);
        break;
    case KeyRelease: {
        RedKey key = to_red_key_code(event.xkey.keycode);
        XEvent next_event;
        XLockDisplay(x_display);
        Bool check = XCheckWindowEvent(x_display, red_window->_win, ~long(0), &next_event);
        XUnlockDisplay(x_display);
        if (check) {
            XPutBackEvent(x_display, &next_event);
            if ((next_event.type == KeyPress) &&
                (event.xkey.keycode == next_event.xkey.keycode) &&
                (event.xkey.time == next_event.xkey.time)) {
                break;
            }
        }
        if (key != REDKEY_KOREAN_HANGUL && key != REDKEY_KOREAN_HANGUL_HANJA) {
            red_window->get_listener().on_key_release(key);
        }
        break;
    }
    case ButtonPress: {
        unsigned int state = to_red_buttons_state(event.xbutton.state);
        SpiceMouseButton button = to_red_button(event.xbutton.button, state, true);
        if (button == SPICE_MOUSE_BUTTON_INVALID) {
            DBG(0, "ButtonPress: invalid button %u", event.xbutton.button);
            break;
        }
        red_window->get_listener().on_mouse_button_press(button, state);
        red_window->_last_event_time = event.xkey.time;
        XChangeProperty(x_display, red_window->_win, wm_user_time,
                        XA_CARDINAL, 32, PropModeReplace,
                        (unsigned char *)&event.xbutton.time, 1);
        break;
    }
    case ButtonRelease: {
        unsigned int state = to_red_buttons_state(event.xbutton.state);
        SpiceMouseButton button = to_red_button(event.xbutton.button, state, false);
        if (button == SPICE_MOUSE_BUTTON_INVALID) {
            DBG(0, "ButtonRelease: invalid button %u", event.xbutton.button);
            break;
        }
        red_window->get_listener().on_mouse_button_release(button, state);
        break;
    }
    case Expose: {
        SpicePoint origin;
        SpiceRect area;

        origin = red_window->get_origin();
        area.left = event.xexpose.x - origin.x;
        area.right = area.left + event.xexpose.width;
        area.top = event.xexpose.y - origin.y;
        area.bottom = area.top + event.xexpose.height;
        red_window->get_listener().on_exposed_rect(area);
        break;
    }
    case FocusIn:
        /* Ignore focus events caused by grabbed (hotkeys) */
        if (event.xfocus.mode == NotifyWhileGrabbed) {
            break;
        }

        if (event.xany.serial < focus_serial) {
            DBG(0, "Ignored FocusIn win=%p (serial=%d, Last foucs serial=%d)",
                   red_window,  event.xany.serial, focus_serial);
            break;
        }

        if (!red_window->_ignore_foucs) {
            RedWindow* prev_focus_window = focus_window;
            focus_window = red_window;
            focus_serial = event.xany.serial;
            if (prev_focus_window && (red_window != prev_focus_window)) {
                prev_focus_window->on_focus_out();
            }
            red_window->on_focus_in();
        } else {
            red_window->_shadow_foucs_state = true;
            memcpy(&red_window->_shadow_focus_event, &event, sizeof(XEvent));
        }
        break;
    case FocusOut:
        /* Ignore focus events caused by grabbed (hotkeys) */
        if (event.xfocus.mode == NotifyWhileGrabbed) {
            break;
        }

        if (event.xany.serial <= focus_serial) {
            DBG(0, "Ignored FocusOut win=%p (serial=%d, Last foucs serial=%d)",
                   red_window, event.xany.serial, focus_serial);
            break;
        }

        if (!red_window->_ignore_foucs) {
            focus_serial = event.xany.serial;
            if (red_window != focus_window) {
                break;
            }
            focus_window = NULL;
            red_window->on_focus_out();
        } else {
            red_window->_shadow_foucs_state = false;
            memcpy(&red_window->_shadow_focus_event, &event, sizeof(XEvent));
        }
        break;
    case ConfigureNotify:
        break;
    case ClientMessage:
        if (event.xclient.message_type == wm_protocol_atom) {
            ASSERT(event.xclient.format == 32);
            if ((Atom)event.xclient.data.l[0] == wm_delete_window_atom) {
                DBG(0, "wm_delete_window");
                Platform::send_quit_request();
            }
        }
        break;
    case DestroyNotify:
        break;
    case MapNotify:
        red_window->set_visibale(true);
        break;
    case UnmapNotify:
        red_window->set_visibale(false);
        break;
    case EnterNotify:
        if (!red_window->_ignore_pointer) {
            SpicePoint origin = red_window->get_origin();
            red_window->on_pointer_enter(event.xcrossing.x - origin.x, event.xcrossing.y - origin.y,
                                         to_red_buttons_state(event.xcrossing.state));
        } else {
            red_window->_shadow_pointer_state = true;
            memcpy(&red_window->_shadow_pointer_event, &event, sizeof(XEvent));
        }
        break;
    case LeaveNotify:
        if (!red_window->_ignore_pointer) {
             red_window->on_pointer_leave();
        } else {
            red_window->_shadow_pointer_state = false;
            memcpy(&red_window->_shadow_pointer_event, &event, sizeof(XEvent));
        }
        break;
    }
}

void RedWindow_p::sync(bool shadowed)
{
    XEvent event;

    if (shadowed) {
        _ignore_foucs = true;
        _ignore_pointer = true;
        _shadow_foucs_state = _focused;
        _shadow_pointer_state = _pointer_in_window;
        _shadow_focus_event.xany.serial = 0;
    }

    XLockDisplay(x_display);
    XSync(x_display, False);
    while (XCheckWindowEvent(x_display, _win, ~long(0), &event)) {
        XUnlockDisplay(x_display);
        win_proc(event);
        XLockDisplay(x_display);
    }
    XUnlockDisplay(x_display);

    if (!shadowed) {
        return;
    }
    _ignore_foucs = false;
    _ignore_pointer = false;
    if (_shadow_foucs_state != _focused) {
        DBG(0, "put back shadowed focus event");
        XPutBackEvent(x_display, &_shadow_focus_event);
    } else if (_shadow_focus_event.xany.serial > 0) {
        focus_serial = _shadow_focus_event.xany.serial;
    }
    if (_shadow_pointer_state != _pointer_in_window) {
        DBG(0, "put back shadowed pointer event");
        XPutBackEvent(x_display, &_shadow_pointer_event);
    }
}

void RedWindow_p::wait_for_reparent()
{
    XEvent event;
    for (int i = 0; i < 50; i++) {
        XLockDisplay(x_display);
        bool check = XCheckTypedWindowEvent(x_display, _win, ReparentNotify, &event);
        XUnlockDisplay(x_display);
        if (check) {
            return;
        }
        usleep(20 * 1000);
        // HDG: why?? this makes no sense
        XLockDisplay(x_display);
        XSync(x_display, False);
        XUnlockDisplay(x_display);
    }
    DBG(0, "failed");
}

void RedWindow_p::wait_for_map()
{
    bool wait_parent = _expect_parent;
    while (!_visibale) {
        XEvent event;
        XLockDisplay(x_display);
        XWindowEvent(x_display, _win, ~0, &event);
        XUnlockDisplay(x_display);
        switch (event.type) {
        case ReparentNotify:
            wait_parent = false;
            break;
        case MapNotify:
            _visibale = true;
            break;
        default:
            //todo: post state messages to app message queue instead of
            //      calling win_proc
            win_proc(event);
        }
    }

    if (wait_parent) {
        wait_for_reparent();
    }
}

void RedWindow_p::wait_for_unmap()
{
    bool wait_parent = _expect_parent;
    while (_visibale) {
        XEvent event;
        XLockDisplay(x_display);
        XWindowEvent(x_display, _win, ~0, &event);
        XUnlockDisplay(x_display);
        switch (event.type) {
        case ReparentNotify:
            wait_parent = false;
            break;
        case UnmapNotify:
            _visibale = false;
            break;
        //default:
        //    win_proc(event);
        }
    }

    if (wait_parent) {
        wait_for_reparent();
    }
}

#ifdef USE_OPENGL
void RedWindow_p::set_glx(int width, int height)
{
    if (_glcont_copy) {
        XLockDisplay(x_display);
        XSync(x_display, False);
        XUnlockDisplay(x_display);
        glXMakeCurrent(x_display, _win, _glcont_copy);
        //glDrawBuffer(GL_FRONT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0, width, 0, height);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glViewport(0, 0, width, height);
        glColor3f(1, 1, 1);
        glEnable(GL_TEXTURE_2D);
        GLC_ERROR_TEST_FINISH;
    }
}
#endif // USE_OPENGL

void RedWindow_p::set_minmax(PixelsSource_p& pix_source)
{
    //todo: auto res
    XSizeHints* size_hints = XAllocSizeHints();
    ASSERT(size_hints);
    size_hints->flags = PMinSize | PMaxSize;
    if (_red_window->_type == RedWindow::TYPE_FULLSCREEN) {
        /* Some window managers won't allow full screen mode with a fixed
           width / height */
        size_hints->min_width = 1;
        size_hints->max_width = 65535;
        size_hints->min_height = 1;
        size_hints->max_height = 65535;
    } else {
        size_hints->min_width = size_hints->max_width = _width;
        size_hints->min_height = size_hints->max_height = _height;
    }
    XSetWMNormalHints(x_display, _win, size_hints);
    XFree(size_hints);
    pix_source.x_drawable.height = _height;
    pix_source.x_drawable.width = _width;
}

Cursor RedWindow_p::create_invisible_cursor(Window window)
{
    XColor color;
    char data[1] = {0};
    Window root_window = RootWindow(x_display, DefaultScreen(x_display));
    XLockDisplay(x_display);
    Pixmap blank = XCreateBitmapFromData(x_display, root_window, data, 1, 1);
    Cursor cursor = XCreatePixmapCursor(x_display, blank, blank, &color, &color, 0, 0);
    XUnlockDisplay(x_display);
    XFreePixmap(x_display, blank);
    return cursor;
}

RedWindow_p::RedWindow_p()
    : _win (None)
    , _show_pos_valid (false)
#ifdef USE_OPENGL
    , _glcont_copy (NULL)
#endif // USE_OPENGL
    , _icon (NULL)
    , _focused (false)
    , _ignore_foucs (false)
    , _pointer_in_window (false)
    , _ignore_pointer (false)
    ,_width (200)
    ,_height (200)
    ,_last_event_time (0)
{
}

void RedWindow_p::destroy(RedWindow& red_window, PixelsSource_p& pix_source)
{
    XEvent event;

    if (_win == None) {
        return;
    }

    if (focus_window == &red_window) {
        focus_window = NULL;
        red_window.on_focus_out();
    }

    XPlatform::cleare_win_proc(_win);
    XSelectInput(x_display, _win, 0);

    XLockDisplay(x_display);
    XSync(x_display, False);
    while (XCheckWindowEvent(x_display, _win, ~long(0), &event));
    XUnlockDisplay(x_display);

    Window window = _win;
    _win = None;
    XFreeCursor(x_display, _invisible_cursor);
    _invisible_cursor = None;
    XDeleteContext(x_display, window, user_data_context);
#ifdef USE_OPENGL
    if (_glcont_copy) {
        glXDestroyContext(x_display, _glcont_copy);
        _glcont_copy = NULL;
    }
#endif // USE_OPENGL
    XDestroyWindow(x_display, window);
    XFreeColormap(x_display, _colormap);
    XFreeGC(x_display, pix_source.x_drawable.gc);
    pix_source.x_drawable.gc = NULL;
    pix_source.x_drawable.drawable = None;
    if (_icon) {
        _icon->unref();
    }
}

void RedWindow_p::create(RedWindow& red_window, PixelsSource_p& pix_source,
                         int x, int y, int in_screen)
{
    Window window = None;
    Cursor cursor = None;
    GC gc = NULL;

    Window root_window = RootWindow(x_display, in_screen);
    XSetWindowAttributes win_attributes;

    unsigned long mask = CWBorderPixel | CWEventMask;
    win_attributes.border_pixel = 1;
    win_attributes.event_mask = StructureNotifyMask | SubstructureNotifyMask | ExposureMask |
                                KeyPressMask | KeyReleaseMask | ButtonPressMask |
                                ButtonReleaseMask | PointerMotionMask | FocusChangeMask |
                                EnterWindowMask | LeaveWindowMask;

    XLockDisplay(x_display);
    _colormap = XCreateColormap(x_display, root_window, XPlatform::get_vinfo()[in_screen]->visual,
                               AllocNone);
    win_attributes.colormap = _colormap;
    mask |= CWColormap;
    window = XCreateWindow(x_display, root_window, x, y,
                           _width, _height, 0, XPlatform::get_vinfo()[in_screen]->depth,
                           InputOutput, XPlatform::get_vinfo()[in_screen]->visual, mask,
                           &win_attributes);
    XUnlockDisplay(x_display);

    if (!window) {
        THROW("create X window failed");
    }

    try {
        int res;
        XClassHint *class_hint;

        XLockDisplay(x_display);
        res = XSaveContext(x_display, window, user_data_context, (XPointer)&red_window);
        XUnlockDisplay(x_display);
        if (res) {
            THROW("set win usr data failed");
        }

        XSetWMProtocols(x_display, window, &wm_delete_window_atom, 1);
        class_hint = XAllocClassHint();
        if (!class_hint) {
            THROW("allocating class hint failed");
        }
        class_hint->res_name = (char *)"spicec";
        class_hint->res_class = (char *)"spicec";
        XSetClassHint(x_display, window, class_hint);
        XFree(class_hint);

        XGCValues gc_vals;
        XLockDisplay(x_display);
        gc = XCreateGC(x_display, window, 0, &gc_vals);
        XUnlockDisplay(x_display);
        if (!gc) {
            THROW("create gc failed");
        }

        cursor = create_invisible_cursor(window);
        if (!cursor) {
            THROW("create invisible cursor failed");
        }

        XPlatform::set_win_proc(window, win_proc);
    } catch (...) {
        if (gc) {
            XFreeGC(x_display, gc);
        }

        XDeleteContext(x_display, window, user_data_context);
        XDestroyWindow(x_display, window);
        if (cursor != None) {
            XFreeCursor(x_display, cursor);
        }

        throw;
    }
    _screen = in_screen;
    _win = window;
    _invisible_cursor = cursor;
    _show_pos.x = x;
    _show_pos.y = y;
    _visibale = false;
    _expect_parent = false;
    _red_window = &red_window;
    pix_source.type = PIXELS_SOURCE_TYPE_X_DRAWABLE;
    pix_source.x_drawable.drawable = window;
    pix_source.x_drawable.screen = _screen;
    pix_source.x_drawable.gc = gc;
    set_minmax(pix_source);
    sync();
}

void RedWindow_p::migrate(RedWindow& red_window, PixelsSource_p& pix_source, int to_screen)
{
    if (to_screen == _screen) {
        return;
    }
    XTextProperty text_pro;
    XLockDisplay(x_display);
    bool valid_title = XGetWMName(x_display, _win, &text_pro) && text_pro.value;
    XUnlockDisplay(x_display);
    destroy(red_window, pix_source);
    create(red_window, pix_source, _show_pos.x, _show_pos.y, to_screen);
    if (valid_title) {
        XSetWMName(x_display, _win, &text_pro);
        XFree(text_pro.value); //???
    }
    if (_icon) {
        AutoRef<Icon> red(_icon->ref());
        red_window.set_icon(_icon);
    }
}

void RedWindow_p::move_to_current_desktop()
{
    Window root = RootWindow(x_display, _screen);
    Atom actual_type_return;
    int actual_format_return;
    unsigned long bytes_after_return;
    unsigned long nitems_return;
    unsigned char *prop_return;
    long desktop = ~long(0);
    int status;

    XLockDisplay(x_display);
    status = XGetWindowProperty(x_display, root, wm_current_desktop, 0, 1, False, AnyPropertyType,
                                &actual_type_return, &actual_format_return, &nitems_return,
                                &bytes_after_return, &prop_return);
    if ((status  == Success) && (actual_type_return != None) && (actual_format_return == 32)) {
        desktop = *(uint32_t *)prop_return;
    } else {
        DBG(0, "get current desktop failed");
    }
    if (status == Success)
        XFree(prop_return);
    XUnlockDisplay(x_display);

    XEvent xevent;
    xevent.type = ClientMessage;
    xevent.xclient.window = _win;
    xevent.xclient.message_type = wm_desktop;
    xevent.xclient.format = 32;
    xevent.xclient.data.l[0] = desktop;
    xevent.xclient.data.l[1] = 0;
    xevent.xclient.data.l[2] = 0;
    xevent.xclient.data.l[3] = 0;
    xevent.xclient.data.l[4] = 0;
    if (!XSendEvent(x_display, root, False, SubstructureNotifyMask | SubstructureRedirectMask,
                    &xevent)) {
        DBG(0, "failed");
    }
}

RedWindow::RedWindow(RedWindow::Listener& listener, int screen)
    : _listener (listener)
    , _type (TYPE_NORMAL)
    , _local_cursor (NULL)
    , _cursor_visible (true)
    , _trace_key_interception (false)
    , _key_interception_on (false)
    , _menu (NULL)
{
    ASSERT(x_display);
    create(*this, *(PixelsSource_p*)get_opaque(), 0, 0,
           (screen == DEFAULT_SCREEN) ? DefaultScreen(x_display) : screen);
}

RedWindow::~RedWindow()
{
    destroy(*this, *(PixelsSource_p*)get_opaque());
    if (_local_cursor) {
        _local_cursor->unref();
    }
}

void RedWindow::set_title(std::string& title)
{
    XTextProperty text_prop;
    char *name = const_cast<char *>(title.c_str());
    int r;
    if (_win) {
        XLockDisplay(x_display);
        r = Xutf8TextListToTextProperty(x_display, &name, 1, XUTF8StringStyle, &text_prop);
        XUnlockDisplay(x_display);
        if (r == Success) {
            XSetWMName(x_display, _win, &text_prop);
            XFree(text_prop.value);
        } else {
            LOG_WARN("XwcTextListToTextProperty Error %d", r);
        }
    }
}

void RedWindow::set_icon(Icon* icon)
{
    if (_icon) {
        _icon->unref();
        _icon = NULL;
    }
    if (!icon) {
        return;
    }
    _icon = icon->ref();

    XWMHints* wm_hints;
    if (_win == None || !(wm_hints = XAllocWMHints())) {
        return;
    }

    try {
        XIcon* xicon = (XIcon*)icon;
        xicon->get_pixmaps(_screen, wm_hints->icon_pixmap, wm_hints->icon_mask);
        wm_hints->flags = IconPixmapHint | IconMaskHint;
        XSetWMHints(x_display, _win, wm_hints);
    } catch (...) {
    }
    XFree(wm_hints);
}

static XErrorHandler old_error_handler = NULL;
static unsigned char x_error = Success;

static int x_error_handler(Display* display, XErrorEvent* error_event)
{
    x_error = error_event->error_code;
    if (error_event->error_code == BadWindow) {
        return 0;
    }
    ASSERT(old_error_handler);
    XSetErrorHandler(old_error_handler);
    old_error_handler(display, error_event);
    old_error_handler = NULL;
    return 0;
}

class AutoXErrorHandler {
public:
    AutoXErrorHandler()
    {
        ASSERT(old_error_handler == NULL);
        XLockDisplay(x_display);
        XSync(x_display, False);
        x_error = Success;
        old_error_handler = XSetErrorHandler(x_error_handler);
        XUnlockDisplay(x_display);
    }

    ~AutoXErrorHandler()
    {
        if (old_error_handler) {
            XLockDisplay(x_display);
            XSetErrorHandler(old_error_handler);
            XUnlockDisplay(x_display);
            old_error_handler = NULL;
        }
    }
};

static Window get_window_for_reposition(Window window)
{
    for (;;) {
        Window root;
        Window parent;
        Window* childrens;
        unsigned int num_childrens;
        int res;

        XLockDisplay(x_display);
        res = XQueryTree(x_display, window, &root, &parent, &childrens, &num_childrens);
        XUnlockDisplay(x_display);
        if (!res) {
            return None;
        }

        if (childrens) {
            XFree(childrens);
        }

        if (parent == root) {
            break;
        }
        window = parent;
    }
    return window;
}

void RedWindow::raise()
{
    AutoXErrorHandler auto_error_handler;
    int raise_retries = RAISE_RETRIES;
    for (;; --raise_retries) {
        Window window = get_window_for_reposition(_win);
        if (window != None) {
            x_error = Success;
            XRaiseWindow(x_display, window);
            if (x_error == Success) {
                break;
            }
            if (x_error != BadWindow) {
                THROW("XRaiseWindow failed");
            }
        }

        if (!raise_retries) {
            THROW("failed");
        }
        usleep(X_RETRY_DELAY_MICRO);
    }
    if (raise_retries < RAISE_RETRIES) {
        DBG(0, "retries %d", (RAISE_RETRIES - raise_retries));
    }
    sync();
}

void RedWindow::position_after(RedWindow *after)
{
    if (!after || after->_screen != _screen) {
        raise();
        return;
    }

    AutoXErrorHandler auto_error_handler;
    int position_retries = Z_POSITION_RETRIES;
    for (;; --position_retries) {
        Window sibling = get_window_for_reposition(after->get_window());
        Window self = get_window_for_reposition(_win);
        if (sibling != None && self != None) {
            XWindowChanges changes;
            changes.sibling = sibling;
            changes.stack_mode = Below;
            x_error = Success;
            XConfigureWindow(x_display, self, CWSibling | CWStackMode, &changes);
            if (x_error == Success) {
                break;
            }
            if (x_error != BadWindow) {
                THROW("XConfigureWindow failed");
            }
        }

        if (!position_retries) {
            THROW("failed");
        }
        usleep(X_RETRY_DELAY_MICRO);
    }
    if (position_retries < Z_POSITION_RETRIES) {
        DBG(0, "retries %d", (Z_POSITION_RETRIES - position_retries));
    }
}

void RedWindow::show(int screen_id)
{
    if (_visibale) {
        return;
    }

    bool wait_parent;

    if (screen_id != _screen) {
        _listener.pre_migrate();
        migrate(*this, *(PixelsSource_p*)get_opaque(), screen_id);
        _listener.post_migrate();
    }

    /* We must update min/max for fullscreen / normal switching */
    set_minmax(*(PixelsSource_p*)get_opaque());

    if (_type == TYPE_FULLSCREEN) {
        Atom state[2];
        state[0] = wm_state_above;
        state[1] = wm_state_fullscreen;
        XChangeProperty(x_display, _win, wm_state, XA_ATOM, 32, PropModeReplace,
                        (unsigned char*)state, 2);
        wait_parent = false;
    } else {
        XDeleteProperty(x_display, _win, wm_state);
        wait_parent = true;
    }
    if (_last_event_time != 0)
        XChangeProperty(x_display, _win, wm_user_time, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *)&_last_event_time, 1);
    XMapWindow(x_display, _win);
    move_to_current_desktop();
    _expect_parent = wait_parent;
    wait_for_map();
    if (_show_pos_valid) {
        move(_show_pos.x, _show_pos.y);
    }
}

static bool get_prop_32(Window win, Atom prop, uint32_t &val)
{
    Atom actual_type_return;
    int actual_format_return;
    unsigned long bytes_after_return;
    unsigned long nitems_return;
    unsigned char *prop_return;
    bool retval = false;

    XLockDisplay(x_display);
    if (XGetWindowProperty(x_display, win, prop, 0, 1, False, AnyPropertyType,
                           &actual_type_return, &actual_format_return,
                           &nitems_return, &bytes_after_return, &prop_return) == Success &&
                                                            nitems_return == 1 &&
                                                            actual_type_return != None &&
                                                            actual_format_return == 32) {
        val = *(uint32_t *)prop_return;
        retval = true;
    }
    XUnlockDisplay(x_display);

    return retval;
}

void RedWindow::external_show()
{
    Atom atom;

    DBG(0, "");
    show(_screen);
    raise();
    activate();

    XLockDisplay(x_display);
    atom = XInternAtom(x_display, "_NET_ACTIVE_WINDOW", true);
    XUnlockDisplay(x_display);
    if (atom != None) {
        Window root;
        XEvent xev;
        long eventmask;

        xev.xclient.type = ClientMessage;
        xev.xclient.serial = 0;
        xev.xclient.send_event = True;
        xev.xclient.window = _win;
        xev.xclient.message_type = atom;

        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 1;

        uint32_t user_time;
        if (get_prop_32(_win, wm_user_time, user_time)) {
            xev.xclient.data.l[1] = user_time;
        } else {
            xev.xclient.data.l[1] = 0;
        }
        xev.xclient.data.l[2] = 0;
        xev.xclient.data.l[3] = 0;
        xev.xclient.data.l[4] = 0;

        root = RootWindow(x_display, _screen),
        eventmask = SubstructureRedirectMask | SubstructureNotifyMask;

        XSendEvent(x_display, root, False, eventmask, &xev);
    }
}

void RedWindow::hide()
{
    if (!_visibale) {
        return;
    }
    on_pointer_leave();
    on_focus_out();
    XUnmapWindow(x_display, _win);
    _show_pos = get_position();
    _show_pos_valid = true;
    wait_for_unmap();
    ASSERT(!_focused);
    ASSERT(!_pointer_in_window);
    _expect_parent = false;
}

static void send_expose(Window window, int width, int height)
{
    XExposeEvent event;
    event.type = Expose;
    event.display = x_display;
    event.window = window;
    event.x = 0;
    event.y = 0;
    event.width = width;
    event.height = height;
    event.count = 0;
    XSendEvent(x_display, window, False, ExposureMask, (XEvent *)&event);
}

void RedWindow::move_and_resize(int x, int y, int width, int height)
{
    _width = width;
    _height = height;
    set_minmax(*(PixelsSource_p*)get_opaque());
    XMoveResizeWindow(x_display, _win, x, y, width, height);
    _show_pos.x = x;
    _show_pos.y = y;
    _show_pos_valid = true;
    if (_visibale) {
        send_expose(_win, width, height);
    }
}

void RedWindow::move(int x, int y)
{
    XMoveWindow(x_display, _win, x, y);
    _show_pos.x = x;
    _show_pos.y = y;
    _show_pos_valid = true;
}

void RedWindow::resize(int width, int height)
{
    _width = width;
    _height = height;
    set_minmax(*(PixelsSource_p*)get_opaque());
    XResizeWindow(x_display, _win, width, height);
    if (_visibale) {
        send_expose(_win, width, height);
    }
}

void RedWindow::activate()
{
    //todo: use _NET_ACTIVE_WINDOW
    XSetInputFocus(x_display, _win, RevertToParent, CurrentTime);
    /* kwin won't raise on focus */
    XRaiseWindow(x_display, _win);
}

void RedWindow::minimize()
{
    XIconifyWindow(x_display, _win, _screen);
    sync();
}

static bool __get_position(Window window, SpicePoint& pos)
{
    pos.x = pos.y = 0;
    for (;;) {
        XWindowAttributes attrib;
        Window root;
        Window parent;
        Window* childrens;
        unsigned int num_childrens;
        int res;

        XLockDisplay(x_display);
        res = XGetWindowAttributes(x_display, window, &attrib);
        XUnlockDisplay(x_display);
        if (!res) {
            return false;
        }
        pos.x += attrib.x;
        pos.y += attrib.y;

        XLockDisplay(x_display);
        res = XQueryTree(x_display, window, &root, &parent, &childrens, &num_childrens);
        XUnlockDisplay(x_display);
        if (!res) {
            return false;
        }

        if (childrens) {
            XFree(childrens);
        }

        if (parent == None) {
            break;
        }
        window = parent;
    }
    return true;
}

SpicePoint RedWindow::get_position()
{
    SpicePoint pos;

    AutoXErrorHandler auto_error_handler;
    int get_position_retries = GET_POSITION_RETRIES;
    for (;; --get_position_retries) {
        if (__get_position(_win, pos)) {
            break;
        }
        if (!get_position_retries) {
            THROW("failed");
        }
        usleep(X_RETRY_DELAY_MICRO);
    }

    if (get_position_retries < GET_POSITION_RETRIES) {
        DBG(0, "retries %d", (GET_POSITION_RETRIES - get_position_retries));
    }
    return pos;
}

void RedWindow::do_start_key_interception()
{
    // Working with KDE: XGrabKeyboard generate focusout and focusin events
    // while we have the focus. This behavior trigger infinite recursive. for
    // that reason we temporary disable focus event handling. Same happens
    // LeaveNotify and EnterNotify.

    ASSERT(_focused);
    XLockDisplay(x_display);
    XGrabKeyboard(x_display, _win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    XUnlockDisplay(x_display);
    sync(true);
    _listener.on_start_key_interception();
    _key_interception_on = true;
}

void RedWindow::do_stop_key_interception()
{
    XLockDisplay(x_display);
    XUngrabKeyboard(x_display, CurrentTime);
    XUnlockDisplay(x_display);
    sync(true);
    _key_interception_on = false;
    _listener.on_stop_key_interception();
}

void RedWindow::start_key_interception()
{
    if (_trace_key_interception) {
        return;
    }
    _trace_key_interception = true;
    if (_pointer_in_window && _focused) {
        do_start_key_interception();
    }
}

void RedWindow::stop_key_interception()
{
    if (!_trace_key_interception) {
        return;
    }
    _trace_key_interception = false;
    if (_key_interception_on) {
        do_stop_key_interception();
    }
}

void RedWindow::set_cursor(LocalCursor* local_cursor)
{
    ASSERT(local_cursor);
    if (_local_cursor) {
        _local_cursor->unref();
    }
    _local_cursor = local_cursor->ref();
    _local_cursor->set(_win);
    _cursor_visible = true;
}

void RedWindow::show_cursor()
{
    if (!_cursor_visible) {
        if (_local_cursor) {
            _local_cursor->set(_win);
        }
        _cursor_visible = true;
    }
}

void RedWindow::hide_cursor()
{
    if (_cursor_visible) {
        XDefineCursor(x_display, _win, _invisible_cursor);
        _cursor_visible = false;
    }
}

void RedWindow::release_mouse()
{
    XLockDisplay(x_display);
    XUngrabPointer(x_display, CurrentTime);
    XUnlockDisplay(x_display);
    sync(true);
}

void RedWindow::capture_mouse()
{
    int grab_retries = MOUSE_GRAB_RETRIES;
    XLockDisplay(x_display);
    XSync(x_display, False);
    XUnlockDisplay(x_display);
    for (;; --grab_retries) {
        XLockDisplay(x_display);
        int result = XGrabPointer(x_display, _win, True, 0,
                                  GrabModeAsync, GrabModeAsync,
                                  _win, None, CurrentTime);
        XUnlockDisplay(x_display);
        if (result == GrabSuccess) {
            break;
        }

        if (!grab_retries) {
            THROW("grab pointer failed (%d)", result);
        }
        usleep(X_RETRY_DELAY_MICRO);
        DBG(0, "grab failed result=%d", result);
    }
    sync();
}

void RedWindow::set_mouse_position(int x, int y)
{
    XWarpPointer(x_display, None, _win, 0, 0, 0, 0, x + get_origin().x, y + get_origin().y);
}

SpicePoint RedWindow::get_size()
{
    XWindowAttributes attrib;
    XLockDisplay(x_display);
    XGetWindowAttributes(x_display, _win, &attrib);
    XUnlockDisplay(x_display);
    SpicePoint size;
    size.x = attrib.width;
    size.y = attrib.height;
    return size;
}

static void window_area_from_attributes(SpiceRect& area, XWindowAttributes& attrib)
{
    area.left = attrib.x;
    area.right = area.left + attrib.width;
    area.top = attrib.y;
    area.bottom = area.top + attrib.height;
}

#define FAIL_ON_BAD_WINDOW(error, format, ...)              \
    if (error) {                                            \
        if ((x_error) == BadWindow) {                       \
            return NULL;                                    \
        }                                                   \
        THROW(format, ## __VA_ARGS__);                      \
    }


static QRegion *get_visibale_region(Window window)
{
    QRegion* region = new QRegion;
    region_init(region);
    XWindowAttributes attrib;
    int res;

    XLockDisplay(x_display);
    res = XGetWindowAttributes(x_display, window, &attrib);
    XUnlockDisplay(x_display);
    if (!res) {
        return NULL;
    }

    if (attrib.map_state != IsViewable) {
        DBG(0, "not viewable");
        return region;
    }

    SpiceRect window_area;
    window_area_from_attributes(window_area, attrib);
    window_area.right -= window_area.left;
    window_area.bottom -= window_area.top;
    window_area.top = window_area.left = 0;
    region_add(region, &window_area);
    Window prev = None;
    Window root;
    Window parent;
    Window* childrens;
    unsigned int num_childrens;

    AutoXErrorHandler auto_error_handler;
    for (;;) {
        int res;

        XLockDisplay(x_display);
        res = XQueryTree(x_display, window, &root, &parent, &childrens,
                         &num_childrens);
        XUnlockDisplay(x_display);
        FAIL_ON_BAD_WINDOW(!res,
                           "%s: query X tree failed", __FUNCTION__);
        for (int i = num_childrens - 1; i >= 0 && childrens[i] != prev; i--) {

            XLockDisplay(x_display);
            res = XGetWindowAttributes(x_display, childrens[i], &attrib);
            XUnlockDisplay(x_display);
            FAIL_ON_BAD_WINDOW(!res,
                               "%s: get win attributes failed", __FUNCTION__);

            if (attrib.map_state == IsViewable) {
                window_area_from_attributes(window_area, attrib);
                window_area.left -= attrib.border_width;
                window_area.right += attrib.border_width;
                window_area.top -= attrib.border_width;
                window_area.bottom += attrib.border_width;
                region_remove(region, &window_area);
            }
        }

        if (childrens) {
            XFree(childrens);
        }

        XLockDisplay(x_display);
        res = XGetWindowAttributes(x_display, window, &attrib);
        XUnlockDisplay(x_display);
        FAIL_ON_BAD_WINDOW(!res,
                           "%s: get win attributes failed", __FUNCTION__);
        window_area_from_attributes(window_area, attrib);
        region_offset(region, window_area.left, window_area.top);

        if (parent == None) {
            break;
        }

        XLockDisplay(x_display);
        res = XGetWindowAttributes(x_display, parent, &attrib);
        XUnlockDisplay(x_display);
        FAIL_ON_BAD_WINDOW(!res,
                           "%s: get win attributes failed", __FUNCTION__);
        window_area_from_attributes(window_area, attrib);
        window_area.right -= window_area.left;
        window_area.bottom -= window_area.top;
        window_area.top = window_area.left = 0;

        QRegion parent_region;
        region_init(&parent_region);
        region_add(&parent_region, &window_area);
        region_and(region, &parent_region);
        region_destroy(&parent_region);

        prev = window;
        window = parent;
    }

    //todo: intersect with monitors
    return region;
}

class Region_p {
public:
    Region_p(QRegion* region) : _region (region) {}
    ~Region_p() { delete _region;}

    void get_bbox(SpiceRect& bbox) const
    {
        if (region_is_empty(_region)) {
            bbox.left = bbox.right = bbox.top = bbox.bottom = 0;
        } else {
            bbox.left = _region->extents.x1;
            bbox.top = _region->extents.y1;
            bbox.right = _region->extents.x2;
            bbox.bottom = _region->extents.y2;
        }
    }

    bool contains_point(int x, int y) const
    {
        return region_contains_point(_region, x, y);
    }

private:
    QRegion* _region;
};

bool RedWindow::get_mouse_anchor_point(SpicePoint& pt)
{
    QRegion* vis_region;
    int vis_region_retries = GET_VIS_REGION_RETRIES;

    while (!(vis_region = get_visibale_region(_win))) {
        if (!vis_region_retries) {
            THROW("get visible region failed");
        }
        --vis_region_retries;
        usleep(X_RETRY_DELAY_MICRO);
    }

    if (vis_region_retries < GET_VIS_REGION_RETRIES) {
        DBG(0, "retries %d", (GET_VIS_REGION_RETRIES - vis_region_retries));
    }

    Region_p region(vis_region);
    if (!find_anchor_point(region, pt)) {
        return false;
    }
    SpicePoint position = get_position();
    pt.x -= (position.x + get_origin().x);
    pt.y -= (position.y + get_origin().y);
    return true;
}

#ifdef USE_OPENGL
RedGlContext RedWindow::create_context_gl()
{
    if (XPlatform::get_fbconfig()[_screen]) {
        XLockDisplay(x_display);
        RedGlContext context = glXCreateContext(x_display, XPlatform::get_vinfo()[_screen], NULL, GL_TRUE);
        XUnlockDisplay(x_display);
        return context;
    }
    return NULL;
}

RedPbuffer RedWindow::create_pbuff(int width, int height)
{
    GLXPbuffer pbuff;
    GLXFBConfig** fb_config;

    int pbuf_attr[] = { GLX_PRESERVED_CONTENTS, True,
                        GLX_PBUFFER_WIDTH, width,
                        GLX_PBUFFER_HEIGHT, height,
                        GLX_LARGEST_PBUFFER, False,
                        0, 0 };

    fb_config = XPlatform::get_fbconfig();
    XLockDisplay(XPlatform::get_display());
    pbuff = glXCreatePbuffer(XPlatform::get_display(), fb_config[_screen][0],
                             pbuf_attr);
    XUnlockDisplay(XPlatform::get_display());

    return pbuff;
}

void RedWindow::untouch_context()
{
    glXMakeCurrent(x_display, 0, 0);
}

void RedWindow::set_type_gl()
{
    PixelsSource_p *pix_source = (PixelsSource_p*)get_opaque();

    pix_source->type = PIXELS_SOURCE_TYPE_GL_DRAWABLE;
}

void RedWindow::unset_type_gl()
{
    PixelsSource_p *pix_source = (PixelsSource_p*)get_opaque();

    pix_source->type = PIXELS_SOURCE_TYPE_X_DRAWABLE;
}

void RedWindow::set_gl_context(RedGlContext context)
{
    PixelsSource_p *pix_source = (PixelsSource_p*)get_opaque();

    pix_source->x_drawable.context = context;
}

void RedWindow::set_render_pbuff(RedPbuffer pbuff)
{
    PixelsSource_p *pix_source = (PixelsSource_p*)get_opaque();

    pix_source->x_drawable.rendertype = RENDER_TYPE_PBUFF;
    pix_source->x_drawable.pbuff = pbuff;
}

void RedWindow::set_render_fbo(GLuint fbo)
{
    PixelsSource_p *pix_source = (PixelsSource_p*)get_opaque();

    pix_source->x_drawable.rendertype = RENDER_TYPE_FBO;
    pix_source->x_drawable.fbo = fbo;
}
#endif // USE_OPENGL

int RedWindow::get_screen_num()
{
    return _screen;
}

RedDrawable::Format RedWindow::get_format()
{
  return XPlatform::get_screen_format(_screen);
}

void RedWindow::on_focus_in()
{
    if (_focused) {
        return;
    }
    _focused = true;
    if (x_input_context) {
        XLockDisplay(x_display);
        XwcResetIC(x_input_context);
        XUnlockDisplay(x_display);
    }
    XPlatform::on_focus_in();
    get_listener().on_activate();
    if (_trace_key_interception && _pointer_in_window) {
        do_start_key_interception();
    }
}

void RedWindow::on_focus_out()
{
    if (!_focused) {
        return;
    }
    _focused = false;
    if (_key_interception_on) {
        do_stop_key_interception();
    }
    get_listener().on_deactivate();
    XPlatform::on_focus_out();
}

void RedWindow::on_pointer_enter(int x, int y, unsigned int buttons_state)
{
    if (_pointer_in_window) {
        return;
    }
    _pointer_in_window = true;
    _listener.on_pointer_enter(x, y, buttons_state);
    if (_focused && _trace_key_interception) {
        do_start_key_interception();
    }
}

void RedWindow::on_pointer_leave()
{
    if (!_pointer_in_window) {
        return;
    }
    _pointer_in_window = false;
    _listener.on_pointer_leave();
    if (_key_interception_on) {
        do_stop_key_interception();
    }
}

int RedWindow::set_menu(Menu* menu)
{
    return 0;
}

void RedWindow::init()
{
    x_display = XPlatform::get_display();
    x_input_context = XPlatform::get_input_context();
    ASSERT(x_display);
    user_data_context = XUniqueContext();

    wm_protocol_atom = XInternAtom(x_display, "WM_PROTOCOLS", False);
    wm_delete_window_atom = XInternAtom(x_display, "WM_DELETE_WINDOW", False);

    wm_desktop = XInternAtom(x_display, "_NET_WM_DESKTOP", False);
    wm_current_desktop = XInternAtom(x_display, "_NET_CURRENT_DESKTOP", False);

    wm_state = XInternAtom(x_display, "_NET_WM_STATE", False);
    wm_state_above = XInternAtom(x_display, "_NET_WM_STATE_ABOVE", False);
    wm_state_fullscreen = XInternAtom(x_display, "_NET_WM_STATE_FULLSCREEN", False);

    wm_user_time = XInternAtom(x_display, "_NET_WM_USER_TIME", False);

#ifdef USE_X11_KEYCODE
    init_key_map();
#else
    init_key_table_0xff();
    init_key_table_0x00();
    init_key_table_0xfe();
#endif
}

void RedWindow::cleanup()
{
}
