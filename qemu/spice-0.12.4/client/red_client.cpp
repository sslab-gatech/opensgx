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

#include <algorithm>
#include <math.h>
#include "common/client_marshallers.h"

#include "common.h"
#include "red_client.h"
#include "application.h"
#include "process_loop.h"
#include "utils.h"
#include "debug.h"

#ifndef INFINITY
#define INFINITY HUGE
#endif

#ifdef __GNUC__
typedef struct __attribute__ ((__packed__)) OldRedMigrationBegin {
#else
typedef struct __declspec(align(1)) OldRedMigrationBegin {
#endif
    uint16_t port;
    uint16_t sport;
    char host[0];
} OldRedMigrationBegin;

class MouseModeEvent: public Event {
public:
    MouseModeEvent(RedClient& client)
        : _client (client)
    {
    }

    class SetModeFunc: public ForEachChannelFunc {
    public:
        SetModeFunc(bool capture_mode)
            : _capture_mode (capture_mode)
        {
        }

        virtual bool operator() (RedChannel& channel)
        {
            if (channel.get_type() == SPICE_CHANNEL_DISPLAY) {
                static_cast<DisplayChannel&>(channel).set_capture_mode(_capture_mode);
            }
            return true;
        }

    public:
        bool _capture_mode;
    };

    virtual void response(AbstractProcessLoop& events_loop)
    {
        bool capture_mode = _client.get_mouse_mode() == SPICE_MOUSE_MODE_SERVER;
        if (!capture_mode) {
            _client.get_application().release_mouse_capture();
        }

        SetModeFunc func(capture_mode);
        _client.for_each_channel(func);
    }

private:
    RedClient& _client;
};

uint32_t default_agent_caps[] = {
    (1 << VD_AGENT_CAP_MOUSE_STATE) |
    (1 << VD_AGENT_CAP_MONITORS_CONFIG) |
    (1 << VD_AGENT_CAP_REPLY)
    };

void ClipboardGrabEvent::response(AbstractProcessLoop& events_loop)
{
    static_cast<RedClient*>(events_loop.get_owner())->send_agent_clipboard_message(
        VD_AGENT_CLIPBOARD_GRAB, _type_count * sizeof(uint32_t), _types);
    Platform::set_clipboard_owner(Platform::owner_client);
}

void ClipboardRequestEvent::response(AbstractProcessLoop& events_loop)
{
    if (Platform::get_clipboard_owner() != Platform::owner_guest) {
        LOG_WARN("received clipboard req from client while clipboard is not owned by guest");
        Platform::on_clipboard_notify(VD_AGENT_CLIPBOARD_NONE, NULL, 0);
        return;
    }

    VDAgentClipboardRequest request = {_type};
    static_cast<RedClient*>(events_loop.get_owner())->send_agent_clipboard_message(
        VD_AGENT_CLIPBOARD_REQUEST, sizeof(request), &request);
}

void ClipboardNotifyEvent::response(AbstractProcessLoop& events_loop)
{
    static_cast<RedClient*>(events_loop.get_owner())->send_agent_clipboard_notify_message(
        _type, _data, _size);
}

void ClipboardReleaseEvent::response(AbstractProcessLoop& events_loop)
{
    if (Platform::get_clipboard_owner() != Platform::owner_client)
        return;

    static_cast<RedClient*>(events_loop.get_owner())->send_agent_clipboard_message(
        VD_AGENT_CLIPBOARD_RELEASE, 0, NULL);
}

void MigrateEndEvent::response(AbstractProcessLoop& events_loop)
{
    static_cast<RedClient*>(events_loop.get_owner())->send_migrate_end();
}

Migrate::Migrate(RedClient& client)
    : _client (client)
    , _running (false)
    , _aborting (false)
    , _connected (false)
    , _thread (NULL)
    , _pending_con (0)
    , _protocol (0)
{
}

Migrate::~Migrate()
{
    ASSERT(!_thread);
    delete_channels();
}

void Migrate::delete_channels()
{
    while (!_channels.empty()) {
        MigChannels::iterator iter = _channels.begin();
        delete *iter;
        _channels.erase(iter);
    }
}

void Migrate::clear_channels()
{
    Lock lock(_lock);
    ASSERT(!_running);
    delete_channels();
}

void Migrate::add_channel(MigChannel* channel)
{
    Lock lock(_lock);
    _channels.push_back(channel);
}

void Migrate::swap_peer(RedChannelBase& other)
{
    DBG(0, "channel type %u id %u", other.get_type(), other.get_id());
    try {
        Lock lock(_lock);
        MigChannels::iterator iter = _channels.begin();

        if (_running) {
            THROW("swap and running");
        }

        if (!_connected) {
            THROW("not connected");
        }

        for (; iter != _channels.end(); ++iter) {
            MigChannel* curr = *iter;
            if (curr->get_type() == other.get_type() && curr->get_id() == other.get_id()) {
                if (!curr->is_valid()) {
                    THROW("invalid");
                }
                other.swap(curr);
                curr->set_valid(false);
                if (!--_pending_con) {
                    lock.unlock();
                    _client.set_target(_host.c_str(), _port, _sport, _protocol);
                    abort();
                }
                return;
            }
        }
        THROW("no channel");
    } catch (...) {
        abort();
        throw;
    }
}

void Migrate::connect_one(MigChannel& channel, const RedPeer::ConnectionOptions& options,
                          uint32_t connection_id)
{
    if (_aborting) {
        DBG(0, "aborting");
        THROW("aborting");
    }
    channel.connect(options, connection_id, _host.c_str(), _password);
    ++_pending_con;
    channel.set_valid(true);
    if (_protocol == 0) {
        if (channel.get_peer_major() == 1) {
            _protocol = 1;
        } else {
            _protocol = 2;
        }
    }
}

void Migrate::run()
{
    uint32_t connection_id;
    RedPeer::ConnectionOptions::Type conn_type;

    DBG(0, "");
    try {
        conn_type = _client.get_connection_options(SPICE_CHANNEL_MAIN);
        RedPeer::ConnectionOptions con_opt(conn_type, _port, _sport,
					   _client.get_protocol(),
					   _auth_options, _con_ciphers);
        MigChannels::iterator iter = _channels.begin();
        connection_id = _client.get_connection_id();
        connect_one(**iter, con_opt, connection_id);

        for (++iter; iter != _channels.end(); ++iter) {
            conn_type = _client.get_connection_options((*iter)->get_type());
            con_opt = RedPeer::ConnectionOptions(conn_type, _port, _sport,
                                                 _protocol,
                                                 _auth_options, _con_ciphers);
            connect_one(**iter, con_opt, connection_id);
        }
        _connected = true;
        DBG(0, "connected");
    } catch (...) {
        close_channels();
    }

    Lock lock(_lock);
    _cond.notify_one();
    if (_connected) {
        Message* message = new Message(SPICE_MSGC_MAIN_MIGRATE_CONNECTED);
        _client.post_message(message);
    } else {
        Message* message = new Message(SPICE_MSGC_MAIN_MIGRATE_CONNECT_ERROR);
        _client.post_message(message);
    }
    _running = false;
}

void* Migrate::worker_main(void *data)
{
    Migrate* mig = (Migrate*)data;
    mig->run();
    return NULL;
}

void Migrate::start(const SpiceMsgMainMigrationBegin* migrate)
{
    uint32_t peer_major;
    uint32_t peer_minor;

    DBG(0, "");
    abort();
    peer_major = _client.get_peer_major();
    peer_minor = _client.get_peer_minor();
    if ((peer_major == 1) && (peer_minor < 1)) {
        LOG_INFO("server minor version incompatible for destination authentication"
                 "(missing dest pubkey in SpiceMsgMainMigrationBegin)");
        OldRedMigrationBegin* old_migrate = (OldRedMigrationBegin*)migrate;
        _host.assign(old_migrate->host);
        _port = old_migrate->port ? old_migrate->port : -1;
        _sport = old_migrate->sport ? old_migrate->sport : -1;;
        _auth_options = _client.get_host_auth_options();
    } else {
        _host.assign((char *)migrate->dst_info.host_data);
        _port = migrate->dst_info.port ? migrate->dst_info.port : -1;
        _sport = migrate->dst_info.sport ? migrate->dst_info.sport : -1;
        if ((peer_major == 1) || (peer_major == 2 && peer_minor < 1)) {
            _auth_options.type_flags = SPICE_SSL_VERIFY_OP_PUBKEY;
            _auth_options.host_pubkey.assign(migrate->dst_info.pub_key_data, migrate->dst_info.pub_key_data +
                                             migrate->dst_info.pub_key_size);
        } else {
            _auth_options.type_flags = SPICE_SSL_VERIFY_OP_SUBJECT;
            _auth_options.CA_file =  _client.get_host_auth_options().CA_file;
            if (migrate->dst_info.cert_subject_size != 0) {
                _auth_options.host_subject.assign(migrate->dst_info.cert_subject_data,
                                                  migrate->dst_info.cert_subject_data +
                                                  migrate->dst_info.cert_subject_size);
            }
        }
    }

    _con_ciphers = _client.get_connection_ciphers();
    _password = _client._password;
    Lock lock(_lock);
    _running = true;
    lock.unlock();
    _thread = new Thread(Migrate::worker_main, this);
}

void Migrate::disconnect_channels()
{
    MigChannels::iterator iter = _channels.begin();

    for (; iter != _channels.end(); ++iter) {
        (*iter)->disconnect();
        (*iter)->set_valid(false);
    }
}

void Migrate::close_channels()
{
    MigChannels::iterator iter = _channels.begin();

    for (; iter != _channels.end(); ++iter) {
        (*iter)->close();
        (*iter)->set_valid(false);
        (*iter)->enable();
    }
}

bool Migrate::abort()
{
    Lock lock(_lock);
    if (_aborting) {
        return false;
    }
    _aborting = true;
    for (;;) {
        disconnect_channels();
        if (!_running) {
            break;
        }
        uint64_t timout = 1000 * 1000 * 10; /*10ms*/
        _cond.timed_wait(lock, timout);
    }
    close_channels();
    _pending_con = 0;
    _connected = false;
    _aborting = false;
    if (_thread) {
        _thread->join();
        delete _thread;
        _thread = NULL;
    }
    return true;
}

#define AGENT_TIMEOUT (1000 * 30)

void AgentTimer::response(AbstractProcessLoop& events_loop)
{
    Application* app = static_cast<Application*>(events_loop.get_owner());
    app->deactivate_interval_timer(this);

    LOG_WARN("timeout while waiting for agent response");
    _client->send_main_attach_channels();
}

class MainChannelLoop: public MessageHandlerImp<RedClient, SPICE_CHANNEL_MAIN> {
public:
    MainChannelLoop(RedClient& client): MessageHandlerImp<RedClient, SPICE_CHANNEL_MAIN>(client) {}
};

RedClient::RedClient(Application& application)
    : RedChannel(*this, SPICE_CHANNEL_MAIN, 0, new MainChannelLoop(*this))
    , _application (application)
    , _port (-1)
    , _sport (-1)
    , _protocol (0)
    , _connection_id (0)
    , _mouse_mode (SPICE_MOUSE_MODE_SERVER)
    , _notify_disconnect (false)
    , _auto_display_res (false)
    , _agent_reply_wait_type (VD_AGENT_END_MESSAGE)
    , _aborting (false)
    , _msg_attach_channels_sent(false)
    , _agent_connected (false)
    , _agent_mon_config_sent (false)
    , _agent_disp_config_sent (false)
    , _agent_msg (new VDAgentMessage)
    , _agent_msg_data (NULL)
    , _agent_msg_pos (0)
    , _agent_out_msg (NULL)
    , _agent_out_msg_size (0)
    , _agent_out_msg_pos (0)
    , _agent_tokens (0)
    , _agent_timer (new AgentTimer(this))
    , _agent_caps_size(0)
    , _agent_caps(NULL)
    , _migrate (*this)
    , _glz_window (_glz_debug)
    , _during_migration (false)
{
    Platform::set_clipboard_listener(this);
    MainChannelLoop* message_loop = static_cast<MainChannelLoop*>(get_message_handler());
    uint32_t default_caps_size = SPICE_N_ELEMENTS(default_agent_caps);

    _agent_caps_size = VD_AGENT_CAPS_SIZE;
    ASSERT(VD_AGENT_CAPS_SIZE >= default_caps_size);
    _agent_caps = new uint32_t[_agent_caps_size];
    memcpy(_agent_caps, default_agent_caps, default_caps_size);
    message_loop->set_handler(SPICE_MSG_MIGRATE, &RedClient::handle_migrate);
    message_loop->set_handler(SPICE_MSG_SET_ACK, &RedClient::handle_set_ack);
    message_loop->set_handler(SPICE_MSG_PING, &RedClient::handle_ping);
    message_loop->set_handler(SPICE_MSG_WAIT_FOR_CHANNELS, &RedClient::handle_wait_for_channels);
    message_loop->set_handler(SPICE_MSG_DISCONNECTING, &RedClient::handle_disconnect);
    message_loop->set_handler(SPICE_MSG_NOTIFY, &RedClient::handle_notify);

    message_loop->set_handler(SPICE_MSG_MAIN_MIGRATE_BEGIN, &RedClient::handle_migrate_begin);
    message_loop->set_handler(SPICE_MSG_MAIN_MIGRATE_CANCEL, &RedClient::handle_migrate_cancel);
    message_loop->set_handler(SPICE_MSG_MAIN_MIGRATE_END, &RedClient::handle_migrate_end);
    message_loop->set_handler(SPICE_MSG_MAIN_MIGRATE_SWITCH_HOST,
                              &RedClient::handle_migrate_switch_host);
    message_loop->set_handler(SPICE_MSG_MAIN_INIT, &RedClient::handle_init);
    message_loop->set_handler(SPICE_MSG_MAIN_CHANNELS_LIST, &RedClient::handle_channels);
    message_loop->set_handler(SPICE_MSG_MAIN_MOUSE_MODE, &RedClient::handle_mouse_mode);
    message_loop->set_handler(SPICE_MSG_MAIN_MULTI_MEDIA_TIME, &RedClient::handle_mm_time);

    message_loop->set_handler(SPICE_MSG_MAIN_AGENT_CONNECTED, &RedClient::handle_agent_connected);
    message_loop->set_handler(SPICE_MSG_MAIN_AGENT_DISCONNECTED, &RedClient::handle_agent_disconnected);
    message_loop->set_handler(SPICE_MSG_MAIN_AGENT_DATA, &RedClient::handle_agent_data);
    message_loop->set_handler(SPICE_MSG_MAIN_AGENT_TOKEN, &RedClient::handle_agent_tokens);

    set_capability(SPICE_MAIN_CAP_SEMI_SEAMLESS_MIGRATE);
    start();
}

RedClient::~RedClient()
{
    ASSERT(_channels.empty());
    _application.deactivate_interval_timer(*_agent_timer);
    delete _agent_msg;
    delete[] _agent_caps;
}

void RedClient::set_target(const std::string& host, int port, int sport, int protocol)
{
    if (protocol != get_protocol()) {
        LOG_INFO("old protocol %d, new protocol %d", get_protocol(), protocol);
    }

    _port = port;
    _sport = sport;
    _host.assign(host);
    set_protocol(protocol);
}

void RedClient::push_event(Event* event)
{
    _application.push_event(event);
}

void RedClient::activate_interval_timer(Timer* timer, unsigned int millisec)
{
    _application.activate_interval_timer(timer, millisec);
}

void RedClient::deactivate_interval_timer(Timer* timer)
{
    _application.deactivate_interval_timer(timer);
}

void RedClient::on_connecting()
{
    _notify_disconnect = true;
}

void RedClient::on_connect()
{
    AutoRef<ConnectedEvent> event(new ConnectedEvent());
    push_event(*event);
    _migrate.add_channel(new MigChannel(SPICE_CHANNEL_MAIN, 0, get_common_caps(),
                                        get_caps()));
}

void RedClient::on_disconnect()
{
    _migrate.abort();
    _connection_id = 0;
    _application.deactivate_interval_timer(*_agent_timer);
    // todo: if migration remains not seemless, we shouldn't
    // resend monitors and display setting to the agent
    _agent_mon_config_sent = false;
    _agent_disp_config_sent = false;
    delete[] _agent_msg_data;
    _agent_msg_data = NULL;
    _agent_msg_pos = 0;
    _agent_tokens = 0;
    AutoRef<SyncEvent> sync_event(new SyncEvent());
    get_client().push_event(*sync_event);
    (*sync_event)->wait();
}

void RedClient::on_disconnect_mig_src()
{
    _application.deactivate_interval_timer(*_agent_timer);
    delete[] _agent_msg_data;
    _agent_msg_data = NULL;
    _agent_msg_pos = 0;
    _agent_tokens = 0;
}

void RedClient::delete_channels()
{
    Lock lock(_channels_lock);
    _pending_mig_disconnect_channels.clear();
    while (!_channels.empty()) {
        RedChannel *channel = *_channels.begin();
        _channels.pop_front();
        delete channel;
    }
}

void RedClient::for_each_channel(ForEachChannelFunc& func)
{
    Lock lock(_channels_lock);
    Channels::iterator iter = _channels.begin();
    for (; iter != _channels.end() && func(**iter) ;iter++);
}


void RedClient::on_mouse_capture_trigger(RedScreen& screen)
{
    _application.capture_mouse();
}

RedPeer::ConnectionOptions::Type RedClient::get_connection_options(uint32_t channel_type)
{
    return _con_opt_map[channel_type];
}

void RedClient::connect()
{
    connect(false);
}

void RedClient::connect(bool wait_main_disconnect)
{
    // assumption: read _connection_id is atomic
    if (_connection_id) {
        if (!wait_main_disconnect) {
            return;
        }
    }

    while (!abort_channels() || _connection_id) {
        _application.process_events_queue();
        Platform::msleep(100);
    }

    _pixmap_cache.clear();
    _glz_window.clear();
    memset(_sync_info, 0, sizeof(_sync_info));
    _aborting = false;
    _migrate.clear_channels();
    delete_channels();
    enable();

    _con_opt_map.clear();
    PeerConnectionOptMap::const_iterator iter = _application.get_con_opt_map().begin();
    PeerConnectionOptMap::const_iterator end = _application.get_con_opt_map().end();
    for (; iter != end; iter++) {
        _con_opt_map[(*iter).first] = (*iter).second;
    }

    _host_auth_opt = _application.get_host_auth_opt();
    _con_ciphers = _application.get_connection_ciphers();
    RedChannel::connect();
}

void RedClient::disconnect()
{
    _migrate.abort();
    _msg_attach_channels_sent = false;
    RedChannel::disconnect();
}

void RedClient::disconnect_channels()
{
    Lock lock(_channels_lock);
    Channels::iterator iter = _channels.begin();
    for (; iter != _channels.end(); ++iter) {
        (*iter)->RedPeer::disconnect();
    }
}

void RedClient::on_channel_disconnected(RedChannel& channel)
{
    Lock lock(_notify_lock);
    if (_notify_disconnect) {
        _notify_disconnect = false;
        int connection_error = channel.get_connection_error();
        AutoRef<DisconnectedEvent> disconn_event(new DisconnectedEvent(connection_error));
        push_event(*disconn_event);
    }
    disconnect_channels();
    RedPeer::disconnect();
}

bool RedClient::abort_channels()
{
    Lock lock(_channels_lock);
    Channels::iterator iter = _channels.begin();

    for (; iter != _channels.end(); ++iter) {
        if (!(*iter)->abort()) {
            return false;
        }
    }
    return true;
}

bool RedClient::abort()
{
    if (!_aborting) {
        Platform::set_clipboard_listener(NULL);
        Lock lock(_sync_lock);
        _aborting = true;
        _sync_condition.notify_all();
    }
    _pixmap_cache.abort();
    _glz_window.abort();
    if (RedChannel::abort() && abort_channels()) {
        delete_channels();
        _migrate.abort();
        return true;
    } else {
        return false;
    }
}

void RedClient::handle_migrate_begin(RedPeer::InMessage* message)
{
    LOG_INFO("");
    SpiceMsgMainMigrationBegin* migrate = (SpiceMsgMainMigrationBegin*)message->data();
    //add mig channels
    _migrate.start(migrate);
}

void RedClient::handle_migrate_cancel(RedPeer::InMessage* message)
{
    LOG_INFO("");
    _migrate.abort();
}

void RedClient::handle_migrate_end(RedPeer::InMessage* message)
{
    LOG_INFO("");

    Lock lock(_channels_lock);
    ASSERT(_pending_mig_disconnect_channels.empty());
    Channels::iterator iter = _channels.begin();
    for (; iter != _channels.end(); ++iter) {
        (*iter)->disconnect_migration_src();
        _pending_mig_disconnect_channels.push_back(*iter);
    }
    RedChannel::disconnect_migration_src();
     _pending_mig_disconnect_channels.push_back(this);
     _during_migration = true;
}

void RedClient::on_channel_disconnect_mig_src_completed(RedChannel& channel)
{
    Lock lock(_channels_lock);
    Channels::iterator pending_iter = std::find(_pending_mig_disconnect_channels.begin(),
                                                _pending_mig_disconnect_channels.end(),
                                                &channel);

    LOG_INFO("");
    if (pending_iter == _pending_mig_disconnect_channels.end()) {
        THROW("unexpected channel");
    }

    _pending_mig_disconnect_channels.erase(pending_iter);
    /* clean shared data when all channels have disconnected */
    if (_pending_mig_disconnect_channels.empty()) {
        _pixmap_cache.clear();
        _glz_window.clear();
        memset(_sync_info, 0, sizeof(_sync_info));
        LOG_INFO("calling main to connect and wait for handle_init to tell all the other channels to connect");
        RedChannel::connect_migration_target();
        AutoRef<MigrateEndEvent> mig_end_event(new MigrateEndEvent());
        get_process_loop().push_event(*mig_end_event);
    }
}

void RedClient::send_migrate_end()
{
    Message* message = new Message(SPICE_MSGC_MAIN_MIGRATE_END);
    post_message(message);
}

ChannelFactory* RedClient::find_factory(uint32_t type)
{
    Factorys::iterator iter = _factorys.begin();
    for (; iter != _factorys.end(); ++iter) {
        if ((*iter)->type() == type) {
            return *iter;
        }
    }
    LOG_WARN("no factory for %u", type);
    return NULL;
}

void RedClient::create_channel(uint32_t type, uint32_t id)
{
    ChannelFactory* factory = find_factory(type);
    if (!factory) {
        return;
    }
    RedChannel* channel = factory->construct(*this, id);
    ASSERT(channel);
    Lock lock(_channels_lock);
    _channels.push_back(channel);
    channel->start();
    channel->connect();
    _migrate.add_channel(new MigChannel(type, id, channel->get_common_caps(), channel->get_caps()));
}

void RedClient::send_agent_monitors_config()
{
    AutoRef<MonitorsQuery > qury(new MonitorsQuery());
    push_event(*qury);
    (*qury)->wait();
    if (!(*qury)->success()) {
        THROW(" monitors query failed");
    }

    double min_distance = INFINITY;
    int dx = 0;
    int dy = 0;
    int i;

    std::vector<MonitorInfo>& monitors = (*qury)->get_monitors();
    std::vector<MonitorInfo>::iterator iter = monitors.begin();
    for (; iter != monitors.end(); iter++) {
        double distance = sqrt(pow((double)(*iter).position.x, 2) + pow((double)(*iter).position.y,
                                                                        2));
        if (distance < min_distance) {
            min_distance = distance;
            dx = -(*iter).position.x;
            dy = -(*iter).position.y;
        }
    }

    Message* message = new Message(SPICE_MSGC_MAIN_AGENT_DATA);
    VDAgentMessage* msg = (VDAgentMessage*)
      spice_marshaller_reserve_space(message->marshaller(), sizeof(VDAgentMessage));
    msg->protocol = VD_AGENT_PROTOCOL;
    msg->type = VD_AGENT_MONITORS_CONFIG;
    msg->opaque = 0;
    msg->size = sizeof(VDAgentMonitorsConfig) + monitors.size() * sizeof(VDAgentMonConfig);

    VDAgentMonitorsConfig* mon_config = (VDAgentMonitorsConfig*)
      spice_marshaller_reserve_space(message->marshaller(),
				     sizeof(VDAgentMonitorsConfig) + monitors.size() * sizeof(VDAgentMonConfig));
    mon_config->num_of_monitors = monitors.size();
    mon_config->flags = 0;
    if (Platform::is_monitors_pos_valid()) {
        mon_config->flags = VD_AGENT_CONFIG_MONITORS_FLAG_USE_POS;
    }
    for (iter = monitors.begin(), i = 0; iter != monitors.end(); iter++, i++) {
        mon_config->monitors[i].depth = (*iter).depth;
        mon_config->monitors[i].width = (*iter).size.x;
        mon_config->monitors[i].height = (*iter).size.y;
        mon_config->monitors[i].x = (*iter).position.x + dx;
        mon_config->monitors[i].y = (*iter).position.y + dy;
    }
    ASSERT(_agent_tokens)
    _agent_tokens--;
    post_message(message);
    _agent_mon_config_sent = true;
    _agent_reply_wait_type = VD_AGENT_MONITORS_CONFIG;
}

void RedClient::send_agent_announce_capabilities(bool request)
{
    Message* message = new Message(SPICE_MSGC_MAIN_AGENT_DATA);
    VDAgentMessage* msg = (VDAgentMessage*)
        spice_marshaller_reserve_space(message->marshaller(),
                                       sizeof(VDAgentMessage));
    VDAgentAnnounceCapabilities* caps;

    msg->protocol = VD_AGENT_PROTOCOL;
    msg->type = VD_AGENT_ANNOUNCE_CAPABILITIES;
    msg->opaque = 0;
    msg->size = sizeof(VDAgentAnnounceCapabilities) + VD_AGENT_CAPS_BYTES;

    caps = (VDAgentAnnounceCapabilities*)
        spice_marshaller_reserve_space(message->marshaller(), msg->size);

    caps->request = request;
    memset(caps->caps, 0, VD_AGENT_CAPS_BYTES);
    VD_AGENT_SET_CAPABILITY(caps->caps, VD_AGENT_CAP_MOUSE_STATE);
    VD_AGENT_SET_CAPABILITY(caps->caps, VD_AGENT_CAP_MONITORS_CONFIG);
    VD_AGENT_SET_CAPABILITY(caps->caps, VD_AGENT_CAP_REPLY);
    VD_AGENT_SET_CAPABILITY(caps->caps, VD_AGENT_CAP_DISPLAY_CONFIG);
    VD_AGENT_SET_CAPABILITY(caps->caps, VD_AGENT_CAP_CLIPBOARD_BY_DEMAND);
    ASSERT(_agent_tokens)
    _agent_tokens--;
    post_message(message);
}

void RedClient::send_agent_display_config()
{
    Message* message = new Message(SPICE_MSGC_MAIN_AGENT_DATA);
    VDAgentMessage* msg = (VDAgentMessage*)
        spice_marshaller_reserve_space(message->marshaller(), sizeof(VDAgentMessage));
    VDAgentDisplayConfig* disp_config;

    DBG(0,"");
    msg->protocol = VD_AGENT_PROTOCOL;
    msg->type = VD_AGENT_DISPLAY_CONFIG;
    msg->opaque = 0;
    msg->size = sizeof(VDAgentDisplayConfig);

    disp_config = (VDAgentDisplayConfig*)
        spice_marshaller_reserve_space(message->marshaller(), sizeof(VDAgentDisplayConfig));

    disp_config->flags = 0;
    disp_config->depth = 0;
    if (_display_setting._disable_wallpaper) {
        disp_config->flags |= VD_AGENT_DISPLAY_CONFIG_FLAG_DISABLE_WALLPAPER;
    }

    if (_display_setting._disable_font_smooth) {
        disp_config->flags |= VD_AGENT_DISPLAY_CONFIG_FLAG_DISABLE_FONT_SMOOTH;
    }

    if (_display_setting._disable_animation) {
        disp_config->flags |= VD_AGENT_DISPLAY_CONFIG_FLAG_DISABLE_ANIMATION;
    }

    if (_display_setting._set_color_depth) {
        disp_config->flags |= VD_AGENT_DISPLAY_CONFIG_FLAG_SET_COLOR_DEPTH;
        disp_config->depth = _display_setting._color_depth;
    }

    ASSERT(_agent_tokens)
    _agent_tokens--;
    post_message(message);
    _agent_disp_config_sent = true;

    if (!_display_setting.is_empty()) {
        _agent_reply_wait_type = VD_AGENT_DISPLAY_CONFIG;
    }
}

#define MIN_DISPLAY_PIXMAP_CACHE (1024 * 1024 * 20)
#define MAX_DISPLAY_PIXMAP_CACHE (1024 * 1024 * 80)
#define MIN_MEM_FOR_OTHERS (1024 * 1024 * 40)

// tmp till the pci mem will be shared by the qxls
#define MIN_GLZ_WINDOW_SIZE (1024 * 1024 * 12)
#define MAX_GLZ_WINDOW_SIZE MIN((LZ_MAX_WINDOW_SIZE * 4), 1024 * 1024 * 64)

void RedClient::calc_pixmap_cach_and_glz_window_size(uint32_t display_channels_hint,
                                                     uint32_t pci_mem_hint)
{
#ifdef WIN32
    display_channels_hint = MAX(1, display_channels_hint);
    uint64_t max_cache_size = display_channels_hint * MAX_DISPLAY_PIXMAP_CACHE;
    uint64_t min_cache_size = display_channels_hint * MIN_DISPLAY_PIXMAP_CACHE;

    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);

    if (!GlobalMemoryStatusEx(&mem_status)) {
        THROW("get mem status failed %u", GetLastError());
    }

    //ullTotalPageFile is physical memory plus the size of the page file, minus a small overhead
    uint64_t free_mem = mem_status.ullAvailPageFile;
    if (free_mem < (min_cache_size + MIN_MEM_FOR_OTHERS + MIN_GLZ_WINDOW_SIZE)) {
        THROW_ERR(SPICEC_ERROR_CODE_NOT_ENOUGH_MEMORY, "low memory condition");
    }
    free_mem -= MIN_MEM_FOR_OTHERS;
    _glz_window_size = MIN(MAX_GLZ_WINDOW_SIZE, pci_mem_hint / 2);
    _glz_window_size = (int)MIN(free_mem / 3, _glz_window_size);
    _glz_window_size = MAX(MIN_GLZ_WINDOW_SIZE, _glz_window_size);
    free_mem -= _glz_window_size;
    _pixmap_cache_size = MIN(free_mem, mem_status.ullAvailVirtual);
    _pixmap_cache_size = MIN(free_mem, max_cache_size);
#else
    //for now
    _glz_window_size = (int)MIN(MAX_GLZ_WINDOW_SIZE, pci_mem_hint / 2);
    _glz_window_size = MAX(MIN_GLZ_WINDOW_SIZE, _glz_window_size);
    _pixmap_cache_size = MAX_DISPLAY_PIXMAP_CACHE;
#endif

    _pixmap_cache_size /= 4;
    _glz_window_size /= 4;
}

