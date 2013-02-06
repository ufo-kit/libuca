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
#include <string.h>
#include <stdlib.h>
#include "uca-camera.h"
#include "uca-plugin-manager.h"

typedef void (*GrabFrameFunc) (UcaCamera *camera, gpointer buffer, guint n_frames);

static UcaCamera *camera = NULL;

static void
sigint_handler(int signal)
{
    g_print ("Closing down libuca\n");
    uca_camera_stop_recording (camera, NULL);
    g_object_unref (camera);
    exit (signal);
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

static void
log_handler (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user)
{
    gsize       n_written;
    GError     *error = NULL;
    GIOChannel *channel = user;

#if GLIB_CHECK_VERSION(2, 26, 0)
    GTimeZone  *tz;
    GDateTime  *date_time;
    gchar      *new_message;

    tz = g_time_zone_new_local ();
    date_time = g_date_time_new_now (tz);

    new_message = g_strdup_printf ("[%s] %s\n",
                                   g_date_time_format (date_time, "%FT%H:%M:%S%z"), message);

    g_time_zone_unref (tz);
    g_date_time_unref (date_time);

    g_io_channel_write_chars (channel, new_message, strlen (new_message), &n_written, &error);
    g_assert_no_error (error);
    g_free (new_message);
#else
    g_io_channel_write_chars (channel, message, strlen (message), &n_written, &error);
    g_assert_no_error (error);
#endif

    g_io_channel_flush (channel, &error);
    g_assert_no_error (error);
}

static void
grab_frames_sync (UcaCamera *camera, gpointer buffer, guint n_frames)
{
    GError *error = NULL;

    uca_camera_start_recording (camera, &error);

    for (guint i = 0; i < n_frames; i++) {
        uca_camera_grab(camera, &buffer, &error);

        if (error != NULL) {
            g_warning ("Error grabbing frame %02i/%i: `%s'", i, n_frames, error->message);
            g_error_free (error);
            error = NULL;
        }
    }

    uca_camera_stop_recording (camera, &error);
}

static void
grab_callback (gpointer data, gpointer user_data)
{
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
    guint *n_acquired_frames = user_data;

    g_static_mutex_lock (&mutex);
    *n_acquired_frames += 1;
    g_static_mutex_unlock (&mutex);
}

static void
grab_frames_async (UcaCamera *camera, gpointer buffer, guint n_frames)
{
    GError *error = NULL;
    guint n_acquired_frames = 0;

    uca_camera_set_grab_func (camera, grab_callback, &n_acquired_frames);
    uca_camera_start_recording (camera, &error);

    /*
     * Behold! Spinlooping is probably a bad idea but nowadays single core
     * machines are relatively rare.
     */
    while (n_acquired_frames < n_frames)
        ;

    uca_camera_stop_recording (camera, &error);

}

static void
benchmark_method (UcaCamera *camera, gpointer buffer, GrabFrameFunc func, guint n_runs, guint n_frames, guint n_bytes)
{
    GTimer *timer;
    gdouble fps;
    gdouble bandwidth;
    gdouble total_time = 0.0;
    GError *error = NULL;

    g_print ("%-10i%-10i", n_frames, n_runs);
    timer = g_timer_new ();
    g_assert_no_error (error);

    for (guint run = 0; run < n_runs; run++) {
        g_message ("Start run %i of %i", run, n_runs);
        g_timer_start (timer);

        func (camera, buffer, n_frames);

        g_timer_stop (timer);
        total_time += g_timer_elapsed (timer, NULL);
    }

    g_assert_no_error (error);

    fps = n_runs * n_frames / total_time;
    bandwidth = n_bytes * fps / 1024 / 1024;
    g_print ("%-16.2f%-16.2f\n", fps, bandwidth);

    g_timer_destroy (timer);
}

static void
benchmark (UcaCamera *camera)
{
    const guint n_runs = 3;
    const guint n_frames = 100;

    guint   sensor_width;
    guint   sensor_height;
    guint   roi_width;
    guint   roi_height;
    guint   bits;
    guint   n_bytes_per_pixel;
    guint   n_bytes;
    gdouble exposure = 0.00001;
    gpointer buffer;

    g_object_set (G_OBJECT (camera),
                  "exposure-time", exposure,
                  NULL);

    g_object_get (G_OBJECT (camera),
                  "sensor-width", &sensor_width,
                  "sensor-height", &sensor_height,
                  "sensor-bitdepth", &bits,
                  "roi-width", &roi_width,
                  "roi-height", &roi_height,
                  "exposure-time", &exposure,
                  NULL);

    g_print ("# --- General information ---\n");
    g_print ("# Sensor size: %ix%i\n", sensor_width, sensor_height);
    g_print ("# ROI size: %ix%i\n", roi_width, roi_height);
    g_print ("# Exposure time: %fs\n", exposure);
    g_print ("# Bits: %i\n", bits);

    /* Synchronous frame acquisition */
    g_print ("# %-10s%-10s%-10s%-16s%-16s\n", "type", "n_frames", "n_runs", "frames/s", "MiB/s");
    g_print ("  %-10s", "sync");

    g_message ("Start synchronous benchmark");

    n_bytes_per_pixel = bits > 8 ? 2 : 1;
    n_bytes = roi_width * roi_height * n_bytes_per_pixel;
    buffer = g_malloc0(n_bytes);

    benchmark_method (camera, buffer, grab_frames_sync, n_runs, n_frames, n_bytes);

    /* Asynchronous frame acquisition */
    g_object_set (G_OBJECT(camera),
                 "transfer-asynchronously", TRUE,
                 NULL);

    g_message ("Start asynchronous benchmark");
    g_print ("  %-10s", "async");

    benchmark_method (camera, buffer, grab_frames_async, n_runs, n_frames, n_bytes);

    g_free (buffer);
}

int
main (int argc, char *argv[])
{
    UcaPluginManager *manager;
    GIOChannel  *log_channel;
    GError      *error = NULL;

    (void) signal (SIGINT, sigint_handler);
    g_type_init();

    if (argc < 2) {
        print_usage();
        return 1;
    }

    log_channel = g_io_channel_new_file ("error.log", "a+", &error);
    g_assert_no_error (error);
    g_log_set_handler (NULL, G_LOG_LEVEL_MASK, log_handler, log_channel);

    manager = uca_plugin_manager_new ();
    camera = uca_plugin_manager_get_camera (manager, argv[1], &error);

    if (camera == NULL) {
        g_error ("Initialization: %s", error->message);
        return 1;
    }

    benchmark (camera);

    g_object_unref (camera);
    g_io_channel_shutdown (log_channel, TRUE, &error);
    g_assert_no_error (error);

    return 0;
}
