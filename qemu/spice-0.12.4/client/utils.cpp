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
#include <stdlib.h>
#include <stdarg.h>

#include "utils.h"

void string_printf(std::string& str, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    string_vprintf(str, format, ap);
    va_end(ap);
}

int str_to_port(const char *str)
{
    long port;
    char *endptr;
    port = strtol(str, &endptr, 0);
    if (endptr != str + strlen(str) || port < 0 || port > 0xffff) {
        return -1;
    }
    return port;
}
