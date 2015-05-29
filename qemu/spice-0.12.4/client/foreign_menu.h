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

#ifndef _H_FOREIGN_MENU
#define _H_FOREIGN_MENU

#include "named_pipe.h"
#include "menu.h"

class ForeignMenuConnection;
struct FrgMenuInit;
struct FrgMenuMsg;

class ForeignMenuInterface : public CommandTarget {
public:
    virtual ~ForeignMenuInterface() {}

    virtual int get_foreign_menu_item_id(int32_t opaque_conn_ref, uint32_t msg_id) = 0;
    virtual void clear_menu_items(int32_t opaque_conn_ref) = 0;
    virtual void remove_menu_item(int item_id) = 0;
    virtual Menu* get_app_menu() = 0;
    virtual void update_menu() = 0;
};

class ForeignMenu : public NamedPipe::ListenerInterface {
public:
    ForeignMenu(ForeignMenuInterface *handler, bool active = false);
    virtual ~ForeignMenu();

    ForeignMenu* ref() { _refs++; return this;}
    void unref() { if (!--_refs) delete this;}

    virtual NamedPipe::ConnectionInterface &create();
    void add_connection(NamedPipe::ConnectionRef conn_ref, ForeignMenuConnection *conn);
    void remove_connection(NamedPipe::ConnectionRef conn_ref);
    void add_sub_menus();
    void on_command(NamedPipe::ConnectionRef conn_ref, int32_t id);
    void on_activate();
    void on_deactivate();

private:
    void send_active_state(ForeignMenuConnection *conn, int32_t cmd);

private:
    ForeignMenuInterface *_handler;
    std::map<NamedPipe::ConnectionRef, ForeignMenuConnection*> _connections;
    NamedPipe::ListenerRef _foreign_menu;
    bool _active;
    int _refs;
};

#define FOREIGN_MENU_BUF_SIZE 4096

class ForeignMenuConnection : public NamedPipe::ConnectionInterface {
public:
    ForeignMenuConnection(ForeignMenuInterface *handler, ForeignMenu& parent);
    virtual ~ForeignMenuConnection();

    virtual void bind(NamedPipe::ConnectionRef conn_ref);
    virtual void on_data();
    bool write_msg(const void *buf, int len);
    void reset_handler() { _handler = NULL;}
    void add_sub_menu();

private:
    bool read_msgs();
    bool handle_init(FrgMenuInit *init);
    bool handle_message(FrgMenuMsg *hdr);
    int get_item_state(int item_type);

private:
    ForeignMenuInterface *_handler;
    ForeignMenu& _parent;
    Menu* _sub_menu;
    bool _initialized;
    int _write_pending;
    uint8_t *_write_pos;
    uint8_t *_read_pos;
    uint8_t _write_buf[FOREIGN_MENU_BUF_SIZE];
    uint8_t _read_buf[FOREIGN_MENU_BUF_SIZE];
    RecurciveMutex _write_lock;
};

#endif
