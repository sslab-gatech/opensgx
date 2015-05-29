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
#include "hot_keys.h"
#include "utils.h"
#include "debug.h"

HotKeysParser::HotKeysParser(const std::string& hotkeys, const CommandsMap& commands_map)
{
    DBG(0, "hotkeys = %s", hotkeys.c_str());

    std::istringstream is(hotkeys);
    std::string hotkey;
    while (std::getline(is, hotkey, ',')) {
        add_hotkey(hotkey, commands_map);
    }
}

void HotKeysParser::parse_keys(int command_id, const std::string& hotkey)
{
    HotkeySet& keys = _hot_keys[command_id];
    std::istringstream is(hotkey);
    std::string key;
    while (std::getline(is, key, '+')) {
        add_key(keys, key.c_str());
    }
}

void HotKeysParser::add_key(HotkeySet& keys, const char* key)
{
    ASSERT(key != NULL);

    static const struct {
        const char* name;
        RedKey main;
        RedKey alter;
    } keyboard[] = {
        { "alt", REDKEY_R_ALT, REDKEY_L_ALT },
        { "ralt", REDKEY_R_ALT, REDKEY_INVALID },
        { "rightalt", REDKEY_R_ALT, REDKEY_INVALID },
        { "right-alt", REDKEY_R_ALT, REDKEY_INVALID },
        { "lalt", REDKEY_L_ALT, REDKEY_INVALID },
        { "leftalt", REDKEY_L_ALT, REDKEY_INVALID },
        { "left-alt", REDKEY_L_ALT, REDKEY_INVALID },
        { "ctrl", REDKEY_R_CTRL, REDKEY_L_CTRL },
        { "rctrl", REDKEY_R_CTRL, REDKEY_INVALID },
        { "rightctrl", REDKEY_R_CTRL, REDKEY_INVALID },
        { "right-ctrl", REDKEY_R_CTRL, REDKEY_INVALID },
        { "lctrl", REDKEY_L_CTRL, REDKEY_INVALID },
        { "leftctrl", REDKEY_L_CTRL, REDKEY_INVALID },
        { "left-ctrl", REDKEY_L_CTRL, REDKEY_INVALID },
        { "shift", REDKEY_R_SHIFT, REDKEY_L_SHIFT },
        { "rshift", REDKEY_R_SHIFT, REDKEY_INVALID },
        { "rightshift", REDKEY_R_SHIFT, REDKEY_INVALID },
        { "right-shift", REDKEY_R_SHIFT, REDKEY_INVALID },
        { "lshift", REDKEY_L_SHIFT, REDKEY_INVALID },
        { "leftshift", REDKEY_L_SHIFT, REDKEY_INVALID },
        { "left-shift", REDKEY_L_SHIFT, REDKEY_INVALID },
        { "cmd", REDKEY_RIGHT_CMD, REDKEY_LEFT_CMD },
        { "rcmd", REDKEY_RIGHT_CMD, REDKEY_INVALID },
        { "rightcmd", REDKEY_RIGHT_CMD, REDKEY_INVALID },
        { "right-cmd", REDKEY_RIGHT_CMD, REDKEY_INVALID },
        { "lcmd", REDKEY_LEFT_CMD, REDKEY_INVALID },
        { "leftcmd", REDKEY_LEFT_CMD, REDKEY_INVALID },
        { "left-cmd", REDKEY_LEFT_CMD, REDKEY_INVALID },
        { "win", REDKEY_RIGHT_CMD, REDKEY_LEFT_CMD },
        { "rwin", REDKEY_RIGHT_CMD, REDKEY_INVALID },
        { "rightwin", REDKEY_RIGHT_CMD, REDKEY_INVALID },
        { "right-win", REDKEY_RIGHT_CMD, REDKEY_INVALID },
        { "lwin", REDKEY_LEFT_CMD, REDKEY_INVALID },
        { "leftwin", REDKEY_LEFT_CMD, REDKEY_INVALID },
        { "left-win", REDKEY_LEFT_CMD, REDKEY_INVALID },
        { "esc", REDKEY_ESCAPE, REDKEY_INVALID },
        { "escape", REDKEY_ESCAPE, REDKEY_INVALID },
        { "ins", REDKEY_INSERT, REDKEY_INVALID },
        { "insert", REDKEY_INSERT, REDKEY_INVALID },
        { "del", REDKEY_DELETE, REDKEY_INVALID },
        { "delete", REDKEY_DELETE, REDKEY_INVALID },
        { "pgup", REDKEY_PAGEUP, REDKEY_INVALID },
        { "pageup", REDKEY_PAGEUP, REDKEY_INVALID },
        { "pgdn", REDKEY_PAGEDOWN, REDKEY_INVALID },
        { "pagedown", REDKEY_PAGEDOWN, REDKEY_INVALID },
        { "home", REDKEY_HOME, REDKEY_INVALID },
        { "end", REDKEY_END, REDKEY_INVALID },
        { "space", REDKEY_SPACE, REDKEY_INVALID },
        { "enter", REDKEY_ENTER, REDKEY_INVALID },
        { "tab", REDKEY_TAB, REDKEY_INVALID },
        { "f1", REDKEY_F1, REDKEY_INVALID },
        { "f2", REDKEY_F2, REDKEY_INVALID },
        { "f3", REDKEY_F3, REDKEY_INVALID },
        { "f4", REDKEY_F4, REDKEY_INVALID },
        { "f5", REDKEY_F5, REDKEY_INVALID },
        { "f6", REDKEY_F6, REDKEY_INVALID },
        { "f7", REDKEY_F7, REDKEY_INVALID },
        { "f8", REDKEY_F8, REDKEY_INVALID },
        { "f9", REDKEY_F9, REDKEY_INVALID },
        { "f10", REDKEY_F10, REDKEY_INVALID },
        { "f11", REDKEY_F11, REDKEY_INVALID },
        { "f12", REDKEY_F12, REDKEY_INVALID }
    };

    for (unsigned i = 0; i < (sizeof(keyboard) / sizeof(keyboard[0])); ++i) {
        if (strcasecmp(key, keyboard[i].name) == 0) {
            HotkeyKey hotkey;
            hotkey.main = keyboard[i].main;
            hotkey.alter = keyboard[i].alter;
            DBG(0, "keys = %s", keyboard[i].name);
            keys.push_back(hotkey);
            return;
        }
    }
    THROW("unknown key name %s", key);
}

void HotKeysParser::add_hotkey(const std::string& hotkey, const CommandsMap& commands_map)
{
    std::string::size_type key_start = hotkey.find('=', 0);
    if (key_start == std::string::npos) {
        THROW("unable to parse hot keys");
    }
    std::string command_name = hotkey.substr(0, key_start);

    if (commands_map.find(command_name) == commands_map.end()) {
        THROW("invalid action bname %s", command_name.c_str());
    }
    int command_id = commands_map.find(command_name)->second;
    std::string keys = hotkey.substr(key_start + 1);
    parse_keys(command_id, keys);
}
