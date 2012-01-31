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
#include <unistd.h>
#include "uca.h"

struct image_props {
    uint32_t width;
    uint32_t height;
    uint32_t bits;
};

void grab_callback(uint64_t image_number, void *buffer, void *meta_data, void *user)
{
    struct image_props *props = (struct image_props *) user;
    const int pixel_size = props->bits == 8 ? 1 : 2;
    char filename[256];

    sprintf(filename, "out-%04i.raw", (int) image_number);
    FILE *fp = fopen(filename, "wb");
    fwrite(buffer, props->width * props->height, pixel_size, fp);
    fclose(fp);

    printf("grabbed picture %i at %p (%ix%i @ %i bits)\n", 
            (int) image_number, buffer, 
            props->width, props->height, props->bits);
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

    uint32_t val = 5000;
    uca_cam_set_property(cam, UCA_PROP_EXPOSURE, &val);
    val = 0;
    uca_cam_set_property(cam, UCA_PROP_DELAY, &val);

    struct image_props props;
    uca_cam_get_property(cam, UCA_PROP_WIDTH, &props.width, 0);
    uca_cam_get_property(cam, UCA_PROP_HEIGHT, &props.height, 0);
    uca_cam_get_property(cam, UCA_PROP_BITDEPTH, &props.bits, 0);

    uca_cam_alloc(cam, 10);

    uca_cam_register_callback(cam, &grab_callback, &props);
    uca_cam_start_recording(cam);
    printf("grabbing for 1 second ... ");
    fflush(stdout);
    sleep(1);
    uca_cam_stop_recording(cam);
    printf("done\n");

    uca_destroy(u);
    return 0;
}
