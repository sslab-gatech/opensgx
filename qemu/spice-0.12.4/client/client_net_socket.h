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

#ifndef _H_CLIENT_NET_SOCKET
#define _H_CLIENT_NET_SOCKET

#include "platform_utils.h"
#include "common.h"
#include "process_loop.h"

/* interface for connections inside client LAN */

typedef enum {
    SOCKET_STATUS_OPEN,
    SOCKET_STATUS_SENT_FIN,
    SOCKET_STATUS_RECEIVED_FIN,
    SOCKET_STATUS_CLOSED,
} SocketStatus;

class ClientNetSocket: public EventSources::Socket {
public:
    class ReceiveBuffer;
    class SendBuffer;

    class EventHandler;

    class SendException {};
    class ReceiveException {};
    class ShutdownExcpetion {};

    ClientNetSocket(uint16_t id, const struct in_addr& dst_addr, uint16_t dst_port,
                    ProcessLoop& process_loop, ClientNetSocket::EventHandler& event_handler);
    virtual ~ClientNetSocket();

    bool connect(uint32_t recv_tokens);
    void push_disconnect();
    void push_fin();
    void push_send(SendBuffer& buf);
    void add_recv_tokens(uint32_t num_tokens);

    bool is_connected() {return _status != SOCKET_STATUS_CLOSED;}

    inline uint16_t id() {return _id;}
    inline const struct in_addr& local_addr() {return _local_addr;}
    inline uint16_t local_port() {return _local_port;}

    /* EventSources::Socket interface */
    void on_event();
    int get_socket() {return _peer;}

protected:
    virtual ReceiveBuffer& alloc_receive_buffer() = 0;

private:
    void send();
    void receive();

    uint32_t send_buf(const uint8_t* buf, uint32_t size);
    uint32_t receive_buf(uint8_t* buf, uint32_t max_size, bool& shutdown);

    bool can_receive();
    bool can_send();

    void apply_disconnect();
    void apply_guest_fin();

    void close();
    void close_and_tell();

    void handle_client_fin();

    bool during_send();
    void release_wait_send_messages();
    void clear();
    void send_message_done();

private:
    uint16_t _id;
    struct in_addr _local_addr;
    uint16_t _local_port;

    SOCKET _peer;

    ProcessLoop& _process_loop;

    EventHandler& _event_handler;

    uint32_t _num_recv_tokens;

    std::list<SendBuffer*> _send_messages;
    SendBuffer* _send_message;
    uint32_t _send_pos;

    SocketStatus _status;
    bool _fin_pending;
    bool _close_pending;
};

class ClientNetSocket::ReceiveBuffer {
public:
    ReceiveBuffer() {}

    virtual uint8_t* buf() = 0;
    virtual uint32_t buf_max_size() = 0;
    virtual void set_buf_size(uint32_t size) = 0;
    virtual void release_buf() = 0;

protected:
    virtual ~ReceiveBuffer() {}
};

class ClientNetSocket::SendBuffer {
public:
    SendBuffer() {};

    virtual const uint8_t* data() = 0;
    virtual uint32_t size() = 0;
    virtual ClientNetSocket::SendBuffer* ref() = 0;
    virtual void unref() = 0;

protected:
    virtual ~SendBuffer() {}
};

class ClientNetSocket::EventHandler {
public:
    EventHandler() {}
    virtual ~EventHandler() {}
    virtual void on_socket_message_recv_done(ClientNetSocket& sckt, ReceiveBuffer& buf) = 0;
    virtual void on_socket_message_send_done(ClientNetSocket& sckt) = 0;
    virtual void on_socket_disconnect(ClientNetSocket& sckt) = 0;
    virtual void on_socket_fin_recv(ClientNetSocket& sckt) = 0;
};



#endif
