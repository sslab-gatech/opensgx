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
#include "tunnel_channel.h"
#include <spice/protocol.h>

#define SOCKET_WINDOW_SIZE 60
#define SOCKET_TOKENS_TO_SEND 20

/* classes for tunneling msgs without reallocations and memcpy */

class InSocketMessage;
class OutSocketMessage;

class InSocketMessage: public ClientNetSocket::SendBuffer {
public:
    InSocketMessage(RedChannel::CompoundInMessage& full_msg);

    const uint8_t* data();
    uint32_t size();
    ClientNetSocket::SendBuffer* ref();
    void unref();

protected:
    virtual ~InSocketMessage() {}

private:
    int _refs;
    RedChannel::CompoundInMessage& _full_msg;
    SpiceMsgTunnelSocketData* _sckt_msg;
    uint32_t _buf_size;
};

InSocketMessage::InSocketMessage(RedChannel::CompoundInMessage& full_msg)
    : _refs (1)
    , _full_msg (full_msg)
{
    ASSERT(full_msg.type() == SPICE_MSG_TUNNEL_SOCKET_DATA);
    _full_msg.ref();
    _sckt_msg = (SpiceMsgTunnelSocketData*)(_full_msg.data());
    _buf_size = _full_msg.size() - sizeof(SpiceMsgTunnelSocketData);
}

const uint8_t* InSocketMessage::data()
{
    return _sckt_msg->data;
}

uint32_t InSocketMessage::size()
{
    return _buf_size;
}

ClientNetSocket::SendBuffer* InSocketMessage::ref()
{
    _full_msg.ref();
    _refs++;
    return this;
}

void InSocketMessage::unref()
{
    _full_msg.unref();
    if (!--_refs) {
        delete this;
    }
}

class OutSocketMessage: public RedPeer::OutMessage,
                        public RedChannel::OutMessage,
                        public ClientNetSocket::ReceiveBuffer {
public:

    virtual RedPeer::OutMessage& peer_message() { return *this;}
    virtual void release();

    virtual uint8_t* buf() { return _the_buf; };
    virtual uint32_t buf_max_size() {return _max_data_size;}
    virtual void set_buf_size(uint32_t size);
    virtual void release_buf();

    static void init(uint32_t max_data_size);
    static OutSocketMessage& alloc_message(uint16_t id, SpiceMessageMarshallers *marshallers);

    static void clear_free_messages();

protected:
    OutSocketMessage();
    virtual ~OutSocketMessage() {}

private:
    static std::list<OutSocketMessage*> _free_messages;
    static uint32_t _max_data_size;
    uint8_t *_the_buf;
};

std::list<OutSocketMessage*> OutSocketMessage::_free_messages;
uint32_t OutSocketMessage::_max_data_size;

OutSocketMessage::OutSocketMessage()
    : RedPeer::OutMessage(SPICE_MSGC_TUNNEL_SOCKET_DATA)
    , RedChannel::OutMessage()
    , ClientNetSocket::ReceiveBuffer()
{
}

void OutSocketMessage::set_buf_size(uint32_t size)
{
    spice_marshaller_unreserve_space(_marshaller, _max_data_size - size);
}

void OutSocketMessage::release()
{
    OutSocketMessage::_free_messages.push_front(this);
}

void OutSocketMessage::release_buf()
{
    release();
}

void OutSocketMessage::init(uint32_t max_data_size)
{
    _max_data_size = max_data_size;
}

OutSocketMessage& OutSocketMessage::alloc_message(uint16_t id, SpiceMessageMarshallers *marshallers)
{
    OutSocketMessage* ret;
    if (!_free_messages.empty()) {
        ret = _free_messages.front();
        _free_messages.pop_front();
	spice_marshaller_reset(ret->marshaller());
    } else {
        ret = new OutSocketMessage();
    }

    SpiceMsgcTunnelSocketData data;
    data.connection_id = id;
    marshallers->msgc_tunnel_socket_data(ret->marshaller(), &data);
    ret->_the_buf = spice_marshaller_reserve_space(ret->marshaller(), _max_data_size);

    return *ret;
}

void OutSocketMessage::clear_free_messages()
{
    while (!_free_messages.empty()) {
        OutSocketMessage* message = _free_messages.front();
        _free_messages.pop_front();
        delete message;
    }
}

