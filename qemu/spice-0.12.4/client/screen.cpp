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
#include "screen.h"
#include "application.h"
#include "screen_layer.h"
#include "utils.h"
#include "debug.h"
#include "monitor.h"
#include "red_pixmap_sw.h"
#include "resource.h"
#include "icon.h"

class UpdateEvent: public Event {
public:
    UpdateEvent(int screen) : _screen (screen) {}

    virtual void response(AbstractProcessLoop& events_loop)
    {
        Application* app = static_cast<Application*>(events_loop.get_owner());
        RedScreen* screen = app->find_screen(_screen);
        if (screen) {
            screen->update();
        }
    }

private:
    int _screen;
};

class LayerChangedEvent: public Event {
public:
    LayerChangedEvent (int screen) : _screen (screen) {}

    virtual void response(AbstractProcessLoop& events_loop)
    {
        Application* app = static_cast<Application*>(events_loop.get_owner());
        RedScreen* screen = app->find_screen(_screen);
        if (screen) {
            Lock lock(screen->_layer_changed_lock);
            screen->_active_layer_change_event = false;
            lock.unlock();
            if (screen->_pointer_on_screen) {
                screen->update_pointer_layer();
            }
        }
    }

private:
    int _screen;
};


void UpdateTimer::response(AbstractProcessLoop& events_loop)
{
    _screen->periodic_update();
}

RedScreen::RedScreen(Application& owner, int id, const std::string& name, int width, int height)
    : _owner (owner)
    , _id (id)
    , _refs (1)
    , _window (*this)
    , _active (false)
    , _full_screen (false)
    , _out_of_sync (false)
    , _frame_area (false)
    , _periodic_update (false)
    , _key_interception (false)
    , _update_by_timer (true)
    , _size_locked (false)
    , _menu_needs_update (false)
    , _force_update_timer (0)
    , _update_timer (new UpdateTimer(this))
    , _composit_area (NULL)
    , _update_mark (1)
    , _monitor (NULL)
    , _default_cursor (NULL)
    , _inactive_cursor (NULL)
    , _pixel_format_index (0)
    , _update_interrupt_trigger (NULL)
    , _pointer_layer (NULL)
    , _mouse_captured (false)
    , _active_layer_change_event (false)
    , _pointer_on_screen (false)
{
    region_init(&_dirty_region);
    set_name(name);
    _size.x = width;
    _size.y = height;
    _origin.x = _origin.y = 0;
    create_composit_area();
    _window.resize(_size.x, _size.y);
    save_position();
    if ((_default_cursor = Platform::create_default_cursor()) == NULL) {
        THROW("create default cursor failed");
    }
    if ((_inactive_cursor = Platform::create_inactive_cursor()) == NULL) {
        THROW("create inactive cursor failed");
    }
    _window.set_cursor(_default_cursor);
    update_menu();
    AutoRef<Icon> icon(Platform::load_icon(RED_ICON_RES_ID));
    _window.set_icon(*icon);
    _window.start_key_interception();
}

RedScreen::~RedScreen()
{
    bool captured = is_mouse_captured();
    _window.stop_key_interception();
    relase_mouse();
    destroy_composit_area();
    _owner.deactivate_interval_timer(*_update_timer);
    _owner.on_screen_destroyed(_id, captured);
    region_destroy(&_dirty_region);
    if (_default_cursor) {
        _default_cursor->unref();
    }

    if (_inactive_cursor) {
        _inactive_cursor->unref();
    }
}

void RedScreen::show(bool activate, RedScreen* pos)
{
    _window.position_after((pos) ? &pos->_window : NULL);
    show();
    if (activate) {
        _window.activate();
    }
}

RedScreen* RedScreen::ref()
{
    ++_refs;
    return this;
}

void RedScreen::unref()
{
    if (!--_refs) {
        delete this;
    }
}

void RedScreen::destroy_composit_area()
{
    if (_composit_area) {
        delete _composit_area;
        _composit_area = NULL;
    }
}

void RedScreen::create_composit_area()
{
    destroy_composit_area();
    _composit_area = new RedPixmapSw(_size.x, _size.y, _window.get_format(),
                                     false, &_window);
}

void RedScreen::adjust_window_rect(int x, int y)
{
    _window.move_and_resize(x, y, _size.x, _size.y);
}

void RedScreen::resize(int width, int height)
{
    RecurciveLock lock(_update_lock);
    _size.x = width;
    _size.y = height;
    create_composit_area();
    if (_full_screen) {
        __show_full_screen();
    } else {
        bool cuptur = is_mouse_captured();
        if (cuptur) {
            relase_mouse();
        }
        _window.resize(_size.x, _size.y);
        if (_active && cuptur) {
            capture_mouse();
        }
    }
    notify_new_size();
}