void RedClient::on_display_mode_change()
{
#ifdef USE_OPENGL
    Lock lock(_channels_lock);
    Channels::iterator iter = _channels.begin();
    for (; iter != _channels.end(); ++iter) {
        if ((*iter)->get_type() == SPICE_CHANNEL_DISPLAY) {
            ((DisplayChannel *)(*iter))->recreate_ogl_context();
        }
    }
#endif
}

void RedClient::do_send_agent_clipboard()
{
    uint32_t size;

    while (_agent_tokens &&
           (size = MIN(VD_AGENT_MAX_DATA_SIZE,
                       _agent_out_msg_size - _agent_out_msg_pos))) {
        Message* message = new Message(SPICE_MSGC_MAIN_AGENT_DATA);
        void* data = spice_marshaller_reserve_space(message->marshaller(), size);
        memcpy(data, (uint8_t*)_agent_out_msg + _agent_out_msg_pos, size);
        _agent_tokens--;
        post_message(message);
        _agent_out_msg_pos += size;
        if (_agent_out_msg_pos == _agent_out_msg_size) {
            delete[] (uint8_t *)_agent_out_msg;
            _agent_out_msg = NULL;
            _agent_out_msg_size = 0;
            _agent_out_msg_pos = 0;
        }
    }
}

void RedClient::send_agent_clipboard_message(uint32_t message_type, uint32_t size, void* data)
{
    if (!_agent_connected)
        return;

    if (!VD_AGENT_HAS_CAPABILITY(_agent_caps, _agent_caps_size,
                                 VD_AGENT_CAP_CLIPBOARD_BY_DEMAND))
        return;

    Message* message = new Message(SPICE_MSGC_MAIN_AGENT_DATA);
    VDAgentMessage* msg = (VDAgentMessage*)
      spice_marshaller_reserve_space(message->marshaller(), sizeof(VDAgentMessage) + size);
    msg->protocol = VD_AGENT_PROTOCOL;
    msg->type = message_type;
    msg->opaque = 0;
    msg->size = size;
    if (size && data) {
        memcpy(msg->data, data, size);
    }
    ASSERT(_agent_tokens)
    _agent_tokens--;
    post_message(message);
}

