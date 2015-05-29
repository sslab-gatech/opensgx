/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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

#ifndef _H_DISPLAY_CHANNEL
#define _H_DISPLAY_CHANNEL

#include "common/region.h"

#include "common.h"
#include "canvas.h"
#include "red_channel.h"
#include "cache.hpp"
#include "screen_layer.h"
#include "process_loop.h"
#ifdef USE_OPENGL
#include "red_pixmap_gl.h"
#endif
#include "glz_decoder_window.h"

class RedScreen;
class ChannelFactory;
class VideoStream;
class DisplayChannel;
class CursorData;
class InputsChannel;

class StreamsTrigger: public EventSources::Trigger {
public:
    StreamsTrigger(DisplayChannel& channel);

    virtual void on_event();

private:
    DisplayChannel& _channel;
};

#ifdef USE_OPENGL
class GLInterruptRecreate: public EventSources::Trigger {
public:
    GLInterruptRecreate(DisplayChannel& channel);
    virtual void trigger();
    virtual void on_event();

private:
    DisplayChannel& _channel;
    Mutex _lock;
    Condition _cond;
};
#endif

class InterruptUpdate: public EventSources::Trigger {
public:
    InterruptUpdate(DisplayChannel& channel);

    virtual void on_event();

private:
    DisplayChannel& _channel;
};

class StreamsTimer: public Timer {
public:
    StreamsTimer(DisplayChannel& channel);
    virtual void response(AbstractProcessLoop& events_loop);
private:
    DisplayChannel& _channel;
};

class DisplayChannel: public RedChannel, public ScreenLayer {
public:
    DisplayChannel(RedClient& client, uint32_t id,
                   PixmapCache& pixmap_cache, GlzDecoderWindow& glz_window);
    virtual ~DisplayChannel();

    virtual void copy_pixels(const QRegion& dest_region, RedDrawable& dest_dc);
    virtual void copy_pixels(const QRegion& dest_region, const PixmapHeader &dest);
#ifdef USE_OPENGL
    virtual void recreate_ogl_context();
    virtual void recreate_ogl_context_interrupt();
    virtual void pre_migrate();
    virtual void post_migrate();
#endif
    virtual void update_interrupt();
    void set_cursor(CursorData* cursor);
    void hide_cursor();
    void set_capture_mode(bool on);

    virtual bool pointer_test(int x, int y);
    virtual void on_pointer_enter(int x, int y, unsigned int buttons_state);
    virtual void on_pointer_motion(int x, int y, unsigned int buttons_state);
    virtual void on_pointer_leave();
    virtual void on_mouse_button_press(int button, int buttons_state);
    virtual void on_mouse_button_release(int button, int buttons_state);

    void attach_inputs(InputsChannel* inputs_channel);
    void detach_inputs();

    static ChannelFactory& Factory();

protected:
    virtual void on_connect();
    virtual void on_disconnect();
    virtual void on_disconnect_mig_src();

private:
    void set_draw_handlers();
    void clear_draw_handlers();
    bool create_sw_canvas(int surface_id, int width, int height, uint32_t format);
#ifdef USE_OPENGL
    bool create_ogl_canvas(int surface_id, int width, int height, uint32_t format, bool recreate,
                           RenderType rendertype);
#endif
#ifdef WIN32
    bool create_gdi_canvas(int surface_id, int width, int height, uint32_t format);
#endif
    void destroy_canvas(int surface_id);
    void create_canvas(int surface_id, const std::vector<int>& canvas_type, int width, int height,
                       uint32_t format);
    void destroy_streams();
    void update_cursor();

    void create_primary_surface(int width, int height, uint32_t format);
    void create_surface(int surface_id, int width, int height, uint32_t format);
    void destroy_primary_surface();
    void destroy_surface(int surface_id);
    void destroy_all_surfaces();
    void do_destroy_all_surfaces();
    void destroy_off_screen_surfaces();
    void do_destroy_off_screen_surfaces();

