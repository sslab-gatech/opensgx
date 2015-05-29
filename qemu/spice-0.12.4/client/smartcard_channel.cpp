/*
   Copyright (C) 2010 Red Hat, Inc.

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

#include <spice/enums.h>
#include "common/mutex.h"

#include "red_client.h"

extern "C" {
#include <vscard_common.h>
#include <vreader.h>
#include <vcard_emul.h>
#include <vevent.h>
}

#include "smartcard_channel.h"

#define APDUBufSize 270

#define MAX_ATR_LEN 40

//#define DEBUG_SMARTCARD

#ifdef DEBUG_SMARTCARD
void PrintByteArray(uint8_t *arrBytes, unsigned int nSize)
{
    int i;

    for (i=0; i < nSize; i++) {
        DBG(0, "%X ", arrBytes[i]);
    }
    DBG(0, "\n");
}
#define DEBUG_PRINT_BYTE_ARRAY(arrBytes, nSize) PrintByteArray(arrBytes, nSize)
#else
#define DEBUG_PRINT_BYTE_ARRAY(arrBytes, nSize)
#endif

SmartCardChannel* g_smartcard_channel = NULL; // used for insert/remove of virtual card. Can be
                                // changed if we let Application store the SmartCard instance.

class SmartCardHandler: public MessageHandlerImp<SmartCardChannel, SPICE_CHANNEL_SMARTCARD> {
public:
    SmartCardHandler(SmartCardChannel& channel)
        : MessageHandlerImp<SmartCardChannel, SPICE_CHANNEL_SMARTCARD>(channel) {}
};


SmartCardChannel::SmartCardChannel(RedClient& client, uint32_t id)
    : RedChannel(client, SPICE_CHANNEL_SMARTCARD, id, new SmartCardHandler(*this))
    , _next_sync_vevent(VEVENT_LAST)
{
    SmartCardHandler* handler = static_cast<SmartCardHandler*>(get_message_handler());

    g_smartcard_channel = this;
    handler->set_handler(SPICE_MSG_SMARTCARD_DATA,
                        &SmartCardChannel::handle_smartcard_data);
}

ReaderData* SmartCardChannel::reader_data_from_vreader(VReader* vreader)
{
    if (vreader == NULL) {
        assert(_readers_by_vreader.size() > 0);
        return _readers_by_vreader.begin()->second;
    }
    if (_readers_by_vreader.count(vreader) > 0) {
        return _readers_by_vreader.find(vreader)->second;
    }
    assert(_unallocated_readers_by_vreader.count(vreader) > 0);
    return _unallocated_readers_by_vreader.find(vreader)->second;
}

ReaderData* SmartCardChannel::reader_data_from_reader_id(uint32_t reader_id)
{
    if (_readers_by_id.count(reader_id) > 0) {
        return _readers_by_id.find(reader_id)->second;
    }
    return NULL;
}

/** On VEVENT_READER_INSERT we call this, send a VSC_ReaderAdd, and wait for a VSC_ReaderAddResponse
 */
void SmartCardChannel::add_unallocated_reader(VReader* vreader, const char* name)
{
    ReaderData* data = new ReaderData();

    data->vreader = vreader;
    data->reader_id = VSCARD_UNDEFINED_READER_ID;
    data->name = spice_strdup(name);
    _unallocated_readers_by_vreader.insert(std::pair<VReader*, ReaderData*>(vreader, data));
    LOG_INFO("adding unallocated reader %p", data);
}

/** called upon the VSC_ReaderAddResponse
 */
ReaderData* SmartCardChannel::add_reader(uint32_t reader_id)
{
    ReaderData* data;
    size_t unallocated = _unallocated_readers_by_vreader.size();

    assert(unallocated > 0);
    data = _unallocated_readers_by_vreader.begin()->second;
    data->reader_id = reader_id;
    LOG_INFO("adding %p->%d", data, reader_id);
    _readers_by_vreader.insert(
        std::pair<VReader*, ReaderData*>(data->vreader, data));
    assert(_readers_by_vreader.count(data->vreader) == 1);
    _readers_by_id.insert(std::pair<uint32_t, ReaderData*>(reader_id, data));
    assert(_readers_by_id.count(reader_id) == 1);
    _unallocated_readers_by_vreader.erase(_unallocated_readers_by_vreader.begin());
    assert(_unallocated_readers_by_vreader.size() == unallocated - 1);
    assert(_unallocated_readers_by_vreader.count(data->vreader) == 0);
    return data;
}

