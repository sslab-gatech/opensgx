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

#ifndef _H_HOT_KEYS
#define _H_HOT_KEYS

#include "common.h"
#include "red_key.h"

typedef std::map<std::string, int> CommandsMap;

struct HotkeyKey {
    RedKey main;
    RedKey alter;
};

typedef std::vector<HotkeyKey> HotkeySet;
typedef std::map<int, HotkeySet> HotKeys;

class HotKeysParser {
public:
    HotKeysParser(const std::string& hotkeys, const CommandsMap& commands_map);
    const HotKeys& get() { return _hot_keys;}

private:
    void add_hotkey(const std::string& hotkey, const CommandsMap& commands_map);
    void parse_keys(int command_id, const std::string& hotkey);
    void add_key(HotkeySet& keys, const char* key);

private:
    HotKeys _hot_keys;
};

#endif