struct TunnelService {
    uint32_t type;
    uint32_t id;
    uint32_t group;
    struct in_addr ip;
    uint32_t port;
    std::string name;
    std::string description;

    struct in_addr virtual_ip;
#ifdef TUNNEL_CONFIG
    TunnelConfigConnectionIfc* service_src;
#endif
};

class TunnelChannel::TunnelSocket: public ClientNetSocket {
public:
    TunnelSocket(uint16_t id, TunnelService& dst_service, ProcessLoop& process_loop,
                 EventHandler & event_handler, SpiceMessageMarshallers *marshallers);
    virtual ~TunnelSocket() {}

    void set_num_tokens(uint32_t tokens) {_num_tokens = tokens;}
    void set_server_num_tokens(uint32_t tokens) {_server_num_tokens = tokens;}
    void set_guest_closed() {_guest_closed = true;}

    uint32_t get_num_tokens() {return _num_tokens;}
    uint32_t get_server_num_tokens() {return _server_num_tokens;}
    bool     get_guest_closed() {return _guest_closed;}

protected:
    virtual ReceiveBuffer& alloc_receive_buffer() {return OutSocketMessage::alloc_message(id(), _marshallers);}

private:
    uint32_t _num_tokens;
    uint32_t _server_num_tokens;
    uint32_t _service_id;
    bool _guest_closed;
    SpiceMessageMarshallers *_marshallers;
};

TunnelChannel::TunnelSocket::TunnelSocket(uint16_t id, TunnelService& dst_service,
                                          ProcessLoop& process_loop,
                                          ClientNetSocket::EventHandler& event_handler,
					  SpiceMessageMarshallers *marshallers)
    : ClientNetSocket(id, dst_service.ip, htons((uint16_t)dst_service.port),
                      process_loop, event_handler)
    , _num_tokens (0)
    , _server_num_tokens (0)
    , _service_id (dst_service.id)
    , _guest_closed (false)
    , _marshallers(marshallers)
{
}

class TunnelHandler: public MessageHandlerImp<TunnelChannel, SPICE_CHANNEL_TUNNEL> {
public:
    TunnelHandler(TunnelChannel& channel)
        : MessageHandlerImp<TunnelChannel, SPICE_CHANNEL_TUNNEL>(channel) {}
};

TunnelChannel::TunnelChannel(RedClient& client, uint32_t id)
    : RedChannel(client, SPICE_CHANNEL_TUNNEL, id, new TunnelHandler(*this))
    , _max_socket_data_size(0)
    , _service_id(0)
    , _service_group(0)
#ifdef TUNNEL_CONFIG
    , _config_listener (NULL)
#endif
{
    TunnelHandler* handler = static_cast<TunnelHandler*>(get_message_handler());

    handler->set_handler(SPICE_MSG_MIGRATE, &TunnelChannel::handle_migrate);
    handler->set_handler(SPICE_MSG_SET_ACK, &TunnelChannel::handle_set_ack);
    handler->set_handler(SPICE_MSG_PING, &TunnelChannel::handle_ping);
    handler->set_handler(SPICE_MSG_WAIT_FOR_CHANNELS, &TunnelChannel::handle_wait_for_channels);

    handler->set_handler(SPICE_MSG_TUNNEL_INIT,
                         &TunnelChannel::handle_init);
    handler->set_handler(SPICE_MSG_TUNNEL_SERVICE_IP_MAP,
                         &TunnelChannel::handle_service_ip_map);
    handler->set_handler(SPICE_MSG_TUNNEL_SOCKET_OPEN,
                         &TunnelChannel::handle_socket_open);
    handler->set_handler(SPICE_MSG_TUNNEL_SOCKET_CLOSE,
                         &TunnelChannel::handle_socket_close);
    handler->set_handler(SPICE_MSG_TUNNEL_SOCKET_FIN,
                         &TunnelChannel::handle_socket_fin);
    handler->set_handler(SPICE_MSG_TUNNEL_SOCKET_TOKEN,
                         &TunnelChannel::handle_socket_token);
    handler->set_handler(SPICE_MSG_TUNNEL_SOCKET_CLOSED_ACK,
                         &TunnelChannel::handle_socket_closed_ack);
    handler->set_handler(SPICE_MSG_TUNNEL_SOCKET_DATA,
                         &TunnelChannel::handle_socket_data);
}

