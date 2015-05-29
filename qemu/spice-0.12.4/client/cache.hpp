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

#ifndef _H_CACHE
#define _H_CACHE

#include "utils.h"

/*class Cache::Treat {
    T* get(T*);
    void release(T*);
    const char* name();
};*/

template <class T, class Treat, int HASH_SIZE, class Base = EmptyBase>
class Cache : public Base {
public:
    Cache()
    {
        memset(_hash, 0, sizeof(_hash));
    }

    ~Cache()
    {
        clear();
    }

    void add(uint64_t id, T* data)
    {
        Item** item = &_hash[key(id)];

        while (*item) {
            if ((*item)->id == id) {
                THROW("%s id %" PRIu64 ", double insert", Treat::name(), id);
            }
            item = &(*item)->next;
        }
        *item = new Item(id, data);
    }

    T* get(uint64_t id)
    {
        Item* item = _hash[key(id)];

        while (item && item->id != id) {
            item = item->next;
        }

        if (!item) {
            THROW("%s id %" PRIu64 ", not found", Treat::name(), id);
        }
        return Treat::get(item->data);
    }

    void remove(uint64_t id)
    {
        Item** item = &_hash[key(id)];

        while (*item) {
            if ((*item)->id == id) {
                Item *rm_item = *item;
                *item = rm_item->next;
                delete rm_item;
                return;
            }
            item = &(*item)->next;
        }
        THROW("%s id %" PRIu64 ", not found", Treat::name(), id);
    }

    void clear()
    {
        for (int i = 0; i < HASH_SIZE; i++) {
            while (_hash[i]) {
                Item *item = _hash[i];
                _hash[i] = item->next;
                delete item;
            }
        }
    }

private:
    inline uint32_t key(uint64_t id) {return uint32_t(id) % HASH_SIZE;}

private:
    class Item {
    public:
        Item(uint64_t in_id, T* data)
            : id (in_id)
            , next (NULL)
            , data (Treat::get(data)) {}

        ~Item()
        {
            Treat::release(data);
        }

        uint64_t id;
        Item* next;
        T* data;
    };

    Item* _hash[HASH_SIZE];
};

#endif
