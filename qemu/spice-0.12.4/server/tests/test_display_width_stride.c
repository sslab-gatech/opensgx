/**
 * Recreate the primary surface endlessly.
 */

#include <config.h>
#include <math.h>
#include <stdlib.h>
#include "test_display_base.h"

SpiceTimer *ping_timer;

void show_channels(SpiceServer *server);

int ping_ms = 100;

void pinger(void *opaque)
{
    Test *test = opaque;
    // show_channels is not thread safe - fails if disconnections / connections occur
    //show_channels(server);

    test->core->timer_start(ping_timer, ping_ms);
}

static int g_surface_id = 1;
static uint8_t *g_surface_data;

void set_draw_parameters(Test *test, Command *command)
{
    static int count = 17;
    CommandDrawSolid *solid = &command->solid;

    solid->bbox.top = 0;
    solid->bbox.left = 0;
    solid->bbox.bottom = 20;
    solid->bbox.right = count;
    solid->surface_id = g_surface_id;
    count++;
}

void set_surface_params(Test *test, Command *command)
{
    CommandCreateSurface *create = &command->create_surface;

    // UGLY
    if (g_surface_data) {
        exit(0);
    }
    create->format = SPICE_SURFACE_FMT_8_A;
    create->width = 128;
    create->height = 128;
    g_surface_data = realloc(g_surface_data, create->width * create->height * 1);
    create->surface_id = g_surface_id;
    create->data = g_surface_data;
}

void set_destroy_parameters(Test *test, Command *command)
{
    if (g_surface_data) {
        free(g_surface_data);
        g_surface_data = NULL;
    }
}

static Command commands[] = {
    {SIMPLE_CREATE_SURFACE, set_surface_params},
    {SIMPLE_DRAW_SOLID, set_draw_parameters},
    {SIMPLE_DRAW_SOLID, set_draw_parameters},
    {SIMPLE_DRAW_SOLID, set_draw_parameters},
    {SIMPLE_DRAW_SOLID, set_draw_parameters},
    {SIMPLE_DRAW_SOLID, set_draw_parameters},
    {SIMPLE_DRAW_SOLID, set_draw_parameters},
    {SIMPLE_DRAW_SOLID, set_draw_parameters},
    {SIMPLE_DRAW_SOLID, set_draw_parameters},
    {SIMPLE_DRAW_SOLID, set_draw_parameters},
    {SIMPLE_DESTROY_SURFACE, set_destroy_parameters},
};

void on_client_connected(Test *test)
{
    test_set_command_list(test, commands, COUNT(commands));
}

int main(void)
{
    SpiceCoreInterface *core;
    Test *test;

    core = basic_event_loop_init();
    test = test_new(core);
    test->on_client_connected = on_client_connected;
    //spice_server_set_image_compression(server, SPICE_IMAGE_COMPRESS_OFF);
    test_add_display_interface(test);

    ping_timer = core->timer_add(pinger, test);
    core->timer_start(ping_timer, ping_ms);

    basic_event_loop_mainloop();

    return 0;
}
