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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xresource.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/render.h>
#include <X11/extensions/XKB.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xfixes.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <set>
#include <values.h>
#include <signal.h>
#include <sys/shm.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <spice/vd_agent.h>
#include "common/rect.h"

#include "platform.h"
#include "application.h"
#include "utils.h"
#include "x_platform.h"
#include "debug.h"
#include "monitor.h"
#include "record.h"
#include "playback.h"
#include "resource.h"
#include "res.h"
#include "cursor.h"
#include "process_loop.h"

#define DWORD uint32_t
#define BOOL bool
#include "named_pipe.h"

//#define X_DEBUG_SYNC(display) do { XLockDisplay(display); XSync(display, False); XUnlockDisplay(display); } while(0)
#define X_DEBUG_SYNC(display)
#ifdef HAVE_XRANDR12
#define USE_XRANDR_1_2
#endif

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#define USE_XINERAMA_1_0
#endif

static Display* x_display = NULL;
static bool x_shm_avail = false;
static XVisualInfo **vinfo = NULL;
static RedDrawable::Format *screen_format = NULL;
#ifdef USE_OPENGL
static GLXFBConfig **fb_config = NULL;
#endif // USE_OPENGL
static XIM x_input_method = NULL;
static XIC x_input_context = NULL;

static Window platform_win = 0;
static XContext win_proc_context;
static ProcessLoop* main_loop = NULL;
static int focus_count = 0;

static bool using_xrandr_1_0 = false;
#ifdef USE_XRANDR_1_2
static bool using_xrandr_1_2 = false;
#endif

static int xrandr_event_base;
static int xrandr_error_base;
static int xrandr_major = 0;
static int xrandr_minor = 0;

static bool using_xrender_0_5 = false;

static bool using_xfixes_1_0 = false;

static int xfixes_event_base;
static int xfixes_error_base;

#ifdef USE_XINERAMA_1_0
static bool using_xinerama_1_0 = false;
#endif

static unsigned int caps_lock_mask = 0;
static unsigned int num_lock_mask = 0;

//FIXME: nicify
struct clipboard_format_info {
    uint32_t type;
    const char *atom_names[16];
    Atom atoms[16];
    int atom_count;
};

static struct clipboard_format_info clipboard_formats[] = {
    { VD_AGENT_CLIPBOARD_UTF8_TEXT, { "UTF8_STRING",
      "text/plain;charset=UTF-8", "text/plain;charset=utf-8", NULL }, { 0 }, 0},
    { VD_AGENT_CLIPBOARD_IMAGE_PNG, { "image/png", NULL }, { 0 }, 0},
    { VD_AGENT_CLIPBOARD_IMAGE_BMP, { "image/bmp", "image/x-bmp",
      "image/x-MS-bmp", "image/x-win-bitmap", NULL }, { 0 }, 0},
    { VD_AGENT_CLIPBOARD_IMAGE_TIFF, { "image/tiff", NULL }, { 0 }, 0},
    { VD_AGENT_CLIPBOARD_IMAGE_JPG, { "image/jpeg", NULL }, { 0 }, 0},
};

#define clipboard_format_count ((int)(sizeof(clipboard_formats)/sizeof(clipboard_formats[0])))

struct selection_request {
    XEvent event;
    selection_request *next;
};

static int expected_targets_notifies = 0;
static bool waiting_for_property_notify = false;
static uint8_t* clipboard_data = NULL;
static uint32_t clipboard_data_size = 0;
static uint32_t clipboard_data_space = 0;
static Atom clipboard_request_target = None;
static selection_request *next_selection_request = NULL;
static int clipboard_type_count = 0;
static uint32_t clipboard_agent_types[256];
static Atom clipboard_x11_targets[256];
static Mutex clipboard_lock;
static Atom clipboard_prop;
static Atom incr_atom;
static Atom targets_atom;
static Atom multiple_atom;
static Bool handle_x_error = false;
static int x_error_code;

static void handle_selection_request();

class DefaultEventListener: public Platform::EventListener {
public:
    virtual void on_app_activated() {}
    virtual void on_app_deactivated() {}
    virtual void on_monitors_change() {}
};

static DefaultEventListener default_event_listener;
static Platform::EventListener* event_listener = &default_event_listener;

class DefaultDisplayModeListener: public Platform::DisplayModeListener {
public:
    void on_display_mode_change() {}
};

static DefaultDisplayModeListener default_display_mode_listener;
static Platform::DisplayModeListener* display_mode_listener = &default_display_mode_listener;

class DefaultClipboardListener: public Platform::ClipboardListener {
public:
    void on_clipboard_grab(uint32_t *types, uint32_t type_count) {}
    void on_clipboard_request(uint32_t type) {}
    void on_clipboard_notify(uint32_t type, uint8_t* data, int32_t size) {}
    void on_clipboard_release() {}
};

static DefaultClipboardListener default_clipboard_listener;
static Platform::ClipboardListener* clipboard_listener = &default_clipboard_listener;

static void handle_x_errors_start(void)
{
    handle_x_error = True;
    x_error_code = 0;
}

static int handle_x_errors_stop(void)
{
    handle_x_error = False;
    return x_error_code;
}

static const char *atom_name(Atom atom)
{
    const char *name;

    if (atom == None)
        return "None";

    XLockDisplay(x_display);
    handle_x_errors_start();
    name = XGetAtomName(x_display, atom);
    if (handle_x_errors_stop()) {
        name = "Bad Atom";
    }
    XUnlockDisplay(x_display);

    return name;
}

static uint32_t get_clipboard_type(Atom target) {
    int i, j;

    if (target == None)
        return VD_AGENT_CLIPBOARD_NONE;

    for (i = 0; i < clipboard_format_count; i++) {
        for (j = 0; j < clipboard_formats[i].atom_count; i++) {
            if (clipboard_formats[i].atoms[j] == target) {
                return clipboard_formats[i].type;
            }
        }
    }

    LOG_WARN("unexpected selection type %s", atom_name(target));
    return VD_AGENT_CLIPBOARD_NONE;
}

static Atom get_clipboard_target(uint32_t type) {
    int i;

    for (i = 0; i < clipboard_type_count; i++)
        if (clipboard_agent_types[i] == type)
            return clipboard_x11_targets[i];

    LOG_WARN("client requested unavailable type %u", type);
    return None;
}

NamedPipe::ListenerRef NamedPipe::create(const char *name, ListenerInterface& listener_interface)
{
    ASSERT(main_loop && main_loop->is_same_thread(pthread_self()));
    return (ListenerRef)(new LinuxListener(name, listener_interface, *main_loop));
}

void NamedPipe::destroy(ListenerRef listener_ref)
{
    ASSERT(main_loop && main_loop->is_same_thread(pthread_self()));
    delete (LinuxListener *)listener_ref;
}

void NamedPipe::destroy_connection(ConnectionRef conn_ref)
{
    ASSERT(main_loop && main_loop->is_same_thread(pthread_self()));
    delete (Session *)conn_ref;
}

int32_t NamedPipe::read(ConnectionRef conn_ref, uint8_t* buf, int32_t size)
{
    if (((Session *)conn_ref) != NULL) {
        return ((Session *)conn_ref)->read(buf, size);
    }
    return -1;
}

int32_t NamedPipe::write(ConnectionRef conn_ref, const uint8_t* buf, int32_t size)
{
    if (((Session *)conn_ref) != NULL) {
        return ((Session *)conn_ref)->write(buf, size);
    }
    return -1;
}

class XEventHandler: public EventSources::File {
public:
    XEventHandler(Display& x_display, XContext& win_proc_context);
    virtual void on_event();
    virtual int get_fd() {return _x_fd;}

private:
    Display& _x_display;
    XContext& _win_proc_context;
    int _x_fd;
};

XEventHandler::XEventHandler(Display& x_display, XContext& win_proc_context)
    : _x_display (x_display)
    , _win_proc_context (win_proc_context)
{
    if ((_x_fd = ConnectionNumber(&x_display)) == -1) {
        THROW("get x fd failed");
    }
}

void XEventHandler::on_event()
{
    XLockDisplay(x_display);
    while (XPending(&_x_display)) {
        XPointer proc_pointer;
        XEvent event;

        XNextEvent(&_x_display, &event);
        if (event.xany.window == None) {
            continue;
        }

	if (XFilterEvent(&event, None)) {
	    continue;
	}

        if (XFindContext(&_x_display, event.xany.window, _win_proc_context, &proc_pointer)) {
            /* When XIM + ibus is in use XIM creates an invisible window for
               its own purposes, we sometimes get a _GTK_LOAD_ICONTHEMES
               ClientMessage event on this window -> skip logging. */
            if (event.type != ClientMessage) {
                LOG_WARN(
                    "Event on window without a win proc, type: %d, window: %u",
                    event.type, (unsigned int)event.xany.window);
            }
            continue;
        }
        XUnlockDisplay(x_display);
        ((XPlatform::win_proc_t)proc_pointer)(event);
        XLockDisplay(x_display);
    }
    XUnlockDisplay(x_display);
}

Display* XPlatform::get_display()
{
    return x_display;
}

bool XPlatform::is_x_shm_avail()
{
    return x_shm_avail;
}

XImage *XPlatform::create_x_shm_image(RedDrawable::Format format,
                                      int width, int height, int depth,
                                      Visual *visual,
                                      XShmSegmentInfo **shminfo_out)
{
    XImage *image;
    XShmSegmentInfo *shminfo;

    /* We need to lock the display early, and force any pending requests to
       be processed, to make sure that any errors reported by
       handle_x_errors_stop() are actually ours */
    XLockDisplay(XPlatform::get_display());
    XSync(XPlatform::get_display(), False);

    shminfo = new XShmSegmentInfo;
    shminfo->shmid = -1;
    shminfo->shmaddr = NULL;

    image = XShmCreateImage(XPlatform::get_display(),
                            format == RedDrawable::A1 ? NULL : visual,
                            format == RedDrawable::A1 ? 1 : depth,
                            format == RedDrawable::A1 ? XYBitmap : ZPixmap,
                            NULL, shminfo, width, height);
    if (image == NULL) {
	x_shm_avail = false;
        goto err1;
    }

    shminfo->shmid = shmget(IPC_PRIVATE, height * image->bytes_per_line,
                            IPC_CREAT | 0777);
    if (shminfo->shmid < 0) {
        /* EINVAL indicates, most likely, that the segment we asked for
         * is bigger than SHMMAX, so we don't treat it as a permanent
         * error. ENOSPC and ENOMEM may also indicate this, but
         * more likely are permanent errors.
         */
        if (errno != EINVAL) {
            x_shm_avail = false;
        }
        goto err2;
    }

    shminfo->shmaddr = (char *)shmat(shminfo->shmid, 0, 0);
    if (!shminfo->shmaddr) {
        /* Failure in shmat is almost certainly permanent. Most likely error is
         * EMFILE, which would mean that we've exceeded the per-process
         * Shm segment limit.
         */
        x_shm_avail = false;

        goto err2;
    }

    shminfo->readOnly = False;
    if (!XShmAttach(XPlatform::get_display(), shminfo)) {
        x_shm_avail = false;
        goto err2;
    }

    handle_x_errors_start();

    /* Ensure the xserver has attached the xshm segment */
    XSync (XPlatform::get_display(), False);

    if (handle_x_errors_stop()) {
        x_shm_avail = false;
        goto err2;
    }

    /* Mark segment as released so that it will be destroyed when
       the xserver releases the segment. This way we won't leak
       the segment if the client crashes. */
    shmctl(shminfo->shmid, IPC_RMID, 0);

    XUnlockDisplay(XPlatform::get_display());

    image->data = (char *)shminfo->shmaddr;

    *shminfo_out = shminfo;
    return image;

err2:
    XDestroyImage(image);
    if (shminfo->shmaddr != NULL) {
        shmdt(shminfo->shmaddr);
    }
    if (shminfo->shmid != -1) {
        shmctl(shminfo->shmid, IPC_RMID, 0);
    }

err1:
    XUnlockDisplay(XPlatform::get_display());
    delete shminfo;
    return NULL;
}

XImage *XPlatform::create_x_image(RedDrawable::Format format,
                                  int width, int height, int depth,
                                  Visual *visual,
                                  XShmSegmentInfo **shminfo_out)
{
    XImage *image = NULL;
    uint8_t *data;
    size_t stride;

    *shminfo_out = NULL;

    if (XPlatform::is_x_shm_avail()) {
        image = XPlatform::create_x_shm_image(format, width, height,
                                              depth, visual,
                                              shminfo_out);
    }

    if (image != NULL) {
        return image;
    }

    stride = SPICE_ALIGN(width * RedDrawable::format_to_bpp (format), 32) / 8;
    /* Must use malloc here, not new, because XDestroyImage will free() it */
    data = (uint8_t *)malloc(height * stride);
    if (data == NULL) {
        THROW("Out of memory");
    }

    XLockDisplay(XPlatform::get_display());
    if (format == RedDrawable::A1) {
        image = XCreateImage(XPlatform::get_display(),
                             NULL, 1, XYBitmap,
                             0, (char *)data, width, height, 32, stride);
    } else {
        image = XCreateImage(XPlatform::get_display(),
                             visual, depth, ZPixmap,
                             0, (char *)data, width, height, 32, stride);
    }
    XUnlockDisplay(XPlatform::get_display());

    return image;
}


void XPlatform::free_x_image(XImage *image,
                             XShmSegmentInfo *shminfo)
{
    if (shminfo) {
        XLockDisplay(XPlatform::get_display());
        XShmDetach(XPlatform::get_display(), shminfo);
    }
    if (image) {
        XDestroyImage(image);
    }
    if (shminfo) {
        XSync(XPlatform::get_display(), False);
        shmdt(shminfo->shmaddr);
        XUnlockDisplay(XPlatform::get_display());
        delete shminfo;
    }
}


XVisualInfo** XPlatform::get_vinfo()
{
    return vinfo;
}

RedDrawable::Format XPlatform::get_screen_format(int screen)
{
    return screen_format[screen];
}

