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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

#include <limits.h>
#include <stdlib.h>

#include "gui.h"
#include "screen.h"
#include "utils.h"
#include "debug.h"
#include "red_pixmap_sw.h"
#include "resource_provider.h"

#include "CEGUISystem.h"
#include "CEGUIWindowManager.h"
#include "CEGUIWindow.h"
#include "CEGUIFontManager.h"
#include "CEGUIExceptions.h"
#include "CEGUIScheme.h"
#include "elements/CEGUIPushButton.h"
#include "elements/CEGUIEditbox.h"
#include "elements/CEGUITabControl.h"
#include "elements/CEGUIListbox.h"
#include "elements/CEGUIListboxTextItem.h"

#define MAIN_GUI_WIDTH 640
#define MAIN_GUI_HEIGHT 480
#define BUTTON_WIDTH 90
#define BUTTON_HEIGHT 22
#define GUI_SPACE 10
#define GUI_LABEL_WIDTH 65
#define GUI_LABEL_HEIGHT 22
#define GUI_PORT_WIDTH 50
#define GUI_TEXT_BOX_HEIGHT GUI_LABEL_HEIGHT

#define LOGIN_DIALOG_WIDTH 400
#define LOGIN_DIALOG_HEIGHT 300
#define LOGIN_DIALOG_V_START 150

#define CONNECTING_DIALOG_WIDTH 400
#define CONNECTING_DIALOG_HEIGHT 300

#define MESSAGE_BOX_WIDTH 380
#define MESSAGE_BOX_HEIGHT 150

#define CONNECT_OPTION_DIALOG_WIDTH LOGIN_DIALOG_WIDTH
#define CONNECT_OPTION_DIALOG_HEIGHT LOGIN_DIALOG_HEIGHT

static inline void set_win_pos(CEGUI::Window* win, int x, int y)
{
    win->setPosition(CEGUI::UVector2(cegui_absdim((float)x), cegui_absdim((float)y)));
}

static inline void set_win_size(CEGUI::Window* win, int width, int height)
{
    win->setSize(CEGUI::UVector2(cegui_absdim((float)width), cegui_absdim((float)height)));
}

static void add_bottom_button(CEGUI::Window* parent, const char *str,
                              CEGUI::SubscriberSlot subscriber,
                              int& position)
{
    CEGUI::Window* button = CEGUI::WindowManager::getSingleton().createWindow("TaharezLook/Button");
    int win_width = (int)parent->getWidth().asAbsolute(1);
    int win_height = (int)parent->getHeight().asAbsolute(1);
    int y_pos = win_height - BUTTON_HEIGHT - GUI_SPACE;

    position += BUTTON_WIDTH + GUI_SPACE;
    set_win_pos(button, win_width - position, y_pos);
    set_win_size(button, BUTTON_WIDTH, BUTTON_HEIGHT);
    button->setText(str);
    button->subscribeEvent(CEGUI::PushButton::EventClicked, subscriber);
    button->setInheritsAlpha(false);
    //button->setTooltipText("tool tip");
    parent->addChildWindow(button);
}

#ifdef GUI_DEMO

class SampleTabFactory: public GUI::TabFactory {
public:

    class MyListItem : public CEGUI::ListboxTextItem {
    public:
        MyListItem (const CEGUI::String& text)
            : CEGUI::ListboxTextItem(text)
        {
            setSelectionBrushImage("TaharezLook", "MultiListSelectionBrush");
        }
    };

    class SampleTab: public GUI::Tab {
    public:
        SampleTab(int id, int width, int height)
        {
            string_printf(_name, "SampleTab-%d", id);
            CEGUI::WindowManager& winMgr = CEGUI::WindowManager::getSingleton();
            _root_window = winMgr.createWindow("TaharezLook/StaticText");
            set_win_pos(_root_window, 0, 0);
            set_win_size(_root_window, width, height);
            _root_window->setText("Tab of SampleTabFactory");

            if (id != 1) {
                return;
            }

            _list = (CEGUI::Listbox*)winMgr.createWindow("TaharezLook/Listbox");
            set_win_pos(_list, 10, 10);
            set_win_size(_list, 200, 100);
            _list->setMultiselectEnabled(false);
            _list->addItem(new MyListItem("Item-1"));
            _list->addItem(new MyListItem("Item-2"));
            _list->addItem(new MyListItem("Item-3"));
            _list->addItem(new MyListItem("Item-4"));
            _list->addItem(new MyListItem("Item-5"));
            _list->addItem(new MyListItem("Item-6"));

            _list->subscribeEvent(CEGUI::Listbox::EventSelectionChanged,
                                  CEGUI::Event::Subscriber(&SampleTab::handle_list_selection,
                                                           this));

            _root_window->addChildWindow(_list);

            _label = winMgr.createWindow("TaharezLook/StaticText");
            set_win_pos(_label, 220, 10);
            set_win_size(_label, 200, 22);
            _root_window->addChildWindow(_label);

            _list->setItemSelectState((size_t)0, true);

        }

        virtual ~SampleTab()
        {
            CEGUI::WindowManager::getSingleton().destroyWindow(_root_window);
        }

        virtual CEGUI::Window& get_root_window()
        {
            return *_root_window;
        }

        virtual const std::string& get_name()
        {
            return _name;
        }

        bool handle_list_selection(const CEGUI::EventArgs& e)
        {
            DBG(0, "changed");
            CEGUI::ListboxItem* selection = _list->getFirstSelectedItem();
            if (selection) {
                _label->setText(selection->getText());
                return true;
            }

            if (_list->getItemCount()) {
                _list->setItemSelectState((size_t)_list->getItemCount() - 1, true);
            }

            return true;
         }

    private:
         std::string _name;
         CEGUI::Window* _root_window;
         CEGUI::Listbox* _list;
         CEGUI::Window* _label;
    };

    SampleTabFactory(int id)
        : GUI::TabFactory(id)
        , _id (id)
    {
    }

    GUI::Tab* create_tab(bool connected, int width, int height)
    {
        if (!connected && _id % 2 == 0) {
            return NULL;
        }
        return new SampleTab(_id, width, height);
    }

private:
    int _id;
};

