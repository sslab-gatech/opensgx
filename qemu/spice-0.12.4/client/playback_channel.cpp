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
#include "red_client.h"
#include "audio_channels.h"
#include "audio_devices.h"

//#define WAVE_CAPTURE
#ifdef WAVE_CAPTURE

#include <fcntl.h>

#define WAVE_BUF_SIZE (1024 * 1024 * 20)

typedef struct __attribute__ ((__packed__)) ChunkHeader {
    uint32_t id;
    uint32_t size;
} ChunkHeader;

typedef struct __attribute__ ((__packed__)) FormatInfo {
    uint16_t compression_code;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t average_bytes_per_second;
    uint16_t block_align;
    uint16_t bits_per_sample;
    //uint16_t extra_format_bytes;
    //uint8_t extra[0];
} FormatInfo;

static uint8_t* wave_buf = NULL;
static uint8_t* wave_now = NULL;
static uint8_t* wave_end = NULL;
static bool wave_blocked = false;

static void write_all(int fd, uint8_t* data, uint32_t size)
{
    while (size) {
        int n = write(fd, data, size);
        if (n == -1) {
            if (errno != EINTR) {
                throw Exception(fmt("%s: failed") % __FUNCTION__);
            }
        } else {
            data += n;
            size -= n;
        }
    }
}

static void write_wave()
{
    static uint32_t file_id = 0;
    char file_name[100];
    ChunkHeader header;
    FormatInfo format;

    if (wave_buf == wave_now) {
        return;
    }

    sprintf(file_name, "/tmp/%u.wav", ++file_id);
    int fd = open(file_name, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd == -1) {
        DBG(0, fmt("open file %s failed") % file_name);
        return;
    }

    memcpy((char *)&header.id, "RIFF", 4);
    header.size = 4;
    write_all(fd, (uint8_t *)&header, sizeof(header));
    write_all(fd, (uint8_t *)"WAVE", 4);

    memcpy((char *)&header.id, "fmt ", 4);
    header.size = sizeof(format);
    write_all(fd, (uint8_t *)&header, sizeof(header));

    format.compression_code = 1;
    format.num_channels = 2;
    format.sample_rate = 44100;
    format.average_bytes_per_second = format.sample_rate * 4;
    format.block_align = 4;
    format.bits_per_sample = 16;
    write_all(fd, (uint8_t *)&format, sizeof(format));

    memcpy((char *)&header.id, "data", 4);
    header.size = wave_now - wave_buf;
    write_all(fd, (uint8_t *)&header, sizeof(header));
    write_all(fd, wave_buf, header.size);
    close(fd);
}

static void init_wave()
{
    if (!wave_buf) {
        wave_buf = new uint8_t[WAVE_BUF_SIZE];
    }
    wave_now = wave_buf;
    wave_end = wave_buf + WAVE_BUF_SIZE;
}

static void start_wave()
{
    wave_blocked = false;
    wave_now = wave_buf;
}

static void put_wave_data(uint8_t *data, uint32_t size)
{
    if (wave_blocked || size > wave_end - wave_now) {
        wave_blocked = true;
        return;
    }
    memcpy((void *)wave_now, (void *)data, size);
    wave_now += size;
}

static void end_wave()
{
    write_wave();
}

#endif

class PlaybackHandler: public MessageHandlerImp<PlaybackChannel, SPICE_CHANNEL_PLAYBACK> {
public:
    PlaybackHandler(PlaybackChannel& channel)
        : MessageHandlerImp<PlaybackChannel, SPICE_CHANNEL_PLAYBACK>(channel) {}
};