#ifdef USE_OPENGL
GLXFBConfig** XPlatform::get_fbconfig()
{
    return fb_config;
}
#endif // USE_OPENGL

XIC XPlatform::get_input_context()
{
    return x_input_context;
}

void XPlatform::set_win_proc(Window win, win_proc_t proc)
{
    int res;

    XLockDisplay(x_display);
    res = XSaveContext(x_display, win, win_proc_context, (XPointer)proc);
    XUnlockDisplay(x_display);
    if (res) {
        THROW("set win proc failed");
    }
}

void XPlatform::cleare_win_proc(Window win)
{
    XLockDisplay(x_display);
    XDeleteContext(x_display, win, win_proc_context);
    XUnlockDisplay(x_display);
}

void Platform::send_quit_request()
{
    ASSERT(main_loop);
    main_loop->quit(0);
}

uint64_t Platform::get_monolithic_time()
{
#ifdef HAVE_CLOCK_GETTIME
    struct timespec time_space;
    clock_gettime(CLOCK_MONOTONIC, &time_space);
    return uint64_t(time_space.tv_sec) * 1000 * 1000 * 1000 + uint64_t(time_space.tv_nsec);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return uint64_t(tv.tv_sec) * 1000 * 1000 * 1000 + uint64_t(tv.tv_usec) * 1000;
#endif
}

void Platform::get_temp_dir(std::string& path)
{
    path = "/tmp/";
}

uint64_t Platform::get_process_id()
{
    static uint64_t pid = uint64_t(getpid());
    return pid;
}

uint64_t Platform::get_thread_id()
{
    return uint64_t(syscall(SYS_gettid));
}

void Platform::error_beep()
{
    if (!x_display) {
        return;
    }

    XBell(x_display, 0);
    XFlush(x_display);
}

void Platform::msleep(unsigned int millisec)
{
    usleep(millisec * 1000);
}

void Platform::yield()
{
    POSIX_YIELD_FUNC;
}

void Platform::term_printf(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

void Platform::set_thread_priority(void* thread, Platform::ThreadPriority in_priority)
{
    ASSERT(thread == NULL);
    int priority;

    switch (in_priority) {
    case PRIORITY_TIME_CRITICAL:
        priority = -20;
        break;
    case PRIORITY_HIGH:
        priority = -2;
        break;
    case PRIORITY_ABOVE_NORMAL:
        priority = -1;
        break;
    case PRIORITY_NORMAL:
        priority = 0;
        break;
    case PRIORITY_BELOW_NORMAL:
        priority = 1;
        break;
    case PRIORITY_LOW:
        priority = 2;
        break;
    case PRIORITY_IDLE:
        priority = 19;
        break;
    default:
        THROW("invalid priority %d", in_priority);
    }

    pid_t tid = syscall(SYS_gettid);
    if (setpriority(PRIO_PROCESS, tid, priority) == -1) {
        DBG(0, "setpriority failed %s", strerror(errno));
    }
}

void Platform::set_event_listener(EventListener* listener)
{
    event_listener = listener ? listener : &default_event_listener;
}

void Platform::set_display_mode_listner(DisplayModeListener* listener)
{
    display_mode_listener = listener ? listener : &default_display_mode_listener;
}

#ifdef USE_XRANDR_1_2
class FreeScreenResources {
public:
    void operator () (XRRScreenResources* res) { XRRFreeScreenResources(res);}
};
typedef _AutoRes<XRRScreenResources, FreeScreenResources> AutoScreenRes;

class FreeOutputInfo {
public:
    void operator () (XRROutputInfo* output_info) { XRRFreeOutputInfo(output_info);}
};

typedef _AutoRes<XRROutputInfo, FreeOutputInfo> AutoOutputInfo;

class FreeCrtcInfo {
public:
    void operator () (XRRCrtcInfo* crtc_info) { XRRFreeCrtcInfo(crtc_info);}
};
typedef _AutoRes<XRRCrtcInfo, FreeCrtcInfo> AutoCrtcInfo;

static XRRModeInfo* find_mod(XRRScreenResources* res, RRMode mode)
{
    for (int i = 0; i < res->nmode; i++) {
        if (res->modes[i].id == mode) {
            return &res->modes[i];
        }
    }
    return NULL;
}

#endif

//#define SHOW_SCREEN_INFO
#ifdef SHOW_SCREEN_INFO

static float mode_refresh(XRRModeInfo *mode_info)
{
    if (!mode_info->hTotal || !mode_info->vTotal) {
        return 0;
    }

    return ((float)mode_info->dotClock / ((float)mode_info->hTotal * (float)mode_info->vTotal));
}

static void show_scren_info()
{
    int screen = DefaultScreen(x_display);
    Window root_window = RootWindow(x_display, screen);

    int minWidth;
    int minHeight;
    int maxWidth;
    int maxHeight;

    XLockDisplay(x_display);
    AutoScreenRes res(XRRGetScreenResources(x_display, root_window));
    XUnlockDisplay(x_display);

    if (!res.valid()) {
        throw Exception(fmt("%s: get screen resources failed") % __FUNCTION__);
    }

    XLockDisplay(x_display);
    XRRGetScreenSizeRange(x_display, root_window, &minWidth, &minHeight,
                          &maxWidth, &maxHeight);
    printf("screen: min %dx%d max %dx%d\n", minWidth, minHeight,
           maxWidth, maxHeight);

    int i, j;

    for (i = 0; i < res->noutput; i++) {
        AutoOutputInfo output_info(XRRGetOutputInfo(x_display, res.get(), res->outputs[i]));

        printf("output %s", output_info->name);
        if (output_info->crtc == None) {
            printf(" crtc None");
        } else {
            printf(" crtc 0x%lx", output_info->crtc);
        }
        switch (output_info->connection) {
        case RR_Connected:
            printf(" Connected");
            break;
        case RR_Disconnected:
            printf(" Disconnected");
            break;
        case RR_UnknownConnection:
            printf(" UnknownConnection");
            break;
        }
        printf(" ncrtc %u nclone %u nmode %u\n",
               output_info->ncrtc,
               output_info->nclone,
               output_info->nmode);
        for (j = 0; j < output_info->nmode; j++) {
            XRRModeInfo* mode = find_mod(res.get(), output_info->modes[j]);
            printf("\t%lu:", output_info->modes[j]);
            if (!mode) {
                printf(" ???\n");
                continue;
            }
            printf(" %s %ux%u %f\n", mode->name, mode->width, mode->height, mode_refresh(mode));
        }
    }

    for (i = 0; i < res->ncrtc; i++) {
        AutoCrtcInfo crtc_info(XRRGetCrtcInfo(x_display, res.get(), res->crtcs[i]));
        printf("crtc: 0x%lx x %d y %d  width %u height %u  mode %lu\n",
               res->crtcs[i],
               crtc_info->x, crtc_info->y,
               crtc_info->width, crtc_info->height, crtc_info->mode);
    }
    XUnlockDisplay(x_display);
}

#endif

enum RedScreenRotation {
    RED_SCREEN_ROTATION_0,
    RED_SCREEN_ROTATION_90,
    RED_SCREEN_ROTATION_180,
    RED_SCREEN_ROTATION_270,
};

enum RedSubpixelOrder {
    RED_SUBPIXEL_ORDER_UNKNOWN,
    RED_SUBPIXEL_ORDER_H_RGB,
    RED_SUBPIXEL_ORDER_H_BGR,
    RED_SUBPIXEL_ORDER_V_RGB,
    RED_SUBPIXEL_ORDER_V_BGR,
    RED_SUBPIXEL_ORDER_NONE,
};

static void root_win_proc(XEvent& event);
static void process_monitor_configure_events(Window root);

class XMonitor;
typedef std::list<XMonitor*> XMonitorsList;

class XScreen {
public:
    XScreen(Display* display, int screen);
    virtual ~XScreen() {}

    virtual void publish_monitors(MonitorsList& monitors) = 0;

    Display* get_display() {return _display;}
    int get_screen() {return _screen;}

    void set_broken() {_broken = true;}
    bool is_broken() const {return _broken;}
    int get_width() const {return _width;}
    void set_width(int width) {_width = width;}
    int get_height() const { return _height;}
    void set_height(int height) {_height = height;}
    SpicePoint get_position() const {return _position;}

private:
    Display* _display;
    int _screen;
    SpicePoint _position;
    int _width;
    int _height;
    bool _broken;
};

XScreen::XScreen(Display* display, int screen)
    : _display (display)
    , _screen (screen)
    , _broken (false)
{
    int root = RootWindow(display, screen);

    XWindowAttributes attrib;

    XLockDisplay(display);
    XGetWindowAttributes(display, root, &attrib);
    XUnlockDisplay(display);

    _position.x = attrib.x;
    _position.y = attrib.y;
    _width = attrib.width;
    _height = attrib.height;
}

class StaticScreen: public XScreen, public Monitor {
public:
    StaticScreen(Display* display, int screen, int& next_mon_id)
        : XScreen(display, screen)
        , Monitor(next_mon_id++)
        , _out_of_sync (false)
    {
    }

    virtual void publish_monitors(MonitorsList& monitors)
    {
        monitors.push_back(this);
    }

    virtual int get_depth() { return XPlatform::get_vinfo()[0]->depth;}
    virtual SpicePoint get_position() { return XScreen::get_position();}
    virtual SpicePoint get_size() const { SpicePoint pt = {get_width(), get_height()}; return pt;}
    virtual bool is_out_of_sync() { return _out_of_sync;}
    virtual int get_screen_id() { return get_screen();}

protected:
    virtual void do_set_mode(int width, int height)
    {
        _out_of_sync = width > get_width() || height > get_height();
    }

    virtual void do_restore() {}

private:
    bool _out_of_sync;
};

class DynamicScreen: public XScreen, public Monitor {
public:
    DynamicScreen(Display* display, int screen, int& next_mon_id);
    virtual ~DynamicScreen();

    void publish_monitors(MonitorsList& monitors);
    virtual int get_depth() { return XPlatform::get_vinfo()[0]->depth;}
    virtual SpicePoint get_position() { return XScreen::get_position();}
    virtual SpicePoint get_size() const { SpicePoint pt = {get_width(), get_height()}; return pt;}
    virtual bool is_out_of_sync() { return _out_of_sync;}
    virtual int get_screen_id() { return get_screen();}

protected:
    virtual void do_set_mode(int width, int height);
    virtual void do_restore();

private:
    bool set_screen_size(int size_index);

private:
    int _saved_width;
    int _saved_height;
    bool _out_of_sync;
};

static void intern_clipboard_atoms()
{
    int i, j;
    static bool interned = false;
    if (interned) return;

    XLockDisplay(x_display);
    clipboard_prop = XInternAtom(x_display, "CLIPBOARD", False);
    incr_atom = XInternAtom(x_display, "INCR", False);
    multiple_atom = XInternAtom(x_display, "MULTIPLE", False);
    targets_atom = XInternAtom(x_display, "TARGETS", False);
    for(i = 0; i < clipboard_format_count; i++) {
        for(j = 0; clipboard_formats[i].atom_names[j]; j++) {
            clipboard_formats[i].atoms[j] =
                XInternAtom(x_display, clipboard_formats[i].atom_names[j],
                            False);
        }
        clipboard_formats[i].atom_count = j;
    }

    XUnlockDisplay(x_display);

    interned = true;
}

DynamicScreen::DynamicScreen(Display* display, int screen, int& next_mon_id)
    : XScreen(display, screen)
    , Monitor(next_mon_id++)
    , _saved_width (get_width())
    , _saved_height (get_height())
    , _out_of_sync (false)
{
    if (platform_win != 0)
        return;

    X_DEBUG_SYNC(display);
    //FIXME: replace RootWindow() in other refs as well?
    XLockDisplay(display);
    platform_win = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, 1, 1, 0, 0, 0);
    XUnlockDisplay(display);

    LOG_INFO("platform_win: %u", (unsigned int)platform_win);
    intern_clipboard_atoms();
    XSelectInput(display, platform_win, StructureNotifyMask);
    XRRSelectInput(display, platform_win, RRScreenChangeNotifyMask);
    if (using_xfixes_1_0) {
        XFixesSelectSelectionInput(display, platform_win, clipboard_prop,
                                   XFixesSetSelectionOwnerNotifyMask|
                                   XFixesSelectionWindowDestroyNotifyMask|
                                   XFixesSelectionClientCloseNotifyMask);
    }

    Monitor::self_monitors_change++;
    process_monitor_configure_events(platform_win);
    Monitor::self_monitors_change--;

    XPlatform::set_win_proc(platform_win, root_win_proc);
    X_DEBUG_SYNC(display);
}

DynamicScreen::~DynamicScreen()
{
    restore();
}

void DynamicScreen::publish_monitors(MonitorsList& monitors)
{
    monitors.push_back(this);
}

class SizeInfo {
public:
    SizeInfo(int int_index, XRRScreenSize* in_size) : index (int_index), size (in_size) {}

    int index;
    XRRScreenSize* size;
};

class SizeCompare {
public:
    bool operator () (const SizeInfo& size1, const SizeInfo& size2) const
    {
        int area1 = size1.size->width * size1.size->height;
        int area2 = size2.size->width * size2.size->height;
        return area1 < area2 || (area1 == area2 && size1.index < size2.index);
    }
};

void DynamicScreen::do_set_mode(int width, int height)
{
    int num_sizes;

    X_DEBUG_SYNC(get_display());

    XLockDisplay(get_display());
    XRRScreenSize* sizes = XRRSizes(get_display(), get_screen(), &num_sizes);
    XUnlockDisplay(get_display());

    typedef std::set<SizeInfo, SizeCompare> SizesSet;
    SizesSet sizes_set;

    for (int i = 0; i < num_sizes; i++) {
        if (sizes[i].width >= width && sizes[i].height >= height) {
            sizes_set.insert(SizeInfo(i, &sizes[i]));
        }
    }
    _out_of_sync = true;
    if (!sizes_set.empty() && set_screen_size((*sizes_set.begin()).index)) {
        _out_of_sync = false;
    }
    X_DEBUG_SYNC(get_display());
}

