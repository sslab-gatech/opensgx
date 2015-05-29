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

#include "common/rect.h"

#include "common.h"
#include "cursor_channel.h"
#include "display_channel.h"
#include "cursor.h"
#include "red_client.h"
#include "application.h"
#include "debug.h"
#include "utils.h"
#include "screen.h"
#include "red_pixmap_sw.h"

static inline uint8_t revers_bits(uint8_t byte)
{
    uint8_t ret = 0;
    int i;

    for (i = 0; i < 4; i++) {
        int shift = 7 - i * 2;
        ret |= (byte & (1 << i)) << shift;
        ret |= (byte & (0x80 >> i)) >> shift;
    }
    return ret;
}

class NaitivCursor: public CursorOpaque {
public:
    virtual void draw(RedDrawable& dest, int x, int y, const SpiceRect& area) = 0;
};

class AlphaCursor: public NaitivCursor {
public:
    AlphaCursor(const SpiceCursorHeader& header, const uint8_t* data);

    virtual void draw(RedDrawable& dest, int x, int y, const SpiceRect& area);

private:
    std::auto_ptr<RedPixmap> _pixmap;
};

class MonoCursor: public NaitivCursor {
public:
    MonoCursor(const SpiceCursorHeader& header, const uint8_t* data);

    virtual void draw(RedDrawable& dest, int x, int y, const SpiceRect& area);

private:
    std::auto_ptr<RedPixmap> _pixmap;
    int _height;
};

class UnsupportedCursor: public NaitivCursor {
public:
    UnsupportedCursor(const SpiceCursorHeader& header);
    virtual void draw(RedDrawable& dest, int x, int y, const SpiceRect& area);

private:
    int _hot_x;
    int _hot_y;
};

UnsupportedCursor::UnsupportedCursor(const SpiceCursorHeader& header)
    : _hot_x (header.hot_spot_x)
    , _hot_y (header.hot_spot_y)
{
    LOG_WARN("Unsupported cursor %hu", header.type);
}

void UnsupportedCursor::draw(RedDrawable& dest, int x, int y, const SpiceRect& area)
{
    SpiceRect dest_area;
    SpiceRect rect;

    dest_area.left = area.left;
    dest_area.right = area.right;
    dest_area.top = area.top;
    dest_area.bottom = area.bottom;

    rect.left = x + _hot_x - 2;
    rect.right = rect.left + 8;
    rect.top = y + _hot_y - 2;
    rect.bottom = rect.top + 8;
    rect_sect(rect, dest_area);

    dest.fill_rect(rect, rgb32_make(0xf8, 0xf1, 0xb8));

    rect.left = x + _hot_x - 1;
    rect.right = rect.left + 6;
    rect.top = y + _hot_y - 1;
    rect.bottom = rect.top + 6;
    rect_sect(rect, dest_area);

    dest.frame_rect(rect, rgb32_make(0, 0, 0));
}

AlphaCursor::AlphaCursor(const SpiceCursorHeader& header, const uint8_t* data)
    : _pixmap (new RedPixmapSw(header.width, header.height,
                               RedDrawable::ARGB32, true, NULL))
{
    int stride = _pixmap->get_stride();
    uint8_t* dest = _pixmap->get_data();
    int line_size = header.width * sizeof(uint32_t);
    for (int i = 0; i < header.height; i++, data += line_size, dest += stride) {
        memcpy(dest, data, line_size);
    }
}

void AlphaCursor::draw(RedDrawable& dest, int x, int y, const SpiceRect& area)
{
    dest.blend_pixels(*_pixmap, area.left - x, area.top - y, area);
}

MonoCursor::MonoCursor(const SpiceCursorHeader& header, const uint8_t* data)
    : _pixmap (NULL)
    , _height (header.height)
{
    _pixmap.reset(new RedPixmapSw(header.width, _height * 2, RedDrawable::A1,
                                  true, NULL));

    int dest_stride = _pixmap->get_stride();
    uint8_t *dest_line = _pixmap->get_data();
    int src_stride = SPICE_ALIGN(header.width, 8) >> 3;
    const uint8_t* src_line = data;
    const uint8_t* end_line = src_line + _pixmap->get_height() * src_stride;

    if (_pixmap->is_big_endian_bits()) {
        for (; src_line < end_line; src_line += src_stride, dest_line += dest_stride) {
            memcpy(dest_line, src_line, src_stride);
        }
    } else {
        for (; src_line < end_line; src_line += src_stride, dest_line += dest_stride) {
            for (int i = 0; i < src_stride; i++) {
                dest_line[i] = revers_bits(src_line[i]);
            }
        }
    }
}

