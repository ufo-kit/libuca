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

#include "config.h"

#include <glib-object.h>
#include <stdio.h>
#include <stdlib.h>
#include "uca-plugin-manager.h"
#include "uca-camera.h"
#include "uca-ring-buffer.h"
#include "common.h"

#ifdef HAVE_LIBTIFF
#include <tiffio.h>
#endif


typedef struct {
    gint n_frames;
    gdouble duration;
    gdouble exposure_time;
    gchar *filename;
#ifdef HAVE_LIBTIFF
    gboolean write_tiff;
#endif
} Options;


static guint
get_bytes_per_pixel (guint bits_per_pixel)
{
    return bits_per_pixel > 8 ? 2 : 1;
}

#ifdef HAVE_LIBTIFF
static void
write_tiff (UcaRingBuffer *buffer,
            Options *opts,
            guint width,
            guint height,
            guint bits_per_pixel)
{
    TIFF *tif;
    guint32 rows_per_strip;
    guint n_frames;
    guint bits_per_sample;
    gsize bytes_per_pixel;

    if (opts->filename)
        tif = TIFFOpen (opts->filename, "w");
    else
        tif = TIFFOpen ("frames.tif", "w");

    n_frames = uca_ring_buffer_get_num_blocks (buffer);
    rows_per_strip = TIFFDefaultStripSize (tif, (guint32) - 1);
    bytes_per_pixel = get_bytes_per_pixel (bits_per_pixel);
    bits_per_sample = bits_per_pixel > 8 ? 16 : 8;

    /* Write multi page TIFF file */
    TIFFSetField (tif, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);

    for (guint i = 0; i < n_frames; i++) {
        gpointer data;
        gsize offset = 0;

        data = uca_ring_buffer_get_read_pointer (buffer);

        TIFFSetField (tif, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField (tif, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField (tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
        TIFFSetField (tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        TIFFSetField (tif, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField (tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField (tif, TIFFTAG_ROWSPERSTRIP, rows_per_strip);
        TIFFSetField (tif, TIFFTAG_PAGENUMBER, i, n_frames);

        for (guint y = 0; y < height; y++, offset += width * bytes_per_pixel)
            TIFFWriteScanline (tif, data + offset, y, 0);

        TIFFWriteDirectory (tif);
    }

    TIFFClose (tif);
}
#endif

static void
write_raw (UcaRingBuffer *buffer,
           Options *opts)
{
    guint n_frames;
    gsize size;

    size = uca_ring_buffer_get_block_size (buffer);
    n_frames = uca_ring_buffer_get_num_blocks (buffer);

    for (gint i = 0; i < n_frames; i++) {
        FILE *fp;
        gchar *filename;
        gpointer data;

        if (opts->filename)
            filename = g_strdup_printf ("%s-%08i.raw", opts->filename, i);
        else
            filename = g_strdup_printf ("frame-%08i.raw", i);

        fp = fopen(filename, "wb");
        data = uca_ring_buffer_get_read_pointer (buffer);

        fwrite (data, size, 1, fp);
        fclose (fp);
        g_free (filename);
    }
}

static GError *
record_frames (UcaCamera *camera, Options *opts)
{
    guint roi_width;
    guint roi_height;
    guint bits;
    guint pixel_size;
    gsize size;
    gint n_frames;
    guint n_allocated;
    GTimer *timer;
    UcaRingBuffer *buffer;
    GError *error = NULL;
    gdouble last_printed;

    g_object_get (G_OBJECT (camera),
                  "roi-width", &roi_width,
                  "roi-height", &roi_height,
                  "sensor-bitdepth", &bits,
                  NULL);

    pixel_size = get_bytes_per_pixel (bits);
    size = roi_width * roi_height * pixel_size;
    n_allocated = opts->n_frames > 0 ? opts->n_frames : 256;
    buffer = uca_ring_buffer_new (size, n_allocated);
    timer = g_timer_new();

    g_print("Start recording: %ix%i at %i bits/pixel\n",
            roi_width, roi_height, bits);

    uca_camera_start_recording(camera, &error);

    if (error != NULL)
        return error;

    n_frames = 0;
    g_timer_start(timer);
    last_printed = 0.0;

    while (1) {
        gdouble elapsed;

        uca_camera_grab (camera, uca_ring_buffer_get_write_pointer (buffer), &error);
        uca_ring_buffer_write_advance (buffer);

        if (error != NULL)
            return error;

        n_frames++;
        elapsed = g_timer_elapsed (timer, NULL);

        if (n_frames == opts->n_frames || (opts->duration > 0.0 && elapsed >= opts->duration))
            break;

        if (elapsed - last_printed >= 1.0) {
            g_print ("Recorded %i frames at %.2f frames/s\n",
                     n_frames, n_frames / elapsed);
            last_printed = elapsed;
        }
    }

    g_print ("Stop recording: %3.2f frames/s\n",
             n_frames / g_timer_elapsed (timer, NULL));

    uca_camera_stop_recording (camera, &error);

#ifdef HAVE_LIBTIFF
    if (opts->write_tiff)
        write_tiff (buffer, opts, roi_width, roi_height, bits);
    else
        write_raw (buffer, opts);
#else
    write_raw (buffer, opts);
#endif

    g_object_unref (buffer);
    g_timer_destroy (timer);

    return error;
}

int
main (int argc, char *argv[])
{
    GOptionContext *context;
    UcaPluginManager *manager;
    UcaCamera *camera;
    GError *error = NULL;

    static Options opts = {
        .n_frames = -1,
        .duration = -1.0,
        .exposure_time = 0.001,
        .filename = NULL,
#ifdef HAVE_LIBTIFF
        .write_tiff = FALSE,
#endif
    };

    static GOptionEntry entries[] = {
        { "num-frames", 'n', 0, G_OPTION_ARG_INT, &opts.n_frames, "Number of frames to acquire", "N" },
        { "duration", 'd', 0, G_OPTION_ARG_DOUBLE, &opts.duration, "Duration in seconds", NULL },
        { "exposure-time", 'e', 0, G_OPTION_ARG_DOUBLE, &opts.exposure_time, "Exposure time in seconds", NULL },
        { "output", 'o', 0, G_OPTION_ARG_STRING, &opts.filename, "Output file name", "FILE" },
#ifdef HAVE_LIBTIFF
        { "write-tiff", 't', 0, G_OPTION_ARG_NONE, &opts.write_tiff, "Write as TIFF", NULL },
#endif
        { NULL }
    };

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init();
#endif

    manager = uca_plugin_manager_new ();
    context = uca_option_context_new (manager);
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print ("Failed parsing arguments: %s\n", error->message);
        goto cleanup_manager;
    }

    if (argc < 2) {
        g_print ("%s\n", g_option_context_get_help (context, TRUE, NULL));
        goto cleanup_manager;
    }

    if (opts.n_frames < 0 && opts.duration < 0.0) {
        g_print ("You must specify at least one of --num-frames and --output.\n");
        goto cleanup_manager;
    }

    camera = uca_plugin_manager_get_camera (manager, argv[1], &error, NULL);

    if (camera == NULL) {
        g_print ("Error during initialization: %s\n", error->message);
        goto cleanup_camera;
    }

    g_object_set (camera, "exposure-time", opts.exposure_time, NULL);
    error = record_frames (camera, &opts);

    if (error != NULL)
        g_print ("Error: %s\n", error->message);

    g_option_context_free (context);

cleanup_camera:
    g_object_unref (camera);

cleanup_manager:
    g_object_unref (manager);

    return error != NULL ? 1 : 0;
}
