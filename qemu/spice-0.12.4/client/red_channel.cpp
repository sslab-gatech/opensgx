/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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
#include "red_channel.h"
#include "red_client.h"
#include "application.h"
#include "debug.h"
#include "utils.h"

#include "openssl/rsa.h"
#include "openssl/evp.h"
#include "openssl/x509.h"

void MigrationDisconnectSrcEvent::response(AbstractProcessLoop& events_loop)
{
    static_cast<RedChannel*>(events_loop.get_owner())->do_migration_disconnect_src();
}

void MigrationConnectTargetEvent::response(AbstractProcessLoop& events_loop)
{
    static_cast<RedChannel*>(events_loop.get_owner())->do_migration_connect_target();
}

RedChannelBase::RedChannelBase(uint8_t type, uint8_t id, const ChannelCaps& common_caps,
                               const ChannelCaps& caps)
    : RedPeer()
    , _type (type)
    , _id (id)
    , _common_caps (common_caps)
    , _caps (caps)
{
}

RedChannelBase::~RedChannelBase()
{
}

static const char *spice_link_error_string(int err)
{
    switch (err) {
        case SPICE_LINK_ERR_OK: return "no error";
        case SPICE_LINK_ERR_ERROR: return "general error";
        case SPICE_LINK_ERR_INVALID_MAGIC: return "invalid magic";
        case SPICE_LINK_ERR_INVALID_DATA: return "invalid data";
        case SPICE_LINK_ERR_VERSION_MISMATCH: return "version mismatch";
        case SPICE_LINK_ERR_NEED_SECURED: return "need secured connection";
        case SPICE_LINK_ERR_NEED_UNSECURED: return "need unsecured connection";
        case SPICE_LINK_ERR_PERMISSION_DENIED: return "permission denied";
        case SPICE_LINK_ERR_BAD_CONNECTION_ID: return "bad connection id";
        case SPICE_LINK_ERR_CHANNEL_NOT_AVAILABLE: return "channel not available";
    }
    return "";
}