void* SmartCardChannel::cac_card_events_thread_entry(void* data)
{
    static_cast<SmartCardChannel*>(data)->cac_card_events_thread_main();
    return NULL;
}

VEventEvent::VEventEvent(SmartCardChannel* smartcard_channel, VEvent* vevent)
    : _smartcard_channel(smartcard_channel)
    , _vreader(vevent->reader)
    , _vevent(vevent)
{
}

VEventEvent::~VEventEvent()
{
    vevent_delete(_vevent);
}

void ReaderAddEvent::response(AbstractProcessLoop& events_loop)
{
    static int num = 0;
    char name[20];

    sprintf(name, "test%4d", num++);
    _smartcard_channel->add_unallocated_reader(_vreader, name);
    _smartcard_channel->send_reader_added(name);
}

void ReaderRemoveEvent::response(AbstractProcessLoop& events_loop)
{
    ReaderData* data;

    data = _smartcard_channel->reader_data_from_vreader(_vreader);
    _smartcard_channel->send_reader_removed(data->reader_id);
    _smartcard_channel->remove_reader(data);
}

void CardInsertEvent::response(AbstractProcessLoop& events_loop)
{
    ReaderData* data = _smartcard_channel->reader_data_from_vreader(
                                                    _vreader);

    if (data->reader_id == VSCARD_UNDEFINED_READER_ID) {
        data->card_insert_pending = true;
    } else {
        _smartcard_channel->send_atr(_vreader);
    }
}

void CardRemoveEvent::response(AbstractProcessLoop& events_loop)
{
    ReaderData* data = _smartcard_channel->reader_data_from_vreader(
                                                    _vreader);

    ASSERT(data->reader_id != VSCARD_UNDEFINED_READER_ID);
    _smartcard_channel->send_message(data->reader_id, VSC_CardRemove, NULL, 0);
}

void SmartCardChannel::remove_reader(ReaderData* data)
{
    // TODO - untested code (caccard doesn't produce these events yet)
    if (_readers_by_vreader.count(data->vreader) > 0) {
        _readers_by_vreader.erase(_readers_by_vreader.find(data->vreader));
        _readers_by_id.erase(_readers_by_id.find(data->reader_id));
    } else {
        _unallocated_readers_by_vreader.erase(
            _unallocated_readers_by_vreader.find(data->vreader));
    }
    free(data->name);
    delete data;
}

/* Sync events need to be sent one by one, waiting for VSC_Error
 * messages from the server in between. */
void SmartCardChannel::push_sync_event(VEventType type, Event *event)
{
    event->ref();
    _sync_events.push_back(SmartCardEvent(type, event));
    if (_next_sync_vevent != VEVENT_LAST) {
        return;
    }
    send_next_sync_event();
}

void SmartCardChannel::send_next_sync_event()
{
    if (_sync_events.empty()) {
        _next_sync_vevent = VEVENT_LAST;
        return;
    }
    SmartCardEvent sync_event = _sync_events.front();
    _sync_events.pop_front();
    get_client().push_event(sync_event.second);
    sync_event.second->unref();
    _next_sync_vevent = sync_event.first;
}

void SmartCardChannel::handle_reader_add_response(VSCMsgHeader *vheader,
                                                  VSCMsgError *error)
{
    ReaderData* data;

    if (error->code == VSC_SUCCESS) {
        data = add_reader(vheader->reader_id);
        if (data->card_insert_pending) {
            data->card_insert_pending = false;
            send_atr(data->vreader);
        }
    } else {
        LOG_WARN("VSC Error: reader %d, code %d",
            vheader->reader_id, error->code);
    }
}

void SmartCardChannel::handle_error_message(VSCMsgHeader *vheader,
    VSCMsgError *error)
{
    switch (_next_sync_vevent) {
        case VEVENT_READER_INSERT:
            handle_reader_add_response(vheader, error);
            break;
        case VEVENT_CARD_INSERT:
        case VEVENT_CARD_REMOVE:
        case VEVENT_READER_REMOVE:
            break;
        default:
            LOG_WARN("Unexpected Error message: %d", error->code);
    }
    send_next_sync_event();
}

