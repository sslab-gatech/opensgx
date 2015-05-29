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

#ifndef _H_PLATFORM_UTILS
#define _H_PLATFORM_UTILS

#include <winsock.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "utils.h"

#define mb() __asm {lock add [esp], 0}

template<class T, class FreeRes = FreeObject<T>, intptr_t invalid = 0 >
class AutoRes {
public:
    AutoRes() : res(invalid) {}
    AutoRes(T inRes) : res(inRes) {}
    ~AutoRes() { set(invalid); }

    void set(T inRes) {if (res != invalid) free_res(res); res = inRes; }
    T get() {return res;}
    T release() {T tmp = res; res = invalid; return tmp; }
    bool valid() { return res != invalid; }

private:
    AutoRes(const AutoRes&);
    AutoRes& operator = (const AutoRes&);

private:
    T res;
    FreeRes free_res;
};

class Delete_DC {
public:
    void operator () (HDC dc) { DeleteDC(dc);}
};

typedef AutoRes<HDC, Delete_DC> AutoDC;

class Delete_Object {
public:
    void operator () (HGDIOBJ obj) { DeleteObject(obj);}
};
typedef AutoRes<HGDIOBJ, Delete_Object> AutoGDIObject;

class DeleteOGLContext {
public:
    void operator () (HGLRC ctx) { wglDeleteContext(ctx);}
};

typedef AutoRes<HGLRC, DeleteOGLContext> AutoOGLCtx;

HDC create_compatible_dc();
HBITMAP get_bitmap_res(int id);
HBITMAP get_alpha_bitmap_res(int id);

class WindowDC {
public:
    WindowDC(HWND window): _window (window), _dc (GetDC(window)) {}
    ~WindowDC() { ReleaseDC(_window, _dc);}
    HDC operator * () { return _dc;}

private:
    HWND _window;
    HDC _dc;
};

typedef AutoRes<HDC, Delete_DC> AutoReleaseDC;

const char* sys_err_to_str(int error);

#define SHUT_WR SD_SEND
#define SHUT_RD SD_RECEIVE
#define SHUT_RDWR SD_BOTH
#define MSG_NOSIGNAL 0

#define SHUTDOWN_ERR WSAESHUTDOWN
#define INTERRUPTED_ERR WSAEINTR
#define WOULDBLOCK_ERR WSAEWOULDBLOCK
#define sock_error() WSAGetLastError()
#define sock_err_message(err) sys_err_to_str(err)
int inet_aton(const char* ip, struct in_addr* in_addr);

#endif
