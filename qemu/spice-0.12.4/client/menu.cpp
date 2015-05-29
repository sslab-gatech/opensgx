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
#include "menu.h"
#include "utils.h"
#include "debug.h"

Menu::Menu(CommandTarget& target, const std::string& name, int id)
    : _refs (1)
    , _target (target)
    , _name (name)
    , _id (id)
{
}

Menu::~Menu()
{
    clear();
}

void Menu::add_item(MenuItem& item)
{
    int pos = _items.size();
    _items.resize(pos + 1);
    _items[pos] = item;
}

void Menu::add_command(const std::string& name, int cmd_id, int state)
{
    MenuCommand* cmd = new MenuCommand(name, cmd_id, state);
    MenuItem item;
    item.type = MENU_ITEM_TYPE_COMMAND;
    item.obj = cmd;
    add_item(item);
}

void Menu::add_separator()
{
    MenuItem item;
    item.type = MENU_ITEM_TYPE_SEPARATOR;
    item.obj = NULL;
    add_item(item);
}

void Menu::add_sub(Menu* menu)
{
    ASSERT(menu);
    MenuItem item;
    item.type = MENU_ITEM_TYPE_MENU;
    item.obj = menu->ref();
    add_item(item);
}

void Menu::remove_command(int cmd_id)
{
    for (unsigned int i = 0; i < _items.size(); i++) {
        if (_items[i].type == MENU_ITEM_TYPE_COMMAND &&
                ((MenuCommand*)_items[i].obj)->get_cmd_id() == cmd_id) {
            delete (MenuCommand*)_items[i].obj;
            _items.erase(_items.begin() + i);
            return;
        }
    }
}

void Menu::remove_sub(Menu* menu)
{
    for (unsigned int i = 0; i < _items.size(); i++) {
        if (_items[i].type == MENU_ITEM_TYPE_MENU && (Menu*)_items[i].obj == menu) {
            ((Menu*)_items[i].obj)->unref();
            _items.erase(_items.begin() + i);
            return;
        }
    }
}

Menu::ItemType Menu::item_type_at(int pos)
{
    if (pos >= (int)_items.size()) {
        return MENU_ITEM_TYPE_INVALID;
    }
    return _items[pos].type;
}

void Menu::command_at(int pos, std::string& name, int& cmd_id, int& state)
{
    if (_items[pos].type != MENU_ITEM_TYPE_COMMAND) {
        THROW("incorrect item type");
    }
    MenuCommand* cmd = (MenuCommand*)_items[pos].obj;
    name = cmd->get_name();
    cmd_id = cmd->get_cmd_id();
    state = cmd->get_state();
}

Menu* Menu::sub_at(int pos)
{
    if (_items[pos].type != MENU_ITEM_TYPE_MENU) {
        THROW("incorrect item type");
    }
    return ((Menu*)_items[pos].obj)->ref();
}

Menu* Menu::find_sub(int id)
{
    Menu* sub;

    if (_id == id) {
        return ref();
    }
    for (unsigned int i = 0; i < _items.size(); i++) {
        if (_items[i].type == MENU_ITEM_TYPE_MENU && (sub = ((Menu*)_items[i].obj)->find_sub(id))) {
            return sub;
        }
    }
    return NULL;
}

void Menu::clear()
{
    for (unsigned int i = 0; i < _items.size(); i++) {
        if (_items[i].type == MENU_ITEM_TYPE_COMMAND) {
            delete (MenuCommand*)_items[i].obj;
        } else if (_items[i].type == MENU_ITEM_TYPE_MENU) {
            ((Menu*)_items[i].obj)->unref();
        }
    }
    _items.clear();
}
