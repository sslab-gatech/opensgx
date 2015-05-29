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

#ifndef _H_AUDIO_DEVICES
#define _H_AUDIO_DEVICES

class WavePlaybackAbstract {
public:
    WavePlaybackAbstract() {}
    virtual ~WavePlaybackAbstract() {}

    virtual bool write(uint8_t* frame) = 0;
    virtual bool abort() = 0;
    virtual void stop() = 0;
    virtual uint32_t get_delay_ms() = 0;

    enum {
        FRAME_SIZE = 256,
    };
};

class WaveRecordAbstract {
public:
    WaveRecordAbstract() {}
    virtual ~WaveRecordAbstract() {}


    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool abort() = 0;

    enum {
        FRAME_SIZE = 256,
    };
};

#endif