TunnelChannel::~TunnelChannel()
{
    destroy_sockets();
    OutSocketMessage::clear_free_messages();
}

void TunnelChannel::handle_init(RedPeer::InMessage* message)
{
    SpiceMsgTunnelInit* init_msg = (SpiceMsgTunnelInit*)message->data();
    _max_socket_data_size = init_msg->max_socket_data_size;
    OutSocketMessage::init(_max_socket_data_size);
    _sockets.resize(init_msg->max_num_of_sockets);
}

void TunnelChannel::send_service(TunnelService& service)
{
    if (service.type != SPICE_TUNNEL_SERVICE_TYPE_IPP &&
        service.type == SPICE_TUNNEL_SERVICE_TYPE_GENERIC) {
        THROW("%s: invalid service type", __FUNCTION__);
    }

    Message* service_msg = new Message(SPICE_MSGC_TUNNEL_SERVICE_ADD);
    SpiceMsgcTunnelAddGenericService add;
    SpiceMarshaller *name_out, *description_out;
    add.id = service.id;
    add.group = service.group;
    add.type = service.type;
    add.port = service.port;

    if (service.type == SPICE_TUNNEL_SERVICE_TYPE_IPP) {
        add.u.ip.type = SPICE_TUNNEL_IP_TYPE_IPv4;
    }

    _marshallers->msgc_tunnel_service_add(service_msg->marshaller(), &add,
                                           &name_out, &description_out);

    spice_marshaller_add(name_out, (uint8_t *)service.name.c_str(), service.name.length() + 1);
    spice_marshaller_add(description_out, (uint8_t *)service.description.c_str(), service.description.length() + 1);

    post_message(service_msg);
}

void TunnelChannel::handle_service_ip_map(RedPeer::InMessage* message)
{
    SpiceMsgTunnelServiceIpMap* service_ip_msg = (SpiceMsgTunnelServiceIpMap*)message->data();
    TunnelService* service = find_service(service_ip_msg->service_id);
    if (!service) {
        THROW("%s: attempt to map non-existing service id=%d", __FUNCTION__,
              service_ip_msg->service_id);
    }

    if (service_ip_msg->virtual_ip.type == SPICE_TUNNEL_IP_TYPE_IPv4) {
        memcpy(&service->virtual_ip.s_addr, service_ip_msg->virtual_ip.data,
               sizeof(SpiceTunnelIPv4));
    } else {
        THROW("unexpected ip type %d", service_ip_msg->virtual_ip.type);
    }
    DBG(0, "service_id=%d (%s), virtual_ip=%s", service->id, service->name.c_str(),
        inet_ntoa(service->virtual_ip));
#ifdef TUNNEL_CONFIG
    service->service_src->send_virtual_ip(service->virtual_ip);
#endif
}

void TunnelChannel::handle_socket_open(RedPeer::InMessage* message)
{
    SpiceMsgTunnelSocketOpen* open_msg = (SpiceMsgTunnelSocketOpen*)message->data();
    TunnelSocket* sckt;
    Message* out_msg;

    if (_sockets[open_msg->connection_id]) {
        THROW("%s: attempt to open an already opened connection id=%d", __FUNCTION__,
              open_msg->connection_id);
    }

    TunnelService* service = find_service(open_msg->service_id);
    if (!service) {
        THROW("%s: attempt to access non-existing service id=%d", __FUNCTION__,
              open_msg->service_id);
    }

    sckt = new TunnelSocket(open_msg->connection_id, *service, get_process_loop(), *this, _marshallers);

    if (sckt->connect(open_msg->tokens)) {
        _sockets[open_msg->connection_id] = sckt;
        out_msg = new Message(SPICE_MSGC_TUNNEL_SOCKET_OPEN_ACK);
        sckt->set_num_tokens(0);
        sckt->set_server_num_tokens(SOCKET_WINDOW_SIZE);
        SpiceMsgcTunnelSocketOpenAck ack;
        ack.connection_id = open_msg->connection_id;
        ack.tokens = SOCKET_WINDOW_SIZE;
        _marshallers->msgc_tunnel_socket_open_ack(out_msg->marshaller(), &ack);
    } else {
        out_msg = new Message(SPICE_MSGC_TUNNEL_SOCKET_OPEN_NACK);
        SpiceMsgcTunnelSocketOpenNack nack;
        nack.connection_id = open_msg->connection_id;
        _marshallers->msgc_tunnel_socket_open_nack(out_msg->marshaller(), &nack);
        delete sckt;
    }

    post_message(out_msg);
}