void RedClient::on_clipboard_grab(uint32_t *types, uint32_t type_count)
{
    AutoRef<ClipboardGrabEvent> event(new ClipboardGrabEvent(types, type_count));
    get_process_loop().push_event(*event);
}

void RedClient::on_clipboard_request(uint32_t type)
{
    AutoRef<ClipboardRequestEvent> event(new ClipboardRequestEvent(type));
    get_process_loop().push_event(*event);
}

void RedClient::on_clipboard_notify(uint32_t type, uint8_t* data, int32_t size)
{
    AutoRef<ClipboardNotifyEvent> event(new ClipboardNotifyEvent(type, data, size));
    get_process_loop().push_event(*event);
}

void RedClient::on_clipboard_release()
{
    AutoRef<ClipboardReleaseEvent> event(new ClipboardReleaseEvent());
    get_process_loop().push_event(*event);
}

void RedClient::send_agent_clipboard_notify_message(uint32_t type, uint8_t *data, uint32_t size)
{
    ASSERT(data || !size);
    if (!_agent_connected) {
        return;
    }
    if (!VD_AGENT_HAS_CAPABILITY(_agent_caps, _agent_caps_size,
                                 VD_AGENT_CAP_CLIPBOARD_BY_DEMAND))
        return;
    if (_agent_out_msg) {
        DBG(0, "clipboard change is already pending");
        return;
    }
    if (Platform::get_clipboard_owner() != Platform::owner_client) {
        LOG_WARN("received clipboard data from client while clipboard is not owned by client");
        type = VD_AGENT_CLIPBOARD_NONE;
        size = 0;
    }
    _agent_out_msg_pos = 0;
    _agent_out_msg_size = sizeof(VDAgentMessage) + sizeof(VDAgentClipboard) + size;
    _agent_out_msg = (VDAgentMessage*)new uint8_t[_agent_out_msg_size];
    _agent_out_msg->protocol = VD_AGENT_PROTOCOL;
    _agent_out_msg->type = VD_AGENT_CLIPBOARD;
    _agent_out_msg->opaque = 0;
    _agent_out_msg->size = sizeof(VDAgentClipboard) + size;
    VDAgentClipboard* clipboard = (VDAgentClipboard*)_agent_out_msg->data;
    clipboard->type = type;
    memcpy(clipboard->data, data, size);
    if (_agent_tokens) {
        do_send_agent_clipboard();
    }
}

