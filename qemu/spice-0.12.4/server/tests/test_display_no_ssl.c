/**
 * Test ground for developing specific tests.
 *
 * Any specific test can start of from here and set the server to the
 * specific required state, and create specific operations or reuse
 * existing ones in the test_display_base supplied queue.
 */

#include <config.h>
#include <stdlib.h>
#include "test_display_base.h"

SpiceCoreInterface *core;
SpiceTimer *ping_timer;

void show_channels(SpiceServer *server);

int ping_ms = 100;

void pinger(void *opaque)
{
    // show_channels is not thread safe - fails if disconnections / connections occur
    //show_channels(server);

    core->timer_start(ping_timer, ping_ms);
}

int simple_commands[] = {
    //SIMPLE_CREATE_SURFACE,
    //SIMPLE_DRAW,
    //SIMPLE_DESTROY_SURFACE,
    //PATH_PROGRESS,
    SIMPLE_DRAW,
    //SIMPLE_COPY_BITS,
    SIMPLE_UPDATE,
};

int main(void)
{
    Test *test;

    core = basic_event_loop_init();
    test = test_new(core);
    //spice_server_set_image_compression(server, SPICE_IMAGE_COMPRESS_OFF);
    test_add_display_interface(test);
    test_add_agent_interface(test->server);
    test_set_simple_command_list(test, simple_commands, COUNT(simple_commands));

    ping_timer = core->timer_add(pinger, NULL);
    core->timer_start(ping_timer, ping_ms);

    basic_event_loop_mainloop();

    return 0;
}