void SmartCardChannel::cac_card_events_thread_main()
{
    VEvent *vevent = NULL;
    bool cont = true;

    while (cont) {
        vevent = vevent_wait_next_vevent();
        if (vevent == NULL) {
            break;
        }
        switch (vevent->type) {
        case VEVENT_READER_INSERT:
            LOG_INFO("VEVENT_READER_INSERT");
            {
                AutoRef<ReaderAddEvent> event(new ReaderAddEvent(this, vevent));
                push_sync_event(vevent->type, *event);
            }
            break;
        case VEVENT_READER_REMOVE:
            LOG_INFO("VEVENT_READER_REMOVE");
            {
                AutoRef<ReaderRemoveEvent> event(new ReaderRemoveEvent(this, vevent));
                push_sync_event(vevent->type, *event);
            }
            break;
        case VEVENT_CARD_INSERT:
            LOG_INFO("VEVENT_CARD_INSERT");
            {
                AutoRef<CardInsertEvent> event(new CardInsertEvent(this, vevent));
                push_sync_event(vevent->type, *event);
            }
            break;
        case VEVENT_CARD_REMOVE:
            LOG_INFO("VEVENT_CARD_REMOVE");
            {
                AutoRef<CardRemoveEvent> event(new CardRemoveEvent(this, vevent));
                push_sync_event(vevent->type, *event);
            }
            break;
        case VEVENT_LAST:
            cont = false;
        default:
           /* anything except VEVENT_LAST and default
            * gets to VEventEvent which does vevent_delete in VEventEvent~ */
            vevent_delete(vevent);
        }
    }
}

void virtual_card_insert()
{
    if (g_smartcard_channel == NULL) {
        return;
    }
    g_smartcard_channel->virtual_card_insert();
}

void SmartCardChannel::virtual_card_insert()
{
    if (_readers_by_id.size() == 0) {
        return;
    }
    vcard_emul_force_card_insert(_readers_by_id.begin()->second->vreader);
}

void virtual_card_remove()
{
    if (g_smartcard_channel == NULL) {
        return;
    }
    g_smartcard_channel->virtual_card_remove();
}

void SmartCardChannel::virtual_card_remove()
{
    if (_readers_by_id.size() == 0) {
        return;
    }
    vcard_emul_force_card_remove(_readers_by_id.begin()->second->vreader);
}

#define CERTIFICATES_DEFAULT_DB "/etc/pki/nssdb"
#define CERTIFICATES_ARGS_TEMPLATE "db=\"%s\" use_hw=no soft=(,Virtual Card,CAC,,%s,%s,%s)"

SmartcardOptions::SmartcardOptions() :
dbname(CERTIFICATES_DEFAULT_DB),
enable(false)
{
}

static VCardEmulError init_vcard_local_certs(const char* dbname, const char* cert1,
    const char* cert2, const char* cert3)
{
    char emul_args[200];
    VCardEmulOptions *options = NULL;

    snprintf(emul_args, sizeof(emul_args) - 1, CERTIFICATES_ARGS_TEMPLATE,
        dbname, cert1, cert2, cert3);
    options = vcard_emul_options(emul_args);
    if (options == NULL) {
        LOG_WARN("not using certificates due to initialization error");
    }
    return vcard_emul_init(options);
}

static bool g_vcard_inited = false;

void smartcard_init(const SmartcardOptions* options)
{
    if (g_vcard_inited) {
        return;
    }
    if (options->certs.size() == 3) {
        const char* dbname = options->dbname.c_str();
        if (init_vcard_local_certs(dbname, options->certs[0].c_str(),
            options->certs[1].c_str(), options->certs[2].c_str()) != VCARD_EMUL_OK) {
            throw Exception("smartcard: emulated card initialization failed (check certs/db)");
        }
    } else {
        if (options->certs.size() > 0) {
            LOG_WARN("Ignoring smartcard certificates - must be exactly three for virtual card emulation");
        }
        if (vcard_emul_init(NULL) != VCARD_EMUL_OK) {
            throw Exception("smartcard: vcard initialization failed (check hardware/drivers)");
        }
    }
    g_vcard_inited = true;
}

void SmartCardChannel::on_connect()
{
    _event_thread = new Thread(SmartCardChannel::cac_card_events_thread_entry, this);
}

void SmartCardChannel::on_disconnect()
{
    VEvent *stop_event;

    if (_event_thread == NULL) {
        return;
    }
    stop_event = vevent_new(VEVENT_LAST, NULL, NULL);
    vevent_queue_vevent(stop_event);
    _event_thread->join();
    delete _event_thread;
    _event_thread = NULL;
}


void SmartCardChannel::send_reader_removed(uint32_t reader_id)
{
    send_message(reader_id, VSC_ReaderRemove, NULL, 0);
}

void SmartCardChannel::send_reader_added(const char* reader_name)
{
    send_message(VSCARD_UNDEFINED_READER_ID,
        VSC_ReaderAdd, (uint8_t*)reader_name, strlen(reader_name));
}