void RedClient::set_mouse_mode(uint32_t supported_modes, uint32_t current_mode)
{
    if (current_mode != _mouse_mode) {
        _mouse_mode = current_mode;
        Lock lock(_channels_lock);
        Channels::iterator iter = _channels.begin();
        for (; iter != _channels.end(); ++iter) {
            if ((*iter)->get_type() == SPICE_CHANNEL_CURSOR) {
                ((CursorChannel *)(*iter))->on_mouse_mode_change();
            }
        }
        AutoRef<MouseModeEvent> event(new MouseModeEvent(*this));
        push_event(*event);
    }
    // FIXME: use configured mouse mode (currently, use client mouse mode if supported by server)
    if ((supported_modes & SPICE_MOUSE_MODE_CLIENT) && (current_mode != SPICE_MOUSE_MODE_CLIENT)) {
        Message* message = new Message(SPICE_MSGC_MAIN_MOUSE_MODE_REQUEST);
        SpiceMsgcMainMouseModeRequest mouse_mode_request;
        mouse_mode_request.mode = SPICE_MOUSE_MODE_CLIENT;
	_marshallers->msgc_main_mouse_mode_request(message->marshaller(),
						    &mouse_mode_request);

        post_message(message);
    }
}

/* returns true if we should wait for a response from the agent */
bool RedClient::init_guest_display()
{
    if (_agent_connected) {
        if (_auto_display_res) {
            send_agent_monitors_config();
        }

        if (_auto_display_res || !_display_setting.is_empty()) {
            _application.activate_interval_timer(*_agent_timer, AGENT_TIMEOUT);
        } else {
            return false;
        }
    } else {
        if (_auto_display_res || !_display_setting.is_empty()) {
            LOG_WARN("no agent running, display options have been ignored");
        }
        return false;
    }
    return true;
}

