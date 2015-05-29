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

#ifndef _H_TUNNEL_CHANNEL
#define _H_TUNNEL_CHANNEL

#include "common.h"
#include "red_channel.h"
#include "red_client.h"
#include "client_net_socket.h"
#include "platform.h"

#define TUNNEL_CONFIG

#ifdef TUNNEL_CONFIG
class TunnelConfigConnectionIfc;
class TunnelConfigListenerIfc;
#endif

/* channel for tunneling tcp from guest to client network */
typedef struct TunnelService TunnelService;
class TunnelChannel: public RedChannel,
                     public ClientNetSocket::EventHandler {
public:

    TunnelChannel(RedClient& client, uint32_t id);
    virtual ~TunnelChannel();

    virtual void on_socket_message_recv_done(ClientNetSocket& sckt,
                                             ClientNetSocket::ReceiveBuffer& buf);
    virtual void on_socket_message_send_done(ClientNetSocket& sckt);
    virtual void on_socket_fin_recv(ClientNetSocket& sckt);
    virtual void on_socket_disconnect(ClientNetSocket& sckt);

#ifdef TUNNEL_CONFIG
    void add_service(TunnelConfigConnectionIfc& source,
                     uint32_t type, struct in_addr& ip, uint32_t port,
                     std::string& name, std::string& description);
#endif
    static ChannelFactory& Factory();

protected:
    class TunnelSocket;

    virtual void on_disconnect();
    virtual void on_connect();

private:
    void handle_init(RedPeer::InMessage* message);
    void handle_service_ip_map(RedPeer::InMessage* message);

    void handle_socket_open(RedPeer::InMessage* message);
    void handle_socket_fin(RedPeer::InMessage* message);
    void handle_socket_close(RedPeer::InMessage* message);
    void handle_socket_closed_ack(RedPeer::InMessage* message);
    void handle_socket_data(RedPeer::InMessage* message);
    void handle_socket_token(RedPeer::InMessage* message);

    TunnelService* find_service(uint32_t id);
    TunnelService* find_service(struct in_addr& ip);
    TunnelService* find_service(struct in_addr& ip, uint32_t port);

    void send_service(TunnelService& service);
    void destroy_sockets();

private:
    std::vector<TunnelSocket*> _sockets;
    std::list<TunnelService*> _services;
    uint32_t _max_socket_data_size;
    uint32_t _service_id;
    uint32_t _service_group;
#ifdef TUNNEL_CONFIG
    TunnelConfigListenerIfc* _config_listener;
    friend class TunnelConfigListenerIfc;
#endif
};

#ifdef TUNNEL_CONFIG
#ifdef _WIN32
#define TUNNEL_CONFIG_PIPE_NAME "tunnel-config.pipe"
#else
#define TUNNEL_CONFIG_PIPE_NAME "/tmp/tunnel-config.pipe"
#endif

class TunnelConfigConnectionIfc;

class TunnelConfigListenerIfc: public NamedPipe::ListenerInterface {
public:
    TunnelConfigListenerIfc(TunnelChannel& tunnel);
    virtual ~TunnelConfigListenerIfc();
    virtual NamedPipe::ConnectionInterface& create();
    virtual void destroy_connection(TunnelConfigConnectionIfc* conn);

private:
    TunnelChannel& _tunnel;
    NamedPipe::ListenerRef _listener_ref;
    std::list<TunnelConfigConnectionIfc*> _connections;
};

#define TUNNEL_CONFIG_MAX_MSG_LEN 2048
class TunnelConfigConnectionIfc: public NamedPipe::ConnectionInterface {
public:
    TunnelConfigConnectionIfc(TunnelChannel& tunnel,
                              TunnelConfigListenerIfc& listener);
    virtual void bind(NamedPipe::ConnectionRef conn_ref);
    virtual void on_data();
    void send_virtual_ip(struct in_addr& ip);
    NamedPipe::ConnectionRef get_ref() {return _opaque;}
    void handle_msg();

private:
    TunnelChannel& _tunnel;
    TunnelConfigListenerIfc& _listener;
    char _in_msg[TUNNEL_CONFIG_MAX_MSG_LEN]; // <service_type> <ip> <port> <name> <desc>\n
    int _in_msg_len;

    std::string _out_msg;  // <virtual ip>\n
    unsigned _out_msg_pos;
};
#endif

#endif
