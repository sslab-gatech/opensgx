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
#include "red_window.h"
#include "pixels_source_p.h"
#include "utils.h"
#include "debug.h"
#include <spice/protocol.h>
#include "menu.h"
#include "win_platform.h"
#include "platform_utils.h"

#include <list>

#define NATIVE_CAPTION_STYLE (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)

extern HINSTANCE instance;

static ATOM class_atom = 0;
static const LPCWSTR win_class_name = L"redc_wclass";
static HWND focus_window = NULL;
static HHOOK low_keyboard_hook = NULL;
static bool low_keyboard_hook_on = false;
static HHOOK msg_filter_hook = NULL;
typedef std::list<RedKey> KeysList;
static KeysList filtered_up_keys;

static LRESULT CALLBACK MessageFilterProc(int nCode, WPARAM wParam, LPARAM lParam);

static inline int to_red_mouse_state(WPARAM wParam)
{
    return ((wParam & MK_LBUTTON) ? SPICE_MOUSE_BUTTON_MASK_LEFT : 0) |
           ((wParam & MK_MBUTTON) ? SPICE_MOUSE_BUTTON_MASK_MIDDLE : 0) |
           ((wParam & MK_RBUTTON) ? SPICE_MOUSE_BUTTON_MASK_RIGHT : 0);
}

// Return true if VK_RCONTROL is followed by a VK_RMENU with the same timestamp.
static bool is_fake_ctrl(UINT message, WPARAM wParam, LPARAM lParam)
{
    if ((wParam == VK_CONTROL) && ((HIWORD (lParam) & KF_EXTENDED) == 0)) {
        UINT next_peek;
        if (message == WM_KEYDOWN) {
            next_peek = WM_KEYDOWN;
        } else if (message == WM_SYSKEYUP) {
            next_peek = WM_KEYUP;
        } else {
            next_peek = WM_NULL;
        }
        if (next_peek != WM_NULL) {
            MSG next_msg;
            LONG time = GetMessageTime();
            BOOL msg_exist = PeekMessage(&next_msg, NULL,
                next_peek, next_peek, PM_NOREMOVE);
            if ((msg_exist == TRUE) && ((LONG)next_msg.time == time) &&
                (next_msg.wParam == VK_MENU) &&
                (HIWORD (next_msg.lParam) & KF_EXTENDED)) {
                    return true;
            }
        }
    }
    return false;
}

static inline RedKey translate_key(UINT message, WPARAM wParam, LPARAM lParam)
{
    uint32_t scan = HIWORD(lParam) & 0xff;
    if (scan == 0) {
        return REDKEY_INVALID;
    }
    switch (wParam) {
    case VK_PAUSE:
        return REDKEY_PAUSE;
    case VK_SNAPSHOT:
        return REDKEY_CTRL_PRINT_SCREEN;
    case VK_NUMLOCK:
        return REDKEY_NUM_LOCK;
    case VK_HANGUL:
        return REDKEY_KOREAN_HANGUL;
    case VK_HANJA:
        return REDKEY_KOREAN_HANGUL_HANJA;
    case VK_PROCESSKEY:
        if (scan == 0xf1) {
            return REDKEY_INVALID; // prevent double key (VK_PROCESSKEY + VK_HANJA)
        } else if (scan == 0xf2) {
            return REDKEY_KOREAN_HANGUL;
        }
        break;
    case VK_CONTROL:
        // Ignore the fake right ctrl message which is send when alt-gr is
        // pressed when using a non-US keyboard layout.
        if (is_fake_ctrl(message, wParam, lParam)) {
            return REDKEY_INVALID;
        }
        break;
    default:
        break;
    }
    // TODO: always use virtual key
    bool extended = ((HIWORD (lParam) & KF_EXTENDED) != 0);
    if (extended) {
        scan += REDKEY_ESCAPE_BASE;
    }
    return (RedKey)scan;
}