void DynamicScreen::do_restore()
{
    X_DEBUG_SYNC(get_display());
    if (is_broken() || (get_width() == _saved_width && get_height() == _saved_height)) {
        return;
    }
    int num_sizes;

    XLockDisplay(get_display());
    XRRScreenSize* sizes = XRRSizes(get_display(), get_screen(), &num_sizes);
    XUnlockDisplay(get_display());

    for (int i = 0; i < num_sizes; i++) {
        if (sizes[i].width == _saved_width && sizes[i].height == _saved_height) {
            set_screen_size(i);
            return;
        }
    }
    X_DEBUG_SYNC(get_display());
    LOG_WARN("can't find startup mode");
}

bool DynamicScreen::set_screen_size(int size_index)
{
    X_DEBUG_SYNC(get_display());
    Window root_window = RootWindow(get_display(), get_screen());
    XRRScreenConfiguration* config;

    XLockDisplay(get_display());
    config = XRRGetScreenInfo(get_display(), root_window);
    XUnlockDisplay(get_display());

    if (!config) {
        LOG_WARN("get screen info failed");
        return false;
    }
    Rotation rotation;
    XRRConfigCurrentConfiguration(config, &rotation);

    Monitor::self_monitors_change++;
    XLockDisplay(get_display());
    /*what status*/
    XRRSetScreenConfig(get_display(), config, root_window, size_index, rotation, CurrentTime);
    XUnlockDisplay(get_display());
    process_monitor_configure_events(platform_win);
    Monitor::self_monitors_change--;
    XRRFreeScreenConfigInfo(config);
    X_DEBUG_SYNC(get_display());

    int num_sizes;

    XLockDisplay(get_display());
    XRRScreenSize* sizes = XRRSizes(get_display(), get_screen(), &num_sizes);
    XUnlockDisplay(get_display());

    if (num_sizes <= size_index) {
        THROW("invalid sizes size");
    }
    set_width(sizes[size_index].width);
    set_height(sizes[size_index].height);
    return true;
}

#ifdef USE_XINERAMA_1_0

class XineramaMonitor;
typedef std::list<XineramaMonitor*> XineramaMonitorsList;

class XineramaScreen : public XScreen {
public:
    XineramaScreen(Display* display, int screen, int& next_mon_id, XineramaScreenInfo* xin_screens,
                   int num_xin_screens);
    virtual ~XineramaScreen();

    void publish_monitors(MonitorsList& monitors);

private:
    XineramaMonitorsList _monitors;
};

class XineramaMonitor : public Monitor {
public:
    XineramaMonitor(int id, XineramaScreenInfo& xin_screen);

    virtual void do_set_mode(int width, int height);
    virtual void do_restore() {}
    virtual int get_depth() { return 32;}
    virtual SpicePoint get_position() { return _position;}
    virtual SpicePoint get_size() const { return _size;}
    virtual bool is_out_of_sync() { return _out_of_sync;}
    virtual int get_screen_id() { return 0;}

private:
    SpicePoint _position;
    SpicePoint _size;
    bool _out_of_sync;
};

XineramaScreen::XineramaScreen(Display* display, int screen, int& next_mon_id,
                               XineramaScreenInfo* xin_screens, int num_xin_screens)
    : XScreen(display, screen)
{
    X_DEBUG_SYNC(display);
    for (int i = 0; i < num_xin_screens; i++) {
        _monitors.push_back(new XineramaMonitor(next_mon_id++, xin_screens[i]));
    }
    Window root_window = RootWindow(display, screen);
    XSelectInput(display, root_window, StructureNotifyMask);
    XRRSelectInput(display, root_window, RRScreenChangeNotifyMask);     // TODO: this fails if we don't have RR extension (but do have XINERAMA)
    XPlatform::set_win_proc(root_window, root_win_proc);     // Xlib:  extension "RANDR" missing on display ":3.0".
    X_DEBUG_SYNC(display);
}

XineramaScreen::~XineramaScreen()
{
    while (!_monitors.empty()) {
        XineramaMonitor* monitor = _monitors.front();
        _monitors.pop_front();
        delete monitor;
    }
}

void XineramaScreen::publish_monitors(MonitorsList& monitors)
{
    XineramaMonitorsList::iterator iter = _monitors.begin();
    for (; iter != _monitors.end(); iter++) {
        monitors.push_back(*iter);
    }
}

XineramaMonitor::XineramaMonitor(int id, XineramaScreenInfo& screen_info)
    : Monitor(id)
    , _out_of_sync (false)
{
    _position.x = screen_info.x_org;
    _position.y = screen_info.y_org;
    _size.x = screen_info.width;
    _size.y = screen_info.height;
}


void XineramaMonitor::do_set_mode(int width, int height)
{
    _out_of_sync = width > _size.x || height > _size.y;
}

#endif

#ifdef USE_XRANDR_1_2

class MultyMonScreen: public XScreen {
public:
    MultyMonScreen(Display* display, int screen, int& next_mon_id);
    virtual ~MultyMonScreen();

    virtual void publish_monitors(MonitorsList& monitors);

    void disable();
    void enable();
    void restore();

    bool set_monitor_mode(XMonitor& monitor, const XRRModeInfo& mode_info);

private:
    void set_size(int width, int height);
    void get_trans_size(int& width, int& hight);
    SpicePoint get_trans_top_left();
    SpicePoint get_trans_bottom_right();
    bool changed();

    XMonitor* crtc_overlap_test(int x, int y, int width, int height);
    void monitors_cleanup();

private:
    int _min_width;
    int _min_height;
    int _max_width;
    int _max_height;
    int _saved_width;
    int _saved_height;
    int _saved_width_mm;
    int _saved_height_mm;
    XMonitorsList _monitors;
};

#define MAX_TRANS_DEPTH 3

class XMonitor: public Monitor {
public:
    XMonitor(MultyMonScreen& container, int id, RRCrtc crtc);
    virtual ~XMonitor();

    virtual int get_depth();
    virtual SpicePoint get_position();
    virtual SpicePoint get_size() const;
    virtual bool is_out_of_sync();
    virtual int get_screen_id() { return _container.get_screen();}

    void add_clone(XMonitor *clone);
    void revert();
    void disable();
    void enable();

    void set_mode(const XRRModeInfo& mode);
    const SpiceRect& get_prev_area();
    SpiceRect& get_trans_area();
    void pin() { _pin_count++;}
    void unpin() { ASSERT(_pin_count > 0); _pin_count--;}
    bool is_pinned() {return !!_pin_count;}
    void commit_trans_position();
    void set_pusher(XMonitor& pusher) { _pusher = &pusher;}
    XMonitor* get_pusher() { return _pusher;}
    void push_trans();
    void begin_trans();
    bool mode_changed();
    bool position_changed();

    static void inc_change_ref() { Monitor::self_monitors_change++;}
    static void dec_change_ref() { Monitor::self_monitors_change--;}

protected:
    virtual void do_set_mode(int width, int height);
    virtual void do_restore();

private:
    void update_position();
    bool find_mode_in_outputs(RRMode mode, int start_index, XRRScreenResources* res);
    bool find_mode_in_clones(RRMode mode, XRRScreenResources* res);
    XRRModeInfo* find_mode(unsigned int width, unsigned int height, XRRScreenResources* res);

private:
    MultyMonScreen& _container;
    RRCrtc _crtc;
    XMonitorsList _clones;
    SpicePoint _position;
    SpicePoint _size;
    RRMode _mode;
    Rotation _rotation;
    int _noutput;
    RROutput* _outputs;

    SpicePoint _saved_position;
    SpicePoint _saved_size;
    RRMode _saved_mode;
    Rotation _saved_rotation;

    bool _out_of_sync;
    RedScreenRotation _red_rotation;
    RedSubpixelOrder _subpixel_order;

    int _trans_depth;
    SpiceRect _trans_area[MAX_TRANS_DEPTH];
    int _pin_count;
    XMonitor* _pusher;
};

MultyMonScreen::MultyMonScreen(Display* display, int screen, int& next_mon_id)
    : XScreen(display, screen)
    , _saved_width (get_width())
    , _saved_height (get_height())
    , _saved_width_mm (DisplayWidthMM(display, screen))
    , _saved_height_mm (DisplayHeightMM(display, screen))
{
    X_DEBUG_SYNC(get_display());
    Window root_window = RootWindow(display, screen);

    XLockDisplay(display);
    XRRGetScreenSizeRange(display, root_window, &_min_width, &_min_height,
                          &_max_width, &_max_height);
    AutoScreenRes res(XRRGetScreenResources(display, root_window));
    XUnlockDisplay(display);

    if (!res.valid()) {
        THROW("get screen resources failed");
    }

#ifdef SHOW_SCREEN_INFO
    show_scren_info();
#endif
    XLockDisplay(display);
    try {
        for (int i = 0; i < res->ncrtc; i++) {
            AutoCrtcInfo crtc_info(XRRGetCrtcInfo(display, res.get(), res->crtcs[i]));

            if (!crtc_info.valid()) {
                THROW("get crtc info failed");
            }

            if (crtc_info->mode == None) {
                continue;
            }

            ASSERT(crtc_info->noutput);

            XMonitor* clone_mon = crtc_overlap_test(crtc_info->x, crtc_info->y,
                                                    crtc_info->width, crtc_info->height);

            if (clone_mon) {
                clone_mon->add_clone(new XMonitor(*this, next_mon_id++, res->crtcs[i]));
                continue;
            }

            _monitors.push_back(new XMonitor(*this, next_mon_id++, res->crtcs[i]));
        }
        XUnlockDisplay(display);
    } catch (...) {
        XUnlockDisplay(display);
        monitors_cleanup();
        throw;
    }

    if (platform_win != 0)
        return;

    XLockDisplay(display);
    platform_win = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, 1, 1, 0, 0, 0);
    XUnlockDisplay(display);

    LOG_INFO("platform_win: %u", (unsigned int)platform_win);
    intern_clipboard_atoms();
    XSelectInput(display, platform_win, StructureNotifyMask);
    X_DEBUG_SYNC(get_display());
    XRRSelectInput(display, platform_win, RRScreenChangeNotifyMask);
    X_DEBUG_SYNC(get_display());
    if (using_xfixes_1_0) {
        XFixesSelectSelectionInput(display, platform_win, clipboard_prop,
                                   XFixesSetSelectionOwnerNotifyMask|
                                   XFixesSelectionWindowDestroyNotifyMask|
                                   XFixesSelectionClientCloseNotifyMask);
    }

    XMonitor::inc_change_ref();
    process_monitor_configure_events(platform_win);
    XMonitor::dec_change_ref();

    XPlatform::set_win_proc(platform_win, root_win_proc);
    X_DEBUG_SYNC(get_display());
}

MultyMonScreen::~MultyMonScreen()
{
    restore();
    monitors_cleanup();
}

XMonitor* MultyMonScreen::crtc_overlap_test(int x, int y, int width, int height)
{
    XMonitorsList::iterator iter = _monitors.begin();
    for (; iter != _monitors.end(); iter++) {
        XMonitor* mon = *iter;

        SpicePoint pos = mon->get_position();
        SpicePoint size = mon->get_size();

        if (x == pos.x && y == pos.y && width == size.x && height == size.y) {
            return mon;
        }

        if (x < pos.x + size.x && x + width > pos.x && y < pos.y + size.y && y + height > pos.y) {
            THROW("unsupported partial overlap");
        }
    }
    return NULL;
}

void MultyMonScreen::publish_monitors(MonitorsList& monitors)
{
    XMonitorsList::iterator iter = _monitors.begin();
    for (; iter != _monitors.end(); iter++) {
        monitors.push_back(*iter);
    }
}

void MultyMonScreen::monitors_cleanup()
{
    while (!_monitors.empty()) {
        XMonitor* monitor = _monitors.front();
        _monitors.pop_front();
        delete monitor;
    }
}

void MultyMonScreen::disable()
{
    XMonitorsList::iterator iter = _monitors.begin();
    for (; iter != _monitors.end(); iter++) {
        (*iter)->disable();
    }
}

void MultyMonScreen::enable()
{
    XMonitorsList::iterator iter = _monitors.begin();
    for (; iter != _monitors.end(); iter++) {
        (*iter)->enable();
    }
}

void MultyMonScreen::set_size(int width, int height)
{
    X_DEBUG_SYNC(get_display());
    Window root_window = RootWindow(get_display(), get_screen());
    set_width(width);
    int width_mm = (int)((double)_saved_width_mm / _saved_width * width);
    set_height(height);
    int height_mm = (int)((double)_saved_height_mm / _saved_height * height);
    XLockDisplay(get_display());
    XRRSetScreenSize(get_display(), root_window, width, height, width_mm, height_mm);
    XUnlockDisplay(get_display());
    X_DEBUG_SYNC(get_display());
}

bool MultyMonScreen::changed()
{
    if (get_width() != _saved_width || get_height() != _saved_height) {
        return true;
    }

    XMonitorsList::iterator iter = _monitors.begin();
    for (; iter != _monitors.end(); iter++) {
        if ((*iter)->mode_changed() || (*iter)->position_changed()) {
            return true;
        }
    }
    return false;
}

void MultyMonScreen::restore()
{
    if (is_broken() || !changed()) {
        return;
    }
    X_DEBUG_SYNC(get_display());
    XMonitor::inc_change_ref();
    disable();
    set_size(_saved_width, _saved_height);
    XMonitorsList::iterator iter = _monitors.begin();
    for (; iter != _monitors.end(); iter++) {
        (*iter)->revert();
    }
    enable();
    process_monitor_configure_events(platform_win);
    XMonitor::dec_change_ref();
    X_DEBUG_SYNC(get_display());
}

SpicePoint MultyMonScreen::get_trans_top_left()
{
    SpicePoint position;
    position.y = position.x = MAXINT;

    XMonitorsList::iterator iter = _monitors.begin();
    for (; iter != _monitors.end(); iter++) {
        SpiceRect& area = (*iter)->get_trans_area();
        position.x = MIN(position.x, area.left);
        position.y = MIN(position.y, area.top);
    }
    return position;
}