void RedChannelBase::link(uint32_t connection_id, const std::string& password,
                          int protocol)
{
    SpiceLinkHeader header;
    SpiceLinkMess link_mess;
    SpiceLinkReply* reply;
    uint32_t link_res;
    uint32_t i;

    BIO *bioKey;
    uint8_t *buffer, *p;
    uint32_t expected_major;

    header.magic = SPICE_MAGIC;
    header.size = sizeof(link_mess);
    if (protocol == 1) {
        /* protocol 1 == major 1, old 0.4 protocol, last active minor */
        expected_major = header.major_version = 1;
        header.minor_version = 3;
    } else if (protocol == 2) {
        /* protocol 2 == current */
        expected_major = header.major_version = SPICE_VERSION_MAJOR;
        header.minor_version = SPICE_VERSION_MINOR;
    } else {
        THROW("unsupported protocol version specified");
    }
    link_mess.connection_id = connection_id;
    link_mess.channel_type = _type;
    link_mess.channel_id = _id;
    link_mess.num_common_caps = get_common_caps().size();
    link_mess.num_channel_caps = get_caps().size();
    link_mess.caps_offset = sizeof(link_mess);
    header.size += (link_mess.num_common_caps + link_mess.num_channel_caps) * sizeof(uint32_t);

    buffer =
        new uint8_t[sizeof(header) + sizeof(link_mess) +
                    _common_caps.size() * sizeof(uint32_t) +
                    _caps.size() * sizeof(uint32_t)];
    p = buffer;

    memcpy(p, (uint8_t*)&header, sizeof(header));
    p += sizeof(header);
    memcpy(p, (uint8_t*)&link_mess, sizeof(link_mess));
    p += sizeof(link_mess);
    for (i = 0; i < _common_caps.size(); i++) {
        *(uint32_t *)p = _common_caps[i];
        p += sizeof(uint32_t);
    }

    for (i = 0; i < _caps.size(); i++) {
        *(uint32_t *)p = _caps[i];
        p += sizeof(uint32_t);
    }

    send(buffer, p - buffer);
    delete [] buffer;

    receive((uint8_t*)&header, sizeof(header));

    if (header.magic != SPICE_MAGIC) {
        THROW_ERR(SPICEC_ERROR_CODE_CONNECT_FAILED, "bad magic");
    }

    if (header.major_version != expected_major) {
        THROW_ERR(SPICEC_ERROR_CODE_VERSION_MISMATCH,
                  "version mismatch: expect %u got %u",
                  expected_major,
                  header.major_version);
    }

    _remote_major = header.major_version;
    _remote_minor = header.minor_version;

    AutoArray<uint8_t> reply_buf(new uint8_t[header.size]);
    receive(reply_buf.get(), header.size);

    reply = (SpiceLinkReply *)reply_buf.get();

    if (reply->error != SPICE_LINK_ERR_OK) {
        THROW_ERR(SPICEC_ERROR_CODE_CONNECT_FAILED, "connect error %u - %s",
                        reply->error, spice_link_error_string(reply->error));
    }

    uint32_t num_caps = reply->num_channel_caps + reply->num_common_caps;
    if ((uint8_t *)(reply + 1) > reply_buf.get() + header.size ||
        (uint8_t *)reply + reply->caps_offset + num_caps * sizeof(uint32_t) >
                                                                    reply_buf.get() + header.size) {
        THROW_ERR(SPICEC_ERROR_CODE_CONNECT_FAILED, "access violation");
    }

    uint32_t *caps = (uint32_t *)((uint8_t *)reply + reply->caps_offset);

    _remote_common_caps.clear();
    for (i = 0; i < reply->num_common_caps; i++, caps++) {
        _remote_common_caps.resize(i + 1);
        _remote_common_caps[i] = *caps;
    }

    _remote_caps.clear();
    for (i = 0; i < reply->num_channel_caps; i++, caps++) {
        _remote_caps.resize(i + 1);
        _remote_caps[i] = *caps;
    }

    bioKey = BIO_new(BIO_s_mem());
    if (bioKey != NULL) {
        EVP_PKEY *pubkey;
        int nRSASize;
        RSA *rsa;

        BIO_write(bioKey, reply->pub_key, SPICE_TICKET_PUBKEY_BYTES);
        pubkey = d2i_PUBKEY_bio(bioKey, NULL);
        rsa = pubkey->pkey.rsa;
        nRSASize = RSA_size(rsa);
        AutoArray<unsigned char> bufEncrypted(new unsigned char[nRSASize]);

        /*
                The use of RSA encryption limit the potential maximum password length.
                for RSA_PKCS1_OAEP_PADDING it is RSA_size(rsa) - 41.
        */
        if (RSA_public_encrypt(password.length() + 1, (unsigned char *)password.c_str(),
                               (uint8_t *)bufEncrypted.get(),
                               rsa, RSA_PKCS1_OAEP_PADDING) > 0) {
            send((uint8_t*)bufEncrypted.get(), nRSASize);
        } else {
            EVP_PKEY_free(pubkey);
            BIO_free(bioKey);
            THROW("could not encrypt password");
        }

        memset(bufEncrypted.get(), 0, nRSASize);
        EVP_PKEY_free(pubkey);
    } else {
        THROW("Could not initiate BIO");
    }

    BIO_free(bioKey);

    receive((uint8_t*)&link_res, sizeof(link_res));
    if (link_res != SPICE_LINK_ERR_OK) {
        int error_code = (link_res == SPICE_LINK_ERR_PERMISSION_DENIED) ?
                                SPICEC_ERROR_CODE_CONNECT_FAILED : SPICEC_ERROR_CODE_CONNECT_FAILED;
        THROW_ERR(error_code, "connect failed %u", link_res);
    }
}