static inline void send_filtered_keys(RedWindow* window)
{
    KeysList::iterator iter;

    for (iter = filtered_up_keys.begin(); iter != filtered_up_keys.end(); iter++) {
        window->get_listener().on_key_release(*iter);
    }
    filtered_up_keys.clear();
}

static inline bool is_high_surrogate(uint32_t val)
{
    return val >= 0xd800 &&  val <= 0xdbff;
}

static inline bool is_low_surrogate(uint32_t val)
{
    return val >= 0xdc00 &&  val <= 0xdfff;
}

static uint32_t utf16_to_utf32(uint16_t*& utf16, int& len)
{
    if (!len) {
        return 0;
    }

    uint32_t val = utf16[0];

    if (!is_high_surrogate(val)) {
        utf16++;
        len--;
        return val;
    }

    if (len < 2) {
        THROW("partial char");
    }

    uint32_t val2 = utf16[1];

    if (!is_low_surrogate(val2)) {
        THROW("invalid sequence");
    }

    utf16 += 2;
    len -= 2;

    return (((val & 0x3ff) << 10) | (val2 & 0x3ff)) + 0x10000;
}

LRESULT CALLBACK RedWindow_p::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    RedWindow* window = (RedWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    ASSERT(window);

    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps;

        BeginPaint(hWnd, &ps);
        SpicePoint origin = window->get_origin();
        SpiceRect r;
        r.left = ps.rcPaint.left - origin.x;
        r.top = ps.rcPaint.top - origin.y;
        r.right = ps.rcPaint.right - origin.x;
        r.bottom = ps.rcPaint.bottom - origin.y;
        window->get_listener().on_exposed_rect(r);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_MOUSEMOVE: {
        SpicePoint origin = window->get_origin();
        int x = LOWORD(lParam) - origin.x;
        int y = HIWORD(lParam) - origin.y;
        unsigned int buttons_state = to_red_mouse_state(wParam);
        if (!window->_pointer_in_window) {
            window->on_pointer_enter(x, y, buttons_state);
        } else {
            window->get_listener().on_pointer_motion(x, y, buttons_state);
        }
        break;
    }
    case WM_MOUSELEAVE:
        window->on_pointer_leave();
        break;
    case WM_SETFOCUS:
        window->on_focus_in();
        break;
    case WM_KILLFOCUS:
        window->on_focus_out();
        break;
    case WM_LBUTTONDOWN:
        window->get_listener().on_mouse_button_press(SPICE_MOUSE_BUTTON_LEFT,
                                                     to_red_mouse_state(wParam));
        break;
    case WM_LBUTTONUP:
        window->get_listener().on_mouse_button_release(SPICE_MOUSE_BUTTON_LEFT,
                                                       to_red_mouse_state(wParam));
        break;
    case WM_RBUTTONDOWN:
        window->get_listener().on_mouse_button_press(SPICE_MOUSE_BUTTON_RIGHT,
                                                     to_red_mouse_state(wParam));
        break;
    case WM_RBUTTONUP:
        window->get_listener().on_mouse_button_release(SPICE_MOUSE_BUTTON_RIGHT,
                                                       to_red_mouse_state(wParam));
        break;
    case WM_MBUTTONDOWN:
        window->get_listener().on_mouse_button_press(SPICE_MOUSE_BUTTON_MIDDLE,
                                                     to_red_mouse_state(wParam));
        break;
    case WM_MBUTTONUP:
        window->get_listener().on_mouse_button_release(SPICE_MOUSE_BUTTON_MIDDLE,
                                                       to_red_mouse_state(wParam));
        break;
    case WM_MOUSEWHEEL:
        if (HIWORD(wParam) & 0x8000) {
            window->get_listener().on_mouse_button_press(SPICE_MOUSE_BUTTON_DOWN,
                                                         to_red_mouse_state(wParam));
            window->get_listener().on_mouse_button_release(SPICE_MOUSE_BUTTON_DOWN,
                                                           to_red_mouse_state(wParam));
        } else {
            window->get_listener().on_mouse_button_press(SPICE_MOUSE_BUTTON_UP,
                                                         to_red_mouse_state(wParam));
            window->get_listener().on_mouse_button_release(SPICE_MOUSE_BUTTON_UP,
                                                           to_red_mouse_state(wParam));
        }
        break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
        RedKey key = translate_key(message, wParam, lParam);
        window->get_listener().on_key_press(key);

        BYTE key_state[256];
        WCHAR buff[10];
        uint16_t* str_buf = (uint16_t*)buff;
        GetKeyboardState(key_state);
        int n = ToUnicode(wParam, HIWORD(lParam) & 0xff, key_state, buff, 10, 0);
        if (n > 0) {
            uint32_t utf32;
            while ((utf32 = utf16_to_utf32(str_buf, n)) != 0) {
                window->get_listener().on_char(utf32);
            }
        }

        // Allow Windows to translate Alt-F4 to WM_CLOSE message.
        if (!window->_key_interception_on) {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }
    case WM_SYSKEYUP:
    case WM_KEYUP: {
        RedKey key = translate_key(message, wParam, lParam);
        window->get_listener().on_key_release(key);
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* info = (MINMAXINFO*)lParam;
        info->ptMaxSize.x = window->_window_size.x;
        info->ptMaxSize.y = window->_window_size.y;
        info->ptMinTrackSize = info->ptMaxSize;
        info->ptMaxTrackSize = info->ptMaxSize;
        info->ptMaxPosition.x = info->ptMaxPosition.y = 0;
        break;
    }
    case WM_SYSCOMMAND:
        if (window->prossec_menu_commands(wParam & ~0x0f)) {
            break;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_ENTERSIZEMOVE:
    case WM_ENTERMENULOOP:
        ASSERT(filtered_up_keys.empty());
        DBG(0, "enter modal");
        window->get_listener().enter_modal_loop();
        WinPlatform::enter_modal_loop();
        if (msg_filter_hook) {
            LOG_WARN("entering modal loop while filter hook is active");
            UnhookWindowsHookEx(msg_filter_hook);
        }
        msg_filter_hook = SetWindowsHookEx(WH_MSGFILTER, MessageFilterProc,
                                           GetModuleHandle(NULL), GetCurrentThreadId());
        return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_EXITSIZEMOVE:
    case WM_EXITMENULOOP:
        DBG(0, "exit modal");
        window->get_listener().exit_modal_loop();
        WinPlatform::exit_modal_loop();
        UnhookWindowsHookEx(msg_filter_hook);
        msg_filter_hook = NULL;
        send_filtered_keys(window);
        return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_SETCURSOR:
        if (!window->_pointer_in_window) {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) {
            window->on_minimized();
        } else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
            window->on_restored();
        }
        break;
    case WM_WINDOWPOSCHANGING:
        window->on_pos_changing(*window);
        return DefWindowProc(hWnd, message, wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

static ATOM register_class(HINSTANCE instance)
{
    WNDCLASSEX wclass;

    wclass.cbSize = sizeof(WNDCLASSEX);
    wclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wclass.lpfnWndProc = DefWindowProc;
    wclass.cbClsExtra = 0;
    wclass.cbWndExtra = 0;
    wclass.hInstance = instance;
    wclass.hIcon = NULL;
    wclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wclass.hbrBackground = NULL;
    wclass.lpszMenuName = NULL;
    wclass.lpszClassName = win_class_name;
    wclass.hIconSm = NULL;
    return RegisterClassEx(&wclass);
}

RedWindow_p::RedWindow_p()
    : _win (NULL)
    , _modal_refs (0)
    , _no_taskmgr_dll (NULL)
    , _no_taskmgr_hook (NULL)
    , _focused (false)
    , _pointer_in_window (false)
    , _minimized (false)
    , _valid_pos (false)
    , _sys_menu (NULL)
{
}

void RedWindow_p::create(RedWindow& red_window, PixelsSource_p& pixels_source)
{
    HWND window;
    if (!(window = CreateWindow(win_class_name, L"", NATIVE_CAPTION_STYLE, CW_USEDEFAULT,
                                0, CW_USEDEFAULT, 0, NULL, NULL, NULL, NULL))) {
        THROW("create window failed");
    }
    HDC dc = GetDC(window);
    if (!dc) {
        THROW("get dc failed");
    }
    _win = window;
    pixels_source.dc = dc;

    int depth = GetDeviceCaps(dc, BITSPIXEL);
    switch (depth) {
    case 16:
        _format = RedDrawable::RGB16_555;
        break;
    case 32:
    default:
        _format = RedDrawable::RGB32;
        break;
    }
    SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)&red_window);
    SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WindowProc);
}

