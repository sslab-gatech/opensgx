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

#ifndef _H_STAT
#define _H_STAT

#include <stdint.h>

typedef uint32_t StatNodeRef;
#define INVALID_STAT_REF (~(StatNodeRef)0)

#ifdef RED_STATISTICS

StatNodeRef stat_add_node(StatNodeRef parent, const char *name, int visible);
void stat_remove_node(StatNodeRef node);
uint64_t *stat_add_counter(StatNodeRef parent, const char *name, int visible);
void stat_remove_counter(uint64_t *counter);

#define stat_inc_counter(counter, value) {  \
    if (counter) {                          \
        *(counter) += (value);              \
    }                                       \
}

#else
#define stat_add_node(p, n, v) INVALID_STAT_REF
#define stat_remove_node(n)
#define stat_add_counter(p, n, v) NULL
#define stat_remove_counter(c)
#define stat_inc_counter(c, v)
#endif

#endif
