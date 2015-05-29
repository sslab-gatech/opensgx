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
#include "inputs_channel.h"
#include "utils.h"
#include "debug.h"
#include "red_client.h"
#include "application.h"
#include "display_channel.h"

#define SYNC_REMOTE_MODIFIERS

class SetInputsHandlerEvent: public Event {
public:
    SetInputsHandlerEvent(InputsChannel& channel) : _channel (channel) {}

    class AttachFunc: public ForEachChannelFunc {
    public:
        AttachFunc(InputsChannel& channel)
            : _channel (channel)
        {
        }

        virtual bool operator() (RedChannel& channel)
        {
            if (channel.get_type() == SPICE_CHANNEL_DISPLAY) {
                static_cast<DisplayChannel&>(channel).attach_inputs(&_channel);
            }
            return true;
        }

    public:
        InputsChannel& _channel;
    };

    virtual void response(AbstractProcessLoop& events_loop)
    {
        static_cast<Application*>(events_loop.get_owner())->set_key_handler(_channel);
        static_cast<Application*>(events_loop.get_owner())->set_mouse_handler(_channel);
        AttachFunc func(_channel);
        _channel.get_client().for_each_channel(func);
    }

private:
    InputsChannel& _channel;
};

class KeyModifiersEvent: public Event {
public:
    KeyModifiersEvent(InputsChannel& channel) : _channel (channel) {}

    virtual void response(AbstractProcessLoop& events_loop)
    {
        Lock lock(_channel._update_modifiers_lock);
        _channel._active_modifiers_event = false;
        _channel.set_local_modifiers();
    }

private:
    InputsChannel& _channel;
};

class RemoveInputsHandlerEvent: public SyncEvent {
public:
    RemoveInputsHandlerEvent(InputsChannel& channel) : _channel (channel) {}

    class DetachFunc: public ForEachChannelFunc {
    public:
        virtual bool operator() (RedChannel& channel)
        {
            if (channel.get_type() == SPICE_CHANNEL_DISPLAY) {
                static_cast<DisplayChannel&>(channel).detach_inputs();
            }
            return true;
        }
    };

    virtual void do_response(AbstractProcessLoop& events_loop)
    {
        static_cast<Application*>(events_loop.get_owner())->remove_key_handler(_channel);
        static_cast<Application*>(events_loop.get_owner())->remove_mouse_handler(_channel);
        DetachFunc detach_func;
        _channel.get_client().for_each_channel(detach_func);
    }

private:
    InputsChannel& _channel;
};

class MotionMessage: public RedChannel::OutMessage, public RedPeer::OutMessage {
public:
    MotionMessage(InputsChannel& channel);
    virtual RedPeer::OutMessage& peer_message();
    virtual void release();

private:
    InputsChannel& _channel;
};

MotionMessage::MotionMessage(InputsChannel& channel)
    : RedChannel::OutMessage()
    , RedPeer::OutMessage(SPICE_MSGC_INPUTS_MOUSE_MOTION)
    , _channel (channel)
{
}

void MotionMessage::release()
{
    delete this;
}

RedPeer::OutMessage& MotionMessage::peer_message()
{

    _channel.marshall_motion_event(_marshaller);

    return *this;
}

class PositionMessage: public RedChannel::OutMessage, public RedPeer::OutMessage {
public:
    PositionMessage(InputsChannel& channel);
    virtual RedPeer::OutMessage& peer_message();
    virtual void release();

private:
    InputsChannel& _channel;
};

PositionMessage::PositionMessage(InputsChannel& channel)
    : RedChannel::OutMessage()
    , RedPeer::OutMessage(SPICE_MSGC_INPUTS_MOUSE_POSITION)
    , _channel (channel)
{
}

void PositionMessage::release()
{
    delete this;
}

RedPeer::OutMessage& PositionMessage::peer_message()
{
    _channel.marshall_position_event(_marshaller);
    return *this;
}

class InputsMessHandler: public MessageHandlerImp<InputsChannel, SPICE_CHANNEL_INPUTS> {
public:
    InputsMessHandler(InputsChannel& channel)
        : MessageHandlerImp<InputsChannel, SPICE_CHANNEL_INPUTS>(channel) {}
};