void RedClient::handle_init(RedPeer::InMessage* message)
{
    SpiceMsgMainInit *init = (SpiceMsgMainInit *)message->data();
    LOG_INFO("");
    _connection_id = init->session_id;
    set_mm_time(init->multi_media_time);
    if (!_during_migration) {
        calc_pixmap_cach_and_glz_window_size(init->display_channels_hint, init->ram_hint);
    }
    set_mouse_mode(init->supported_mouse_modes, init->current_mouse_mode);
    _agent_tokens = init->agent_tokens;
    _agent_connected = !!init->agent_connected;
    if (_agent_connected) {
        Message* msg = new Message(SPICE_MSGC_MAIN_AGENT_START);
        SpiceMsgcMainAgentStart agent_start;
        agent_start.num_tokens = ~0;
        _marshallers->msgc_main_agent_start(msg->marshaller(), &agent_start);
        post_message(msg);
        send_agent_announce_capabilities(true);
    }

    if (!_during_migration) {
        if (!init_guest_display()) {
            send_main_attach_channels();
        }
    } else {
        LOG_INFO("connecting all channels after migration");
        Channels::iterator iter = _channels.begin();
        for (; iter != _channels.end(); ++iter) {
            (*iter)->connect_migration_target();
        }
        _during_migration = false;
    }
}

