
#include <stdio.h>
#include <stdlib.h>
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

    uint32_t width, height;
    cam->get_property(cam, UCA_PROP_WIDTH, &width);
    cam->get_property(cam, UCA_PROP_HEIGHT, &height);
    printf("Image dimensions: %ix%i\n", width, height);

    if (uca_cam_alloc(cam, 20) != UCA_NO_ERROR)
        printf("Couldn't allocate buffer memory\n");

    uint16_t *buffer = (uint16_t *) malloc(width * height * 2);

    cam->start_recording(cam);
    cam->grab(cam, (char *) buffer, width*height*2);
    cam->stop_recording(cam);
    uca_destroy(uca);

    FILE *fp = fopen("out.raw", "wb");
    fwrite(buffer, width*height, 2, fp);
    fclose(fp);

    free(buffer);
    return 0;
}