void RedScreen::lock_size()
{
    ASSERT(!_size_locked);
    _size_locked = true;
}

void RedScreen::unlock_size()
{
    _size_locked = false;
    _owner.on_screen_unlocked(*this);
}

void RedScreen::set_name(const std::string& name)
{
    if (!name.empty()) {
        string_printf(_name, name.c_str(), _id);
    }
    _window.set_title(_name);
}

void RedScreen::on_layer_changed(ScreenLayer& layer)
{
    Lock lock(_layer_changed_lock);
    if (_active_layer_change_event) {
        return;
    }
    _active_layer_change_event = true;
    AutoRef<LayerChangedEvent> change_event(new LayerChangedEvent(_id));
    _owner.push_event(*change_event);
}

void RedScreen::attach_layer(ScreenLayer& layer)
{
    RecurciveLock lock(_update_lock);
    int order = layer.z_order();

    if ((int)_layers.size() < order + 1) {
        _layers.resize(order + 1);
    }
    if (_layers[order]) {
        THROW("layer in use");
    }
    layer.set_screen(this);
    _layers[order] = &layer;
    ref();
    lock.unlock();
    layer.invalidate();
    if (_pointer_on_screen) {
        update_pointer_layer();
    }
}

void RedScreen::detach_layer(ScreenLayer& layer)
{
    bool need_pointer_layer_update = false;
    if (_pointer_layer == &layer) {
        _pointer_layer->on_pointer_leave();
        _pointer_layer = NULL;
        need_pointer_layer_update = true;
    }

    RecurciveLock lock(_update_lock);

    int order = layer.z_order();

    if ((int)_layers.size() < order + 1 || _layers[order] != &layer) {
        THROW("not found");
    }
    QRegion layer_area;
    region_clone(&layer_area, &layer.area());
    _layers[order]->set_screen(NULL);
    _layers[order] = NULL;
    lock.unlock();
    invalidate(layer_area);
    region_destroy(&layer_area);
    unref();
    if (need_pointer_layer_update && !update_pointer_layer()) {
        _window.set_cursor(_inactive_cursor);
    }
}

void RedScreen::composit_to_screen(RedDrawable& win_dc, const QRegion& region)
{
    pixman_box32_t *rects;
    int num_rects;

    rects = pixman_region32_rectangles((pixman_region32_t *)&region, &num_rects);
    for (int i = 0; i < num_rects; i++) {
        SpiceRect r;

        r.left = rects[i].x1;
        r.top = rects[i].y1;
        r.right = rects[i].x2;
        r.bottom = rects[i].y2;
        win_dc.copy_pixels(*_composit_area, r.left, r.top, r);
    }
}

void RedScreen::notify_new_size()
{
    for (int i = 0; i < (int)_layers.size(); i++) {
        if (!_layers[i]) {
            continue;
        }
        _layers[i]->on_size_changed();
    }
}

inline void RedScreen::begin_update(QRegion& direct_rgn, QRegion& composit_rgn,
                                    QRegion& frame_rgn)
{
    region_init(&composit_rgn);
    RecurciveLock lock(_update_lock);
    region_clone(&direct_rgn, &_dirty_region);
    region_clear(&_dirty_region);
    _update_mark++;
    lock.unlock();

    QRegion rect_rgn;
    SpiceRect r;
    r.top = r.left = 0;
    r.right = _size.x;
    r.bottom = _size.y;
    region_init(&rect_rgn);
    region_add(&rect_rgn, &r);

    if (_frame_area) {
        region_clone(&frame_rgn, &direct_rgn);
        region_exclude(&frame_rgn, &rect_rgn);
    }
    region_and(&direct_rgn, &rect_rgn);
    region_destroy(&rect_rgn);

    for (int i = _layers.size() - 1; i >= 0; i--) {
        ScreenLayer* layer;

        if (!(layer = _layers[i])) {
            continue;
        }
        layer->begin_update(direct_rgn, composit_rgn);
    }
}

inline void RedScreen::update_done()
{
    for (unsigned int i = 0; i < _layers.size(); i++) {
        ScreenLayer* layer;

        if (!(layer = _layers[i])) {
            continue;
        }
        layer->on_update_completion(_update_mark - 1);
    }
}

inline void RedScreen::update_composit(QRegion& composit_rgn)
{
    erase_background(*_composit_area, composit_rgn);
    for (int i = 0; i < (int)_layers.size(); i++) {
        ScreenLayer* layer;

        if (!(layer = _layers[i])) {
            continue;
        }
        QRegion& dest_region = layer->composit_area();
        region_or(&composit_rgn, &dest_region);
        layer->copy_pixels(dest_region, *_composit_area);
    }
}