void RedClient::handle_channels(RedPeer::InMessage* message)
{
    SpiceMsgChannels *init = (SpiceMsgChannels *)message->data();
    SpiceChannelId* channels = init->channels;
    for (unsigned int i = 0; i < init->num_of_channels; i++) {
        create_channel(channels[i].type, channels[i].id);
    }
}

void RedClient::handle_mouse_mode(RedPeer::InMessage* message)
{
    SpiceMsgMainMouseMode *mouse_mode = (SpiceMsgMainMouseMode *)message->data();
    set_mouse_mode(mouse_mode->supported_modes, mouse_mode->current_mode);
}

void RedClient::handle_mm_time(RedPeer::InMessage* message)
{
    SpiceMsgMainMultiMediaTime *mm_time = (SpiceMsgMainMultiMediaTime *)message->data();
    set_mm_time(mm_time->time);
}

void RedClient::handle_agent_connected(RedPeer::InMessage* message)
{
    DBG(0, "");
    _agent_connected = true;
    Message* msg = new Message(SPICE_MSGC_MAIN_AGENT_START);
    SpiceMsgcMainAgentStart agent_start;
    agent_start.num_tokens = ~0;
    _marshallers->msgc_main_agent_start(msg->marshaller(), &agent_start);
    post_message(msg);
    send_agent_announce_capabilities(false);

    if (_auto_display_res && !_agent_mon_config_sent) {
        send_agent_monitors_config();
    }
}

