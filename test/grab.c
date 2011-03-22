
#include <stdio.h>
#include <stdlib.h>
#include "uca.h"
#include "uca-cam.h"

#define handle_error(errno) {if ((errno) != UCA_NO_ERROR) printf("error at <%s:%i>\n", \
    __FILE__, __LINE__);}


int main(int argc, char *argv[])
{
    struct uca *u = uca_init(NULL);
    if (u == NULL) {
        printf("Couldn't find a camera\n");
        return 1;
    }

    /* take first camera */
    struct uca_camera *cam = u->cameras;

    uint32_t val = 5000;
    handle_error(cam->set_property(cam, UCA_PROP_EXPOSURE, &val));
    val = 0;
    handle_error(cam->set_property(cam, UCA_PROP_DELAY, &val));

    uint32_t width, height, bits;
    handle_error(cam->get_property(cam, UCA_PROP_WIDTH, &width, 0));
    handle_error(cam->get_property(cam, UCA_PROP_HEIGHT, &height, 0));
    handle_error(cam->get_property(cam, UCA_PROP_BITDEPTH, &bits, 0));

    handle_error(uca_cam_alloc(cam, 10));

    const int pixel_size = bits == 8 ? 1 : 2;
    uint16_t *buffer = (uint16_t *) malloc(width * height * pixel_size);

    handle_error(cam->start_recording(cam));
    handle_error(cam->grab(cam, (char *) buffer, NULL));
    handle_error(cam->stop_recording(cam));
    uca_destroy(u);

    FILE *fp = fopen("out.raw", "wb");
    fwrite(buffer, width*height, pixel_size, fp);
    fclose(fp);

    free(buffer);
    return 0;
}
