#include <config.h>
#include <spice.h>

int main(void)
{
    SpiceServer *server = spice_server_new();
    SpiceCoreInterface core;

    spice_server_init(server, &core);
    spice_server_set_port(server, 5911);

    return 0;
}