void RedClient::handle_agent_disconnected(RedPeer::InMessage* message)
{
    DBG(0, "");
    _agent_connected = false;
}

void RedClient::send_main_attach_channels(void)
{
    if (_msg_attach_channels_sent)
        return;

    post_message(new Message(SPICE_MSGC_MAIN_ATTACH_CHANNELS));
    _msg_attach_channels_sent = true;
}

void RedClient::on_agent_announce_capabilities(
    VDAgentAnnounceCapabilities* caps, uint32_t msg_size)
{
    uint32_t caps_size = VD_AGENT_CAPS_SIZE_FROM_MSG_SIZE(msg_size);

    if (_agent_caps_size != caps_size) {
        delete[] _agent_caps;
        _agent_caps = new uint32_t[caps_size];
        ASSERT(_agent_caps != NULL);
        _agent_caps_size = caps_size;
    }
    memcpy(_agent_caps, caps->caps, sizeof(_agent_caps[0]) * caps_size);

    if (caps->request) {
        send_agent_announce_capabilities(false);
    }
    if (VD_AGENT_HAS_CAPABILITY(caps->caps, caps_size,
            VD_AGENT_CAP_DISPLAY_CONFIG) && !_agent_disp_config_sent) {
        // not sending the color depth through send_agent_monitors_config, since
        // it applies only for attached screens.
        send_agent_display_config();
    } else if (!_auto_display_res) {
        /* some agents don't support monitors/displays agent messages, so
         * we'll never reach on_agent_reply which sends this
         * ATTACH_CHANNELS message which is needed for client startup to go
         * on.
         */
        if (!_display_setting.is_empty()) {
            LOG_WARN("display options have been requested, but the agent doesn't support these options");
        }
        send_main_attach_channels();
        _application.deactivate_interval_timer(*_agent_timer);
    }
}

void RedClient::on_agent_reply(VDAgentReply* reply)
{
    DBG(0, "agent reply type: %d", reply->type);
    switch (reply->error) {
    case VD_AGENT_SUCCESS:
        break;
    case VD_AGENT_ERROR:
        THROW_ERR(SPICEC_ERROR_CODE_AGENT_ERROR, "vdagent error");
    default:
        THROW("unknown vdagent error");
    }
    switch (reply->type) {
    case VD_AGENT_MONITORS_CONFIG:
    case VD_AGENT_DISPLAY_CONFIG:
        if (_agent_reply_wait_type == reply->type) {
            send_main_attach_channels();
            _application.deactivate_interval_timer(*_agent_timer);
            _agent_reply_wait_type = VD_AGENT_END_MESSAGE;
        }
        break;
    default:
        THROW("unexpected vdagent reply type");
    }
}

