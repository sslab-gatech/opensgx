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

#ifndef _H_SHARED_CACHE
#define _H_SHARED_CACHE

#include "utils.h"
#include "threads.h"

/*class SharedCache::Treat {
    T* get(T*);
    void release(T*);
    const char* name();
};*/

template <class T, class Treat, int HASH_SIZE, class Base = EmptyBase>
class SharedCache : public Base {
public:
    SharedCache()
        : _aborting (false)
    {
        memset(_hash, 0, sizeof(_hash));
    }

    ~SharedCache()
    {
        clear();
    }

    void add(uint64_t id, T* data, bool is_lossy = FALSE)
    {
        Lock lock(_lock);
        Item** item = &_hash[key(id)];

        while (*item) {
            if ((*item)->id == id) {
                (*item)->refs++;
                return;
            }
            item = &(*item)->next;
        }
        *item = new Item(id, data, is_lossy);
        _new_item_cond.notify_all();
    }

    T* get(uint64_t id)
    {
        Lock lock(_lock);
        Item* item = _hash[key(id)];

        for (;;) {
            if (!item) {
                if (_aborting) {
                    THROW("%s aborting", Treat::name());
                }
                _new_item_cond.wait(lock);
                item = _hash[key(id)];
                continue;
            }

            if (item->id != id) {
                item = item->next;
                continue;
            }

            return Treat::get(item->data);
        }
    }

    T* get_lossless(uint64_t id)
    {
        Lock lock(_lock);
        Item* item = _hash[key(id)];

        for (;;) {
            if (!item) {
                if (_aborting) {
                    THROW("%s aborting", Treat::name());
                }
                _new_item_cond.wait(lock);
                item = _hash[key(id)];
                continue;
            }

            if (item->id != id) {
                item = item->next;
                continue;
            }
            break;
        }

        // item has been retreived. Now checking if lossless
        for (;;) {
            if (item->lossy) {
                if (_aborting) {
                    THROW("%s aborting", Treat::name());
                }
                _replace_data_cond.wait(lock);
                continue;
            }

            return Treat::get(item->data);
        }
    }

    void replace(uint64_t id, T* data, bool is_lossy = FALSE)
    {
        Lock lock(_lock);
        Item* item = _hash[key(id)];

        for (;;) {
            if (!item) {
                if (_aborting) {
                    THROW("%s aborting", Treat::name());
                }
                _new_item_cond.wait(lock);
                item = _hash[key(id)];
                continue;
            }

            if (item->id != id) {
                item = item->next;
                continue;
            }

            item->replace(data, is_lossy);
            break;
        }
        _replace_data_cond.notify_all();
    }

    void remove(uint64_t id)
    {
        Lock lock(_lock);
        Item** item = &_hash[key(id)];

        while (*item) {
            if ((*item)->id == id) {
                if (!--(*item)->refs) {
                    Item *rm_item = *item;
                    *item = rm_item->next;
                    delete rm_item;
                }
                return;
            }
            item = &(*item)->next;
        }
        THROW("%s id %" PRIu64 ", not found", Treat::name(), id);
    }

    void clear()
    {
        Lock lock(_lock);
        for (int i = 0; i < HASH_SIZE; i++) {
            while (_hash[i]) {
                Item *item = _hash[i];
                _hash[i] = item->next;
                delete item;
            }
        }
    }

    void abort()
    {
        Lock lock(_lock);
        _aborting = true;
        _new_item_cond.notify_all();
    }

private:
    inline uint32_t key(uint64_t id) {return uint32_t(id) % HASH_SIZE;}

private:
    class Item {
    public:
        Item(uint64_t in_id, T* data, bool is_lossy = FALSE)
            : id (in_id)
            , refs (1)
            , next (NULL)
            , data (Treat::get(data))
            , lossy (is_lossy) {}

        ~Item()
        {
            Treat::release(data);
        }

        void replace(T* new_data, bool is_lossy = FALSE)
        {
            Treat::release(data);
            data = Treat::get(new_data);
            lossy = is_lossy;
        }

        uint64_t id;
        int refs;
        Item* next;
        T* data;
        bool lossy;
    };

    Item* _hash[HASH_SIZE];
    Mutex _lock;
    Condition _new_item_cond;
    Condition _replace_data_cond;
    bool _aborting;
};

#endif
