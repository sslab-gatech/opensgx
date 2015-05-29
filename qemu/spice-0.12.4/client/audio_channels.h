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

#ifndef _H_AUDIO_CHANNELS
#define _H_AUDIO_CHANNELS

#include <celt051/celt.h>

#include "red_channel.h"
#include "debug.h"

class ChannelFactory;

class WavePlaybackAbstract;
class WaveRecordAbstract;
class RecordSamplesMessage;

class PlaybackChannel: public RedChannel {
public:
    PlaybackChannel(RedClient& client, uint32_t id);
    ~PlaybackChannel(void);
    bool abort(void);

    static ChannelFactory& Factory();

protected:
    virtual void on_disconnect();

private:
    void handle_mode(RedPeer::InMessage* message);
    void handle_start(RedPeer::InMessage* message);
    void handle_stop(RedPeer::InMessage* message);
    void handle_raw_data(RedPeer::InMessage* message);
    void handle_celt_data(RedPeer::InMessage* message);
    void null_handler(RedPeer::InMessage* message);
    void disable();

    void set_data_handler();

    void clear();

private:
    WavePlaybackAbstract* _wave_player;
    uint32_t _mode;
    uint32_t _frame_bytes;
    CELTMode *_celt_mode;
    CELTDecoder *_celt_decoder;
    bool _playing;
    uint32_t _frame_count;
};

class RecordChannel: public RedChannel, private Platform::RecordClient {
public:
    RecordChannel(RedClient& client, uint32_t id);
    ~RecordChannel(void);

    bool abort(void);

    static ChannelFactory& Factory();

protected:
    virtual void on_connect();
    virtual void on_disconnect();

private:
    void handle_start(RedPeer::InMessage* message);
    void handle_stop(RedPeer::InMessage* message);

    virtual void add_event_source(EventSources::File& event_source);
    virtual void remove_event_source(EventSources::File& event_source);
    virtual void add_event_source(EventSources::Trigger& event_source);
    virtual void remove_event_source(EventSources::Trigger& event_source);
    virtual void push_frame(uint8_t *frame);

    void send_start_mark();
    void release_message(RecordSamplesMessage *message);
    RecordSamplesMessage * get_message();
    void clear();

private:
    WaveRecordAbstract* _wave_recorder;
    Mutex _messages_lock;
    std::list<RecordSamplesMessage *> _messages;
    int _mode;
    CELTMode *_celt_mode;
    CELTEncoder *_celt_encoder;
    uint32_t _frame_bytes;

    static int data_mode;

    friend class RecordSamplesMessage;
};

#endif