SpicePoint MultyMonScreen::get_trans_bottom_right()
{
    SpicePoint position;
    position.y = position.x = MININT;

    XMonitorsList::iterator iter = _monitors.begin();
    for (; iter != _monitors.end(); iter++) {
        SpiceRect& area = (*iter)->get_trans_area();
        position.x = MAX(position.x, area.right);
        position.y = MAX(position.y, area.bottom);
    }
    return position;
}

void MultyMonScreen::get_trans_size(int& width, int& height)
{
    ASSERT(get_trans_top_left().x == 0 && get_trans_top_left().y == 0);
    SpicePoint bottom_right = get_trans_bottom_right();
    ASSERT(bottom_right.x > 0 && bottom_right.y > 0);
    width = bottom_right.x;
    height = bottom_right.y;
}

#endif

/*class Variant {
    static void get_area_in_front(const SpiceRect& base, int size, SpiceRect& area)
    static int get_push_distance(const SpiceRect& fix_area, const SpiceRect& other)
    static int get_head(const SpiceRect& area)
    static int get_tail(const SpiceRect& area)
    static void move_head(SpiceRect& area, int delta)
    static int get_pull_distance(const SpiceRect& fix_area, const SpiceRect& other)
    static void offset(SpiceRect& area, int delta)
    static void shrink(SpiceRect& area, int delta)
    static int get_distance(const SpiceRect& area, const SpiceRect& other_area)
    static bool is_on_tail(const SpiceRect& area, const SpiceRect& other_area)
    static bool is_on_perpendiculars(const SpiceRect& area, const SpiceRect& other_area)
}*/

#ifdef USE_XRANDR_1_2

class SortRightToLeft {
public:
    bool operator () (XMonitor* mon1, XMonitor* mon2) const
    {
        return mon1->get_trans_area().right > mon2->get_trans_area().right;
    }
};

typedef std::multiset<XMonitor*, SortRightToLeft> PushLeftSet;

class LeftVariant {
public:

    static void get_area_in_front(const SpiceRect& base, int size, SpiceRect& area)
    {
        area.right = base.left;
        area.left = area.right - size;
        area.bottom = base.bottom;
        area.top = base.top;
    }

    static int get_push_distance(const SpiceRect& fix_area, const SpiceRect& other)
    {
        return other.right - fix_area.left;
    }

    static int get_head(const SpiceRect& area)
    {
        return area.left;
    }

    static int get_tail(const SpiceRect& area)
    {
        return area.right;
    }

    static void move_head(SpiceRect& area, int delta)
    {
        area.left -= delta;
        ASSERT(area.right >= area.left);
    }

    static int get_pull_distance(const SpiceRect& fix_area, const SpiceRect& other)
    {
        return other.left - fix_area.right;
    }

    static void offset(SpiceRect& area, int delta)
    {
        rect_offset(area, -delta, 0);
    }

    static void shrink(SpiceRect& area, int delta)
    {
        area.right -= delta;
        ASSERT(area.right > area.left);
    }

    static int get_distance(const SpiceRect& area, const SpiceRect& other_area)
    {
        return other_area.left - area.left;
    }

    static bool is_on_tail(const SpiceRect& area, const SpiceRect& other_area)
    {
        return area.right == other_area.left && other_area.top < area.bottom &&
               other_area.bottom > area.top;
    }

    static bool is_on_perpendiculars(const SpiceRect& area, const SpiceRect& other_area)
    {
        return (other_area.bottom == area.top || other_area.top == area.bottom) &&
               other_area.left < area.right && other_area.right > area.left;
    }
};

class SortLeftToRight {
public:
    bool operator () (XMonitor* mon1, XMonitor* mon2) const
    {
        return mon1->get_trans_area().left < mon2->get_trans_area().left;
    }
};

typedef std::multiset<XMonitor*, SortLeftToRight> PushRightSet;

class RightVariant {
public:

    static void get_area_in_front(const SpiceRect& base, int size, SpiceRect& area)
    {
        area.left = base.right;
        area.right = area.left + size;
        area.top = base.top;
        area.bottom = base.bottom;
    }

    static int get_push_distance(const SpiceRect& fix_area, const SpiceRect& other)
    {
        return fix_area.right - other.left;
    }

    static int get_head(const SpiceRect& area)
    {
        return area.right;
    }

    static int get_tail(const SpiceRect& area)
    {
        return area.left;
    }

    static void move_head(SpiceRect& area, int delta)
    {
        area.right += delta;
        ASSERT(area.right >= area.left);
    }

    static int get_pull_distance(const SpiceRect& fix_area, const SpiceRect& other)
    {
        return fix_area.left - other.right;
    }

    static void offset(SpiceRect& area, int delta)
    {
        rect_offset(area, delta, 0);
    }

    static bool is_on_tail(const SpiceRect& area, const SpiceRect& other_area)
    {
        return other_area.right == area.left && other_area.top < area.bottom &&
               other_area.bottom > area.top;
    }

    static bool is_on_perpendiculars(const SpiceRect& area, const SpiceRect& other_area)
    {
        return (other_area.bottom == area.top || other_area.top == area.bottom) &&
               other_area.left < area.right && other_area.right > area.left;
    }
};

class SortBottomToTop {
public:
    bool operator () (XMonitor* mon1, XMonitor* mon2) const
    {
        return mon1->get_trans_area().bottom > mon2->get_trans_area().bottom;
    }
};

typedef std::multiset<XMonitor*, SortBottomToTop> PushTopSet;

class TopVariant {
public:
    static void get_area_in_front(const SpiceRect& base, int size, SpiceRect& area)
    {
        area.left = base.left;
        area.right = base.right;
        area.bottom = base.top;
        area.top = area.bottom - size;
    }

    static int get_push_distance(const SpiceRect& fix_area, const SpiceRect& other)
    {
        return other.bottom - fix_area.top;
    }

    static int get_head(const SpiceRect& area)
    {
        return area.top;
    }

    static int get_tail(const SpiceRect& area)
    {
        return area.bottom;
    }

    static void move_head(SpiceRect& area, int delta)
    {
        area.top -= delta;
        ASSERT(area.bottom >= area.top);
    }

    static int get_pull_distance(const SpiceRect& fix_area, const SpiceRect& other)
    {
        return other.top - fix_area.bottom;
    }

    static void offset(SpiceRect& area, int delta)
    {
        rect_offset(area, 0, -delta);
    }

    static void shrink(SpiceRect& area, int delta)
    {
        area.bottom -= delta;
        ASSERT(area.bottom > area.top);
    }

    static int get_distance(const SpiceRect& area, const SpiceRect& other_area)
    {
        return other_area.top - area.top;
    }

    static bool is_on_tail(const SpiceRect& area, const SpiceRect& other_area)
    {
        return area.bottom == other_area.top && other_area.left < area.right &&
               other_area.right > area.left;
    }

    static bool is_on_perpendiculars(const SpiceRect& area, const SpiceRect& other_area)
    {
        return (other_area.right == area.left || other_area.left == area.right) &&
               other_area.top < area.bottom && other_area.bottom > area.top;
    }
};

class SortTopToBottom {
public:
    bool operator () (XMonitor* mon1, XMonitor* mon2) const
    {
        return mon1->get_trans_area().top < mon2->get_trans_area().top;
    }
};

typedef std::multiset<XMonitor*, SortTopToBottom> PushBottomSet;

class BottomVariant {
public:

    static void get_area_in_front(const SpiceRect& base, int size, SpiceRect& area)
    {
        area.left = base.left;
        area.right = base.right;
        area.top = base.bottom;
        area.bottom = area.top + size;
    }

    static int get_push_distance(const SpiceRect& fix_area, const SpiceRect& other)
    {
        return fix_area.bottom - other.top;
    }

    static int get_head(const SpiceRect& area)
    {
        return area.bottom;
    }

    static int get_tail(const SpiceRect& area)
    {
        return area.top;
    }

    static void move_head(SpiceRect& area, int delta)
    {
        area.bottom += delta;
        ASSERT(area.bottom >= area.top);
    }

    static int get_pull_distance(const SpiceRect& fix_area, const SpiceRect& other)
    {
        return fix_area.top - other.bottom;
    }

    static void offset(SpiceRect& area, int delta)
    {
        rect_offset(area, 0, delta);
    }

    static bool is_on_tail(const SpiceRect& area, const SpiceRect& other_area)
    {
        return other_area.bottom == area.top && other_area.left < area.right &&
               other_area.right > area.left;
    }

    static bool is_on_perpendiculars(const SpiceRect& area, const SpiceRect& other_area)
    {
        return (other_area.right == area.left || other_area.left == area.right) &&
               other_area.top < area.bottom && other_area.bottom > area.top;
    }
};

volatile int wait_for_me = false;

template <class Variant>
static void bounce_back(XMonitor& monitor, const XMonitorsList& monitors, int head, int distance)
{
    ASSERT(distance > 0);
    while (wait_for_me);

    for (XMonitorsList::const_iterator iter = monitors.begin(); iter != monitors.end(); iter++) {
        SpiceRect& area = (*iter)->get_trans_area();
        if (Variant::get_tail(area) == head && (*iter)->get_pusher() == &monitor) {
            Variant::offset(area, -distance);
            bounce_back<Variant>(**iter, monitors, Variant::get_head(area) + distance, distance);
            //todo: pull_back monitors on perpendiculars
        }
    }
}

template <class Variant, class SortList, class SortListIter>
static int push(XMonitor& pusher, XMonitor& monitor, const XMonitorsList& monitors, int delta)
{
    monitor.pin();
    monitor.set_pusher(pusher);

    SortList sort;
    XMonitorsList::const_iterator iter = monitors.begin();
    for (; iter != monitors.end(); iter++) {
        if (*iter == &monitor) {
            continue;
        }
        sort.insert(*iter);
    }

    SpiceRect area_to_clear;
    Variant::get_area_in_front(monitor.get_trans_area(), delta, area_to_clear);

    SortListIter sort_iter = sort.begin();

    for (; sort_iter != sort.end(); sort_iter++) {
        const SpiceRect& other_area = (*sort_iter)->get_trans_area();

        if (rect_intersects(area_to_clear, other_area)) {
            int distance = Variant::get_push_distance(area_to_clear, other_area);
            ASSERT(distance > 0);
            if (!(*sort_iter)->is_pinned()) {
                distance = distance - push<Variant, SortList, SortListIter>(monitor, **sort_iter,
                                                                            monitors, distance);
            }

            if (distance) {
                delta -= distance;
                bounce_back<Variant>(monitor, monitors, Variant::get_head(area_to_clear), distance);
                Variant::move_head(area_to_clear, -distance);
            }
        }
    }
    Variant::offset(monitor.get_trans_area(), delta);

    const SpiceRect& area = monitor.get_prev_area();
    for (iter = monitors.begin(); iter != monitors.end(); iter++) {
        if ((*iter)->is_pinned()) {
            continue;
        }

        const SpiceRect& other_area = (*iter)->get_prev_area();
        if (Variant::is_on_perpendiculars(area, other_area)) {
            int current_distance = Variant::get_pull_distance(monitor.get_trans_area(),
                                                              (*iter)->get_trans_area());
            int base_distance = Variant::get_pull_distance(area, other_area);
            int distance = current_distance - base_distance;
            if (distance > 0) {
                push<Variant, SortList, SortListIter>(monitor, **iter, monitors, distance);
            }
        } else if (Variant::is_on_tail(area, other_area)) {
            int distance = Variant::get_pull_distance(monitor.get_trans_area(),
                                                      (*iter)->get_trans_area());
            ASSERT(distance >= 0);
            push<Variant, SortList, SortListIter>(monitor, **iter, monitors, distance);
        }
    }
    return delta;
}

template <class Variant>
static void pin(XMonitor& monitor, const XMonitorsList& monitors)
{
    const SpiceRect& area = monitor.get_trans_area();

    for (XMonitorsList::const_iterator iter = monitors.begin(); iter != monitors.end(); iter++) {
        const SpiceRect& other_area = (*iter)->get_trans_area();
        if ((*iter)->is_pinned()) {
            continue;
        }
        if (Variant::is_on_tail(other_area, area) ||
                                                  Variant::is_on_perpendiculars(area, other_area)) {
            (*iter)->pin();
            pin<Variant>(**iter, monitors);
        }
    }
}

template <class Variant, class SortList, class SortListIter>
static void shrink(XMonitor& monitor, const XMonitorsList& monitors, int delta)
{
    monitor.pin();
    pin<Variant>(monitor, monitors);
    ASSERT(delta > 0);

    SortList sort;
    XMonitorsList::const_iterator iter = monitors.begin();
    for (; iter != monitors.end(); iter++) {
        if (*iter == &monitor) {
            continue;
        }
        sort.insert(*iter);
    }

    const SpiceRect area = monitor.get_trans_area();
    Variant::shrink(monitor.get_trans_area(), delta);
    for (SortListIter sort_iter = sort.begin(); sort_iter != sort.end(); sort_iter++) {
        const SpiceRect& other_area = (*sort_iter)->get_trans_area();
        if (Variant::is_on_perpendiculars(area, other_area)) {
            int distance = Variant::get_distance(area, other_area);
            if (distance > 0) {
                distance = MIN(distance, delta);
                push<Variant, SortList, SortListIter>(monitor, **sort_iter, monitors, distance);
            }
        } else if (Variant::is_on_tail(area, other_area)) {
            push<Variant, SortList, SortListIter>(monitor, **sort_iter, monitors, delta);
        }
    }
}