#endif

class GUI::Dialog {
public:
    Dialog(GUI& gui, bool close_on_message_click = false)
        : _gui (gui)
        , _root (NULL)
        , _message_box (NULL)
        , _box_response (NULL)
        , _close_on_message_click (close_on_message_click)
    {
    }

    virtual ~Dialog()
    {
        if (gui_system().getGUISheet() == _root) {
            gui_system().setGUISheet(NULL);
        }

        CEGUI::WindowManager::getSingleton().destroyWindow(_root);
    }

    CEGUI::Window& root_window() { return *_root;}
    Application& application() { return _gui.get_application();}
    CEGUI::System& gui_system() { return _gui.gui_system();}

    void set_dialog(Dialog* dialog) { _gui.set_dialog(dialog);}
    void dettach() { _gui.dettach_dialog(this);}
    TabFactorys& get_factoris() { return _gui._tab_factorys;}

    bool message_box(MessageType type, const char *text, const ButtonsList& buttons,
                     BoxResponse* response_handler);
    void error_box(const char* error_message);

    void pre_destroy();

private:
    void handle_message_click(int id);
    void set_opaque(CEGUI::Window* win);

    void dim();
    void undim();

protected:
    GUI& _gui;
    CEGUI::Window* _root;

private:
    class BottonAction {
    public:
        BottonAction(Dialog& dialog, int id)
            : _dialog (dialog)
            , _id (id)
        {
        }

        bool operator () (const CEGUI::EventArgs& e)
        {
            _dialog.handle_message_click(_id);
            return true;
        }

    private:
        Dialog& _dialog;
        int _id;
    };

    CEGUI::Window* _message_box;
    BoxResponse* _box_response;

    class UndimInfo {
    public:
        UndimInfo(const CEGUI::String& name, float alpha, bool inherits)
            : _name (name)
            , _alpha (alpha)
            , _inherits (inherits)
        {
        }

        void restore()
        {
            try {
                CEGUI::Window* win = CEGUI::WindowManager::getSingleton().getWindow(_name);
                win->setAlpha(_alpha);
                win->setInheritsAlpha(_inherits);
            } catch (...) {
            }
        }

    private:
        CEGUI::String _name;
        float _alpha;
        bool _inherits;

    };

    std::list<UndimInfo*> _undim_info_list;
    bool _close_on_message_click;
};


void GUI::Dialog::set_opaque(CEGUI::Window* win)
{
    float alpha = win->getAlpha();
    bool inherits = win->inheritsAlpha();

    if (alpha != 1 || !inherits ) {
        _undim_info_list.push_back(new UndimInfo(win->getName(), alpha, inherits));
        win->setInheritsAlpha(true);
        win->setAlpha(1);
    }

    size_t child_count = win->getChildCount();
    for (size_t i = 0; i < child_count; ++i) {
        CEGUI::Window* child = win->getChildAtIdx(i);
        set_opaque(child);
    }
}

void GUI::Dialog::dim()
{
    set_opaque(_root);
    _root->setAlpha(0.5);
}

void GUI::Dialog::undim()
{
    while (!_undim_info_list.empty()) {
        UndimInfo* inf = _undim_info_list.front();
        _undim_info_list.pop_front();
        inf->restore();
        delete inf;
    }
    _root->setAlpha(1);
}

bool GUI::Dialog::message_box(MessageType type, const char *text, const ButtonsList& buttons,
                              BoxResponse* response_handler)
{
    if (_message_box) {
        return false;
    }

    try {
        CEGUI::WindowManager& winMgr = CEGUI::WindowManager::getSingleton();

        CEGUI::Window* wnd = _message_box = winMgr.createWindow("TaharezLook/StaticText");
        int x_pos = (MAIN_GUI_WIDTH - MESSAGE_BOX_WIDTH) / 2;
        int y_pos = (MAIN_GUI_HEIGHT - MESSAGE_BOX_HEIGHT) / 3;
        set_win_pos(wnd, x_pos, y_pos);
        set_win_size(wnd, MESSAGE_BOX_WIDTH, MESSAGE_BOX_HEIGHT);
        wnd->setModalState(true);
        wnd->setInheritsAlpha(false);
        dim();
        _root->addChildWindow(wnd);

        CEGUI::Window* text_wnd = winMgr.createWindow("TaharezLook/StaticText");
        set_win_pos(text_wnd, GUI_SPACE, GUI_SPACE);
        set_win_size(text_wnd, MESSAGE_BOX_WIDTH - 2 * GUI_SPACE,
                     MESSAGE_BOX_HEIGHT - 3 * GUI_SPACE - BUTTON_HEIGHT);
        text_wnd->setProperty("FrameEnabled", "false");
        text_wnd->setProperty("HorzFormatting", "WordWrapLeftAligned");
        text_wnd->setProperty("VertFormatting", "TopAligned");
        text_wnd->setText(text);
        //text_wnd->getTextRenderArea();
        wnd->addChildWindow(text_wnd);

        x_pos = 0;

        for (unsigned int i = 0; i < buttons.size(); i++) {
            add_bottom_button(wnd, buttons[i].text,
                              CEGUI::Event::Subscriber(BottonAction(*this, buttons[i].id)),
                              x_pos);
        }

        _box_response = response_handler;
    } catch (CEGUI::Exception& e) {
        LOG_ERROR("Exception: %s", e.getMessage().c_str());
        throw;
    }

    return true;
}

class BoxResponseEvent: public Event {
public:
    BoxResponseEvent(GUI::BoxResponse* box_response, int id)
        : _box_response (box_response)
        , _id (id)
    {
    }

    virtual void response(AbstractProcessLoop &events_loop)
    {
        _box_response->response(_id);
    }

private:
    GUI::BoxResponse* _box_response;
    int _id;
};

class BoxAbortEvent: public Event {
public:
    BoxAbortEvent(GUI::BoxResponse* box_response)
        : _box_response (box_response)
    {
    }

    virtual void response(AbstractProcessLoop &events_loop)
    {
        _box_response->aborted();
    }

private:
    GUI::BoxResponse* _box_response;
};