void RedWindow_p::destroy(PixelsSource_p& pixels_source)
{
    if (!_win) {
        return;
    }

    ReleaseDC(_win, pixels_source.dc);
    SetWindowLongPtr(_win, GWLP_WNDPROC, (LONG_PTR)DefWindowProc);
    SetWindowLongPtr(_win, GWLP_USERDATA, (LONG_PTR)NULL);
    DestroyWindow(_win);
}

RedDrawable::Format RedWindow::get_format()
{
    return _format;
}


void RedWindow_p::on_pos_changing(RedWindow& red_window)
{
    if (_minimized || IsIconic(_win)) {
        return;
    }
    SpicePoint pos = red_window.get_position();
    _x = pos.x;
    _y = pos.y;
    _valid_pos = true;
}

void RedWindow_p::on_minimized()
{
    _minimized = true;
}

void RedWindow_p::on_restored()
{
    if (!_minimized) {
        return;
    }
    _minimized = false;
    if (!_valid_pos) {
        return;
    }
    _valid_pos = false;
    SetWindowPos(_win, NULL, _x, _y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
}

bool RedWindow_p::prossec_menu_commands(int cmd)
{
    CommandMap::iterator iter = _commands_map.find(cmd);
    if (iter == _commands_map.end()) {
        return false;
    }
    (*iter).second.menu->get_target().do_command((*iter).second.command);
    return true;
}

RedWindow::RedWindow(RedWindow::Listener& listener, int screen_id)
    : _listener (listener)
    , _type (TYPE_NORMAL)
    , _local_cursor (NULL)
    , _cursor_visible (true)
    , _trace_key_interception (false)
    , _key_interception_on (false)
    , _menu (NULL)
{
    RECT win_rect;

    create(*this, *(PixelsSource_p*)get_opaque());
    GetWindowRect(_win, &win_rect);
    _window_size.x = win_rect.right - win_rect.left;
    _window_size.y = win_rect.bottom - win_rect.top;
}

RedWindow::~RedWindow()
{
    release_menu(_menu);
    destroy(*(PixelsSource_p*)get_opaque());
    if (_local_cursor) {
        _local_cursor->unref();
    }
}

void RedWindow::set_title(std::string& title)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), title.length(), NULL, 0) + 1;
    WCHAR* wtitle = new WCHAR[len * sizeof(WCHAR)];

    if (wtitle && MultiByteToWideChar(CP_UTF8, 0, title.c_str(), title.length(), wtitle, len)) {
        wtitle[len - 1] = L'\0';
        SetWindowText(_win, wtitle);
    }
    delete []wtitle;
}

