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

#include <shlobj.h>
#include <io.h>
#include <conio.h>
#include <fcntl.h>

#include "platform.h"
#include "win_platform.h"
#include "utils.h"
#include "threads.h"
#include "debug.h"
#include "monitor.h"
#include "record.h"
#include "playback.h"
#include "cursor.h"
#include "named_pipe.h"

#ifndef DISABLE_CXIMAGE
#define USE_CXIMAGE
#endif

#ifdef USE_CXIMAGE
#include "ximage.h"
#endif
#include <spice/vd_agent.h>

extern HINSTANCE instance;

class DefaultEventListener: public Platform::EventListener {
public:
    virtual void on_app_activated() {}
    virtual void on_app_deactivated() {}
    virtual void on_monitors_change() {}
};

static DefaultEventListener default_event_listener;
static Platform::EventListener* event_listener = &default_event_listener;
static HWND platform_win = NULL;
static ProcessLoop* main_loop = NULL;

class DefaultClipboardListener: public Platform::ClipboardListener {
public:
    virtual void on_clipboard_grab(uint32_t *types, uint32_t type_count) {}
    virtual void on_clipboard_request(uint32_t type) {}
    virtual void on_clipboard_notify(uint32_t type, uint8_t* data, int32_t size) {}
    virtual void on_clipboard_release() {}
};

static DefaultClipboardListener default_clipboard_listener;
static Platform::ClipboardListener* clipboard_listener = &default_clipboard_listener;

// The next window in the clipboard viewer chain, which is refered in all clipboard events.
static HWND next_clipboard_viewer_win = NULL;
static HANDLE clipboard_event = NULL;

static const int CLIPBOARD_TIMEOUT_MS = 10000;

static const int CLIPBOARD_FORMAT_MAX_TYPES = 16;

typedef struct ClipboardFormat {
    uint32_t format;
    uint32_t types[CLIPBOARD_FORMAT_MAX_TYPES];
} ClipboardFormat;

static ClipboardFormat clipboard_formats[] = {
    {CF_UNICODETEXT, {VD_AGENT_CLIPBOARD_UTF8_TEXT, 0}},
    //FIXME: support more image types
    {CF_DIB, {VD_AGENT_CLIPBOARD_IMAGE_PNG, VD_AGENT_CLIPBOARD_IMAGE_BMP, 0}},
};

#define clipboard_formats_count (sizeof(clipboard_formats) / sizeof(clipboard_formats[0]))

#ifdef USE_CXIMAGE
typedef struct ImageType {
    uint32_t type;
    DWORD cximage_format;
} ImageType;

static ImageType image_types[] = {
    {VD_AGENT_CLIPBOARD_IMAGE_PNG, CXIMAGE_FORMAT_PNG},
    {VD_AGENT_CLIPBOARD_IMAGE_BMP, CXIMAGE_FORMAT_BMP},
};
#endif

static std::set<uint32_t> grab_types;

static const unsigned long MODAL_LOOP_TIMER_ID = 1;
static const int MODAL_LOOP_DEFAULT_TIMEOUT = 100;
static bool modal_loop_active = false;
static bool set_modal_loop_timer();

void Platform::send_quit_request()
{
    ASSERT(main_loop);
    main_loop->quit(0);
}

static uint32_t get_clipboard_type(uint32_t format) {
    uint32_t* types = NULL;

    for (size_t i = 0; i < clipboard_formats_count && !types; i++) {
        if (clipboard_formats[i].format == format) {
            types = clipboard_formats[i].types;
        }
    }
    if (!types) {
        return VD_AGENT_CLIPBOARD_NONE;
    }
    for (uint32_t* ptype = types; *ptype; ptype++) {
        if (grab_types.find(*ptype) != grab_types.end()) {
            return *ptype;
        }
    }
    return VD_AGENT_CLIPBOARD_NONE;
}

static uint32_t get_clipboard_format(uint32_t type) {
    for (size_t i = 0; i < clipboard_formats_count; i++) {
        for (uint32_t* ptype = clipboard_formats[i].types; *ptype; ptype++) {
            if (*ptype == type) {
                return clipboard_formats[i].format;
            }
        }
    }
    return 0;
}

static int get_available_clipboard_types(uint32_t** types)
{
    int count = 0;

    *types = new uint32_t[clipboard_formats_count * CLIPBOARD_FORMAT_MAX_TYPES];
    for (size_t i = 0; i < clipboard_formats_count; i++) {
        if (IsClipboardFormatAvailable(clipboard_formats[i].format)) {
            for (uint32_t* ptype = clipboard_formats[i].types; *ptype; ptype++) {
                (*types)[count++] = *ptype;
            }
        }
    }
    if (!count) {
        delete[] *types;
        *types = NULL;
    }
    return count;
}

