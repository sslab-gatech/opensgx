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

#ifndef _H_COMMON
#define _H_COMMON

#ifndef WIN32
#include "config.h"
#endif

#ifndef _WIN32_WCE
#include <errno.h>
#endif

#ifndef _WIN32
#include <inttypes.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <exception>
#include <list>
#include <string.h>

#ifdef WIN32
#ifdef __GNUC__
#define UNICODE 1
#define _UNICODE 1
#if !defined __MINGW32__
#define WINVER 0x0501
#define swprintf_s(_str, _len, _fmt, ...) \
    swprintf(_str, _fmt, ## __VA_ARGS__)
#define vsnprintf_s(_str, _len1, _len2, _fmt, _valist) \
    vsnprintf(_str, _len2, _fmt, _valist)
#define _ftime_s(_t) _ftime(_t)
#endif
#endif
#include <winsock2.h>
#include <windows.h>

#ifndef __GNUC__
#pragma warning(disable:4355)
#pragma warning(disable:4996)
#pragma warning(disable:4200)

extern const char* PACKAGE_VERSION;
#endif

#define strcasecmp stricmp

#else
#include <unistd.h>
#include <X11/X.h>
#ifdef USE_OPENGL
#include <GL/glx.h>
#endif
#endif

#ifdef __GNUC__
    #if __SIZEOF_POINTER__ == 8
    #define RED64
    #endif
#elif defined(_WIN64)
#define RED64
#endif

#if defined(_WIN32) && !defined(PRIu64)
#define PRIu64 "I64u"
#endif

#include <spice/types.h>
#include "red_types.h"

#endif
