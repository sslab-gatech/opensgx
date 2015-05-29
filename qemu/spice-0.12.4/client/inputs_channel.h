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

#ifndef _H_INPUTS_CHANNEL
#define _H_INPUTS_CHANNEL

#include "red_channel.h"
#include "inputs_handler.h"

class ChannelFactory;

class InputsChannel: public RedChannel, public KeyHandler, public MouseHandler {
public:
    InputsChannel(RedClient& client, uint32_t id);
    virtual ~InputsChannel();

    virtual void on_mouse_motion(int dx, int dy, int buttons_state);
    virtual void on_mouse_down(int button, int buttons_state);
    virtual void on_mouse_up(int button, int buttons_state);
    virtual void on_key_down(RedKey key);
    virtual void on_key_up(RedKey key);
    virtual void on_focus_in();
    virtual void on_focus_out();

    void on_mouse_position(int x, int y, int buttons_state, int display_id);

    static ChannelFactory& Factory();

protected:
    virtual void on_connect();
    virtual void on_disconnect();
    virtual void on_migrate();

private:
    void marshall_motion_event(SpiceMarshaller *marshaller);
    void marshall_position_event(SpiceMarshaller *marshaller);
    void set_local_modifiers();

    void handle_init(RedPeer::InMessage* message);
    void handle_modifiers(RedPeer::InMessage* message);
    void handle_motion_ack(RedPeer::InMessage* message);

    static uint32_t get_make_scan_code(RedKey key);
    static uint32_t get_break_scan_code(RedKey key);
    static void init_scan_code(int index);
    static void init_korean_scan_code(int index);
    static void init_escape_scan_code(int index);
    static void init_pause_scan_code();
    static void init_scan_table();

private:
    Mutex _motion_lock;
    int _mouse_buttons_state;
    int _mouse_dx;
    int _mouse_dy;
    unsigned int _mouse_x;
    unsigned int _mouse_y;
    int _display_id;
    bool _active_motion;
    int _motion_count;
    uint32_t _modifiers;
    uint32_t _on_focus_modifiers;
    Mutex _update_modifiers_lock;
    bool _active_modifiers_event;

    struct KeyInfo {
        uint32_t make_scan;
        uint32_t break_scan;
    };

    static KeyInfo _scan_table[REDKEY_NUM_KEYS];

    friend class InitGlobals;
    friend class MotionMessage;
    friend class PositionMessage;
    friend class KeyModifiersEvent;
    friend class SetInputsHandlerEvent;
    friend class RemoveInputsHandlerEvent;
};


#endif
