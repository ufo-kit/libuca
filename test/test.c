
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

    uca->cam_set_property(uca, UCA_PROP_WIDTH, &width);
    uca->cam_set_property(uca, UCA_PROP_HEIGHT, &height);

    char camera_name[256] = "foobar";
    uca->cam_get_property(uca, UCA_PROP_NAME, camera_name);

    printf("Camera name: %s\n", camera_name);

    uca_destroy(uca);
    return 0;
}
