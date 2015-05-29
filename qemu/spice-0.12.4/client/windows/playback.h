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

#ifndef _H_WINDOWS_AUDIO_PLAYBACK
#define _H_WINDOWS_AUDIO_PLAYBACK

#include "audio_devices.h"

class WavePlayer: public WavePlaybackAbstract {
public:
    WavePlayer(uint32_t sampels_per_sec, uint32_t bits_per_sample, uint32_t channels);
    virtual ~WavePlayer();

    virtual bool write(uint8_t* frame);
    virtual bool abort();
    virtual void stop();
    virtual uint32_t get_delay_ms();

private:
    WAVEHDR* wave_hader(uint32_t position);
    void move_head();
    WAVEHDR* get_buff();
    void reclaim();
    void drain();
    void init_ring(uint32_t sample_bytes);

private:
    HWAVEOUT _wave_out;
    uint32_t _sampels_per_ms;
    uint32_t _frame_bytes;
    uint32_t _start_mark;
    uint32_t _ring_item_size;
    uint8_t* _ring;
    uint32_t _ring_size;
    uint32_t _head;
    uint32_t _in_use;
    bool _paused;
};

#endif
