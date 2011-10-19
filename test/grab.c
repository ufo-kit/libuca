
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "uca.h"

#define handle_error(errno) {if ((errno) != UCA_NO_ERROR) printf("error at <%s:%i>\n", \
    __FILE__, __LINE__);}

static struct uca *u = NULL;

void sigint_handler(int signal)
{
    printf("Closing down libuca\n");
    handle_error(uca_cam_stop_recording(u->cameras));
    uca_destroy(u);
    exit(signal);
}

int main(int argc, char *argv[])
{
    (void) signal(SIGINT, sigint_handler);

    u = uca_init(NULL);
    if (u == NULL) {
        printf("Couldn't find a camera\n");
        return 1;
    }

    /* take first camera */
    struct uca_camera *cam = u->cameras;

    uint32_t val = 5000;
    handle_error(uca_cam_set_property(cam, UCA_PROP_EXPOSURE, &val));
    val = 0;
    handle_error(uca_cam_set_property(cam, UCA_PROP_DELAY, &val));
    val = UCA_TIMESTAMP_ASCII | UCA_TIMESTAMP_BINARY;
    handle_error(uca_cam_set_property(cam, UCA_PROP_TIMESTAMP_MODE, &val));

    uint32_t width, height, bits;
    handle_error(uca_cam_get_property(cam, UCA_PROP_WIDTH, &width, 0));
    handle_error(uca_cam_get_property(cam, UCA_PROP_HEIGHT, &height, 0));
    handle_error(uca_cam_get_property(cam, UCA_PROP_BITDEPTH, &bits, 0));

    handle_error(uca_cam_alloc(cam, 20));

    const int pixel_size = bits == 8 ? 1 : 2;
    uint16_t *buffer = (uint16_t *) malloc(width * height * pixel_size);

    handle_error(uca_cam_start_recording(cam));

    uint32_t error = UCA_NO_ERROR;
    char filename[FILENAME_MAX];
    int counter = 0;

    while ((error == UCA_NO_ERROR) && (counter < 20)) {
        handle_error(uca_cam_grab(cam, (char *) buffer, NULL));
        snprintf(filename, FILENAME_MAX, "frame-%08i.raw", counter++);
        FILE *fp = fopen(filename, "wb");
        fwrite(buffer, width*height, pixel_size, fp);
        fclose(fp);
    }

    handle_error(uca_cam_stop_recording(cam));
    uca_destroy(u);
    free(buffer);

    return error != UCA_NO_ERROR ? 1 : 0;
}
