/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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
#ifndef _H_GUI
#define _H_GUI

#include "softrenderer.h"
#include "screen_layer.h"
#include "inputs_handler.h"
#include "application.h"

class RedPixmapSw;

class GUI : public ScreenLayer, public KeyHandler {
public:
    class Dialog;
    class Tab;
    class TabFactory;

    typedef std::list<TabFactory*> TabFactorys;

    GUI(Application& app, Application::State state);
    virtual ~GUI();

    void set_screen(RedScreen* screen); //show and hide

    Application& get_application() { return _app;}
#ifdef USE_GUI
    CEGUI::System& gui_system() { return *_gui_system;}
#endif // USE_GUI

    void set_state(Application::State state);
    bool is_visible() { return !!_dialog;}
    bool prepare_dialog();
    bool is_disconnect_allowed() { return _app.is_disconnect_allowed();}

    virtual bool pointer_test(int x, int y) { return contains_point(x, y);}
    virtual void on_pointer_enter(int x, int y, unsigned int buttons_state);
    virtual void on_pointer_leave();
    virtual void on_pointer_motion(int x, int y, unsigned int buttons_state);
    virtual void on_mouse_button_press(int button, int buttons_state);
    virtual void on_mouse_button_release(int button, int buttons_state);

    virtual void on_key_down(RedKey key);
    virtual void on_key_up(RedKey key);
    virtual void on_char(uint32_t ch);
    virtual bool permit_focus_loss() { return false;}

    void idle();

    virtual void copy_pixels(const QRegion& dest_region, RedDrawable& dest_dc);
    virtual void on_size_changed();

    void register_tab_factory(TabFactory& factory);
    void unregister_tab_factory(TabFactory& factory);

    class BoxResponse {
    public:
        virtual ~BoxResponse() {}
        virtual void response(int response) = 0;
        virtual void aborted() = 0;
    };

    enum MessageType {
        QUESTION,
        INFO,
        WARNING,
        ERROR_MSG
    };

    struct ButtonInfo {
        int id;
        const char *text;
    };

    typedef std::vector<ButtonInfo> ButtonsList;
    bool message_box(MessageType type, const char *text, const ButtonsList& buttons,
                     BoxResponse* _response_handler);

private:
    TabFactorys& get_factoris() { return _tab_factorys;}

    void create_dialog();
    void detach();
    void update_layer_area();
    void init_cegui();
    void conditional_update();
    void set_dialog(Dialog* dialog);
    void dettach_dialog(Dialog* dialog);

private:
    Application& _app;
    Application::State _state;
    RedPixmapSw* _pixmap;
#ifdef USE_GUI
    CEGUI::SoftRenderer* _renderer;
    CEGUI::System* _gui_system;
#endif // USE_GUI
    Dialog* _dialog;
    uint64_t _prev_time;
    TabFactorys _tab_factorys;

    friend class Dialog;
};

class GUI::Tab {
public:
    virtual ~Tab() {}

#ifdef USE_GUI
    virtual CEGUI::Window& get_root_window() = 0;
#endif // USE_GUI
    virtual const std::string& get_name() = 0;
};

class GUI::TabFactory {
public:
    TabFactory() : _order (-1) {}
    TabFactory(int order) : _order (order) {}

    virtual ~TabFactory() {}
    virtual Tab* create_tab(bool connected, int width, int hight) = 0;
    int get_order() { return _order;}

private:
    int _order;
};

#endif