void GUI::Dialog::handle_message_click(int id)
{
    DBG(0, "");
    ASSERT(_message_box);
    _message_box->setModalState(false);
    _root->removeChildWindow(_message_box);
    CEGUI::Window *win = _message_box;
    _message_box = NULL;
    CEGUI::WindowManager::getSingleton().destroyWindow(win);
    undim();
    if (_box_response) {
        AutoRef<BoxResponseEvent> event(new BoxResponseEvent(_box_response, id));
        _box_response = NULL;
        application().push_event(*event);
    }

    if (_close_on_message_click) {
        application().hide_gui();
    }
}

void GUI::Dialog::pre_destroy()
 {
    if (_box_response) {
        AutoRef<BoxAbortEvent> event(new BoxAbortEvent(_box_response));
        _box_response = NULL;
        application().push_event(*event);
    }
}

void GUI::Dialog::error_box(const char* message)
{
    ASSERT(message && strlen(message) > 0);
    DBG(0, "%s", message);
    ButtonsList list(1);
    list[0].id = 0;
    list[0].text = res_get_string(STR_BUTTON_OK);
    message_box(INFO, message, list, NULL);
}

class TabDialog : public GUI::Dialog {
public:
    TabDialog(GUI& gui, bool connected);
    virtual ~TabDialog();

protected:
    CEGUI::Window* _main_win;

private:
    typedef std::list<GUI::Tab*> Tabs;
    Tabs _tabs;
};

class MessageDialog : public GUI::Dialog {
public:
    MessageDialog(GUI& gui);
    virtual ~MessageDialog() {}
};

class LoginDialog : public GUI::Dialog {
public:
    LoginDialog(GUI& guip);

    bool handle_connect(const CEGUI::EventArgs& e);
    bool handle_quit(const CEGUI::EventArgs& e);
    bool handle_options(const CEGUI::EventArgs& e);

private:
    static void set_port_text(CEGUI::Window* win, int port);

private:
    CEGUI::Window* _host_box;
    CEGUI::Window* _port_box;
    CEGUI::Window* _sport_box;
    CEGUI::Window* _pass_box;
};

class PreLoginDialog: public TabDialog {
public:
    PreLoginDialog(GUI& gui, LoginDialog* login_dialog);
    virtual ~PreLoginDialog() { delete _login_dialog;}

    bool handle_back(const CEGUI::EventArgs& e);
    bool handle_quit(const CEGUI::EventArgs& e);

private:
    LoginDialog* _login_dialog;
};

PreLoginDialog::PreLoginDialog(GUI& gui, LoginDialog* login_dialog)
    : TabDialog(gui, false)
    , _login_dialog (login_dialog)
{
    try {

        int position = 0;
        add_bottom_button(_main_win, res_get_string(STR_BUTTON_BACK),
                          CEGUI::Event::Subscriber(&PreLoginDialog::handle_back, this),
                          position);
        add_bottom_button(_main_win, res_get_string(STR_BUTTON_QUIT),
                          CEGUI::Event::Subscriber(&PreLoginDialog::handle_quit, this),
                          position);

    } catch (CEGUI::Exception& e) {
        LOG_ERROR("Exception: %s", e.getMessage().c_str());
    } catch (...) {
        throw;
    }
}

bool PreLoginDialog::handle_back(const CEGUI::EventArgs& e)
{
    ASSERT(_login_dialog);
    LoginDialog* login_dialog = _login_dialog;
    _login_dialog = NULL;
    set_dialog(login_dialog);
    return true;
}

bool PreLoginDialog::handle_quit(const CEGUI::EventArgs& e)
{
    DBG(0, "");
    application().quit();
    return true;
}

MessageDialog::MessageDialog(GUI& gui)
    : GUI::Dialog(gui, true)
{
    try {
        CEGUI::WindowManager& winMgr = CEGUI::WindowManager::getSingleton();
        _root = winMgr.createWindow("DefaultWindow");
        set_win_size(_root, MAIN_GUI_WIDTH, MAIN_GUI_HEIGHT);
    } catch (CEGUI::Exception& e) {
        LOG_ERROR("Exception: %s", e.getMessage().c_str());
        throw;
    }
}

