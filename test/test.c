
#include <stdio.h>
#include "uca.h"

int main(int argc, char *argv[])
{
    struct uca_t *uca = uca_init();
    if (uca == NULL) {
        printf("Couldn't find a camera\n");
        return 1;
    }

    uint32_t width = 2560, height = 2160;

    uca->cam_set_dimensions(uca, &width, &height);

    if (uca->camera_name != NULL)
        printf("Camera name: %s\n", uca->camera_name);

    uca_destroy(uca);
    return 0;
}
