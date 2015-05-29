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

#ifndef _H_REDCHANNEL
#define _H_REDCHANNEL

#include "common/client_demarshallers.h"
#include "common/client_marshallers.h"

#include "common.h"
#include "utils.h"
#include "threads.h"
#include "red_peer.h"
#include "platform.h"
#include "process_loop.h"

enum {
    PASSIVE_STATE,
    DISCONNECTED_STATE,
    CONNECTING_STATE,
    CONNECTED_STATE,
    TERMINATED_STATE,
};

enum {
    WAIT_ACTION,
    CONNECT_ACTION,
    DISCONNECT_ACTION,
    QUIT_ACTION,
};

class RedClient;
class RedChannel;

typedef std::vector<uint32_t> ChannelCaps;

class RedChannelBase: public RedPeer {
public:
    RedChannelBase(uint8_t type, uint8_t id, const ChannelCaps& common_caps,
                   const ChannelCaps& caps);

    virtual ~RedChannelBase();

    uint8_t get_type() { return _type;}
    uint8_t get_id() { return _id;}

    void connect(const ConnectionOptions& options, uint32_t connection_id, const char *host,
                 std::string password);

    const ChannelCaps& get_common_caps() { return _common_caps;}
    const ChannelCaps& get_caps() {return _caps;}

     uint32_t get_peer_major() { return _remote_major;}
     uint32_t get_peer_minor() { return _remote_minor;}

     virtual void swap(RedChannelBase* other);

protected:
    void set_common_capability(uint32_t cap);
    void set_capability(uint32_t cap);
    bool test_common_capability(uint32_t cap);
    bool test_capability(uint32_t cap);

private:
    void set_capability(ChannelCaps& caps, uint32_t cap);
    bool test_capability(const ChannelCaps& caps, uint32_t cap);
    void link(uint32_t connection_id, const std::string& password, int protocol);

private:
    uint8_t _type;
    uint8_t _id;

    ChannelCaps _common_caps;
    ChannelCaps _caps;

    ChannelCaps _remote_common_caps;
    ChannelCaps _remote_caps;

    uint32_t _remote_major;
    uint32_t _remote_minor;
};

class SendTrigger: public EventSources::Trigger {
public:
    SendTrigger(RedChannel& channel);

    virtual void on_event();

private:
    RedChannel& _channel;
};

class AbortTrigger: public EventSources::Trigger {
public:
    virtual void on_event();
};

class MigrationDisconnectSrcEvent: public Event {
public:
    virtual void response(AbstractProcessLoop& events_loop);
};

class MigrationConnectTargetEvent: public Event {
public:
    virtual void response(AbstractProcessLoop& events_loop);
};

struct SyncInfo {
    Mutex* lock;
    Condition* condition;
    uint64_t* message_serial;
};

class RedChannel: public RedChannelBase {
public:
    class MessageHandler;
    class OutMessage;

    RedChannel(RedClient& client, uint8_t type, uint8_t id, MessageHandler* handler,
               Platform::ThreadPriority worker_priority = Platform::PRIORITY_NORMAL);
    virtual ~RedChannel();
    void start();

    virtual void connect();
    virtual void disconnect();
    virtual bool abort();

    virtual void disconnect_migration_src();
    virtual void connect_migration_target();

    virtual CompoundInMessage *receive();

    virtual void post_message(RedChannel::OutMessage* message);
    int get_connection_error() { return _error;}
    Platform::ThreadPriority get_worker_priority() { return _worker_priority;}

protected:
    RedClient& get_client() { return _client;}
    ProcessLoop& get_process_loop() { return _loop;}
    MessageHandler* get_message_handler() { return _message_handler.get();}
    virtual void on_connecting() {}
    virtual void on_connect() {}
    virtual void on_disconnect() {}
    virtual void on_migrate() {}
    virtual void on_disconnect_mig_src() { on_disconnect();}
    virtual void on_connect_mig_target() { on_connect();}
    void handle_migrate(RedPeer::InMessage* message);
    void handle_set_ack(RedPeer::InMessage* message);
    void handle_ping(RedPeer::InMessage* message);
    void handle_wait_for_channels(RedPeer::InMessage* message);
    void handle_disconnect(RedPeer::InMessage* message);
    void handle_notify(RedPeer::InMessage* message);

    SpiceMessageMarshallers *_marshallers;

private:
    void set_state(int state);
    void run();
    void send_migrate_flush_mark();
    void send_messages();
    void receive_messages();
    void on_send_trigger();
    virtual void on_event();
    void on_message_received();
    void on_message_complition(uint64_t serial);
    void do_migration_disconnect_src();
    void do_migration_connect_target();

    static void* worker_main(void *);

    RedChannel::OutMessage* get_outgoing_message();
    void clear_outgoing_messages();

private:
    RedClient& _client;
    int _state;
    int _action;
    int _error;
    bool _wait_for_threads;
    bool _socket_in_loop;