LoginDialog::LoginDialog(GUI& gui)
    : GUI::Dialog(gui)
{
    try {
        CEGUI::WindowManager& winMgr = CEGUI::WindowManager::getSingleton();
        _root = winMgr.createWindow("DefaultWindow");

        CEGUI::Window* wnd = winMgr.createWindow("TaharezLook/StaticText");
        int x_pos = (MAIN_GUI_WIDTH - LOGIN_DIALOG_WIDTH) / 2;
        int y_pos = (MAIN_GUI_HEIGHT - LOGIN_DIALOG_HEIGHT) / 3;
        set_win_pos(wnd, x_pos, y_pos);
        set_win_size(wnd, LOGIN_DIALOG_WIDTH, LOGIN_DIALOG_HEIGHT);
        _root->addChildWindow(wnd);

        CEGUI::Window* host_label = winMgr.createWindow("TaharezLook/StaticText");
        host_label->setText(res_get_string(STR_LABEL_HOST));
        host_label->setProperty("FrameEnabled", "false");
        host_label->setProperty("BackgroundEnabled", "false");
        set_win_pos(host_label, GUI_SPACE, LOGIN_DIALOG_V_START);
        set_win_size(host_label, GUI_LABEL_WIDTH, GUI_LABEL_HEIGHT);

        wnd->addChildWindow(host_label);

        _host_box = winMgr.createWindow("TaharezLook/Editbox");
        set_win_pos(_host_box, GUI_LABEL_WIDTH + GUI_SPACE, LOGIN_DIALOG_V_START);
        int width = LOGIN_DIALOG_WIDTH - GUI_LABEL_WIDTH - 2 * GUI_SPACE;
        set_win_size(_host_box, width, GUI_LABEL_HEIGHT);
        _host_box->setText(application().get_host());
        wnd->addChildWindow(_host_box);

        CEGUI::Window* port_label = winMgr.createWindow("TaharezLook/StaticText");
        port_label->setText(res_get_string(STR_LABEL_PORT));
        port_label->setProperty("FrameEnabled", "false");
        port_label->setProperty("BackgroundEnabled", "false");
        y_pos = LOGIN_DIALOG_V_START + GUI_LABEL_HEIGHT + GUI_SPACE;
        set_win_pos(port_label, GUI_SPACE, y_pos);
        set_win_size(port_label, GUI_LABEL_WIDTH, GUI_LABEL_HEIGHT);
        wnd->addChildWindow(port_label);

        _port_box = winMgr.createWindow("TaharezLook/Editbox");
        set_win_pos(_port_box, GUI_SPACE + GUI_LABEL_WIDTH, y_pos);
        set_win_size(_port_box, GUI_PORT_WIDTH, GUI_TEXT_BOX_HEIGHT);
        set_port_text(_port_box, application().get_port());
        wnd->addChildWindow(_port_box);

        _sport_box = winMgr.createWindow("TaharezLook/Editbox");
        x_pos = LOGIN_DIALOG_WIDTH - GUI_SPACE - GUI_PORT_WIDTH;
        set_win_pos(_sport_box, x_pos, y_pos);
        set_win_size(_sport_box, GUI_PORT_WIDTH, GUI_TEXT_BOX_HEIGHT);
        set_port_text(_sport_box, application().get_sport());
        wnd->addChildWindow(_sport_box);

        CEGUI::Window* sport_label = winMgr.createWindow("TaharezLook/StaticText");
        sport_label->setText(res_get_string(STR_LABEL_SPORT));
        sport_label->setProperty("FrameEnabled", "false");
        sport_label->setProperty("BackgroundEnabled", "false");
        x_pos -= GUI_LABEL_WIDTH;
        set_win_pos(sport_label, x_pos, y_pos);
        set_win_size(sport_label, GUI_LABEL_WIDTH, GUI_LABEL_HEIGHT);
        wnd->addChildWindow(sport_label);

        CEGUI::Window* label = winMgr.createWindow("TaharezLook/StaticText");
        label->setText(res_get_string(STR_LABEL_PASSWORD));
        label->setProperty("FrameEnabled", "false");
        label->setProperty("BackgroundEnabled", "false");
        y_pos += GUI_LABEL_HEIGHT + GUI_SPACE;
        set_win_pos(label, GUI_SPACE, y_pos);
        set_win_size(label, GUI_LABEL_WIDTH, GUI_LABEL_HEIGHT);
        wnd->addChildWindow(label);

        _pass_box = winMgr.createWindow("TaharezLook/Editbox");
        x_pos = GUI_LABEL_WIDTH + GUI_SPACE;
        set_win_pos(_pass_box, x_pos, y_pos);
        width = LOGIN_DIALOG_WIDTH - GUI_LABEL_WIDTH - 2 * GUI_SPACE;
        set_win_size(_pass_box, width, GUI_TEXT_BOX_HEIGHT);
        ((CEGUI::Editbox*)_pass_box)->setTextMasked(true);
        ((CEGUI::Editbox*)_pass_box)->setMaskCodePoint(/*0x000026AB*/ 0x00002022 );
        _pass_box->setText(application().get_password().c_str());
        wnd->addChildWindow(_pass_box);

        x_pos = 0;
        add_bottom_button(wnd,
                          res_get_string(STR_BUTTON_CONNECT),
                          CEGUI::Event::Subscriber(&LoginDialog::handle_connect, this),
                          x_pos);
        add_bottom_button(wnd,
                          res_get_string(STR_BUTTON_QUIT),
                          CEGUI::Event::Subscriber(&LoginDialog::handle_quit, this),
                          x_pos);
        add_bottom_button(wnd,
                          res_get_string(STR_BUTTON_OPTIONS),
                          CEGUI::Event::Subscriber(&LoginDialog::handle_options, this),
                          x_pos);

    } catch (CEGUI::Exception& e) {
        LOG_ERROR("Exception: %s", e.getMessage().c_str());
        throw;
    }
}

bool LoginDialog::handle_connect(const CEGUI::EventArgs& e)
{
    const char* host_name = _host_box->getText().c_str();

    if (strlen(host_name) == 0) {
        error_box(res_get_string(STR_MESG_MISSING_HOST_NAME));
        return true;
    }
    int port = -1;
    int sport = -1;

    const char* port_str = _port_box->getText().c_str();
    if (strlen(port_str) != 0 && (port = str_to_port(port_str)) == -1) {
        error_box(res_get_string(STR_MESG_INVALID_PORT));
        return true;
    }

    const char* sport_str = _sport_box->getText().c_str();
    if (strlen(sport_str) != 0 && (sport = str_to_port(sport_str)) == -1) {
        error_box(res_get_string(STR_MESG_INVALID_SPORT));
        return true;
    }

    if (port == sport && port == -1) {
        error_box(res_get_string(STR_MESG_MISSING_PORT));
        return true;
    }

    DBG(0, "host %s port %d sport %d", host_name, port, sport);
    application().connect(host_name, port, sport, _pass_box->getText().c_str());
    return true;
}

bool LoginDialog::handle_quit(const CEGUI::EventArgs& e)
{
    DBG(0, "");
    application().quit();
    return true;
}

bool LoginDialog::handle_options(const CEGUI::EventArgs& e)
{
    dettach();
    set_dialog(new PreLoginDialog(_gui, this));
    return true;
}

void LoginDialog::set_port_text(CEGUI::Window* win, int port)
{
    if (port == -1) {
        win->setText("");
        return;
    }

    char port_string[25];
    sprintf(port_string, "%d", port);
    win->setText(port_string);
}

class ConnectingDialog : public GUI::Dialog {
public:
    ConnectingDialog(GUI& gui);

private:
    bool handle_cancel(const CEGUI::EventArgs& e);
    bool handle_quit(const CEGUI::EventArgs& e);
};

bool ConnectingDialog::handle_cancel(const CEGUI::EventArgs& e)
{
    DBG(0, "");
    application().disconnect();
    return true;
}

bool ConnectingDialog::handle_quit(const CEGUI::EventArgs& e)
{
    DBG(0, "");
    application().quit();
    return true;
}