InputsChannel::InputsChannel(RedClient& client, uint32_t id)
    : RedChannel(client, SPICE_CHANNEL_INPUTS, id, new InputsMessHandler(*this))
    , _mouse_buttons_state (0)
    , _mouse_dx (0)
    , _mouse_dy (0)
    , _mouse_x (~0)
    , _mouse_y (~0)
    , _display_id (-1)
    , _active_motion (false)
    , _motion_count (0)
    , _active_modifiers_event (false)
{
    InputsMessHandler* handler = static_cast<InputsMessHandler*>(get_message_handler());
    handler->set_handler(SPICE_MSG_MIGRATE, &InputsChannel::handle_migrate);
    handler->set_handler(SPICE_MSG_SET_ACK, &InputsChannel::handle_set_ack);
    handler->set_handler(SPICE_MSG_PING, &InputsChannel::handle_ping);
    handler->set_handler(SPICE_MSG_WAIT_FOR_CHANNELS, &InputsChannel::handle_wait_for_channels);
    handler->set_handler(SPICE_MSG_DISCONNECTING, &InputsChannel::handle_disconnect);
    handler->set_handler(SPICE_MSG_NOTIFY, &InputsChannel::handle_notify);

    handler->set_handler(SPICE_MSG_INPUTS_INIT, &InputsChannel::handle_init);
    handler->set_handler(SPICE_MSG_INPUTS_KEY_MODIFIERS, &InputsChannel::handle_modifiers);
    handler->set_handler(SPICE_MSG_INPUTS_MOUSE_MOTION_ACK, &InputsChannel::handle_motion_ack);
}

InputsChannel::~InputsChannel()
{
}

void InputsChannel::on_connect()
{
    _motion_count = _mouse_dx = _mouse_dy = _mouse_buttons_state = _modifiers = 0;
    _mouse_x = _mouse_y = ~0;
    _display_id = -1;
}

void InputsChannel::on_disconnect()
{
    AutoRef<RemoveInputsHandlerEvent> remove_handler_event(new RemoveInputsHandlerEvent(*this));
    get_client().push_event(*remove_handler_event);
    (*remove_handler_event)->wait();
}

void InputsChannel::handle_init(RedPeer::InMessage* message)
{
    SpiceMsgInputsInit* init = (SpiceMsgInputsInit*)message->data();
    _modifiers = init->keyboard_modifiers;
    AutoRef<SetInputsHandlerEvent> set_handler_event(new SetInputsHandlerEvent(*this));
    get_client().push_event(*set_handler_event);
}

void InputsChannel::handle_modifiers(RedPeer::InMessage* message)
{
    SpiceMsgInputsKeyModifiers* init = (SpiceMsgInputsKeyModifiers*)message->data();
    _modifiers = init->modifiers;
    Lock lock(_update_modifiers_lock);
    if (_active_modifiers_event) {
        return;
    }
    _active_modifiers_event = true;
    AutoRef<KeyModifiersEvent> modifiers_event(new KeyModifiersEvent(*this));
    get_client().push_event(*modifiers_event);
}

void InputsChannel::handle_motion_ack(RedPeer::InMessage* message)
{
    Lock lock(_motion_lock);
    if (_motion_count < SPICE_INPUT_MOTION_ACK_BUNCH) {
        LOG_WARN("invalid motion count");
        _motion_count = 0;
    } else {
        _motion_count -= SPICE_INPUT_MOTION_ACK_BUNCH;
    }
    if (!_active_motion && (_mouse_dx || _mouse_dy || _display_id != -1)) {
        _active_motion = true;
        _motion_count++;
        switch (get_client().get_mouse_mode()) {
        case SPICE_MOUSE_MODE_CLIENT:
            post_message(new PositionMessage(*this));
            break;
        case SPICE_MOUSE_MODE_SERVER:
            post_message(new MotionMessage(*this));
            break;
        default:
            THROW("invalid mouse mode");
        }
    }
}

void InputsChannel::marshall_motion_event(SpiceMarshaller *marshaller)
{
    SpiceMsgcMouseMotion motion;

    Lock lock(_motion_lock);
    motion.buttons_state = _mouse_buttons_state;
    motion.dx = _mouse_dx;
    motion.dy = _mouse_dy;
    _mouse_dx = _mouse_dy = 0;
    _active_motion = false;

    _marshallers->msgc_inputs_mouse_motion(marshaller, &motion);
}