void MonoCursor::draw(RedDrawable& dest, int x, int y, const SpiceRect& area)
{
    dest.combine_pixels(*_pixmap, area.left - x, area.top - y, area, RedDrawable::OP_AND);
    dest.combine_pixels(*_pixmap, area.left - x, area.top - y + _height, area, RedDrawable::OP_XOR);
}

class ColorCursor: public NaitivCursor {
public:
    ColorCursor(const SpiceCursorHeader& header);

    virtual void draw(RedDrawable& dest, int x, int y, const SpiceRect& area);

protected:
    void init_pixels(const SpiceCursorHeader& header, const uint8_t* _pixels, const uint8_t *and_mask);
    virtual uint32_t get_pixel_color(const uint8_t *data, int row, int col) = 0;

private:
    std::auto_ptr<RedPixmap> _pixmap;
    std::auto_ptr<RedPixmap> _invers;
};

ColorCursor::ColorCursor(const SpiceCursorHeader& header)
    : _pixmap (new RedPixmapSw(header.width, header.height,
                               RedDrawable::ARGB32, true, NULL))
    , _invers (NULL)
{
    _invers.reset(new RedPixmapSw(header.width, header.height, RedDrawable::A1,
                                  true, NULL));
}

void ColorCursor::init_pixels(const SpiceCursorHeader& header, const uint8_t* pixels,
                              const uint8_t *and_mask)
{
    int mask_stride = SPICE_ALIGN(header.width, 8) / 8;
    int invers_stride = _invers->get_stride();
    int pixmap_stride = _pixmap->get_stride();
    uint8_t *_pixmap_line = _pixmap->get_data();
    uint8_t* invers_line = _invers->get_data();
    bool be_bits = _invers->is_big_endian_bits();
    memset(invers_line, 0, header.height * invers_stride);
    for (int i = 0; i < header.height; i++, and_mask += mask_stride, invers_line += invers_stride,
                                            _pixmap_line += pixmap_stride) {
        uint32_t *line_32 = (uint32_t *)_pixmap_line;
        for (int j = 0; j < header.width; j++) {
            uint32_t pixel_val = get_pixel_color(pixels, i, j);
            int and_val = test_bit_be(and_mask, j);
            if ((pixel_val & 0x00ffffff) == 0 && and_val) {
                line_32[j] = 0;
            } else if ((pixel_val & 0x00ffffff) == 0x00ffffff && and_val) {
                line_32[j] = 0;
                if (be_bits) {
                    set_bit_be(invers_line, j);
                } else {
                    set_bit(invers_line, j);
                }
            } else {
                line_32[j] = pixel_val | 0xff000000;
            }
        }
    }
}

void ColorCursor::draw(RedDrawable& dest, int x, int y, const SpiceRect& area)
{
    dest.blend_pixels(*_pixmap, area.left - x, area.top - y, area);
    dest.combine_pixels(*_invers, area.left - x, area.top - y, area, RedDrawable::OP_XOR);
}

class ColorCursor32: public ColorCursor {
public:
    ColorCursor32(const SpiceCursorHeader& header, const uint8_t* data)
        : ColorCursor(header)
        , _src_stride (header.width * sizeof(uint32_t))
    {
        init_pixels(header, data, data + _src_stride * header.height);
    }

private:
    uint32_t get_pixel_color(const uint8_t *data, int row, int col)
    {
        return *((uint32_t *)(data + row * _src_stride) + col);
    }

private:
    int _src_stride;
};

class ColorCursor16: public ColorCursor {
public:
    ColorCursor16(const SpiceCursorHeader& header, const uint8_t* data)
        : ColorCursor(header)
        , _src_stride (header.width * sizeof(uint16_t))
    {
        init_pixels(header, data, data + _src_stride * header.height);
    }

private:
    uint32_t get_pixel_color(const uint8_t *data, int row, int col)
    {
        uint32_t pix = *((uint16_t*)(data + row * _src_stride) + col);
        return ((pix & 0x1f) << 3) | ((pix & 0x3e0) << 6) | ((pix & 0x7c00) << 9);
    }

private:
    int _src_stride;
};

