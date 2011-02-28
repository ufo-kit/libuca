
#include <stdio.h>
#include "uca.h"
#include "uca-cam.h"

int main(int argc, char *argv[])
{
    struct uca_t *uca = uca_init();
    if (uca == NULL) {
        printf("Couldn't find a camera\n");
        return 1;
    }

    /* take first camera */
    struct uca_camera_t *cam = uca->cameras;

    uint32_t val = 5000;
    cam->set_property(cam, UCA_PROP_EXPOSURE, &val);
    val = 0;
    cam->set_property(cam, UCA_PROP_DELAY, &val);

    if (uca_cam_alloc(cam, 20) != UCA_NO_ERROR)
        printf("Couldn't allocate buffer memory\n");
    
    uca_destroy(uca);
    return 0;
}