inline void RedScreen::draw_direct(RedDrawable& win_dc, QRegion& direct_rgn, QRegion& composit_rgn,
                                   QRegion& frame_rgn)
{
    erase_background(win_dc, direct_rgn);

    if (_frame_area) {
        erase_background(win_dc, frame_rgn);
        region_destroy(&frame_rgn);
    }

    for (int i = 0; i < (int)_layers.size(); i++) {
        ScreenLayer* layer;

        if (!(layer = _layers[i])) {
            continue;
        }
        QRegion& dest_region = layer->direct_area();
        layer->copy_pixels(dest_region, win_dc);
    }
}

void RedScreen::periodic_update()
{
    bool need_update;
    RecurciveLock lock(_update_lock);
    if (is_dirty()) {
        need_update = true;
    } else {
        if (!_force_update_timer) {
            _owner.deactivate_interval_timer(*_update_timer);
            _periodic_update = false;
        }
        need_update = false;
    }
    lock.unlock();

    if (need_update) {
        if (update_by_interrupt()) {
            interrupt_update();
        } else {
            update();
        }
    }
}

void RedScreen::activate_timer()
{
    RecurciveLock lock(_update_lock);
    if (_periodic_update) {
        return;
    }
    _periodic_update = true;
    lock.unlock();
    _owner.activate_interval_timer(*_update_timer, 1000 / 30);
}

void RedScreen::update()
{
    if (is_out_of_sync()) {
        return;
    }

    QRegion direct_rgn;
    QRegion composit_rgn;
    QRegion frame_rgn;

    begin_update(direct_rgn, composit_rgn, frame_rgn);
    update_composit(composit_rgn);
    draw_direct(_window, direct_rgn, composit_rgn, frame_rgn);
    composit_to_screen(_window, composit_rgn);
    update_done();
    region_destroy(&direct_rgn);
    region_destroy(&composit_rgn);

    if (_update_by_timer) {
        activate_timer();
    }
}

bool RedScreen::_invalidate(const SpiceRect& rect, bool urgent, uint64_t& update_mark)
{
    RecurciveLock lock(_update_lock);
    bool update_triger = !is_dirty() && (urgent || !_periodic_update);
    region_add(&_dirty_region, &rect);
    update_mark = _update_mark;
    return update_triger;
}

uint64_t RedScreen::invalidate(const SpiceRect& rect, bool urgent)
{
    uint64_t update_mark;
    if (_invalidate(rect, urgent, update_mark)) {
        if (!urgent && _update_by_timer) {
            activate_timer();
        } else {
            if (update_by_interrupt()) {
                interrupt_update();
            } else {
                AutoRef<UpdateEvent> update_event(new UpdateEvent(_id));
                _owner.push_event(*update_event);
            }
        }
    }
    return update_mark;
}

void RedScreen::invalidate(const QRegion &region)
{
    pixman_box32_t *rects, *end;
    int num_rects;

    rects = pixman_region32_rectangles((pixman_region32_t *)&region, &num_rects);
    end = rects + num_rects;

    while (rects != end) {
        SpiceRect r;

        r.left = rects->x1;
        r.top = rects->y1;
        r.right = rects->x2;
        r.bottom = rects->y2;
        rects++;

        invalidate(r, false);
    }
}

inline void RedScreen::erase_background(RedDrawable& dc, const QRegion& composit_rgn)
{
    pixman_box32_t *rects;
    int num_rects;

    rects = pixman_region32_rectangles((pixman_region32_t *)&composit_rgn, &num_rects);
    for (int i = 0; i < num_rects; i++) {
        SpiceRect r;

        r.left = rects[i].x1;
        r.top = rects[i].y1;
        r.right = rects[i].x2;
        r.bottom = rects[i].y2;

        dc.fill_rect(r, 0);
    }
}

void RedScreen::reset_mouse_pos()
{
    _window.set_mouse_position(_mouse_anchor_point.x, _mouse_anchor_point.y);
}

void RedScreen::capture_mouse()
{
    if (_mouse_captured || !_window.get_mouse_anchor_point(_mouse_anchor_point)) {
        return;
    }

    if (_pointer_layer) {
        _pointer_layer->on_pointer_leave();
        _pointer_layer = NULL;
    }
    _pointer_on_screen = false;
    _mouse_captured = true;
    _window.hide_cursor();
    reset_mouse_pos();
    _window.capture_mouse();
}