template <class Variant, class SortList, class SortListIter>
static void expand(XMonitor& monitor, const XMonitorsList& monitors, int delta)
{
    monitor.pin();
    ASSERT(delta > 0);

    SortList sort;
    XMonitorsList::const_iterator iter = monitors.begin();
    for (; iter != monitors.end(); iter++) {
        if (*iter == &monitor) {
            continue;
        }
        sort.insert(*iter);
    }

    SpiceRect area_to_clear;
    Variant::get_area_in_front(monitor.get_trans_area(), delta, area_to_clear);

    for (SortListIter sort_iter = sort.begin(); sort_iter != sort.end(); sort_iter++) {
        const SpiceRect& other_area = (*sort_iter)->get_trans_area();

        if (rect_intersects(area_to_clear, other_area)) {
            int distance = Variant::get_push_distance(area_to_clear, other_area);
            ASSERT(distance > 0);
            ASSERT(!(*sort_iter)->is_pinned());
#ifdef RED_DEBUG
            int actual =
#endif
            push<Variant, SortList, SortListIter>(monitor, **sort_iter, monitors, distance);
            ASSERT(actual == distance);
        }
    }
    Variant::move_head(monitor.get_trans_area(), delta);
}

bool MultyMonScreen::set_monitor_mode(XMonitor& monitor, const XRRModeInfo& mode_info)
{
    if (is_broken()) {
        return false;
    }

    SpicePoint size = monitor.get_size();
    int dx = mode_info.width - size.x;
    int dy = mode_info.height - size.y;

    XMonitorsList::iterator iter = _monitors.begin();

    for (; iter != _monitors.end(); iter++) {
        (*iter)->begin_trans();
    }

    if (dx > 0) {
        expand<RightVariant, PushRightSet, PushRightSet::iterator>(monitor, _monitors, dx);
    } else if (dx < 0) {
        shrink<LeftVariant, PushLeftSet, PushLeftSet::iterator>(monitor, _monitors, -dx);
    }

    for (iter = _monitors.begin(); iter != _monitors.end(); iter++) {
        (*iter)->push_trans();
    }

    if (dy > 0) {
        expand<BottomVariant, PushBottomSet, PushBottomSet::iterator>(monitor, _monitors, dy);
    } else if (dy < 0) {
        shrink<TopVariant, PushTopSet, PushTopSet::iterator>(monitor, _monitors, -dy);
    }

    int screen_width;
    int screen_height;

    get_trans_size(screen_width, screen_height);

    if (screen_width > _max_width || screen_height > _max_height) {
        return false;
    }

    screen_width = MAX(screen_width, _min_width);
    screen_height = MAX(screen_height, _min_height);

    XMonitor::inc_change_ref();
    disable();
    for (iter = _monitors.begin(); iter != _monitors.end(); iter++) {
        (*iter)->commit_trans_position();
    }
    X_DEBUG_SYNC(get_display());
    monitor.set_mode(mode_info);
    set_size(screen_width, screen_height);
    enable();
    process_monitor_configure_events(platform_win);
    XMonitor::dec_change_ref();
    X_DEBUG_SYNC(get_display());
    return true;
}

XMonitor::XMonitor(MultyMonScreen& container, int id, RRCrtc crtc)
    : Monitor(id)
    , _container (container)
    , _crtc (crtc)
    , _out_of_sync (false)
{
    update_position();
    _saved_position = _position;
    _saved_size = _size;
    _saved_mode = _mode;
    _saved_rotation = _rotation;
}

XMonitor::~XMonitor()
{
    while (!_clones.empty()) {
        XMonitor* clone = _clones.front();
        _clones.pop_front();
        delete clone;
    }
    delete[] _outputs;
}

void XMonitor::update_position()
{
    Display* display = _container.get_display();
    X_DEBUG_SYNC(display);
    Window root_window = RootWindow(display, _container.get_screen());

    XLockDisplay(display);
    AutoScreenRes res(XRRGetScreenResources(display, root_window));
    XUnlockDisplay(display);

    if (!res.valid()) {
        THROW("get screen resources failed");
    }

    XLockDisplay(display);
    AutoCrtcInfo crtc_info(XRRGetCrtcInfo(display, res.get(), _crtc));
    XUnlockDisplay(display);

    ASSERT(crtc_info->noutput);

    _position.x = crtc_info->x;
    _position.y = crtc_info->y;
    _size.x = crtc_info->width;
    _size.y = crtc_info->height;

    switch (crtc_info->rotation & 0xf) {
    case RR_Rotate_0:
        _red_rotation = RED_SCREEN_ROTATION_0;
        break;
    case RR_Rotate_90:
        _red_rotation = RED_SCREEN_ROTATION_90;
        break;
    case RR_Rotate_180:
        _red_rotation = RED_SCREEN_ROTATION_180;
        break;
    case RR_Rotate_270:
        _red_rotation = RED_SCREEN_ROTATION_270;
        break;
    default:
        THROW("invalid rotation");
    }

    if (crtc_info->noutput > 1) {
        //todo: set valid subpixel order in case all outputs share the same type
        _subpixel_order = RED_SUBPIXEL_ORDER_UNKNOWN;
    } else {
        XLockDisplay(display);
        AutoOutputInfo output_info(XRRGetOutputInfo(display, res.get(), crtc_info->outputs[0]));
        XUnlockDisplay(display);

        switch (output_info->subpixel_order) {
        case SubPixelUnknown:
            _subpixel_order = RED_SUBPIXEL_ORDER_UNKNOWN;
        break;
        case SubPixelHorizontalRGB:
            _subpixel_order = RED_SUBPIXEL_ORDER_H_RGB;
            break;
        case SubPixelHorizontalBGR:
            _subpixel_order = RED_SUBPIXEL_ORDER_H_BGR;
            break;
        case SubPixelVerticalRGB:
            _subpixel_order = RED_SUBPIXEL_ORDER_V_RGB;
            break;
        case SubPixelVerticalBGR:
            _subpixel_order = RED_SUBPIXEL_ORDER_V_BGR;
            break;
        case SubPixelNone:
            _subpixel_order = RED_SUBPIXEL_ORDER_NONE;
            break;
        default:
            THROW("invalid subpixel order");
        }
    }

    _mode = crtc_info->mode;
    _rotation = crtc_info->rotation;
    _noutput = crtc_info->noutput;
    _outputs = new RROutput[_noutput];
    memcpy(_outputs, crtc_info->outputs, _noutput * sizeof(RROutput));
    X_DEBUG_SYNC(display);
}

bool XMonitor::find_mode_in_outputs(RRMode mode, int start_index, XRRScreenResources* res)
{
    int i, j;
    bool retval = true;

    X_DEBUG_SYNC(_container.get_display());
    XLockDisplay(_container.get_display());
    for (i = start_index; i < _noutput; i++) {
        AutoOutputInfo output_info(XRRGetOutputInfo(_container.get_display(), res, _outputs[i]));
        for (j = 0; j < output_info->nmode; j++) {
            if (output_info->modes[j] == mode) {
                break;
            }
        }
        if (j == output_info->nmode) {
            retval = false;
            break;
        }
    }
    XUnlockDisplay(_container.get_display());
    X_DEBUG_SYNC(_container.get_display());
    return retval;
}

bool XMonitor::find_mode_in_clones(RRMode mode, XRRScreenResources* res)
{
    XMonitorsList::iterator iter = _clones.begin();
    for (; iter != _clones.end(); iter++) {
        if (!(*iter)->find_mode_in_outputs(mode, 0, res)) {
            return false;
        }
    }
    return true;
}

class ModeInfo {
public:
    ModeInfo(int int_index, XRRModeInfo* in_info) : index (int_index), info (in_info) {}

    int index;
    XRRModeInfo* info;
};

class ModeCompare {
public:
    bool operator () (const ModeInfo& mode1, const ModeInfo& mode2) const
    {
        int area1 = mode1.info->width * mode1.info->height;
        int area2 = mode2.info->width * mode2.info->height;
        return area1 < area2 || (area1 == area2 && mode1.index < mode2.index);
    }
};

XRRModeInfo* XMonitor::find_mode(unsigned int width, unsigned int height, XRRScreenResources* res)
{
    typedef std::set<ModeInfo, ModeCompare> ModesSet;
    ModesSet modes_set;
    X_DEBUG_SYNC(_container.get_display());

    XLockDisplay(_container.get_display());
    AutoOutputInfo output_info(XRRGetOutputInfo(_container.get_display(), res, _outputs[0]));
    XUnlockDisplay(_container.get_display());

    for (int i = 0; i < output_info->nmode; i++) {
        XRRModeInfo* mode_inf = find_mod(res, output_info->modes[i]);
        if (mode_inf->width >= width && mode_inf->height >= height) {
            modes_set.insert(ModeInfo(i, mode_inf));
        }
    }

    while (!modes_set.empty()) {
        ModesSet::iterator iter = modes_set.begin();

        if (!find_mode_in_outputs((*iter).info->id, 1, res)) {
            modes_set.erase(iter);
            continue;
        }

        if (!find_mode_in_clones((*iter).info->id, res)) {
            modes_set.erase(iter);
            continue;
        }
        return (*iter).info;
    }
    X_DEBUG_SYNC(_container.get_display());
    return NULL;
}

void XMonitor::do_set_mode(int width, int height)
{
    if (width == _size.x && height == _size.y) {
        _out_of_sync = false;
        return;
    }
    Display* display = _container.get_display();
    X_DEBUG_SYNC(display);
    Window root_window = RootWindow(display, _container.get_screen());

    XLockDisplay(display);
    AutoScreenRes res(XRRGetScreenResources(display, root_window));
    XUnlockDisplay(display);

    if (!res.valid()) {
        THROW("get screen resource failed");
    }
    XRRModeInfo* mode_info = find_mode(width, height, res.get());

    if (!mode_info || !_container.set_monitor_mode(*this, *mode_info)) {
        _out_of_sync = true;
        X_DEBUG_SYNC(display);
        return;
    }
    _out_of_sync = false;
}

void XMonitor::revert()
{
    _position = _saved_position;
    _size = _saved_size;
    _mode = _saved_mode;
    _rotation = _saved_rotation;
    XMonitorsList::iterator iter = _clones.begin();
    for (; iter != _clones.end(); iter++) {
        (*iter)->revert();
    }
}

void XMonitor::disable()
{
    Display* display = _container.get_display();
    X_DEBUG_SYNC(display);
    Window root_window = RootWindow(display, _container.get_screen());

    XLockDisplay(display);
    AutoScreenRes res(XRRGetScreenResources(display, root_window));
    XUnlockDisplay(display);

    if (!res.valid()) {
        THROW("get screen resources failed");
    }
    XLockDisplay(display);
    XRRSetCrtcConfig(display, res.get(), _crtc, CurrentTime,
                     0, 0, None, RR_Rotate_0, NULL, 0);
    XUnlockDisplay(display);

    XMonitorsList::iterator iter = _clones.begin();
    for (; iter != _clones.end(); iter++) {
        (*iter)->disable();
    }
    XFlush(x_display);
    X_DEBUG_SYNC(display);
}

void XMonitor::enable()
{
    Display* display = _container.get_display();
    X_DEBUG_SYNC(display);
    Window root_window = RootWindow(display, _container.get_screen());

    XLockDisplay(display);
    AutoScreenRes res(XRRGetScreenResources(display, root_window));
    XUnlockDisplay(display);

    if (!res.valid()) {
        THROW("get screen resources failed");
    }
    XLockDisplay(display);
    XRRSetCrtcConfig(display, res.get(), _crtc, CurrentTime,
                     _position.x, _position.y,
                     _mode, _rotation,
                     _outputs, _noutput);
    XUnlockDisplay(display);

    XMonitorsList::iterator iter = _clones.begin();
    for (; iter != _clones.end(); iter++) {
        (*iter)->enable();
    }
    XFlush(x_display);
    X_DEBUG_SYNC(display);
}

bool XMonitor::mode_changed()
{
    return _size.x != _saved_size.x || _size.y != _saved_size.y ||
           _mode != _saved_mode || _rotation != _saved_rotation;
}

bool XMonitor::position_changed()
{
    return _position.x != _saved_position.x || _position.y != _saved_position.y;
}

void XMonitor::do_restore()
{
    if (!mode_changed()) {
        return;
    }
    _container.restore();
}

int XMonitor::get_depth()
{
    return XPlatform::get_vinfo()[0]->depth;
}

SpicePoint XMonitor::get_position()
{
    return _position;
}

SpicePoint XMonitor::get_size() const
{
    return _size;
}

bool XMonitor::is_out_of_sync()
{
    return _out_of_sync;
}

void XMonitor::add_clone(XMonitor *clone)
{
    _clones.push_back(clone);
}

const SpiceRect& XMonitor::get_prev_area()
{
    return _trans_area[_trans_depth - 1];
}

SpiceRect& XMonitor::get_trans_area()
{
    return _trans_area[_trans_depth];
}

void XMonitor::push_trans()
{
    _trans_depth++;
    ASSERT(_trans_depth < MAX_TRANS_DEPTH);
    _trans_area[_trans_depth] = _trans_area[_trans_depth - 1];
    _pin_count = 0;
    _pusher = NULL;
}

void XMonitor::begin_trans()
{
    _trans_area[0].left = _position.x;
    _trans_area[0].right = _trans_area[0].left + _size.x;
    _trans_area[0].top = _position.y;
    _trans_area[0].bottom = _trans_area[0].top + _size.y;
    _trans_area[1] = _trans_area[0];
    _trans_depth = 1;
    _pin_count = 0;
    _pusher = NULL;
}

void XMonitor::commit_trans_position()
{
    _position.x = _trans_area[_trans_depth].left;
    _position.y = _trans_area[_trans_depth].top;
    XMonitorsList::iterator iter = _clones.begin();
    for (; iter != _clones.end(); iter++) {
        (*iter)->_position = _position;
    }
}

void XMonitor::set_mode(const XRRModeInfo& mode)
{
    _mode = mode.id;
    _size.x = mode.width;
    _size.y = mode.height;
    XMonitorsList::iterator iter = _clones.begin();
    for (; iter != _clones.end(); iter++) {
        (*iter)->set_mode(mode);
    }
}

#endif

#ifdef USE_XINERAMA_1_0