ConnectingDialog::ConnectingDialog(GUI& gui)
    :  GUI::Dialog(gui)
{
    try {
        CEGUI::WindowManager& winMgr = CEGUI::WindowManager::getSingleton();
        _root = winMgr.createWindow("DefaultWindow");

        CEGUI::Window* wnd = winMgr.createWindow("TaharezLook/StaticText");
        int x_pos = (MAIN_GUI_WIDTH - CONNECTING_DIALOG_WIDTH) / 2;
        int y_pos = (MAIN_GUI_HEIGHT - CONNECTING_DIALOG_HEIGHT) / 2;
        set_win_pos(wnd, x_pos, y_pos);
        set_win_size(wnd, CONNECTING_DIALOG_WIDTH, CONNECTING_DIALOG_HEIGHT);
        CEGUI::String text(res_get_string(STR_MESG_CONNECTING));
        wnd->setText(text + " " + application().get_host());
        wnd->setProperty("HorzFormatting", "LeftAligned");
        wnd->setProperty("VertFormatting", "TopAligned");
        _root->addChildWindow(wnd);

        x_pos = 0;

        add_bottom_button(wnd, res_get_string(STR_BUTTON_CANCEL),
                          CEGUI::Event::Subscriber(&ConnectingDialog::handle_cancel, this),
                          x_pos);

        if (_gui.is_disconnect_allowed()) {
            add_bottom_button(wnd, res_get_string(STR_BUTTON_QUIT),
                              CEGUI::Event::Subscriber(&ConnectingDialog::handle_quit, this),
                              x_pos);
        }

    } catch (CEGUI::Exception& e) {
        LOG_ERROR("Exception: %s", e.getMessage().c_str());
        throw;
    }
}

TabDialog::TabDialog(GUI& gui, bool connected)
    : GUI::Dialog(gui)
{
    CEGUI::WindowManager& winMgr = CEGUI::WindowManager::getSingleton();

    try {
         _root = winMgr.createWindow("DefaultWindow");

        CEGUI::Window* wnd = _main_win = winMgr.createWindow("TaharezLook/StaticText");
        set_win_pos(wnd, 0, 0);
        set_win_size(wnd, MAIN_GUI_WIDTH, MAIN_GUI_HEIGHT);
        wnd->setAlpha(0.5);
        _root->addChildWindow(wnd);

        CEGUI::TabControl* tab_ctrl;
        tab_ctrl = static_cast<CEGUI::TabControl*>(winMgr.createWindow("TaharezLook/TabControl"));
        set_win_pos(tab_ctrl, GUI_SPACE, GUI_SPACE);
        int tab_width = MAIN_GUI_WIDTH - GUI_SPACE * 2;
        int tab_height = MAIN_GUI_HEIGHT - GUI_SPACE * 3 - BUTTON_HEIGHT;
        set_win_size(tab_ctrl, tab_width, tab_height);
        tab_ctrl->setInheritsAlpha(false);
        tab_ctrl->setTabHeight(cegui_absdim(22));
        tab_ctrl->setTabTextPadding(cegui_absdim(10));
        tab_height = (int)tab_ctrl->getTabHeight().asAbsolute(1);
        wnd->addChildWindow(tab_ctrl);

        GUI::TabFactorys& _tab_factorys = get_factoris();
        GUI::TabFactorys::iterator iter = _tab_factorys.begin();

        int tab_content_width = MAIN_GUI_WIDTH - GUI_SPACE * 4;
        int tab_content_height = MAIN_GUI_HEIGHT - GUI_SPACE * 5 - BUTTON_HEIGHT - tab_height;

        for (; iter != _tab_factorys.end(); iter++) {

            GUI::Tab* tab = (*iter)->create_tab(connected, tab_content_width,
                                                    tab_content_height);

            if (!tab) {
                continue;
            }

            _tabs.push_front(tab);
            CEGUI::Window* gui_sheet = winMgr.createWindow("DefaultGUISheet");
            gui_sheet->setText(tab->get_name());
            set_win_pos(gui_sheet, GUI_SPACE, GUI_SPACE);
            set_win_size(gui_sheet, tab_content_width, tab_content_height);
            tab_ctrl->addTab(gui_sheet);
            CEGUI::Window& tab_window = tab->get_root_window();
            tab_window.setDestroyedByParent(false);
            gui_sheet->addChildWindow(&tab_window);
        }

    } catch (CEGUI::Exception& e) {
        LOG_ERROR("Exception: %s", e.getMessage().c_str());
    } catch (...) {
        throw;
    }
}

TabDialog::~TabDialog()
{
    while (!_tabs.empty()) {
        GUI::Tab* tab = *_tabs.begin();
        _tabs.pop_front();
        delete tab;
    }
}

class SettingsDialog : public TabDialog {
public:
    SettingsDialog(GUI& gui);

    bool handle_close(const CEGUI::EventArgs& e);
    bool handle_quit(const CEGUI::EventArgs& e);
    bool handle_disconnect(const CEGUI::EventArgs& e);
};

bool SettingsDialog::handle_close(const CEGUI::EventArgs& e)
{
    DBG(0, "");
    application().hide_gui();
    return true;
}

bool SettingsDialog::handle_quit(const CEGUI::EventArgs& e)
{
    DBG(0, "");
    application().quit();
    return true;
}

bool SettingsDialog::handle_disconnect(const CEGUI::EventArgs& e)
{
    DBG(0, "");
    application().disconnect();
    return true;
}

SettingsDialog::SettingsDialog(GUI& gui)
    :  TabDialog(gui, true)
{
    try {

        int position = 0;
        add_bottom_button(_main_win, res_get_string(STR_BUTTON_CLOSE),
                          CEGUI::Event::Subscriber(&SettingsDialog::handle_close, this),
                          position);
        add_bottom_button(_main_win, res_get_string(STR_BUTTON_QUIT),
                          CEGUI::Event::Subscriber(&SettingsDialog::handle_quit, this),
                          position);

        if (_gui.is_disconnect_allowed()) {
            add_bottom_button(_main_win, res_get_string(STR_BUTTON_DISCONNECT),
                              CEGUI::Event::Subscriber(&SettingsDialog::handle_disconnect, this),
                              position);
        }

    } catch (CEGUI::Exception& e) {
        LOG_ERROR("Exception: %s", e.getMessage().c_str());
    } catch (...) {
        throw;
    }
}