void RedWindow::set_icon(Icon* icon)
{
    if (!icon) {
        return;
    }
    WinIcon* w_icon = (WinIcon *)icon;
    SendMessage(_win, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)w_icon->get_handle());
    SendMessage(_win, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)w_icon->get_handle());
}

void RedWindow::raise()
{
    SetWindowPos(_win, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void RedWindow::position_after(RedWindow *win)
{
    HWND after = NULL;

    if (win) {
        after = win->_win;
    }
    SetWindowPos(_win, after, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static LONG to_native_style(RedWindow::Type type)
{
    LONG win_style;

    switch (type) {
    case RedWindow::TYPE_NORMAL:
        win_style = NATIVE_CAPTION_STYLE;
        break;
    case RedWindow::TYPE_FULLSCREEN:
        win_style = 0;
        break;
    default:
        THROW("invalid type %d", type);
    }
    return win_style;
}

void RedWindow::show(int screen_id)
{
    if (IsIconic(_win)) {
        ShowWindow(_win, SW_RESTORE);
    }

    const UINT set_pos_flags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
                               SWP_FRAMECHANGED;
    HWND pos;

    SetWindowLong(_win, GWL_STYLE, to_native_style(_type));
    switch (_type) {
    case TYPE_NORMAL:
        pos = HWND_NOTOPMOST;
        break;
    case TYPE_FULLSCREEN:
        pos = HWND_TOPMOST;
        break;
    default:
        THROW("invalid type %d", _type);
    }
    SetWindowPos(_win, pos, 0, 0, 0, 0, set_pos_flags);
}

void RedWindow::external_show()
{
    LONG_PTR style = ::GetWindowLongPtr(_win, GWL_STYLE);
    if ((style & WS_MINIMIZE) == WS_MINIMIZE) {
        ShowWindow(_win, SW_RESTORE);
    } else {
        // Handle the case when hide() was called and the window is not
        // visible. Since we're not the active window, the call just set the
        // windows' style and doesn't show the window.
        if ((style & WS_VISIBLE) != WS_VISIBLE) {
            show(0);
        }
        // We're not the active the window, so we must be attached to the
        // calling thread's message queue before focus is grabbed.
        HWND front = GetForegroundWindow();
        if (front != NULL) {
            DWORD thread = GetWindowThreadProcessId(front, NULL);
            AttachThreadInput(thread, GetCurrentThreadId(), TRUE);
            SetFocus(_win);
            AttachThreadInput(thread, GetCurrentThreadId(), FALSE);
        }
    }
}

void RedWindow::hide()
{
    ShowWindow(_win, SW_HIDE);
}

static void client_to_window_size(HWND win, int width, int height, SpicePoint& win_size,
                                  RedWindow::Type type)
{
    RECT area;

    SetRect(&area, 0, 0, width, height);
    AdjustWindowRectEx(&area, to_native_style(type), FALSE, GetWindowLong(win, GWL_EXSTYLE));
    win_size.x = area.right - area.left;
    win_size.y = area.bottom - area.top;
}

void RedWindow::move_and_resize(int x, int y, int width, int height)
{
    client_to_window_size(_win, width, height, _window_size, _type);
    SetWindowPos(_win, NULL, x, y, _window_size.x, _window_size.y, SWP_NOACTIVATE | SWP_NOZORDER);
    if (_minimized) {
        _valid_pos = true;
        _x = x;
        _y = y;
    }
}

void RedWindow::move(int x, int y)
{
    SetWindowPos(_win, NULL, x, y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
    if (_minimized) {
        _valid_pos = true;
        _x = x;
        _y = y;
    }
}

void RedWindow::resize(int width, int height)
{
    client_to_window_size(_win, width, height, _window_size, _type);
    SetWindowPos(_win, NULL, 0, 0, _window_size.x, _window_size.y,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
}

void RedWindow::activate()
{
    SetActiveWindow(_win);
    SetFocus(_win);
}

void RedWindow::minimize()
{
    ShowWindow(_win, SW_SHOWMINIMIZED);
}

void RedWindow::set_mouse_position(int x, int y)
{
    POINT pt;
    pt.x = x + get_origin().x;
    pt.y = y + get_origin().y;
    ClientToScreen(_win, &pt);
    SetCursorPos(pt.x, pt.y);
}

class Region_p {
public:
    Region_p(HRGN region) : _region (region) {}
    ~Region_p() {}

    void get_bbox(SpiceRect& bbox) const
    {
        RECT box;

        if (GetRgnBox(_region, &box) == 0) {
            THROW("get region bbox failed");
        }
        bbox.left = box.left;
        bbox.right = box.right;
        bbox.top = box.top;
        bbox.bottom = box.bottom;
    }

    bool contains_point(int x, int y) const
    {
        return !!PtInRegion(_region, x, y);
    }

private:
    HRGN _region;
};

bool RedWindow::get_mouse_anchor_point(SpicePoint& pt)
{
    AutoGDIObject region(CreateRectRgn(0, 0, 0, 0));
    WindowDC win_dc(_win);

    GetRandomRgn(*win_dc, (HRGN)region.get(), SYSRGN);
    SpicePoint anchor;
    Region_p region_p((HRGN)region.get());
    if (!find_anchor_point(region_p, anchor)) {
        return false;
    }
    POINT screen_pt;
    screen_pt.x = anchor.x;
    screen_pt.y = anchor.y;
    ScreenToClient(_win, &screen_pt);
    pt.x = screen_pt.x - get_origin().x;
    pt.y = screen_pt.y - get_origin().y;
    return true;
}

void RedWindow::capture_mouse()
{
    RECT client_rect;
    POINT origin;

    origin.x = origin.y = 0;
    ClientToScreen(_win, &origin);
    GetClientRect(_win, &client_rect);
    OffsetRect(&client_rect, origin.x, origin.y);
    ClipCursor(&client_rect);
}

void RedWindow::release_mouse()
{
    ClipCursor(NULL);
}

void RedWindow::set_cursor(LocalCursor* local_cursor)
{
    ASSERT(local_cursor);
    if (_local_cursor) {
        _local_cursor->unref();
    }
    _local_cursor = local_cursor->ref();
    if (_pointer_in_window) {
        _local_cursor->set(_win);
        while (ShowCursor(TRUE) < 0);
    }
    _cursor_visible = true;
}

void RedWindow::hide_cursor()
{
    if (_cursor_visible) {
        if (_pointer_in_window) {
            while (ShowCursor(FALSE) > -1);
        }
        _cursor_visible = false;
    }
}

void RedWindow::show_cursor()
{
    if (!_cursor_visible) {
        if (_pointer_in_window) {
            while (ShowCursor(TRUE) < 0);
        }
        _cursor_visible = true;
    }
}

SpicePoint RedWindow::get_position()
{
    SpicePoint position;
    if (_minimized || IsIconic(_win)) {
        if (_valid_pos) {
            position.x = _x;
            position.y = _y;
        } else {
            position.x = position.y = 0;
        }
    } else {
        RECT window_rect;
        GetWindowRect(_win, &window_rect);
        position.x = window_rect.left;
        position.y = window_rect.top;
    }
    return position;
}

SpicePoint RedWindow::get_size()
{
    RECT client_rect;
    GetClientRect(_win, &client_rect);
    SpicePoint pt = {client_rect.right - client_rect.left, client_rect.bottom - client_rect.top};
    return pt;
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if  (low_keyboard_hook_on && focus_window && nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *hooked = (KBDLLHOOKSTRUCT*)lParam;
        DWORD dwMsg = (hooked->flags << 24) | (hooked->scanCode << 16) | 1;

        if (hooked->vkCode == VK_NUMLOCK || hooked->vkCode == VK_RSHIFT) {
            dwMsg &= ~(1 << 24);
            SendMessage(focus_window, wParam, hooked->vkCode, dwMsg);
        }
        switch (hooked->vkCode) {
        case VK_CAPITAL:
        case VK_SCROLL:
        case VK_NUMLOCK:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
            break;
        default:
            SendMessage(focus_window, wParam, hooked->vkCode, dwMsg);
            return 1;
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void RedWindow::do_start_key_interception()
{
    low_keyboard_hook_on = true;
    _key_interception_on = true;
    _listener.on_start_key_interception();
}

void RedWindow::do_stop_key_interception()
{
    low_keyboard_hook_on = false;
    _key_interception_on = false;
    _listener.on_stop_key_interception();
}

void RedWindow::start_key_interception()
{
    if (_trace_key_interception) {
        return;
    }
    _trace_key_interception = true;
    if (_focused && _pointer_in_window) {
        do_start_key_interception();
    }
}

void RedWindow::stop_key_interception()
{
    if (!_trace_key_interception) {
        return;
    }
    _trace_key_interception = false;
    if (_key_interception_on) {
        do_stop_key_interception();
    }
}

void RedWindow::init()
{
    if (!(class_atom = register_class(instance))) {
        THROW("register class failed");
    }
    low_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                         GetModuleHandle(NULL), 0);
}

void RedWindow::cleanup()
{
    UnhookWindowsHookEx(low_keyboard_hook);
}

#ifdef USE_OPENGL

void RedWindow::touch_context_draw()
{
}

void RedWindow::touch_context_copy()
{
}

void RedWindow::untouch_context()
{
}

#endif

void RedWindow::on_focus_in()
{
    _focused = true;
    focus_window = _win;
    get_listener().on_activate();
    if (_pointer_in_window && _trace_key_interception) {
        do_start_key_interception();
    }
}

void RedWindow::on_focus_out()
{
    if (!_focused) {
        return;
    }

    _focused = false;
    focus_window = NULL;

    if (_key_interception_on) {
        do_stop_key_interception();
    }
    get_listener().on_deactivate();
}

void RedWindow::on_pointer_enter(int x, int y, unsigned int buttons_state)
{
    if (_pointer_in_window) {
        return;
    }

    if (_cursor_visible) {
        if (_local_cursor) {
            _local_cursor->set(_win);
        }
        while (ShowCursor(TRUE) < 0);
    } else {
        while (ShowCursor(FALSE) > -1);
    }
    _pointer_in_window = true;
    _listener.on_pointer_enter(x, y, buttons_state);

    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = _win;
    if (!TrackMouseEvent(&tme)) {
        THROW("track mouse event failed");
    }
    if (_focused && _trace_key_interception) {
        do_start_key_interception();
    }
}

void RedWindow::on_pointer_leave()
{
    if (!_pointer_in_window) {
        return;
    }
    if (!_cursor_visible) {
        while (ShowCursor(TRUE) < 0);
    }
    _pointer_in_window = false;
    _listener.on_pointer_leave();
    if (_key_interception_on) {
        do_stop_key_interception();
    }
}

static void insert_separator(HMENU menu)
{
    MENUITEMINFO item_info;
    item_info.cbSize = sizeof(item_info);
    item_info.fMask = MIIM_TYPE;
    item_info.fType = MFT_SEPARATOR;
    item_info.dwTypeData = NULL;
    item_info.dwItemData = 0;
    InsertMenuItem(menu, GetMenuItemCount(menu), TRUE, &item_info);
}

static void utf8_to_wchar(const std::string& src, std::wstring& dest)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, src.c_str(), -1, NULL, 0);
    if (!len) {
        THROW("fail to conver utf8 to wchar");
    }
    dest.resize(len);
    MultiByteToWideChar(CP_UTF8, 0, src.c_str(), -1, (wchar_t *)dest.c_str(), len);
}

static void insert_command(HMENU menu, const std::string& name, int id, int state)
{
    MENUITEMINFO item_info;
    item_info.cbSize = sizeof(item_info);
    item_info.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
    item_info.fType = MFT_STRING;
    std::wstring wname;
    utf8_to_wchar(name, wname);
    item_info.cch = wname.size();
    item_info.dwTypeData = (wchar_t *)wname.c_str();
    item_info.wID = id;
    item_info.fState = MFS_ENABLED;
    if (state & Menu::MENU_ITEM_STATE_CHECKED) {
        item_info.fState |= MFS_CHECKED;
    }
    if (state & Menu::MENU_ITEM_STATE_DIM) {
        item_info.fState |= MFS_DISABLED;
    }
    InsertMenuItem(menu, GetMenuItemCount(menu), TRUE, &item_info);
}

static HMENU insert_sub_menu(HMENU menu, const std::string& name)
{
    MENUITEMINFO item_info;
    item_info.cbSize = sizeof(item_info);
    item_info.fMask = MIIM_TYPE | MIIM_SUBMENU;
    item_info.fType = MFT_STRING;
    std::wstring wname;
    utf8_to_wchar(name, wname);
    item_info.cch = wname.size();
    item_info.dwTypeData = (wchar_t *)wname.c_str();
    item_info.hSubMenu = CreateMenu();
    InsertMenuItem(menu, GetMenuItemCount(menu), TRUE, &item_info);
    return item_info.hSubMenu;
}

static int next_free_id = 1;
static const int last_id = 0x0f00;

static std::list<int> free_sys_menu_id;

static int alloc_sys_cmd_id()
{
    if (!free_sys_menu_id.empty()) {
        int ret = *free_sys_menu_id.begin();
        free_sys_menu_id.pop_front();
        return ret;
    }
    if (next_free_id == last_id) {
        THROW("failed");
    }

    return next_free_id++ << 4;
}

static void free_sys_cmd_id(int id)
{
    free_sys_menu_id.push_back(id);
}

static void insert_menu(Menu* menu, HMENU native, CommandMap& _commands_map)
{
    int pos = 0;

    for (;; pos++) {
        Menu::ItemType type = menu->item_type_at(pos);
        switch (type) {
        case Menu::MENU_ITEM_TYPE_COMMAND: {
            std::string name;
            int command_id;
            int state;
            menu->command_at(pos, name, command_id, state);
            int sys_command = alloc_sys_cmd_id();
            _commands_map[sys_command] = CommandInfo(menu, command_id);
            insert_command(native, name, sys_command, state);
            break;
        }
        case Menu::MENU_ITEM_TYPE_MENU: {
            AutoRef<Menu> sub_menu(menu->sub_at(pos));
            HMENU native_sub = insert_sub_menu(native, (*sub_menu)->get_name());
            insert_menu(*sub_menu, native_sub, _commands_map);
            break;
        }
        case Menu::MENU_ITEM_TYPE_SEPARATOR:
            insert_separator(native);
            break;
        case Menu::MENU_ITEM_TYPE_INVALID:
            return;
        }
    }
}

void RedWindow_p::release_menu(Menu* menu)
{
    if (menu) {
        while (!_commands_map.empty()) {
            free_sys_cmd_id((*_commands_map.begin()).first);
            _commands_map.erase(_commands_map.begin());
        }
        GetSystemMenu(_win, TRUE);
        _sys_menu = NULL;
        menu->unref();
        return;
    }
}

int RedWindow::set_menu(Menu* menu)
{
    release_menu(_menu);
    _menu = NULL;

    if (!menu) {
        return 0;
    }

    _sys_menu = GetSystemMenu(_win, FALSE);
    if (! _sys_menu) {
        return -1;
    }

    _menu = menu->ref();

    insert_separator(_sys_menu);
    insert_menu(_menu, _sys_menu, _commands_map);

    return 0;
}

static LRESULT CALLBACK MessageFilterProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        MSG* msg = (MSG*)lParam;

        switch (msg->message) {
        case WM_SYSKEYUP:
        case WM_KEYUP: {
            RedKey key = translate_key(msg->message, wParam, lParam);
            filtered_up_keys.push_back(key);
            break;
        }
        default:
            break;
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