void InputsChannel::marshall_position_event(SpiceMarshaller *marshaller)
{
    SpiceMsgcMousePosition position;
    Lock lock(_motion_lock);
    position.buttons_state = _mouse_buttons_state;
    position.x = _mouse_x;
    position.y = _mouse_y;
    position.display_id = _display_id;
    _mouse_x = _mouse_y = ~0;
    _display_id = -1;
    _active_motion = false;
    _marshallers->msgc_inputs_mouse_position(marshaller, &position);
}

void InputsChannel::on_mouse_motion(int dx, int dy, int buttons_state)
{
    Lock lock(_motion_lock);
    _mouse_buttons_state = buttons_state;
    _mouse_dx += dx;
    _mouse_dy += dy;
    if (!_active_motion && _motion_count < SPICE_INPUT_MOTION_ACK_BUNCH * 2) {
        _active_motion = true;
        _motion_count++;
        post_message(new MotionMessage(*this));
    }
}

void InputsChannel::on_mouse_position(int x, int y, int buttons_state, int display_id)
{
    Lock lock(_motion_lock);
    _mouse_buttons_state = buttons_state;
    _mouse_x = x;
    _mouse_y = y;
    _display_id = display_id;
    if (!_active_motion && _motion_count < SPICE_INPUT_MOTION_ACK_BUNCH * 2) {
        _active_motion = true;
        _motion_count++;
        post_message(new PositionMessage(*this));
    }
}

void InputsChannel::on_migrate()
{
    _motion_count = _active_motion ? 1 : 0;
}

void InputsChannel::on_mouse_down(int button, int buttons_state)
{
    Message* message;

    message = new Message(SPICE_MSGC_INPUTS_MOUSE_PRESS);
    SpiceMsgcMousePress event;
    event.button = button;
    event.buttons_state = buttons_state;
    _marshallers->msgc_inputs_mouse_press(message->marshaller(), &event);

    post_message(message);
}

void InputsChannel::on_mouse_up(int button, int buttons_state)
{
    Message* message;

    message = new Message(SPICE_MSGC_INPUTS_MOUSE_RELEASE);
    SpiceMsgcMouseRelease event;
    event.button = button;
    event.buttons_state = buttons_state;
    _marshallers->msgc_inputs_mouse_release(message->marshaller(), &event);
    post_message(message);
}

InputsChannel::KeyInfo InputsChannel::_scan_table[REDKEY_NUM_KEYS];

uint32_t InputsChannel::get_make_scan_code(RedKey key)
{
    return _scan_table[key].make_scan;
}

uint32_t InputsChannel::get_break_scan_code(RedKey key)
{
    return _scan_table[key].break_scan;
}

void InputsChannel::on_key_down(RedKey key)
{
    uint32_t scan_code = get_make_scan_code(key);
    if (!scan_code) {
        LOG_WARN("no make code for %d", key);
        return;
    }

    Message* message = new Message(SPICE_MSGC_INPUTS_KEY_DOWN);
    SpiceMsgcKeyDown event;
    event.code = scan_code;
    _marshallers->msgc_inputs_key_down(message->marshaller(), &event);

    post_message(message);
}

void InputsChannel::on_key_up(RedKey key)
{
    uint32_t scan_code = get_break_scan_code(key);
    if (!scan_code) {
        LOG_WARN("no break code for %d", key);
        return;
    }

    Message* message = new Message(SPICE_MSGC_INPUTS_KEY_UP);
    SpiceMsgcKeyUp event;
    event.code = scan_code;
    _marshallers->msgc_inputs_key_up(message->marshaller(), &event);
    post_message(message);
}

void InputsChannel::set_local_modifiers()
{
    unsigned int modifiers = 0;

    if (_modifiers & SPICE_KEYBOARD_MODIFIER_FLAGS_SCROLL_LOCK) {
        modifiers |= Platform::SCROLL_LOCK_MODIFIER;
    }

    if (_modifiers & SPICE_KEYBOARD_MODIFIER_FLAGS_NUM_LOCK) {
        modifiers |= Platform::NUM_LOCK_MODIFIER;
    }

    if (_modifiers & SPICE_KEYBOARD_MODIFIER_FLAGS_CAPS_LOCK) {
        modifiers |= Platform::CAPS_LOCK_MODIFIER;
    }

    Platform::set_keyboard_lock_modifiers(modifiers);
}

