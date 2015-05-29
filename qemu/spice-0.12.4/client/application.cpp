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
#ifdef WIN32
#include <io.h>
#endif

#include "common/quic.h"
#include "common/mutex.h"
#include "common/rect.h"

#include "application.h"
#include "screen.h"
#include "utils.h"
#include "debug.h"
#include "screen_layer.h"
#include "monitor.h"
#include "resource.h"
#ifdef WIN32
#include "red_gdi_canvas.h"
#endif
#include "platform.h"
#include "red_sw_canvas.h"
#ifdef USE_OPENGL
#include "red_gl_canvas.h"
#endif
#include "cmd_line_parser.h"
#ifdef USE_TUNNEL
#include "tunnel_channel.h"
#endif
#ifdef USE_GUI
#include "gui/gui.h"
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef USE_SMARTCARD
#include <smartcard_channel.h>
#endif

#define STICKY_KEY_PIXMAP ALT_IMAGE_RES_ID
#define STICKY_KEY_TIMEOUT 750

#define CA_FILE_NAME "spice_truststore.pem"

static const char* app_name = "spicec";

void ConnectedEvent::response(AbstractProcessLoop& events_loop)
{
    static_cast<Application*>(events_loop.get_owner())->on_connected();
}

void DisconnectedEvent::response(AbstractProcessLoop& events_loop)
{
    Application* app = static_cast<Application*>(events_loop.get_owner());
    app->on_disconnected(_error_code);
}

void VisibilityEvent::response(AbstractProcessLoop& events_loop)
{
    Application* app = static_cast<Application*>(events_loop.get_owner());
    app->on_visibility_start(_screen_id);
}

void MonitorsQuery::do_response(AbstractProcessLoop& events_loop)
{
    Monitor* mon;
    int i = 0;

    while ((mon = (static_cast<Application*>(events_loop.get_owner()))->find_monitor(i++))) {
        MonitorInfo info;
        info.size = mon->get_size();
        info.depth = 32;
        info.position = mon->get_position();
        _monitors.push_back(info);
    }
}

SwitchHostEvent::SwitchHostEvent(const char* host, int port, int sport, const char* cert_subject)
{
    _host = host;
    _port = port;
    _sport = sport;
    if (cert_subject) {
        _cert_subject = cert_subject;
    }
}

void SwitchHostEvent::response(AbstractProcessLoop& events_loop)
{
    Application* app = static_cast<Application*>(events_loop.get_owner());
    app->switch_host(_host, _port, _sport, _cert_subject);
}

#ifdef USE_GUI
//todo: add inactive visual appearance
class GUIBarrier: public ScreenLayer {
public:
    GUIBarrier(int id)
        : ScreenLayer(SCREEN_LAYER_GUI_BARIER, true)
        , _id (id)
        , _cursor (Platform::create_inactive_cursor())
    {
    }

    ~GUIBarrier()
    {
        detach();
    }

    int get_id() { return _id;}

    void attach(RedScreen& in_screen)
    {
        if (screen()) {
            ASSERT(&in_screen == screen())
            return;
        }
        in_screen.attach_layer(*this);
    }

    void detach()
    {
        if (!screen()) {
            return;
        }
        screen()->detach_layer(*this);
    }

    virtual bool pointer_test(int x, int y) { return true;}
    virtual void on_pointer_enter(int x, int y, unsigned int buttons_state)
    {
        AutoRef<LocalCursor> cursor(Platform::create_inactive_cursor());
        screen()->set_cursor(*cursor);
        return;
    }

private:
    int _id;
    AutoRef<LocalCursor> _cursor;
};

class GUITimer: public Timer {
public:
    GUITimer(GUI& gui)
        : _gui (gui)
    {
    }

    virtual void response(AbstractProcessLoop& events_loop)
    {
        _gui.idle();
    }

private:
    GUI& _gui;
};


#ifdef GUI_DEMO
class TestTimer: public Timer {
public:
    TestTimer(Application& app)
        : _app (app)
    {
    }

    virtual void response(AbstractProcessLoop& events_loop)
    {
        _app.message_box_test();
    }

private:
    Application& _app;
};
#endif

#endif // USE_GUI

class InfoLayer: public ScreenLayer {
public:
    InfoLayer();

    virtual void copy_pixels(const QRegion& dest_region, RedDrawable& dest_dc);

    void set_info_mode();
    void set_sticky(bool is_on);
    virtual void on_size_changed();

private:
    void draw_info(const QRegion& dest_region, RedDrawable& dest);
    void update_sticky_rect();

private:
    AlphaImageFromRes _sticky_pixmap;
    SpicePoint _sticky_pos;
    SpiceRect _sticky_rect;
    bool _sticky_on;
    RecurciveMutex _update_lock;
};

InfoLayer::InfoLayer()
    : ScreenLayer(SCREEN_LAYER_INFO, false)
    , _sticky_pixmap (STICKY_KEY_PIXMAP)
    , _sticky_on (false)
{
}

void InfoLayer::draw_info(const QRegion& dest_region, RedDrawable& dest)
{
    pixman_box32_t *rects;
    int num_rects;

    rects = pixman_region32_rectangles((pixman_region32_t *)&dest_region, &num_rects);
    for (int i = 0; i < num_rects; i++) {
        SpiceRect r;

        r.left = rects[i].x1;
        r.top = rects[i].y1;
        r.right = rects[i].x2;
        r.bottom = rects[i].y2;

        /* is rect inside sticky region ? */
        if (_sticky_on && rect_intersects(r, _sticky_rect)) {
            dest.blend_pixels(_sticky_pixmap, r.left - _sticky_pos.x, r.top - _sticky_pos.y, r);
        }
    }
}

void InfoLayer::copy_pixels(const QRegion& dest_region, RedDrawable& dest_dc)
{
    RecurciveLock lock(_update_lock);
    draw_info(dest_region, dest_dc);
}

void InfoLayer::set_info_mode()
{
    ASSERT(screen());

    clear_area();
    update_sticky_rect();
    set_sticky(_sticky_on);
}

void InfoLayer::update_sticky_rect()
{
    SpicePoint size = _sticky_pixmap.get_size();
    SpicePoint screen_size = screen()->get_size();

    _sticky_pos.x = (screen_size.x - size.x) / 2;
    _sticky_pos.y = screen_size.y * 2 / 3;
    _sticky_rect.left = _sticky_pos.x;
    _sticky_rect.top = _sticky_pos.y;
    _sticky_rect.right = _sticky_rect.left + size.x;
    _sticky_rect.bottom = _sticky_rect.top + size.y;
}

void InfoLayer::set_sticky(bool is_on)
{
    RecurciveLock lock(_update_lock);
    if (!_sticky_on && !is_on) {
        return;
    }

    _sticky_on = is_on;

    if (_sticky_on) {
        add_rect_area(_sticky_rect);
        invalidate(_sticky_rect);
    } else {
        remove_rect_area(_sticky_rect);
    }
}

void InfoLayer::on_size_changed()
{
    set_info_mode();
}

void StickyKeyTimer::response(AbstractProcessLoop& events_loop)
{
    Application* app = (Application*)events_loop.get_owner();
    StickyInfo* sticky_info = &app->_sticky_info;
    ASSERT(app->is_sticky_trace_key(sticky_info->key));
    ASSERT(app->_keyboard_state[sticky_info->key]);
    ASSERT(sticky_info->key_first_down);
    ASSERT(sticky_info->key_down);
    sticky_info->sticky_mode = true;
    DBG(0, "ON sticky");
    app->_info_layer->set_sticky(true);
    app->deactivate_interval_timer(this);
}

static MouseHandler default_mouse_handler;
static KeyHandler default_key_handler;

enum AppCommands {
    APP_CMD_INVALID,
    APP_CMD_SEND_CTL_ALT_DEL,
    APP_CMD_TOGGLE_FULL_SCREEN,
    APP_CMD_RELEASE_CAPTURE,
    APP_CMD_SEND_TOGGLE_KEYS,
    APP_CMD_SEND_RELEASE_KEYS,
    APP_CMD_SEND_CTL_ALT_END,
#ifdef RED_DEBUG
    APP_CMD_CONNECT,
    APP_CMD_DISCONNECT,
#endif
#ifdef USE_GUI
    APP_CMD_SHOW_GUI,
#endif // USE_GUI
#ifdef USE_SMARTCARD
    APP_CMD_SMARTCARD_INSERT,
    APP_CMD_SMARTCARD_REMOVE,
#endif
    APP_CMD_EXTERNAL_BEGIN = 0x400,
    APP_CMD_EXTERNAL_END = 0x800,
};

Application::Application()
    : ProcessLoop (this)
    , _client (*this)
    , _con_ciphers ("DEFAULT")
    , _enabled_channels (SPICE_END_CHANNEL, true)
    , _main_screen (NULL)
    , _active (false)
    , _full_screen (false)
    , _changing_screens (false)
    , _out_of_sync (false)
    , _exit_code (0)
    , _active_screen (NULL)
    , _num_keys_pressed (0)
    , _info_layer (new InfoLayer())
    , _key_handler (&default_key_handler)
    , _mouse_handler (&default_mouse_handler)
    , _monitors (NULL)
    , _title ("SPICEc:%d")
    , _sys_key_intercept_mode (false)
    , _enable_controller (false)
#ifdef USE_GUI
    , _gui_mode (GUI_MODE_FULL)
#endif // USE_GUI
    , _during_host_switch(false)
    , _state (DISCONNECTED)
#ifdef USE_SMARTCARD
    , _smartcard_options(new SmartcardOptions())
