
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "uca.h"
#include "uca-cam.h"

void grab_callback(uint32_t image_number, void *buffer, void *user)
{
    printf("got picture number %i\n", image_number);
}

int main(int argc, char *argv[])
{
    struct uca *u = uca_init(NULL);
    if (u == NULL) {
        printf("Couldn't find a camera\n");
        return 1;
    }

    /* take first camera */
    struct uca_camera *cam = u->cameras;

    uint32_t val = 1;
    cam->set_property(cam, UCA_PROP_EXPOSURE, &val);
    val = 0;
    cam->set_property(cam, UCA_PROP_DELAY, &val);
    val = 10;
    cam->set_property(cam, UCA_PROP_FRAMERATE, &val);

    uint32_t width, height, bits;
    cam->get_property(cam, UCA_PROP_WIDTH, &width, 0);
    cam->get_property(cam, UCA_PROP_HEIGHT, &height, 0);
    cam->get_property(cam, UCA_PROP_BITDEPTH, &bits, 0);

    uca_cam_alloc(cam, 10);

    cam->register_callback(cam, &grab_callback, NULL);
    cam->start_recording(cam);
    printf("waiting for 5 seconds\n");
    sleep(5);
    cam->stop_recording(cam);

    uca_destroy(u);
    return 0;
}
