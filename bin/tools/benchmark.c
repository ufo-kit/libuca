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
#include <stdio.h>
#include "uca-camera.h"
#include "uca-plugin-manager.h"
#include "common.h"


typedef struct {
    gint n_frames;
    gint n_runs;
    gboolean test_async;
    gboolean test_software;
    gboolean test_external;
    gboolean test_readout;

    gsize n_bytes;
} Options;

typedef guint (*GrabFrameFunc) (UcaCamera *, gpointer, guint, UcaCameraTriggerSource, GTimer *);

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

    new_message = g_strdup_printf ("[%s] %s\n", g_date_time_format (date_time, "%FT%H:%M:%S%z"), message);

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

static guint
grab_frames_sync (UcaCamera *camera, gpointer buffer, guint n_frames, UcaCameraTriggerSource trigger_source, GTimer *timer)
{
    GError *error = NULL;
    guint total;

    g_object_set (camera, "trigger-source", trigger_source, NULL);
    uca_camera_start_recording (camera, &error);
    total = 0;

    g_timer_start (timer);
    for (guint i = 0; i < n_frames; i++) {
        if (trigger_source == UCA_CAMERA_TRIGGER_SOURCE_SOFTWARE)
            uca_camera_trigger (camera, &error);

        if (!uca_camera_grab (camera, buffer, &error))
            g_warning ("Data stream ended");

        if (error != NULL) {
            g_warning ("Error grabbing frame %02i/%i: `%s'", i, n_frames, error->message);
            g_error_free (error);
            error = NULL;
        }
        else {
            total++;
        }
    }
    g_timer_stop (timer);

    uca_camera_stop_recording (camera, &error);
    return total;
}

static guint
grab_frames_readout (UcaCamera *camera, gpointer buffer, guint n_frames, UcaCameraTriggerSource trigger_source, GTimer *timer)
{
    GError *error = NULL;
    guint recorded_frames = 0;

    g_object_set(camera, "trigger-source", trigger_source, NULL);
    uca_camera_start_recording(camera, &error);

    do {
        g_object_get (camera, "recorded-frames", &recorded_frames, NULL);
    }while(recorded_frames < n_frames);

    uca_camera_stop_recording(camera, &error);

    // This sets is-readout in camera object. So the grab is sequential from image 1
    uca_camera_start_readout(camera, &error);

    g_timer_start (timer);
    
    /*This is required because its possible that the camera has recorded frames more
    than what is required. Index starts at 1 for consistency (camRAM index start from 1)*/
    for(int i = 1; i <= n_frames; i++) {
        uca_camera_grab (camera, buffer, &error);
        if(error != NULL){
            g_warning("There was an error grabbing frame %d during readout from camRAM",i+1);
            error = NULL;
        }
    }

    g_timer_stop (timer);
    uca_camera_stop_readout(camera, &error);

    return n_frames;
}

static void
grab_callback (gpointer data, gpointer user_data)
{
    static GMutex mutex;
    guint *n_acquired_frames = user_data;

    g_mutex_lock (&mutex);
    *n_acquired_frames += 1;
    g_mutex_unlock (&mutex);
}

static guint
grab_frames_async (UcaCamera *camera, gpointer buffer, guint n_frames, UcaCameraTriggerSource trigger_source, GTimer *timer)
{
    GError *error = NULL;
    guint n_acquired_frames = 0;

    g_object_set (camera, "trigger-source", trigger_source, NULL);
    uca_camera_set_grab_func (camera, grab_callback, &n_acquired_frames);
    g_timer_start (timer);
    uca_camera_start_recording (camera, &error);

    /*
     * Behold! Spinlooping is probably a bad idea but nowadays single core
     * machines are relatively rare.
     */
    while (n_acquired_frames < n_frames)
        ;

    uca_camera_stop_recording (camera, &error);
    g_timer_stop (timer);
    return n_frames;
}

static void
benchmark_method (UcaCamera *camera, gpointer buffer, GrabFrameFunc func, Options *options, UcaCameraTriggerSource trigger_source)
{
    GTimer *timer;
    gdouble fps;
    gdouble bandwidth;
    gdouble total_time = 0.0;
    guint num_frames_total;
    guint num_frames_acquired = 0;
    GError *error = NULL;

    timer = g_timer_new ();
    g_assert_no_error (error);

    if (func == grab_frames_sync)
        g_print ("sync   ");
    else if (func == grab_frames_readout)
        g_print ("rout   ");
    else
        g_print ("async  ");

    switch (trigger_source) {
        case UCA_CAMERA_TRIGGER_SOURCE_AUTO:
            g_print ("auto  ");
            break;
        case UCA_CAMERA_TRIGGER_SOURCE_SOFTWARE:
            g_print ("soft  ");
            break;
        case UCA_CAMERA_TRIGGER_SOURCE_EXTERNAL:
            g_print ("ext   ");
            break;
    }

    for (guint run = 0; run < options->n_runs; run++) {
        g_print ("%i/%i", run + 1, options->n_runs);
        g_message ("Start run %i of %i", run + 1, options->n_runs);

        num_frames_acquired += func (camera, buffer, options->n_frames, trigger_source, timer);

        total_time += g_timer_elapsed (timer, NULL);
        g_print ("\b\b\b");
    }

    g_assert_no_error (error);

    fps = options->n_runs * options->n_frames / total_time;
    bandwidth = options->n_bytes * fps / 1024 / 1024;
    num_frames_total = options->n_runs * options->n_frames;
    g_print (" %8.2f Hz  %8.2f MB/s  %d/%d acquired (%3.2f%% dropped)\n",
             fps, bandwidth, num_frames_acquired, num_frames_total,
             100 * (num_frames_total - num_frames_acquired) / ((gdouble) num_frames_total));

    g_timer_destroy (timer);
}