static XineramaScreenInfo* init_xinerama_screens(int* num_xin_screens)
{
    XineramaScreenInfo* xin_screens = NULL;

    if (using_xinerama_1_0 && ScreenCount(x_display) == 1) {
        int ncrtc = 0;
#ifdef USE_XRANDR_1_2
        if (using_xrandr_1_2) {
            AutoScreenRes res(XRRGetScreenResources(x_display, RootWindow(x_display, 0)));
            if (res.valid()) {
                ncrtc = res->ncrtc;
            }
        }
#endif
        if (ncrtc < 2) {
            xin_screens = XineramaQueryScreens(x_display, num_xin_screens);
        }
    }
    if (xin_screens && *num_xin_screens < 2) {
        XFree(xin_screens);
        return NULL;
    }
    return xin_screens;
}

#endif

static MonitorsList monitors;
static Monitor* primary_monitor = NULL;

typedef std::list<XScreen*> ScreenList;
static ScreenList screens;

const MonitorsList& Platform::init_monitors()
{
    int next_mon_id = 0;
    ASSERT(screens.empty());

#ifdef USE_XINERAMA_1_0
    int num_xin_screens;
    XineramaScreenInfo* xin_screens = init_xinerama_screens(&num_xin_screens);
    if (xin_screens) {
        screens.push_back(new XineramaScreen(x_display, 0, next_mon_id, xin_screens, num_xin_screens));
        XFree(xin_screens);
    } else
#endif
#ifdef USE_XRANDR_1_2
    if (using_xrandr_1_2) {
        for (int i = 0; i < ScreenCount(x_display); i++) {
            screens.push_back(new MultyMonScreen(x_display, i, next_mon_id));
        }
    } else
#endif
    if (using_xrandr_1_0) {
        for (int i = 0; i < ScreenCount(x_display); i++) {
            screens.push_back(new DynamicScreen(x_display, i, next_mon_id));
        }
    } else {
        for (int i = 0; i < ScreenCount(x_display); i++) {
            screens.push_back(new StaticScreen(x_display, i, next_mon_id));
        }
    }

    ASSERT(monitors.empty());
    ScreenList::iterator iter = screens.begin();
    for (; iter != screens.end(); iter++) {
        (*iter)->publish_monitors(monitors);
    }
    MonitorsList::iterator mon_iter = monitors.begin();
    for (; mon_iter != monitors.end(); mon_iter++) {
        Monitor *mon = *mon_iter;
        if (mon->get_id() == 0) {
            primary_monitor = mon;
            break;
        }
    }
    return monitors;
}

void Platform::destroy_monitors()
{
    primary_monitor = NULL;
    monitors.clear();
    while (!screens.empty()) {
        XScreen* screen = screens.front();
        screens.pop_front();
        delete screen;
    }
}

bool Platform::is_monitors_pos_valid()
{
    return (ScreenCount(x_display) == 1);
}

void Platform::get_app_data_dir(std::string& path, const std::string& app_name)
{
    const char* home_dir = getenv("HOME");

    if (!home_dir || strlen(home_dir) == 0) {
        throw Exception("get home dir failed");
    }

    path = home_dir;
    std::string::iterator end = path.end();

    while (end != path.begin() && *(end - 1) == '/') {
        path.erase(--end);
    }

    path += "/.";
    path += app_name;

    if (mkdir(path.c_str(), 0700) == -1 && errno != EEXIST) {
        throw Exception("create appdata dir failed");
    }
}

void Platform::path_append(std::string& path, const std::string& partial_path)
{
    path += "/";
    path += partial_path;
}

static void ensure_clipboard_data_space(uint32_t size)
{
    if (size > clipboard_data_space) {
        free(clipboard_data);
        clipboard_data = (uint8_t *)malloc(size);
        assert(clipboard_data);
        clipboard_data_space = size;
    }
}

static void send_selection_notify(Atom prop, int process_next_req)
{
    XEvent res, *event = &next_selection_request->event;
    selection_request *old_request;

    res.xselection.property = prop;
    res.xselection.type = SelectionNotify;
    res.xselection.display = event->xselectionrequest.display;
    res.xselection.requestor = event->xselectionrequest.requestor;
    res.xselection.selection = event->xselectionrequest.selection;
    res.xselection.target = event->xselectionrequest.target;
    res.xselection.time = event->xselectionrequest.time;
    XSendEvent(x_display, event->xselectionrequest.requestor, 0, 0, &res);
    XFlush(x_display);

    old_request = next_selection_request;
    next_selection_request = next_selection_request->next;
    delete old_request;

    if (process_next_req)
        handle_selection_request();
}

static void print_targets(const char *action, Atom *atoms, int c)
{
    int i;

    LOG_INFO("%s %d targets:", action, c);
    for (i = 0; i < c; i++)
        LOG_INFO("%s", atom_name(atoms[i]));
}

static void send_targets(XEvent& request_event)
{
    Atom targets[256] = { targets_atom, };
    int i, j, k, target_count = 1;

    for (i = 0; i < clipboard_type_count; i++) {
        for (j = 0; j < clipboard_format_count; j++) {
            if (clipboard_formats[j].type != clipboard_agent_types[i]) {
                continue;
            }
            for (k = 0; k < clipboard_formats[j].atom_count; k++) {
                targets[target_count] = clipboard_formats[j].atoms[k];
                target_count++;
                if (target_count == sizeof(targets)/sizeof(Atom)) {
                    LOG_WARN("sendtargets: too many targets");
                    goto exit_loop;
                }
            }
        }
    }
exit_loop:

    Window requestor_win = request_event.xselectionrequest.requestor;
    Atom prop = request_event.xselectionrequest.property;
    if (prop == None)
        prop = request_event.xselectionrequest.target;

    XChangeProperty(x_display, requestor_win, prop, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&targets,
                    target_count);
    print_targets("sent", targets, target_count);
    send_selection_notify(prop, 1);
}

static int get_selection(XEvent &event, Atom type, Atom prop, int format,
                         unsigned char **data_ret, bool incr)
{
    Bool del = incr ? True: False;
    Atom type_ret;
    int res, format_ret, ret_val = -1;
    unsigned long len, remain;
    unsigned char *data = NULL;

    if (incr) {
        if (event.xproperty.atom != prop) {
            LOG_WARN("PropertyNotify parameters mismatch");
            goto exit;
        }
    } else {
        if (event.xselection.property == None) {
            LOG_INFO("XConvertSelection refused by clipboard owner");
            goto exit;
        }

        if (event.xselection.requestor != platform_win ||
            event.xselection.selection != clipboard_prop ||
            event.xselection.property  != prop) {
            LOG_WARN("SelectionNotify parameters mismatch");
            goto exit;
        }
    }

    /* Warning we are running with the clipboard_lock held!! That is ok, as
       there is no code holding XLockDisplay which calls code taking
       the clipboard_lock!!  */
    XLockDisplay(x_display);
    res = XGetWindowProperty(x_display, platform_win, prop, 0,
                             LONG_MAX, del, type, &type_ret, &format_ret, &len,
                             &remain, &data);
    XUnlockDisplay(x_display);
    if (res != Success) {
        LOG_WARN("XGetWindowProperty failed");
        goto exit;
    }

    if (!incr) {
        if (type_ret == incr_atom) {
            if (waiting_for_property_notify) {
                LOG_WARN("received an incr property notify while still reading another incr property");
                goto exit;
            }
            XSelectInput(x_display, platform_win, PropertyChangeMask);
            XDeleteProperty(x_display, platform_win, prop);
            XFlush(x_display);
            waiting_for_property_notify = true;
            ensure_clipboard_data_space(*(uint32_t*)data);
            XFree(data);
            return 0; /* Wait for more data */
        }
        XDeleteProperty(x_display, platform_win, prop);
        XFlush(x_display);
    }

    if (type_ret != type) {
        LOG_WARN("expected property type: %s, got: %s", atom_name(type),
                 atom_name(type_ret));
        goto exit;
    }

    if (format_ret != format) {
        LOG_WARN("expected %d bit format, got %d bits", format, format_ret);
        goto exit;
    }

    /* Convert len to bytes */
    switch(format) {
    case 8:
        break;
    case 16:
        len *= sizeof(short);
        break;
    case 32:
        len *= sizeof(long);
        break;
    }

    if (incr) {
        if (len) {
            if (clipboard_data_size + len > clipboard_data_space) {
                clipboard_data_space = clipboard_data_size + len;
                clipboard_data = (uint8_t *)realloc(clipboard_data, clipboard_data_space);
                assert(clipboard_data);
            }
            memcpy(clipboard_data + clipboard_data_size, data, len);
            clipboard_data_size += len;
            LOG_INFO("Appended %d bytes to buffer", len);
            XFree(data);
            return 0; /* Wait for more data */
        }
        len = clipboard_data_size;
        *data_ret = clipboard_data;
    } else {
        if (len > 0)
            *data_ret = data;
        else
            *data_ret = NULL;
    }

    if (len > 0)
        ret_val = len;
    else
        LOG_WARN("property contains no data (zero length)");

exit:
    if ((incr || ret_val == -1) && data)
        XFree(data);

    if (incr) {
        clipboard_data_size = 0;
        waiting_for_property_notify = false;
    }

    return ret_val;
}

static void get_selection_free(unsigned char *data, bool incr)
{
    if (incr) {
        /* If the clipboard was large return the memory to the system */
        if (clipboard_data_space > 512 * 1024) {
            free(clipboard_data);
            clipboard_data = NULL;
            clipboard_data_space = 0;
        }
    } else if (data)
        XFree(data);
}

static Atom atom_lists_overlap(Atom *atoms1, Atom *atoms2, int l1, int l2)
{
    int i, j;

    for (i = 0; i < l1; i++)
        for (j = 0; j < l2; j++)
            if (atoms1[i] == atoms2[j])
                return atoms1[i];

    return 0;
}

static void handle_targets_notify(XEvent& event, bool incr)
{
    int i, len;
    Lock lock(clipboard_lock);
    Atom atom, *atoms = NULL;

    if (!expected_targets_notifies) {
        LOG_WARN("unexpected selection notify TARGETS");
        return;
    }
    expected_targets_notifies--;

    /* If we have more targets_notifies pending, ignore this one, we
       are only interested in the targets list of the current owner
       (which is the last one we've requested a targets list from) */
    if (expected_targets_notifies)
        return;

    len = get_selection(event, XA_ATOM, targets_atom, 32,
                        (unsigned char **)&atoms, incr);
    if (len == 0 || len == -1) /* waiting for more data or error? */
        return;

    /* bytes -> atoms */
    len /= sizeof(Atom);
    print_targets("received", atoms, len);

    clipboard_type_count = 0;
    for (i = 0; i < clipboard_format_count; i++) {
        atom = atom_lists_overlap(clipboard_formats[i].atoms, atoms,
                                  clipboard_formats[i].atom_count, len);
        if (atom) {
            clipboard_agent_types[clipboard_type_count] =
                clipboard_formats[i].type;
            clipboard_x11_targets[clipboard_type_count] = atom;
            clipboard_type_count++;
            if (clipboard_type_count ==
                    sizeof(clipboard_agent_types)/sizeof(uint32_t)) {
                LOG_WARN("handle_targets_notify: too many matching types");
                break;
            }
        }
    }

    if (clipboard_type_count)
        clipboard_listener->on_clipboard_grab(clipboard_agent_types,
                                              clipboard_type_count);

    get_selection_free((unsigned char *)atoms, incr);
}

static void handle_selection_notify(XEvent& event, bool incr)
{
    int len = -1;
    uint32_t type = get_clipboard_type(clipboard_request_target);
    unsigned char *data = NULL;

    if (clipboard_request_target == None)
        LOG_INFO("SelectionNotify received without a target");
    else if (!incr &&
             event.xselection.target != clipboard_request_target &&
             event.xselection.target != incr_atom)
        LOG_WARN("Requested %s target got %s",
                 atom_name(clipboard_request_target),
                 atom_name(event.xselection.target));
    else
        len = get_selection(event, clipboard_request_target, clipboard_prop,
                            8, &data, incr);

    if (len == 0) /* waiting for more data? */
        return;
    if (len == -1) {
        type = VD_AGENT_CLIPBOARD_NONE;
        len = 0;
    }

    clipboard_listener->on_clipboard_notify(type, data, len);
    clipboard_request_target = None;
    get_selection_free(data, incr);
}

static void handle_selection_request()
{
    XEvent *event;
    uint32_t type = VD_AGENT_CLIPBOARD_NONE;

    if (!next_selection_request)
        return;

    event = &next_selection_request->event;

    if (Platform::get_clipboard_owner() != Platform::owner_guest) {
        LOG_INFO("received selection request event for target %s, "
                 "while clipboard not owned by guest",
                 atom_name(event->xselectionrequest.target));
        send_selection_notify(None, 1);
        return;
    }

    if (event->xselectionrequest.target == multiple_atom) {
        LOG_WARN("multiple target not supported");
        send_selection_notify(None, 1);
        return;
    }

    if (event->xselectionrequest.target == targets_atom) {
        send_targets(*event);
        return;
    }

    type = get_clipboard_type(event->xselectionrequest.target);
    if (type == VD_AGENT_CLIPBOARD_NONE) {
        send_selection_notify(None, 1);
        return;
    }

    clipboard_listener->on_clipboard_request(type);
}

