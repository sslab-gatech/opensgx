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

#ifndef _H_MENU
#define _H_MENU

class CommandTarget {
public:
    virtual void do_command(int command) = 0;
    virtual ~CommandTarget() {}
};

class Menu {
public:
    Menu(CommandTarget& target, const std::string& name, int id = 0);

    enum ItemType {
        MENU_ITEM_TYPE_INVALID,
        MENU_ITEM_TYPE_COMMAND,
        MENU_ITEM_TYPE_MENU,
        MENU_ITEM_TYPE_SEPARATOR,
    };

    enum ItemState {
        MENU_ITEM_STATE_CHECKED = 1 << 0,
        MENU_ITEM_STATE_DIM     = 1 << 1,
    };

    Menu* ref() { _refs++; return this;}
    void unref() { if (!--_refs) delete this;}

    void set_name(const std::string& name) { _name = name;}
    const std::string& get_name() { return _name;}
    CommandTarget& get_target() { return _target;}
    int get_id() { return _id;}

    void add_command(const std::string& name, int cmd_id, int state = 0);
    void add_separator();
    void add_sub(Menu* sub);

    void remove_command(int cmd_id);
    void remove_sub(Menu* menu);

    ItemType item_type_at(int pos);
    void command_at(int pos, std::string& name, int& cmd_id, int& state);
    Menu* sub_at(int pos);
    Menu* find_sub(int id);

    void clear();

private:
    virtual ~Menu();

    class MenuCommand {
    public:
        MenuCommand(const std::string& name, int cmd_id, int state)
            : _name (name)
            , _cmd_id (cmd_id)
            , _state (state)
        {
        }

        const std::string& get_name() { return _name;}
        int get_cmd_id() { return _cmd_id;}
        int get_state() { return _state;}

    private:
        std::string _name;
        int _cmd_id;
        int _state;
    };

    struct MenuItem {
        ItemType type;
        void *obj;
    };

    void add_item(MenuItem& item);

private:
    int _refs;
    CommandTarget& _target;
    std::string _name;
    std::vector<MenuItem> _items;
    int _id;
};

#endif
