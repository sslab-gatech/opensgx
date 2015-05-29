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
#include "foreign_menu.h"
#include <spice/foreign_menu_prot.h>
#include "menu.h"
#include "utils.h"
#include "debug.h"
#include "platform.h"

#define PIPE_NAME_MAX_LEN 50

#ifdef WIN32
#define PIPE_NAME "SpiceForeignMenu-%lu"
#elif defined(__i386__) || __SIZEOF_LONG__ == 4
#define PIPE_NAME "/tmp/SpiceForeignMenu-%llu.uds"
#else
#define PIPE_NAME "/tmp/SpiceForeignMenu-%lu.uds"
#endif

ForeignMenu::ForeignMenu(ForeignMenuInterface *handler, bool active)
    : _handler (handler)
    , _active (active)
    , _refs (1)
{
    char pipe_name[PIPE_NAME_MAX_LEN];

    ASSERT(_handler != NULL);
#ifndef WIN32
    const char *p_socket = getenv("SPICE_FOREIGN_MENU_SOCKET");
    if (p_socket) {
        LOG_INFO("Creating a foreign menu connection %s", p_socket);
        _foreign_menu = NamedPipe::create(p_socket, *this);
    } else
#endif
    {
        snprintf(pipe_name, PIPE_NAME_MAX_LEN, PIPE_NAME, Platform::get_process_id());
        LOG_INFO("Creating a foreign menu connection %s", pipe_name);
        _foreign_menu = NamedPipe::create(pipe_name, *this);
    }
    if (!_foreign_menu) {
        LOG_ERROR("Failed to create a foreign menu connection");
    }
}

ForeignMenu::~ForeignMenu()
{
    std::map<NamedPipe::ConnectionRef, ForeignMenuConnection*>::const_iterator conn;
    for (conn = _connections.begin(); conn != _connections.end(); ++conn) {
        conn->second->reset_handler();
        delete conn->second;
    }
    if (_foreign_menu) {
        NamedPipe::destroy(_foreign_menu);
    }
}

NamedPipe::ConnectionInterface& ForeignMenu::create()
{
    ForeignMenuConnection *conn = new ForeignMenuConnection(_handler, *this);

    if (conn == NULL) {
        throw Exception("Error allocating a new foreign menu connection");
    }
    return *conn;
}

void ForeignMenu::add_connection(NamedPipe::ConnectionRef conn_ref, ForeignMenuConnection *conn)
{
    _connections[conn_ref] = conn;
    if (_active) {
        send_active_state(conn, FOREIGN_MENU_APP_ACTIVATED);
    }
    conn->on_data();
}

void ForeignMenu::remove_connection(NamedPipe::ConnectionRef conn_ref)
{
    ForeignMenuConnection *conn = _connections[conn_ref];
    _connections.erase(conn_ref);
    delete conn;
}

void ForeignMenu::add_sub_menus()
{
    std::map<NamedPipe::ConnectionRef, ForeignMenuConnection*>::const_iterator conn;
    for (conn = _connections.begin(); conn != _connections.end(); ++conn) {
        conn->second->add_sub_menu();
    }
}

void ForeignMenu::on_command(NamedPipe::ConnectionRef conn_ref, int32_t id)
{
    ForeignMenuConnection *conn = _connections[conn_ref];
    FrgMenuEvent msg;

    ASSERT(conn);
    msg.base.id = FOREIGN_MENU_ITEM_EVENT;
    msg.base.size = sizeof(FrgMenuEvent);
    msg.id = id;
    msg.action = FOREIGN_MENU_EVENT_CLICK;
    conn->write_msg(&msg.base, msg.base.size);
}

void ForeignMenu::on_activate()
{
    std::map<NamedPipe::ConnectionRef, ForeignMenuConnection*>::const_iterator conn;
    _active = true;
    for (conn = _connections.begin(); conn != _connections.end(); ++conn) {
        send_active_state(conn->second, FOREIGN_MENU_APP_ACTIVATED);
    }
}

