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

#ifndef _H_WINDOWS_RECORD
#define _H_WINDOWS_RECORD

#include "audio_devices.h"
#include "platform.h"

class WaveRecorder: public WaveRecordAbstract, public EventSources::Trigger {
public:
    WaveRecorder(Platform::RecordClient& client, uint32_t sampels_per_sec,
                 uint32_t bits_per_sample, uint32_t channels);
    virtual ~WaveRecorder();

    virtual void start();
    virtual void stop();
    virtual bool abort();

    virtual void on_event();

private:
    void init_ring(uint32_t sampels_per_sec, uint32_t frame_bytes, uint32_t frame_align);
    WAVEHDR* wave_hader(uint32_t position);
    void move_head();

    void reclaim();
    void push_frames();

private:
    Platform::RecordClient& _client;
    HWAVEIN _wave_in;
    uint8_t* _ring;
    uint32_t _ring_item_size;
    uint32_t _ring_size;
    uint32_t _head;
    uint32_t _in_use;
    uint8_t* _frame;
    uint8_t* _frame_pos;
    uint8_t* _frame_end;
};

#endif