#endif
{
    DBG(0, "");

    _commands_map["toggle-fullscreen"] = APP_CMD_TOGGLE_FULL_SCREEN;
    _commands_map["release-cursor"] = APP_CMD_RELEASE_CAPTURE;
#ifdef RED_DEBUG
    _commands_map["connect"] = APP_CMD_CONNECT;
    _commands_map["disconnect"] = APP_CMD_DISCONNECT;
#endif
#ifdef USE_GUI
    _commands_map["show-gui"] = APP_CMD_SHOW_GUI;
#endif // USE_GUI
#ifdef USE_SMARTCARD
    _commands_map["smartcard-insert"] = APP_CMD_SMARTCARD_INSERT;
    _commands_map["smartcard-remove"] = APP_CMD_SMARTCARD_REMOVE;
#endif

    _canvas_types.resize(1);
#ifdef WIN32
    _canvas_types[0] = CANVAS_OPTION_GDI;
#else
    _canvas_types[0] = CANVAS_OPTION_SW;
#endif

    _host_auth_opt.type_flags = SPICE_SSL_VERIFY_OP_HOSTNAME;

    Platform::get_app_data_dir(_host_auth_opt.CA_file, app_name);
    Platform::path_append(_host_auth_opt.CA_file, CA_FILE_NAME);

    this->set_default_hotkeys();

    _sticky_info.trace_is_on = false;
    _sticky_info.sticky_mode = false;
    _sticky_info.key_first_down = false;
    _sticky_info.key_down = false;
    _sticky_info.key  = REDKEY_INVALID;
    _sticky_info.timer.reset(new StickyKeyTimer());

    for (int i = SPICE_CHANNEL_MAIN; i < SPICE_END_CHANNEL; i++) {
        _peer_con_opt[i] = RedPeer::ConnectionOptions::CON_OP_BOTH;
    }
    memset(_keyboard_state, 0, sizeof(_keyboard_state));
}

Application::~Application()
{
#ifdef USE_GUI
    if (*_gui_timer != NULL) {
        deactivate_interval_timer(*_gui_timer);
    }
#ifdef GUI_DEMO
    if (*_gui_test_timer != NULL) {
        deactivate_interval_timer(*_gui_test_timer);
    }
#endif
    destroyed_gui_barriers();
    if (_gui.get() != NULL) {
        _gui->set_screen(NULL);
    }
#endif // USE_GUI

    if (_info_layer->screen()) {
        _main_screen->detach_layer(*_info_layer);
    }

    if (_main_screen != NULL) {
        _main_screen->unref();
        destroy_monitors();
    }
#ifdef USE_SMARTCARD
    delete _smartcard_options;
#endif
}

void Application::init_menu()
{
    //fixme: menu items name need to be dynamically updated by hot keys configuration.
    AutoRef<Menu> root_menu(new Menu(*this, ""));
    (*root_menu)->add_command("Send Ctrl+Alt+Del\tCtrl+Alt+End", APP_CMD_SEND_CTL_ALT_DEL);
    (*root_menu)->add_command("Toggle full screen\tShift+F11", APP_CMD_TOGGLE_FULL_SCREEN);
    AutoRef<Menu> key_menu(new Menu(*this, "Special keys"));
    (*key_menu)->add_command("Send Shift+F11", APP_CMD_SEND_TOGGLE_KEYS);
    (*key_menu)->add_command("Send Shift+F12", APP_CMD_SEND_RELEASE_KEYS);
    (*key_menu)->add_command("Send Ctrl+Alt+End", APP_CMD_SEND_CTL_ALT_END);
    (*root_menu)->add_sub(key_menu.release());
    _app_menu.reset(root_menu.release());
}

void Application::__remove_key_handler(KeyHandler& handler)
{
    KeyHandlersStack::iterator iter = _key_handlers.begin();

    for (; iter != _key_handlers.end(); iter++) {
        if (*iter == &handler) {
            _key_handlers.erase(iter);
            return;
        }
    }
}

void Application::set_key_handler(KeyHandler& handler)
{
    if (&handler == _key_handler) {
        return;
    }

    __remove_key_handler(handler);
    if (!_key_handler->permit_focus_loss()) {
        _key_handlers.push_front(&handler);
        return;
    }

    unpress_all();
    if (_active) {
        _key_handler->on_focus_out();
    }

    _key_handlers.push_front(_key_handler);
    _key_handler = &handler;
    if (_active) {
        _key_handler->on_focus_in();
    }
}

void Application::remove_key_handler(KeyHandler& handler)
{
    bool is_current = (&handler == _key_handler);

    if (!is_current) {
        __remove_key_handler(handler);
        return;
    }

    KeyHandler damy_handler;
    _key_handler = &damy_handler;
    set_key_handler(**_key_handlers.begin());
    __remove_key_handler(damy_handler);
}

void Application::set_mouse_handler(MouseHandler& handler)
{
    _mouse_handler = &handler;
}

void Application::remove_mouse_handler(MouseHandler& handler)
{
    if (_mouse_handler != &handler) {
        return;
    }
    _mouse_handler = &default_mouse_handler;
}

void Application::capture_mouse()
{
    if (!_active_screen
#ifdef USE_GUI
        || _gui->screen()
#endif // USE_GUI
        ) {
        return;
    }
    _active_screen->capture_mouse();
}

void Application::release_mouse_capture()
{
    if (!_active_screen || !_active_screen->is_mouse_captured()) {
        return;
    }
    _active_screen->relase_mouse();
}

void Application::abort()
{
    Platform::set_event_listener(NULL);
    Platform::set_display_mode_listner(NULL);
    unpress_all();
    while (!_client.abort()) {
        ProcessLoop::process_events_queue();
        Platform::msleep(100);
    }
}

class AutoAbort {
public:
    AutoAbort(Application& app) : _app(app) {}
    ~AutoAbort() { _app.abort();}

private:
    Application& _app;
};

void Application::connect()
{
    ASSERT(_state == DISCONNECTED);
    set_state(CONNECTING);
    _client.connect();
}

void Application::switch_host(const std::string& host, int port, int sport,
                              const std::string& cert_subject)
{
    LOG_INFO("host=%s port=%d sport=%d", host.c_str(), port, sport);
    _during_host_switch = true;
    // we will try to connect to the new host when DiconnectedEvent occurs
    do_disconnect();
    _client.set_target(host.c_str(), port, sport);

    if (!cert_subject.empty()) {
        set_host_cert_subject(cert_subject.c_str(), "spicec");
    }
}

int Application::run()
{
    _exit_code = ProcessLoop::run();
    return _exit_code;
}

void Application::on_start_running()
{
    _foreign_menu.reset(new ForeignMenu(this, _active));
    if (_enable_controller) {
        _controller.reset(new Controller(this));
        return;
    }
    //FIXME: _client.connect() or use the following instead?
#ifdef USE_GUI
    if (_gui_mode == GUI_MODE_FULL) {
        show_gui();
        return;
    }
#endif // HAVE_GUI
    connect();
}

RedScreen* Application::find_screen(int id)
{
    if ((int)_screens.size() < id + 1) {
        return NULL;
    }
    return _screens[id];
}

bool Application::release_capture()
{
    unpress_all();
    if (!_active_screen || !_active_screen->is_mouse_captured()) {
        return false;
    }
    _active_screen->relase_mouse();
    return true;
}

bool Application::do_connect()
{
    _client.connect();
    return true;
}

bool Application::do_disconnect()
{
    on_disconnecting();
    _client.disconnect();
    return true;
}

#define SCREEN_INIT_WIDTH 800
#define SCREEN_INIT_HEIGHT 600

RedScreen* Application::get_screen(int id)
{
    RedScreen *screen;

    if ((int)_screens.size() < id + 1) {
        _screens.resize(id + 1);
    }

    if (!(screen = _screens[id])) {
        Monitor* mon = find_monitor(id);
        SpicePoint size = {SCREEN_INIT_WIDTH, SCREEN_INIT_HEIGHT};

        if (_full_screen && mon) {
            size = mon->get_size();
        }
        screen = _screens[id] = new RedScreen(*this, id, _title, size.x, size.y);
#ifdef USE_GUI
        create_gui_barrier(*screen, id);
#endif // USE_GUI

        if (id != 0) {
            if (_full_screen) {
                mon = get_monitor(id);
                screen->set_monitor(mon);
                rearrange_monitors(false, true, screen);
            } else {
                screen->show(false, _main_screen);
            }
        }
    } else {
        screen = screen->ref();
    }

    return screen;
}

#ifdef USE_GUI
void Application::attach_gui_barriers()
{
    GUIBarriers::iterator iter = _gui_barriers.begin();

    for (; iter != _gui_barriers.end(); iter++) {
        GUIBarrier* barrier = *iter;
        ASSERT((int)_screens.size() > barrier->get_id() && _screens[barrier->get_id()]);
        barrier->attach(*_screens[barrier->get_id()]);
    }
}

void Application::detach_gui_barriers()
{
    GUIBarriers::iterator iter = _gui_barriers.begin();

    for (; iter != _gui_barriers.end(); iter++) {
        GUIBarrier* barrier = *iter;
        barrier->detach();
    }
}

void Application::create_gui_barrier(RedScreen& screen, int id)
{
     GUIBarrier* barrier = new GUIBarrier(id);
     _gui_barriers.push_front(barrier);
     if (_gui.get() && _gui->screen()) {
         barrier->attach(screen);
     }
}

void Application::destroyed_gui_barriers()
{
    while (_gui_barriers.begin() != _gui_barriers.end()) {
        GUIBarrier* barrier = *_gui_barriers.begin();
        _gui_barriers.erase(_gui_barriers.begin());
        delete barrier;
    }
}

void Application::destroyed_gui_barrier(int id)
{
    GUIBarriers::iterator iter = _gui_barriers.begin();

    for (; iter != _gui_barriers.end(); iter++) {
        GUIBarrier* barrier = *iter;
        if (barrier->get_id() == id) {
            _gui_barriers.erase(iter);
            delete barrier;
            return;
        }
    }
}
#endif // USE_GUI

