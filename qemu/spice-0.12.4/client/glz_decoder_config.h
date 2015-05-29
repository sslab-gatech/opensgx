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

#ifndef _H_GLZ_DECODER_CONFIG
#define _H_GLZ_DECODER_CONFIG

#include <exception>
#include <sstream>
#include <stdio.h>

#include <spice/types.h>
#include <spice/macros.h>
#include "common/lz_common.h"

class GlzException: public std::exception {
public:
    GlzException(const std::string& str) : _mess (str) {}
    virtual ~GlzException() throw () {}
    virtual const char* what() const throw () {return _mess.c_str();}

private:
    std::string _mess;
};

class GlzDecoderDebug {
public:
    virtual ~GlzDecoderDebug() {}
    virtual void error(const std::string& str) = 0;
    virtual void warn(const std::string& str) = 0;
    virtual void info(const std::string& str) = 0;
};

#ifdef RED_DEBUG

#define GLZ_ASSERT(debug, x) {                                  \
    if (!(x)) {                                                 \
        std::ostringstream os;                                  \
        os << __FUNCTION__ << ": ASSERT " << #x << " failed\n"; \
        (debug).error(os.str());                                \
    }                                                           \
}
#else

#define GLZ_ASSERT(debug, x)

#endif

#define GLZ_DECODE_TO_RGB32

#endif  //_H_GLZ_DECODER_CONFIG