void InputsChannel::on_focus_in()
{
    Lock lock(_update_modifiers_lock);
    _active_modifiers_event = false;
    _on_focus_modifiers = Platform::get_keyboard_lock_modifiers();

#ifdef SYNC_REMOTE_MODIFIERS
    Message* message = new Message(SPICE_MSGC_INPUTS_KEY_MODIFIERS);
    SpiceMsgcKeyModifiers modifiers;
    modifiers.modifiers = _on_focus_modifiers;
    _marshallers->msgc_inputs_key_modifiers(message->marshaller(), &modifiers);
    post_message(message);
#else
    set_local_modifiers();
#endif
}

void InputsChannel::on_focus_out()
{
    Lock lock(_update_modifiers_lock);
    _active_modifiers_event = true;
#ifndef SYNC_REMOTE_MODIFIERS
    _modifiers = _on_focus_modifiers;
    set_local_modifiers();
#endif
}

void InputsChannel::init_scan_code(int index)
{
    ASSERT((index & 0x80) == 0);
    _scan_table[index].make_scan = index;
    _scan_table[index].break_scan = index | 0x80;
}

void InputsChannel::init_korean_scan_code(int index)
{
    _scan_table[index].make_scan = index;
    _scan_table[index].break_scan = index;
}

void InputsChannel::init_escape_scan_code(int index)
{
    ASSERT(((index - REDKEY_ESCAPE_BASE) & 0x80) == 0);
    _scan_table[index].make_scan = 0xe0 | ((index - REDKEY_ESCAPE_BASE) << 8);
    _scan_table[index].break_scan = _scan_table[index].make_scan | 0x8000;
}

void InputsChannel::init_pause_scan_code()
{
    _scan_table[REDKEY_PAUSE].make_scan = 0x451de1;
    _scan_table[REDKEY_PAUSE].break_scan = 0xc59de1;
}

