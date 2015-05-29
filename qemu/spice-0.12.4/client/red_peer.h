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

#ifndef _H_REDPEER
#define _H_REDPEER

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <spice/protocol.h>
#include "common/marshaller.h"
#include "common/ssl_verify.h"

#include "common.h"
#include "process_loop.h"
#include "threads.h"
#include "platform_utils.h"
#include "debug.h"

class RedPeer: protected EventSources::Socket {
public:
    RedPeer();
    virtual ~RedPeer();

    class InMessage;
    class CompoundInMessage;
    class OutMessage;
    class DisconnectedException {};

    class HostAuthOptions {
    public:

        typedef std::vector<uint8_t> PublicKey;
        typedef std::pair<std::string, std::string> CertFieldValuePair;
        typedef std::list<CertFieldValuePair> CertFieldValueList;

        HostAuthOptions() : type_flags(SPICE_SSL_VERIFY_OP_NONE) {}

    public:

        SPICE_SSL_VERIFY_OP type_flags;

        PublicKey host_pubkey;
        std::string host_subject;
        std::string CA_file;
    };

    class ConnectionOptions {
    public:

        enum Type {
            CON_OP_INVALID,
            CON_OP_SECURE,
            CON_OP_UNSECURE,
            CON_OP_BOTH,
        };

        ConnectionOptions(Type in_type, int in_port, int in_sport,
                          int in_protocol,
                          const HostAuthOptions& in_host_auth,
                          const std::string& in_ciphers)
            : type (in_type)
            , unsecure_port (in_port)
            , secure_port (in_sport)
            , protocol (in_protocol)
            , host_auth (in_host_auth)
            , ciphers (in_ciphers)
        {
        }

        virtual ~ConnectionOptions() {}

        bool allow_secure() const
        {
            return (type == CON_OP_BOTH || type == CON_OP_SECURE) && secure_port != -1;
        }

        bool allow_unsecure() const
        {
            return (type == CON_OP_BOTH || type == CON_OP_UNSECURE) && unsecure_port != -1;
        }

    public:
        Type type;
        int unsecure_port;
        int secure_port;
        int protocol; // 0 == auto
        HostAuthOptions host_auth; // for secure connection
        std::string ciphers;
    };

    void connect_unsecure(const char* host, int port);
    void connect_secure(const ConnectionOptions& options, const char* host);

    void disconnect();
    void swap(RedPeer* other);
    void close();
    void enable() { _shut = false;}

    virtual CompoundInMessage* receive();
    uint32_t do_send(OutMessage& message, uint32_t skip_bytes);
    uint32_t send(OutMessage& message);

    uint32_t receive(uint8_t* buf, uint32_t size);
    uint32_t send(uint8_t* buf, uint32_t size);

protected:
    virtual void on_event() {}
    virtual int get_socket() { return _peer;}
    void cleanup();

private:
    void connect_to_peer(const char* host, int port);
    void shutdown();

private:
    SOCKET _peer;
    Mutex _lock;
    bool _shut;
    uint64_t _serial;

    SSL_CTX *_ctx;
    SSL *_ssl;
};

class RedPeer::InMessage {
public:
    InMessage(uint16_t type, uint32_t size, uint8_t * data)
        : _type (type)
        , _size (size)
        , _data (data)
    {
    }

    virtual ~InMessage() {}

    uint16_t type() { return _type;}
    uint8_t* data() { return _data;}
    virtual uint32_t size() { return _size;}

protected:
    uint16_t _type;
    uint32_t _size;
    uint8_t* _data;
};

class RedPeer::CompoundInMessage: public RedPeer::InMessage {
public:
    CompoundInMessage(uint64_t _serial, uint16_t type, uint32_t size, uint32_t sub_list)
        : InMessage(type, size, new uint8_t[size])
        , _refs (1)
        , _serial (_serial)
        , _sub_list (sub_list)
    {
    }

    RedPeer::InMessage* ref() { _refs++; return this;}
    void unref() {if (!--_refs) delete this;}

    uint64_t serial() { return _serial;}
    uint32_t sub_list() { return _sub_list;}

    virtual uint32_t size() { return _sub_list ? _sub_list : _size;}
    uint32_t compound_size() {return _size;}

protected:
    virtual ~CompoundInMessage() { delete[] _data;}

private:
    int _refs;
    uint64_t _serial;
    uint32_t _sub_list;
};

class RedPeer::OutMessage {
public:
    OutMessage(uint32_t type);
    virtual ~OutMessage();

    SpiceMarshaller *marshaller() { return _marshaller;}
    void reset(uint32_t type);

private:
    uint32_t message_size() { return spice_marshaller_get_total_size(_marshaller);}
    uint8_t* base() { return spice_marshaller_get_ptr(_marshaller);}
    SpiceDataHeader& header() { return *(SpiceDataHeader *)base();}

protected:
    SpiceMarshaller *_marshaller;

    friend class RedPeer;
    friend class RedChannel;
};

#endif