void TunnelChannel::handle_socket_fin(RedPeer::InMessage* message)
{
    SpiceMsgTunnelSocketFin* fin_msg = (SpiceMsgTunnelSocketFin*)message->data();
    TunnelSocket* sckt = _sockets[fin_msg->connection_id];

    if (!sckt) {
        THROW("%s: fin connection that doesn't exist id=%d", __FUNCTION__, fin_msg->connection_id);
    }

    DBG(0, "guest fin connection_id=%d", fin_msg->connection_id);
    if (sckt->is_connected()) {
        sckt->push_fin();
    }
}

void TunnelChannel::handle_socket_close(RedPeer::InMessage* message)
{
    SpiceMsgTunnelSocketClose* close_msg = (SpiceMsgTunnelSocketClose*)message->data();
    TunnelSocket* sckt = _sockets[close_msg->connection_id];

    if (!sckt) {
        THROW("%s: closing connection that doesn't exist id=%d", __FUNCTION__,
              close_msg->connection_id);
    }
    DBG(0, "guest closed connection_id=%d", close_msg->connection_id);

    sckt->set_guest_closed();

    if (sckt->is_connected()) {
        sckt->push_disconnect();
    } else {
        // close happened in the server side before it received the client
        // close msg. we should ack the server and free the socket
        on_socket_disconnect(*sckt);
    }
}

void TunnelChannel::handle_socket_closed_ack(RedPeer::InMessage* message)
{
    SpiceMsgTunnelSocketClosedAck* close_ack_msg = (SpiceMsgTunnelSocketClosedAck*)message->data();
    TunnelSocket* sckt = _sockets[close_ack_msg->connection_id];
    if (!sckt) {
        THROW("%s: close ack to connection that doesn't exist id=%d", __FUNCTION__,
              close_ack_msg->connection_id);
    }

    if (sckt->is_connected()) {
        THROW("%s: close ack to connection that is not closed id=%d",
              __FUNCTION__, close_ack_msg->connection_id);
    }
    _sockets[sckt->id()] = NULL;
    DBG(0, "guest Acked closed connection_id=%d", close_ack_msg->connection_id);
    delete sckt;
}

void TunnelChannel::handle_socket_data(RedPeer::InMessage* message)
{
    SpiceMsgTunnelSocketData* send_msg = (SpiceMsgTunnelSocketData*)message->data();
    TunnelSocket* sckt = _sockets[send_msg->connection_id];

    if (!sckt) {
        THROW("%s: sending data to connection that doesn't exist id=%d", __FUNCTION__,
              send_msg->connection_id);
    }

    if (!sckt->get_server_num_tokens()) {
        THROW("%s: token violation connectio_id=%d", __FUNCTION__, sckt->id());
    }

    sckt->set_server_num_tokens(sckt->get_server_num_tokens() - 1);

    if (!sckt->is_connected()) {
        // server hasn't handled the close msg yet
        return;
    }

    InSocketMessage* sckt_msg = new InSocketMessage(*(
                                          static_cast<RedChannel::CompoundInMessage*>(message)));
    if (sckt_msg->size() > _max_socket_data_size) {
        THROW("%s: socket data exceeds size limit %d > %d connection_id=%d", __FUNCTION__,
              sckt_msg->size(), _max_socket_data_size, sckt->id());
    }
    sckt->push_send(*sckt_msg);
    sckt_msg->unref();
}

void TunnelChannel::handle_socket_token(RedPeer::InMessage* message)
{
    SpiceMsgTunnelSocketTokens* token_msg = (SpiceMsgTunnelSocketTokens*)message->data();
    TunnelSocket* sckt = _sockets[token_msg->connection_id];

    if (!sckt) {
        THROW("%s: ack connection that doesn't exist id=%d", __FUNCTION__,
              token_msg->connection_id);
    }
    if (!sckt->is_connected()) {
        return;
    }
    sckt->add_recv_tokens(token_msg->num_tokens);
}

void TunnelChannel::on_socket_message_recv_done(ClientNetSocket& sckt,
                                                ClientNetSocket::ReceiveBuffer& buf)
{
    OutSocketMessage* out_msg = static_cast<OutSocketMessage*>(&buf);

    post_message(out_msg);
}

