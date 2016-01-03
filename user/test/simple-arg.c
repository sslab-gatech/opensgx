// The simplest enclave enter/exit.

#include "test.h"

void enclave_main(int *temp)
{
    printf("%d\n", *temp);

    *temp += 1;

    printf("%d\n", *temp);
}
