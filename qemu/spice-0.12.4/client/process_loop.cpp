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
#include "process_loop.h"
#include "debug.h"
#include "platform.h"
#include "utils.h"

SyncEvent::SyncEvent()
    : _err (false)
    , _ready (false)
{
}

SyncEvent::~SyncEvent()
{
}

void SyncEvent::response(AbstractProcessLoop& events_loop)
{
    try {
        do_response(events_loop);
    } catch (Exception& e) {
        LOG_WARN("unhandled exception: %s", e.what());
        _err = true;
    } catch (...) {
        _err = true;
    }
    Lock lock(_mutex);
    _ready = true;
    _condition.notify_one();
}

void SyncEvent::wait()
{
#ifdef RED_DEBUG
    ASSERT(_process_loop && !_process_loop->is_same_thread(pthread_self()));
#endif
    Lock lock(_mutex);
    while (!_ready) {
        _condition.wait(lock);
    }
}

class ProcessLoop::QuitEvent: public Event {
public:
    QuitEvent(int error_code) : _error_code(error_code) {}
    virtual void response(AbstractProcessLoop& events_loop);
private:
    int _error_code;
};

void ProcessLoop::QuitEvent::response(AbstractProcessLoop& events_loop)
{
    events_loop.do_quit(_error_code);
}

/* EventsQueue */

EventsQueue::EventsQueue(AbstractProcessLoop& owner)
    : _events_gen (0)
    , _owner (owner)
{
}

EventsQueue::~EventsQueue()
{
    clear_queue();
}

void EventsQueue::clear_queue()
{
    Lock lock(_events_lock);
    while (!_events.empty()) {
        Event* event = _events.front();
        _events.pop_front();
        event->unref();
    }
}

int EventsQueue::push_event(Event* event)
{
    Lock lock(_events_lock);
    _events.push_back(event);
    event->set_generation(_events_gen);
    event->ref();
#ifdef RED_DEBUG
    event->set_process_loop(&_owner);
#endif
    return _events.size();
}

void EventsQueue::process_events()
{
    _events_gen++;

    for (;;) {
        Event* event;
        Lock lock(_events_lock);
        if (_events.empty()) {
            return;
        }
        event = _events.front();
        if (event->get_generation() == _events_gen) {
            return;
        }
        _events.pop_front();

        lock.unlock();
        event->response(_owner);
        event->unref();
    }
}

bool EventsQueue::is_empty()
{
    Lock lock(_events_lock);
    return _events.empty();
}

/* Timers Queue */

Timer::Timer()
    : _is_armed (false)
{
}

Timer::~Timer()
{
}

void Timer::arm(uint32_t msec)
{
    _interval = msec;
    _expiration = get_now();
    calc_next_expiration_time();
    _is_armed = true;
}

void Timer::disarm()
{
    _is_armed = false;
}

#define TIMER_COMPENSATION

void Timer::calc_next_expiration_time(uint64_t now)
{
#ifndef TIMER_COMPENSATION
    _expiratoin = now;
#endif
    calc_next_expiration_time();
#ifdef TIMER_COMPENSATION
    if (_expiration <= now) {
        _expiration = now;
        calc_next_expiration_time();
     }
#endif
}

uint64_t Timer::get_now()
{
    return (Platform::get_monolithic_time() / 1000 / 1000);
}

TimersQueue::TimersQueue(AbstractProcessLoop& owner)
    : _owner (owner)
{
}

TimersQueue::~TimersQueue()
{
    clear_queue();
}

void TimersQueue::clear_queue()
{
    RecurciveLock lock(_timers_lock);
    TimersSet::iterator iter;
    for (iter = _armed_timers.begin(); iter != _armed_timers.end(); iter++) {
        (*iter)->disarm();
    }
    _armed_timers.clear();
}

void TimersQueue::activate_interval_timer(Timer* timer, unsigned int millisec)
{
    RecurciveLock lock(_timers_lock);
    timer->ref();
    deactivate_interval_timer(timer);
    timer->arm(millisec);
    _armed_timers.insert(timer);
}

void TimersQueue::deactivate_interval_timer(Timer* timer)
{
    RecurciveLock lock(_timers_lock);
    if (timer->is_armed()) {
#ifdef  RED_DEBUG
        int ret =
#endif
        _armed_timers.erase(timer);
        ASSERT(ret);
        timer->disarm();
        timer->unref();
    }
}