void SmartCardChannel::send_atr(VReader* vreader)
{
    unsigned char atr[ MAX_ATR_LEN];
    int atr_len = MAX_ATR_LEN;
    uint32_t reader_id = reader_data_from_vreader(vreader)->reader_id;

    assert(reader_id != VSCARD_UNDEFINED_READER_ID);
    vreader_power_on(vreader, atr, &atr_len);
    DBG(0, "ATR: ");
    DEBUG_PRINT_BYTE_ARRAY(atr, atr_len);
    send_message(reader_id, VSC_ATR, (uint8_t*)atr, atr_len);
}

void SmartCardChannel::send_message(uint32_t reader_id, VSCMsgType type, uint8_t* data, uint32_t len)
{
    VSCMsgHeader mhHeader;
    Message* msg = new Message(SPICE_MSGC_SMARTCARD_DATA);
    SpiceMarshaller* m = msg->marshaller();

    mhHeader.type = type;
    mhHeader.length = len;
    mhHeader.reader_id = reader_id;
    spice_marshaller_add(m, (uint8_t*)&mhHeader, sizeof(mhHeader));
    spice_marshaller_add(m, data, len);
    post_message(msg);
}

VSCMessageEvent::VSCMessageEvent(SmartCardChannel* smartcard_channel,
    VSCMsgHeader* vheader)
    : _smartcard_channel(smartcard_channel)
    , _vheader(NULL)
{
    _vheader = (VSCMsgHeader*)spice_memdup(vheader,
                        sizeof(VSCMsgHeader) + vheader->length);
    ASSERT(_vheader);
}

VSCMessageEvent::~VSCMessageEvent()
{
    free(_vheader);
}

void VSCMessageEvent::response(AbstractProcessLoop& loop)
{
    static int recv_count = 0;
    int dwSendLength;
    int dwRecvLength;
    uint8_t* pbSendBuffer = _vheader->data;
    uint8_t pbRecvBuffer[APDUBufSize+sizeof(uint32_t)];
    VReaderStatus reader_status;
    uint32_t rv;

    switch (_vheader->type) {
        case VSC_APDU:
            break;
        case VSC_Error:
            _smartcard_channel->handle_error_message(
                            _vheader,
                            (VSCMsgError*)_vheader->data);
            return;
        case VSC_Init:
            break;
        default:
            LOG_WARN("unhandled VSC %d of length %d, reader %d",
                _vheader->type, _vheader->length, _vheader->reader_id);
            return;
    }

    /* Transmit recieved APDU */
    dwSendLength = _vheader->length;
    dwRecvLength = sizeof(pbRecvBuffer);

    DBG(0, " %3d: recv APDU: ", recv_count++);
    DEBUG_PRINT_BYTE_ARRAY(pbSendBuffer, _vheader->length);

    ReaderData* reader_data = _smartcard_channel->reader_data_from_reader_id(
                                                        _vheader->reader_id);
    if (reader_data == NULL) {
        LOG_WARN("got message for non existant reader");
        return;
    }

    VReader* vreader = reader_data->vreader;

    reader_status = vreader_xfr_bytes(vreader,
        pbSendBuffer, dwSendLength,
        pbRecvBuffer, &dwRecvLength);
    if (reader_status == VREADER_OK) {
        DBG(0, " sent APDU: ");
        DEBUG_PRINT_BYTE_ARRAY(pbRecvBuffer, dwRecvLength);
        _smartcard_channel->send_message (
            _vheader->reader_id,
            VSC_APDU,
            pbRecvBuffer,
            dwRecvLength
        );
    } else {
        rv = reader_status; /* warning: not meaningful */
        _smartcard_channel->send_message (
            _vheader->reader_id,
            VSC_Error,
            (uint8_t*)&rv,
            sizeof (uint32_t)
        );
    }
}

void SmartCardChannel::handle_smartcard_data(RedPeer::InMessage* message)
{
    VSCMsgHeader* mhHeader = (VSCMsgHeader*)(message->data());

    AutoRef<VSCMessageEvent> event(new VSCMessageEvent(this, mhHeader));
    get_client().push_event(*event);
}

class SmartCardFactory: public ChannelFactory {
public:
    SmartCardFactory() : ChannelFactory(SPICE_CHANNEL_SMARTCARD) {}
    virtual RedChannel* construct(RedClient& client, uint32_t id)
    {
        return new SmartCardChannel(client, id);
    }
};

static SmartCardFactory factory;

ChannelFactory& SmartCardChannel::Factory()
{
    return factory;
}