void RedClient::handle_agent_data(RedPeer::InMessage* message)
{
    uint32_t msg_size = message->size();
    uint8_t* msg_pos = message->data();
    uint32_t n;

    DBG(0, "");
    while (msg_size) {
        if (_agent_msg_pos < sizeof(VDAgentMessage)) {
            n = MIN(sizeof(VDAgentMessage) - _agent_msg_pos, msg_size);
            memcpy((uint8_t*)_agent_msg + _agent_msg_pos, msg_pos, n);
            _agent_msg_pos += n;
            msg_size -= n;
            msg_pos += n;
            if (_agent_msg_pos == sizeof(VDAgentMessage)) {
                DBG(0, "agent msg start: msg_size=%d, protocol=%d, type=%d",
                    _agent_msg->size, _agent_msg->protocol, _agent_msg->type);
                if (_agent_msg->protocol != VD_AGENT_PROTOCOL) {
                    THROW("Invalid protocol %u", _agent_msg->protocol);
                }
                _agent_msg_data = new uint8_t[_agent_msg->size];
            }
        }
        if (_agent_msg_pos >= sizeof(VDAgentMessage)) {
            n = MIN(sizeof(VDAgentMessage) + _agent_msg->size - _agent_msg_pos, msg_size);
            memcpy(_agent_msg_data + _agent_msg_pos - sizeof(VDAgentMessage), msg_pos, n);
            _agent_msg_pos += n;
            msg_size -= n;
            msg_pos += n;
        }
        if (_agent_msg_pos == sizeof(VDAgentMessage) + _agent_msg->size) {
            DBG(0, "agent msg end");
            dispatch_agent_message(_agent_msg, _agent_msg_data);
            delete[] _agent_msg_data;
            _agent_msg_data = NULL;
            _agent_msg_pos = 0;
        }
    }
}

void RedClient::dispatch_agent_message(VDAgentMessage* msg, void* data)
{
    switch (msg->type) {
    case VD_AGENT_ANNOUNCE_CAPABILITIES: {
        on_agent_announce_capabilities((VDAgentAnnounceCapabilities*)data, msg->size);
        break;
    }
    case VD_AGENT_REPLY: {
        on_agent_reply((VDAgentReply*)data);
        break;
    }
    case VD_AGENT_CLIPBOARD: {
        if (Platform::get_clipboard_owner() != Platform::owner_guest) {
            LOG_WARN("received clipboard data from guest while clipboard is not owned by guest");
            Platform::on_clipboard_notify(VD_AGENT_CLIPBOARD_NONE, NULL, 0);
            break;
        }

        VDAgentClipboard* clipboard = (VDAgentClipboard*)data;
        Platform::on_clipboard_notify(clipboard->type, clipboard->data,
                                     msg->size - sizeof(VDAgentClipboard));
        break;
    }
    case VD_AGENT_CLIPBOARD_GRAB:
        Platform::on_clipboard_grab((uint32_t *)data,
                                      msg->size / sizeof(uint32_t));
        break;
    case VD_AGENT_CLIPBOARD_REQUEST:
        if (Platform::get_clipboard_owner() != Platform::owner_client) {
            LOG_WARN("received clipboard req from guest while clipboard is not owned by client");
            on_clipboard_notify(VD_AGENT_CLIPBOARD_NONE, NULL, 0);
            break;
        }

        if (!Platform::on_clipboard_request(((VDAgentClipboardRequest*)data)->type)) {
            on_clipboard_notify(VD_AGENT_CLIPBOARD_NONE, NULL, 0);
        }
        break;
    case VD_AGENT_CLIPBOARD_RELEASE:
        if (Platform::get_clipboard_owner() != Platform::owner_guest) {
            LOG_INFO("received clipboard release from guest while clipboard is not owned by guest");
            break;
        }

        Platform::on_clipboard_release();
        break;
    default:
        DBG(0, "Unsupported message type %u size %u", msg->type, msg->size);
    }
}

void RedClient::handle_agent_tokens(RedPeer::InMessage* message)
{
    SpiceMsgMainAgentTokens *token = (SpiceMsgMainAgentTokens *)message->data();
    _agent_tokens += token->num_tokens;
    if (_agent_out_msg_pos < _agent_out_msg_size) {
        do_send_agent_clipboard();
    }
}

void RedClient::handle_migrate_switch_host(RedPeer::InMessage* message)
{
    SpiceMsgMainMigrationSwitchHost* migrate = (SpiceMsgMainMigrationSwitchHost*)message->data();
    char* host = (char *)migrate->host_data;
    char* subject = NULL;

    if (host[migrate->host_size - 1] != '\0') {
        THROW("host is not a null-terminated string");
    }

    if (migrate->cert_subject_size) {
        subject = (char *)migrate->cert_subject_data;
        if (subject[migrate->cert_subject_size - 1] != '\0') {
            THROW("cert subject is not a null-terminated string");
        }
    }

    AutoRef<SwitchHostEvent> switch_event(new SwitchHostEvent(host,
                                                              migrate->port,
                                                              migrate->sport,
                                                              subject));
    push_event(*switch_event);
}


void RedClient::migrate_channel(RedChannel& channel)
{
    DBG(0, "channel type %u id %u", channel.get_type(), channel.get_id());
    _migrate.swap_peer(channel);
}

void RedClient::get_sync_info(uint8_t channel_type, uint8_t channel_id, SyncInfo& info)
{
    info.lock = &_sync_lock;
    info.condition = &_sync_condition;
    info.message_serial = &_sync_info[channel_type][channel_id];
}

void RedClient::wait_for_channels(int wait_list_size, SpiceWaitForChannel* wait_list)
{
    for (int i = 0; i < wait_list_size; i++) {
        if (wait_list[i].channel_type >= SPICE_END_CHANNEL) {
            THROW("invalid channel type %u", wait_list[i].channel_type);
        }
        uint64_t& sync_cell = _sync_info[wait_list[i].channel_type][wait_list[i].channel_id];
#ifndef RED64
        Lock lock(_sync_lock);
#endif
        if (sync_cell >= wait_list[i].message_serial) {
            continue;
        }
#ifdef RED64
        Lock lock(_sync_lock);
#endif
        for (;;) {
            if (sync_cell >= wait_list[i].message_serial) {
                break;
            }
            if (_aborting) {
                THROW("aborting");
            }
            _sync_condition.wait(lock);
            continue;
        }
    }
}

void RedClient::set_mm_time(uint32_t time)
{
    Lock lock(_mm_clock_lock);
    _mm_clock_last_update = Platform::get_monolithic_time();
    _mm_time = time;
}

uint32_t RedClient::get_mm_time()
{
    Lock lock(_mm_clock_lock);
    return uint32_t((Platform::get_monolithic_time() - _mm_clock_last_update) / 1000 / 1000 +
                    _mm_time);
}

void RedClient::register_channel_factory(ChannelFactory& factory)
{
    _factorys.push_back(&factory);
}