void ForeignMenu::on_deactivate()
{
    std::map<NamedPipe::ConnectionRef, ForeignMenuConnection*>::const_iterator conn;
    _active = false;
    for (conn = _connections.begin(); conn != _connections.end(); ++conn) {
        send_active_state(conn->second, FOREIGN_MENU_APP_DEACTIVATED);
    }
}

void ForeignMenu::send_active_state(ForeignMenuConnection *conn, int32_t cmd)
{
    FrgMenuMsg msg;

    ASSERT(conn != NULL);
    msg.id = cmd;
    msg.size = sizeof(FrgMenuMsg);
    conn->write_msg(&msg, msg.size);
}

ForeignMenuConnection::ForeignMenuConnection(ForeignMenuInterface *handler, ForeignMenu& parent)
    : _handler (handler)
    , _parent (parent)
    , _sub_menu (NULL)
    , _initialized (false)
    , _write_pending (0)
    , _write_pos (_write_buf)
    , _read_pos (_read_buf)
{
}

ForeignMenuConnection::~ForeignMenuConnection()
{
    if (_opaque != NamedPipe::INVALID_CONNECTION) {
        NamedPipe::destroy_connection(_opaque);
    }
    if (_handler) {
        AutoRef<Menu> app_menu(_handler->get_app_menu());
        (*app_menu)->remove_sub(_sub_menu);
        _handler->update_menu();
        _handler->clear_menu_items(_opaque);
    }
    if (_sub_menu) {
        _sub_menu->unref();
    }
}

void ForeignMenuConnection::bind(NamedPipe::ConnectionRef conn_ref)
{
    _opaque = conn_ref;
    _parent.add_connection(conn_ref, this);
}

void ForeignMenuConnection::on_data()
{
    if (_write_pending) {
        LOG_INFO("Resume pending write %d", _write_pending);
        if (!write_msg(_write_pos, _write_pending)) {
            return;
        }
    }
    while (read_msgs());
}

bool ForeignMenuConnection::read_msgs()
{
    uint8_t *pos = _read_buf;
    size_t nread = _read_pos - _read_buf;
    int32_t size;

    ASSERT(_handler);
    ASSERT(_opaque != NamedPipe::INVALID_CONNECTION);
    size = NamedPipe::read(_opaque, (uint8_t*)_read_pos, sizeof(_read_buf) - nread);
    if (size == 0) {
        return false;
    } else if (size < 0) {
        LOG_ERROR("Error reading from named pipe %d", size);
        _parent.remove_connection(_opaque);
        return false;
    }
    nread += size;
    while (nread > 0) {
        if (!_initialized && nread >= sizeof(FrgMenuInitHeader)) {
            FrgMenuInitHeader *init = (FrgMenuInitHeader *)pos;
            if (init->magic != FOREIGN_MENU_MAGIC || init->version != FOREIGN_MENU_VERSION) {
                LOG_ERROR("Bad foreign menu init, magic=0x%x version=%u", init->magic,
                          init->version);
                _parent.remove_connection(_opaque);
                return false;
            }
            if (nread < init->size) {
                break;
            }
            if (!handle_init((FrgMenuInit*)init)) {
                _parent.remove_connection(_opaque);
                return false;
            }
            nread -= init->size;
            pos += init->size;
            _initialized = true;
        }
        if (!_initialized || nread < sizeof(FrgMenuMsg)) {
            break;
        }
        FrgMenuMsg *hdr = (FrgMenuMsg *)pos;
        if (hdr->size < sizeof(FrgMenuMsg)) {
            LOG_ERROR("Bad foreign menu message, size=%u", hdr->size);
            _parent.remove_connection(_opaque);
            return false;
        }
        if (nread < hdr->size) {
            break;
        }
        handle_message(hdr);
        nread -= hdr->size;
        pos += hdr->size;
    }
    if (nread > 0 && pos != _read_buf) {
        memmove(_read_buf, pos, nread);
    }
    _read_pos = _read_buf + nread;
    return true;
}