void Application::on_screen_destroyed(int id, bool was_captured)
{
    bool reposition = false;

#ifdef USE_GUI
    destroyed_gui_barrier(id);
#endif // USE_GUI

    if ((int)_screens.size() < id + 1 || !_screens[id]) {
        THROW("no screen");
    }
    if (_active_screen == _screens[id]) {
        _active_screen = NULL;
    }

    if (_full_screen && _screens[id]->has_monitor()) {
        _screens[id]->get_monitor()->restore();
        reposition = true;
    }
    _screens[id] = NULL;
    if (reposition) {
        rearrange_monitors(was_captured, false);
    }
}

void Application::on_mouse_motion(int dx, int dy, int buttons_state)
{
    _mouse_handler->on_mouse_motion(dx, dy, buttons_state);
}

void Application::on_mouse_down(int button, int buttons_state)
{
    _mouse_handler->on_mouse_down(button, buttons_state);
}

void Application::on_mouse_up(int button, int buttons_state)
{
    _mouse_handler->on_mouse_up(button, buttons_state);
}

void Application::unpress_all()
{
    reset_sticky();
    for (int i = 0; i < REDKEY_NUM_KEYS; i++) {
        if (_keyboard_state[i]) {
            _key_handler->on_key_up((RedKey)i);
            unpress_key((RedKey)i);
        }
    }
}

void Application::set_state(State state)
{
    if (state == _state) {
        return;
    }
    _state = state;
#ifdef USE_GUI
    _gui->set_state(_state);
    if (_gui->screen() && !_gui->is_visible()) {
        hide_gui();
    }
#endif // USE_GUI
    reset_sticky();
}

void Application::on_connected()
{
    _during_host_switch = false;

    set_state(CONNECTED);
}

void Application::on_disconnected(int error_code)
{
    bool host_switch = _during_host_switch && (error_code == SPICEC_ERROR_CODE_SUCCESS);
    if (
#ifdef USE_GUI
    _gui_mode != GUI_MODE_FULL &&
#endif // USE_GUI
    !host_switch) {
        _during_host_switch = false;
        ProcessLoop::quit(error_code);
        return;
    }

    // todo: display special notification for host switch (during migration)
    set_state(DISCONNECTED);
#ifdef USE_GUI
    show_gui();
#endif
    if (host_switch) {
        _client.connect(true);
    }
}

void Application::on_visibility_start(int screen_id)
{
    if (screen_id) {
        return;
    }
    set_state(VISIBILITY);
#ifdef USE_GUI
    hide_gui();
#endif
    show_info_layer();
}

void Application::on_disconnecting()
{
    release_capture();
}

Menu* Application::get_app_menu()
{
    if (!*_app_menu) {
        return NULL;
    }
    return (*_app_menu)->ref();
}

void Application::show_info_layer()
{
    if (_info_layer->screen() || _state != VISIBILITY) {
        return;
    }

    _main_screen->attach_layer(*_info_layer);
    _info_layer->set_info_mode();
    reset_sticky();
}

void Application::hide_info_layer()
{
    if (!_info_layer->screen()) {
        return;
    }

    _main_screen->detach_layer(*_info_layer);
    reset_sticky();
}

#ifdef GUI_DEMO

class TestResponce: public GUI::BoxResponse {
public:
    virtual void response(int response)
    {
        DBG(0, "%d", response);
    }

    virtual void aborted()
    {
        DBG(0, "");
    }
};


TestResponce response_test;

void Application::message_box_test()
{
    GUI::ButtonsList list(3);
    list[0].id = 101;
    list[0].text = "Yes";
    list[1].id = 102;
    list[1].text = "No";
    list[2].id = 103;
    list[2].text = "Don't Know";


    if (!_gui->message_box(GUI::QUESTION, "My question", list, &response_test)) {
        DBG(0, "busy");
    } else {
        show_gui();
    }
}

#endif

#ifdef USE_GUI
void Application::show_gui()
{
    if (_gui->screen() || !_gui->prepare_dialog()) {
        return;
    }

    hide_info_layer();
    release_capture();
    _gui->set_screen(_main_screen);
    attach_gui_barriers();
}

void Application::hide_gui()
{
    if (!_gui->screen()) {
        return;
    }

    _gui->set_screen(NULL);
    detach_gui_barriers();
    show_info_layer();
}
#endif // USE_GUI

void Application::do_command(int command)
{
    switch (command) {
    case APP_CMD_SEND_CTL_ALT_DEL:
        send_alt_ctl_del();
        break;
    case APP_CMD_TOGGLE_FULL_SCREEN:
        toggle_full_screen();
        break;
    case APP_CMD_SEND_TOGGLE_KEYS:
        send_command_hotkey(APP_CMD_SEND_TOGGLE_KEYS);
        break;
    case APP_CMD_SEND_RELEASE_KEYS:
        send_command_hotkey(APP_CMD_SEND_RELEASE_KEYS);
        break;
    case APP_CMD_SEND_CTL_ALT_END:
        send_ctrl_alt_end();
        break;
    case APP_CMD_RELEASE_CAPTURE:
        release_capture();
        break;
#ifdef RED_DEBUG
    case APP_CMD_CONNECT:
        do_connect();
        break;
    case APP_CMD_DISCONNECT:
        do_disconnect();
        break;
#endif
#ifdef USE_GUI
    case APP_CMD_SHOW_GUI:
        show_gui();
        break;
#endif // USE_GUI
#ifdef USE_SMARTCARD
    case APP_CMD_SMARTCARD_INSERT:
        virtual_card_insert();
        break;
    case APP_CMD_SMARTCARD_REMOVE:
        virtual_card_remove();
        break;
#endif
    default:
        AppMenuItemMap::iterator iter = _app_menu_items.find(command);
        ASSERT(iter != _app_menu_items.end());
        AppMenuItem* item = &(*iter).second;
        if (item->type == APP_MENU_ITEM_TYPE_FOREIGN) {
            ASSERT(*_foreign_menu);
            (*_foreign_menu)->on_command(item->conn_ref, item->ext_id);
        } else if (item->type == APP_MENU_ITEM_TYPE_CONTROLLER) {
            ASSERT(*_controller);
            (*_controller)->on_command(item->conn_ref, item->ext_id);
        }
    }
}

#ifdef REDKEY_DEBUG

