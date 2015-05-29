/**
 * Test two servers on one main loop.
 */

#include <config.h>
#include <stdlib.h>
#include "test_display_base.h"

SpiceCoreInterface *core;

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
    Test *t1;
    Test *t2;

    core = basic_event_loop_init();
    t1 = test_new(core);
    t2 = test_new(core);
    //spice_server_set_image_compression(server, SPICE_IMAGE_COMPRESS_OFF);
    test_add_display_interface(t1);
    test_add_display_interface(t2);
    test_set_simple_command_list(t1, simple_commands, COUNT(simple_commands));
    test_set_simple_command_list(t2, simple_commands, COUNT(simple_commands));

    basic_event_loop_mainloop();
    return 0;
}
