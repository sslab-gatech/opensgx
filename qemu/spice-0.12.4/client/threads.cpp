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
#include "threads.h"
#include "utils.h"
#include "debug.h"
#ifdef WIN32
#include <sys/timeb.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef __MINGW32__
//workaround for what I think is a mingw bug: it has a prototype for
//_ftime_s in its headers, but no symbol for it at link time.
//The #define from common.h cannot be used since it breaks other mingw
//headers if any are included after the #define.
#define _ftime_s _ftime
#endif

Thread::Thread(thread_main_t thread_main, void* opaque)
{
    int r = pthread_create(&_thread, NULL, thread_main, opaque);
    if (r) {
        THROW("failed %d", r);
    }
}

void Thread::join()
{
    pthread_join(_thread, NULL);
}

static inline void rel_time(struct timespec& time, uint64_t delta_nano)
{
#ifdef WIN32
    struct _timeb now;
    _ftime_s(&now);
    time.tv_sec = (long)now.time;
    time.tv_nsec = now.millitm * 1000 * 1000;
#elif defined(HAVE_CLOCK_GETTIME)
    clock_gettime(CLOCK_MONOTONIC, &time);
#else
    struct timeval tv;
    gettimeofday(&tv,NULL);
    time.tv_sec = tv.tv_sec;
    time.tv_nsec = tv.tv_usec*1000;
#endif
    delta_nano += (uint64_t)time.tv_sec * 1000 * 1000 * 1000;
    delta_nano += time.tv_nsec;
    time.tv_sec = long(delta_nano / (1000 * 1000 * 1000));
    time.tv_nsec = long(delta_nano % (1000 * 1000 * 1000));
}

void Lock::timed_lock(uint64_t timout_nano)
{
    struct timespec time;
    int r;

    rel_time(time, timout_nano);
    if ((r = pthread_mutex_timedlock(_mutex.get(), &time))) {
        _locked = false;
        if (r != ETIMEDOUT) {
            THROW("failed %d", r);
        }
        return;
    }
    _locked = true;
}

Condition::Condition()
{
#ifdef WIN32
    pthread_cond_init(&_condition, NULL);
#else
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    int r;
    if ((r = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC))) {
        THROW("set clock failed %d", r);
    }
    pthread_cond_init(&_condition, &attr);
    pthread_condattr_destroy(&attr);
#endif
}

bool Condition::timed_wait(Lock& lock, uint64_t nano)
{
    struct timespec time;
    rel_time(time, nano);
    int r = pthread_cond_timedwait(&_condition, lock.get(), &time);
    if (r) {
        if (r != ETIMEDOUT) {
            THROW("failed %d", r);
        }
        return false;
    }
    return true;
}

Mutex::Mutex(Type type)
{
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    if (type == NORMAL) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    } else if (type == RECURSIVE) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    } else {
        THROW("invalid type %d", type);
    }

    int r;
    if ((r = pthread_mutex_init(&_mutex, &attr))) {
        THROW("int failed %d", r);
    }
    pthread_mutexattr_destroy(&attr);
}

Mutex::~Mutex()
{
    pthread_mutex_destroy(&_mutex);
}