void InputsChannel::init_scan_table()
{
    memset(_scan_table, 0, sizeof(_scan_table));
    init_scan_code(REDKEY_ESCAPE);
    init_scan_code(REDKEY_1);
    init_scan_code(REDKEY_2);
    init_scan_code(REDKEY_3);
    init_scan_code(REDKEY_4);
    init_scan_code(REDKEY_5);
    init_scan_code(REDKEY_6);
    init_scan_code(REDKEY_7);
    init_scan_code(REDKEY_8);
    init_scan_code(REDKEY_9);
    init_scan_code(REDKEY_0);
    init_scan_code(REDKEY_MINUS);
    init_scan_code(REDKEY_EQUALS);
    init_scan_code(REDKEY_BACKSPACE);
    init_scan_code(REDKEY_TAB);
    init_scan_code(REDKEY_Q);
    init_scan_code(REDKEY_W);
    init_scan_code(REDKEY_E);
    init_scan_code(REDKEY_R);
    init_scan_code(REDKEY_T);
    init_scan_code(REDKEY_Y);
    init_scan_code(REDKEY_U);
    init_scan_code(REDKEY_I);
    init_scan_code(REDKEY_O);
    init_scan_code(REDKEY_P);
    init_scan_code(REDKEY_L_BRACKET);
    init_scan_code(REDKEY_R_BRACKET);
    init_scan_code(REDKEY_ENTER);
    init_scan_code(REDKEY_L_CTRL);
    init_scan_code(REDKEY_A);
    init_scan_code(REDKEY_S);
    init_scan_code(REDKEY_D);
    init_scan_code(REDKEY_F);
    init_scan_code(REDKEY_G);
    init_scan_code(REDKEY_H);
    init_scan_code(REDKEY_J);
    init_scan_code(REDKEY_K);
    init_scan_code(REDKEY_L);
    init_scan_code(REDKEY_SEMICOLON);
    init_scan_code(REDKEY_QUOTE);
    init_scan_code(REDKEY_BACK_QUOTE);
    init_scan_code(REDKEY_L_SHIFT);
    init_scan_code(REDKEY_BACK_SLASH);
    init_scan_code(REDKEY_Z);
    init_scan_code(REDKEY_X);
    init_scan_code(REDKEY_C);
    init_scan_code(REDKEY_V);
    init_scan_code(REDKEY_B);
    init_scan_code(REDKEY_N);
    init_scan_code(REDKEY_M);
    init_scan_code(REDKEY_COMMA);
    init_scan_code(REDKEY_PERIOD);
    init_scan_code(REDKEY_SLASH);
    init_scan_code(REDKEY_R_SHIFT);
    init_scan_code(REDKEY_PAD_MULTIPLY);
    init_scan_code(REDKEY_L_ALT);
    init_scan_code(REDKEY_SPACE);
    init_scan_code(REDKEY_CAPS_LOCK);
    init_scan_code(REDKEY_F1);
    init_scan_code(REDKEY_F2);
    init_scan_code(REDKEY_F3);
    init_scan_code(REDKEY_F4);
    init_scan_code(REDKEY_F5);
    init_scan_code(REDKEY_F6);
    init_scan_code(REDKEY_F7);
    init_scan_code(REDKEY_F8);
    init_scan_code(REDKEY_F9);
    init_scan_code(REDKEY_F10);
    init_scan_code(REDKEY_NUM_LOCK);
    init_scan_code(REDKEY_SCROLL_LOCK);
    init_scan_code(REDKEY_PAD_7);
    init_scan_code(REDKEY_PAD_8);
    init_scan_code(REDKEY_PAD_9);
    init_scan_code(REDKEY_PAD_MINUS);
    init_scan_code(REDKEY_PAD_4);
    init_scan_code(REDKEY_PAD_5);
    init_scan_code(REDKEY_PAD_6);
    init_scan_code(REDKEY_PAD_PLUS);
    init_scan_code(REDKEY_PAD_1);
    init_scan_code(REDKEY_PAD_2);
    init_scan_code(REDKEY_PAD_3);
    init_scan_code(REDKEY_PAD_0);
    init_scan_code(REDKEY_PAD_POINT);

    init_scan_code(REDKEY_EUROPEAN);
    init_scan_code(REDKEY_F11);
    init_scan_code(REDKEY_F12);

    init_scan_code(REDKEY_JAPANESE_HIRAGANA_KATAKANA);
    init_scan_code(REDKEY_JAPANESE_BACKSLASH);
    init_scan_code(REDKEY_JAPANESE_HENKAN);
    init_scan_code(REDKEY_JAPANESE_MUHENKAN);
    init_scan_code(REDKEY_JAPANESE_YEN);

    init_korean_scan_code(REDKEY_KOREAN_HANGUL);
    init_korean_scan_code(REDKEY_KOREAN_HANGUL_HANJA);

    init_escape_scan_code(REDKEY_ESCAPE_BASE);
    init_escape_scan_code(REDKEY_PAD_ENTER);
    init_escape_scan_code(REDKEY_R_CTRL);
    init_escape_scan_code(REDKEY_MUTE);
    init_escape_scan_code(REDKEY_FAKE_L_SHIFT);
    init_escape_scan_code(REDKEY_VOLUME_DOWN);
    init_escape_scan_code(REDKEY_VOLUME_UP);
    init_escape_scan_code(REDKEY_PAD_DIVIDE);
    init_escape_scan_code(REDKEY_FAKE_R_SHIFT);
    init_escape_scan_code(REDKEY_CTRL_PRINT_SCREEN);
    init_escape_scan_code(REDKEY_R_ALT);
    init_escape_scan_code(REDKEY_CTRL_BREAK);
    init_escape_scan_code(REDKEY_HOME);
    init_escape_scan_code(REDKEY_UP);
    init_escape_scan_code(REDKEY_PAGEUP);
    init_escape_scan_code(REDKEY_LEFT);
    init_escape_scan_code(REDKEY_RIGHT);
    init_escape_scan_code(REDKEY_END);
    init_escape_scan_code(REDKEY_DOWN);
    init_escape_scan_code(REDKEY_PAGEDOWN);
    init_escape_scan_code(REDKEY_INSERT);
    init_escape_scan_code(REDKEY_DELETE);
    init_escape_scan_code(REDKEY_LEFT_CMD);
    init_escape_scan_code(REDKEY_RIGHT_CMD);
    init_escape_scan_code(REDKEY_MENU);

    init_pause_scan_code();
}

class InitGlobals {
public:
    InitGlobals()
    {
        InputsChannel::init_scan_table();
    }
};

static InitGlobals init_globals;

class InputsFactory: public ChannelFactory {
public:
    InputsFactory() : ChannelFactory(SPICE_CHANNEL_INPUTS) {}
    virtual RedChannel* construct(RedClient& client, uint32_t id)
    {
        return new InputsChannel(client, id);
    }
};

static InputsFactory factory;

ChannelFactory& InputsChannel::Factory()
{
    return factory;
}