void RedScreen::relase_mouse()
{
    if (!_mouse_captured) {
        return;
    }
    _mouse_captured = false;
    _window.release_mouse();
    update_pointer_layer();
}

void RedScreen::set_cursor(LocalCursor* cursor)
{
    if (_mouse_captured) {
        return;
    }

    _window.set_cursor(cursor);
}

void RedScreen::hide_cursor()
{
    _window.hide_cursor();
}

ScreenLayer* RedScreen::find_pointer_layer()
{
    for (int i = _layers.size() - 1; i >= 0; i--) {
        ScreenLayer* layer;

        if (!(layer = _layers[i])) {
            continue;
        }

        if (layer->pointer_test(_pointer_pos.x, _pointer_pos.y)) {
            return layer;
        }
    }
    return NULL;
}

bool RedScreen::update_pointer_layer()
{
    ASSERT(!_mouse_captured);

    ScreenLayer* now = find_pointer_layer();

    if (now == _pointer_layer) {
        return false;
    }

    if (_pointer_layer) {
        _pointer_layer->on_pointer_leave();
    }

    _pointer_layer = find_pointer_layer();

    if (_pointer_layer) {
        _pointer_layer->on_pointer_enter(_pointer_pos.x, _pointer_pos.y, _mouse_botton_state);
    } else {
        set_cursor(_inactive_cursor);
    }

    return true;
}

void RedScreen::on_pointer_enter(int x, int y, unsigned int buttons_state)
{
    if (_mouse_captured) {
        return;
    }

    _pointer_on_screen = true;
    _pointer_pos.x = x;
    _pointer_pos.y = y;
    _mouse_botton_state = buttons_state;

    ScreenLayer* layer = find_pointer_layer();
    if (!layer) {
        set_cursor(_inactive_cursor);
        return;
    }

    _pointer_layer = layer;
    _pointer_layer->on_pointer_enter(_pointer_pos.x, _pointer_pos.y, buttons_state);

    if (_full_screen) {
        /* allowing enterance to key interception mode without
           requiring the user to press the window
        */
        activate();
    }
}

void RedScreen::on_mouse_motion(int x, int y, unsigned int buttons_state)
{
    if (x != _mouse_anchor_point.x || y != _mouse_anchor_point.y) {
        _owner.on_mouse_motion(x - _mouse_anchor_point.x,
                               y - _mouse_anchor_point.y,
                               buttons_state);
        reset_mouse_pos();
    }
}

void RedScreen::on_pointer_motion(int x, int y, unsigned int buttons_state)
{
    if (_mouse_captured) {
        on_mouse_motion(x, y, buttons_state);
        return;
    }

    _pointer_pos.x = x;
    _pointer_pos.y = y;
    _mouse_botton_state = buttons_state;

    if (update_pointer_layer() || !_pointer_layer) {
        return;
    }

    _pointer_layer->on_pointer_motion(x, y, buttons_state);
}

void RedScreen::on_mouse_button_press(SpiceMouseButton button, unsigned int buttons_state)
{
    if (_mouse_captured) {
        _owner.on_mouse_down(button, buttons_state);
        return;
    }

    if (!_pointer_layer) {
        return;
    }

    _pointer_layer->on_mouse_button_press(button, buttons_state);
}

void RedScreen::on_mouse_button_release(SpiceMouseButton button, unsigned int buttons_state)
{
    if (_mouse_captured) {
        _owner.on_mouse_up(button, buttons_state);
        return;
    }

    if (!_pointer_layer) {
        return;
    }
    _pointer_layer->on_mouse_button_release(button, buttons_state);
}

void RedScreen::on_pointer_leave()
{
//    ASSERT(!_mouse_captured);

    if (_pointer_layer) {
        _pointer_layer->on_pointer_leave();
        _pointer_layer = NULL;
    }
    _pointer_on_screen = false;
}

void RedScreen::on_key_press(RedKey key)
{
    _owner.on_key_down(key);
}

void RedScreen::on_key_release(RedKey key)
{
    _owner.on_key_up(key);
}

void RedScreen::on_char(uint32_t ch)
{
    _owner.on_char(ch);
}

void RedScreen::on_deactivate()
{
    relase_mouse();
    _active = false;
    _owner.on_deactivate_screen(this);
}

void RedScreen::on_activate()
{
    _active = true;
    _owner.on_activate_screen(this);
}

void RedScreen::on_start_key_interception()
{
    _key_interception = true;
    _owner.on_start_screen_key_interception(this);
}

void RedScreen::on_stop_key_interception()
{
    _key_interception = false;
    _owner.on_stop_screen_key_interception(this);
}

