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
#include "controller.h"
#include <spice/controller_prot.h>
#include "cmd_line_parser.h"
#include "menu.h"
#include "utils.h"
#include "debug.h"
#include "platform.h"


#ifdef WIN32
#define PIPE_NAME_MAX_LEN 50
#define PIPE_NAME "SpiceController-%lu"
#endif

Controller::Controller(ControllerInterface *handler)
    : _handler (handler)
    , _exclusive (false)
    , _refs (1)
{
    ASSERT(_handler);
#ifdef WIN32
    char pipe_name[PIPE_NAME_MAX_LEN];
    snprintf(pipe_name, PIPE_NAME_MAX_LEN, PIPE_NAME, Platform::get_process_id());
#else
    const char *pipe_name = getenv("SPICE_XPI_SOCKET");
    if (!pipe_name) {
        LOG_ERROR("Failed to get a controller connection (SPICE_XPI_SOCKET)");
        throw Exception("Failed to get a controller connection (SPICE_XPI_SOCKET)");
    }
#endif
    LOG_INFO("Creating a controller connection %s", pipe_name);
    _pipe = NamedPipe::create(pipe_name, *this);
    if (!_pipe) {
        LOG_ERROR("Failed to create a controller connection");
    }
}

Controller::~Controller()
{
    std::map<NamedPipe::ConnectionRef, ControllerConnection*>::const_iterator conn;
    for (conn = _connections.begin(); conn != _connections.end(); ++conn) {
        conn->second->reset_handler();
        delete conn->second;
    }
    if (_pipe) {
        NamedPipe::destroy(_pipe);
    }
}

NamedPipe::ConnectionInterface& Controller::create()
{
    ControllerConnection *conn = new ControllerConnection(_handler, *this);

    if (conn == NULL) {
        throw Exception("Error allocating a new controller connection");
    }
    return *conn;
}

bool Controller::set_exclusive(bool exclusive)
{
    if (_exclusive) {
        LOG_ERROR("Cannot init controller, an exclusive controller already connected");
        return false;
    }
    if (exclusive && _connections.size() > 1) {
        LOG_ERROR("Cannot init exclusive controller, other controllers already connected");
        return false;
    }
    _exclusive = exclusive;
    return true;
}

void Controller::add_connection(NamedPipe::ConnectionRef conn_ref, ControllerConnection *conn)
{
    _connections[conn_ref] = conn;
    conn->on_data();
}

void Controller::remove_connection(NamedPipe::ConnectionRef conn_ref)
{
    ControllerConnection *conn = _connections[conn_ref];
    _connections.erase(conn_ref);
    _exclusive = false;
    delete conn;
}

void Controller::on_command(NamedPipe::ConnectionRef conn_ref, int32_t id)
{
    ControllerConnection *conn = _connections[conn_ref];
    ControllerValue msg;

    ASSERT(conn);
    msg.base.id = CONTROLLER_MENU_ITEM_CLICK;
    msg.base.size = sizeof(msg);
    msg.value = id;
    conn->write_msg(&msg.base, msg.base.size);
}

ControllerConnection::ControllerConnection(ControllerInterface *handler, Controller& parent)
    : _handler (handler)
    , _parent (parent)
    , _initialized (false)
    , _write_pending (0)
    , _write_pos (_write_buf)
    , _read_pos (_read_buf)
    , _port (-1)
    , _sport (-1)
    , _full_screen (false)
    , _auto_display_res (false)
{
}

ControllerConnection::~ControllerConnection()
{
    if (_opaque != NamedPipe::INVALID_CONNECTION) {
        NamedPipe::destroy_connection(_opaque);
    }
    if (_handler) {
        _handler->clear_menu_items(_opaque);
        _handler->delete_menu();
    }
}

void ControllerConnection::bind(NamedPipe::ConnectionRef conn_ref)
{
    _opaque = conn_ref;
    _parent.add_connection(conn_ref, this);
}

