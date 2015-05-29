#include <config.h>
#include <sys/select.h>
#include <spice.h>
#include "basic_event_loop.h"

int main(void)
{
    SpiceServer *server = spice_server_new();
    SpiceCoreInterface *core = basic_event_loop_init();

    spice_server_set_port(server, 5912);
    spice_server_set_noauth(server);
    spice_server_init(server, core);

    basic_event_loop_mainloop();

    return 0;
}
