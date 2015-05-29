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

#ifndef _H_CONTROLLER_MENU
#define _H_CONTROLLER_MENU

#include "named_pipe.h"
#include "threads.h"

class ControllerConnection;
struct ControllerInit;
struct ControllerMsg;
class CmdLineParser;
class Menu;

class ControllerInterface {
public:
    virtual ~ControllerInterface() {}

    virtual bool connect(const std::string& host, int port, int sport,
                         const std::string& password) = 0;
    virtual void set_title(const std::string& title) = 0;
    virtual void set_auto_display_res(bool auto_display_res) = 0;
    virtual void show_me(bool full_screen) = 0;
    virtual void hide_me() = 0;
    virtual bool set_channels_security(CmdLineParser& parser, bool on, char *val,
                                       const char* arg0) = 0;
    virtual bool set_enable_channels(CmdLineParser& parser, bool enable, char *val,
                                     const char* arg0) = 0;
    virtual bool set_connection_ciphers(const char* ciphers, const char* arg0) = 0;
    virtual bool set_ca_file(const char* ca_file, const char* arg0) = 0;
    virtual bool set_host_cert_subject(const char* subject, const char* arg0) = 0;
    virtual void set_hotkeys(const std::string& hotkeys) = 0;
    virtual int get_controller_menu_item_id(int32_t opaque_conn_ref, uint32_t id) = 0;
    virtual void clear_menu_items(int32_t opaque_conn_ref) = 0;
    virtual Menu* get_app_menu() = 0;
    virtual void set_menu(Menu* menu) = 0;
    virtual void delete_menu() = 0;
#ifdef USE_SMARTCARD
    virtual void enable_smartcard(bool enable) = 0;
#endif
};

class Controller : public NamedPipe::ListenerInterface {
public:
    Controller(ControllerInterface *handler);
    virtual ~Controller();

    Controller* ref() { _refs++; return this;}
    void unref() { if (!--_refs) delete this;}

    virtual NamedPipe::ConnectionInterface &create();
    bool set_exclusive(bool exclusive);
    void add_connection(NamedPipe::ConnectionRef conn_ref, ControllerConnection *conn);
    void remove_connection(NamedPipe::ConnectionRef conn_ref);
    void on_command(NamedPipe::ConnectionRef conn_ref, int32_t id);

private:
    ControllerInterface *_handler;
    std::map<NamedPipe::ConnectionRef, ControllerConnection*> _connections;
    NamedPipe::ListenerRef _pipe;
    bool _exclusive;
    int _refs;
};

#define CONTROLLER_BUF_SIZE 4096

class ControllerConnection : public NamedPipe::ConnectionInterface {
public:
    ControllerConnection(ControllerInterface *handler, Controller& parent);
    virtual ~ControllerConnection();

    virtual void bind(NamedPipe::ConnectionRef conn_ref);
    virtual void on_data();
    bool write_msg(const void *buf, int len);
    void reset_handler() { _handler = NULL;}

private:
    bool read_msgs();
    bool handle_init(ControllerInit *init);
    bool handle_message(ControllerMsg *hdr);
    bool create_menu(char* resource);
    bool set_multi_val(uint32_t op, char* multi_val);

private:
    ControllerInterface *_handler;
    Controller& _parent;
    bool _initialized;

    int _write_pending;
    uint8_t *_write_pos;
    uint8_t *_read_pos;
    uint8_t _write_buf[CONTROLLER_BUF_SIZE];
    uint8_t _read_buf[CONTROLLER_BUF_SIZE];
    RecurciveMutex _write_lock;

    std::string _host;
    std::string _password;
    int _port;
    int _sport;
    bool _full_screen;
    bool _auto_display_res;
};

#endif