GUI::GUI(Application& app, Application::State state)
    : ScreenLayer (SCREEN_LAYER_GUI, false)
    , _app (app)
    , _state (state)
    , _pixmap (new RedPixmapSw(MAIN_GUI_WIDTH, MAIN_GUI_HEIGHT, RedDrawable::RGB32, true, 0))
    , _renderer (new CEGUI::SoftRenderer(_pixmap->get_data(), MAIN_GUI_WIDTH, MAIN_GUI_HEIGHT,
                                         _pixmap->get_stride()))
    , _dialog (NULL)
    , _prev_time (Platform::get_monolithic_time())

{
    LOG_INFO("");
    init_cegui();
#ifdef GUI_DEMO
    register_tab_factory(*(new SampleTabFactory(2)));
    register_tab_factory(*(new SampleTabFactory(3)));
    register_tab_factory(*(new SampleTabFactory(1)));
    register_tab_factory(*(new SampleTabFactory(4)));
    register_tab_factory(*(new SampleTabFactory(5)));
#endif
    create_dialog();
}

GUI::~GUI()
{
    delete _dialog;
    detach();
    delete _gui_system;
    delete _renderer;
    delete _pixmap;
}

void GUI::init_cegui()
{
    std::string log_file_name;

    Platform::get_app_data_dir(log_file_name, "spicec");
    Platform::path_append(log_file_name, "cegui.log");

    _gui_system = new CEGUI::System(_renderer, new CEGUIResourceProvider(),
                                    NULL, NULL, "", log_file_name);

    CEGUI::SchemeManager::getSingleton().loadScheme("TaharezLook.scheme");
    _gui_system->setDefaultMouseCursor("TaharezLook", "MouseArrow");
    _gui_system->setDefaultTooltip("TaharezLook/Tooltip");

    CEGUI::String font_name("DejaVuSans-10");
    CEGUI::Font* font;

    if (!CEGUI::FontManager::getSingleton().isFontPresent(font_name)) {
        font = CEGUI::FontManager::getSingleton().createFont(font_name + ".font");
    } else {
        font = CEGUI::FontManager::getSingleton().getFont(font_name);
    }

    //font->setProperty("PointSize", "10");
    CEGUI::System::getSingleton().setDefaultFont(font);
}

bool comp_factorys(GUI::TabFactory* f1, GUI::TabFactory* f2)
{
    return f1->get_order() < f2->get_order();
}

void GUI::register_tab_factory(TabFactory& factory)
{
    TabFactorys::iterator iter = _tab_factorys.begin();

    for (; iter != _tab_factorys.end(); iter++) {
        if ((*iter) == &factory) {
            return;
        }
    }

    _tab_factorys.push_back(&factory);
    _tab_factorys.sort(comp_factorys);
}

void GUI::unregister_tab_factory(TabFactory& factory)
{
    TabFactorys::iterator iter = _tab_factorys.begin();

    for (; iter != _tab_factorys.end(); iter++) {
        if ((*iter) == &factory) {
            _tab_factorys.erase(iter);
            return;
        }
    }
}

void GUI::detach()
{
    if (!screen()) {
        return;
    }
    clear_area();
    screen()->detach_layer(*this);
    set_dialog(NULL);
}

void GUI::conditional_update()
{
    if (_gui_system->isRedrawRequested()) {
        invalidate();
    }
}

void GUI::update_layer_area()
{
    if (!_dialog || !screen()) {
        clear_area();
        return;
    }
    SpicePoint screen_size = screen()->get_size();

    int dx = (screen_size.x - MAIN_GUI_WIDTH) / 2;
    int dy = (screen_size.y - MAIN_GUI_HEIGHT) / 2;

    DBG(0, "screen_size.x = %d screen_size.y = %d", screen_size.x, screen_size.y);

    _pixmap->set_origin(-dx, -dy);
    CEGUI::Window& root = _dialog->root_window();
    QRegion regin;
    region_init(&regin);

    for (unsigned int i = 0; i < root.getChildCount(); i++) {

        CEGUI::Window* child = root.getChildAtIdx(i);
        if (!child->isVisible()) {
            continue;
        }

        CEGUI::Rect area = child->getPixelRect();
        SpiceRect r;
        r.left = (int)area.d_left + dx;
        r.right = (int)area.d_right + dx;
        r.top = (int)area.d_top + dy;
        r.bottom = (int)area.d_bottom + dy;
        region_add(&regin, &r);

    }
    set_area(regin);
    region_destroy(&regin);
}

void GUI::copy_pixels(const QRegion& dest_region, RedDrawable& dest)
{
    pixman_box32_t *rects;
    int num_rects;

    if (region_is_empty(&dest_region)) {
        return;
    }

    rects = pixman_region32_rectangles((pixman_region32_t *)&dest_region, &num_rects);
    for (int i = 0; i < num_rects; i++) {
        SpiceRect r;

        r.left = rects[i].x1;
        r.top = rects[i].y1;
        r.right = rects[i].x2;
        r.bottom = rects[i].y2;
        _pixmap->copy_pixels(dest, r.left, r.top, r);
    }

    _gui_system->renderGUI();
    for (int i = 0; i < num_rects; i++) {
        SpiceRect r;

        r.left = rects[i].x1;
        r.top = rects[i].y1;
        r.right = rects[i].x2;
        r.bottom = rects[i].y2;
        dest.copy_pixels(*_pixmap, r.left, r.top, r);
    }
}

void GUI::on_size_changed()
{
    DBG(0, "");
    update_layer_area();
}

void GUI::set_dialog(Dialog* dialog)
{
    if (_dialog) {
        _dialog->pre_destroy();
        delete _dialog;
        _dialog = NULL;
    }

    if (!dialog) {
        return;
    }

    _dialog = dialog;
    gui_system().setGUISheet(&_dialog->root_window());
    update_layer_area();
}

void GUI::dettach_dialog(Dialog* dialog)
{
    if (!dialog || _dialog != dialog) {
        return;
    }
    gui_system().setGUISheet(NULL);
    _dialog = NULL;
    update_layer_area();
}

