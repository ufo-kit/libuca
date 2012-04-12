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

#include <glib-object.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "uca-camera.h"

#define handle_error(errno) {if ((errno) != UCA_NO_ERROR) printf("error at <%s:%i>\n", \
    __FILE__, __LINE__);}

static UcaCamera *camera = NULL;

void sigint_handler(int signal)
{
    printf("Closing down libuca\n");
    uca_camera_stop_recording(camera, NULL);
    g_object_unref(camera);
    exit(signal);
}

int main(int argc, char *argv[])
{
    GError *error = NULL;
    (void) signal(SIGINT, sigint_handler);

    g_type_init();
    camera = uca_camera_new("pco", &error);

    if (camera == NULL) {
        g_print("Couldn't initialize camera\n");
        return 1;
    }

    guint width, height, bits;
    g_object_get(G_OBJECT(camera),
            "sensor-width", &width,
            "sensor-height", &height,
            "sensor-bitdepth", &bits,
            NULL);

    const int pixel_size = bits == 8 ? 1 : 2;
    gpointer buffer = g_malloc0(width * height * pixel_size);

    gchar filename[FILENAME_MAX];

    for (int i = 0; i < 2; i++) {
        gint counter = 0;
        g_print("Start recording\n");
        uca_camera_start_recording(camera, &error);
        g_assert_no_error(error);

        while (counter < 4) {
            g_print(" grab frame ... ");
            uca_camera_grab(camera, &buffer, &error);
            if (error != NULL)
                break;
            g_print("done\n");

            snprintf(filename, FILENAME_MAX, "frame-%08i.raw", counter++);
            FILE *fp = fopen(filename, "wb");
            fwrite(buffer, width*height, pixel_size, fp);
            fclose(fp);
        }

        g_print("Start recording\n");
        uca_camera_stop_recording(camera, &error);
        g_assert_no_error(error);
    }

    g_object_unref(camera);
    g_free(buffer);

    return error != NULL ? 1 : 0;
}