bool ForeignMenuConnection::write_msg(const void *buf, int len)
{
    RecurciveLock lock(_write_lock);
    uint8_t *pos;
    int32_t written = 0;

    ASSERT(_opaque != NamedPipe::INVALID_CONNECTION);
    if (_write_pending && buf != _write_pos) {
        if ((_write_pos + _write_pending + len > _write_buf + sizeof(_write_buf)) &&
                                              !write_msg(_write_pos, _write_pending)) {
            return false;
        }
        if (_write_pending) {
            if (_write_pos + _write_pending + len > _write_buf + sizeof(_write_buf)) {
                DBG(0, "Dropping message, due to insufficient space in write buffer");
                return true;
            }
            memcpy(_write_pos + _write_pending, buf, len);
            _write_pending += len;
        }
    }
    pos = (uint8_t*)buf;
    while (len && (written = NamedPipe::write(_opaque, pos, len)) > 0) {
        pos += written;
        len -= written;
    }
    if (len && written == 0) {
        if (_write_pending) {
            _write_pos = pos;
        } else {
            _write_pos = _write_buf;
            memcpy(_write_buf, pos, len);
        }
        _write_pending = len;
    } else if (written < 0) {
        LOG_ERROR("Error writing to named pipe %d", written);
        _parent.remove_connection(_opaque);
        return false;
    } else {
        _write_pending = 0;
    }
    return true;
}

bool ForeignMenuConnection::handle_init(FrgMenuInit *init)
{
    std::string title = "Untitled";

    ASSERT(_handler);
    if (_sub_menu) {
        LOG_ERROR("Foreign menu already initialized");
        return false;
    }
    if (init->credentials != 0) {
        LOG_ERROR("Foreign menu has wrong credentials 0x%x", init->credentials);
        return false;
    }
    if (init->base.size > offsetof(FrgMenuInit, title)) {
        ((char*)init)[init->base.size - 1] = '\0';
        title = (char*)init->title;
    }
    _sub_menu = new Menu((CommandTarget&)*_handler, title);
    add_sub_menu();
    _handler->update_menu();
    return true;
}

void ForeignMenuConnection::add_sub_menu()
{
    if (_sub_menu) {
        AutoRef<Menu> app_menu(_handler->get_app_menu());
        (*app_menu)->add_sub(_sub_menu);
    }
}

bool ForeignMenuConnection::handle_message(FrgMenuMsg *hdr)
{
    ASSERT(_sub_menu);
    ASSERT(_handler);
    switch (hdr->id) {
    case FOREIGN_MENU_SET_TITLE:
        ((char*)hdr)[hdr->size - 1] = '\0';
        _sub_menu->set_name((char*)((FrgMenuSetTitle*)hdr)->string);
        break;
    case FOREIGN_MENU_ADD_ITEM: {
        FrgMenuAddItem *msg = (FrgMenuAddItem*)hdr;
        ((char*)hdr)[hdr->size - 1] = '\0';
        int id = _handler->get_foreign_menu_item_id(_opaque, msg->id);
        _sub_menu->add_command((char*)msg->string, id, get_item_state(msg->type));
        break;
    }
    case FOREIGN_MENU_REMOVE_ITEM: {
        int id = _handler->get_foreign_menu_item_id(_opaque, ((FrgMenuRmItem*)hdr)->id);
        _sub_menu->remove_command(id);
        _handler->remove_menu_item(id);
        break;
    }
    case FOREIGN_MENU_CLEAR:
        _sub_menu->clear();
        _handler->clear_menu_items(_opaque);
        break;
    case FOREIGN_MENU_MODIFY_ITEM:
    default:
        LOG_ERROR("Ignoring an unknown foreign menu identifier %u", hdr->id);
        return false;
    }
    _handler->update_menu();
    return true;
}

int ForeignMenuConnection::get_item_state(int item_type)
{
    int state = 0;

    if (item_type & FOREIGN_MENU_ITEM_TYPE_CHECKED) {
        state |= Menu::MENU_ITEM_STATE_CHECKED;
    }
    if (item_type & FOREIGN_MENU_ITEM_TYPE_DIM) {
        state |= Menu::MENU_ITEM_STATE_DIM;
    }
    return state;
}