void ControllerConnection::on_data()
{
    if (_write_pending) {
        LOG_INFO("Resume pending write %d", _write_pending);
        if (!write_msg(_write_pos, _write_pending)) {
            return;
        }
    }
    while (read_msgs());
}

bool ControllerConnection::read_msgs()
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
        if (!_initialized && nread >= sizeof(ControllerInitHeader)) {
            ControllerInitHeader *init = (ControllerInitHeader *)pos;
            if (init->magic != CONTROLLER_MAGIC || init->version != CONTROLLER_VERSION) {
                LOG_ERROR("Bad controller init, magic=0x%x version=%u", init->magic,
                          init->version);
                _parent.remove_connection(_opaque);
                return false;
            }
            if (nread < init->size) {
                break;
            }
            if (!handle_init((ControllerInit*)init)) {
                _parent.remove_connection(_opaque);
                return false;
            }
            nread -= init->size;
            pos += init->size;
            _initialized = true;
        }
        if (!_initialized || nread < sizeof(ControllerMsg)) {
            break;
        }
        ControllerMsg *hdr = (ControllerMsg *)pos;
        if (hdr->size < sizeof(ControllerMsg)) {
            LOG_ERROR("Bad controller message, size=%u", hdr->size);
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

bool ControllerConnection::write_msg(const void *buf, int len)
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

bool ControllerConnection::handle_init(ControllerInit *init)
{
    ASSERT(_handler);
    if (init->credentials != 0) {
        LOG_ERROR("Controller menu has wrong credentials 0x%x", init->credentials);
        return false;
    }
    if (!_parent.set_exclusive(init->flags & CONTROLLER_FLAG_EXCLUSIVE)) {
        return false;
    }
    return true;
}

bool ControllerConnection::handle_message(ControllerMsg *hdr)
{
    uint32_t value = ((ControllerValue*)hdr)->value;
    char *data = (char*)((ControllerData*)hdr)->data;

    ASSERT(_handler);
    switch (hdr->id) {
    case CONTROLLER_HOST:
        _host.assign(data);
        break;
    case CONTROLLER_PORT:
        _port = value;
        break;
    case CONTROLLER_SPORT:
        _sport = value;
        break;
    case CONTROLLER_PASSWORD:
        _password.assign(data);
        break;
    case CONTROLLER_SECURE_CHANNELS:
    case CONTROLLER_DISABLE_CHANNELS:
        return set_multi_val(hdr->id, data);
    case CONTROLLER_TLS_CIPHERS:
        return _handler->set_connection_ciphers(data, "Controller");
    case CONTROLLER_CA_FILE:
        return _handler->set_ca_file(data, "Controller");
    case CONTROLLER_HOST_SUBJECT:
        return _handler->set_host_cert_subject(data, "Controller");
    case CONTROLLER_FULL_SCREEN:
        _full_screen = !!(value & CONTROLLER_SET_FULL_SCREEN);
        _handler->set_auto_display_res(!!(value & CONTROLLER_AUTO_DISPLAY_RES));
        break;
    case CONTROLLER_SET_TITLE: {
        std::string str;
        string_printf(str, "%s", data);
        _handler->set_title(str);
        break;
    }
    case CONTROLLER_HOTKEYS:
        _handler->set_hotkeys(data);
        break;
    case CONTROLLER_CONNECT:
        _handler->connect(_host, _port, _sport, _password);
        break;
    case CONTROLLER_SHOW:
        _handler->show_me(_full_screen);
        break;
    case CONTROLLER_HIDE:
        _handler->hide_me();
        break;
    case CONTROLLER_CREATE_MENU:
        return create_menu((char*)data);
    case CONTROLLER_DELETE_MENU:
        _handler->delete_menu();
        break;
#if USE_SMARTCARD
    case CONTROLLER_ENABLE_SMARTCARD:
        _handler->enable_smartcard(value);
        break;
#endif
    case CONTROLLER_SEND_CAD:
    default:
        LOG_ERROR("Ignoring an unknown/SEND_CAD controller message %u", hdr->id);
        return false;
    }
    return true;
}

#ifdef WIN32
#define next_tok(str, delim, state) strtok(str, delim)
#else
#define next_tok(str, delim, state) strtok_r(str, delim, state)
#endif

bool ControllerConnection::create_menu(char* resource)
{
    bool ret = true;
#ifndef WIN32
    char* item_state = 0;
#endif
    char* next_item;
    const char* param;
    const char* text;
    int parent_id;
    int flags;
    int state;
    int id;

    ASSERT(_handler);
    AutoRef<Menu> app_menu(_handler->get_app_menu());
    AutoRef<Menu> menu(new Menu((*app_menu)->get_target(), ""));
    char* item = next_tok(resource, CONTROLLER_MENU_ITEM_DELIMITER, &item_state);
    while (item != NULL) {
        next_item = item + strlen(item) + 1;
        ret = ret && (param = next_tok(item, CONTROLLER_MENU_PARAM_DELIMITER, &item_state)) &&
              sscanf(param, "%d", &parent_id);
        ret = ret && (param = next_tok(NULL, CONTROLLER_MENU_PARAM_DELIMITER, &item_state)) &&
              sscanf(param, "%d", &id);
        ret = ret && (text = next_tok(NULL, CONTROLLER_MENU_PARAM_DELIMITER, &item_state));
        ret = ret && (param = next_tok(NULL, CONTROLLER_MENU_PARAM_DELIMITER, &item_state)) &&
              sscanf(param, "%d", &flags);

        if (!ret) {
            DBG(0, "item parsing failed %s", item);
            break;
        }
        DBG(0, "parent_id=%d, id=%d, text=%s, flags=%d", parent_id, id, text, flags);

        AutoRef<Menu> sub_menu((*menu)->find_sub(parent_id));
        if (!(ret = !!*sub_menu)) {
            DBG(0, "submenu not found %s", item);
            break;
        }

        if (flags & CONTROLLER_MENU_FLAGS_SEPARATOR) {
            (*sub_menu)->add_separator();
        } else if (flags & CONTROLLER_MENU_FLAGS_POPUP) {
            AutoRef<Menu> sub(new Menu((*app_menu)->get_target(), text, id));
            (*sub_menu)->add_sub(sub.release());
        } else {
            state = 0;
            if (flags & (CONTROLLER_MENU_FLAGS_DISABLED | CONTROLLER_MENU_FLAGS_GRAYED)) {
                state |= Menu::MENU_ITEM_STATE_DIM;
            }
            if (flags & CONTROLLER_MENU_FLAGS_CHECKED) {
                state |= Menu::MENU_ITEM_STATE_CHECKED;
            }
            if (id >= SPICE_MENU_INTERNAL_ID_BASE) {
                id = ((id - SPICE_MENU_INTERNAL_ID_BASE) >> SPICE_MENU_INTERNAL_ID_SHIFT) + 1;
            } else {
                id = _handler->get_controller_menu_item_id(_opaque, id);
            }
            (*sub_menu)->add_command(text, id, state);
        }
        item = next_tok(next_item, CONTROLLER_MENU_ITEM_DELIMITER, &item_state);
    }
    if (ret) {
        _handler->set_menu(*menu);
    }
    return ret;
}

bool ControllerConnection::set_multi_val(uint32_t op, char* multi_val)
{
    CmdLineParser parser("", false);
    int id = CmdLineParser::OPTION_FIRST_AVAILABLE;
    char* argv[] = {NULL, (char*)"--set", multi_val};
    char* val;

    ASSERT(_handler);
    parser.add(id, "set", "none", "none", true);
    parser.set_multi(id, ',');
    parser.begin(3, argv);
    if (parser.get_option(&val) != id) {
        return false;
    }
    switch (op) {
    case CONTROLLER_SECURE_CHANNELS:
        _handler->set_channels_security(parser, true, val, "Controller");
        break;
    case CONTROLLER_DISABLE_CHANNELS:
        _handler->set_enable_channels(parser, false, val, "Controller");
        break;
    default:
        DBG(0, "unsupported op %u", op);
        return false;
    }
    return true;
}
