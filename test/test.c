
#include <stdio.h>
#include "uca.h"

int main(int argc, char *argv[])
{
    struct uca_t *uca = uca_init();
    if (uca == NULL) {
        printf("Couldn't find a camera\n");
    }
    return 0;
}