unsigned int TimersQueue::get_soonest_timeout()
{
    RecurciveLock lock(_timers_lock);
    TimersSet::iterator iter;
    iter = _armed_timers.begin();
    if (iter == _armed_timers.end()) {
        return INFINITE;
    }

    uint64_t now = Timer::get_now();
    uint64_t next_time = (*iter)->get_expiration();

    if (next_time <= now) {
        return 0;
    }
    return (int)(next_time - now);
}


void TimersQueue::timers_action()
{
    RecurciveLock lock(_timers_lock);
    uint64_t now = Timer::get_now();
    TimersSet::iterator iter;

    while (((iter = _armed_timers.begin()) != _armed_timers.end()) &&
           ((*iter)->get_expiration() <= now)) {
        Timer* timer = *iter;
        _armed_timers.erase(iter);
        timer->calc_next_expiration_time(now);
        _armed_timers.insert(timer);
        timer->response(_owner);
    }
}

ProcessLoop::ProcessLoop(void* owner)
    : _events_queue (*this)
    , _timers_queue (*this)
    , _owner (owner)
    , _quitting (false)
    , _exit_code (0)
    , _started (false)
{
    _event_sources.add_trigger(_wakeup_trigger);
}

ProcessLoop::~ProcessLoop()
{
    _event_sources.remove_trigger(_wakeup_trigger);
}

int ProcessLoop::run()
{
    _thread = pthread_self();
    _started = true;
    on_start_running();
    for (;;) {
        if (_event_sources.wait_events(_timers_queue.get_soonest_timeout())) {
            _quitting = true;
            break;
        }
        _timers_queue.timers_action();
        process_events_queue();
        if (_quitting) {
            break;
        }
    }

    return _exit_code;
}

void ProcessLoop::do_quit(int error_code)
{
    ASSERT(!_started || pthread_equal(pthread_self(), _thread));
    if (_quitting) {
        return;
    }
    _quitting = true;
    _exit_code = error_code;
}

void ProcessLoop::quit(int error_code)
{
    AutoRef<QuitEvent> quit_event(new QuitEvent(error_code));
    push_event(*quit_event);
}

void ProcessLoop::process_events_queue()
{
    ASSERT(!_started || pthread_equal(pthread_self(), _thread));
    _events_queue.process_events();
    if (!_events_queue.is_empty()) {
        wakeup();
    }
}

void ProcessLoop::wakeup()
{
    _wakeup_trigger.trigger();
}

void ProcessLoop::add_trigger(EventSources::Trigger& trigger)
{
    ASSERT(!_started || pthread_equal(pthread_self(), _thread));
    _event_sources.add_trigger(trigger);
}

void ProcessLoop::remove_trigger(EventSources::Trigger& trigger)
{
    ASSERT(!_started || pthread_equal(pthread_self(), _thread));
    _event_sources.remove_trigger(trigger);
}

void ProcessLoop::add_socket(EventSources::Socket& socket)
{
    ASSERT(!_started || pthread_equal(pthread_self(), _thread));
    _event_sources.add_socket(socket);
}

void ProcessLoop::remove_socket(EventSources::Socket& socket)
{
    ASSERT(!_started || pthread_equal(pthread_self(), _thread));
    _event_sources.remove_socket(socket);
}

void ProcessLoop::add_file(EventSources::File& file)
{
    ASSERT(!_started || pthread_equal(pthread_self(), _thread));
    _event_sources.add_file(file);
}

void ProcessLoop::remove_file(EventSources::File& file)
{
    ASSERT(!_started || pthread_equal(pthread_self(), _thread));
    _event_sources.remove_file(file);
}

void ProcessLoop::add_handle(EventSources::Handle& handle)
{
    ASSERT(!_started || pthread_equal(pthread_self(), _thread));
    _event_sources.add_handle(handle);
}

void ProcessLoop::remove_handle(EventSources::Handle& handle)
{
    ASSERT(!_started || pthread_equal(pthread_self(), _thread));
    _event_sources.remove_handle(handle);
}

void ProcessLoop::push_event(Event* event)
{
    int queue_size = _events_queue.push_event(event);
    if (queue_size == 1) { // queue was empty before the push
        wakeup();
    }
}

void ProcessLoop::activate_interval_timer(Timer* timer, unsigned int millisec)
{
    _timers_queue.activate_interval_timer(timer, millisec);

    if (_started && !pthread_equal(pthread_self(), _thread)) {
        wakeup();
    }
}

void ProcessLoop::deactivate_interval_timer(Timer* timer)
{
    _timers_queue.deactivate_interval_timer(timer);
}

unsigned ProcessLoop::get_soonest_timeout()
{
    return _timers_queue.get_soonest_timeout();
}

void ProcessLoop::timers_action()
{
    _timers_queue.timers_action();
}