void RedScreen::enter_modal_loop()
{
    _force_update_timer++;
    activate_timer();
}

void RedScreen::exit_modal_loop()
{
    ASSERT(_force_update_timer > 0)
    _force_update_timer--;
}

void RedScreen::pre_migrate()
{
    for (int i = 0; i < (int)_layers.size(); i++) {
        if (!_layers[i]) {
            continue;
        }
        _layers[i]->pre_migrate();
    }
}

void RedScreen::post_migrate()
{
    for (int i = 0; i < (int)_layers.size(); i++) {
        if (!_layers[i]) {
            continue;
        }
        _layers[i]->post_migrate();
    }
}

void RedScreen::exit_full_screen()
{
    if (!_full_screen) {
        return;
    }
    RecurciveLock lock(_update_lock);
    _window.hide();
    region_clear(&_dirty_region);
    _window.set_type(RedWindow::TYPE_NORMAL);
    adjust_window_rect(_save_pos.x, _save_pos.y);
    _origin.x = _origin.y = 0;
    _window.set_origin(0, 0);
    show();
    if (_menu_needs_update) {
        update_menu();
    }
    _full_screen = false;
    _out_of_sync = false;
    _frame_area = false;
}

void RedScreen::save_position()
{
    _save_pos = _window.get_position();
}

void RedScreen::__show_full_screen()
{
    if (!_monitor) {
        hide();
        return;
    }
    SpicePoint position = _monitor->get_position();
    SpicePoint monitor_size = _monitor->get_size();
    _frame_area = false;
    region_clear(&_dirty_region);
    _window.set_type(RedWindow::TYPE_FULLSCREEN);
    _window.move_and_resize(position.x, position.y, monitor_size.x, monitor_size.y);

    if (!(_out_of_sync = _monitor->is_out_of_sync())) {
        ASSERT(monitor_size.x >= _size.x);
        ASSERT(monitor_size.y >= _size.y);
        _origin.x = (monitor_size.x - _size.x) / 2;
        _origin.y = (monitor_size.y - _size.y) / 2;
        _frame_area = monitor_size.x != _size.x || monitor_size.y != _size.y;
    } else {
        _origin.x = _origin.y = 0;
    }
    _window.set_origin(_origin.x, _origin.y);
    show();
}

void RedScreen::show_full_screen()
{
    if (_full_screen) {
        return;
    }
    RecurciveLock lock(_update_lock);
#ifndef WIN32
    /* performing hide during resolution changes resulted in
       missing WM_KEYUP events */
    hide();
#endif
    save_position();
    _full_screen = true;
    __show_full_screen();
}

void RedScreen::minimize()
{
    _window.minimize();
}

void RedScreen::position_full_screen(const SpicePoint& position)
{
    if (!_full_screen) {
        return;
    }

    _window.move(position.x, position.y);
}

void RedScreen::hide()
{
    _window.hide();
}

void RedScreen::show()
{
    RecurciveLock lock(_update_lock);
    _window.show(_monitor ? _monitor->get_screen_id() : 0);
}

void RedScreen::activate()
{
    _window.activate();
}

void RedScreen::external_show()
{
    DBG(0, "Entry");
    _window.external_show();
}

void RedScreen::update_menu()
{
    AutoRef<Menu> menu(_owner.get_app_menu());
    int ret = _window.set_menu(*menu);
    _menu_needs_update = (ret != 0); /* try again if menu update failed */
}

void RedScreen::on_exposed_rect(const SpiceRect& area)
{
    if (is_out_of_sync()) {
        _window.fill_rect(area, rgb32_make(0xff, 0xff, 0xff));
        return;
    }
    invalidate(area, false);
}

int RedScreen::get_screen_id()
{
    return _monitor ? _monitor->get_screen_id() : 0;
}

#ifdef USE_OPENGL
void RedScreen::untouch_context()
{
    _window.untouch_context();
}

bool RedScreen::need_recreate_context_gl()
{
    if (_full_screen) {
        return true;
    }
    return false;
}

#endif

void RedScreen::set_update_interrupt_trigger(EventSources::Trigger *trigger)
{
    _update_interrupt_trigger = trigger;
}

bool RedScreen::update_by_interrupt()
{
    return _update_interrupt_trigger != NULL;
}

void RedScreen::interrupt_update()
{
    _update_interrupt_trigger->trigger();
}

#ifdef USE_OPENGL
void RedScreen::set_type_gl()
{
    _window.set_type_gl();
}

void RedScreen::unset_type_gl()
{
    _window.unset_type_gl();
}
#endif // USE_OPENGL
