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
#include <math.h>
#include "uca-plugin-manager.h"
#include "uca-camera.h"
#include "uca-ring-buffer.h"
#include "common.h"

#ifdef HAVE_LIBTIFF
#include <tiffio.h>
#endif


typedef struct {
    gint n_frames;
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

static guint
count_format_specifiers (const gchar *template)
{
    GRegex *regex;
    GMatchInfo *info;
    gint count = 0;

    regex = g_regex_new ("%[0-9]+[ui]", 0, 0, NULL);
    g_regex_match (regex, template, 0, &info);

    while (g_match_info_matches (info)) {
        count++;
        g_match_info_next (info, NULL);
    }

    g_match_info_free (info);
    g_regex_unref (regex);

    return count > 0 ? (guint) count : 0;
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

    if (count_format_specifiers (opts->filename) > 0)
        g_warning ("Can only write multi-page TIFF, format specifier is ignored.\n");

    tif = TIFFOpen (opts->filename, "w");
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
    guint num_format_specifiers;
    gboolean multiple_files;
    FILE *fp;

    size = uca_ring_buffer_get_block_size (buffer);
    n_frames = uca_ring_buffer_get_num_blocks (buffer);
    num_format_specifiers = count_format_specifiers (opts->filename);

    if (num_format_specifiers > 1) {
        g_printerr ("Can only use zero or one format specifiers. Aborting write.\n");
        return;
    }

    multiple_files = num_format_specifiers == 1;

    if (!multiple_files)
        fp = fopen (opts->filename, "wb");

    for (gint i = 0; i < n_frames; i++) {
        gpointer data;

        if (multiple_files) {
            gchar *filename;
            filename = g_strdup_printf (opts->filename, i);
            fp = fopen (filename, "wb");
            g_free (filename);
        }

        data = uca_ring_buffer_get_read_pointer (buffer);
        fwrite (data, size, 1, fp);

        if (multiple_files)
            fclose (fp);
    }

    if (!multiple_files)
        fclose (fp);
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
    guint n_digits;
    gchar *fmt_string;
    GTimer *timer;
    gdouble elapsed;
    UcaRingBuffer *buffer;
    GError *error = NULL;

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

    g_print ("Acquiring %i images at %ix%i with %i bits per pixel\n",
             opts->n_frames, roi_width, roi_height, bits);

    uca_camera_start_recording(camera, &error);

    if (error != NULL)
        return error;

    n_frames = 0;
    n_digits = floor (log10 (abs (opts->n_frames))) + 1;
    fmt_string = g_strdup_printf ("\33[2K\r%%%ii/%%i images acquired ...", n_digits);
    g_timer_start(timer);

    while (1) {
        uca_camera_grab (camera, uca_ring_buffer_get_write_pointer (buffer), &error);
        uca_ring_buffer_write_advance (buffer);

        if (error != NULL)
            return error;

        g_print (fmt_string, ++n_frames, opts->n_frames);

        if (n_frames == opts->n_frames)
            break;
    }

    elapsed = g_timer_elapsed (timer, NULL);
    g_print ("\nAcquired %3.2f images/s = %3.2f ms/image = %.4f MB/s\n",
             n_frames / elapsed, elapsed / n_frames * 1000.,
             n_frames * size / 1024. / 1024. / elapsed);

    uca_camera_stop_recording (camera, &error);

    if (opts->filename == NULL)
        g_print ("No filename given, not writing data.\n");
    else {
#ifdef HAVE_LIBTIFF
        if (opts->write_tiff)
            write_tiff (buffer, opts, roi_width, roi_height, bits);
        else
            write_raw (buffer, opts);
#else
        write_raw (buffer, opts);
#endif
    }

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
        .filename = NULL,
#ifdef HAVE_LIBTIFF
        .write_tiff = FALSE,
#endif
    };

    static GOptionEntry entries[] = {
        { "num-frames", 'n', 0, G_OPTION_ARG_INT, &opts.n_frames, "Number of frames to acquire", "N" },
        { "output", 'o', 0, G_OPTION_ARG_STRING, &opts.filename, "Output file name template", "FILE" },
#ifdef HAVE_LIBTIFF
        { "write-tiff", 't', 0, G_OPTION_ARG_NONE, &opts.write_tiff, "Write as TIFF", NULL },
#endif
        { NULL }
    };

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init();
#endif

    manager = uca_plugin_manager_new ();
    context = uca_common_context_new (manager);
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print ("Failed parsing arguments: %s\n", error->message);
        goto cleanup_manager;
    }

    if (argc < 2) {
        g_print ("%s\n", g_option_context_get_help (context, TRUE, NULL));
        goto cleanup_manager;
    }

    if (opts.n_frames < 0) {
        g_print ("You must specify the number of acquired frames with -n/--num-frames.\n");
        goto cleanup_manager;
    }

    camera = uca_common_get_camera (manager, argv[argc - 1], &error);

    if (camera == NULL) {
        g_print ("Error during initialization: %s\n", error->message);
        goto cleanup_manager;
    }

    error = record_frames (camera, &opts);

    if (error != NULL)
        g_print ("Error: %s\n", error->message);

    g_option_context_free (context);
    g_object_unref (camera);

cleanup_manager:
    g_object_unref (manager);

    return error != NULL ? 1 : 0;
}