void RedChannelBase::connect(const ConnectionOptions& options, uint32_t connection_id,
                             const char* host, std::string password)
{
    int protocol = options.protocol;

    if (protocol == 0) { /* AUTO, try major 2 first */
        protocol = 2;
    }

 retry:
    try {
        if (options.allow_unsecure()) {
            try {
                RedPeer::connect_unsecure(host, options.unsecure_port);
                link(connection_id, password, protocol);
                return;
            } catch (Exception& e) {
                // On protocol version mismatch, don't connect_secure with the same version
                if (e.get_error_code() == SPICEC_ERROR_CODE_VERSION_MISMATCH ||
                                                        !options.allow_secure()) {
                    throw;
                }
                RedPeer::close();
            }
        }
        ASSERT(options.allow_secure());
        RedPeer::connect_secure(options, host);
        link(connection_id, password, protocol);
    } catch (Exception& e) {
        // On protocol version mismatch, retry with older version
        if (e.get_error_code() == SPICEC_ERROR_CODE_VERSION_MISMATCH &&
                                 protocol == 2 && options.protocol == 0) {
            RedPeer::cleanup();
            protocol = 1;
            goto retry;
        }
        throw;
    }
}

void RedChannelBase::set_capability(ChannelCaps& caps, uint32_t cap)
{
    uint32_t word_index = cap / 32;

    if (caps.size() < word_index + 1) {
        caps.resize(word_index + 1);
    }
    caps[word_index] |= 1 << (cap % 32);
}

void RedChannelBase::set_common_capability(uint32_t cap)
{
    set_capability(_common_caps, cap);
}

void RedChannelBase::set_capability(uint32_t cap)
{
    set_capability(_caps, cap);
}

bool RedChannelBase::test_capability(const ChannelCaps& caps, uint32_t cap)
{
    uint32_t word_index = cap / 32;

    if (caps.size() < word_index + 1) {
        return false;
    }

    return (caps[word_index] & (1 << (cap % 32))) != 0;
}

bool RedChannelBase::test_common_capability(uint32_t cap)
{
    return test_capability(_remote_common_caps, cap);
}

bool RedChannelBase::test_capability(uint32_t cap)
{
    return test_capability(_remote_caps, cap);
}

void RedChannelBase::swap(RedChannelBase* other)
{
    int tmp_ver;

    RedPeer::swap(other);
    tmp_ver = _remote_major;
    _remote_major = other->_remote_major;
    other->_remote_major = tmp_ver;

    tmp_ver = _remote_minor;
    _remote_minor = other->_remote_minor;
    other->_remote_minor = tmp_ver;
}

SendTrigger::SendTrigger(RedChannel& channel)
    : _channel (channel)
{
}

void SendTrigger::on_event()
{
    _channel.on_send_trigger();
}

SPICE_GNUC_NORETURN void AbortTrigger::on_event()
{
    THROW("abort");
}

RedChannel::RedChannel(RedClient& client, uint8_t type, uint8_t id,
                       RedChannel::MessageHandler* handler,
                       Platform::ThreadPriority worker_priority)
    : RedChannelBase(type, id, ChannelCaps(), ChannelCaps())
    , _marshallers (NULL)
    , _client (client)
    , _state (PASSIVE_STATE)
    , _action (WAIT_ACTION)
    , _error (SPICEC_ERROR_CODE_SUCCESS)
    , _wait_for_threads (true)
    , _socket_in_loop (false)
    , _worker (NULL)
    , _worker_priority (worker_priority)
    , _message_handler (handler)
    , _outgoing_message (NULL)
    , _incomming_header_pos (0)
    , _incomming_message (NULL)
    , _message_ack_count (0)
    , _message_ack_window (0)
    , _loop (this)
    , _send_trigger (*this)
    , _disconnect_stamp (0)
    , _disconnect_reason (SPICE_LINK_ERR_OK)
{
    _loop.add_trigger(_send_trigger);
    _loop.add_trigger(_abort_trigger);
}

RedChannel::~RedChannel()
{
    ASSERT(_state == TERMINATED_STATE || _state == PASSIVE_STATE);
    delete _worker;
}