void TunnelChannel::on_socket_fin_recv(ClientNetSocket& sckt)
{
    TunnelChannel::TunnelSocket* tunnel_sckt = static_cast<TunnelChannel::TunnelSocket*>(&sckt);
    Message* out_msg = new Message(SPICE_MSGC_TUNNEL_SOCKET_FIN);
    DBG(0, "FIN from client coonection id=%d", tunnel_sckt->id());
    SpiceMsgcTunnelSocketFin fin;
    fin.connection_id = tunnel_sckt->id();
    _marshallers->msgc_tunnel_socket_fin(out_msg->marshaller(), &fin);
    post_message(out_msg);
}

void TunnelChannel::on_socket_disconnect(ClientNetSocket& sckt)
{
    TunnelChannel::TunnelSocket* tunnel_sckt = static_cast<TunnelChannel::TunnelSocket*>(&sckt);
    Message* out_msg;
    // close initiated by server -> needs ack
    if (tunnel_sckt->get_guest_closed()) {
        DBG(0, "send close ack connection_id=%d", tunnel_sckt->id());
        out_msg = new Message(SPICE_MSGC_TUNNEL_SOCKET_CLOSED_ACK);
        SpiceMsgcTunnelSocketClosedAck ack;
        ack.connection_id = tunnel_sckt->id();
        _marshallers->msgc_tunnel_socket_closed_ack(out_msg->marshaller(), &ack);
        _sockets[tunnel_sckt->id()] = NULL;
        delete &sckt;
    } else { // close initiated by client
        DBG(0, "send close coonection_id=%d", tunnel_sckt->id());
        out_msg = new Message(SPICE_MSGC_TUNNEL_SOCKET_CLOSED);
        SpiceMsgcTunnelSocketClosed closed;
        closed.connection_id = tunnel_sckt->id();
        _marshallers->msgc_tunnel_socket_closed(out_msg->marshaller(), &closed);
    }

    post_message(out_msg);
}

void TunnelChannel::on_socket_message_send_done(ClientNetSocket& sckt)
{
    TunnelChannel::TunnelSocket* tunnel_sckt = static_cast<TunnelChannel::TunnelSocket*>(&sckt);
    uint32_t num_tokens = tunnel_sckt->get_num_tokens();
    num_tokens++;

    if (num_tokens == SOCKET_TOKENS_TO_SEND) {
        Message* out_msg = new Message(SPICE_MSGC_TUNNEL_SOCKET_TOKEN);
        SpiceMsgcTunnelSocketTokens tokens_msg;
        tokens_msg.connection_id = tunnel_sckt->id();
        tokens_msg.num_tokens = num_tokens;
        _marshallers->msgc_tunnel_socket_token(out_msg->marshaller(), &tokens_msg);
        post_message(out_msg);

        tunnel_sckt->set_num_tokens(0);
        tunnel_sckt->set_server_num_tokens(tunnel_sckt->get_server_num_tokens() + num_tokens);

        ASSERT(tunnel_sckt->get_server_num_tokens() <= SOCKET_WINDOW_SIZE);
    } else {
        tunnel_sckt->set_num_tokens(num_tokens);
    }
}

TunnelService* TunnelChannel::find_service(uint32_t id)
{
    for (std::list<TunnelService*>::iterator iter = _services.begin();
            iter != _services.end(); iter++) {
        if ((*iter)->id == id) {
            return *iter;
        }
    }
    return NULL;
}

/* returns the first service with the same ip */
TunnelService* TunnelChannel::find_service(struct in_addr& ip)
{
    for (std::list<TunnelService*>::iterator iter = _services.begin();
            iter != _services.end(); iter++) {
        if ((*iter)->ip.s_addr == ip.s_addr) {
            return *iter;
        }
    }
    return NULL;
}

TunnelService* TunnelChannel::find_service(struct in_addr& ip, uint32_t port)
{
    for (std::list<TunnelService*>::iterator iter = _services.begin();
            iter != _services.end(); iter++) {
        if (((*iter)->ip.s_addr == ip.s_addr) && ((*iter)->port == port)) {
            return *iter;
        }
    }
    return NULL;
}

void TunnelChannel::destroy_sockets()
{
    for (unsigned int i = 0; i < _sockets.size(); i++) {
        if (_sockets[i]) {
            delete _sockets[i];
            _sockets[i] = NULL;
        }
    }
}

