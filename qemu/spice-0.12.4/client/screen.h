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

#ifndef _H_SCREEN
#define _H_SCREEN

#include "common/region.h"

#include "common.h"
#include "red_key.h"
#ifdef USE_OPENGL
#include "GL/gl.h"
#endif // USE_OPENGL

#include "red_window.h"
#include "platform.h"
#include "process_loop.h"
#include "threads.h"
#include "utils.h"

class Application;
class ScreenLayer;
class Monitor;
class RedScreen;

enum {
    SCREEN_LAYER_DISPLAY,
    SCREEN_LAYER_CURSOR,
    SCREEN_LAYER_GUI_BARIER,
    SCREEN_LAYER_GUI,
    SCREEN_LAYER_INFO,
};

class UpdateTimer: public Timer {
public:
    UpdateTimer(RedScreen* screen) : _screen (screen) {}
    virtual void response(AbstractProcessLoop& events_loop);
private:
    RedScreen* _screen;
};

class RedScreen: public RedWindow::Listener {
public:
    RedScreen(Application& owner, int id, const std::string& name, int width, int height);

    RedScreen* ref();
    void unref();

    void attach_layer(ScreenLayer& layer);
    void detach_layer(ScreenLayer& layer);
    void on_layer_changed(ScreenLayer& layer);
    /* When resizing on full screen mode, the monitor must be configured
     * correctly before calling resize*/
    void resize(int width, int height);
    void set_name(const std::string& name);
    uint64_t invalidate(const SpiceRect& rect, bool urgent);
    void invalidate(const QRegion &region);
    void capture_mouse();
    void relase_mouse();
    bool is_mouse_captured() { return _mouse_captured;}
    bool intercepts_sys_key() { return _key_interception;}
    SpicePoint get_size() { return _size;}
    bool has_monitor() { return _monitor != 0;}
    void lock_size();
    void unlock_size();
    bool is_size_locked() { return _size_locked;}
    void set_monitor(Monitor *monitor) { _monitor = monitor;}
    Monitor* get_monitor() { return _monitor;}
    RedWindow* get_window() { return &_window;}
    bool is_out_of_sync() { return _out_of_sync;}
    void set_cursor(LocalCursor* cursor);
    void hide_cursor();
    void exit_full_screen();
    void minimize();
    void show(bool activate, RedScreen* pos);
    void show_full_screen();
    void position_full_screen(const SpicePoint& position);
    void hide();
    void show();
    void activate();
    void external_show();
    void update_menu();

    int get_id() { return _id;}
    int get_screen_id();

#ifdef USE_OPENGL
    void untouch_context();
    bool need_recreate_context_gl();
    void set_type_gl();
    void unset_type_gl();
#endif
    void set_update_interrupt_trigger(EventSources::Trigger *trigger);
    bool update_by_interrupt();
    void interrupt_update();

    void update();

private:
    friend class UpdateEvent;
    friend class UpdateTimer;

    virtual ~RedScreen();
    void create_composit_area();

    void destroy_composit_area();
    void erase_background(RedDrawable& dc, const QRegion& region);
    void notify_new_size();
    void adjust_window_rect(int x, int y);
    void save_position();
    void __show_full_screen();

    bool _invalidate(const SpiceRect& rect, bool urgent, uint64_t& update_mark);
    void begin_update(QRegion& direct_rgn, QRegion& composit_rgn, QRegion& frame_rgn);
    void update_composit(QRegion& composit_rgn);
    void draw_direct(RedDrawable& win_dc, QRegion& direct_rgn, QRegion& composit_rgn,
                     QRegion& frame_rgn);
    void activate_timer();
    void update_done();
    void periodic_update();
    bool is_dirty() {return !region_is_empty(&_dirty_region);}
    void composit_to_screen(RedDrawable& win_dc, const QRegion& region);

    void reset_mouse_pos();
    ScreenLayer* find_pointer_layer();
    bool update_pointer_layer();

    virtual void on_exposed_rect(const SpiceRect& area);
    virtual void on_pointer_enter(int x, int y, unsigned int buttons_state);
    virtual void on_pointer_motion(int x, int y, unsigned int buttons_state);
    virtual void on_pointer_leave();
    void on_mouse_motion(int x, int y, unsigned int buttons_state);
    virtual void on_mouse_button_press(SpiceMouseButton button, unsigned int buttons_state);
    virtual void on_mouse_button_release(SpiceMouseButton button, unsigned int buttons_state);

    virtual void on_key_press(RedKey key);
    virtual void on_key_release(RedKey key);
    virtual void on_char(uint32_t ch);

    virtual void on_deactivate();
    virtual void on_activate();

    virtual void on_start_key_interception();
    virtual void on_stop_key_interception();
    virtual void enter_modal_loop();
    virtual void exit_modal_loop();

    virtual void pre_migrate();
    virtual void post_migrate();

private:
    Application& _owner;
    int _id;
    AtomicCount _refs;
    std::string _name;
    RedWindow _window;
    std::vector<ScreenLayer*> _layers;
    QRegion _dirty_region;
    RecurciveMutex _update_lock;
    bool _active;
    bool _full_screen;
    bool _out_of_sync;
    bool _frame_area;
    bool _periodic_update;
    bool _key_interception;
    bool _update_by_timer;
    bool _size_locked;
    bool _menu_needs_update;
    int _force_update_timer;
    AutoRef<UpdateTimer> _update_timer;
    RedDrawable* _composit_area;
    uint64_t _update_mark;

    SpicePoint _size;
    SpicePoint _origin;
    SpicePoint _mouse_anchor_point;
    SpicePoint _save_pos;
    Monitor* _monitor;

    LocalCursor* _default_cursor;
    LocalCursor* _inactive_cursor;

    int _pixel_format_index;
    EventSources::Trigger *_update_interrupt_trigger;

    ScreenLayer* _pointer_layer;
    bool _mouse_captured;
    Mutex _layer_changed_lock;
    bool _active_layer_change_event;
    bool _pointer_on_screen;
    SpicePoint _pointer_pos;
    unsigned int _mouse_botton_state;

    friend class LayerChangedEvent;
};

#endif
