#ifndef __TEST_DISPLAY_BASE_H__
#define __TEST_DISPLAY_BASE_H__

#include <spice.h>
#include "basic_event_loop.h"

#define COUNT(x) ((sizeof(x)/sizeof(x[0])))

/*
 * simple queue for commands.
 * each command can have up to two parameters (grow as needed)
 *
 * TODO: switch to gtk main loop. Then add gobject-introspection. then
 * write tests in python/guile/whatever.
 */
typedef enum {
    PATH_PROGRESS,
    SIMPLE_CREATE_SURFACE,
    SIMPLE_DRAW,
    SIMPLE_DRAW_BITMAP,
    SIMPLE_DRAW_SOLID,
    SIMPLE_COPY_BITS,
    SIMPLE_DESTROY_SURFACE,
    SIMPLE_UPDATE,
    DESTROY_PRIMARY,
    CREATE_PRIMARY,
    SLEEP
} CommandType;

typedef struct CommandCreatePrimary {
    uint32_t width;
    uint32_t height;
} CommandCreatePrimary;

typedef struct CommandCreateSurface {
    uint32_t surface_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint8_t *data;
} CommandCreateSurface;

typedef struct CommandDrawBitmap {
    QXLRect bbox;
    uint8_t *bitmap;
    uint32_t surface_id;
    uint32_t num_clip_rects;
    QXLRect *clip_rects;
} CommandDrawBitmap;

typedef struct CommandDrawSolid {
    QXLRect bbox;
    uint32_t color;
    uint32_t surface_id;
} CommandDrawSolid;

typedef struct CommandSleep {
    uint32_t secs;
} CommandSleep;

typedef struct Command Command;
typedef struct Test Test;

struct Command {
    CommandType command;
    void (*cb)(Test *test, Command *command);
    void *cb_opaque;
    union {
        CommandCreatePrimary create_primary;
        CommandDrawBitmap bitmap;
        CommandDrawSolid solid;
        CommandSleep sleep;
        CommandCreateSurface create_surface;
    };
};

#define MAX_HEIGHT 2048
#define MAX_WIDTH 2048

#define SURF_WIDTH 320
#define SURF_HEIGHT 240

struct Test {
    SpiceCoreInterface *core;
    SpiceServer *server;

    QXLInstance qxl_instance;
    QXLWorker *qxl_worker;

    uint8_t primary_surface[MAX_HEIGHT * MAX_WIDTH * 4];
    int primary_height;
    int primary_width;

    SpiceTimer *wakeup_timer;
    int wakeup_ms;

    int cursor_notify;

    uint8_t secondary_surface[SURF_WIDTH * SURF_HEIGHT * 4];
    int has_secondary;

    // Current mode (set by create_primary)
    int width;
    int height;

    // qxl scripted rendering commands and io
    Command *commands;
    int num_commands;
    int cmd_index;

    int target_surface;

    // callbacks
    void (*on_client_connected)(Test *test);
    void (*on_client_disconnected)(Test *test);
};

void test_set_simple_command_list(Test *test, int *command, int num_commands);
void test_set_command_list(Test *test, Command *command, int num_commands);
void test_add_display_interface(Test *test);
void test_add_agent_interface(SpiceServer *server); // TODO - Test *test
Test* test_new(SpiceCoreInterface* core);

uint32_t test_get_width(void);
uint32_t test_get_height(void);

void spice_test_config_parse_args(int argc, char **argv);

#endif /* __TEST_DISPLAY_BASE_H__ */