void* RedChannel::worker_main(void *data)
{
    try {
        RedChannel* channel = static_cast<RedChannel*>(data);
        channel->set_state(DISCONNECTED_STATE);
        Platform::set_thread_priority(NULL, channel->get_worker_priority());
        channel->run();
    } catch (Exception& e) {
        LOG_ERROR("unhandled exception: %s", e.what());
    } catch (std::exception& e) {
        LOG_ERROR("unhandled exception: %s", e.what());
    } catch (...) {
        LOG_ERROR("unhandled exception");
    }
    return NULL;
}

void RedChannel::post_message(RedChannel::OutMessage* message)
{
    Lock lock(_outgoing_lock);
    _outgoing_messages.push_back(message);
    lock.unlock();
    _send_trigger.trigger();
}

RedPeer::CompoundInMessage *RedChannel::receive()
{
    CompoundInMessage *message = RedChannelBase::receive();
    on_message_received();
    return message;
}

RedChannel::OutMessage* RedChannel::get_outgoing_message()
{
    if (_state != CONNECTED_STATE || _outgoing_messages.empty()) {
        return NULL;
    }
    RedChannel::OutMessage* message = _outgoing_messages.front();
    _outgoing_messages.pop_front();
    return message;
}

class AutoMessage {
public:
    AutoMessage(RedChannel::OutMessage* message) : _message (message) {}
    ~AutoMessage() {if (_message) _message->release();}
    void set(RedChannel::OutMessage* message) { _message = message;}
    RedChannel::OutMessage* get() { return _message;}
    RedChannel::OutMessage* release();

private:
    RedChannel::OutMessage* _message;
};

RedChannel::OutMessage* AutoMessage::release()
{
    RedChannel::OutMessage* ret = _message;
    _message = NULL;
    return ret;
}

void RedChannel::start()
{
    ASSERT(!_worker);
    _worker = new Thread(RedChannel::worker_main, this);
    Lock lock(_state_lock);
    while (_state == PASSIVE_STATE) {
        _state_cond.wait(lock);
    }
}

void RedChannel::set_state(int state)
{
    Lock lock(_state_lock);
    _state = state;
    _state_cond.notify_all();
}

void RedChannel::connect()
{
    Lock lock(_action_lock);

    if (_state != DISCONNECTED_STATE && _state != PASSIVE_STATE) {
        return;
    }
    _action = CONNECT_ACTION;
    _action_cond.notify_one();
}

void RedChannel::disconnect()
{
    clear_outgoing_messages();

    Lock lock(_action_lock);
    if (_state != CONNECTING_STATE && _state != CONNECTED_STATE) {
        return;
    }
    _action = DISCONNECT_ACTION;
    RedPeer::disconnect();
    _action_cond.notify_one();
}

void RedChannel::disconnect_migration_src()
{
    clear_outgoing_messages();

    Lock lock(_action_lock);
    if (_state == CONNECTING_STATE || _state == CONNECTED_STATE) {
        AutoRef<MigrationDisconnectSrcEvent> migrate_event(new MigrationDisconnectSrcEvent());
        _loop.push_event(*migrate_event);
    }
}

void RedChannel::connect_migration_target()
{
    LOG_INFO("");
    AutoRef<MigrationConnectTargetEvent> migrate_event(new MigrationConnectTargetEvent());
    _loop.push_event(*migrate_event);
}

void RedChannel::do_migration_disconnect_src()
{
    if (_socket_in_loop) {
        _socket_in_loop = false;
        _loop.remove_socket(*this);
    }

    clear_outgoing_messages();
    if (_outgoing_message) {
        _outgoing_message->release();
        _outgoing_message = NULL;
    }
    _incomming_header_pos = 0;
    if (_incomming_message) {
        _incomming_message->unref();
        _incomming_message = NULL;
    }

    on_disconnect_mig_src();
    get_client().migrate_channel(*this);
    get_client().on_channel_disconnect_mig_src_completed(*this);
}

void RedChannel::do_migration_connect_target()
{
    LOG_INFO("");
    ASSERT(get_client().get_protocol() != 0);
    if (get_client().get_protocol() == 1) {
        _marshallers = spice_message_marshallers_get1();
    } else {
        _marshallers = spice_message_marshallers_get();
    }
    _loop.add_socket(*this);
    _socket_in_loop = true;
    on_connect_mig_target();
    set_state(CONNECTED_STATE);
    on_event();
}