static void root_win_proc(XEvent& event)
{

#ifdef USE_XRANDR_1_2
    ASSERT(using_xrandr_1_0 || using_xrandr_1_2);
#else
    ASSERT(using_xrandr_1_0);
#endif
    if (event.type == ConfigureNotify || event.type - xrandr_event_base == RRScreenChangeNotify) {
        XRRUpdateConfiguration(&event);
        if (event.type - xrandr_event_base == RRScreenChangeNotify) {
            display_mode_listener->on_display_mode_change();
        }

        if (Monitor::is_self_change()) {
            return;
        }

        ScreenList::iterator iter = screens.begin();
        for (; iter != screens.end(); iter++) {
            (*iter)->set_broken();
        }
        event_listener->on_monitors_change();
        return;
    }
    if (event.type == XFixesSelectionNotify + xfixes_event_base) {
        XFixesSelectionNotifyEvent* selection_event = (XFixesSelectionNotifyEvent *)&event;
        switch (selection_event->subtype) {
        case XFixesSetSelectionOwnerNotify:
            break;
        /* Treat ... as a SelectionOwnerNotify None */
        case XFixesSelectionWindowDestroyNotify:
        case XFixesSelectionClientCloseNotify:
            selection_event->owner = None;
            break;
        default:
            LOG_INFO("Unsupported selection event %u", selection_event->subtype);
            return;
        }
        LOG_INFO("XFixesSetSelectionOwnerNotify %u",
                 (unsigned int)selection_event->owner);

        /* Ignore becoming the owner ourselves */
        if (selection_event->owner == platform_win)
            return;

        /* If the clipboard owner is changed we no longer own it */
        Platform::set_clipboard_owner(Platform::owner_none);
        if (selection_event->owner == None)
            return;

        /* Request the supported targets from the new owner */
        XConvertSelection(x_display, clipboard_prop, targets_atom,
                          targets_atom, platform_win, CurrentTime);
        XFlush(x_display);
        expected_targets_notifies++;
        return;
    }
    switch (event.type) {
    case SelectionRequest: {
        Lock lock(clipboard_lock);
        struct selection_request *req, *new_req;

        new_req = new selection_request;
        assert(new_req);

        new_req->event = event;
        new_req->next = NULL;

        if (!next_selection_request) {
            next_selection_request = new_req;
            handle_selection_request();
            break;
        }

        /* maybe we should limit the selection_request stack depth ? */
        req = next_selection_request;
        while (req->next)
            req = req->next;

        req->next = new_req;
        break;
    }
    case SelectionClear:
        /* Do nothing the clipboard ownership will get updated through
           the XFixesSetSelectionOwnerNotify event */
        break;
    case SelectionNotify:
        if (event.xselection.target == targets_atom)
            handle_targets_notify(event, false);
        else
            handle_selection_notify(event, false);
        break;
    case PropertyNotify:
        if (!waiting_for_property_notify || event.xproperty.state != PropertyNewValue) {
            break;
        }
        if (event.xproperty.atom == targets_atom)
            handle_targets_notify(event, true);
        else
            handle_selection_notify(event, true);
        break;
    default:
        return;
    }
}

static void process_monitor_configure_events(Window root)
{
    XEvent event;

    XLockDisplay(x_display);
    XSync(x_display, False);

    while (XCheckTypedWindowEvent(x_display, root, ConfigureNotify, &event)) {
        XUnlockDisplay(x_display);
        root_win_proc(event);
        XLockDisplay(x_display);
    }

    while (XCheckTypedWindowEvent(x_display, root, xrandr_event_base + RRScreenChangeNotify,
                                  &event)) {
        XUnlockDisplay(x_display);
        root_win_proc(event);
        XLockDisplay(x_display);
    }

    XUnlockDisplay(x_display);
}

static void cleanup(void)
{
    int i;

    DBG(0, "");
    if (!Monitor::is_self_change()) {
        Platform::destroy_monitors();
    }
    if (vinfo) {
        for (i = 0; i < ScreenCount(x_display); ++i) {
            XFree(vinfo[i]);
        }
        delete[] vinfo;
        vinfo = NULL;
    }
#ifdef USE_OPENGL
    if (fb_config) {
        for (i = 0; i < ScreenCount(x_display); ++i) {
            if (fb_config[i]) {
                XFree(fb_config[i]);
            }
        }
        delete fb_config;
        fb_config = NULL;
    }
#endif // USE_OPENGL
}

static void quit_handler(int sig)
{
    LOG_INFO("signal %d", sig);
    Platform::send_quit_request();
}

static void abort_handler(int sig)
{
    LOG_INFO("signal %d", sig);
    Platform::destroy_monitors();
}

static void init_xrandr()
{
    Bool xrandr_ext = XRRQueryExtension(x_display, &xrandr_event_base, &xrandr_error_base);
    if (xrandr_ext) {
        XRRQueryVersion(x_display, &xrandr_major, &xrandr_minor);
        if (xrandr_major < 1) {
            return;
        }
#ifdef USE_XRANDR_1_2
        if (xrandr_major == 1 && xrandr_minor < 2) {
            using_xrandr_1_0 = true;
            return;
        }
        using_xrandr_1_2 = true;
#else
        using_xrandr_1_0 = true;
#endif
    }
}

static void init_xrender()
{
    int event_base;
    int error_base;
    int major;
    int minor;

    using_xrender_0_5 = XRenderQueryExtension(x_display, &event_base, &error_base) &&
        XRenderQueryVersion(x_display, &major, &minor) && (major > 0 || minor >= 5);
}

static void init_xinerama()
{
#ifdef USE_XINERAMA_1_0
    int event_base;
    int error_base;
    int major;
    int minor;

    using_xinerama_1_0 = XineramaQueryExtension(x_display, &event_base, &error_base) &&
        XineramaQueryVersion(x_display, &major, &minor) && major >= 1 && minor >= 0 &&
        XineramaIsActive(x_display);
#endif
}

static void init_xfixes()
{
    int major;
    int minor;

    using_xfixes_1_0 = XFixesQueryExtension(x_display, &xfixes_event_base, &xfixes_error_base) &&
        XFixesQueryVersion(x_display, &major, &minor) && major >= 1;
}

static void init_kbd()
{
    int xkb_major = XkbMajorVersion;
    int xkb_minor = XkbMinorVersion;
    int opcode;
    int event;
    int error;

    if (!XkbLibraryVersion(&xkb_major, &xkb_minor) ||
        !XkbQueryExtension(x_display, &opcode, &event, &error, &xkb_major, &xkb_minor)) {
        return;
    }
    caps_lock_mask = XkbKeysymToModifiers(x_display, XK_Caps_Lock);
    num_lock_mask = XkbKeysymToModifiers(x_display, XK_Num_Lock);
}

static void init_XIM()
{
    char app_name[20];
    strcpy(app_name, "spicec");

    XSetLocaleModifiers("");
    x_input_method = XOpenIM(x_display, NULL, app_name, app_name);

    if (!x_input_method) {
        return;
    }

    x_input_context = XCreateIC(x_input_method, XNInputStyle, XIMPreeditNone | XIMStatusNone, NULL);

    if (!x_input_context) {
        THROW("create IC failed");
    }
}

static int x_error_handler(Display* display, XErrorEvent* error_event)
{
    char error_str[256];
    char request_str[256];
    char number_str[32];

    if (handle_x_error) {
        if (error_event->error_code) {
            x_error_code = error_event->error_code;
        }
        return 0;
    }

    char* display_name = XDisplayString(display);
    XGetErrorText(display, error_event->error_code, error_str, sizeof(error_str));

    if (error_event->request_code < 128) {
        snprintf(number_str, sizeof(number_str), "%d", error_event->request_code);
        XGetErrorDatabaseText(display, "XRequest", number_str, "",
                              request_str, sizeof(request_str));
    } else {
        snprintf(request_str, sizeof(request_str), "%d", error_event->request_code);
    }

    LOG_ERROR("x error on display %s error %s minor %u request %s",
              display_name,
              error_str,
              (uint32_t)error_event->minor_code,
              request_str);
    _exit(-1);
    return 0;
}

static SPICE_GNUC_NORETURN int x_io_error_handler(Display* display)
{
    LOG_ERROR("x io error on %s", XDisplayString(display));
    _exit(-1);
}

static XVisualInfo* get_x_vis_info(int screen)
{
    XVisualInfo vtemplate;
    int count;

    Visual* visual = DefaultVisualOfScreen(ScreenOfDisplay(x_display, screen));
    vtemplate.screen = screen;
    vtemplate.visualid = XVisualIDFromVisual(visual);
    return XGetVisualInfo(x_display, VisualIDMask | VisualScreenMask, &vtemplate, &count);
}

void Platform::init()
{
#ifdef USE_OPENGL
    int err, ev;
    int threads_enable;
#endif // USE_OPENGL
    int major, minor;
    Bool pixmaps;

    DBG(0, "");

    setlocale(LC_ALL, "");

#ifdef USE_OPENGL
    threads_enable = XInitThreads();
#else
    XInitThreads();
#endif

    if (!(x_display = XOpenDisplay(NULL))) {
        THROW("open X display failed");
    }

    if (XShmQueryExtension (x_display) &&
        XShmQueryVersion (x_display, &major, &minor, &pixmaps)) {
        x_shm_avail = true;
    }

    vinfo = new XVisualInfo *[ScreenCount(x_display)];
    memset(vinfo, 0, sizeof(XVisualInfo *) * ScreenCount(x_display));
    screen_format = new RedDrawable::Format[ScreenCount(x_display)];
    memset(screen_format, 0, sizeof(RedDrawable::Format) * ScreenCount(x_display));
#ifdef USE_OPENGL
    fb_config = new GLXFBConfig *[ScreenCount(x_display)];
    memset(fb_config, 0, sizeof(GLXFBConfig *) * ScreenCount(x_display));

    if (threads_enable && glXQueryExtension(x_display, &err, &ev)) {
        int num_configs;
        int attrlist[] = {
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT | GLX_WINDOW_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8,
            GLX_STENCIL_SIZE, 4,
            GLX_DEPTH_SIZE, 0,
            None
        };

        for (int i = 0; i < ScreenCount(x_display); ++i) {
            fb_config[i] = glXChooseFBConfig(x_display, i, attrlist, &num_configs);
            if (fb_config[i] != NULL) {
                ASSERT(num_configs > 0);
                vinfo[i] = glXGetVisualFromFBConfig(x_display, fb_config[i][0]);
            }

            if (vinfo[i] == NULL) {
                if (fb_config[i]) {
                    XFree(fb_config[i]);
                    fb_config[i] = NULL;
                }
                vinfo[i] = get_x_vis_info(i);
            }
        }
    } else
#else // !USE_OPENGL
    {
        for (int i = 0; i < ScreenCount(x_display); ++i) {
            vinfo[i] = get_x_vis_info(i);
        }
    }
#endif // USE_OPENGL

    for (int i = 0; i < ScreenCount(x_display); ++i) {
        if (vinfo[i] == NULL) {
            THROW("Unable to find a visual for screen");
        }
        if ((vinfo[i]->depth == 32  || vinfo[i]->depth == 24) &&
            vinfo[i]->red_mask == 0xff0000 &&
            vinfo[i]->green_mask == 0x00ff00 &&
            vinfo[i]->blue_mask == 0x0000ff) {
            screen_format[i] = RedDrawable::RGB32;
        } else if (vinfo[i]->depth == 16 &&
                   vinfo[i]->red_mask == 0xf800 &&
                   vinfo[i]->green_mask == 0x7e0 &&
                   vinfo[i]->blue_mask == 0x1f) {
            screen_format[i] = RedDrawable::RGB16_565;
        } else if (vinfo[i]->depth == 15 &&
                   vinfo[i]->red_mask == 0x7c00 &&
                   vinfo[i]->green_mask == 0x3e0 &&
                   vinfo[i]->blue_mask == 0x1f) {
            screen_format[i] = RedDrawable::RGB16_555;
        } else {
            THROW("Unsupported visual for screen");
        }
    }

    XSetErrorHandler(x_error_handler);
    XSetIOErrorHandler(x_io_error_handler);

    win_proc_context = XUniqueContext();

    init_kbd();
    init_xrandr();
    init_xrender();
    init_xfixes();
    init_XIM();
    init_xinerama();

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    sigfillset(&act.sa_mask);

    act.sa_handler = quit_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);

    act.sa_flags = SA_RESETHAND;
    act.sa_handler = abort_handler;
    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGILL, &act, NULL);
    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGFPE, &act, NULL);

    atexit(cleanup);
}

void Platform::set_process_loop(ProcessLoop& main_process_loop)
{
    main_loop = &main_process_loop;
    XEventHandler *x_event_handler;
    x_event_handler = new XEventHandler(*x_display, win_proc_context);
    main_loop->add_file(*x_event_handler);
}

uint32_t Platform::get_keyboard_lock_modifiers()
{
    XKeyboardState keyboard_state;
    uint32_t modifiers = 0;

    XLockDisplay(x_display);
    XGetKeyboardControl(x_display, &keyboard_state);
    XUnlockDisplay(x_display);

    if (keyboard_state.led_mask & 0x01) {
        modifiers |= CAPS_LOCK_MODIFIER;
    }
    if (keyboard_state.led_mask & 0x02) {
        modifiers |= NUM_LOCK_MODIFIER;
    }
    if (keyboard_state.led_mask & 0x04) {
        modifiers |= SCROLL_LOCK_MODIFIER;
    }
    return modifiers;
}

enum XLed {
    X11_CAPS_LOCK_LED = 1,
    X11_NUM_LOCK_LED,
    X11_SCROLL_LOCK_LED,
};

static void  set_keyboard_led(XLed led, int set)
{
    switch (led) {
    case X11_CAPS_LOCK_LED:
        if (caps_lock_mask) {
            XkbLockModifiers(x_display, XkbUseCoreKbd, caps_lock_mask, set ? caps_lock_mask : 0);
            XFlush(x_display);
        }
        return;
    case X11_NUM_LOCK_LED:
        if (num_lock_mask) {
            XkbLockModifiers(x_display, XkbUseCoreKbd, num_lock_mask, set ? num_lock_mask : 0);
            XFlush(x_display);
        }
        return;
    case X11_SCROLL_LOCK_LED:
        XKeyboardControl keyboard_control;
        keyboard_control.led_mode = set ? LedModeOn : LedModeOff;
        keyboard_control.led = led;
        XChangeKeyboardControl(x_display, KBLed | KBLedMode, &keyboard_control);
        XFlush(x_display);
        return;
    }
}

