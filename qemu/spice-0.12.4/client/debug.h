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

#ifndef _H_DEBUG
#define _H_DEBUG

#include <stdlib.h>
#include <sstream>

#include "platform.h"

#ifdef WIN32
#define snprintf _snprintf
#endif

#define ON_PANIC() ::abort()

#ifdef RED_DEBUG

#ifdef WIN32
#define ASSERTBREAK DebugBreak()
#else
#define ASSERTBREAK ::abort()
#endif

#define ASSERT(x) if (!(x)) {                               \
    printf("%s: ASSERT %s failed\n", __FUNCTION__, #x);     \
    ASSERTBREAK;                                            \
}

#else

#define ASSERT(cond)

#endif

enum {
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
  LOG_FATAL
};

void spice_log(unsigned int type, const char *function, const char *format, ...);
void spice_log_cleanup(void);

#ifdef __GNUC__
#define SPICE_FUNC_NAME __PRETTY_FUNCTION__
#else
#define SPICE_FUNC_NAME __FUNCTION__
#endif

#define LOG(type, format, ...) spice_log(type, SPICE_FUNC_NAME, format, ## __VA_ARGS__)

#define LOG_INFO(format, ...) LOG(LOG_INFO, format, ## __VA_ARGS__)
#define LOG_WARN(format, ...) LOG(LOG_WARN, format, ## __VA_ARGS__)
#define LOG_ERROR(format, ...) LOG(LOG_ERROR, format, ## __VA_ARGS__)

#define PANIC(format, ...) {                \
    LOG(LOG_FATAL, format, ## __VA_ARGS__);     \
    ON_PANIC();                             \
}

#define PANIC_ON(x) if ((x)) {                      \
    LOG(LOG_FATAL, "%s panic %s\n", __FUNCTION__, #x);  \
    ON_PANIC();                                     \
}

#define DBGLEVEL 1000

#define DBG(level, format, ...) {               \
    if (level <= DBGLEVEL) {                    \
        LOG(LOG_DEBUG, format, ## __VA_ARGS__); \
    }                                           \
}

#endif // _H_DEBUG
