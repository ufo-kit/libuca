/* Copyright (C) 2011, 2012 Matthias Vogelgesang <matthias.vogelgesang@kit.edu>
   (Karlsruhe Institute of Technology)

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published by the
   Free Software Foundation; either version 2.1 of the License, or (at your
   option) any later version.

   This library is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
   details.

   You should have received a copy of the GNU Lesser General Public License along
   with this library; if not, write to the Free Software Foundation, Inc., 51
   Franklin St, Fifth Floor, Boston, MA 02110, USA */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
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
    val = 1;
    handle_error(uca_cam_set_property(cam, UCA_PROP_GRAB_AUTO, &val));

    uint32_t width, height, bits;
    handle_error(uca_cam_get_property(cam, UCA_PROP_WIDTH, &width, 0));
    handle_error(uca_cam_get_property(cam, UCA_PROP_HEIGHT, &height, 0));
    handle_error(uca_cam_get_property(cam, UCA_PROP_BITDEPTH, &bits, 0));

    handle_error(uca_cam_alloc(cam, 20));

    const int pixel_size = bits == 8 ? 1 : 2;
    uint16_t *buffer = (uint16_t *) malloc(width * height * pixel_size);

    handle_error(uca_cam_start_recording(cam));
    sleep(3);

    uint32_t error = UCA_NO_ERROR;
    char filename[FILENAME_MAX];
    int counter = 0;

    while ((error == UCA_NO_ERROR) && (counter < 20)) {
        error = uca_cam_grab(cam, (char *) buffer, NULL);
        if (error != UCA_NO_ERROR)
            break;

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