#ifdef USE_CXIMAGE
static DWORD get_cximage_format(uint32_t type)
{
    for (size_t i = 0; i < sizeof(image_types) / sizeof(image_types[0]); i++) {
        if (image_types[i].type == type) {
            return image_types[i].cximage_format;
        }
    }
    return 0;
}
#endif

static LRESULT CALLBACK PlatformWinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_TIMER:
        if (modal_loop_active) {
            main_loop->timers_action();
            if (!set_modal_loop_timer()) {
                LOG_WARN("failed to set modal loop timer");
            }
        } else {
            LOG_WARN("received WM_TIMER not inside a modal loop");
        }
        break;
    case WM_ACTIVATEAPP:
        if (wParam) {
            event_listener->on_app_activated();
        } else {
            event_listener->on_app_deactivated();
        }
        break;
    case WM_DISPLAYCHANGE:
        event_listener->on_monitors_change();
        break;
    case WM_CHANGECBCHAIN:
        if (next_clipboard_viewer_win == (HWND)wParam) {
            next_clipboard_viewer_win = (HWND)lParam;
        } else if (next_clipboard_viewer_win) {
            SendMessage(next_clipboard_viewer_win, message, wParam, lParam);
        }
        break;
    case WM_DRAWCLIPBOARD:
        if (platform_win != GetClipboardOwner()) {
            int type_count;
            uint32_t* types;
            Platform::set_clipboard_owner(Platform::owner_none);
            if ((type_count = get_available_clipboard_types(&types))) {
                clipboard_listener->on_clipboard_grab(types, type_count);
                delete[] types;
            } else {
                LOG_INFO("Unsupported clipboard format");
            }
        }
        if (next_clipboard_viewer_win) {
            SendMessage(next_clipboard_viewer_win, message, wParam, lParam);
        }
        break;
    case WM_RENDERFORMAT: {
        // In delayed rendering, Windows requires us to SetClipboardData before we return from
        // handling WM_RENDERFORMAT. Therefore, we try our best by sending CLIPBOARD_REQUEST to the
        // agent, while waiting alertably for a while (hoping for good) for receiving CLIPBOARD data
        // or CLIPBOARD_RELEASE from the agent, which both will signal clipboard_event.
        uint32_t type = get_clipboard_type(wParam);
        if (!type) {
            LOG_INFO("Unsupported clipboard format %u", wParam);
            break;
        }
        clipboard_listener->on_clipboard_request(type);
        DWORD start_tick = GetTickCount();
        while (WaitForSingleObjectEx(clipboard_event, 1000, TRUE) != WAIT_OBJECT_0 &&
               GetTickCount() < start_tick + CLIPBOARD_TIMEOUT_MS);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

static void create_message_wind()
{
    WNDCLASSEX wclass;
    ATOM class_atom;
    DWORD err;

    const LPCWSTR class_name = L"spicec_platform_wclass";

    wclass.cbSize = sizeof(WNDCLASSEX);
    wclass.style = 0;
    wclass.lpfnWndProc = PlatformWinProc;
    wclass.cbClsExtra = 0;
    wclass.cbWndExtra = 0;
    wclass.hInstance = instance;
    wclass.hIcon = NULL;
    wclass.hCursor = NULL;
    wclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wclass.lpszMenuName = NULL;
    wclass.lpszClassName = class_name;
    wclass.hIconSm = NULL;

    if ((class_atom = RegisterClassEx(&wclass)) == 0) {
        THROW("register class failed");
    }

    if (!(platform_win = CreateWindow(class_name, L"", 0, 0, 0, 0, 0, NULL, NULL, instance, NULL))) {
        THROW("create message window failed");
    }
    SetLastError(0);
    if (!(next_clipboard_viewer_win = SetClipboardViewer(platform_win)) && (err = GetLastError())) {
        THROW("set clipboard viewer failed %u", err);
    }
    if (!(clipboard_event = CreateEvent(NULL, FALSE, FALSE, NULL))) {
        THROW("create clipboard event failed");
    }
}

NamedPipe::ListenerRef NamedPipe::create(const char *name, ListenerInterface& listener_interface)
{
    ASSERT(main_loop && main_loop->is_same_thread(pthread_self()));
    return (ListenerRef)(new WinListener(name, listener_interface, *main_loop));
}

void NamedPipe::destroy(ListenerRef listener_ref)
{
    ASSERT(main_loop && main_loop->is_same_thread(pthread_self()));
    delete (WinListener *)listener_ref;
}

void NamedPipe::destroy_connection(ConnectionRef conn_ref)
{
    ASSERT(main_loop && main_loop->is_same_thread(pthread_self()));
    delete (WinConnection *)conn_ref;
}

int32_t NamedPipe::read(ConnectionRef conn_ref, uint8_t *buf, int32_t size)
{
    return ((WinConnection *)conn_ref)->read(buf, size);
}

int32_t NamedPipe::write(ConnectionRef conn_ref, const uint8_t *buf, int32_t size)
{
    return ((WinConnection *)conn_ref)->write(buf, size);
}

void Platform::msleep(unsigned int msec)
{
    Sleep(msec);
}

void Platform::yield()
{
    Sleep(0);
}

void Platform::set_thread_priority(void* thread, Platform::ThreadPriority in_priority)
{
    ASSERT(thread == NULL);
    int priority;

    switch (in_priority) {
    case PRIORITY_TIME_CRITICAL:
        priority = THREAD_PRIORITY_TIME_CRITICAL;
        break;
    case PRIORITY_HIGH:
        priority = THREAD_PRIORITY_HIGHEST;
        break;
    case PRIORITY_ABOVE_NORMAL:
        priority = THREAD_PRIORITY_ABOVE_NORMAL;
        break;
    case PRIORITY_NORMAL:
        priority = THREAD_PRIORITY_NORMAL;
        break;
    case PRIORITY_BELOW_NORMAL:
        priority = THREAD_PRIORITY_BELOW_NORMAL;
        break;
    case PRIORITY_LOW:
        priority = THREAD_PRIORITY_LOWEST;
        break;
    case PRIORITY_IDLE:
        priority = THREAD_PRIORITY_IDLE;
        break;
    default:
        THROW("invalid priority %d", in_priority);
    }
    SetThreadPriority(GetCurrentThread(), priority);
}

void Platform::set_event_listener(EventListener* listener)
{
    event_listener = listener ? listener : &default_event_listener;
}

uint64_t Platform::get_monolithic_time()
{
    return uint64_t(GetTickCount()) * 1000 * 1000;
}

void Platform::get_temp_dir(std::string& path)
{
    DWORD len = GetTempPathA(0, NULL);
    if (len <= 0) {
        throw Exception("get temp patch failed");
    }
    char* tmp_path = new char[len + 1];
    GetTempPathA(len, tmp_path);
    path = tmp_path;
    delete[] tmp_path;
}

uint64_t Platform::get_process_id()
{
    static uint64_t pid = GetCurrentProcessId();
    return pid;
}

uint64_t Platform::get_thread_id()
{
    return GetCurrentThreadId();
}

void Platform::error_beep()
{
    MessageBeep(MB_ICONERROR);
}

class WinMonitor: public Monitor {
public:
    WinMonitor(int id, const wchar_t* name, const wchar_t* string);

    virtual int get_depth() { return _depth;}
    virtual SpicePoint get_position();
    virtual SpicePoint get_size() const { SpicePoint size = {_width, _height}; return size;}
    virtual bool is_out_of_sync() { return _out_of_sync;}
    virtual int get_screen_id() { return 0;}

protected:
    virtual ~WinMonitor();
    virtual void do_set_mode(int width, int height);
    virtual void do_restore();

private:
    void update_position();
    bool change_display_settings(int width, int height, int depth);
    bool best_display_setting(uint32_t width, uint32_t height, uint32_t depth);

private:
    std::wstring _dev_name;
    std::wstring _dev_string;
    bool _active;
    SpicePoint _position;
    int _width;
    int _height;
    int _depth;
    bool _out_of_sync;
};

WinMonitor::WinMonitor(int id, const wchar_t* name, const wchar_t* string)
    : Monitor(id)
    , _dev_name (name)
    , _dev_string (string)
    , _active (false)
    , _out_of_sync (false)
{
    update_position();
}

WinMonitor::~WinMonitor()
{
    do_restore();
}

void WinMonitor::update_position()
{
    DEVMODE mode;
    mode.dmSize = sizeof(DEVMODE);
    mode.dmDriverExtra = 0;
    EnumDisplaySettings(_dev_name.c_str(), ENUM_CURRENT_SETTINGS, &mode);
    _position.x = mode.dmPosition.x;
    _position.y = mode.dmPosition.y;
    _width = mode.dmPelsWidth;
    _height = mode.dmPelsHeight;
    _depth = mode.dmBitsPerPel;
}

SpicePoint WinMonitor::get_position()
{
    update_position();
    return _position;
}

bool WinMonitor::change_display_settings(int width, int height, int depth)
{
    DEVMODE mode;
    mode.dmSize = sizeof(DEVMODE);
    mode.dmDriverExtra = 0;
    mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    mode.dmPelsWidth = width;
    mode.dmPelsHeight = height;
    mode.dmBitsPerPel = depth;

    return ChangeDisplaySettingsEx(_dev_name.c_str(), &mode, NULL, CDS_FULLSCREEN, NULL)
                                                                        == DISP_CHANGE_SUCCESSFUL;
}

bool WinMonitor::best_display_setting(uint32_t width, uint32_t height, uint32_t depth)
{
    DEVMODE mode;
    DWORD mode_id = 0;
    uint32_t mod_waste = ~0;
    DWORD mod_width = 0;
    DWORD mod_height = 0;
    DWORD mod_depth = 0;
    DWORD mod_frequency = 0;

    mode.dmSize = sizeof(DEVMODE);
    mode.dmDriverExtra = 0;
    while (EnumDisplaySettings(_dev_name.c_str(), mode_id++, &mode)) {
        // Workaround for
        // Lenovo T61p, Nvidia Quadro FX 570M and
        // Lenovo T61, Nvidia Quadro NVS 140M
        //
        // with dual monitors configuration
        //
        // we get strange values from EnumDisplaySettings 640x480x4 frequency 1
        // and calling ChangeDisplaySettingsEx with that configuration result with
        // machine that is stucked for a long period of time
        if (mode.dmDisplayFrequency == 1) {
            continue;
        }

        if (mode.dmPelsWidth >= width && mode.dmPelsHeight >= height) {
            bool replace = false;
            uint32_t curr_waste = mode.dmPelsWidth * mode.dmPelsHeight - width * height;
            if (curr_waste < mod_waste) {
                replace = true;
            } else if (curr_waste == mod_waste) {
                if (mod_depth == mode.dmBitsPerPel) {
                    replace = mode.dmDisplayFrequency > mod_frequency;
                } else if (mod_depth != depth && mode.dmBitsPerPel > mod_depth) {
                    replace = true;
                }
            }
            if (replace) {
                mod_waste = curr_waste;
                mod_width = mode.dmPelsWidth;
                mod_height = mode.dmPelsHeight;
                mod_depth = mode.dmBitsPerPel;
                mod_frequency = mode.dmDisplayFrequency;
            }
        }
    }
    if (mod_waste == ~0u) {
        return false;
    }
    mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
    mode.dmPelsWidth = mod_width;
    mode.dmPelsHeight = mod_height;
    mode.dmBitsPerPel = mod_depth;
    mode.dmDisplayFrequency = mod_frequency;

    return ChangeDisplaySettingsEx(_dev_name.c_str(), &mode, NULL, CDS_FULLSCREEN, NULL)
                                                                        == DISP_CHANGE_SUCCESSFUL;
}

void WinMonitor::do_set_mode(int width, int height)
{
    update_position();
    if (width == _width && height == _height) {
        _out_of_sync = false;
        return;
    }
    self_monitors_change++;
    if (!change_display_settings(width, height, 32) && !best_display_setting(width, height, 32)) {
        _out_of_sync = true;
    } else {
        _out_of_sync = false;
    }
    self_monitors_change--;
    _active = true;
    update_position();
}

void WinMonitor::do_restore()
{
    if (_active) {
        _active = false;
        self_monitors_change++;
        ChangeDisplaySettingsEx(_dev_name.c_str(), NULL, NULL, 0, NULL);
        self_monitors_change--;
        update_position();
    }
}

static MonitorsList monitors;
static Monitor* primary_monitor = NULL;

const MonitorsList& Platform::init_monitors()
{
    ASSERT(monitors.empty());

    int id = 0;
    Monitor* mon;
    DISPLAY_DEVICE device_info;
    DWORD device_id = 0;
    device_info.cb = sizeof(device_info);
    while (EnumDisplayDevices(NULL, device_id, &device_info, 0)) {
        if ((device_info.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) &&
                 !(device_info.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)) {
            mon = new WinMonitor(id++, device_info.DeviceName, device_info.DeviceString);
            monitors.push_back(mon);
            if (device_info.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
                primary_monitor = mon;
            }
        }
        device_id++;
    }
    return monitors;
}

void Platform::destroy_monitors()
{
    primary_monitor = NULL;
    while (!monitors.empty()) {
        Monitor* monitor = monitors.front();
        monitors.pop_front();
        delete monitor;
    }
}

bool Platform::is_monitors_pos_valid()
{
    return true;
}

void Platform::get_app_data_dir(std::string& path, const std::string& app_name)
{
    char app_data_path[MAX_PATH];
    HRESULT res = SHGetFolderPathA(NULL, CSIDL_APPDATA,  NULL, 0, app_data_path);
    if (res != S_OK) {
        throw Exception("get user app data dir failed");
    }

    path = app_data_path;
    path_append(path, app_name);

    if (!CreateDirectoryA(path.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        throw Exception("create user app data dir failed");
    }
}

void Platform::path_append(std::string& path, const std::string& partial_path)
{
    path += "\\";
    path += partial_path;
}

static void cleanup()
{
    ChangeClipboardChain(platform_win, next_clipboard_viewer_win);
    CloseHandle(clipboard_event);
}

void Platform::init()
{
    create_message_wind();
    atexit(cleanup);
}

void Platform::set_process_loop(ProcessLoop& main_process_loop)
{
    main_loop = &main_process_loop;
}

WaveRecordAbstract* Platform::create_recorder(RecordClient& client,
                                              uint32_t sampels_per_sec,
                                              uint32_t bits_per_sample,
                                              uint32_t channels)
{
    return new WaveRecorder(client, sampels_per_sec, bits_per_sample, channels);
}

WavePlaybackAbstract* Platform::create_player(uint32_t sampels_per_sec,
                                              uint32_t bits_per_sample,
                                              uint32_t channels)
{
    return new WavePlayer(sampels_per_sec, bits_per_sample, channels);
}

static void toggle_modifier(int key)
{
    INPUT inputs[2];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].type = inputs[1].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = inputs[1].ki.wVk = key;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

uint32_t Platform::get_keyboard_lock_modifiers()
{
    uint32_t modifiers = 0;
    if ((GetKeyState(VK_SCROLL) & 1)) {
        modifiers |= SCROLL_LOCK_MODIFIER;
    }
    if ((GetKeyState(VK_NUMLOCK) & 1)) {
        modifiers |= NUM_LOCK_MODIFIER;
    }
    if ((GetKeyState(VK_CAPITAL) & 1)) {
        modifiers |= CAPS_LOCK_MODIFIER;
    }
    return modifiers;
}

void Platform::set_keyboard_lock_modifiers(uint32_t modifiers)
{
    if (((modifiers >> SCROLL_LOCK_MODIFIER_SHIFT) & 1) != (GetKeyState(VK_SCROLL) & 1)) {
        toggle_modifier(VK_SCROLL);
    }

    if (((modifiers >> Platform::NUM_LOCK_MODIFIER_SHIFT) & 1) != (GetKeyState(VK_NUMLOCK) & 1)) {
        toggle_modifier(VK_NUMLOCK);
    }

    if (((modifiers >> CAPS_LOCK_MODIFIER_SHIFT) & 1) != (GetKeyState(VK_CAPITAL) & 1)) {
        toggle_modifier(VK_CAPITAL);
    }
}

typedef struct KeyboardModifier {
    int vkey;
    int bit;
} KeyboardModifier;

static const KeyboardModifier KEYBOARD_MODIFIERS[] = {
    {VK_LSHIFT, Platform::L_SHIFT_MODIFIER},
    {VK_RSHIFT, Platform::R_SHIFT_MODIFIER},
    {VK_LCONTROL, Platform::L_CTRL_MODIFIER},
    {VK_RCONTROL, Platform::R_CTRL_MODIFIER},
    {VK_LMENU, Platform::L_ALT_MODIFIER},
    {VK_RMENU, Platform::R_ALT_MODIFIER}};

uint32_t Platform::get_keyboard_modifiers()
{
    uint32_t modifiers_state = 0;
    int num_modifiers = sizeof(KEYBOARD_MODIFIERS)/sizeof(KEYBOARD_MODIFIERS[0]);

    for (int i = 0; i < num_modifiers; i++) {
        short key_state = GetAsyncKeyState(KEYBOARD_MODIFIERS[i].vkey);
        modifiers_state |= (key_state & 0x8000) ? KEYBOARD_MODIFIERS[i].bit : 0;
    }

    return modifiers_state;
}

void Platform::reset_cursor_pos()
{
    if (!primary_monitor) {
        return;
    }
    SpicePoint pos =  primary_monitor->get_position();
    SpicePoint size =  primary_monitor->get_size();
    SetCursorPos(pos.x + size.x / 2, pos.y + size.y / 2);
}

class WinBaseLocalCursor: public LocalCursor {
public:
    WinBaseLocalCursor() : _handle (0) {}
    void set(Window window) { SetCursor(_handle);}

protected:
    HCURSOR _handle;
};

class WinLocalCursor: public WinBaseLocalCursor {
public:
    WinLocalCursor(CursorData* cursor_data);
    ~WinLocalCursor();

private:
    bool _shared;
};

WinLocalCursor::WinLocalCursor(CursorData* cursor_data)
    : _shared (false)
{
    const SpiceCursorHeader& header = cursor_data->header();
    const uint8_t* data = cursor_data->data();
    int cur_size;
    int bits = get_size_bits(header, cur_size);
    if (!bits) {
        THROW("invalid curosr type");
    }
    if (header.type == SPICE_CURSOR_TYPE_MONO) {
        _handle = CreateCursor(NULL, header.hot_spot_x, header.hot_spot_y,
                               header.width, header.height, data, data + cur_size);
        return;
    }
    ICONINFO icon;
    icon.fIcon = FALSE;
    icon.xHotspot = header.hot_spot_x;
    icon.yHotspot = header.hot_spot_y;
    icon.hbmColor = icon.hbmMask = NULL;
    HDC hdc = GetDC(NULL);

    switch (header.type) {
    case SPICE_CURSOR_TYPE_ALPHA:
    case SPICE_CURSOR_TYPE_COLOR32:
    case SPICE_CURSOR_TYPE_COLOR16: {
        BITMAPV5HEADER bmp_hdr;
        ZeroMemory(&bmp_hdr, sizeof(bmp_hdr));
        bmp_hdr.bV5Size = sizeof(bmp_hdr);
        bmp_hdr.bV5Width = header.width;
        bmp_hdr.bV5Height = -header.height;
        bmp_hdr.bV5Planes = 1;
        bmp_hdr.bV5BitCount = bits;
        bmp_hdr.bV5Compression = BI_BITFIELDS;
        if (bits == 32) {
            bmp_hdr.bV5RedMask   = 0x00FF0000;
            bmp_hdr.bV5GreenMask = 0x0000FF00;
            bmp_hdr.bV5BlueMask  = 0x000000FF;
        } else if (bits == 16) {
            bmp_hdr.bV5RedMask   = 0x00007C00;
            bmp_hdr.bV5GreenMask = 0x000003E0;
            bmp_hdr.bV5BlueMask  = 0x0000001F;
        }
        if (header.type == SPICE_CURSOR_TYPE_ALPHA) {
            bmp_hdr.bV5AlphaMask = 0xFF000000;
        }
        void* bmp_pixels = NULL;
        icon.hbmColor = CreateDIBSection(hdc, (BITMAPINFO *)&bmp_hdr, DIB_RGB_COLORS, &bmp_pixels,
                                         NULL, 0);
        memcpy(bmp_pixels, data, cur_size);
        icon.hbmMask = CreateBitmap(header.width, header.height, 1, 1,
                                    (header.type == SPICE_CURSOR_TYPE_ALPHA) ? NULL :
                                                                   (CONST VOID *)(data + cur_size));
        break;
    }
    case SPICE_CURSOR_TYPE_COLOR4: {
        BITMAPINFO* bmp_info;
        bmp_info = (BITMAPINFO *)new uint8_t[sizeof(BITMAPINFO) + (sizeof(RGBQUAD) << bits)];
        ZeroMemory(bmp_info, sizeof(BITMAPINFO));
        bmp_info->bmiHeader.biSize = sizeof(bmp_info->bmiHeader);
        bmp_info->bmiHeader.biWidth = header.width;
        bmp_info->bmiHeader.biHeight = -header.height;
        bmp_info->bmiHeader.biPlanes = 1;
        bmp_info->bmiHeader.biBitCount = bits;
        bmp_info->bmiHeader.biCompression =  BI_RGB;
        memcpy(bmp_info->bmiColors, data + cur_size, sizeof(RGBQUAD) << bits);
        icon.hbmColor = CreateDIBitmap(hdc, &bmp_info->bmiHeader, CBM_INIT, data,
                                       bmp_info, DIB_RGB_COLORS);
        icon.hbmMask = CreateBitmap(header.width, header.height, 1, 1,
                                    (CONST VOID *)(data + cur_size + (sizeof(uint32_t) << bits)));
        delete[] (uint8_t *)bmp_info;
        break;
    }
    case SPICE_CURSOR_TYPE_COLOR24:
    case SPICE_CURSOR_TYPE_COLOR8:
    default:
        LOG_WARN("unsupported cursor type %d", header.type);
        _handle = LoadCursor(NULL, IDC_ARROW);
        _shared = true;
        ReleaseDC(NULL, hdc);
        return;
    }

    ReleaseDC(NULL, hdc);

    if (icon.hbmColor && icon.hbmMask) {
        _handle = CreateIconIndirect(&icon);
    }
    if (icon.hbmMask) {
        DeleteObject(icon.hbmMask);
    }
    if (icon.hbmColor) {
        DeleteObject(icon.hbmColor);
    }
}

WinLocalCursor::~WinLocalCursor()
{
    if (_handle && !_shared) {
        DestroyCursor(_handle);
    }
}

LocalCursor* Platform::create_local_cursor(CursorData* cursor_data)
{
    return new WinLocalCursor(cursor_data);
}

class WinInactiveCursor: public WinBaseLocalCursor {
public:
    WinInactiveCursor() { _handle = LoadCursor(NULL, IDC_NO);}
};

LocalCursor* Platform::create_inactive_cursor()
{
    return new WinInactiveCursor();
}

class WinDefaultCursor: public WinBaseLocalCursor {
public:
    WinDefaultCursor() { _handle = LoadCursor(NULL, IDC_ARROW);}
};

LocalCursor* Platform::create_default_cursor()
{
    return new WinDefaultCursor();
}

void Platform::set_display_mode_listner(DisplayModeListener* listener)
{
}

Icon* Platform::load_icon(int id)
{
    HICON icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(id));
    if (!icon) {
        return NULL;
    }
    return new WinIcon(icon);
}

void WinPlatform::enter_modal_loop()
{
    if (modal_loop_active) {
        LOG_INFO("modal loop already active");
        return;
    }

    if (set_modal_loop_timer()) {
        modal_loop_active = true;
    } else {
        LOG_WARN("failed to create modal loop timer");
    }
}

static bool set_modal_loop_timer()
{
    unsigned timeout = main_loop->get_soonest_timeout();
    if (timeout == INFINITE) {
        timeout = MODAL_LOOP_DEFAULT_TIMEOUT; /* for cases timeouts are added after
                                                 the enterance to the loop*/
    }

    if (!SetTimer(platform_win, MODAL_LOOP_TIMER_ID, timeout, NULL)) {
        return false;
    }
    return true;
}

void WinPlatform::exit_modal_loop()
{
    if (!modal_loop_active) {
        LOG_INFO("not inside the loop");
        return;
    }
    KillTimer(platform_win, MODAL_LOOP_TIMER_ID);
    modal_loop_active = false;
}

int Platform::_clipboard_owner = Platform::owner_none;

void Platform::set_clipboard_owner_unlocked(int new_owner)
{
    set_clipboard_owner(new_owner);
}

void Platform::set_clipboard_owner(int new_owner)
{
    const char * const owner_str[] = { "none", "guest", "client" };

    if (new_owner == owner_none) {
        clipboard_listener->on_clipboard_release();

        /* FIXME clear cached clipboard type info and data */
    }
    _clipboard_owner = new_owner;
    LOG_INFO("new clipboard owner: %s", owner_str[new_owner]);
}

bool Platform::on_clipboard_grab(uint32_t *types, uint32_t type_count)
{
    std::set<uint32_t> grab_formats;

    grab_types.clear();
    for (uint32_t i = 0; i < type_count; i++) {
        uint32_t format = get_clipboard_format(types[i]);
        //On first supported type, open and empty the clipboard
        if (format && grab_formats.empty()) {
            if (!OpenClipboard(platform_win)) {
                return false;
            }
            EmptyClipboard();
        }
        //For all supported type set delayed rendering
        if (format) {
            grab_types.insert(types[i]);
            if (grab_formats.insert(format).second) {
                SetClipboardData(format, NULL);
            }
        }
    }
    if (grab_formats.empty()) {
        LOG_INFO("No supported clipboard types in client grab");
        return false;
    }
    CloseClipboard();

    set_clipboard_owner(owner_guest);
    return true;
}

void Platform::set_clipboard_listener(ClipboardListener* listener)
{
    clipboard_listener = listener ? listener : &default_clipboard_listener;
}

static HGLOBAL utf8_alloc(LPCSTR data, int size)
{
    HGLOBAL handle;
    LPVOID buf;
    int len;

    // Received utf8 string is not null-terminated
    if (!(len = MultiByteToWideChar(CP_UTF8, 0, data, size, NULL, 0))) {
        return NULL;
    }
    len++;
    // Allocate and lock clipboard memory
    if (!(handle = GlobalAlloc(GMEM_DDESHARE, len * sizeof(WCHAR)))) {
        return NULL;
    }
    if (!(buf = GlobalLock(handle))) {
        GlobalFree(handle);
        return NULL;
    }
    // Translate data and set clipboard content
    if (!(MultiByteToWideChar(CP_UTF8, 0, data, size, (LPWSTR)buf, len))) {
        GlobalUnlock(handle);
        GlobalFree(handle);
        return NULL;
    }
    ((LPWSTR)buf)[len - 1] = L'\0';
    GlobalUnlock(handle);
    return handle;
}

bool Platform::on_clipboard_notify(uint32_t type, const uint8_t* data, int32_t size)
{
    HANDLE clip_data;
    UINT format;
    bool ret = false;

    if (type == VD_AGENT_CLIPBOARD_NONE) {
        SetEvent(clipboard_event);
        return true;
    }
    switch (type) {
    case VD_AGENT_CLIPBOARD_UTF8_TEXT:
        clip_data = utf8_alloc((LPCSTR)data, size);
        break;
#ifdef USE_CXIMAGE
    case VD_AGENT_CLIPBOARD_IMAGE_PNG:
    case VD_AGENT_CLIPBOARD_IMAGE_BMP: {
        DWORD cximage_format = get_cximage_format(type);
        ASSERT(cximage_format);
        CxImage image((BYTE *)data, size, cximage_format);
        clip_data = image.CopyToHandle();
        break;
    }
#endif
    default:
        LOG_INFO("Unsupported clipboard type %u", type);
        return true;
    }

    format = get_clipboard_format(type);
    if (SetClipboardData(format, clip_data)) {
        SetEvent(clipboard_event);
        return true;
    }
    // We retry clipboard open-empty-set-close only when there is a timeout in WM_RENDERFORMAT
    if (!OpenClipboard(platform_win)) {
        return false;
    }
    EmptyClipboard();
    ret = !!SetClipboardData(format, clip_data);
    CloseClipboard();
    return ret;
}

bool Platform::on_clipboard_request(uint32_t type)
{
    UINT format = get_clipboard_format(type);
    HANDLE clip_data;
    LPCWSTR clip_buf;
    uint8_t* new_data = NULL;
    long new_size;

    bool ret = false;

    if (!format || !IsClipboardFormatAvailable(format) || !OpenClipboard(platform_win)) {
        return false;
    }
    if (!(clip_data = GetClipboardData(format))) {
        CloseClipboard();
        return false;
    }

    switch (type) {
    case VD_AGENT_CLIPBOARD_UTF8_TEXT: {
        if (!(clip_buf = (LPCWSTR)GlobalLock(clip_data))) {
            break;
        }
        size_t len = wcslen((wchar_t*)clip_buf);
        new_size = WideCharToMultiByte(CP_UTF8, 0, clip_buf, len, NULL, 0, NULL, NULL);
        if (!new_size) {
            GlobalUnlock(clip_data);
            break;
        }
        new_data = new uint8_t[new_size];
        if (WideCharToMultiByte(CP_UTF8, 0, clip_buf, len, (LPSTR)new_data, new_size,
                                NULL, NULL)) {
            clipboard_listener->on_clipboard_notify(type, new_data, new_size);
            ret = true;
        }
        delete[] (uint8_t *)new_data;
        GlobalUnlock(clip_data);
        break;
    }
#ifdef USE_CXIMAGE
    case VD_AGENT_CLIPBOARD_IMAGE_PNG:
    case VD_AGENT_CLIPBOARD_IMAGE_BMP: {
        DWORD cximage_format = get_cximage_format(type);
        ASSERT(cximage_format);
        CxImage image;
        if (!image.CreateFromHANDLE(clip_data)) {
            LOG_INFO("Image create from handle failed");
            break;
        }
        if (!image.Encode(new_data, new_size, cximage_format)) {
            LOG_INFO("Image encode to type %u failed", type);
            break;
        }
        LOG_INFO("Image encoded to %u bytes", new_size);
        clipboard_listener->on_clipboard_notify(type, new_data, new_size);
        image.FreeMemory(new_data);
        ret = true;
        break;
    }
#endif
    default:
        LOG_INFO("Unsupported clipboard type %u", type);
    }

    CloseClipboard();
    return ret;
}

void Platform::on_clipboard_release()
{
    SetEvent(clipboard_event);
    set_clipboard_owner(owner_none);
}

static bool has_console = false;
static BOOL parent_console;

static void create_console()
{
    static Mutex console_mutex;

    Lock lock(console_mutex);

    if (has_console) {
        return;
    }

    parent_console = AttachConsole(-1 /* parent */);

    if (!parent_console) {
        AllocConsole();
    }

    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    int hConHandle = _open_osfhandle((intptr_t)h, _O_TEXT);
    FILE * fp;
    /* _open_osfhandle can fail, for instance this will fail:
        start /wait spicec.exe --help | more
       however this actually works:
        cmd /c spicec --help | more
       */
    if (hConHandle != -1) {
        fp = _fdopen(hConHandle, "w");
        *stdout = *fp;
    }

    h = GetStdHandle(STD_INPUT_HANDLE);
    hConHandle = _open_osfhandle((intptr_t)h, _O_TEXT);
    if (hConHandle != -1) {
        fp = _fdopen(hConHandle, "r");
        *stdin = *fp;
    }

    h = GetStdHandle(STD_ERROR_HANDLE);
    hConHandle = _open_osfhandle((intptr_t)h, _O_TEXT);
    if (hConHandle != -1) {
        fp = _fdopen(hConHandle, "w");
        *stderr = *fp;
    }

    has_console = true;

    HWND consol_window = GetConsoleWindow();

    if (consol_window && !parent_console) {
        SetForegroundWindow(consol_window);
    }
}

class ConsoleWait {
public:
    ~ConsoleWait()
    {
        if (has_console && !parent_console) {
            Platform::term_printf("\n\nPress any key to exit...");
            _getch();
        }
    }

} console_wait;


void Platform::term_printf(const char* format, ...)
{
    if (!has_console) {
        create_console();
    }

    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}
