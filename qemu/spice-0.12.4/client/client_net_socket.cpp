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


    Author:
        yhalperi@redhat.com
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "client_net_socket.h"
#include "debug.h"
#include <spice/error_codes.h>
#include "utils.h"

ClientNetSocket::ClientNetSocket(uint16_t id, const struct in_addr& dst_addr, uint16_t dst_port,
                                 ProcessLoop& process_loop, EventHandler& event_handler)
    : _id (id)
    , _local_addr (dst_addr)
    , _local_port (dst_port)
    , _peer (INVALID_SOCKET)
    , _process_loop (process_loop)
    , _event_handler (event_handler)
    , _num_recv_tokens (0)
    , _send_message (NULL)
    , _send_pos (0)
    , _status (SOCKET_STATUS_CLOSED)
    , _fin_pending (false)
    , _close_pending (false)
{
}

ClientNetSocket::~ClientNetSocket()
{
    close();
}

bool ClientNetSocket::connect(uint32_t recv_tokens)
{
    struct sockaddr_in addr;
    int no_delay;


    ASSERT(_peer == INVALID_SOCKET && _status == SOCKET_STATUS_CLOSED);

    addr.sin_port = _local_port;
    addr.sin_addr.s_addr = _local_addr.s_addr;
    addr.sin_family = AF_INET;

    if ((_peer = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
        int err = sock_error();
        THROW("%s: failed to create socket: %s", __FUNCTION__, sock_err_message(err));
    }

    no_delay = 1;
    if (setsockopt(_peer, IPPROTO_TCP, TCP_NODELAY,
                   (const char*)&no_delay, sizeof(no_delay)) == SOCKET_ERROR) {
        LOG_WARN("set TCP_NODELAY failed");
    }

    LOG_INFO("connect to ip=%s port=%d (connection_id=%d)",
             inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), _id);

    if (::connect(_peer, (struct sockaddr*)&addr, sizeof(sockaddr_in)) == SOCKET_ERROR) {
        int err = sock_error();
        closesocket(_peer);
        _peer = INVALID_SOCKET;
        LOG_INFO("connect to ip=%s port=%d failed %s (connection_id=%d)",
                 inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), sock_err_message(err), _id);
        return false;
    }

    _process_loop.add_socket(*this);
    _status = SOCKET_STATUS_OPEN;
    _num_recv_tokens = recv_tokens;
    return true;
}

void ClientNetSocket::push_disconnect()
{
    if ((_status == SOCKET_STATUS_CLOSED) || _close_pending) {
        THROW("%s: disconnect attempt for disconnected socket %s %d", __FUNCTION__,
              inet_ntoa(_local_addr), ntohs(_local_port));
    }

    _close_pending = true;

    if (!during_send()) {
        close_and_tell();
    }
}

void ClientNetSocket::push_fin()
{
    if ((_status == SOCKET_STATUS_OPEN) || (_status == SOCKET_STATUS_RECEIVED_FIN)) {
        _fin_pending = true;
        if (!during_send()) {
            try {
                apply_guest_fin();
            } catch (ClientNetSocket::ShutdownExcpetion&) {
                close_and_tell();
            }
        }
    } else {
        THROW("%s: unexpected fin connection_id=%d (status=%d)", __FUNCTION__,
              _id, _status);
    }
}

void ClientNetSocket::add_recv_tokens(uint32_t num_tokens)
{
    if ((_status == SOCKET_STATUS_CLOSED) || _close_pending) {
        THROW("%s: ack attempt for disconnected socket connection_id=%d", __FUNCTION__,
              _id);
    }

    _num_recv_tokens += num_tokens;

    // recv might have not been called because tokens weren't available
    if (_num_recv_tokens && (_num_recv_tokens == num_tokens)) {
        if (can_receive()) {
            receive();
        }
    }
}

void ClientNetSocket::push_send(SendBuffer& buf)
{
    if (!can_send()) {
        THROW("%s: unexpected send attempt for connection_id=%d (status = %d)",
              __FUNCTION__, _id, _status);
    }

    if (_fin_pending || _close_pending) {
        THROW("%s: unexpected send attempt for connection_id=%d - shutdown send pending",
              __FUNCTION__, _id);
    }

    _send_messages.push_back(buf.ref());
    send();
}

void ClientNetSocket::on_event()
{
    if (can_send()) {
        send();
    }

    if (!during_send()) {
        if (_close_pending) {
            close_and_tell();
        } else if (_fin_pending) {
            apply_guest_fin();
        }
    }

    if (can_receive()) {
        receive();
    }
}

void ClientNetSocket::apply_guest_fin()
{
    if (_status == SOCKET_STATUS_OPEN) {
        if (shutdown(_peer, SHUT_WR) == SOCKET_ERROR) {
            int err = sock_error();
            LOG_INFO("shutdown in connection_id=%d failed %s", _id, sock_err_message(err));
            throw ClientNetSocket::ShutdownExcpetion();
        }

        _fin_pending = false;
        _status = SOCKET_STATUS_SENT_FIN;
    } else if (_status == SOCKET_STATUS_RECEIVED_FIN) {
        close_and_tell();
    }
}