void GUI::create_dialog()
{
    switch (_state) {
    case Application::DISCONNECTED:
        set_dialog(new LoginDialog(*this));
        break;
    case Application::VISIBILITY:
        set_dialog(new SettingsDialog(*this));
        break;
    case Application::CONNECTING:
        set_dialog(new ConnectingDialog(*this));
        break;
    case Application::CONNECTED:
        break;
    case Application::DISCONECTING:
        set_dialog(NULL);
        break;
    }
}

void GUI::set_state(Application::State state)
{
    if (_state == state) {
        return;
    }
    _state = state;
    create_dialog();
}

bool GUI::prepare_dialog()
{
    if (!_dialog) {
        create_dialog();
    }

    return !!_dialog;
}

void GUI::set_screen(RedScreen* in_screen)
{
    detach();
    if (!in_screen) {
        _app.remove_key_handler(*this);
        return;
    }
    ASSERT(!screen());
    in_screen->attach_layer(*this);
    CEGUI::MouseCursor::getSingleton().hide();
    update_layer_area();
    _app.set_key_handler(*this);
}

void GUI::on_pointer_enter(int x, int y, unsigned int buttons_state)
{
    CEGUI::MouseCursor::getSingleton().show();
    screen()->hide_cursor();
    _app.set_key_handler(*this);
    on_pointer_motion(x, y, buttons_state);
}

void GUI::on_pointer_motion(int x, int y, unsigned int buttons_state)
{
    _gui_system->injectMousePosition(float(x + _pixmap->get_origin().x),
                                     float(y + _pixmap->get_origin().y));
    invalidate();
}

void GUI::on_mouse_button_press(int button, int buttons_state)
{
    _app.set_key_handler(*this);
    switch (button) {
    case SPICE_MOUSE_BUTTON_LEFT:
        _gui_system->injectMouseButtonDown(CEGUI::LeftButton);
        break;
    case SPICE_MOUSE_BUTTON_MIDDLE:
        _gui_system->injectMouseButtonDown(CEGUI::MiddleButton);
        break;
    case SPICE_MOUSE_BUTTON_RIGHT:
        _gui_system->injectMouseButtonDown(CEGUI::RightButton);
        break;
    case SPICE_MOUSE_BUTTON_UP:
        _gui_system->injectMouseWheelChange(-1);
        break;
    case SPICE_MOUSE_BUTTON_DOWN:
        _gui_system->injectMouseWheelChange(1);
        break;
    default:
        THROW("invalid SpiceMouseButton %d", button);
    }
    conditional_update();
}

void GUI::on_mouse_button_release(int button, int buttons_state)
{
    switch (button) {
    case SPICE_MOUSE_BUTTON_LEFT:
        _gui_system->injectMouseButtonUp(CEGUI::LeftButton);
        break;
    case SPICE_MOUSE_BUTTON_MIDDLE:
        _gui_system->injectMouseButtonUp(CEGUI::MiddleButton);
        break;
    case SPICE_MOUSE_BUTTON_RIGHT:
        _gui_system->injectMouseButtonUp(CEGUI::RightButton);
        break;
    case SPICE_MOUSE_BUTTON_UP:
    case SPICE_MOUSE_BUTTON_DOWN:
        break;
    default:
        THROW("invalid SpiceMouseButton %d", button);
    }
    conditional_update();
}

void GUI::on_pointer_leave()
{
    CEGUI::MouseCursor::getSingleton().hide();
    invalidate();
}

#define KEY_CASE(a, b)          \
    case a:                     \
        return  CEGUI::Key::b

