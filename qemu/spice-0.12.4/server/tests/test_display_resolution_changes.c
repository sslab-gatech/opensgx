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

void set_primary_params(Test *test, Command *command)
{
#if 0
    static int toggle = 0;

    if (toggle) {
        *arg1 = 800;
        *arg2 = 600;
    } else {
        *arg1 = 1024;
        *arg2 = 768;
    }
    toggle = 1 - toggle;
#endif
    static int count = 0;

    command->create_primary.width = 800 + sin((float)count / 6) * 200;
    command->create_primary.height = 600 + cos((float)count / 6) * 200;
    count++;
}

static Command commands[] = {
    {DESTROY_PRIMARY, NULL},
    {CREATE_PRIMARY, set_primary_params},
};

int main(void)
{
    SpiceCoreInterface *core;
    Test *test;

    core = basic_event_loop_init();
    test = test_new(core);
    //spice_server_set_image_compression(server, SPICE_IMAGE_COMPRESS_OFF);
    test_add_display_interface(test);
    test_set_command_list(test, commands, COUNT(commands));

    ping_timer = core->timer_add(pinger, test);
    core->timer_start(ping_timer, ping_ms);

    basic_event_loop_mainloop();

    return 0;
}