static void show_red_key(RedKey key)
{

#define KEYCASE(red_key)    \
    case red_key:           \
        DBG(0, #red_key);   \
        return;

    switch (key) {
        KEYCASE(REDKEY_INVALID);
        KEYCASE(REDKEY_ESCAPE);
        KEYCASE(REDKEY_1);
        KEYCASE(REDKEY_2);
        KEYCASE(REDKEY_3);
        KEYCASE(REDKEY_4);
        KEYCASE(REDKEY_5);
        KEYCASE(REDKEY_6);
        KEYCASE(REDKEY_7);
        KEYCASE(REDKEY_8);
        KEYCASE(REDKEY_9);
        KEYCASE(REDKEY_0);
        KEYCASE(REDKEY_MINUS);
        KEYCASE(REDKEY_EQUALS);
        KEYCASE(REDKEY_BACKSPACE);
        KEYCASE(REDKEY_TAB);
        KEYCASE(REDKEY_Q);
        KEYCASE(REDKEY_W);
        KEYCASE(REDKEY_E);
        KEYCASE(REDKEY_R);
        KEYCASE(REDKEY_T);
        KEYCASE(REDKEY_Y);
        KEYCASE(REDKEY_U);
        KEYCASE(REDKEY_I);
        KEYCASE(REDKEY_O);
        KEYCASE(REDKEY_P);
        KEYCASE(REDKEY_ENTER);
        KEYCASE(REDKEY_L_BRACKET);
        KEYCASE(REDKEY_R_BRACKET);
        KEYCASE(REDKEY_L_CTRL);
        KEYCASE(REDKEY_A);
        KEYCASE(REDKEY_S);
        KEYCASE(REDKEY_D);
        KEYCASE(REDKEY_F);
        KEYCASE(REDKEY_G);
        KEYCASE(REDKEY_H);
        KEYCASE(REDKEY_J);
        KEYCASE(REDKEY_K);
        KEYCASE(REDKEY_L);
        KEYCASE(REDKEY_SEMICOLON);
        KEYCASE(REDKEY_QUOTE);
        KEYCASE(REDKEY_BACK_QUOTE);
        KEYCASE(REDKEY_L_SHIFT);
        KEYCASE(REDKEY_BACK_SLASH);
        KEYCASE(REDKEY_Z);
        KEYCASE(REDKEY_X);
        KEYCASE(REDKEY_C);
        KEYCASE(REDKEY_V);
        KEYCASE(REDKEY_B);
        KEYCASE(REDKEY_N);
        KEYCASE(REDKEY_M);
        KEYCASE(REDKEY_COMMA);
        KEYCASE(REDKEY_PERIOD);
        KEYCASE(REDKEY_SLASH);
        KEYCASE(REDKEY_R_SHIFT);
        KEYCASE(REDKEY_PAD_MULTIPLY);
        KEYCASE(REDKEY_L_ALT);
        KEYCASE(REDKEY_SPACE);
        KEYCASE(REDKEY_CAPS_LOCK);
        KEYCASE(REDKEY_F1);
        KEYCASE(REDKEY_F2);
        KEYCASE(REDKEY_F3);
        KEYCASE(REDKEY_F4);
        KEYCASE(REDKEY_F5);
        KEYCASE(REDKEY_F6);
        KEYCASE(REDKEY_F7);
        KEYCASE(REDKEY_F8);
        KEYCASE(REDKEY_F9);
        KEYCASE(REDKEY_F10);
        KEYCASE(REDKEY_NUM_LOCK);
        KEYCASE(REDKEY_SCROLL_LOCK);
        KEYCASE(REDKEY_PAD_7);
        KEYCASE(REDKEY_PAD_8);
        KEYCASE(REDKEY_PAD_9);
        KEYCASE(REDKEY_PAD_MINUS);
        KEYCASE(REDKEY_PAD_4);
        KEYCASE(REDKEY_PAD_5);
        KEYCASE(REDKEY_PAD_6);
        KEYCASE(REDKEY_PAD_PLUS);
        KEYCASE(REDKEY_PAD_1);
        KEYCASE(REDKEY_PAD_2);
        KEYCASE(REDKEY_PAD_3);
        KEYCASE(REDKEY_PAD_0);
        KEYCASE(REDKEY_PAD_POINT);
        KEYCASE(REDKEY_F11);
        KEYCASE(REDKEY_F12);
        KEYCASE(REDKEY_PAD_ENTER);
        KEYCASE(REDKEY_R_CTRL);
        KEYCASE(REDKEY_FAKE_L_SHIFT);
        KEYCASE(REDKEY_PAD_DIVIDE);
        KEYCASE(REDKEY_FAKE_R_SHIFT);
        KEYCASE(REDKEY_CTRL_PRINT_SCREEN);
        KEYCASE(REDKEY_R_ALT);
        KEYCASE(REDKEY_CTRL_BREAK);
        KEYCASE(REDKEY_HOME);
        KEYCASE(REDKEY_UP);
        KEYCASE(REDKEY_PAGEUP);
        KEYCASE(REDKEY_LEFT);
        KEYCASE(REDKEY_RIGHT);
        KEYCASE(REDKEY_END);
        KEYCASE(REDKEY_DOWN);
        KEYCASE(REDKEY_PAGEDOWN);
        KEYCASE(REDKEY_INSERT);
        KEYCASE(REDKEY_DELETE);
        KEYCASE(REDKEY_LEFT_CMD);
        KEYCASE(REDKEY_RIGHT_CMD);
        KEYCASE(REDKEY_MENU);
    default:
        DBG(0, "???");
    }
}

#endif

bool Application::press_key(RedKey key)
{
    if (_keyboard_state[key]) {
        return true;
    } else {
        _keyboard_state[key] = true;
        _num_keys_pressed++;
        return false;
    }
}

bool Application::unpress_key(RedKey key)
{
    ASSERT(!_sticky_info.key_down || !is_sticky_trace_key(key));

    if (_keyboard_state[key]) {
        _keyboard_state[key] = false;
        _num_keys_pressed--;
        ASSERT(_num_keys_pressed >= 0);
        return true;
    } else {
        return false;
    }
}

inline bool Application::is_sticky_trace_key(RedKey key)
{
    return ((key == REDKEY_L_ALT) || (key == REDKEY_R_ALT));
}

void Application::reset_sticky()
{
    _sticky_info.trace_is_on = (_state == VISIBILITY) && _sys_key_intercept_mode;
    _sticky_info.key_first_down = false;
    deactivate_interval_timer(*_sticky_info.timer);
    if (_sticky_info.sticky_mode) {
        ASSERT(_keyboard_state[_sticky_info.key]);
        // if it is physically down, we shouldn't unpress it
        if (!_sticky_info.key_down) {
            do_on_key_up(_sticky_info.key);
        }
        _sticky_info.sticky_mode = false;
        DBG(0, "OFF sticky");
        _info_layer->set_sticky(false);
    }
    _sticky_info.key_down = false;

    _sticky_info.key = REDKEY_INVALID;
}

struct ModifierKey {
    int modifier;
    RedKey key;
};

ModifierKey modifier_keys[] = {
    {Platform::L_SHIFT_MODIFIER, REDKEY_L_SHIFT},
    {Platform::R_SHIFT_MODIFIER, REDKEY_R_SHIFT},
    {Platform::L_CTRL_MODIFIER, REDKEY_L_CTRL},
    {Platform::R_CTRL_MODIFIER, REDKEY_R_CTRL},
    {Platform::L_ALT_MODIFIER, REDKEY_L_ALT},
    {Platform::R_ALT_MODIFIER, REDKEY_R_ALT},
};

void Application::sync_keyboard_modifiers()
{
    uint32_t modifiers = Platform::get_keyboard_modifiers();
    for (size_t i = 0; i < sizeof(modifier_keys) / sizeof(modifier_keys[0]); i++) {
        if (modifiers & modifier_keys[i].modifier) {
            on_key_down(modifier_keys[i].key);
        } else {
            on_key_up(modifier_keys[i].key);
        }
    }
}

void Application::on_key_down(RedKey key)
{
    if (key <= 0 || key >= REDKEY_NUM_KEYS) {
        return;
    }

    bool was_pressed = press_key(key);
    if (_sticky_info.trace_is_on) {
        if (key == _sticky_info.key) {
            _sticky_info.key_down = true;
        }

        if (!_sticky_info.sticky_mode) {
            // during tracing (traced key was pressed and no keyboard event has occurred till now)
            if (_sticky_info.key_first_down) {
                ASSERT(_sticky_info.key != REDKEY_INVALID);
                if (key != _sticky_info.key) {
                    reset_sticky();
                }
            } else if (is_sticky_trace_key(key) && (_num_keys_pressed == 1) && !was_pressed) {
                ASSERT(_sticky_info.key == REDKEY_INVALID);
                // start tracing
                _sticky_info.key =  key;
                _sticky_info.key_first_down = true;
                _sticky_info.key_down = true;
                activate_interval_timer(*_sticky_info.timer, STICKY_KEY_TIMEOUT);
            }
        }
    }

    if (!_sticky_info.sticky_mode) {
        int command = get_hotkeys_command();
        if (command != APP_CMD_INVALID) {
            do_command(command);
            return;
        }
    }

#ifdef WIN32
    if (!_active_screen->intercepts_sys_key() &&
                                           (key == REDKEY_LEFT_CMD || key == REDKEY_RIGHT_CMD ||
                                            key == REDKEY_MENU || _keyboard_state[REDKEY_L_ALT])) {
        unpress_key(key);
        return;
    }
    if (!_sticky_info.sticky_mode &&
        ((_keyboard_state[REDKEY_L_CTRL] || _keyboard_state[REDKEY_R_CTRL]) &&
        (_keyboard_state[REDKEY_L_ALT] || _keyboard_state[REDKEY_R_ALT]))) {
        if (key == REDKEY_END || key == REDKEY_PAD_1) {
            unpress_key(key);
            _key_handler->on_key_down(REDKEY_DELETE);
            _key_handler->on_key_up(REDKEY_DELETE);
        } else if (key == REDKEY_DELETE || key == REDKEY_PAD_POINT) {
            unpress_key(key);
            return;
        }
    }
#endif

    _key_handler->on_key_down(key);
}

void Application::do_on_key_up(RedKey key)
{
    unpress_key(key);
    _key_handler->on_key_up(key);
}

void Application::on_key_up(RedKey key)
{
    if(key < 0 || key >= REDKEY_NUM_KEYS || !_keyboard_state[key]) {
        return;
    }

    if (_sticky_info.trace_is_on) {
        ASSERT(_sticky_info.sticky_mode || (key == _sticky_info.key) ||
               (_sticky_info.key == REDKEY_INVALID));
        if (key == _sticky_info.key) {
            _sticky_info.key_down = false;
            if (_sticky_info.key_first_down) {
                _sticky_info.key_first_down = false;
                if (!_sticky_info.sticky_mode) {
                    reset_sticky();
                } else {
                    return; // ignore the sticky-key first release
                }
            }
        }

        if (_sticky_info.sticky_mode) {
            RedKey old_sticky_key = _sticky_info.key;
            reset_sticky();
            if (key == old_sticky_key) {
                return; // no need to send key_up twice
            }
        }
    }

    do_on_key_up(key);
}

void Application::on_char(uint32_t ch)
{
    _key_handler->on_char(ch);
}

void Application::on_deactivate_screen(RedScreen* screen)
{
    if (_active_screen == screen) {
        _sys_key_intercept_mode = false;
        release_capture();
        _active_screen = NULL;
    }
}

void Application::on_activate_screen(RedScreen* screen)
{
    ASSERT(!_active_screen || (_active_screen == screen));
    _active_screen = screen;
    sync_keyboard_modifiers();
}

void Application::on_start_screen_key_interception(RedScreen* screen)
{
    ASSERT(screen == _active_screen);

    _sys_key_intercept_mode = true;
    reset_sticky();
}

void Application::on_stop_screen_key_interception(RedScreen* screen)
{
    ASSERT(screen == _active_screen);

    _sys_key_intercept_mode = false;
    reset_sticky();
}

void Application::on_app_activated()
{
    _active = true;
    _key_handler->on_focus_in();
    if (*_foreign_menu) {
        (*_foreign_menu)->on_activate();
    }
}

void Application::on_app_deactivated()
{
    _active = false;
    _key_handler->on_focus_out();
    if (*_foreign_menu) {
        (*_foreign_menu)->on_deactivate();
    }
#ifdef WIN32
    if (!_changing_screens) {
        exit_full_screen();
    }
#endif
}

void Application::on_screen_unlocked(RedScreen& screen)
{
    if (_full_screen) {
        return;
    }

    screen.resize(SCREEN_INIT_WIDTH, SCREEN_INIT_HEIGHT);
}

void Application::rearrange_monitors(bool force_capture,
                                     bool enter_full_screen,
                                     RedScreen* screen,
                                     std::vector<SpicePoint> *sizes)
{
    bool capture;
    bool toggle_full_screen;

    if (!_full_screen && !enter_full_screen) {
        return;
    }

    toggle_full_screen = enter_full_screen && !screen;
    capture = release_capture();
#ifndef WIN32
    if (toggle_full_screen) {
        /* performing hide during resolution changes resulted in
           missing WM_KEYUP events */
        hide();
    }
#endif
    prepare_monitors(sizes);
    position_screens();
    if (enter_full_screen) {
        // toggling to full screen
        if (toggle_full_screen) {
            show_full_screen();
            _main_screen->activate();

        } else { // already in full screen mode and a new screen is displayed
            screen->show_full_screen();
            if (screen->is_out_of_sync()) {
                _out_of_sync = true;
                /* If the client monitor cannot handle the guest resolution
                    drop back to windowed mode */
                exit_full_screen();
            }
        }
    }

    if (force_capture || capture) {
        if (!toggle_full_screen) {
            _main_screen->activate();
        }
        _main_screen->capture_mouse();
    }
}

Monitor* Application::find_monitor(int id)
{
    ASSERT(_monitors);
    std::list<Monitor*>::const_iterator iter = _monitors->begin();
    for (; iter != _monitors->end(); iter++) {
        Monitor *mon = *iter;
        if (mon->get_id() == id) {
            return mon;
        }
    }
    return NULL;
}

Monitor* Application::get_monitor(int id)
{
    Monitor *mon = find_monitor(id);
    if ((mon = find_monitor(id))) {
        mon->set_used();
    }
    return mon;
}

void Application::assign_monitors()
{
    for (int i = 0; i < (int)_screens.size(); i++) {
        if (_screens[i]) {
            ASSERT(!_screens[i]->has_monitor());
            _screens[i]->set_monitor(get_monitor(i));
        }
    }
}

void Application::prepare_monitors(std::vector<SpicePoint> *sizes)
{
    //todo: test match of monitors size/position against real world size/position
    for (int i = 0; i < (int)_screens.size(); i++) {
        Monitor* mon;
        if (_screens[i] && (mon = _screens[i]->get_monitor())) {
            if (_screens[i]->is_size_locked()) {
                if (sizes) {
                    mon->set_mode((*sizes)[i].x, (*sizes)[i].y);
                } else {
                    SpicePoint size = _screens[i]->get_size();
                    mon->set_mode(size.x, size.y);
                }
            } else {
                SpicePoint size = mon->get_size();
                _screens[i]->resize(size.x, size.y);
            }
        }
    }
}

void Application::restore_monitors()
{
    //todo: renew monitors (destroy + init)
    std::list<Monitor*>::const_iterator iter = _monitors->begin();
    for (; iter != _monitors->end(); iter++) {
        (*iter)->restore();
    }
}

void Application::position_screens()
{
    for (int i = 0; i < (int)_screens.size(); i++) {
        Monitor* mon;
        if (_screens[i] && (mon = _screens[i]->get_monitor())) {
            _screens[i]->position_full_screen(mon->get_position());
        }
    }
}

void Application::hide()
{
    for (int i = 0; i < (int)_screens.size(); i++) {
        if (_screens[i]) {
            _screens[i]->hide();
        }
    }
}

void Application::show()
{
    for (int i = 0; i < (int)_screens.size(); i++) {
        if (_screens[i]) {
            _screens[i]->show();
        }
    }
}

void Application::external_show()
{
    DBG(0, "Entry, _screens.size()=%lu", _screens.size());
    for (size_t i = 0; i < _screens.size(); ++i) {
        DBG(0, "%lu", i);
        if (_screens[i]) {
            _screens[i]->external_show();
        }
    }
}

void Application::show_full_screen()
{
    for (int i = 0; i < (int)_screens.size(); i++) {
        if (_screens[i]) {
            _screens[i]->show_full_screen();
            if (_screens[i]->is_out_of_sync()) {
                _out_of_sync = true;
            }
        }
    }
}

void Application::enter_full_screen()
{
    LOG_INFO("");
    _changing_screens = true;
    assign_monitors();
    rearrange_monitors(false, true);
    _changing_screens = false;
    _full_screen = true;
    /* If the client monitor cannot handle the guest resolution drop back
       to windowed mode */
    if (_out_of_sync) {
        exit_full_screen();
    }
}

void Application::restore_screens_size()
{
    for (int i = 0; i < (int)_screens.size(); i++) {
        if (!_screens[i] || _screens[i]->is_size_locked()) {
            continue;
        }
        _screens[i]->resize(SCREEN_INIT_WIDTH, SCREEN_INIT_HEIGHT);
    }
}

void Application::exit_full_screen()
{
    if (!_full_screen) {
        return;
    }
    if (_out_of_sync) {
        LOG_WARN("Falling back to windowed mode (guest resolution too large for client?)");
    }
    LOG_INFO("");
    _changing_screens = true;
    release_capture();
    restore_monitors();
    for (int i = 0; i < (int)_screens.size(); i++) {
        if (_screens[i]) {
            Monitor* mon;
            _screens[i]->exit_full_screen();
            if ((mon = _screens[i]->get_monitor())) {
                _screens[i]->set_monitor(NULL);
                mon->set_free();
            }
        }
    }
    _full_screen = false;
    _out_of_sync = false;
    restore_screens_size();
    show();
    _main_screen->activate();
    _changing_screens = false;
}

bool Application::toggle_full_screen()
{
    if (_full_screen) {
        exit_full_screen();
    } else {
        enter_full_screen();
    }
    return _full_screen;
}

void Application::resize_screen(RedScreen *screen, int width, int height)
{
    std::vector<SpicePoint> sizes;
    std::vector<SpicePoint> *p_sizes = NULL;
    bool capture = false;

    if (_full_screen) {
        capture = (_main_screen == screen) && _active_screen &&
                                              _active_screen->is_mouse_captured();
        sizes.resize(_screens.size());
        for (int i = 0; i < (int)_screens.size(); i++) {
            if (_screens[i]) {
                if (_screens[i] == screen) {
                    sizes[i].x = width;
                    sizes[i].y = height;
                } else {
                    sizes[i] = _screens[i]->get_size();
                }
            }
        }
        p_sizes = &sizes;
    }
    rearrange_monitors(false, false, NULL, p_sizes);
    screen->resize(width, height);
    if (capture) {
        screen->capture_mouse();
    }
    if (screen->is_out_of_sync()) {
        _out_of_sync = true;
        /* If the client monitor cannot handle the guest resolution
           drop back to windowed mode */
        exit_full_screen();
    }
}

void Application::minimize()
{
    for (int i = 0; i < (int)_screens.size(); i++) {
        if (_screens[i]) {
            _screens[i]->minimize();
        }
    }
}

void Application::destroy_monitors()
{
    for (int i = 0; i < (int)_screens.size(); i++) {
        if (_screens[i]) {
            _screens[i]->set_monitor(NULL);
        }
    }
    Platform::destroy_monitors();
    _monitors = NULL;
}

void Application::init_monitors()
{
    exit_full_screen();
    destroy_monitors();
    _monitors = &Platform::init_monitors();
}

void Application::on_monitors_change()
{
    if (Monitor::is_self_change()) {
        return;
    }
    exit_full_screen();
    init_monitors();
}

void Application::on_display_mode_change()
{
    _client.on_display_mode_change();
}

uint32_t Application::get_mouse_mode()
{
    return _client.get_mouse_mode();
}

void Application::set_title(const std::string& title)
{
    _title = title;

    for (size_t i = 0; i < _screens.size(); ++i) {
        if (_screens[i]) {
            _screens[i]->set_name(_title);
        }
    }
}

#ifdef USE_SMARTCARD
void Application::enable_smartcard(bool enable)
{
    _smartcard_options->enable = enable;
}
#endif

bool Application::is_key_set_pressed(const HotkeySet& key_set)
{
    HotkeySet::const_iterator iter = key_set.begin();

    while (iter != key_set.end()) {
        if (!(_keyboard_state[iter->main] || _keyboard_state[iter->alter])) {
            break;
        }
        ++iter;
    }

    return iter == key_set.end();
}

int Application::get_hotkeys_command()
{
    HotKeys::const_iterator iter = _hot_keys.begin();

    while (iter != _hot_keys.end()) {
        if (is_key_set_pressed(iter->second)) {
            break;
        }
        ++iter;
    }

    return (iter != _hot_keys.end()) ? iter->first : APP_CMD_INVALID;
}

void Application::send_key_down(RedKey key)
{
    _key_handler->on_key_down(key);
}

void Application::send_key_up(RedKey key)
{
    _key_handler->on_key_up(key);
}

void Application::send_alt_ctl_del()
{
    send_key_down(REDKEY_L_CTRL);
    send_key_down(REDKEY_L_ALT);
    send_key_down(REDKEY_DELETE);

    send_key_up(REDKEY_DELETE);
    send_key_up(REDKEY_L_ALT);
    send_key_up(REDKEY_L_CTRL);
}

void Application::send_ctrl_alt_end()
{
    send_key_down(REDKEY_L_CTRL);
    send_key_down(REDKEY_L_ALT);
    send_key_down(REDKEY_END);

    send_key_up(REDKEY_L_CTRL);
    send_key_up(REDKEY_L_ALT);
    send_key_up(REDKEY_END);
}

void Application::send_command_hotkey(int action)
{
    HotKeys::const_iterator iter = _hot_keys.find(action);
    if (iter != _hot_keys.end()) {
        send_hotkey_key_set(iter->second);
    }
}

void Application::send_hotkey_key_set(const HotkeySet& key_set)
{
    HotkeySet::const_iterator iter;

    for (iter = key_set.begin(); iter != key_set.end(); ++iter) {
        send_key_down(iter->main);
    }

    for (iter = key_set.begin(); iter != key_set.end(); ++iter) {
        send_key_up(iter->main);
    }
}

int Application::get_menu_item_id(AppMenuItemType type, int32_t conn_ref, uint32_t ext_id)
{
    int free_id = APP_CMD_EXTERNAL_BEGIN;
    AppMenuItem item = {type, conn_ref, ext_id};
    AppMenuItemMap::iterator iter = _app_menu_items.begin();
    for (; iter != _app_menu_items.end(); iter++) {
        if (!memcmp(&(*iter).second, &item, sizeof(item))) {
            return (*iter).first;
        } else if (free_id == (*iter).first && ++free_id > APP_CMD_EXTERNAL_END) {
            return APP_CMD_INVALID;
        }
    }
    _app_menu_items[free_id] = item;
    return free_id;
}

void Application::clear_menu_items(int32_t opaque_conn_ref)
{
    AppMenuItemMap::iterator iter = _app_menu_items.begin();
    AppMenuItemMap::iterator curr;

    while (iter != _app_menu_items.end()) {
        curr = iter++;
        if (((*curr).second).conn_ref == opaque_conn_ref) {
            _app_menu_items.erase(curr);
        }
    }
}

void Application::remove_menu_item(int item_id)
{
    _app_menu_items.erase(item_id);
}

int Application::get_foreign_menu_item_id(int32_t opaque_conn_ref, uint32_t msg_id)
{
    return get_menu_item_id(APP_MENU_ITEM_TYPE_FOREIGN, opaque_conn_ref, msg_id);
}

void Application::update_menu()
{
    for (size_t i = 0; i < _screens.size(); ++i) {
        if (_screens[i]) {
            _screens[i]->update_menu();
        }
    }
}

//controller interface begin

void Application::set_auto_display_res(bool auto_display_res)
{
   _client.set_auto_display_res(auto_display_res);
}

bool Application::connect(const std::string& host, int port, int sport, const std::string& password)
{
    if (_state != DISCONNECTED) {
        return false;
    }
    _client.set_target(host, port, sport);
    _client.set_password(password);
    if (!set_channels_security(port, sport)) {
        return false;
    }
    register_channels();
    connect();
    return true;
}

void Application::disconnect()
{
    do_disconnect();
}

void Application::quit()
{
    ProcessLoop::quit(SPICEC_ERROR_CODE_SUCCESS);
}

void Application::show_me(bool full_screen)
{
    if (full_screen) {
        enter_full_screen();
    } else {
        _main_screen->show(true, NULL);
    }
}

void Application::hide_me()
{
    if (_full_screen) {
        exit_full_screen();
    }
    hide();
}

void Application::set_default_hotkeys(void)
{
    const std::string default_hotkeys = "toggle-fullscreen=shift+f11"
                                        ",release-cursor=shift+f12"
#ifdef RED_DEBUG
                                        ",connect=shift+f5"
                                        ",disconnect=shift+f6"
#endif
#ifdef USE_GUI
                                        ",show-gui=shift+f7"
#endif // USE_GUI
#ifdef USE_SMARTCARD
                                        ",smartcard-insert=shift+f8"
                                        ",smartcard-remove=shift+f9"
#endif
                                        "";

    this->set_hotkeys(default_hotkeys);
}

void Application::set_hotkeys(const std::string& hotkeys)
{

    try {
        std::auto_ptr<HotKeysParser> parser(new HotKeysParser(hotkeys, _commands_map));
        _hot_keys = parser->get();
    } catch (Exception &e) {
        LOG_WARN("%s", e.what());
        this->set_default_hotkeys();
    }
}

int Application::get_controller_menu_item_id(int32_t opaque_conn_ref, uint32_t msg_id)
{
    return get_menu_item_id(APP_MENU_ITEM_TYPE_CONTROLLER, opaque_conn_ref, msg_id);
}

void Application::set_menu(Menu* menu)
{
    if (menu) {
        _app_menu.reset(menu->ref());
    } else {
        init_menu();
    }
    if (*_foreign_menu) {
        (*_foreign_menu)->add_sub_menus();
    }
    update_menu();
}

void Application::delete_menu()
{
    set_menu(NULL);
}

#ifdef USE_GUI
bool Application::is_disconnect_allowed()
{
    return _gui_mode == GUI_MODE_FULL;
}
#endif // USE_GUI

const std::string& Application::get_host()
{
    return _client.get_host();
}

int Application::get_port()
{
    return _client.get_port();
}

int Application::get_sport()
{
    return _client.get_sport();
}

const std::string& Application::get_password()
{
    return _client.get_password();
}

//controller interface end

bool Application::set_channels_security(CmdLineParser& parser, bool on, char *val,
                                        const char* arg0)
{
    RedPeer::ConnectionOptions::Type option;
    option = (on) ? RedPeer::ConnectionOptions::CON_OP_SECURE :
                    RedPeer::ConnectionOptions::CON_OP_UNSECURE;

    typedef std::map< std::string, int> ChannelsNamesMap;
    ChannelsNamesMap channels_names;
    channels_names["main"] = SPICE_CHANNEL_MAIN;
    channels_names["display"] = SPICE_CHANNEL_DISPLAY;
    channels_names["inputs"] = SPICE_CHANNEL_INPUTS;
    channels_names["cursor"] = SPICE_CHANNEL_CURSOR;
    channels_names["playback"] = SPICE_CHANNEL_PLAYBACK;
    channels_names["record"] = SPICE_CHANNEL_RECORD;
#ifdef USE_TUNNEL
    channels_names["tunnel"] = SPICE_CHANNEL_TUNNEL;
#endif
#ifdef USE_SMARTCARD
    channels_names["smartcard"] = SPICE_CHANNEL_SMARTCARD;
#endif

    if (!strcmp(val, "all")) {
        if ((val = parser.next_argument())) {
            Platform::term_printf("%s: \"all\" is exclusive in secure-channels\n", arg0);
            _exit_code = SPICEC_ERROR_CODE_INVALID_ARG;
            return false;
        }
        PeerConnectionOptMap::iterator iter = _peer_con_opt.begin();
        for (; iter != _peer_con_opt.end(); iter++) {
            (*iter).second = option;
        }
        return true;
    }

    do {
        ChannelsNamesMap::iterator iter = channels_names.find(val);
        if (iter == channels_names.end()) {
            Platform::term_printf("%s: bad channel name \"%s\" in secure-channels\n", arg0, val);
            _exit_code = SPICEC_ERROR_CODE_INVALID_ARG;
            return false;
        }
        _peer_con_opt[(*iter).second] = option;
    } while ((val = parser.next_argument()));
    return true;
}

bool Application::set_channels_security(int port, int sport)
{
    PeerConnectionOptMap::iterator iter = _peer_con_opt.begin();

    for (; iter != _peer_con_opt.end(); iter++) {
        if ((*iter).second == RedPeer::ConnectionOptions::CON_OP_SECURE) {
            continue;
        }

        if ((*iter).second == RedPeer::ConnectionOptions::CON_OP_UNSECURE) {
            continue;
        }

        if (port != -1 && sport != -1) {
            (*iter).second = RedPeer::ConnectionOptions::CON_OP_BOTH;
            continue;
        }

        if (port != -1) {
            (*iter).second = RedPeer::ConnectionOptions::CON_OP_UNSECURE;
            continue;
        }

        if (sport != -1) {
            (*iter).second = RedPeer::ConnectionOptions::CON_OP_SECURE;
            continue;
        }

        _exit_code = SPICEC_ERROR_CODE_CMD_LINE_ERROR;
        return false;
    }
    return true;
}

bool Application::set_connection_ciphers(const char* ciphers, const char* arg0)
{
    _con_ciphers = ciphers;
    return true;
}

bool Application::set_ca_file(const char* ca_file, const char* arg0)
{
    _host_auth_opt.CA_file = ca_file;
    return true;
}

bool Application::set_host_cert_subject(const char* subject, const char* arg0)
{
    std::string subject_str(subject);
    std::string::const_iterator iter = subject_str.begin();
    std::string entry;
    _host_auth_opt.type_flags = SPICE_SSL_VERIFY_OP_SUBJECT;
    _host_auth_opt.host_subject = subject;

    /* the follow is only checking code, subject is parsed later
       ssl_verify.c.  We keep simply because of better error message... */
    while (true) {
        if ((iter == subject_str.end()) || (*iter == ',')) {
            RedPeer::HostAuthOptions::CertFieldValuePair entry_pair;
            size_t value_pos = entry.find_first_of('=');
            if ((value_pos == std::string::npos) || (value_pos == (entry.length() - 1))) {
                Platform::term_printf("%s: host_subject bad format: assignment for %s is missing\n",
                                      arg0, entry.c_str());
                _exit_code = SPICEC_ERROR_CODE_INVALID_ARG;
                return false;
            }
            size_t start_pos = entry.find_first_not_of(' ');
            if ((start_pos == std::string::npos) || (start_pos == value_pos)) {
                Platform::term_printf("%s: host_subject bad format: first part of assignment must be non empty in %s\n",
                                      arg0, entry.c_str());
                _exit_code = SPICEC_ERROR_CODE_INVALID_ARG;
                return false;
            }
            entry_pair.first = entry.substr(start_pos, value_pos - start_pos);
            entry_pair.second = entry.substr(value_pos + 1);
            DBG(0, "subject entry: %s=%s", entry_pair.first.c_str(), entry_pair.second.c_str());
            if (iter == subject_str.end()) {
                break;
            }
            entry.clear();
        } else if (*iter == '\\') {
            iter++;
            if (iter == subject_str.end()) {
                LOG_WARN("single \\ in host subject");
                entry.append(1, '\\');
                continue;
            } else if ((*iter == '\\') || (*iter == ',')) {
                entry.append(1, *iter);
            } else {
                LOG_WARN("single \\ in host subject");
                entry.append(1, '\\');
                continue;
            }
        } else {
            entry.append(1, *iter);
        }
        iter++;
    }

    return true;
}

bool Application::set_canvas_option(CmdLineParser& parser, char *val, const char* arg0)
{
    typedef std::map< std::string, CanvasOption> CanvasNamesMap;
    CanvasNamesMap canvas_types;

    canvas_types["sw"] = CANVAS_OPTION_SW;
#ifdef WIN32
    canvas_types["gdi"] = CANVAS_OPTION_GDI;
#endif
#ifdef USE_OPENGL
    canvas_types["gl_fbo"] = CANVAS_OPTION_OGL_FBO;
    canvas_types["gl_pbuff"] = CANVAS_OPTION_OGL_PBUFF;
#endif
    _canvas_types.clear();

    do {
        CanvasNamesMap::iterator iter = canvas_types.find(val);
        if (iter == canvas_types.end()) {
            Platform::term_printf("%s: bad canvas type \"%s\"\n", arg0, val);
            _exit_code = SPICEC_ERROR_CODE_INVALID_ARG;
            return false;
        }
        _canvas_types.resize(_canvas_types.size() + 1);
        _canvas_types[_canvas_types.size() - 1] = (*iter).second;
    } while ((val = parser.next_argument()));

    return true;
}

bool Application::set_enable_channels(CmdLineParser& parser, bool enable, char *val,
                                      const char* arg0)
{
    typedef std::map< std::string, int> ChannelsNamesMap;
    ChannelsNamesMap channels_names;
    channels_names["display"] = SPICE_CHANNEL_DISPLAY;
    channels_names["inputs"] = SPICE_CHANNEL_INPUTS;
    channels_names["cursor"] = SPICE_CHANNEL_CURSOR;
    channels_names["playback"] = SPICE_CHANNEL_PLAYBACK;
    channels_names["record"] = SPICE_CHANNEL_RECORD;
#ifdef USE_TUNNEL
    channels_names["tunnel"] = SPICE_CHANNEL_TUNNEL;
#endif
#ifdef USE_SMARTCARD
    channels_names["smartcard"] = SPICE_CHANNEL_SMARTCARD;
#endif

    if (!strcmp(val, "all")) {
        if ((val = parser.next_argument())) {
            Platform::term_printf("%s: \"all\" is exclusive\n", arg0);
            _exit_code = SPICEC_ERROR_CODE_INVALID_ARG;
            return false;
        }
        for (unsigned int i = 0; i < _enabled_channels.size(); i++) {
            _enabled_channels[i] = enable;
        }
        return true;
    }

    do {
        ChannelsNamesMap::iterator iter = channels_names.find(val);
        if (iter == channels_names.end()) {
            Platform::term_printf("%s: bad channel name \"%s\"\n", arg0, val);
            _exit_code = SPICEC_ERROR_CODE_INVALID_ARG;
            return false;
        }
        _enabled_channels[(*iter).second] = enable;
    } while ((val = parser.next_argument()));
    return true;
}

bool Application::set_disabled_display_effects(CmdLineParser& parser, char *val, const char* arg0,
                                               DisplaySetting& disp_setting)
{
    if (!strcmp(val, "all")) {
        if ((val = parser.next_argument())) {
            Platform::term_printf("%s: \"all\" is exclusive\n", arg0);
            _exit_code = SPICEC_ERROR_CODE_INVALID_ARG;
            return false;
        }
        disp_setting._disable_wallpaper = true;
        disp_setting._disable_font_smooth = true;
        disp_setting._disable_animation = true;
        return true;
    }

    do {
        if (!strcmp(val, "wallpaper")) {
            disp_setting._disable_wallpaper = true;
        } else if (!strcmp(val, "font-smooth")) {
            disp_setting._disable_font_smooth = true;
        } else if (!strcmp(val, "animation")) {
            disp_setting._disable_animation = true;
        } else {
            Platform::term_printf("%s: bad display effect type \"%s\"\n", arg0, val);
            _exit_code = SPICEC_ERROR_CODE_INVALID_ARG;
            return false;
        }
    } while ((val = parser.next_argument()));

    return true;
}

void Application::on_cmd_line_invalid_arg(const char* arg0, const char* what, const char* val)
{
    Platform::term_printf("%s: invalid %s value %s\n", arg0, what, val);
    _exit_code = SPICEC_ERROR_CODE_INVALID_ARG;
}

void Application::register_channels()
{
    if (_enabled_channels[SPICE_CHANNEL_DISPLAY]) {
        _client.register_channel_factory(DisplayChannel::Factory());
    }

    if (_enabled_channels[SPICE_CHANNEL_CURSOR]) {
        _client.register_channel_factory(CursorChannel::Factory());
    }

    if (_enabled_channels[SPICE_CHANNEL_INPUTS]) {
        _client.register_channel_factory(InputsChannel::Factory());
    }

    if (_enabled_channels[SPICE_CHANNEL_PLAYBACK]) {
        _client.register_channel_factory(PlaybackChannel::Factory());
    }

    if (_enabled_channels[SPICE_CHANNEL_RECORD]) {
        _client.register_channel_factory(RecordChannel::Factory());
    }

#ifdef USE_TUNNEL
    if (_enabled_channels[SPICE_CHANNEL_TUNNEL]) {
        _client.register_channel_factory(TunnelChannel::Factory());
    }
#endif
#ifdef USE_SMARTCARD
    if (_enabled_channels[SPICE_CHANNEL_SMARTCARD] && _smartcard_options->enable) {
        smartcard_init(_smartcard_options); // throws Exception
        _client.register_channel_factory(SmartCardChannel::Factory());
    }
#endif
}

bool Application::process_cmd_line(int argc, char** argv, bool &full_screen)
{
    std::string host = "";
    int sport = -1;
    int port = -1;
    bool auto_display_res = false;
    std::string password;
    DisplaySetting display_setting;

    enum {
        SPICE_OPT_VERSION = CmdLineParser::OPTION_FIRST_AVAILABLE,
        SPICE_OPT_HOST,
        SPICE_OPT_PORT,
        SPICE_OPT_SPORT,
        SPICE_OPT_PASSWORD,
        SPICE_OPT_FULL_SCREEN,
        SPICE_OPT_SECURE_CHANNELS,
        SPICE_OPT_UNSECURE_CHANNELS,
        SPICE_OPT_TLS_CIPHERS,
        SPICE_OPT_CA_FILE,
        SPICE_OPT_HOST_SUBJECT,
        SPICE_OPT_ENABLE_CHANNELS,
        SPICE_OPT_DISABLE_CHANNELS,
        SPICE_OPT_CANVAS_TYPE,
        SPICE_OPT_DISPLAY_COLOR_DEPTH,
        SPICE_OPT_DISABLE_DISPLAY_EFFECTS,
        SPICE_OPT_CONTROLLER,
        SPICE_OPT_TITLE,
        SPICE_OPT_HOTKEYS,
#ifdef USE_SMARTCARD
        SPICE_OPT_SMARTCARD,
        SPICE_OPT_NOSMARTCARD,
        SPICE_OPT_SMARTCARD_CERT,
        SPICE_OPT_SMARTCARD_DB,
#endif
    };

    full_screen = false;

#ifdef USE_GUI
    if (argc == 1) {
        _gui_mode = GUI_MODE_FULL;
        register_channels();
        return true;
    }
#endif // USE_GUI

#ifdef USE_GUI
    _gui_mode = GUI_MODE_ACTIVE_SESSION;
#endif // USE_GUI

    CmdLineParser parser("Spice client", false);

    parser.add(SPICE_OPT_VERSION, "version", "spice client version", false);
    parser.add(SPICE_OPT_HOST, "host", "spice server address", "host", true, 'h');
    parser.add(SPICE_OPT_PORT, "port", "spice server port", "port", true, 'p');
    parser.add(SPICE_OPT_SPORT, "secure-port", "spice server secure port", "port", true, 's');
    parser.add(SPICE_OPT_SECURE_CHANNELS, "secure-channels",
               "force secure connection on the specified channels", "channel",
               true);
    parser.set_multi(SPICE_OPT_SECURE_CHANNELS, ',');
    parser.add(SPICE_OPT_UNSECURE_CHANNELS, "unsecure-channels",
               "force unsecure connection on the specified channels", "channel",
               true);
    parser.set_multi(SPICE_OPT_UNSECURE_CHANNELS, ',');
    parser.add(SPICE_OPT_TLS_CIPHERS, "tls-ciphers", "ciphers for secure connections",
               "ciphers", true);
    parser.add(SPICE_OPT_CA_FILE, "ca-file", "truststore file for secure connections",
               "ca-file", true);
    parser.add(SPICE_OPT_HOST_SUBJECT, "host-subject",
               "subject of the host certificate. Format: field=value pairs separated"
               " by commas. Commas and backslashes within values must be preceded by"
               " a backslash", "host-subject", true);
    parser.add(SPICE_OPT_PASSWORD, "password", "server password", "password", true, 'w');
    parser.add(SPICE_OPT_FULL_SCREEN, "full-screen", "open in full screen mode", "auto-conf",
               false, 'f');

    parser.add(SPICE_OPT_ENABLE_CHANNELS, "enable-channels", "enable channels", "channel", true);
    parser.set_multi(SPICE_OPT_ENABLE_CHANNELS, ',');

    parser.add(SPICE_OPT_DISABLE_CHANNELS, "disable-channels", "disable channels", "channel", true);
    parser.set_multi(SPICE_OPT_DISABLE_CHANNELS, ',');

    parser.add(SPICE_OPT_CANVAS_TYPE, "canvas-type", "set rendering canvas", "canvas_type", true);
    parser.set_multi(SPICE_OPT_CANVAS_TYPE, ',');

    parser.add(SPICE_OPT_DISPLAY_COLOR_DEPTH, "color-depth",
               "guest display color depth (if supported by the guest vdagent)",
               "16/32", true);

    parser.add(SPICE_OPT_DISABLE_DISPLAY_EFFECTS, "disable-effects",
               "disable guest display effects " \
               "(if supported by the guest vdagent)",
               "wallpaper/font-smooth/animation/all", true);
    parser.set_multi(SPICE_OPT_DISABLE_DISPLAY_EFFECTS, ',');

    parser.add(SPICE_OPT_CONTROLLER, "controller", "enable external controller");

    parser.add(SPICE_OPT_TITLE, "title", "set window title", "title", true, 't');
    parser.add(SPICE_OPT_HOTKEYS, "hotkeys", "Set hotkey keybindings",
               "toggle-fullscreen=shift+f11,release-cursor=shift+f12,etc.",
               true);

#ifdef USE_SMARTCARD
    parser.add(SPICE_OPT_SMARTCARD, "smartcard", "enable smartcard channel");
    parser.add(SPICE_OPT_NOSMARTCARD, "nosmartcard", "disable smartcard channel");
    parser.add(SPICE_OPT_SMARTCARD_CERT, "smartcard-cert",
               "Use virtual reader+card with given cert(s)",
               "smartcard-cert", true);
    parser.set_multi(SPICE_OPT_SMARTCARD_CERT, ',');
    parser.add(SPICE_OPT_SMARTCARD_DB, "smartcard-db", "Use given db for smartcard certs", "smartcard-db", true);
#endif

    for (int i = SPICE_CHANNEL_MAIN; i < SPICE_END_CHANNEL; i++) {
        _peer_con_opt[i] = RedPeer::ConnectionOptions::CON_OP_INVALID;
    }

    parser.begin(argc, argv);

    char* val;
    int op;
    while ((op = parser.get_option(&val)) != CmdLineParser::OPTION_DONE) {
        switch (op) {
        case SPICE_OPT_VERSION: {
            std::ostringstream os;
            os << argv[0] << " "<< PACKAGE_VERSION << std::endl;
            Platform::term_printf("%s", os.str().c_str());
            return false;
        }
        case SPICE_OPT_HOST:
            host = val;
            break;
        case SPICE_OPT_PORT: {
            if ((port = str_to_port(val)) == -1) {
                on_cmd_line_invalid_arg(argv[0], "port", val);
                return false;
            }
            break;
        }
        case SPICE_OPT_SPORT: {
            if ((sport = str_to_port(val)) == -1) {
                on_cmd_line_invalid_arg(argv[0], "secure port", val);
                return false;
            }
            break;
        }
        case SPICE_OPT_FULL_SCREEN:
            if (val) {
                if (strcmp(val, "auto-conf")) {
                    on_cmd_line_invalid_arg(argv[0], "full screen mode", val);
                    return false;
                }
                auto_display_res = true;
            }
            full_screen = true;
            break;
        case SPICE_OPT_PASSWORD:
            password = val;
            break;
        case SPICE_OPT_SECURE_CHANNELS:
            if (!set_channels_security(parser, true, val, argv[0])) {
                return false;
            }
            break;
        case SPICE_OPT_UNSECURE_CHANNELS:
            if (!set_channels_security(parser, false, val, argv[0])) {
                return false;
            }
            break;
        case SPICE_OPT_TLS_CIPHERS:
            if (!set_connection_ciphers(val, argv[0])) {
                return false;
            }
            break;
        case SPICE_OPT_CA_FILE:
            if (!set_ca_file(val, argv[0])) {
                return false;
            }
            break;
        case SPICE_OPT_HOST_SUBJECT:
            if (!set_host_cert_subject(val, argv[0])) {
                return false;
            }
            break;
        case SPICE_OPT_ENABLE_CHANNELS:
            if (!set_enable_channels(parser, true, val, argv[0])) {
                return false;
            }
            break;
        case SPICE_OPT_DISABLE_CHANNELS:
            if (!set_enable_channels(parser, false, val, argv[0])) {
                return false;
            }
            break;
        case SPICE_OPT_CANVAS_TYPE:
            if (!set_canvas_option(parser, val, argv[0])) {
                return false;
            }
            break;
        case SPICE_OPT_DISPLAY_COLOR_DEPTH:
            display_setting._set_color_depth = true;
            if (!strcmp(val, "16")) {
                display_setting._color_depth = 16;
            } else if (!strcmp(val, "32")) {
                display_setting._color_depth = 32;
            } else {
                 on_cmd_line_invalid_arg(argv[0], "color depth", val);
                 return false;
            }
            break;
        case SPICE_OPT_DISABLE_DISPLAY_EFFECTS:
            if (!set_disabled_display_effects(parser, val, argv[0], display_setting)) {
                return false;
            }
            break;
        case SPICE_OPT_CONTROLLER:
            if (argc > 2) {
                Platform::term_printf("%s: controller cannot be combined with other options\n",
                                      argv[0]);
                _exit_code = SPICEC_ERROR_CODE_INVALID_ARG;
                return false;
            }
            _enable_controller = true;
            return true;
        case SPICE_OPT_TITLE:
            set_title(val);
            break;
        case SPICE_OPT_HOTKEYS:
            set_hotkeys(val);
            break;
#ifdef USE_SMARTCARD
        case SPICE_OPT_SMARTCARD:
            _smartcard_options->enable= true;
            break;
        case SPICE_OPT_NOSMARTCARD:
            _smartcard_options->enable= false; // default
            break;
        case SPICE_OPT_SMARTCARD_CERT:
            do {
                _smartcard_options->certs.insert(
                    _smartcard_options->certs.end(), std::string(val));
            } while ((val=parser.next_argument()));
            break;
        case SPICE_OPT_SMARTCARD_DB:
            _smartcard_options->dbname = val;
            break;
#endif
        case CmdLineParser::OPTION_HELP:
            parser.show_help();
            return false;
        case CmdLineParser::OPTION_ERROR:
            _exit_code = SPICEC_ERROR_CODE_CMD_LINE_ERROR;
            return false;
        default:
            throw Exception("cmd line error", SPICEC_ERROR_CODE_CMD_LINE_ERROR);
        }
    }

    if (host.empty()) {
        Platform::term_printf("%s: missing --host\n", argv[0]);
        _exit_code = SPICEC_ERROR_CODE_CMD_LINE_ERROR;
        return false;
    }

    if (parser.is_set(SPICE_OPT_SECURE_CHANNELS) && !parser.is_set(SPICE_OPT_SPORT)) {
        Platform::term_printf("%s: missing --secure-port\n", argv[0]);
        _exit_code = SPICEC_ERROR_CODE_CMD_LINE_ERROR;
        return false;
    }

    if (!set_channels_security(port, sport)) {
        Platform::term_printf("%s: missing --port or --sport\n", argv[0]);
        return false;
    }
    register_channels();

    _client.set_target(host, port, sport);
    _client.set_password(password);
    _client.set_auto_display_res(auto_display_res);
    _client.set_display_setting(display_setting);

    return true;
}

#ifdef RED_DEBUG
static unsigned int log_level = LOG_DEBUG;
#else
static unsigned int log_level = LOG_INFO;
#endif

static FILE *log_file = NULL;

void spice_log_cleanup(void)
{
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

static inline std::string function_to_func_name(const std::string& f_name)
{
#ifdef __GNUC__
    std::string name(f_name);
    std::string::size_type end_pos = f_name.find('(');
    if (end_pos == std::string::npos) {
        return f_name;
    }
    std::string::size_type start = f_name.rfind(' ', end_pos);
    if (start == std::string::npos) {
        return name.substr(0, end_pos);
    }
    end_pos -= ++start;
    return name.substr(start, end_pos);
#else
    return f_name;
#endif
}

void spice_log(unsigned int type, const char *function, const char *format, ...)
{
    std::string formated_message;
    va_list ap;
    const char *type_as_char[] = { "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };
    const char *type_as_nice_char[] = { "Debug", "Info", "Warning", "Error", "Fatal error" };

    if (type < log_level) {
      return;
    }

    ASSERT(type <= LOG_FATAL);

    va_start(ap, format);
    string_vprintf(formated_message, format, ap);
    va_end(ap);

    if (type >= log_level && log_file != NULL) {
        fprintf(log_file,"%ld %s [%" PRIu64 ":%" PRIu64 "] %s: %s\n",
                (long)time(NULL), type_as_char[type],
                Platform::get_process_id(),
                Platform::get_thread_id(),
                function_to_func_name(function).c_str(),
                formated_message.c_str());
        fflush(log_file);
    }

    if (type >= LOG_WARN) {
        fprintf(stderr, "%s: %s\n", type_as_nice_char[type], formated_message.c_str());
    }
}

void Application::init_logger()
{
    std::string log_file_name;

    Platform::get_app_data_dir(log_file_name, app_name);
    Platform::path_append(log_file_name, "spicec.log");

    log_file = ::fopen(log_file_name.c_str(), "a");

    if (log_file == NULL) {
        fprintf(stderr, "Failed to open log file %s\n", log_file_name.c_str());
        return;
    }
}

void Application::init_globals()
{
    init_logger();
    srand((unsigned)time(NULL));

    SSL_library_init();
    SSL_load_error_strings();

    sw_canvas_init();
#ifdef USE_OPENGL
    gl_canvas_init();
#endif
    quic_init();
#ifdef WIN32
    gdi_canvas_init();
#endif
}

/* seperated from init_globals to allow --help to work
 * faster and not require X on linux. */
void Application::init_platform_globals()
{
    Platform::init();
    RedWindow::init();
}

void Application::init_remainder()
{
    Platform::set_process_loop(*this);
    init_monitors();
    init_menu();
    _main_screen = get_screen(0);

    Platform::set_event_listener(this);
    Platform::set_display_mode_listner(this);

#ifdef USE_GUI
    _gui.reset(new GUI(*this, DISCONNECTED));
    _gui_timer.reset(new GUITimer(*_gui.get()));
    activate_interval_timer(*_gui_timer, 1000 / 30);
#ifdef GUI_DEMO
    _gui_test_timer.reset(new TestTimer(*this));
    activate_interval_timer(*_gui_test_timer, 1000 * 30);
#endif
#endif // USE_GUI
}

void Application::cleanup_platform_globals()
{
    RedWindow::cleanup();
}

void Application::cleanup_globals()
{
}

int Application::main(int argc, char** argv, const char* version_str)
{
    int ret;
    bool full_screen;
    char *log_level_str;

    log_level_str = getenv("SPICEC_LOG_LEVEL");
    if (log_level_str) {
        log_level = atoi(log_level_str);
    }

    init_globals();
    LOG_INFO("starting %s", version_str);
    std::string command_line = argv[0];
    for (int i = 1 ; i < argc ; ++i) {
        command_line += " ";
        command_line += argv[i];
    }
    LOG_INFO("command line: %s", command_line.c_str());

    std::auto_ptr<Application> app(new Application());
    AutoAbort auto_abort(*app.get());
    if (app->process_cmd_line(argc, argv, full_screen)) {
        init_platform_globals();
        app->init_remainder();
        if (full_screen) {
            app->enter_full_screen();
        } else {
            app->_main_screen->show(true, NULL);
        }
        ret = app->run();
        cleanup_platform_globals();
    } else {
        ret = app->_exit_code;
    }
    cleanup_globals();
    return ret;
}