static inline CEGUI::Key::Scan red_ket_cegui_scan(RedKey key)
{
    switch (key) {
    KEY_CASE(REDKEY_ESCAPE, Escape);
    KEY_CASE(REDKEY_1, One);
    KEY_CASE(REDKEY_2, Two);
    KEY_CASE(REDKEY_3, Three);
    KEY_CASE(REDKEY_4, Four);
    KEY_CASE(REDKEY_5, Five);
    KEY_CASE(REDKEY_6, Six);
    KEY_CASE(REDKEY_7, Seven);
    KEY_CASE(REDKEY_8, Eight);
    KEY_CASE(REDKEY_9, Nine);
    KEY_CASE(REDKEY_0, Zero);
    KEY_CASE(REDKEY_MINUS, Minus);
    KEY_CASE(REDKEY_EQUALS, Equals);
    KEY_CASE(REDKEY_BACKSPACE, Backspace);
    KEY_CASE(REDKEY_TAB, Tab);
    KEY_CASE(REDKEY_Q, Q);
    KEY_CASE(REDKEY_W, W);
    KEY_CASE(REDKEY_E, E);
    KEY_CASE(REDKEY_R, R);
    KEY_CASE(REDKEY_T, T);
    KEY_CASE(REDKEY_Y, Y);
    KEY_CASE(REDKEY_U, U);
    KEY_CASE(REDKEY_I, I);
    KEY_CASE(REDKEY_O, O);
    KEY_CASE(REDKEY_P, P);
    KEY_CASE(REDKEY_L_BRACKET, LeftBracket);
    KEY_CASE(REDKEY_R_BRACKET, RightBracket);
    KEY_CASE(REDKEY_ENTER, Return);
    KEY_CASE(REDKEY_L_CTRL, LeftControl);
    KEY_CASE(REDKEY_A, A);
    KEY_CASE(REDKEY_S, S);
    KEY_CASE(REDKEY_D, D);
    KEY_CASE(REDKEY_F, F);
    KEY_CASE(REDKEY_G, G);
    KEY_CASE(REDKEY_H, H);
    KEY_CASE(REDKEY_J, J);
    KEY_CASE(REDKEY_K, K);
    KEY_CASE(REDKEY_L, L);
    KEY_CASE(REDKEY_SEMICOLON, Semicolon);
    KEY_CASE(REDKEY_QUOTE, Apostrophe);

    KEY_CASE(REDKEY_BACK_QUOTE, Grave);
    KEY_CASE(REDKEY_L_SHIFT, LeftShift);
    KEY_CASE(REDKEY_BACK_SLASH, Backslash);
    KEY_CASE(REDKEY_Z, Z);
    KEY_CASE(REDKEY_X, X);
    KEY_CASE(REDKEY_C, C);
    KEY_CASE(REDKEY_V, V);
    KEY_CASE(REDKEY_B, B);
    KEY_CASE(REDKEY_N, N);
    KEY_CASE(REDKEY_M, M);
    KEY_CASE(REDKEY_COMMA, Comma);
    KEY_CASE(REDKEY_PERIOD, Period);
    KEY_CASE(REDKEY_SLASH, Slash);
    KEY_CASE(REDKEY_R_SHIFT, RightShift);
    KEY_CASE(REDKEY_PAD_MULTIPLY, Multiply);
    KEY_CASE(REDKEY_L_ALT, LeftAlt);
    KEY_CASE(REDKEY_SPACE, Space);
    KEY_CASE(REDKEY_CAPS_LOCK, Capital);
    KEY_CASE(REDKEY_F1, F1);
    KEY_CASE(REDKEY_F2, F2);
    KEY_CASE(REDKEY_F3, F3);
    KEY_CASE(REDKEY_F4, F4);
    KEY_CASE(REDKEY_F5, F5);
    KEY_CASE(REDKEY_F6, F6);
    KEY_CASE(REDKEY_F7, F7);
    KEY_CASE(REDKEY_F8, F8);
    KEY_CASE(REDKEY_F9, F9);
    KEY_CASE(REDKEY_F10, F10);
    KEY_CASE(REDKEY_NUM_LOCK, NumLock);
    KEY_CASE(REDKEY_SCROLL_LOCK, ScrollLock);
    KEY_CASE(REDKEY_PAD_7, Numpad7);
    KEY_CASE(REDKEY_PAD_8, Numpad8);
    KEY_CASE(REDKEY_PAD_9, Numpad9);
    KEY_CASE(REDKEY_PAD_MINUS, Subtract);
    KEY_CASE(REDKEY_PAD_4, Numpad4);
    KEY_CASE(REDKEY_PAD_5, Numpad5);
    KEY_CASE(REDKEY_PAD_6, Numpad6);
    KEY_CASE(REDKEY_PAD_PLUS, Add);
    KEY_CASE(REDKEY_PAD_1, Numpad1);
    KEY_CASE(REDKEY_PAD_2, Numpad2);
    KEY_CASE(REDKEY_PAD_3, Numpad3);
    KEY_CASE(REDKEY_PAD_0, Numpad0);
    KEY_CASE(REDKEY_PAD_POINT, Decimal);

    KEY_CASE(REDKEY_EUROPEAN, OEM_102);
    KEY_CASE(REDKEY_F11, F11);
    KEY_CASE(REDKEY_F12, F12);

    KEY_CASE(REDKEY_JAPANESE_HIRAGANA_KATAKANA, Kana);
    KEY_CASE(REDKEY_JAPANESE_BACKSLASH, ABNT_C1);
    KEY_CASE(REDKEY_JAPANESE_HENKAN, Convert);
    KEY_CASE(REDKEY_JAPANESE_MUHENKAN, NoConvert);
    KEY_CASE(REDKEY_JAPANESE_YEN, Yen);

    //KEY_CASE(REDKEY_KOREAN_HANGUL_HANJA,
    //KEY_CASE(REDKEY_KOREAN_HANGUL,

    KEY_CASE(REDKEY_PAD_ENTER, NumpadEnter);
    KEY_CASE(REDKEY_R_CTRL, RightControl);
    KEY_CASE(REDKEY_FAKE_L_SHIFT, LeftShift);
    KEY_CASE(REDKEY_PAD_DIVIDE, Divide);
    KEY_CASE(REDKEY_FAKE_R_SHIFT, RightShift);
    KEY_CASE(REDKEY_CTRL_PRINT_SCREEN, SysRq);
    KEY_CASE(REDKEY_R_ALT, RightAlt);
    //KEY_CASE(REDKEY_CTRL_BREAK
    KEY_CASE(REDKEY_HOME, Home);
    KEY_CASE(REDKEY_UP, ArrowUp);
    KEY_CASE(REDKEY_PAGEUP, PageUp);
    KEY_CASE(REDKEY_LEFT, ArrowLeft);
    KEY_CASE(REDKEY_RIGHT, ArrowRight);
    KEY_CASE(REDKEY_END, End);
    KEY_CASE(REDKEY_DOWN, ArrowDown);
    KEY_CASE(REDKEY_PAGEDOWN, PageDown);
    KEY_CASE(REDKEY_INSERT, Insert);
    KEY_CASE(REDKEY_DELETE, Delete);
    KEY_CASE(REDKEY_LEFT_CMD, LeftWindows);
    KEY_CASE(REDKEY_RIGHT_CMD, RightWindows);
    KEY_CASE(REDKEY_MENU, AppMenu);
    KEY_CASE(REDKEY_PAUSE, Pause);
    default:
        return (CEGUI::Key::Scan)0;
    };
}

void GUI::on_key_down(RedKey key)
{
    CEGUI::Key::Scan scan = red_ket_cegui_scan(key);

    if (scan == (CEGUI::Key::Scan)0) {
        return;
    }

    _gui_system->injectKeyDown(scan);
    conditional_update();
}

void GUI::on_key_up(RedKey key)
{
    CEGUI::Key::Scan scan = red_ket_cegui_scan(key);

    if (scan == (CEGUI::Key::Scan)0) {
        return;
    }

    _gui_system->injectKeyUp(scan);
    conditional_update();
}

void GUI::on_char(uint32_t ch)
{
    _gui_system->injectChar(ch);
    conditional_update();
}

void GUI::idle()
{
    uint64_t now = Platform::get_monolithic_time();
    _gui_system->injectTimePulse(float(double(now) - double(_prev_time)) / (1000 * 1000 * 1000));
    _prev_time = now;
    conditional_update();
}

bool GUI::message_box(MessageType type, const char *text, const ButtonsList& buttons,
                          BoxResponse* _response_handler)
{
    if (!_dialog) {
        set_dialog(new MessageDialog(*this));
    }
    return _dialog->message_box(type, text, buttons, _response_handler);
}