class ColorCursor4: public ColorCursor {
public:
    ColorCursor4(const SpiceCursorHeader& header, const uint8_t* data)
        : ColorCursor(header)
        , _src_stride (SPICE_ALIGN(header.width, 2) >> 1)
        , _palette ((uint32_t*)(data + _src_stride * header.height))
    {
        init_pixels(header, data, (uint8_t*)(_palette + 16));
    }

private:
    uint32_t get_pixel_color(const uint8_t *data, int row, int col)
    {
        data += _src_stride * row + (col >> 1);
        return (col & 1) ? _palette[*data & 0x0f] : _palette[*data >> 4];
    }

private:
    int _src_stride;
    uint32_t* _palette;
};

class AttachDispayEvent: public Event {
public:
    AttachDispayEvent(CursorChannel& channel)
        : _channel (channel)
    {
    }

    class UpdateDisplayChannel: public ForEachChannelFunc {
    public:
        UpdateDisplayChannel(CursorChannel& channel)
            : _channel (channel)
        {
        }

        virtual bool operator() (RedChannel& channel)
        {
            if (channel.get_type() != SPICE_CHANNEL_DISPLAY ||
                                                      channel.get_id() != _channel.get_id()) {
                return true;
            }

            _channel.attach_display(&static_cast<DisplayChannel&>(channel));
            return false;
        }

    private:
        CursorChannel& _channel;
    };

    virtual void response(AbstractProcessLoop& events_loop)
    {
        UpdateDisplayChannel func(_channel);
        _channel.get_client().for_each_channel(func);
    }

private:
    CursorChannel& _channel;
};

class CursorUpdateEvent: public Event {
public:
    CursorUpdateEvent(CursorChannel& channel)
        : _channel (channel)
    {
    }

    virtual void response(AbstractProcessLoop& events_loop)
    {
        DisplayChannel* display_channel = _channel._display_channel;
        if (!display_channel) {
            return;
        }

        Lock lock(_channel._update_lock);
        if (_channel._cursor_visible) {
            display_channel->set_cursor(_channel._cursor);
            return;
        }

        display_channel->hide_cursor();
    }

private:
    CursorChannel& _channel;
};

class CursorHandler: public MessageHandlerImp<CursorChannel, SPICE_CHANNEL_CURSOR> {
public:
    CursorHandler(CursorChannel& channel)
      : MessageHandlerImp<CursorChannel, SPICE_CHANNEL_CURSOR>(channel) {}
};

CursorChannel::CursorChannel(RedClient& client, uint32_t id)
    : RedChannel(client, SPICE_CHANNEL_CURSOR, id, new CursorHandler(*this))
    , ScreenLayer(SCREEN_LAYER_CURSOR, false)
    , _cursor (NULL)
    , _cursor_visible (false)
    , _display_channel (NULL)
{
    CursorHandler* handler = static_cast<CursorHandler*>(get_message_handler());

    handler->set_handler(SPICE_MSG_MIGRATE, &CursorChannel::handle_migrate);
    handler->set_handler(SPICE_MSG_SET_ACK, &CursorChannel::handle_set_ack);
    handler->set_handler(SPICE_MSG_PING, &CursorChannel::handle_ping);
    handler->set_handler(SPICE_MSG_WAIT_FOR_CHANNELS, &CursorChannel::handle_wait_for_channels);
    handler->set_handler(SPICE_MSG_DISCONNECTING, &CursorChannel::handle_disconnect);
    handler->set_handler(SPICE_MSG_NOTIFY, &CursorChannel::handle_notify);

    handler->set_handler(SPICE_MSG_CURSOR_INIT, &CursorChannel::handle_init);
    handler->set_handler(SPICE_MSG_CURSOR_RESET, &CursorChannel::handle_reset);
    handler->set_handler(SPICE_MSG_CURSOR_SET, &CursorChannel::handle_cursor_set);
    handler->set_handler(SPICE_MSG_CURSOR_MOVE, &CursorChannel::handle_cursor_move);
    handler->set_handler(SPICE_MSG_CURSOR_HIDE, &CursorChannel::handle_cursor_hide);
    handler->set_handler(SPICE_MSG_CURSOR_TRAIL, &CursorChannel::handle_cursor_trail);
    handler->set_handler(SPICE_MSG_CURSOR_INVAL_ONE, &CursorChannel::handle_inval_one);
    handler->set_handler(SPICE_MSG_CURSOR_INVAL_ALL, &CursorChannel::handle_inval_all);
}