#ifdef TUNNEL_CONFIG
void TunnelChannel::on_connect()
{
    _config_listener = new TunnelConfigListenerIfc(*this);
}
#endif

void TunnelChannel::on_disconnect()
{
    destroy_sockets();
    OutSocketMessage::clear_free_messages();
#ifdef TUNNEL_CONFIG
    if (_config_listener) {
        delete _config_listener;
        _config_listener = NULL;
    }
#endif
}

#ifdef TUNNEL_CONFIG
void TunnelChannel::add_service(TunnelConfigConnectionIfc& source,
                                uint32_t type, struct in_addr& ip, uint32_t port,
                                std::string& name, std::string& description)
{
    if (find_service(ip, port)) {
        LOG_WARN("service ip=%s port=%d was already added",
                 inet_ntoa(ip), port);
        return;
    }
    TunnelService* new_service = new TunnelService;
    TunnelService* service_group = find_service(ip);
    new_service->type = type;
    new_service->id = _service_id++;
    if (service_group) {
        if (name != service_group->name) {
            LOG_WARN("service ip=%s port=%d was not added because of inconsistent name for ip",
                     inet_ntoa(ip), port);
            delete new_service;
            return;
        }
        new_service->group = service_group->group;
    } else {
        new_service->group = _service_group++;
    }
    new_service->ip.s_addr = ip.s_addr;
    new_service->port = port;
    new_service->name = name;
    new_service->description = description;
    new_service->service_src = &source;
    _services.push_back(new_service);
    send_service(*new_service);
}

#endif

class TunnelFactory: public ChannelFactory {
public:
    TunnelFactory() : ChannelFactory(SPICE_CHANNEL_TUNNEL) {}
    virtual RedChannel* construct(RedClient& client, uint32_t id)
    {
        return new TunnelChannel(client, id);
    }
};

static TunnelFactory factory;

ChannelFactory& TunnelChannel::Factory()
{
    return factory;
}

#ifdef TUNNEL_CONFIG
class CreatePipeListenerEvent: public SyncEvent {
public:
    CreatePipeListenerEvent(NamedPipe::ListenerInterface& listener_ifc)
        : _listener_ifc (listener_ifc)
    {
    }

    virtual void do_response(AbstractProcessLoop& events_loop)
    {
        _listener_ref =  NamedPipe::create(TUNNEL_CONFIG_PIPE_NAME, _listener_ifc);
    }

    NamedPipe::ListenerRef get_listener() { return _listener_ref;}
private:
    NamedPipe::ListenerInterface& _listener_ifc;
    NamedPipe::ListenerRef _listener_ref;
};

class DestroyPipeListenerEvent: public SyncEvent {
public:
    DestroyPipeListenerEvent(NamedPipe::ListenerRef listener_ref)
        : _listener_ref (listener_ref)
    {
    }

    virtual void do_response(AbstractProcessLoop& events_loop)
    {
        NamedPipe::destroy(_listener_ref);
    }

private:
    NamedPipe::ListenerRef _listener_ref;
};

class DestroyPipeConnectionEvent: public SyncEvent {
public:
    DestroyPipeConnectionEvent(NamedPipe::ConnectionRef ref) : _conn_ref(ref) {}
    virtual void do_response(AbstractProcessLoop& events_loop)
    {
        NamedPipe::destroy_connection(_conn_ref);
    }
private:
    NamedPipe::ConnectionRef _conn_ref;
};

TunnelConfigListenerIfc::TunnelConfigListenerIfc(TunnelChannel& tunnel)
    : _tunnel (tunnel)
{
    AutoRef<CreatePipeListenerEvent> event(new CreatePipeListenerEvent(*this));
    _tunnel.get_client().push_event(*event);
    (*event)->wait();
    _listener_ref = (*event)->get_listener();
}

TunnelConfigListenerIfc::~TunnelConfigListenerIfc()
{
    AutoRef<DestroyPipeListenerEvent> listen_event(new DestroyPipeListenerEvent(_listener_ref));
    _tunnel.get_client().push_event(*listen_event);
    (*listen_event)->wait();
    for (std::list<TunnelConfigConnectionIfc*>::iterator it = _connections.begin();
            it != _connections.end(); ++it) {
        if ((*it)->get_ref() != NamedPipe::INVALID_CONNECTION) {
            AutoRef<DestroyPipeConnectionEvent> conn_event(new DestroyPipeConnectionEvent(
                                                                    (*it)->get_ref()));
            _tunnel.get_client().push_event(*conn_event);
            (*conn_event)->wait();
        }
        delete (*it);
    }
}

