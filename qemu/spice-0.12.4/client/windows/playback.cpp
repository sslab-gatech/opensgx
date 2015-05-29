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
#include "playback.h"
#include "utils.h"
#include "debug.h"


#define RING_SIZE_MS 380
#define START_MARK_MS 300
#define LOW_MARK_MS 40


WavePlayer::WavePlayer(uint32_t sampels_per_sec, uint32_t bits_per_sample, uint32_t channels)
    : _wave_out (NULL)
    , _sampels_per_ms (sampels_per_sec / 1000)
    , _ring (NULL)
    , _head (0)
    , _in_use (0)
    , _paused (true)
{
    WAVEFORMATEX info;
    uint32_t sample_bytes;

    info.wFormatTag = WAVE_FORMAT_PCM;
    info.nChannels = channels;
    info.nSamplesPerSec = sampels_per_sec;
    sample_bytes = info.nBlockAlign = channels * bits_per_sample / 8;
    info.nAvgBytesPerSec = sampels_per_sec * info.nBlockAlign;
    info.wBitsPerSample = bits_per_sample;

    if (waveOutOpen(&_wave_out, WAVE_MAPPER, &info, 0, 0, CALLBACK_NULL)
                                                            != MMSYSERR_NOERROR) {
        throw Exception("can not open playback device");
    }

    int frame_size = WavePlaybackAbstract::FRAME_SIZE;
    _ring_size = (sampels_per_sec * RING_SIZE_MS / 1000) / frame_size;
    _start_mark = (sampels_per_sec * START_MARK_MS / 1000) / frame_size;
    _frame_bytes = frame_size * channels * bits_per_sample / 8;
    _ring_item_size = sizeof(WAVEHDR) + _frame_bytes + sample_bytes;

    try {
        _ring = new uint8_t[_ring_size * _ring_item_size];
        init_ring(sample_bytes);
    } catch (...) {
        delete[] _ring;
        waveOutClose(_wave_out);
        throw;
    }
    waveOutPause(_wave_out);
}

WavePlayer::~WavePlayer()
{
    waveOutReset(_wave_out);
    reclaim();
    waveOutClose(_wave_out);
    delete[] _ring;
}

void WavePlayer::init_ring(uint32_t sample_bytes)
{
    uint8_t* ptr = _ring;
    uint8_t* end = ptr + _ring_size * _ring_item_size;
    for (; ptr != end; ptr += _ring_item_size) {
        WAVEHDR* buf = (WAVEHDR*)ptr;
        memset(ptr, 0, _ring_item_size);
        buf->dwBufferLength = _frame_bytes;
        ULONG_PTR ptr = (ULONG_PTR)(buf + 1);
        ptr = (ptr + sample_bytes - 1) / sample_bytes * sample_bytes;
        buf->lpData = (LPSTR)ptr;
    }
}

inline WAVEHDR* WavePlayer::wave_hader(uint32_t position)
{
    ASSERT(position < _ring_size);
    return (WAVEHDR*)(_ring + position * _ring_item_size);
}

inline void WavePlayer::move_head()
{
    _head = (_head + 1) % _ring_size;
    _in_use--;
}

void WavePlayer::reclaim()
{
    while (_in_use) {
        WAVEHDR* front_buf = wave_hader(_head);
        if (!(front_buf->dwFlags & WHDR_DONE)) {
            break;
        }
        MMRESULT err = waveOutUnprepareHeader(_wave_out, front_buf, sizeof(WAVEHDR));
        if (err != MMSYSERR_NOERROR) {
            LOG_WARN("waveOutUnprepareHeader failed %u", err);
        }
        front_buf->dwFlags &= ~WHDR_DONE;
        move_head();
    }
}

WAVEHDR* WavePlayer::get_buff()
{
    reclaim();
    if (_in_use == _ring_size) {
        return NULL;
    }

    WAVEHDR* buff = wave_hader((_head + _in_use) % _ring_size);
    ++_in_use;
    return buff;
}

bool WavePlayer::write(uint8_t* frame)
{
    WAVEHDR* buff = get_buff();
    if (buff) {
        memcpy(buff->lpData, frame, _frame_bytes);
        MMRESULT err;
        err = waveOutPrepareHeader(_wave_out, buff, sizeof(WAVEHDR));

        if (err != MMSYSERR_NOERROR) {
            THROW("waveOutPrepareHeader filed %d", err);
        }
        err = waveOutWrite(_wave_out, buff, sizeof(WAVEHDR));
        if (err != MMSYSERR_NOERROR) {
            THROW("waveOutWrite filed %d", err);
        }
    }
    if (_paused && _in_use == _start_mark) {
        _paused = false;
        waveOutRestart(_wave_out);
    }
    return true;
}

void WavePlayer::drain()
{
}

void WavePlayer::stop()
{
    drain();
    waveOutReset(_wave_out);
    waveOutPause(_wave_out);
    _paused = true;
    reclaim();
}

bool WavePlayer::abort()
{
    return true;
}

uint32_t WavePlayer::get_delay_ms()
{
    return _in_use * WavePlaybackAbstract::FRAME_SIZE / _sampels_per_ms;
}