CursorChannel::~CursorChannel()
{
    ASSERT(!_cursor);
}

void CursorChannel::on_connect()
{
    AutoRef<AttachDispayEvent> attach_event(new AttachDispayEvent(*this));
    get_client().push_event(*attach_event);
}

void CursorChannel::on_disconnect()
{
    remove_cursor();
    _cursor_cache.clear();
    AutoRef<SyncEvent> sync_event(new SyncEvent());
    get_client().push_event(*sync_event);
    (*sync_event)->wait();
    detach_from_screen(get_client().get_application());
}

void CursorChannel::update_display_cursor()
{
    if (!_display_channel) {
        return;
    }

    AutoRef<CursorUpdateEvent> update_event(new CursorUpdateEvent(*this));
    get_client().push_event(*update_event);
}

void CursorChannel::remove_cursor()
{
    Lock lock(_update_lock);
    _cursor_visible = false;
    if (_cursor) {
        _cursor->unref();
        _cursor = NULL;
    }
    lock.unlock();
    clear_area();
    update_display_cursor();
}

void CursorChannel::copy_pixels(const QRegion& dest_region, RedDrawable& dest_dc)
{
    pixman_box32_t *rects;
    int num_rects;

    Lock lock(_update_lock);

    if (!_cursor_visible) {
        return;
    }

    rects = pixman_region32_rectangles((pixman_region32_t *)&dest_region, &num_rects);
    for (int i = 0; i < num_rects; i++) {
        SpiceRect r;

        r.left = rects[i].x1;
        r.top = rects[i].y1;
        r.right = rects[i].x2;
        r.bottom = rects[i].y2;
        ASSERT(_cursor && _cursor->get_opaque());
        ((NaitivCursor*)_cursor->get_opaque())->draw(dest_dc, _cursor_rect.left, _cursor_rect.top,
                                                     r);
    }
}

void CursorChannel::create_native_cursor(CursorData* cursor)
{
    CursorOpaque* native_cursor = cursor->get_opaque();

    if (native_cursor) {
        return;
    }

    switch (cursor->header().type) {
    case SPICE_CURSOR_TYPE_ALPHA:
        native_cursor = new AlphaCursor(cursor->header(), cursor->data());
        break;
    case SPICE_CURSOR_TYPE_COLOR32:
        native_cursor = new ColorCursor32(cursor->header(), cursor->data());
        break;
    case SPICE_CURSOR_TYPE_MONO:
        native_cursor = new MonoCursor(cursor->header(), cursor->data());
        break;
    case SPICE_CURSOR_TYPE_COLOR4:
        native_cursor = new ColorCursor4(cursor->header(), cursor->data());
        break;
    case SPICE_CURSOR_TYPE_COLOR8:
        native_cursor = new UnsupportedCursor(cursor->header());
        break;
    case SPICE_CURSOR_TYPE_COLOR16:
        native_cursor = new ColorCursor16(cursor->header(), cursor->data());
        break;
    case SPICE_CURSOR_TYPE_COLOR24:
        native_cursor = new UnsupportedCursor(cursor->header());
        break;
    default:
        THROW("invalid cursor type");
    }
    cursor->set_opaque(native_cursor);
}

void CursorChannel::set_cursor(SpiceCursor& red_cursor, int x, int y, bool visible)
{
    CursorData *cursor;

    if (red_cursor.flags & SPICE_CURSOR_FLAGS_NONE) {
        remove_cursor();
        return;
    }

    if (red_cursor.flags & SPICE_CURSOR_FLAGS_FROM_CACHE) {
        cursor = _cursor_cache.get(red_cursor.header.unique);
    } else {
        cursor = new CursorData(red_cursor, red_cursor.data_size);
        if (red_cursor.flags & SPICE_CURSOR_FLAGS_CACHE_ME) {
            ASSERT(red_cursor.header.unique);
            _cursor_cache.add(red_cursor.header.unique, cursor);
        }
    }

    AutoRef<CursorData> cursor_ref(cursor);
    create_native_cursor(cursor);

    Lock lock(_update_lock);
    _hot_pos.x = x;
    _hot_pos.y = y;
    _cursor_visible = visible;
    _cursor_rect.left = x - cursor->header().hot_spot_x;
    _cursor_rect.right = _cursor_rect.left + cursor->header().width;
    _cursor_rect.top = y - cursor->header().hot_spot_y;
    _cursor_rect.bottom = _cursor_rect.top + cursor->header().height;

    if (_cursor) {
        _cursor->unref();
    }
    _cursor = cursor->ref();
    lock.unlock();

    update_display_cursor();

    if (get_client().get_mouse_mode() == SPICE_MOUSE_MODE_SERVER) {
        if (_cursor_visible) {
            set_rect_area(_cursor_rect);
        } else {
            clear_area();
        }
    }
}