void RedChannel::clear_outgoing_messages()
{
    Lock lock(_outgoing_lock);
    while (!_outgoing_messages.empty()) {
        RedChannel::OutMessage* message = _outgoing_messages.front();
        _outgoing_messages.pop_front();
        message->release();
    }
}

void RedChannel::run()
{
    for (;;) {
        Lock lock(_action_lock);
        if (_action == WAIT_ACTION) {
            _action_cond.wait(lock);
        }
        int action = _action;
        _action = WAIT_ACTION;
        lock.unlock();
        switch (action) {
        case CONNECT_ACTION:
            try {
                get_client().get_sync_info(get_type(), get_id(), _sync_info);
                on_connecting();
                set_state(CONNECTING_STATE);
                ConnectionOptions con_options(_client.get_connection_options(get_type()),
                                              _client.get_port(),
                                              _client.get_sport(),
                                              _client.get_protocol(),
                                              _client.get_host_auth_options(),
                                              _client.get_connection_ciphers());
                RedChannelBase::connect(con_options, _client.get_connection_id(),
                                        _client.get_host().c_str(),
                                        _client.get_password().c_str());
                /* If automatic protocol, remember the first connect protocol type */
                if (_client.get_protocol() == 0) {
                    if (get_peer_major() == 1) {
                        _client.set_protocol(1);
                    } else {
                        /* Major is 2 or unstable high value, use 2 */
                        _client.set_protocol(2);
                    }
                }
                /* Initialize when we know the remote major version */
                if (_client.get_peer_major() == 1) {
                    _marshallers = spice_message_marshallers_get1();
                } else {
                    _marshallers = spice_message_marshallers_get();
                }
                on_connect();
                set_state(CONNECTED_STATE);
                _loop.add_socket(*this);
                _socket_in_loop = true;
                on_event();
                _loop.run();
            } catch (RedPeer::DisconnectedException&) {
                _error = SPICEC_ERROR_CODE_SUCCESS;
            } catch (Exception& e) {
                LOG_WARN("%s", e.what());
                _error = e.get_error_code();
            } catch (std::exception& e) {
                LOG_WARN("%s", e.what());
                _error = SPICEC_ERROR_CODE_ERROR;
            }
            if (_socket_in_loop) {
                _socket_in_loop = false;
                _loop.remove_socket(*this);
            }
            if (_outgoing_message) {
                _outgoing_message->release();
                _outgoing_message = NULL;
            }
            _incomming_header_pos = 0;
            if (_incomming_message) {
                _incomming_message->unref();
                _incomming_message = NULL;
            }
        case DISCONNECT_ACTION:
            close();
            on_disconnect();
            set_state(DISCONNECTED_STATE);
            _client.on_channel_disconnected(*this);
            continue;
        case QUIT_ACTION:
            set_state(TERMINATED_STATE);
            return;
        }
    }
}

bool RedChannel::abort()
{
    clear_outgoing_messages();
    Lock lock(_action_lock);
    if (_state == TERMINATED_STATE) {
        if (_wait_for_threads) {
            _wait_for_threads = false;
            _worker->join();
        }
        return true;
    }

    _action = QUIT_ACTION;
    _action_cond.notify_one();
    lock.unlock();
    RedPeer::disconnect();
    _abort_trigger.trigger();

    for (;;) {
        Lock state_lock(_state_lock);
        if (_state == TERMINATED_STATE) {
            break;
        }
        uint64_t timout = 1000 * 1000 * 100; // 100ms
        if (!_state_cond.timed_wait(state_lock, timout)) {
            return false;
        }
    }
    if (_wait_for_threads) {
        _wait_for_threads = false;
        _worker->join();
    }
    return true;
}

