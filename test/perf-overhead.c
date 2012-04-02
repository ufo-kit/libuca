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
#include <string.h>
#include "uca-camera.h"

#define handle_error(errno) {if ((errno) != UCA_NO_ERROR) printf("error at <%s:%i>\n", \
    __FILE__, __LINE__);}

typedef struct {
    guint counter;
    gsize size;
    gpointer destination;
} thread_data;

static UcaCamera *camera = NULL;

static void sigint_handler(int signal)
{
    printf("Closing down libuca\n");
    uca_camera_stop_recording(camera, NULL);
    g_object_unref(camera);
    exit(signal);
}

static void test_synchronous_operation(UcaCamera *camera)
{
    GError *error = NULL;
    guint width, height, bits;
    g_object_get(G_OBJECT(camera),
            "sensor-width", &width,
            "sensor-height", &height,
            "sensor-bitdepth", &bits,
            NULL);

    const int pixel_size = bits == 8 ? 1 : 2;
    const gsize size = width * height * pixel_size;
    const guint n_trials = 10000;
    gpointer buffer = g_malloc0(size);

    uca_camera_start_recording(camera, &error);
    GTimer *timer = g_timer_new();

    for (guint n = 0; n < n_trials; n++)
        uca_camera_grab(camera, &buffer, &error);

    gdouble total_time = g_timer_elapsed(timer, NULL);
    g_timer_stop(timer);

    g_print("Synchronous data transfer\n");
    g_print(" Bandwidth: %3.2f MB/s\n", size * n_trials / 1024. / 1024. / total_time);
    g_print(" Throughput: %3.2f frames/s\n", n_trials / total_time);

    uca_camera_stop_recording(camera, &error);
    g_free(buffer);
    g_timer_destroy(timer);
}

static void grab_func(gpointer data, gpointer user_data)
{
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    thread_data *d = (thread_data *) user_data;
    g_memmove(d->destination, data, d->size);
    g_static_mutex_lock(&mutex);
    d->counter++;
    g_static_mutex_unlock(&mutex);
}

static void test_asynchronous_operation(UcaCamera *camera)
{
    GError *error = NULL;
    guint width, height, bits;

    g_object_get(G_OBJECT(camera),
            "sensor-width", &width,
            "sensor-height", &height,
            "sensor-bitdepth", &bits,
            NULL);

    const guint pixel_size = bits == 8 ? 1 : 2;

    thread_data d = {
        .counter = 0,
        .size = width * height * pixel_size,
        .destination = g_malloc0(width * height * pixel_size)
    };

    g_object_set(G_OBJECT(camera),
            "transfer-asynchronously", TRUE,
            NULL);

    uca_camera_set_grab_func(camera, &grab_func, &d);
    uca_camera_start_recording(camera, &error);
    g_usleep(G_USEC_PER_SEC);
    uca_camera_stop_recording(camera, &error);

    g_print("Asynchronous data transfer\n");
    g_print(" Bandwidth: %3.2f MB/s\n", d.size * d.counter / 1024. / 1024.);
    g_print(" Throughput: %i frames/s\n", d.counter);

    g_free(d.destination);
}

int main(int argc, char *argv[])
{
    GError *error = NULL;
    (void) signal(SIGINT, sigint_handler);

    g_type_init();
    camera = uca_camera_new("mock", &error);

    if (camera == NULL) {
        g_print("Couldn't initialize camera\n");
        return 1;
    }

    test_synchronous_operation(camera);
    g_print("\n");
    test_asynchronous_operation(camera);

    g_object_unref(camera);

    return error != NULL ? 1 : 0;
}
