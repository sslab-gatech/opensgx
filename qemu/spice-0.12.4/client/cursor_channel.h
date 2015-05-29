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

#ifndef _H_CURSOR_CHANNEL
#define _H_CURSOR_CHANNEL

#include "red_channel.h"
#include "cache.hpp"
#include "cursor.h"
#include "screen_layer.h"

class ChannelFactory;
class CursorChannel;
class DisplayChannel;

class CursorCacheTreat {
public:
    static inline CursorData* get(CursorData* cursor)
    {
        return cursor->ref();
    }

    static inline void release(CursorData* cursor)
    {
        cursor->unref();
    }

    static const char* name() { return "cursor";}
};

typedef Cache<CursorData, CursorCacheTreat, 1024> CursorCache;

class CursorChannel: public RedChannel, public ScreenLayer {
public:
    CursorChannel(RedClient& client, uint32_t id);
    virtual ~CursorChannel();

    static ChannelFactory& Factory();
    void on_mouse_mode_change();

    void attach_display(DisplayChannel* channel);
    void detach_display();

protected:
    virtual void on_connect();
    virtual void on_disconnect();

private:
    static void create_native_cursor(CursorData* cursor);

    void update_display_cursor();
    void set_cursor(SpiceCursor& red_cursor, int x, int y, bool visible);
    void remove_cursor();

    virtual void copy_pixels(const QRegion& dest_region, RedDrawable& dest_dc);

    void handle_init(RedPeer::InMessage* message);
    void handle_reset(RedPeer::InMessage* message);
    void handle_cursor_set(RedPeer::InMessage* message);
    void handle_cursor_move(RedPeer::InMessage* message);
    void handle_cursor_hide(RedPeer::InMessage* message);
    void handle_cursor_trail(RedPeer::InMessage* message);
    void handle_inval_one(RedPeer::InMessage* message);
    void handle_inval_all(RedPeer::InMessage* message);

    friend class AttachDispayEvent;
    friend class CursorUpdateEvent;

private:
    CursorCache _cursor_cache;
    CursorData* _cursor;
    SpicePoint _hot_pos;
    SpiceRect _cursor_rect;
    Mutex _update_lock;
    bool _cursor_visible;
    DisplayChannel* _display_channel;
};

#endif
