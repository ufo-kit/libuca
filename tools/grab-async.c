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
#include "uca-plugin-manager.h"
#include "uca-camera.h"

static UcaCamera *camera = NULL;

typedef struct {
    guint roi_width;
    guint roi_height;
    guint counter;
} CallbackData;

static void
sigint_handler(int signal)
{
    printf("Closing down libuca\n");
    uca_camera_stop_recording(camera, NULL);
    g_object_unref(camera);
    exit(signal);
}

static void
grab_callback(gpointer data, gpointer user_data)
{
    CallbackData *cbd = (CallbackData *) user_data;
    gchar *filename = g_strdup_printf("frame-%04i.raw", cbd->counter++);
    FILE *fp = fopen(filename, "wb");

    fwrite(data, sizeof(guint16), cbd->roi_width * cbd->roi_height, fp);
    g_print(".");
    fclose(fp);
    g_free(filename);
}

static void
print_usage (void)
{
    GList *types;
    UcaPluginManager *manager;

    manager = uca_plugin_manager_new ();
    g_print ("Usage: benchmark [ ");
    types = uca_plugin_manager_get_available_cameras (manager);

    if (types == NULL) {
        g_print ("] -- no camera plugin found\n");
        return;
    }

    for (GList *it = g_list_first (types); it != NULL; it = g_list_next (it)) {
        gchar *name = (gchar *) it->data;
        if (g_list_next (it) == NULL)
            g_print ("%s ]\n", name);
        else
            g_print ("%s, ", name);
    }
}

int
main(int argc, char *argv[])
{
    CallbackData cbd;
    guint sensor_width, sensor_height;
    gchar *name;
    UcaPluginManager *manager;
    GError *error = NULL;
    (void) signal(SIGINT, sigint_handler);

    g_type_init();

    if (argc < 2) {
        print_usage();
        return 1;
    }

    manager = uca_plugin_manager_new ();
    camera = uca_plugin_manager_get_camera (manager, argv[1], &error);

    if (camera == NULL) {
        g_print("Error during initialization: %s\n", error->message);
        return 1;
    }

    g_object_get(G_OBJECT(camera),
            "name", &name,
            "sensor-width", &sensor_width,
            "sensor-height", &sensor_height,
            NULL);

    g_object_set(G_OBJECT(camera),
            "roi-x0", 0,
            "roi-y0", 0,
            "roi-width", sensor_width,
            "roi-height", sensor_height,
            "transfer-asynchronously", TRUE,
            NULL);

    g_object_get(G_OBJECT(camera),
            "roi-width", &cbd.roi_width,
            "roi-height", &cbd.roi_height,
            NULL);

    g_print("Camera: %s\n", name);
    g_free(name);

    g_print("Start asynchronous recording\n");
    cbd.counter = 0;
    uca_camera_set_grab_func(camera, grab_callback, &cbd);
    uca_camera_start_recording(camera, &error);
    g_assert_no_error(error);
    g_usleep(2 * G_USEC_PER_SEC);

    g_print(" done\n");
    uca_camera_stop_recording(camera, NULL);
    g_object_unref(camera);

    return error != NULL ? 1 : 0;
}