NamedPipe::ConnectionInterface& TunnelConfigListenerIfc::create()
{
    DBG(0, "new_connection");
    TunnelConfigConnectionIfc* new_conn = new TunnelConfigConnectionIfc(_tunnel, *this);
    _connections.push_back(new_conn);
    return *new_conn;
}

void TunnelConfigListenerIfc::destroy_connection(TunnelConfigConnectionIfc* conn)
{
    if (conn->get_ref() != NamedPipe::INVALID_CONNECTION) {
        NamedPipe::destroy_connection(conn->get_ref());
    }
    _connections.remove(conn);
    delete conn;
}

TunnelConfigConnectionIfc::TunnelConfigConnectionIfc(TunnelChannel& tunnel,
                                                     TunnelConfigListenerIfc& listener)
    : _tunnel (tunnel)
    , _listener (listener)
    , _in_msg_len (0)
    , _out_msg ("")
    , _out_msg_pos (0)
{
}

void TunnelConfigConnectionIfc::bind(NamedPipe::ConnectionRef conn_ref)
{
    _opaque = conn_ref;
    on_data();
}

void TunnelConfigConnectionIfc::on_data()
{
    if (!_out_msg.empty()) {
        int ret = NamedPipe::write(_opaque, (uint8_t*)_out_msg.c_str() + _out_msg_pos,
                                   _out_msg.length() - _out_msg_pos);
        if (ret == -1) {
            _listener.destroy_connection(this);
            return;
        }
        _out_msg_pos += ret;
        if (_out_msg_pos == _out_msg.length()) {
            _out_msg = "";
            _out_msg_pos = 0;
        }
    } else {
        int ret = NamedPipe::read(_opaque, (uint8_t*)_in_msg + _in_msg_len,
                                  TUNNEL_CONFIG_MAX_MSG_LEN - _in_msg_len);

        if (ret == -1) {
            _listener.destroy_connection(this);
            return;
        }
        _in_msg_len += ret;

        if (_in_msg[_in_msg_len - 1] != '\n') {
            return;
        }
        handle_msg();
        _in_msg_len = 0;
    }
}

void TunnelConfigConnectionIfc::send_virtual_ip(struct in_addr& ip)
{
    _out_msg = inet_ntoa(ip);
    _out_msg += "\n";
    _out_msg_pos = 0;
    on_data();
}

void TunnelConfigConnectionIfc::handle_msg()
{
    std::string space = " \t";
    _in_msg[_in_msg_len - 1] = '\0';
    std::string msg(_in_msg);

    uint32_t service_type;
    struct in_addr ip;
    uint32_t port;
    std::string name;
    std::string desc;

    DBG(0, "msg=%s", _in_msg);
    size_t start_token = 0;
    size_t end_token;

    start_token = msg.find_first_not_of(space);
    end_token = msg.find_first_of(space, start_token);

    if ((end_token - start_token) != 1) {
        THROW("unexpected service type length");
    }
    if (msg[start_token] == '0') {
        service_type = SPICE_TUNNEL_SERVICE_TYPE_GENERIC;
    } else if (msg[start_token] == '1') {
        service_type = SPICE_TUNNEL_SERVICE_TYPE_IPP;
    } else {
        THROW("unexpected service type");
    }

    start_token = msg.find_first_not_of(space, end_token);
    end_token = msg.find_first_of(space, start_token);

    inet_aton(msg.substr(start_token, end_token - start_token).c_str(), &ip);

    start_token = msg.find_first_not_of(space, end_token);
    end_token = msg.find_first_of(space, start_token);

    port = atoi(msg.substr(start_token, end_token - start_token).c_str());

    start_token = msg.find_first_not_of(space, end_token);
    end_token = msg.find_first_of(space, start_token);

    name = msg.substr(start_token, end_token - start_token);

    start_token = msg.find_first_not_of(space, end_token);
    desc = msg.substr(start_token);

    _tunnel.add_service(*this, service_type, ip, port, name, desc);
}

#endif