void CursorChannel::attach_display(DisplayChannel* channel)
{
    if (_display_channel) {
        return;
    }

    _display_channel = channel;

    Lock lock(_update_lock);
    if (!_cursor_visible) {
        return;
    }

    _display_channel->set_cursor(_cursor);
}

void CursorChannel::detach_display()
{
    _display_channel = NULL;
}

void CursorChannel::handle_init(RedPeer::InMessage *message)
{
    SpiceMsgCursorInit *init = (SpiceMsgCursorInit*)message->data();
    attach_to_screen(get_client().get_application(), get_id());
    remove_cursor();
    _cursor_cache.clear();
    set_cursor(init->cursor, init->position.x,
               init->position.y, init->visible != 0);
}

void CursorChannel::handle_reset(RedPeer::InMessage *message)
{
    remove_cursor();
    detach_from_screen(get_client().get_application());
    _cursor_cache.clear();
}

void CursorChannel::handle_cursor_set(RedPeer::InMessage* message)
{
    SpiceMsgCursorSet* set = (SpiceMsgCursorSet*)message->data();
    set_cursor(set->cursor, set->position.x,
               set->position.y, set->visible != 0);
}

void CursorChannel::handle_cursor_move(RedPeer::InMessage* message)
{
    SpiceMsgCursorMove* move = (SpiceMsgCursorMove*)message->data();

    if (!_cursor) {
        return;
    }

    Lock lock(_update_lock);
    _cursor_visible = true;
    int dx = move->position.x - _hot_pos.x;
    int dy = move->position.y - _hot_pos.y;
    _hot_pos.x += dx;
    _hot_pos.y += dy;
    _cursor_rect.left += dx;
    _cursor_rect.right += dx;
    _cursor_rect.top += dy;
    _cursor_rect.bottom += dy;
    lock.unlock();

    if (get_client().get_mouse_mode() == SPICE_MOUSE_MODE_SERVER) {
        set_rect_area(_cursor_rect);
        return;
    }

    update_display_cursor();
}

void CursorChannel::handle_cursor_hide(RedPeer::InMessage* message)
{
    Lock lock(_update_lock);

    _cursor_visible = false;
    update_display_cursor();

    if (get_client().get_mouse_mode() == SPICE_MOUSE_MODE_SERVER) {
        clear_area();
    }
}

void CursorChannel::handle_cursor_trail(RedPeer::InMessage* message)
{
    SpiceMsgCursorTrail* trail = (SpiceMsgCursorTrail*)message->data();
    DBG(0, "length %u frequency %u", trail->length, trail->frequency)
}

void CursorChannel::handle_inval_one(RedPeer::InMessage* message)
{
    SpiceMsgDisplayInvalOne* inval = (SpiceMsgDisplayInvalOne*)message->data();
    _cursor_cache.remove(inval->id);
}

void CursorChannel::handle_inval_all(RedPeer::InMessage* message)
{
    _cursor_cache.clear();
}

void CursorChannel::on_mouse_mode_change()
{
    Lock lock(_update_lock);

    if (get_client().get_mouse_mode() == SPICE_MOUSE_MODE_CLIENT) {
        clear_area();
        return;
    }

    if (_cursor_visible) {
        set_rect_area(_cursor_rect);
    }
}

class CursorFactory: public ChannelFactory {
public:
    CursorFactory() : ChannelFactory(SPICE_CHANNEL_CURSOR) {}
    virtual RedChannel* construct(RedClient& client, uint32_t id)
    {
        return new CursorChannel(client, id);
    }
};

static CursorFactory factory;

ChannelFactory& CursorChannel::Factory()
{
    return factory;
}