    Thread* _worker;
    Platform::ThreadPriority _worker_priority;
    std::auto_ptr<MessageHandler> _message_handler;
    Mutex _state_lock;
    Condition _state_cond;
    Mutex _action_lock;
    Condition _action_cond;
    SyncInfo _sync_info;

    Mutex _outgoing_lock;
    std::list<RedChannel::OutMessage*> _outgoing_messages;
    RedChannel::OutMessage* _outgoing_message;
    uint32_t _outgoing_pos;

    SpiceDataHeader _incomming_header;
    uint32_t _incomming_header_pos;
    RedPeer::CompoundInMessage* _incomming_message;
    uint32_t _incomming_message_pos;

    uint32_t _message_ack_count;
    uint32_t _message_ack_window;

    ProcessLoop _loop;
    SendTrigger _send_trigger;
    AbortTrigger _abort_trigger;

    uint64_t _disconnect_stamp;
    uint64_t _disconnect_reason;

    friend class SendTrigger;
    friend class MigrationDisconnectSrcEvent;
    friend class MigrationConnectTargetEvent;
};


class RedChannel::OutMessage {
public:
    OutMessage() {}
    virtual ~OutMessage() {}

    virtual RedPeer::OutMessage& peer_message() = 0;
    virtual void release() = 0;
};

class Message: public RedChannel::OutMessage, public RedPeer::OutMessage {
public:
    Message(uint32_t type)
        : RedChannel::OutMessage()
        , RedPeer::OutMessage(type)
    {
    }

    virtual RedPeer::OutMessage& peer_message() { return *this;}
    virtual void release() {delete this;}
};


class RedChannel::MessageHandler {
public:
    MessageHandler() {}
    virtual ~MessageHandler() {}
    virtual void handle_message(RedPeer::CompoundInMessage& message) = 0;
};


template <class HandlerClass, unsigned int channel_id>
class MessageHandlerImp: public RedChannel::MessageHandler {
public:
    MessageHandlerImp(HandlerClass& obj);
    ~MessageHandlerImp() { delete [] _handlers; };
    virtual void handle_message(RedPeer::CompoundInMessage& message);
    typedef void (HandlerClass::*Handler)(RedPeer::InMessage* message);
    void set_handler(unsigned int id, Handler handler);

private:
    HandlerClass& _obj;
    unsigned int _max_messages;
    spice_parse_channel_func_t _parser;
    Handler *_handlers;
};

template <class HandlerClass, unsigned int channel_id>
MessageHandlerImp<HandlerClass, channel_id>::MessageHandlerImp(HandlerClass& obj)
    : _obj (obj)
    , _parser (NULL)
{
    /* max_messages is always from current as its larger than for backwards compat */
    spice_get_server_channel_parser(channel_id, &_max_messages);
    _handlers = new Handler[_max_messages + 1];
    memset(_handlers, 0, sizeof(Handler) * (_max_messages + 1));
}

template <class HandlerClass, unsigned int channel_id>
void MessageHandlerImp<HandlerClass, channel_id>::handle_message(RedPeer::CompoundInMessage&
                                                                 message)
{
    uint8_t *msg;
    uint8_t *parsed;
    uint16_t type;
    uint32_t size;
    size_t parsed_size;
    message_destructor_t parsed_free;

    if (_parser == NULL) {
        /* We need to do this lazily rather than at constuction because we
           don't know the major until we've connected */
        if (_obj.get_peer_major() == 1) {
            _parser = spice_get_server_channel_parser1(channel_id, NULL);
        } else {
            _parser = spice_get_server_channel_parser(channel_id, NULL);
        }
    }

    if (message.sub_list()) {
        SpiceSubMessageList *sub_list;
        sub_list = (SpiceSubMessageList *)(message.data() + message.sub_list());
        for (int i = 0; i < sub_list->size; i++) {
            SpiceSubMessage *sub = (SpiceSubMessage *)(message.data() + sub_list->sub_messages[i]);
            msg = (uint8_t *)(sub + 1);
            type = sub->type;
            size = sub->size;
            parsed = _parser(msg, msg + size, type, _obj.get_peer_minor(), &parsed_size, &parsed_free);

            if (parsed == NULL) {
                THROW("failed to parse message type %d", type);
            }

            RedPeer::InMessage sub_message(type, parsed_size, parsed);
            (_obj.*_handlers[type])(&sub_message);

            parsed_free(parsed);
        }
    }

    msg = message.data();
    type = message.type();
    size = message.size();
    parsed = _parser(msg, msg + size, type, _obj.get_peer_minor(), &parsed_size, &parsed_free);

    if (parsed == NULL) {
        THROW("failed to parse message channel %d type %d", channel_id, type);
    }

    RedPeer::InMessage main_message(type, parsed_size, parsed);
    (_obj.*_handlers[type])(&main_message);
    parsed_free(parsed);
}

template <class HandlerClass, unsigned int channel_id>
void MessageHandlerImp<HandlerClass, channel_id>::set_handler(unsigned int id, Handler handler)
{
    if (id > _max_messages) {
        THROW("bad handler id");
    }
    _handlers[id] = handler;
}

#endif
