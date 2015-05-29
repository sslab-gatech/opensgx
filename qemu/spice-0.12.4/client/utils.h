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

#ifndef _H_UTILS
#define _H_UTILS

#include "common.h"
#include <spice/error_codes.h>
#include <spice/macros.h>

class Exception: public std::exception {
public:
    Exception(const std::string& str, int error_code) : _mess (str), _erro_code (error_code) {}
    Exception(const std::string& str) : _mess (str) { _erro_code = SPICEC_ERROR_CODE_ERROR;}
    virtual ~Exception() throw () {}
    virtual const char* what() const throw () {return _mess.c_str();}
    int get_error_code() {return _erro_code;}

private:
    std::string _mess;
    int _erro_code;
};

#define THROW(format, ...)  {                                   \
    std::string exption_string;                                 \
    string_printf(exption_string, format, ## __VA_ARGS__ );     \
    throw Exception(exption_string);                            \
}

#define THROW_ERR(err, format, ...)  {                          \
    std::string exption_string;                                 \
    string_printf(exption_string, format, ## __VA_ARGS__ );     \
    throw Exception(exption_string, err);                       \
}


template <class T>
class AutoRef {
public:
    AutoRef() : _obj (NULL) {}
    AutoRef(T* obj) : _obj (obj) {}
    ~AutoRef() { if (_obj) _obj->unref();}
    T* operator * () {return _obj;}

    void reset(T* obj)
    {
        if (_obj) {
            _obj->unref();
        }
        _obj = obj;
    }

    T* release()
    {
        T* ret = _obj;
        _obj = NULL;
        return ret;
    }

private:
    T* _obj;
};

static inline int test_bit(const void* addr, int bit)
{
    return !!(((uint32_t*)addr)[bit >> 5] & (1 << (bit & 0x1f)));
}

static inline int test_bit_be(const void* addr, int bit)
{
    return !!(((uint8_t*)addr)[bit >> 3] & (0x80 >> (bit & 0x07)));
}

static inline void set_bit(const void* addr, int bit)
{
    ((uint8_t*)addr)[bit >> 5] |= (0x1 << (bit & 0x1f));
}

static inline void set_bit_be(const void* addr, int bit)
{
    ((uint8_t*)addr)[bit >> 3] |= (0x80 >> (bit & 0x07));
}

int str_to_port(const char *str);

void string_vprintf(std::string& str, const char* format, va_list ap);
SPICE_GNUC_PRINTF(2, 3) void string_printf(std::string& str, const char *format, ...);

template<class T>
class FreeObject {
public:
    void operator () (T p) { delete p;}
};

template<class T, class FreeRes = FreeObject<T> >
class _AutoRes {
public:
    _AutoRes() : _res (NULL) {}
    _AutoRes(T* res) : _res (res) {}
    ~_AutoRes() { set(NULL); }

    void set(T* res) {if (_res) _free_res(_res); _res = res; }
    T* get() {return _res;}
    T* operator -> () { return _res;}
    T* release() {T* tmp = _res; _res = NULL; return tmp; }
    bool valid() { return !!_res; }

private:
    _AutoRes(const _AutoRes&);
    _AutoRes& operator = (const _AutoRes&);

private:
    T* _res;
    FreeRes _free_res;
};

template<class T>
class AutoArray {
public:
    AutoArray() : _array (NULL) {}
    AutoArray(T* array) : _array (array) {}
    ~AutoArray() { delete[] _array;}

    void set(T* array) { delete[] _array; _array = array;}
    T* get() {return _array;}
    T* release() {T* tmp = _array; _array = NULL; return tmp; }
    T& operator [] (int i) {return _array[i];}

private:
    T* _array;
};

class EmptyBase {
};

#endif
