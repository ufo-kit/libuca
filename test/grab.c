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

static UcaCamera *camera = NULL;

static void sigint_handler(int signal)
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
    guint sensor_width, sensor_height, sensor_width_extended, sensor_height_extended;
    guint roi_width, roi_height, roi_x, roi_y, roi_width_multiplier, roi_height_multiplier;
    guint bits, sensor_rate;
    gint  cp_min, cp_max, cp_default;
    gchar *name;

    g_type_init();
    camera = uca_camera_new("pco", &error);

    if (camera == NULL) {
        g_print("Error during initialization: %s\n", error->message);
        return 1;
    }

    g_object_get(G_OBJECT(camera),
            "sensor-width", &sensor_width,
            "sensor-height", &sensor_height,
            "sensor-width-extended", &sensor_width_extended,
            "sensor-height-extended", &sensor_height_extended,
            "name", &name,
            NULL);

    g_object_set(G_OBJECT(camera),
            "exposure-time", 0.1,
            "delay-time", 0.0,
            "roi-x0", 0,
            "roi-y0", 0,
            "sensor-extended", FALSE,
            "roi-width", 1000,
            "roi-height", sensor_height,
            NULL);

    g_object_get(G_OBJECT(camera),
            "roi-width", &roi_width,
            "roi-height", &roi_height,
            "roi-width-multiplier", &roi_width_multiplier,
            "roi-height-multiplier", &roi_height_multiplier,
            "roi-x0", &roi_x,
            "roi-y0", &roi_y,
            "sensor-bitdepth", &bits,
            "sensor-pixelrate", &sensor_rate,
            "cooling-point-min", &cp_min,
            "cooling-point-max", &cp_max,
            "cooling-point-default", &cp_default,
            NULL);

    g_print("Camera: %s\n", name);
    g_free(name);

    g_print("Sensor: %ix%i px (extended: %ix%i) @ %i Hz\n", 
            sensor_width, sensor_height, 
            sensor_width_extended, sensor_height_extended,
            sensor_rate);

    g_print("ROI: %ix%i @ (%i, %i), steps: %i, %i\n", 
            roi_width, roi_height, roi_x, roi_y, roi_width_multiplier, roi_height_multiplier);

    g_print("Valid cooling point range: [%i, %i] (default: %i)\n", cp_min, cp_max, cp_default);

    const int pixel_size = bits == 8 ? 1 : 2;
    gpointer buffer = g_malloc0(roi_width * roi_height * pixel_size);
    gchar filename[FILENAME_MAX];
    GTimer *timer = g_timer_new();

    for (int i = 0; i < 1; i++) {
        gint counter = 0;
        g_print("Start recording\n");
        uca_camera_start_recording(camera, &error);
        g_assert_no_error(error);

        while (counter < 2) {
            g_print(" grab frame ... ");
            g_timer_start(timer);
            uca_camera_grab(camera, &buffer, &error);

            if (error != NULL) {
                g_print("\nError: %s\n", error->message);
                goto cleanup;
            }

            g_timer_stop(timer);
            g_print("done (took %3.5fs)\n", g_timer_elapsed(timer, NULL));

            snprintf(filename, FILENAME_MAX, "frame-%08i.raw", counter++);
            FILE *fp = fopen(filename, "wb");
            fwrite(buffer, roi_width * roi_height, pixel_size, fp);
            fclose(fp);
        }

        g_print("Stop recording\n");
        uca_camera_stop_recording(camera, &error);
        g_assert_no_error(error);
    }

    g_timer_destroy(timer);

cleanup:
    uca_camera_stop_recording(camera, NULL);
    g_object_unref(camera);
    g_free(buffer);

    return error != NULL ? 1 : 0;
}