void ClientNetSocket::handle_client_fin()
{
    if (_status == SOCKET_STATUS_OPEN) {
        _status = SOCKET_STATUS_RECEIVED_FIN;
        _event_handler.on_socket_fin_recv(*this);
    } else if (_status == SOCKET_STATUS_SENT_FIN) {
        close_and_tell();
    }
}

inline bool ClientNetSocket::during_send()
{
    return ((!_send_messages.empty()) || _send_message);
}

inline bool ClientNetSocket::can_send()
{
    return ((_status == SOCKET_STATUS_OPEN) || (_status == SOCKET_STATUS_RECEIVED_FIN));
}

inline bool ClientNetSocket::can_receive()
{
    return ((_status == SOCKET_STATUS_OPEN) || (_status == SOCKET_STATUS_SENT_FIN));
}

void ClientNetSocket::send_message_done()
{
    _send_message->unref();
    _send_message = NULL;
    _send_pos = 0;
    _event_handler.on_socket_message_send_done(*this);
}

void ClientNetSocket::send()
{
    ASSERT(_peer != INVALID_SOCKET);
    try {
        if (_send_message) {
            _send_pos += send_buf(_send_message->data() + _send_pos,
                                  _send_message->size() - _send_pos);

            if (_send_pos != _send_message->size()) {
                return;
            } else {
                send_message_done();
            }
        }

        while (!_send_messages.empty()) {
            _send_message = _send_messages.front();
            _send_messages.pop_front();
            _send_pos = send_buf(_send_message->data(), _send_message->size());
            if (_send_pos != _send_message->size()) {
                return;
            } else {
                send_message_done();
            }
        }
    } catch (ClientNetSocket::SendException&) {
        close_and_tell();
    }
}

uint32_t ClientNetSocket::send_buf(const uint8_t* buf, uint32_t size)
{
    const uint8_t* pos = buf;
    ASSERT(_peer != INVALID_SOCKET);
    while (size) {
        int now;
        if ((now = ::send(_peer, (char*)pos, size, MSG_NOSIGNAL)) == SOCKET_ERROR) {
            int err = sock_error();
            if (err == WOULDBLOCK_ERR) {
                break;
            }

            if (err == INTERRUPTED_ERR) {
                continue;
            }

            LOG_INFO("send in connection_id=%d failed %s", _id, sock_err_message(err));
            throw ClientNetSocket::SendException();
        }
        size -= now;
        pos += now;
    }
    return pos - buf;
}

void ClientNetSocket::receive()
{
    ASSERT(_peer != INVALID_SOCKET);
    bool shutdown;
    while (_num_recv_tokens) {
        ReceiveBuffer& rcv_buf = alloc_receive_buffer();
        uint32_t size;

        try {
            size = receive_buf(rcv_buf.buf(), rcv_buf.buf_max_size(), shutdown);
        } catch (ClientNetSocket::ReceiveException&) {
            rcv_buf.release_buf();
            close_and_tell();
            return;
        }

        if (size) {
            rcv_buf.set_buf_size(size);
            _num_recv_tokens--;
            _event_handler.on_socket_message_recv_done(*this, rcv_buf);
        } else {
            rcv_buf.release_buf();
        }

        if (shutdown) {
            handle_client_fin();
            return;
        }

        if (size < rcv_buf.buf_max_size()) {
            return;
        }
    }
}

uint32_t ClientNetSocket::receive_buf(uint8_t* buf, uint32_t max_size, bool& shutdown)
{
    uint8_t* pos = buf;
    ASSERT(_peer != INVALID_SOCKET);

    shutdown = false;

    while (max_size) {
        int now;
        if ((now = ::recv(_peer, (char*)pos, max_size, 0)) <= 0) {
            if (now == 0) {
                shutdown = true;
                break; // a case where fin is received, but before that, there is a msg
            }

            int err = sock_error();

            if (err == WOULDBLOCK_ERR) {
                break;
            }

            if (err == INTERRUPTED_ERR) {
                continue;
            }

            LOG_INFO("receive in connection_id=%d failed errno=%s", _id, sock_err_message(err));
            throw ClientNetSocket::ReceiveException();
        }
        max_size -= now;
        pos += now;
    }

    return (pos - buf);
}

void ClientNetSocket::release_wait_send_messages()
{
    if (_send_message) {
        _send_message->unref();
        _send_message = NULL;
        _send_pos = 0;
    }

    while (!_send_messages.empty()) {
        _send_messages.front()->unref();
        _send_messages.pop_front();
    }
}

void ClientNetSocket::close()
{
    release_wait_send_messages();
    apply_disconnect();
}

void ClientNetSocket::close_and_tell()
{
    close();
    _event_handler.on_socket_disconnect(*this);
}

void ClientNetSocket::apply_disconnect()
{
    if (_peer != INVALID_SOCKET) {
        _process_loop.remove_socket(*this);
        closesocket(_peer);
        _peer = INVALID_SOCKET;
        LOG_INFO("closing connection_id=%d", _id);
    }
    _status = SOCKET_STATUS_CLOSED;
    _close_pending = false;
    _fin_pending = false;
}