void RedChannel::send_messages()
{
    if (_outgoing_message) {
        return;
    }

    for (;;) {
        Lock lock(_outgoing_lock);
        AutoMessage message(get_outgoing_message());
        if (!message.get()) {
            return;
        }
        RedPeer::OutMessage& peer_message = message.get()->peer_message();
        uint32_t n = send(peer_message);
        if (n != peer_message.message_size()) {
            _outgoing_message = message.release();
            _outgoing_pos = n;
            return;
        }
    }
}

void RedChannel::on_send_trigger()
{
    send_messages();
}

void RedChannel::on_message_received()
{
    if (_message_ack_count && !--_message_ack_count) {
        post_message(new Message(SPICE_MSGC_ACK));
        _message_ack_count = _message_ack_window;
    }
}

void RedChannel::on_message_complition(uint64_t serial)
{
    Lock lock(*_sync_info.lock);
    *_sync_info.message_serial = serial;
    _sync_info.condition->notify_all();
}

void RedChannel::receive_messages()
{
    for (;;) {
        uint32_t n = RedPeer::receive((uint8_t*)&_incomming_header, sizeof(SpiceDataHeader));
        if (n != sizeof(SpiceDataHeader)) {
            _incomming_header_pos = n;
            return;
        }
        AutoRef<CompoundInMessage> message(new CompoundInMessage(_incomming_header.serial,
                                                                 _incomming_header.type,
                                                                 _incomming_header.size,
                                                                 _incomming_header.sub_list));
        n = RedPeer::receive((*message)->data(), (*message)->compound_size());
        if (n != (*message)->compound_size()) {
            _incomming_message = message.release();
            _incomming_message_pos = n;
            return;
        }
        on_message_received();
        _message_handler->handle_message(*(*message));
        on_message_complition((*message)->serial());
    }
}

void RedChannel::on_event()
{
    if (_outgoing_message) {
        RedPeer::OutMessage& peer_message = _outgoing_message->peer_message();

        _outgoing_pos += do_send(peer_message,  _outgoing_pos);
        if (_outgoing_pos == peer_message.message_size()) {
            _outgoing_message->release();
            _outgoing_message = NULL;
        }
    }
    send_messages();

    if (_incomming_header_pos) {
        _incomming_header_pos += RedPeer::receive(((uint8_t*)&_incomming_header) +
                                                 _incomming_header_pos,
                                                 sizeof(SpiceDataHeader) - _incomming_header_pos);
        if (_incomming_header_pos != sizeof(SpiceDataHeader)) {
            return;
        }
        _incomming_header_pos = 0;
        _incomming_message = new CompoundInMessage(_incomming_header.serial,
                                                   _incomming_header.type,
                                                   _incomming_header.size,
                                                   _incomming_header.sub_list);
        _incomming_message_pos = 0;
    }

    if (_incomming_message) {
        _incomming_message_pos += RedPeer::receive(_incomming_message->data() +
                                                  _incomming_message_pos,
                                                  _incomming_message->compound_size() -
                                                  _incomming_message_pos);
        if (_incomming_message_pos != _incomming_message->compound_size()) {
            return;
        }
        AutoRef<CompoundInMessage> message(_incomming_message);
        _incomming_message = NULL;
        on_message_received();
        _message_handler->handle_message(*(*message));
        on_message_complition((*message)->serial());
    }
    receive_messages();
}

void RedChannel::send_migrate_flush_mark()
{
    if (_outgoing_message) {
        RedPeer::OutMessage& peer_message = _outgoing_message->peer_message();
        do_send(peer_message, _outgoing_pos);
        _outgoing_message->release();
        _outgoing_message = NULL;
    }
    Lock lock(_outgoing_lock);
    for (;;) {
        AutoMessage message(get_outgoing_message());
        if (!message.get()) {
            break;
        }
        send(message.get()->peer_message());
    }
    lock.unlock();
    std::auto_ptr<RedPeer::OutMessage> message(new RedPeer::OutMessage(SPICE_MSGC_MIGRATE_FLUSH_MARK));
    send(*message);
}