PlaybackChannel::PlaybackChannel(RedClient& client, uint32_t id)
    : RedChannel(client, SPICE_CHANNEL_PLAYBACK, id, new PlaybackHandler(*this),
                 Platform::PRIORITY_HIGH)
    , _wave_player (NULL)
    , _mode (SPICE_AUDIO_DATA_MODE_INVALID)
    , _celt_mode (NULL)
    , _celt_decoder (NULL)
    , _playing (false)
{
#ifdef WAVE_CAPTURE
    init_wave();
#endif
    PlaybackHandler* handler = static_cast<PlaybackHandler*>(get_message_handler());

    handler->set_handler(SPICE_MSG_MIGRATE, &PlaybackChannel::handle_migrate);
    handler->set_handler(SPICE_MSG_SET_ACK, &PlaybackChannel::handle_set_ack);
    handler->set_handler(SPICE_MSG_PING, &PlaybackChannel::handle_ping);
    handler->set_handler(SPICE_MSG_WAIT_FOR_CHANNELS, &PlaybackChannel::handle_wait_for_channels);
    handler->set_handler(SPICE_MSG_DISCONNECTING, &PlaybackChannel::handle_disconnect);
    handler->set_handler(SPICE_MSG_NOTIFY, &PlaybackChannel::handle_notify);

    handler->set_handler(SPICE_MSG_PLAYBACK_MODE, &PlaybackChannel::handle_mode);

    set_capability(SPICE_PLAYBACK_CAP_CELT_0_5_1);
}

void PlaybackChannel::clear()
{
    if (_wave_player) {
        _playing = false;
        _wave_player->stop();
        delete _wave_player;
        _wave_player = NULL;
    }
    _mode = SPICE_AUDIO_DATA_MODE_INVALID;

    if (_celt_decoder) {
        celt051_decoder_destroy(_celt_decoder);
        _celt_decoder = NULL;
    }

    if (_celt_mode) {
        celt051_mode_destroy(_celt_mode);
        _celt_mode = NULL;
    }
}

void PlaybackChannel::on_disconnect()
{
    clear();
}

PlaybackChannel::~PlaybackChannel(void)
{
    clear();
}

bool PlaybackChannel::abort(void)
{
    return (!_wave_player || _wave_player->abort()) && RedChannel::abort();
}

void PlaybackChannel::set_data_handler()
{
    PlaybackHandler* handler = static_cast<PlaybackHandler*>(get_message_handler());

    if (_mode == SPICE_AUDIO_DATA_MODE_RAW) {
        handler->set_handler(SPICE_MSG_PLAYBACK_DATA, &PlaybackChannel::handle_raw_data);
    } else if (_mode == SPICE_AUDIO_DATA_MODE_CELT_0_5_1) {
        handler->set_handler(SPICE_MSG_PLAYBACK_DATA, &PlaybackChannel::handle_celt_data);
    } else {
        THROW("invalid mode");
    }
}

void PlaybackChannel::handle_mode(RedPeer::InMessage* message)
{
    SpiceMsgPlaybackMode* playbacke_mode = (SpiceMsgPlaybackMode*)message->data();
    if (playbacke_mode->mode != SPICE_AUDIO_DATA_MODE_RAW &&
        playbacke_mode->mode != SPICE_AUDIO_DATA_MODE_CELT_0_5_1) {
        THROW("invalid mode");
    }

    _mode = playbacke_mode->mode;
    if (_playing) {
        set_data_handler();
        return;
    }

    PlaybackHandler* handler = static_cast<PlaybackHandler*>(get_message_handler());
    handler->set_handler(SPICE_MSG_PLAYBACK_START, &PlaybackChannel::handle_start);
}

void PlaybackChannel::null_handler(RedPeer::InMessage* message)
{
}

void PlaybackChannel::disable()
{
    PlaybackHandler* handler = static_cast<PlaybackHandler*>(get_message_handler());

    handler->set_handler(SPICE_MSG_PLAYBACK_START, &PlaybackChannel::null_handler);
    handler->set_handler(SPICE_MSG_PLAYBACK_STOP, &PlaybackChannel::null_handler);
    handler->set_handler(SPICE_MSG_PLAYBACK_MODE, &PlaybackChannel::null_handler);
    handler->set_handler(SPICE_MSG_PLAYBACK_DATA, &PlaybackChannel::null_handler);
}