    void handle_mode(RedPeer::InMessage* message);
    void handle_mark(RedPeer::InMessage* message);
    void handle_reset(RedPeer::InMessage* message);

    void handle_inval_list(RedPeer::InMessage* message);
    void handle_inval_all_pixmaps(RedPeer::InMessage* message);
    void handle_inval_palette(RedPeer::InMessage* message);
    void handle_inval_all_palettes(RedPeer::InMessage* message);
    void handle_copy_bits(RedPeer::InMessage* message);
    void handle_stream_create(RedPeer::InMessage* message);
    void handle_stream_data(RedPeer::InMessage* message);
    void handle_stream_clip(RedPeer::InMessage* message);
    void handle_stream_destroy(RedPeer::InMessage* message);
    void handle_stream_destroy_all(RedPeer::InMessage* message);

    void handle_surface_create(RedPeer::InMessage* message);
    void handle_surface_destroy(RedPeer::InMessage* message);

    void handle_draw_fill(RedPeer::InMessage* message);
    void handle_draw_opaque(RedPeer::InMessage* message);
    void handle_draw_copy(RedPeer::InMessage* message);
    void handle_draw_blend(RedPeer::InMessage* message);
    void handle_draw_blackness(RedPeer::InMessage* message);
    void handle_draw_whiteness(RedPeer::InMessage* message);
    void handle_draw_invers(RedPeer::InMessage* message);
    void handle_draw_rop3(RedPeer::InMessage* message);
    void handle_draw_stroke(RedPeer::InMessage* message);
    void handle_draw_text(RedPeer::InMessage* message);
    void handle_draw_transparent(RedPeer::InMessage* message);
    void handle_draw_alpha_blend(RedPeer::InMessage* message);
    void handle_draw_composite(RedPeer::InMessage* message);

    void on_streams_trigger();
    virtual void on_update_completion(uint64_t mark);
    void streams_time();
    void activate_streams_timer();
    void stream_update_request(uint32_t update_time);
    void reset_screen();
    void clear(bool destroy_primary = true);

    static void set_clip_rects(const SpiceClip& clip, uint32_t& num_clip_rects, SpiceRect*& clip_rects);

private:
    SurfacesCache _surfaces_cache;
    PixmapCache& _pixmap_cache;
    PaletteCache _palette_cache;
    GlzDecoderWindow& _glz_window;
    bool _mark;
    int _x_res;
    int _y_res;
    uint32_t _format;
#ifdef USE_OPENGL
    RenderType _rendertype;
#endif

#ifndef RED64
    Mutex _mark_lock;
#endif
    uint64_t _update_mark;
    Mutex _streams_lock;

    Mutex _timer_lock;
    AutoRef<StreamsTimer> _streams_timer;
    uint32_t _next_timer_time;

    AutoRef<CursorData> _cursor;
    bool _cursor_visibal;
    bool _active_pointer;
    bool _capture_mouse_mode;
    InputsChannel* _inputs_channel;

    SpicePoint _pointer_pos;
    int _buttons_state;

    std::vector<VideoStream*> _streams;
    VideoStream* _active_streams;
    StreamsTrigger _streams_trigger;
#ifdef USE_OPENGL
    GLInterruptRecreate _gl_interrupt_recreate;
#endif
    InterruptUpdate _interrupt_update;

    bool _mig_wait_primary;
    friend class SetModeEvent;
    friend class CreatePrimarySurfaceEvent;
    friend class DestroyPrimarySurfaceEvent;
    friend class CreateSurfaceEvent;
    friend class DestroySurfaceEvent;
    friend class DestroyAllSurfacesEvent;
    friend class ActivateTimerEvent;
    friend class VideoStream;
    friend class StreamsTrigger;
    friend class GLInterupt;
    friend class StreamsTimer;
    friend class AttachChannelsEvent;
    friend class DetachChannelsEvent;
    friend class MigPrimarySurfaceTimer;
};

#endif
