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

#ifndef _H_PROCESS_LOOP
#define _H_PROCESS_LOOP

#include "common.h"

#include <set>

#include "event_sources.h"
#include "threads.h"

class AbstractProcessLoop {
public:
    virtual ~AbstractProcessLoop() {}
    virtual int run() = 0;
    virtual void do_quit(int error_code) = 0;
    virtual void* get_owner() = 0;
    virtual bool is_same_thread(pthread_t thread) = 0;
};

class EventBase {
public:
    EventBase() : _refs (1) {}

    virtual void response(AbstractProcessLoop& events_loop) = 0;

    EventBase* ref() { ++_refs; return this;}
    void unref() {if (--_refs == 0) delete this;}

protected:
    virtual ~EventBase() {}

private:
    AtomicCount _refs;
};

class EventsQueue;

class Event: public EventBase {
#ifdef RED_DEBUG
public:
    Event() : _process_loop (NULL) {}

private:
    void set_process_loop(AbstractProcessLoop* process_loop) { _process_loop = process_loop;}

protected:
    AbstractProcessLoop* _process_loop;
#endif

private:
    void set_generation(uint32_t gen) { _generation = gen;}
    uint32_t get_generation() { return _generation;}

private:
    uint32_t _generation;

    friend class EventsQueue;
};

class EventsQueue {
public:
    EventsQueue(AbstractProcessLoop& owner);
    virtual ~EventsQueue();
    /* return the size of the queue (post-push) */
    int push_event(Event* event);
    void process_events();
    bool is_empty();

private:
    void clear_queue();

private:
    std::list<Event*> _events;
    Mutex _events_lock;
    uint32_t _events_gen;

    AbstractProcessLoop& _owner;
};

class SyncEvent: public Event {
public:
    SyncEvent();

    void wait();
    bool success() { return !_err;}

    virtual void do_response(AbstractProcessLoop& events_loop) {}

protected:
    virtual ~SyncEvent();

private:
    virtual void response(AbstractProcessLoop& events_loop);

private:
    Mutex _mutex;
    Condition _condition;
    bool _err;
    bool _ready;
};

class TimersQueue;

class Timer: public EventBase {
public:
    Timer();
    bool is_armed() {return _is_armed;}

protected:
    virtual ~Timer();

private:
    void arm(uint32_t msec);
    void disarm();
    uint64_t get_expiration() const { return _expiration;}
    void calc_next_expiration_time() { _expiration += _interval;}
    void calc_next_expiration_time(uint64_t now);

    static uint64_t get_now();

private:
    bool _is_armed;
    uint32_t _interval;
    uint64_t _expiration;

    class Compare {
    public:
        bool operator () (const Timer* timer1, const Timer* timer2) const
        {
            if (timer1->get_expiration() < timer2->get_expiration()) {
                return true;
            } else if (timer1->get_expiration() > timer2->get_expiration()) {
                return false;
            } else { // elements must be unique (for insertion into set)
                return timer1 < timer2;
            }
        }
    };

    friend class TimersQueue;
};

class TimersQueue {
public:
    TimersQueue(AbstractProcessLoop& owner);
    virtual ~TimersQueue();

    void activate_interval_timer(Timer* timer, unsigned int millisec);
    void deactivate_interval_timer(Timer* timer);

    unsigned int get_soonest_timeout();
    void timers_action();

private:
    void clear_queue();

private:
    typedef std::set<Timer*, Timer::Compare> TimersSet;
    TimersSet _armed_timers;
    RecurciveMutex _timers_lock;
    AbstractProcessLoop& _owner;
};

class ProcessLoop: public AbstractProcessLoop {
public:
    class QuitEvent;
    ProcessLoop(void* owner);
    virtual ~ProcessLoop();
    int run();

    void quit(int error_code);

    /* Event sources to track. Note: the following methods are not thread safe, thus,
       they mustn't be called from other thread than the process loop thread. */
    void add_trigger(EventSources::Trigger& trigger);
    void remove_trigger(EventSources::Trigger& trigger);
    void add_socket(EventSources::Socket& socket);
    void remove_socket(EventSources::Socket& socket);
    void add_file(EventSources::File& file);
    void remove_file(EventSources::File& file);
    void add_handle(EventSources::Handle& handle);
    void remove_handle(EventSources::Handle& handle);

    /* events queue */
    void push_event(Event* event);

    void activate_interval_timer(Timer* timer, unsigned int millisec);
    void deactivate_interval_timer(Timer* timer);

    void process_events_queue();
    /* can be used for handling timers in modal loop state in Windows (mainly,
       for updating the screen) */
    unsigned int get_soonest_timeout();
    void timers_action();

    void* get_owner() { return _owner;}

    bool is_same_thread(pthread_t thread) { return _started && pthread_equal(_thread, thread);}

protected:
    class WakeupTrigger: public EventSources::Trigger {
    public:
        virtual void on_event() {}
    };

    virtual void on_start_running() {}
    void wakeup();
    void do_quit(int error_code);

    friend class QuitEvent; // allowing access to quit

private:
    EventSources _event_sources;
    EventsQueue _events_queue;
    TimersQueue _timers_queue;

    WakeupTrigger _wakeup_trigger;

    void* _owner;

    bool _quitting;
    int _exit_code;
    bool _started;
    pthread_t _thread;
};

#endif
