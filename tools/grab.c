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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "uca-plugin-manager.h"
#include "uca-camera.h"
#include "ring-buffer.h"

#ifdef HAVE_LIBTIFF
#include <tiffio.h>
#endif


typedef struct {
    gint n_frames;
    gdouble duration;
} Options;


static gchar *
get_camera_list (void)
{
    GList *types;
    GString *str;
    UcaPluginManager *manager;

    manager = uca_plugin_manager_new ();
    types = uca_plugin_manager_get_available_cameras (manager);
    str = g_string_new ("[ ");

    if (types != NULL) {
        for (GList *it = g_list_first (types); it != NULL; it = g_list_next (it)) {
            gchar *name = (gchar *) it->data;

            if (g_list_next (it) == NULL)
                g_string_append_printf (str, "%s ]", name);
            else
                g_string_append_printf (str, "%s, ", name);
        }
    }
    else {
        g_string_append (str, "]");
    }

    g_object_unref (manager);
    return g_string_free (str, FALSE);
}

static guint
get_bytes_per_pixel (guint bits_per_pixel)
{
    return bits_per_pixel == 8 ? 1 : 2;
}

#ifdef HAVE_LIBTIFF
static void
write_tiff (RingBuffer *buffer,
            guint width,
            guint height,
            guint bits_per_pixel)
{
    TIFF *tif;
    guint32 rows_per_strip;
    gpointer data;
    guint n_frames;
    gsize bytes_per_pixel;

    tif = TIFFOpen ("frames.tif", "w");
    n_frames = ring_buffer_get_num_blocks (buffer);
    rows_per_strip = TIFFDefaultStripSize (tif, (guint32) - 1);
    bytes_per_pixel = get_bytes_per_pixel (bits_per_pixel);

    /* Write multi page TIFF file */
    TIFFSetField (tif, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);

    for (guint i = 0; i < n_frames; i++) {
        gpointer data;
        gsize offset = 0;

        data = ring_buffer_get_pointer (buffer, i);

        TIFFSetField (tif, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField (tif, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField (tif, TIFFTAG_BITSPERSAMPLE, bits_per_pixel);
        TIFFSetField (tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        TIFFSetField (tif, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField (tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField (tif, TIFFTAG_ROWSPERSTRIP, rows_per_strip);
        TIFFSetField (tif, TIFFTAG_PAGENUMBER, i, n_frames);
        /* start = ((gfloat *) data) + i * width * height; */

        /* for (guint y = 0; y < height; y++, start += width) */
        /*     TIFFWriteScanline (tif, start, y, 0); */

        for (guint y = 0; y < height; y++, offset += width * bytes_per_pixel)
            TIFFWriteScanline (tif, data + offset, y, 0);

        TIFFWriteDirectory (tif);
    }

    TIFFClose (tif);
}
#else
static void
write_raw (RingBuffer *buffer)
{
    guint n_frames;
    gsize size;

    size = ring_buffer_get_block_size (buffer);
    n_frames = ring_buffer_get_num_blocks (buffer);

    for (gint i = 0; i < n_frames; i++) {
        FILE *fp;
        gchar *filename;
        gpointer data;

        filename = g_strdup_printf ("frame-%08i.raw", i);
        fp = fopen(filename, "wb");
        data = ring_buffer_get_pointer (buffer, i);

        fwrite (data, size, 1, fp);
        fclose (fp);
        g_free (filename);
    }
}
#endif

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
    RingBuffer *buffer;
    GError *error = NULL;

    g_object_get (G_OBJECT (camera),
                  "roi-width", &roi_width,
                  "roi-height", &roi_height,
                  "sensor-bitdepth", &bits,
                  NULL);

    pixel_size = get_bytes_per_pixel (bits);
    size = roi_width * roi_height * pixel_size;
    n_allocated = opts->n_frames > 0 ? opts->n_frames : 256;
    buffer = ring_buffer_new (size, n_allocated);
    timer = g_timer_new();

    g_print("Start recording: %ix%i at %i bits/pixel\n",
            roi_width, roi_height, bits);

    uca_camera_start_recording(camera, &error);

    if (error != NULL)
        return error;

    n_frames = 0;
    g_timer_start(timer);

    while (n_frames < opts->n_frames || g_timer_elapsed (timer, NULL) < opts->duration) {
        uca_camera_grab (camera, ring_buffer_get_current_pointer (buffer), &error);
        ring_buffer_proceed (buffer);

        if (error != NULL)
            return error;

        n_frames++;
    }

    g_print ("Stop recording: %3.2f frames/s\n",
             n_frames / g_timer_elapsed (timer, NULL));

    uca_camera_stop_recording (camera, &error);

#ifdef HAVE_LIBTIFF
    write_tiff (buffer, roi_width, roi_height, bits);
    g_print ("writing tiff\n");
#else
    write_raw (buffer);
    g_print ("writing raw\n");
#endif

    ring_buffer_free (buffer);
    g_timer_destroy (timer);

    return error;
}

int
main (int argc, char *argv[])
{
    GOptionContext *context;
    UcaPluginManager *manager;
    UcaCamera *camera;
    gchar *cam_list;
    GError *error = NULL;

    static Options opts = {
        .n_frames = -1,
        .duration = -1.0
    };

    static GOptionEntry entries[] = {
        { "num-frames", 'n', 0, G_OPTION_ARG_INT, &opts.n_frames, "Number of frames to acquire", "N" },
        { "duration", 'd', 0, G_OPTION_ARG_DOUBLE, &opts.duration, "Duration in seconds", NULL },
        { NULL }
    };

    g_type_init();

    cam_list = get_camera_list ();
    context = g_option_context_new (cam_list);
    g_option_context_add_main_entries (context, entries, NULL);
    g_free (cam_list);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print ("Failed parsing arguments: %s\n", error->message);
        exit (1);
    }

    if (argc < 2) {
        g_print ("%s\n", g_option_context_get_help (context, TRUE, NULL));
        exit (0);
    }

    manager = uca_plugin_manager_new ();
    camera = uca_plugin_manager_get_camera (manager, argv[1], &error, NULL);

    if (camera == NULL) {
        g_print ("Error during initialization: %s\n", error->message);
        exit (1);
    }

    error = record_frames (camera, &opts);

    if (error != NULL)
        g_print ("Error: %s\n", error->message);

    g_object_unref (camera);
    return error != NULL ? 1 : 0;
}