void Platform::set_keyboard_lock_modifiers(uint32_t modifiers)
{
    uint32_t now = get_keyboard_lock_modifiers();

    if ((now & CAPS_LOCK_MODIFIER) != (modifiers & CAPS_LOCK_MODIFIER)) {
        set_keyboard_led(X11_CAPS_LOCK_LED, !!(modifiers & CAPS_LOCK_MODIFIER));
    }
    if ((now & NUM_LOCK_MODIFIER) != (modifiers & NUM_LOCK_MODIFIER)) {
        set_keyboard_led(X11_NUM_LOCK_LED, !!(modifiers & NUM_LOCK_MODIFIER));
    }
    if ((now & SCROLL_LOCK_MODIFIER) != (modifiers & SCROLL_LOCK_MODIFIER)) {
        set_keyboard_led(X11_SCROLL_LOCK_LED, !!(modifiers & SCROLL_LOCK_MODIFIER));
    }
}

static uint32_t key_bit(char* keymap, int key, uint32_t bit)
{
    KeyCode key_code = XKeysymToKeycode(x_display, key);
    return (((keymap[key_code >> 3] >> (key_code & 7)) & 1) ? bit : 0);
}

uint32_t Platform::get_keyboard_modifiers()
{
    char keymap[32];
    uint32_t mods;

    XLockDisplay(x_display);
    XQueryKeymap(x_display, keymap);
    mods = key_bit(keymap, XK_Shift_L, L_SHIFT_MODIFIER) |
           key_bit(keymap, XK_Shift_R, R_SHIFT_MODIFIER) |
           key_bit(keymap, XK_Control_L, L_CTRL_MODIFIER) |
           key_bit(keymap, XK_Control_R, R_CTRL_MODIFIER) |
           key_bit(keymap, XK_Alt_L, L_ALT_MODIFIER) |
           key_bit(keymap, XK_Alt_R, R_ALT_MODIFIER);
    XUnlockDisplay(x_display);

    return mods;
}

void Platform::reset_cursor_pos()
{
    if (!primary_monitor) {
        return;
    }
    SpicePoint pos =  primary_monitor->get_position();
    SpicePoint size =  primary_monitor->get_size();
    Window root_window = RootWindow(x_display, DefaultScreen(x_display));
    XWarpPointer(x_display, None, root_window, 0, 0, 0, 0, pos.x + size.x / 2, pos.y + size.y / 2);
    XFlush(x_display);
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

void XPlatform::on_focus_in()
{
    if (focus_count++ == 0) {
        event_listener->on_app_activated();
    }
}

void XPlatform::on_focus_out()
{
    ASSERT(focus_count > 0);
    if (--focus_count == 0) {
        event_listener->on_app_deactivated();
    }
}

class XBaseLocalCursor: public LocalCursor {
public:
    XBaseLocalCursor() : _handle (0) {}
    ~XBaseLocalCursor();
    void set(Window window);

protected:
    Cursor _handle;
};

void XBaseLocalCursor::set(Window window)
{
    if (_handle) {
        XDefineCursor(x_display, window, _handle);
        XFlush(x_display);
    }
}

XBaseLocalCursor::~XBaseLocalCursor()
{
    if (_handle) {
        XFreeCursor(x_display, _handle);
    }
}

class XLocalCursor: public XBaseLocalCursor {
public:
    XLocalCursor(CursorData* cursor_data);
};

static inline uint8_t get_pix_mask(const uint8_t* data, int offset, int pix_index)
{
    return data[offset + (pix_index >> 3)] & (0x80 >> (pix_index % 8));
}

static inline uint32_t get_pix_hack(int pix_index, int width)
{
    return (((pix_index % width) ^ (pix_index / width)) & 1) ? 0xc0303030 : 0x30505050;
}

XLocalCursor::XLocalCursor(CursorData* cursor_data)
{
    const SpiceCursorHeader& header = cursor_data->header();
    const uint8_t* data = cursor_data->data();
    int cur_size = header.width * header.height;
    uint8_t pix_mask;
    uint32_t pix;
    uint16_t i;
    int size;

    if (!get_size_bits(header, size)) {
        THROW("invalid cursor type");
    }

    uint32_t* cur_data = new uint32_t[cur_size];

    switch (header.type) {
    case SPICE_CURSOR_TYPE_ALPHA:
        break;
    case SPICE_CURSOR_TYPE_COLOR32:
        memcpy(cur_data, data, cur_size * sizeof(uint32_t));
        for (i = 0; i < cur_size; i++) {
            pix_mask = get_pix_mask(data, size, i);
            if (pix_mask && *((uint32_t*)data + i) == 0xffffff) {
                cur_data[i] = get_pix_hack(i, header.width);
            } else {
                cur_data[i] |= (pix_mask ? 0 : 0xff000000);
            }
        }
        break;
    case SPICE_CURSOR_TYPE_COLOR16:
        for (i = 0; i < cur_size; i++) {
            pix_mask = get_pix_mask(data, size, i);
            pix = *((uint16_t*)data + i);
            if (pix_mask && pix == 0x7fff) {
                cur_data[i] = get_pix_hack(i, header.width);
            } else {
                cur_data[i] = ((pix & 0x1f) << 3) | ((pix & 0x3e0) << 6) | ((pix & 0x7c00) << 9) |
                    (pix_mask ? 0 : 0xff000000);
            }
        }
        break;
    case SPICE_CURSOR_TYPE_MONO:
        for (i = 0; i < cur_size; i++) {
            pix_mask = get_pix_mask(data, 0, i);
            pix = get_pix_mask(data, size, i);
            if (pix_mask && pix) {
                cur_data[i] = get_pix_hack(i, header.width);
            } else {
                cur_data[i] = (pix ? 0xffffff : 0) | (pix_mask ? 0 : 0xff000000);
            }
        }
        break;
    case SPICE_CURSOR_TYPE_COLOR4:
        for (i = 0; i < cur_size; i++) {
            pix_mask = get_pix_mask(data, size + (sizeof(uint32_t) << 4), i);
            int idx = (i & 1) ? (data[i >> 1] & 0x0f) : ((data[i >> 1] & 0xf0) >> 4);
            pix = *((uint32_t*)(data + size) + idx);
            if (pix_mask && pix == 0xffffff) {
                cur_data[i] = get_pix_hack(i, header.width);
            } else {
                cur_data[i] = pix | (pix_mask ? 0 : 0xff000000);
            }
        }
        break;
    case SPICE_CURSOR_TYPE_COLOR24:
    case SPICE_CURSOR_TYPE_COLOR8:
    default:
        LOG_WARN("unsupported cursor type %d", header.type);
        XLockDisplay(x_display);
        _handle = XCreateFontCursor(x_display, XC_arrow);
        XUnlockDisplay(x_display);
        delete[] cur_data;
        return;
    }

    XImage image;
    memset(&image, 0, sizeof(image));
    image.width = header.width;
    image.height = header.height;
    image.data = (header.type == SPICE_CURSOR_TYPE_ALPHA ? (char*)data : (char*)cur_data);
    image.byte_order = LSBFirst;
    image.bitmap_unit = 32;
    image.bitmap_bit_order = LSBFirst;
    image.bitmap_pad = 8;
    image.bytes_per_line = header.width << 2;
    image.depth = 32;
    image.format = ZPixmap;
    image.bits_per_pixel = 32;
    image.red_mask = 0x00ff0000;
    image.green_mask = 0x0000ff00;
    image.blue_mask = 0x000000ff;
    if (!XInitImage(&image)) {
        THROW("init image failed");
    }

    Window root_window = RootWindow(x_display, DefaultScreen(x_display));
    XGCValues gc_vals;
    gc_vals.function = GXcopy;
    gc_vals.foreground = ~0;
    gc_vals.background = 0;
    gc_vals.plane_mask = AllPlanes;

    XLockDisplay(x_display);
    Pixmap pixmap = XCreatePixmap(x_display, root_window, header.width, header.height, 32);
    GC gc = XCreateGC(x_display, pixmap, GCFunction | GCForeground | GCBackground | GCPlaneMask,
                      &gc_vals);

    XPutImage(x_display, pixmap, gc, &image, 0, 0, 0, 0, header.width, header.height);
    XFreeGC(x_display, gc);

    XRenderPictFormat *xformat = XRenderFindStandardFormat(x_display, PictStandardARGB32);
    Picture picture = XRenderCreatePicture(x_display, pixmap, xformat, 0, NULL);
    _handle = XRenderCreateCursor(x_display, picture, header.hot_spot_x, header.hot_spot_y);
    XUnlockDisplay(x_display);

    XRenderFreePicture(x_display, picture);
    XFreePixmap(x_display, pixmap);
    delete[] cur_data;
}

LocalCursor* Platform::create_local_cursor(CursorData* cursor_data)
{
    ASSERT(using_xrender_0_5);
    return new XLocalCursor(cursor_data);
}

class XInactiveCursor: public XBaseLocalCursor {
public:
    XInactiveCursor() { _handle = XCreateFontCursor(x_display, XC_X_cursor);}
};

LocalCursor* Platform::create_inactive_cursor()
{
    return new XInactiveCursor();
}

class XDefaultCursor: public XBaseLocalCursor {
public:
    XDefaultCursor()
    {
        XLockDisplay(x_display);
        _handle = XCreateFontCursor(x_display, XC_top_left_arrow);
        XUnlockDisplay(x_display);
    }
};

LocalCursor* Platform::create_default_cursor()
{
    return new XDefaultCursor();
}

bool Platform::on_clipboard_grab(uint32_t *types, uint32_t type_count)
{
    Lock lock(clipboard_lock);

    if (type_count > sizeof(clipboard_agent_types)/sizeof(uint32_t)) {
        LOG_WARN("on_clipboard_grab: too many types");
        type_count = sizeof(clipboard_agent_types)/sizeof(uint32_t);
    }

    memcpy(clipboard_agent_types, types, type_count * sizeof(uint32_t));
    clipboard_type_count = type_count;

    XSetSelectionOwner(x_display, clipboard_prop, platform_win, CurrentTime);
    XFlush(x_display);

    set_clipboard_owner_unlocked(owner_guest);
    return true;
}

int Platform::_clipboard_owner = Platform::owner_none;

void Platform::set_clipboard_owner(int new_owner)
{
        Lock lock(clipboard_lock);
        set_clipboard_owner_unlocked(new_owner);
}

void Platform::set_clipboard_owner_unlocked(int new_owner)
{
    const char * const owner_str[] = { "none", "guest", "client" };

    /* Clear pending requests and clipboard data */
    {
        if (next_selection_request) {
            LOG_INFO("selection requests pending upon clipboard owner change, clearing");
            while (next_selection_request)
                send_selection_notify(None, 0);
        }

        clipboard_data_size = 0;
        clipboard_request_target = None;
        waiting_for_property_notify = false;

        /* Clear cached clipboard type info when there is no new owner
           (otherwise the new owner will already have set new type info) */
        if (new_owner == owner_none)
            clipboard_type_count = 0;
    }
    if (new_owner == owner_none)
        clipboard_listener->on_clipboard_release();

    _clipboard_owner = new_owner;
    LOG_INFO("new clipboard owner: %s", owner_str[new_owner]);
}

void Platform::set_clipboard_listener(ClipboardListener* listener)
{
    clipboard_listener = listener ? listener : &default_clipboard_listener;
}

bool Platform::on_clipboard_notify(uint32_t type, const uint8_t* data, int32_t size)
{
    Lock lock(clipboard_lock);
    Atom prop;
    XEvent *event;
    uint32_t type_from_event;

    if (!next_selection_request) {
        LOG_INFO("received clipboard data without an outstanding"
                 "selection request, ignoring");
        return true;
    }

    if (type == VD_AGENT_CLIPBOARD_NONE) {
        send_selection_notify(None, 1);
        return true;
    }

    event = &next_selection_request->event;
    type_from_event = get_clipboard_type(event->xselectionrequest.target);
    if (type_from_event != type) {
        LOG_WARN("expecting type %u clipboard data got %u",
                 type_from_event, type);
        send_selection_notify(None, 1);
        return false;
    }

    prop = event->xselectionrequest.property;
    if (prop == None)
        prop = event->xselectionrequest.target;
    /* FIXME: use INCR for large data transfers */
    XChangeProperty(x_display, event->xselectionrequest.requestor, prop,
                    event->xselectionrequest.target, 8, PropModeReplace,
                    data, size);
    send_selection_notify(prop, 1);
    return true;
}

bool Platform::on_clipboard_request(uint32_t type)
{
    Window owner;
    Lock lock(clipboard_lock);
    Atom target = get_clipboard_target(type);

    if (target == None)
        return false;

    XLockDisplay(x_display);
    owner = XGetSelectionOwner(x_display, clipboard_prop);
    XUnlockDisplay(x_display);
    if (owner == None) {
        LOG_INFO("No owner for the selection");
        return false;
    }

    if (clipboard_request_target) {
        LOG_INFO("XConvertSelection request is already pending");
        return false;
    }
    clipboard_request_target = target;
    XConvertSelection(x_display, clipboard_prop, target, clipboard_prop, platform_win, CurrentTime);
    XFlush(x_display);
    return true;
}

void Platform::on_clipboard_release()
{
    XEvent event;
    Window owner;

    XLockDisplay(x_display);
    owner = XGetSelectionOwner(x_display, clipboard_prop);
    XUnlockDisplay(x_display);
    if (owner != platform_win) {
        LOG_INFO("Platform::on_clipboard_release() called while not selection owner");
        return;
    }
    /* Note there is a small race window here where another x11 app could
       acquire selection ownership and we kick it off again, nothing we
       can do about that :( */
    XSetSelectionOwner(x_display, clipboard_prop, None, CurrentTime);

    /* Make sure we process the XFixesSetSelectionOwnerNotify event caused
       by this, so we don't end up changing the clipboard owner to none, after
       it has already been re-owned because this event is still pending. */
    XLockDisplay(x_display);
    XSync(x_display, False);
    while (XCheckTypedEvent(x_display,
                            XFixesSelectionNotify + xfixes_event_base,
                            &event)) {
        XUnlockDisplay(x_display);
        root_win_proc(event);
        XLockDisplay(x_display);
    }
    XUnlockDisplay(x_display);

    /* Note no need to do a set_clipboard_owner(owner_none) here, as that is
       already done by processing the XFixesSetSelectionOwnerNotify event. */
}
