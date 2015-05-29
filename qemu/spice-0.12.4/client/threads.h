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

#ifndef _H_THREADS
#define _H_THREADS

#include "pthread.h"
#include "atomic_count.h"

class Thread {
public:
    typedef void* (*thread_main_t)(void*);

    Thread(thread_main_t thread_main, void* opaque);
    void join();

private:
    pthread_t _thread;
};

class Mutex {
public:
    enum Type {
        NORMAL,
        RECURSIVE,
    };

    Mutex(Type = NORMAL);
    virtual ~Mutex();

private:
    friend class Lock;
    pthread_mutex_t* get() {return &_mutex;}

private:
    pthread_mutex_t _mutex;
};

class Lock {
public:
    Lock(Mutex& mutex)
        : _locked (true)
        , _mutex (mutex)
    {
        pthread_mutex_lock(_mutex.get());
    }

    Lock(Mutex& mutex, uint64_t timout_nano)
        : _mutex (mutex)
    {
        if (!pthread_mutex_trylock(_mutex.get())) {
            _locked = true;
            return;
        }
        timed_lock(timout_nano);
    }

    ~Lock()
    {
        unlock();
    }

    void unlock()
    {
        if (_locked) {
            pthread_mutex_unlock(_mutex.get());
            _locked = false;
        }
    }

    bool is_locked() { return _locked;}

private:
    friend class Condition;
    pthread_mutex_t* get() {return _mutex.get();}
    void timed_lock(uint64_t timout_nano);

private:
    bool _locked;
    Mutex& _mutex;
};

class RecurciveMutex: public Mutex {
public:
    RecurciveMutex() : Mutex(Mutex::RECURSIVE) {}
};

typedef Lock RecurciveLock;
class Condition {
public:
    Condition();

    ~Condition()
    {
        pthread_cond_destroy(&_condition);
    }

    void notify_one()
    {
        pthread_cond_signal(&_condition);
    }

    void notify_all()
    {
        pthread_cond_broadcast(&_condition);
    }

    void wait(Lock& lock)
    {
        pthread_cond_wait(&_condition, lock.get());
    }

    bool timed_wait(Lock& lock, uint64_t nano);

private:
    pthread_cond_t _condition;
};


#endif
