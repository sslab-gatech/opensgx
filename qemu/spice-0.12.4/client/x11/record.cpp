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

#include "record.h"
#include "utils.h"
#include "debug.h"


class WaveRecorder::EventTrigger: public EventSources::File {
public:
    EventTrigger(WaveRecorder& recorder, int fd);
    virtual void on_event();
    virtual int get_fd() { return _fd;}

private:
    WaveRecorder& _recorder;
    int _fd;
};

WaveRecorder::EventTrigger::EventTrigger(WaveRecorder& recorder, int fd)
    : _recorder (recorder)
    , _fd (fd)
{
}

void WaveRecorder::EventTrigger::on_event()
{
    _recorder.on_event();
}

WaveRecorder::WaveRecorder(Platform::RecordClient& client,
                           uint32_t sampels_per_sec,
                           uint32_t bits_per_sample,
                           uint32_t channels)
    : _client (client)
    , _pcm (NULL)
    , _hw_params (NULL)
    , _sw_params (NULL)
    , _sample_bytes (bits_per_sample * channels / 8)
    , _frame (new uint8_t[_sample_bytes * WaveRecordAbstract::FRAME_SIZE])
    , _frame_pos (_frame)
    , _frame_end (_frame + _sample_bytes * WaveRecordAbstract::FRAME_SIZE)
    , _event_trigger (NULL)
{
    if (!init(sampels_per_sec, bits_per_sample, channels)) {
        cleanup();
        THROW("failed");
    }
}

WaveRecorder::~WaveRecorder()
{
    cleanup();
    delete[] _frame;
}

void WaveRecorder::cleanup()
{
    if (_event_trigger) {
        _client.remove_event_source(*_event_trigger);
        delete _event_trigger;
    }

    if (_sw_params) {
        snd_pcm_sw_params_free(_sw_params);
    }

    if (_hw_params) {
        snd_pcm_hw_params_free(_hw_params);
    }

    if (_pcm) {
        snd_pcm_close(_pcm);
    }
}

bool WaveRecorder::init(uint32_t sampels_per_sec,
                        uint32_t bits_per_sample,
                        uint32_t channels)
{
    const int frame_size = WaveRecordAbstract::FRAME_SIZE;
    const char* pcm_device = "default";
    snd_pcm_format_t format;
    int err;

    switch (bits_per_sample) {
    case 8:
        format = SND_PCM_FORMAT_S8;
        break;
    case 16:
        format = SND_PCM_FORMAT_S16_LE;
        break;
    default:
        return false;
    }

    if ((err = snd_pcm_open(&_pcm, pcm_device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK)) < 0) {
        LOG_ERROR("cannot open audio record device %s %s", pcm_device, snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_hw_params_malloc(&_hw_params)) < 0) {
        LOG_ERROR("cannot allocate hardware parameter structure %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_sw_params_malloc(&_sw_params)) < 0) {
        LOG_ERROR("cannot allocate software parameter structure %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_hw_params_any(_pcm, _hw_params)) < 0) {
        LOG_ERROR("cannot initialize hardware parameter structure %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_hw_params_set_rate_resample(_pcm, _hw_params, 1)) < 0) {
        LOG_ERROR("cannot set rate resample %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_hw_params_set_access(_pcm, _hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        LOG_ERROR("cannot set access type %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_hw_params_set_rate(_pcm, _hw_params, sampels_per_sec, 0)) < 0) {
        LOG_ERROR("cannot set sample rate %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_hw_params_set_channels(_pcm, _hw_params, channels)) < 0) {
        LOG_ERROR("cannot set channel count %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_hw_params_set_format(_pcm, _hw_params, format)) < 0) {
        LOG_ERROR("cannot set sample format %s", snd_strerror(err));
        return false;
    }

    int direction = 0;
    snd_pcm_uframes_t buffer_size = (sampels_per_sec * 160 / 1000) / frame_size * frame_size;

    if ((err = snd_pcm_hw_params_set_buffer_size_near(_pcm, _hw_params, &buffer_size)) < 0) {
        LOG_ERROR("cannot set buffer size %s", snd_strerror(err));
        return false;
    }

    snd_pcm_uframes_t period_size = frame_size;
    if ((err = snd_pcm_hw_params_set_period_size_near(_pcm, _hw_params, &period_size,
                                                      &direction)) < 0) {
        LOG_ERROR("cannot set period size near %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_hw_params(_pcm, _hw_params)) < 0) {
        LOG_ERROR("cannot set parameters %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_sw_params_current(_pcm, _sw_params)) < 0) {
        LOG_ERROR("cannot get current sw parameters %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_sw_params_set_start_threshold(_pcm, _sw_params, frame_size)) < 0) {
        LOG_ERROR("cannot set start threshold %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_sw_params(_pcm, _sw_params)) < 0) {
        LOG_ERROR("cannot set sw parameters %s", snd_strerror(err));
        return false;
    }

    struct pollfd pfd;
    if ((err = snd_pcm_poll_descriptors(_pcm, &pfd, 1)) < 0) {
        LOG_ERROR("cannot get poll ID %s", snd_strerror(err));
        return false;
    }
    _event_trigger = new WaveRecorder::EventTrigger(*this, pfd.fd);
    _client.add_event_source(*_event_trigger);
    return true;
}

void WaveRecorder::start()
{
    _frame_pos = _frame;
    snd_pcm_prepare(_pcm);
    snd_pcm_start(_pcm);
    snd_pcm_nonblock(_pcm, 1);
}

void WaveRecorder::stop()
{
    snd_pcm_drop(_pcm);
    snd_pcm_prepare(_pcm);
}

bool WaveRecorder::abort()
{
    return true;
}

void WaveRecorder::on_event()
{
    for (;;) {
        snd_pcm_sframes_t size = (_frame_end - _frame_pos) / _sample_bytes;
        size = snd_pcm_readi(_pcm, _frame_pos, size);
        if (size < 0) {
            if (snd_pcm_recover(_pcm, size, 1) == 0) {
                continue;
            }
            return;
        }
        _frame_pos += size * _sample_bytes;
        if (_frame_pos == _frame_end) {
            _client.push_frame(_frame);
            _frame_pos = _frame;
        }
    }
}
