
#include <stdio.h>
#include <stdlib.h>
#include "uca.h"
#include "uca-cam.h"

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

    uint32_t width, height, bits;
    cam->get_property(cam, UCA_PROP_WIDTH, &width, 0);
    cam->get_property(cam, UCA_PROP_HEIGHT, &height, 0);
    cam->get_property(cam, UCA_PROP_BITDEPTH, &bits, 0);

    uca_cam_alloc(cam, 10);

    const int pixel_size = bits == 8 ? 1 : 2;
    uint16_t *buffer = (uint16_t *) malloc(width * height * pixel_size);

    cam->start_recording(cam);
    cam->grab(cam, (char *) buffer, NULL);
    cam->stop_recording(cam);
    uca_destroy(u);

    FILE *fp = fopen("out.raw", "wb");
    fwrite(buffer, width*height, pixel_size, fp);
    fclose(fp);

    free(buffer);
    return 0;
}
