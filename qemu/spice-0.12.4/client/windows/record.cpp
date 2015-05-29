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
#include "record.h"
#include "utils.h"
#include "debug.h"

#define RING_SIZE_MS 500

static void CALLBACK in_proc(HWAVEIN handle, UINT msg, DWORD user_data, DWORD param1,
                             DWORD param2)
{
    WaveRecorder* recorder = (WaveRecorder*)user_data;
    recorder->trigger();
}

WaveRecorder::WaveRecorder(Platform::RecordClient& client, uint32_t sampels_per_sec,
                           uint32_t bits_per_sample, uint32_t channels)
    : _client (client)
    , _ring (NULL)
    , _head (0)
    , _in_use (0)
    , _frame (NULL)
{
    WAVEFORMATEX info;
    uint32_t frame_align;

    info.wFormatTag = WAVE_FORMAT_PCM;
    info.nChannels = channels;
    info.nSamplesPerSec = sampels_per_sec;
    info.nBlockAlign = frame_align = channels * bits_per_sample / 8;
    info.nAvgBytesPerSec = sampels_per_sec * info.nBlockAlign;
    info.wBitsPerSample = bits_per_sample;


    if (waveInOpen(&_wave_in, WAVE_MAPPER, &info, (DWORD_PTR)in_proc, (DWORD_PTR)this,
                   CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        throw Exception("cannot open playback device");
    }

    try {
        const int frame_size = WavePlaybackAbstract::FRAME_SIZE;
        uint32_t frame_bytes = frame_size * channels * bits_per_sample / 8;

        _frame = new uint8_t[frame_bytes];
        _frame_pos = _frame;
        _frame_end = _frame + frame_bytes;
        init_ring(sampels_per_sec, frame_bytes, frame_align);
        _client.add_event_source(*this);
    } catch (...) {
        delete[] _ring;
        delete[] _frame;
        waveInClose(_wave_in);
        throw;
    }
}

WaveRecorder::~WaveRecorder()
{
    waveInReset(_wave_in);
    reclaim();
    _client.remove_event_source(*this);
    waveInClose(_wave_in);
    delete[] _ring;
    delete[] _frame;
}

void WaveRecorder::init_ring(uint32_t sampels_per_sec, uint32_t frame_bytes, uint32_t frame_align)
{
    const int frame_size = WavePlaybackAbstract::FRAME_SIZE;

    _ring_size = (sampels_per_sec * RING_SIZE_MS / 1000) / frame_size;
    _ring_item_size = sizeof(WAVEHDR) + frame_bytes + frame_align;

    int ring_bytes = _ring_size * _ring_item_size;
    _ring = new uint8_t[ring_bytes];

    uint8_t* ptr = _ring;
    uint8_t* end = ptr + ring_bytes;
    for (; ptr != end; ptr += _ring_item_size) {
        WAVEHDR* buf = (WAVEHDR*)ptr;
        memset(ptr, 0, _ring_item_size);
        buf->dwBufferLength = frame_bytes;
        ULONG_PTR ptr = (ULONG_PTR)(buf + 1);
        ptr = (ptr + frame_align - 1) / frame_align * frame_align;
        buf->lpData = (LPSTR)(buf + 1);
    }
}

void WaveRecorder::start()
{
    _frame_pos = _frame;
    waveInReset(_wave_in);
    push_frames();
    waveInStart(_wave_in);
}

inline WAVEHDR* WaveRecorder::wave_hader(uint32_t position)
{
    ASSERT(position < _ring_size);
    return (WAVEHDR*)(_ring + position * _ring_item_size);
}

inline void WaveRecorder::move_head()
{
    _head = (_head + 1) % _ring_size;
    _in_use--;
}

void WaveRecorder::push_frames()
{
    while (_in_use != _ring_size) {
        WAVEHDR* buff = wave_hader((_head + _in_use) % _ring_size);
        ++_in_use;

        MMRESULT err = waveInPrepareHeader(_wave_in, buff, sizeof(WAVEHDR));
        if (err != MMSYSERR_NOERROR) {
            THROW("waveInPrepareHeader filed %d", err);
        }
        err = waveInAddBuffer(_wave_in, buff, sizeof(WAVEHDR));
        if (err != MMSYSERR_NOERROR) {
            THROW("waveInAddBuffer filed %d", err);
        }
    }
}

void WaveRecorder::on_event()
{
    while (_in_use) {
        WAVEHDR* front_buf = wave_hader(_head);
        if (!(front_buf->dwFlags & WHDR_DONE)) {
            break;
        }
        waveInUnprepareHeader(_wave_in, front_buf, sizeof(WAVEHDR));
        front_buf->dwFlags &= ~WHDR_DONE;
        int n = front_buf->dwBytesRecorded;
        front_buf->dwBytesRecorded = 0;
        uint8_t* ptr = (uint8_t*)front_buf->lpData;
        while (n) {
            int now = MIN(n, _frame_end - _frame_pos);
            memcpy(_frame_pos, ptr, now);
            if ((_frame_pos += n) == _frame_end) {
                _client.push_frame(_frame);
                _frame_pos = _frame;
            }
            n -= now;
            ptr += now;
        }
        move_head();
        push_frames();
    }
}

void WaveRecorder::reclaim()
{
    while (_in_use) {
        WAVEHDR* front_buf = wave_hader(_head);
        if (!(front_buf->dwFlags & WHDR_DONE)) {
            break;
        }
        waveInUnprepareHeader(_wave_in, front_buf, sizeof(WAVEHDR));
        front_buf->dwFlags &= ~WHDR_DONE;
        front_buf->dwBytesRecorded = 0;
        move_head();
    }
}

void WaveRecorder::stop()
{
    waveInReset(_wave_in);
    reclaim();
}

bool WaveRecorder::abort()
{
    return true;
}