static void
benchmark (UcaCamera *camera, Options *options)
{
    gchar *name;
    guint sensor_width;
    guint sensor_height;
    guint roi_width;
    guint roi_height;
    guint bits;
    gsize n_bytes_per_pixel;
    gdouble exposure_time;
    gpointer buffer;

    g_object_get (G_OBJECT (camera),
                  "name", &name,
                  "sensor-width", &sensor_width,
                  "sensor-height", &sensor_height,
                  "sensor-bitdepth", &bits,
                  "roi-width", &roi_width,
                  "roi-height", &roi_height,
                  "exposure-time", &exposure_time,
                  NULL);

    g_debug ("Benchmarking %s [width=%i height=%i roiwidth=%i roiheight=%i exposure_time=%fs]",
             name, sensor_width, sensor_height, roi_width, roi_height, exposure_time);

    g_free (name);

    /* Synchronous frame acquisition */
    n_bytes_per_pixel = bits > 8 ? 2 : 1;
    options->n_bytes = roi_width * roi_height * n_bytes_per_pixel;
    buffer = g_malloc0 (options->n_bytes);

    g_object_set (G_OBJECT(camera), "transfer-asynchronously", FALSE, NULL);

    if(options->test_readout)
        benchmark_method (camera, buffer, grab_frames_readout, options, UCA_CAMERA_TRIGGER_SOURCE_AUTO);
    else
        benchmark_method (camera, buffer, grab_frames_sync, options, UCA_CAMERA_TRIGGER_SOURCE_AUTO);

    if (options->test_software)
        benchmark_method (camera, buffer, grab_frames_sync, options, UCA_CAMERA_TRIGGER_SOURCE_SOFTWARE);

    if (options->test_external)
        benchmark_method (camera, buffer, grab_frames_sync, options, UCA_CAMERA_TRIGGER_SOURCE_EXTERNAL);

    /* Asynchronous frame acquisition */
    if (options->test_async) {
        g_object_set (G_OBJECT(camera), "transfer-asynchronously", TRUE, NULL);

        benchmark_method (camera, buffer, grab_frames_async, options, UCA_CAMERA_TRIGGER_SOURCE_AUTO);

        if (options->test_software)
            benchmark_method (camera, buffer, grab_frames_async, options, UCA_CAMERA_TRIGGER_SOURCE_SOFTWARE);

        if (options->test_external)
            benchmark_method (camera, buffer, grab_frames_async, options, UCA_CAMERA_TRIGGER_SOURCE_EXTERNAL);
    }

    g_free (buffer);
}

int
main (int argc, char *argv[])
{
    GOptionContext *context;
    UcaPluginManager *manager;
    GIOChannel *log_channel;
    GError *error = NULL;

    static Options options = {
        .n_frames = 1000,
        .n_runs = 3,
        .test_async = FALSE,
        .test_software = FALSE,
        .test_external = FALSE,
        .test_readout = FALSE,
    };

    static GOptionEntry entries[] = {
        { "num-frames", 'n', 0, G_OPTION_ARG_INT, &options.n_frames, "Number of frames per run", "N" },
        { "num-runs", 'r', 0, G_OPTION_ARG_INT, &options.n_runs, "Number of runs", "N" },
        { "async", 0, 0, G_OPTION_ARG_NONE, &options.test_async, "Test asynchronous mode", NULL },
        { "software", 0, 0, G_OPTION_ARG_NONE, &options.test_software, "Test software trigger mode", NULL },
        { "external", 0, 0, G_OPTION_ARG_NONE, &options.test_external, "Test external trigger mode", NULL },
        { "readout", 0, 0, G_OPTION_ARG_NONE, &options.test_readout, "Test readout from camRAM instead of sync acquisition", NULL},
        { NULL }
    };

    (void) signal (SIGINT, sigint_handler);

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init ();
#endif

    manager = uca_plugin_manager_new ();
    context = uca_common_context_new (manager);
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_printerr ("Failed parsing arguments: %s\n", error->message);
        goto cleanup_manager;
    }

    if (argc < 2) {
        gchar *help;

        help = g_option_context_get_help (context, TRUE, NULL);
        g_print ("%s\n", help);
        g_free (help);
        goto cleanup_manager;
    }

    log_channel = g_io_channel_new_file ("benchmark.log", "a+", &error);
    g_assert_no_error (error);
    g_log_set_handler (NULL, G_LOG_LEVEL_MASK, log_handler, log_channel);

    camera = uca_common_get_camera (manager, argv[argc - 1], &error);

    if (camera == NULL) {
        g_printerr ("Initialization: %s\n", error->message);
        goto cleanup_manager;
    }

    benchmark (camera, &options);

    g_io_channel_shutdown (log_channel, TRUE, &error);
    g_assert_no_error (error);
    g_object_unref (camera);

cleanup_manager:
    g_option_context_free (context);
    g_object_unref (manager);

    return 0;
}