void PlaybackChannel::handle_start(RedPeer::InMessage* message)
{
    PlaybackHandler* handler = static_cast<PlaybackHandler*>(get_message_handler());
    SpiceMsgPlaybackStart* start = (SpiceMsgPlaybackStart*)message->data();

    handler->set_handler(SPICE_MSG_PLAYBACK_START, NULL);
    handler->set_handler(SPICE_MSG_PLAYBACK_STOP, &PlaybackChannel::handle_stop);

#ifdef WAVE_CAPTURE
    start_wave();
#endif
    if (!_wave_player) {
        // for now support only one setting
        int celt_mode_err;

        if (start->format != SPICE_AUDIO_FMT_S16) {
            THROW("unexpected format");
        }
        int bits_per_sample = 16;
        int frame_size = 256;
        _frame_bytes = frame_size * start->channels * bits_per_sample / 8;
        try {
            _wave_player = Platform::create_player(start->frequency, bits_per_sample,
                                                   start->channels);
        } catch (...) {
            LOG_WARN("create player failed");
            //todo: support disconnecting single channel
            disable();
            return;
        }

        if (!(_celt_mode = celt051_mode_create(start->frequency, start->channels,
                                               frame_size, &celt_mode_err))) {
            THROW("create celt mode failed %d", celt_mode_err);
        }

        if (!(_celt_decoder = celt051_decoder_create(_celt_mode))) {
            THROW("create celt decoder");
        }
    }
    _playing = true;
    _frame_count = 0;
    set_data_handler();
}

void PlaybackChannel::handle_stop(RedPeer::InMessage* message)
{
    PlaybackHandler* handler = static_cast<PlaybackHandler*>(get_message_handler());

    handler->set_handler(SPICE_MSG_PLAYBACK_STOP, NULL);
    handler->set_handler(SPICE_MSG_PLAYBACK_DATA, NULL);
    handler->set_handler(SPICE_MSG_PLAYBACK_START, &PlaybackChannel::handle_start);

#ifdef WAVE_CAPTURE
    end_wave();
#endif
    _wave_player->stop();
    _playing = false;
}

void PlaybackChannel::handle_raw_data(RedPeer::InMessage* message)
{
    SpiceMsgPlaybackPacket* packet = (SpiceMsgPlaybackPacket*)message->data();
    uint8_t* data = packet->data;
    uint32_t size = packet->data_size;
#ifdef WAVE_CAPTURE
    put_wave_data(data, size);
    return;
#endif
    if (size != _frame_bytes) {
        //for now throw on unexpected size (based on current server imp).
        // will probably be replaced by supporting flexible data size in the player imp
        THROW("unexpected frame size");
    }
    if ((_frame_count++ % 1000) == 0) {
        get_client().set_mm_time(packet->time - _wave_player->get_delay_ms());
    }
    _wave_player->write(data);
}

void PlaybackChannel::handle_celt_data(RedPeer::InMessage* message)
{
    SpiceMsgPlaybackPacket* packet = (SpiceMsgPlaybackPacket*)message->data();
    uint8_t* data = packet->data;
    uint32_t size = packet->data_size;
    celt_int16_t pcm[256 * 2];

    if (celt051_decode(_celt_decoder, data, size, pcm) != CELT_OK) {
        THROW("celt decode failed");
    }
#ifdef WAVE_CAPTURE
    put_wave_data(pcm, _frame_bytes);
    return;
#endif
    if ((_frame_count++ % 1000) == 0) {
        get_client().set_mm_time(packet->time - _wave_player->get_delay_ms());
    }
    _wave_player->write((uint8_t *)pcm);
}

class PlaybackFactory: public ChannelFactory {
public:
    PlaybackFactory() : ChannelFactory(SPICE_CHANNEL_PLAYBACK) {}
    virtual RedChannel* construct(RedClient& client, uint32_t id)
    {
        return new PlaybackChannel(client, id);
    }
};

static PlaybackFactory factory;

ChannelFactory& PlaybackChannel::Factory()
{
    return factory;
}
