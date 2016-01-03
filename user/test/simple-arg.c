// The simplest enclave enter/exit.

#include "test.h"

void enclave_main(int argc, char **argv)
{
    printf("argc = %d\n", argc);
    puts(argv[0]);
    puts(argv[1]);
    puts(argv[2]);
}