void RedChannel::handle_migrate(RedPeer::InMessage* message)
{
    DBG(0, "channel type %u id %u", get_type(), get_id());
    _socket_in_loop = false;
    _loop.remove_socket(*this);
    SpiceMsgMigrate* migrate = (SpiceMsgMigrate*)message->data();
    if (migrate->flags & SPICE_MIGRATE_NEED_FLUSH) {
        send_migrate_flush_mark();
    }
    AutoRef<CompoundInMessage> data_message;
    if (migrate->flags & SPICE_MIGRATE_NEED_DATA_TRANSFER) {
        data_message.reset(receive());
    }
    _client.migrate_channel(*this);
    if (migrate->flags & SPICE_MIGRATE_NEED_DATA_TRANSFER) {
        if ((*data_message)->type() != SPICE_MSG_MIGRATE_DATA) {
            THROW("expect SPICE_MSG_MIGRATE_DATA");
        }
        std::auto_ptr<RedPeer::OutMessage> message(new RedPeer::OutMessage(SPICE_MSGC_MIGRATE_DATA));
	spice_marshaller_add(message->marshaller(), (*data_message)->data(), (*data_message)->size());
        send(*message);
    }
    _loop.add_socket(*this);
    _socket_in_loop = true;
    on_migrate();
    set_state(CONNECTED_STATE);
    on_event();
}

void RedChannel::handle_set_ack(RedPeer::InMessage* message)
{
    SpiceMsgSetAck* ack = (SpiceMsgSetAck*)message->data();
    _message_ack_window = _message_ack_count = ack->window;
    Message *response = new Message(SPICE_MSGC_ACK_SYNC);
    SpiceMsgcAckSync sync;
    sync.generation = ack->generation;
    _marshallers->msgc_ack_sync(response->marshaller(), &sync);
    post_message(response);
}

void RedChannel::handle_ping(RedPeer::InMessage* message)
{
    SpiceMsgPing *ping = (SpiceMsgPing *)message->data();
    Message *pong = new Message(SPICE_MSGC_PONG);
    _marshallers->msgc_pong(pong->marshaller(), ping);
    post_message(pong);
}

void RedChannel::handle_disconnect(RedPeer::InMessage* message)
{
    SpiceMsgDisconnect *disconnect = (SpiceMsgDisconnect *)message->data();
    _disconnect_stamp = disconnect->time_stamp;
    _disconnect_reason = disconnect->reason;
}

void RedChannel::handle_notify(RedPeer::InMessage* message)
{
    SpiceMsgNotify *notify = (SpiceMsgNotify *)message->data();
    const char *severity;
    const char *visibility;
    char *message_str = (char *)"";
    const char *message_prefix = "";

    static const char* severity_strings[] = {"info", "warn", "error"};
    static const char* visibility_strings[] = {"!", "!!", "!!!"};


    if (notify->severity > SPICE_NOTIFY_SEVERITY_ERROR) {
        THROW("bad severity");
    }
    severity = severity_strings[notify->severity];

    if (notify->visibilty > SPICE_NOTIFY_VISIBILITY_HIGH) {
        THROW("bad visibility");
    }
    visibility = visibility_strings[notify->visibilty];


    if (notify->message_len) {
        if ((message->size() - sizeof(*notify) < notify->message_len)) {
            THROW("access violation");
        }
        message_str = new char[notify->message_len + 1];
        memcpy(message_str, notify->message, notify->message_len);
        message_str[notify->message_len] = 0;
        message_prefix = ": ";
    }


    LOG_INFO("remote channel %u:%u %s%s #%u%s%s",
             get_type(), get_id(),
             severity, visibility,
             notify->what,
             message_prefix, message_str);
    if (notify->message_len) {
        delete [] message_str;
    }
}

void RedChannel::handle_wait_for_channels(RedPeer::InMessage* message)
{
    SpiceMsgWaitForChannels *wait = (SpiceMsgWaitForChannels *)message->data();
    if (message->size() < sizeof(*wait) + wait->wait_count * sizeof(wait->wait_list[0])) {
        THROW("access violation");
    }
    _client.wait_for_channels(wait->wait_count, wait->wait_list);
}
