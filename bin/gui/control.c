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

#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>

#include "config.h"
#include "ring-buffer.h"
#include "uca-camera.h"
#include "uca-plugin-manager.h"
#include "egg-property-tree-view.h"
#include "egg-histogram-view.h"

typedef enum {
    IDLE,
    RUNNING,
    RECORDING
} State;

typedef struct {
    UcaCamera   *camera;
    GtkWidget   *main_window;
    GdkPixbuf   *pixbuf;
    GtkWidget   *image;
    GtkWidget   *start_button;
    GtkWidget   *stop_button;
    GtkWidget   *record_button;
    GtkWidget   *download_button;
    GtkWidget   *acquisition_expander;
    GtkWidget   *properties_expander;
    GtkWidget   *zoom_box;
    GtkWidget   *colormap_box;
    GtkLabel    *mean_label;
    GtkLabel    *sigma_label;
    GtkLabel    *max_label;
    GtkLabel    *min_label;

    GtkDialog       *download_dialog;
    GtkProgressBar  *download_progressbar;
    GtkAdjustment   *download_adjustment;
    GtkAdjustment   *count;
    GtkAdjustment   *hadjustment, *vadjustment;

    GtkWidget       *histogram_view;
    GtkToggleButton *histogram_button;
    GtkToggleButton *log_button;
    GtkAdjustment   *frame_slider;

    RingBuffer  *buffer;
    guchar      *pixels;
    gint         display_width, display_height;
    gint         page_width, page_height;
    gint         colormap;
    gdouble      zoom_factor;
    State        state;
    guint        n_recorded;
    gboolean     data_in_camram;

    gint         timestamp;
    gint         width, height;
    gint         pixel_size;
} ThreadData;

static UcaPluginManager *plugin_manager;
static gsize mem_size = 2048; 

static void
up_and_down_scale (ThreadData *data, gpointer buffer)
{
    gdouble min;
    gdouble max;
    gdouble factor;
    gdouble dval;
    gboolean do_log;
    guint8 *output;
    gint i = 0;
    gint zoom;
    gint stride;
    gint offset;
    gint start_wval; 
    gint start_hval;

    egg_histogram_get_range (EGG_HISTOGRAM_VIEW (data->histogram_view), &min, &max);
    factor = 255.0 / (max - min);
    output = data->pixels;
    zoom = (gint) data->zoom_factor;
    stride = (gint) 1 / zoom;
    do_log = gtk_toggle_button_get_active (data->log_button);

    if (data->state == RUNNING) {
        gint page_width = gtk_adjustment_get_page_size (GTK_ADJUSTMENT (data->hadjustment));
        gint page_height = gtk_adjustment_get_page_size (GTK_ADJUSTMENT (data->vadjustment));
        start_wval = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->hadjustment));
        start_hval = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->vadjustment));
        data->page_width = (page_width + start_wval);
        data->page_height = (page_height + start_hval);
    }
    else {
        start_wval = 0;
        start_hval = 0;
        data->page_width = data->display_width;
        data->page_height = data->display_height;
    }

    if (data->pixel_size == 1) {
        guint8 *input = (guint8 *) buffer;

        for (gint y = 0; y < data->display_height; y++) {
            if (zoom <= 1){ 
                offset = y * stride * data->width;
            }

            for (gint x = 0; x < data->display_width; x++) {
                if (zoom <= 1)
                    offset += stride;
                else
                    offset = ((gint) (y / zoom) * data->width) + ((gint) (x / zoom));

                if (y >= start_hval && y < data->page_height) {   
                    if (x >= start_wval && x < data->page_width) {

                        if (do_log)
                            dval = log ((input[offset] - min) * factor);
                        else
                            dval = (input[offset] - min) * factor;
                    }
                }
                guchar val = (guchar) CLAMP(dval, 0.0, 255.0);

                if (data->colormap == 1) {
                    output[i++] = val;
                    output[i++] = val;
                    output[i++] = val;
                } 
                else {
                    val = (float) val; 
                    float red = 0;
                    float green = 0;
                    float blue = 0;

                    if (val == 255) { 
                        red = 255;
                        green = 255;
                        blue = 255; 
                    }
                    else if (val == 0) { 
                    }
                    else if (val <= 31.875) {  
                        blue = 255 - 4 * (31.875 - val); 
                    }
                    else if (val <= 95.625) {
                        green = 255 - 4 * (95.625 - val);  
                        blue = 255;
                    }    
                    else if (val <= 159.375) { 
                        red = 255 - 4 * (159.375 - val);  
                        green = 255;
                        blue = 255 + 4 * (95.625 - val); 
                    }
                    else if (val <= 223.125) { 
                        red = 255;
                        green = 255 + 4 * (159.375 - val); 
                    } 
                    else {   
                        red = 255 + 4 * (223.125 - val); 
                    }
                 
                    output[i++] = (guchar) red;
                    output[i++] = (guchar) green;
                    output[i++] = (guchar) blue; 
                }
            }
        }
    }
    else if (data->pixel_size == 2) {
        guint16 *input = (guint16 *) buffer;

        for (gint y = 0; y < data->display_height; y++) {
            if (zoom <= 1){ 
                offset = y * stride * data->width;
            }

            for (gint x = 0; x < data->display_width; x++) {
                if (zoom <= 1)
                    offset += stride;
                else
                    offset = ((gint) (y / zoom) * data->width) + ((gint) (x / zoom));

                if (y >= start_hval && y < data->page_height)  {   
                    if (x >= start_wval && x < data->page_width) {

                        if (do_log)
                            dval = log ((input[offset] - min) * factor);
                        else
                            dval = (input[offset] - min) * factor;
                    }
                }
                guchar val = (guchar) CLAMP(dval, 0.0, 255.0);

                if (data->colormap == 1) {
                    output[i++] = val;
                    output[i++] = val;
                    output[i++] = val;
                } 
                else {
                    val = (float) val; 
                    float red = 0;
                    float green = 0;
                    float blue = 0;
  
                    if (val == 255) { 
                        red = 65535;
                        green = 65535;
                        blue = 65535; 
                    }
                    else if (val == 0) { 
                    }
                    else if (val <= 31.875) {  
                        blue = (255 - 4 * (31.875 - val)) * 257; 
                    }
                    else if (val <= 95.625) {
                        green = (255 - 4 * (95.625 - val)) * 257;  
                        blue = 65535;
                    }    
                    else if (val <= 159.375) { 
                        red = (255 - 4 * (159.375 - val)) * 257;  
                        green = 65535;
                        blue = (255 + 4 * (95.625 - val)) * 257; 
                    }
                    else if (val <= 223.125) { 
                        red = 65535;
                        green = (255 + 4 * (159.375 - val)) * 257; 
                    } 
                    else {   
                        red = (255 + 4 * (223.125 - val)) * 257; 
                    }
                                
                    output[i++] = (guchar) red;
                    output[i++] = (guchar) green;
                    output[i++] = (guchar) blue; 
                }   
            }
        }
    }
}

static void
get_statistics (ThreadData *data, gdouble *mean, gdouble *sigma, guint *_max, guint *_min)
{
    gdouble sum = 0.0;
    gdouble squared_sum = 0.0;
    guint min = G_MAXUINT;
    guint max = 0;
    guint n = data->width * data->height;

    if (data->pixel_size == 1) {
        guint8 *input = (guint8 *) ring_buffer_get_current_pointer (data->buffer);

        for (gint i = 0; i < n; i++) {
            guint8 val = input[i];

            if (val > max)
                max = val;

            if (val < min)
                min = val;

            sum += val;
            squared_sum += val * val;
        }
    }
    else {
        guint16 *input = (guint16 *) ring_buffer_get_current_pointer (data->buffer);

        for (gint i = 0; i < n; i++) {
            guint16 val = input[i];

            if (val > max)
                max = val;

            if (val < min)
                min = val;

            sum += val;
            squared_sum += val * val;
        }
    }

    if (gtk_toggle_button_get_active (data->log_button)) {
        *mean = log(sum/n);
        *sigma = log (sqrt((squared_sum - sum*sum/n) / (n - 1)));
    }
    else {
        *mean = sum / n;
        *sigma = sqrt ((squared_sum - sum*sum/n) / (n - 1));
    }

    *_min = min;
    *_max = max;
}

static void
update_pixbuf (ThreadData *data)
{
    GString *string;
    gdouble mean;
    gdouble sigma;
    guint min;
    guint max;

    gdk_flush ();
    gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), data->pixbuf);
    gtk_widget_queue_draw_area (data->image, 0, 0, data->display_width, data->display_height);

    egg_histogram_view_update (EGG_HISTOGRAM_VIEW (data->histogram_view),
                               ring_buffer_get_current_pointer (data->buffer));

    get_statistics (data, &mean, &sigma, &max, &min);
    string = g_string_new_len (NULL, 32);

    g_string_printf (string, "\u03bc = %3.2f", mean);
    gtk_label_set_text (data->mean_label, string->str);

    g_string_printf (string, "\u03c3 = %3.2f", sigma);
    gtk_label_set_text (data->sigma_label, string->str);

    g_string_printf (string, "min = %i", min);
    gtk_label_set_text (data->min_label, string->str);

    g_string_printf (string, "max = %i", max);
    gtk_label_set_text (data->max_label, string->str);

    g_string_free (string, TRUE);

    if (gtk_toggle_button_get_active (data->histogram_button))
        gtk_widget_queue_draw (data->histogram_view);
}

static void
update_pixbuf_dimensions (ThreadData *data)
{
    if (data->pixbuf != NULL)
        g_object_unref (data->pixbuf);

    data->display_width = (gint) data->width * data->zoom_factor;
    data->display_height = (gint) data->height * data->zoom_factor;

    data->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, data->display_width, data->display_height);
    data->pixels = gdk_pixbuf_get_pixels (data->pixbuf);
    gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), data->pixbuf);
}

static void
print_and_free_error (GError **error)
{
    g_printerr ("%s\n", (*error)->message);
    g_error_free (*error);
    *error = NULL;
}

static void
set_tool_button_state (ThreadData *data)
{
    gtk_widget_set_sensitive (data->start_button,
                              data->state == IDLE);
    gtk_widget_set_sensitive (data->stop_button,
                              data->state == RUNNING || data->state == RECORDING);
    gtk_widget_set_sensitive (data->record_button,
                              data->state == IDLE);
    gtk_widget_set_sensitive (data->download_button,
                              data->data_in_camram);
    gtk_widget_set_sensitive (data->acquisition_expander,
                              data->state == IDLE);
    gtk_widget_set_sensitive (data->properties_expander,
                              data->state == IDLE);
    gtk_widget_set_sensitive (data->zoom_box,
                              data->state == IDLE);
    gtk_widget_set_sensitive (data->colormap_box,
                              data->state == IDLE);
}

static gpointer
preview_frames (void *args)
{
    ThreadData *data = (ThreadData *) args;
    gint counter = 0;
    GError *error = NULL;;

    while (data->state == RUNNING) {
        gpointer buffer;

        buffer = ring_buffer_get_current_pointer (data->buffer);
        uca_camera_trigger (data->camera, &error);
        uca_camera_grab (data->camera, buffer, &error);

        if (error == NULL) {
            up_and_down_scale (data, buffer);

            gdk_threads_enter ();
            update_pixbuf (data);
            gdk_threads_leave ();

            counter++;
        }
        else
            print_and_free_error (&error);
    }
    return NULL;
}

static gpointer
record_frames (gpointer args)
{
    ThreadData *data;
    gpointer buffer;
    guint n_max;
    guint n_frames = 0;
    GError *error = NULL;

    data = (ThreadData *) args;
    ring_buffer_reset (data->buffer);

    data->n_recorded = 0;
    n_max = (guint) gtk_adjustment_get_value (data->count);

    while (1) {
        if (data->state != RECORDING)
            break;

        if (n_max > 0 && n_frames >= n_max)
            break;

        buffer = ring_buffer_get_current_pointer (data->buffer);
        uca_camera_grab (data->camera, buffer, NULL);

        if (error == NULL) {
            ring_buffer_proceed (data->buffer);
            n_frames++;
            data->n_recorded++;
        }
        else
            print_and_free_error (&error);
    }

    if (n_max > 0) {
        uca_camera_stop_recording (data->camera, NULL);
        data->state = IDLE;
        set_tool_button_state (data);
    }

    n_frames = ring_buffer_get_num_blocks (data->buffer);

    gdk_threads_enter ();
    gtk_adjustment_set_upper (data->frame_slider, n_frames - 1);
    gtk_adjustment_set_value (data->frame_slider, n_frames - 1);
    gdk_threads_leave ();

    return NULL;
}

gboolean
on_delete_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return FALSE;
}

void
on_destroy (GtkWidget *widget, ThreadData *data)
{
    data->state = IDLE;
    g_object_unref (data->camera);
    ring_buffer_free (data->buffer);

    gtk_main_quit ();
}

static void
update_current_frame (ThreadData *data)
{
    gpointer buffer;
    guint index;
    guint n_max;

    index = (guint) gtk_adjustment_get_value (data->frame_slider);
    n_max = ring_buffer_get_num_blocks (data->buffer);

    /* Shift index so that we always show the oldest frames first */
    if (n_max > 0)
        index = (index + data->n_recorded - n_max) % n_max;

    ring_buffer_set_current_pointer (data->buffer, index);

    buffer = ring_buffer_get_current_pointer (data->buffer);
    up_and_down_scale (data, buffer);
    update_pixbuf (data);
}

static void
on_frame_slider_changed (GtkAdjustment *adjustment, ThreadData *data)
{
    if (data->state == IDLE)
        update_current_frame (data);
}

static gboolean
write_raw_file (const gchar *filename, RingBuffer *buffer)
{
    FILE *fp;
    guint n_blocks;
    gsize size;

    fp = fopen (filename, "wb");

    if (fp == NULL)
        return FALSE;

    n_blocks = ring_buffer_get_num_blocks (buffer);
    size = ring_buffer_get_block_size (buffer);

    for (guint i = 0; i < n_blocks; i++)
        fwrite (ring_buffer_get_pointer (buffer, i), size , 1, fp);

    fclose (fp);
    return TRUE;
}

static void
on_save (GtkMenuItem *item, ThreadData *data)
{
    GtkWidget *dialog;

    dialog = gtk_file_chooser_dialog_new ("Save Frames", NULL,
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                          NULL);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename;

        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        write_raw_file (filename, data->buffer);
        g_free (filename);
    }

    gtk_widget_destroy (dialog);
}

static void
on_start_button_clicked (GtkWidget *widget, ThreadData *data)
{
    GError *error = NULL;

    uca_camera_start_recording (data->camera, &error);

    if (error != NULL) {
        g_printerr ("Failed to start recording: %s\n", error->message);
        return;
    }

    data->state = RUNNING;
    set_tool_button_state (data);

    if (!g_thread_create (preview_frames, data, FALSE, &error)) {
        g_printerr ("Failed to create thread: %s\n", error->message);
        data->state = IDLE;
        set_tool_button_state (data);
    }
}

static void
on_stop_button_clicked (GtkWidget *widget, ThreadData *data, GtkAdjustment *adjustment)
{
    GError *error = NULL;

    g_object_get (data->camera, "has-camram-recording", &data->data_in_camram, NULL);
    data->state = IDLE;
    set_tool_button_state (data);
    uca_camera_stop_recording (data->camera, &error);

    on_frame_slider_changed (adjustment, data);

    if (error != NULL)
        g_printerr ("Failed to stop: %s\n", error->message);

}

static void
on_record_button_clicked (GtkWidget *widget, ThreadData *data)
{
    GError *error = NULL;

    uca_camera_start_recording (data->camera, &error);

    if (error != NULL) {
        g_printerr ("Failed to start recording: %s\n", error->message);
    }

    data->timestamp = (int) time (0);
    data->state = RECORDING;
    set_tool_button_state (data);

    if (!g_thread_create (record_frames, data, FALSE, &error)) {
        g_printerr ("Failed to create thread: %s\n", error->message);
        data->state = IDLE;
        set_tool_button_state (data);
    }
}

static gpointer
download_frames (ThreadData *data)
{
    gpointer buffer;
    guint n_frames;
    guint current_frame = 1;
    GError *error = NULL;

    g_object_get (data->camera, "recorded-frames", &n_frames, NULL);
    gdk_threads_enter ();
    gtk_adjustment_set_upper (data->download_adjustment, n_frames);
    gdk_threads_leave ();

    uca_camera_start_readout (data->camera, &error);

    if (error != NULL) {
        g_printerr ("Failed to start read out of camera memory: %s\n", error->message);
        return NULL;
    }

    ring_buffer_reset (data->buffer);

    while (error == NULL) {
        buffer = ring_buffer_get_current_pointer (data->buffer);
        uca_camera_grab (data->camera, buffer, &error);
        ring_buffer_proceed (data->buffer);
        gdk_threads_enter ();
        gtk_adjustment_set_value (data->download_adjustment, current_frame++);
        gdk_threads_leave ();
    }

    if (error->code == UCA_CAMERA_ERROR_END_OF_STREAM) {
        guint n_frames = ring_buffer_get_num_blocks (data->buffer);

        gtk_adjustment_set_upper (data->frame_slider, n_frames - 1);
        gtk_adjustment_set_value (data->frame_slider, n_frames - 1);
    }
    else
        g_printerr ("Error while reading out frames: %s\n", error->message);

    g_error_free (error);
    error = NULL;

    uca_camera_stop_readout (data->camera, &error);

    if (error != NULL)
        g_printerr ("Failed to stop reading out of camera memory: %s\n", error->message);

    gdk_threads_enter ();
    gtk_dialog_response (data->download_dialog, GTK_RESPONSE_OK);
    gdk_threads_leave ();

    return NULL;
}

static void
on_download_button_clicked (GtkWidget *widget, ThreadData *data)
{
    GError *error = NULL;

    if (!g_thread_create ((GThreadFunc) download_frames, data, FALSE, &error)) {
        g_printerr ("Failed to create thread: %s\n", error->message);
    }

    gtk_widget_set_sensitive (data->main_window, FALSE);
    gtk_window_set_modal (GTK_WINDOW (data->download_dialog), TRUE);
    gtk_dialog_run (data->download_dialog);
    gtk_widget_hide (GTK_WIDGET (data->download_dialog));
    gtk_window_set_modal (GTK_WINDOW (data->download_dialog), FALSE);
    gtk_widget_set_sensitive (data->main_window, TRUE);
    gtk_window_set_modal (GTK_WINDOW (data->download_dialog), TRUE);
}

static void
on_histogram_changed (EggHistogramView *view, ThreadData *data)
{
    if (data->state == IDLE)
        update_current_frame (data);
}

static void
on_zoom_changed (GtkComboBox *widget, ThreadData *data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gdouble factor;

    enum {
        DISPLAY_COLUMN,
        FACTOR_COLUMN
    };

    model = gtk_combo_box_get_model (widget);
    gtk_combo_box_get_active_iter (widget, &iter);
    gtk_tree_model_get (model, &iter, FACTOR_COLUMN, &factor, -1);

    data->zoom_factor = factor;
    update_pixbuf_dimensions (data);

    up_and_down_scale (data, ring_buffer_get_current_pointer (data->buffer));
    update_pixbuf (data);
}

static void
on_colormap_changed (GtkComboBox *widget, ThreadData *data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint map;

    enum {
        DISPLAY2_COLUMN,
        MAP_COLUMN
    };

    model = gtk_combo_box_get_model (widget);
    gtk_combo_box_get_active_iter (widget, &iter);
    gtk_tree_model_get (model, &iter, MAP_COLUMN, &map, -1);

    data->colormap = map;
    update_pixbuf_dimensions (data);

    up_and_down_scale (data, ring_buffer_get_current_pointer (data->buffer));
    update_pixbuf (data);
}

static void
on_roi_width_changed (GObject *object, GParamSpec *pspec, ThreadData *data)
{
    g_object_get (object, "roi-width", &data->width, NULL);
    update_pixbuf_dimensions (data);
}

static void
on_roi_height_changed (GObject *object, GParamSpec *pspec, ThreadData *data)
{
    g_object_get (object, "roi-height", &data->height, NULL);
    update_pixbuf_dimensions (data);
}

static void
create_main_window (GtkBuilder *builder, const gchar* camera_name)
{
    static ThreadData td;
    UcaCamera *camera;
    GtkWidget *window;
    GtkWidget *image;
    GtkWidget *histogram_view;
    GtkWidget *property_tree_view;
    GdkPixbuf *pixbuf;
    GtkBox    *histogram_box;
    GtkAdjustment   *max_bin_adjustment;
    RingBuffer      *ring_buffer;
    gsize image_size;
    guint n_frames;
    guint bits_per_sample;
    guint pixel_size;
    guint width, height;
    GError  *error = NULL;

    camera = uca_plugin_manager_get_camera (plugin_manager, camera_name, &error, NULL);

    if ((camera == NULL) || (error != NULL)) {
        g_error ("%s\n", error->message);
        gtk_main_quit ();
    }

    g_object_get (camera,
                  "roi-width", &width,
                  "roi-height", &height,
                  "sensor-bitdepth", &bits_per_sample,
                  NULL);

    g_signal_connect (camera, "notify::roi-width", (GCallback) on_roi_width_changed, &td);
    g_signal_connect (camera, "notify::roi-height", (GCallback) on_roi_height_changed, &td);

    histogram_view      = egg_histogram_view_new (width * height, bits_per_sample, 256);
    property_tree_view  = egg_property_tree_view_new (G_OBJECT (camera));
    image               = GTK_WIDGET (gtk_builder_get_object (builder, "image"));
    histogram_box       = GTK_BOX (gtk_builder_get_object (builder, "histogram-box"));
    window              = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
    max_bin_adjustment  = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "max-bin-value-adjustment"));

    td.acquisition_expander = GTK_WIDGET (gtk_builder_get_object (builder, "acquisition-expander"));
    td.properties_expander  = GTK_WIDGET (gtk_builder_get_object (builder, "properties-expander"));

    td.zoom_box         = GTK_WIDGET (gtk_builder_get_object (builder, "zoom-box"));
    td.colormap_box     = GTK_WIDGET (gtk_builder_get_object (builder, "colormap-box"));
    td.start_button     = GTK_WIDGET (gtk_builder_get_object (builder, "start-button"));
    td.stop_button      = GTK_WIDGET (gtk_builder_get_object (builder, "stop-button"));
    td.record_button    = GTK_WIDGET (gtk_builder_get_object (builder, "record-button"));
    td.download_button  = GTK_WIDGET (gtk_builder_get_object (builder, "download-button"));
    td.histogram_button = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "histogram-checkbutton"));
    td.log_button       = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "logarithm-checkbutton"));
    td.frame_slider     = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "frames-adjustment"));
    td.count            = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "acquisitions-adjustment"));

    td.hadjustment      = GTK_ADJUSTMENT (gtk_builder_get_object(builder, "hadjustment"));
    td.vadjustment      = GTK_ADJUSTMENT (gtk_builder_get_object(builder, "vadjustment"));
    td.page_width       = gtk_adjustment_get_page_size(GTK_ADJUSTMENT(td.hadjustment));
    td.page_height      = gtk_adjustment_get_page_size(GTK_ADJUSTMENT(td.vadjustment));

    td.mean_label       = GTK_LABEL (gtk_builder_get_object (builder, "mean-label"));
    td.sigma_label      = GTK_LABEL (gtk_builder_get_object (builder, "sigma-label"));
    td.max_label        = GTK_LABEL (gtk_builder_get_object (builder, "max-label"));
    td.min_label        = GTK_LABEL (gtk_builder_get_object (builder, "min-label"));

    td.download_dialog  = GTK_DIALOG (gtk_builder_get_object (builder, "download-dialog"));
    td.download_adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "download-adjustment"));

    /* Set initial data */
    pixel_size  = bits_per_sample > 8 ? 2 : 1;
    image_size  = pixel_size * width * height;
    n_frames    = mem_size * 1024 * 1024 / image_size;
    ring_buffer = ring_buffer_new (image_size, n_frames);

    egg_histogram_view_update (EGG_HISTOGRAM_VIEW (histogram_view),
                               ring_buffer_get_current_pointer (ring_buffer));

    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);

    gtk_adjustment_set_value (max_bin_adjustment, pow (2, bits_per_sample) - 1);

    gtk_adjustment_set_upper (td.count, (gdouble) G_MAXULONG);

    g_message ("Allocated memory for %d frames", n_frames);

    td.pixel_size = pixel_size;
    td.image  = image;
    td.pixbuf = NULL;
    td.pixels = NULL;
    td.buffer = ring_buffer;
    td.state  = IDLE;
    td.camera = camera;
    td.width  = td.display_width = width;
    td.height = td.display_height = height;
    td.zoom_factor = 1.0;
    td.colormap = 1;
    td.histogram_view = histogram_view;
    td.data_in_camram = FALSE;
    td.main_window = window;

    update_pixbuf_dimensions (&td);
    set_tool_button_state (&td);

    /* Hook up signals */
    g_object_bind_property (gtk_builder_get_object (builder, "min-bin-value-adjustment"), "value",
                            td.histogram_view, "minimum-bin-value",
                            G_BINDING_BIDIRECTIONAL);

    g_object_bind_property (max_bin_adjustment, "value",
                            td.histogram_view, "maximum-bin-value",
                            G_BINDING_BIDIRECTIONAL);

    g_object_bind_property (gtk_builder_get_object (builder, "repeat-checkbutton"), "active", 
                            gtk_builder_get_object (builder, "repeat-box"), "sensitive", 0);

    g_object_bind_property (camera, "exposure-time",
                            gtk_builder_get_object (builder, "exposure-adjustment"), "value",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

    g_signal_connect (gtk_builder_get_object (builder, "save-item"),
                      "activate", G_CALLBACK (on_save), &td);

    g_signal_connect (gtk_builder_get_object (builder, "zoom-box"),
                      "changed", G_CALLBACK (on_zoom_changed), &td);

    g_signal_connect (gtk_builder_get_object (builder, "colormap-box"),
                      "changed", G_CALLBACK (on_colormap_changed), &td);

    g_signal_connect (td.frame_slider, "value-changed", G_CALLBACK (on_frame_slider_changed), &td);
    g_signal_connect (td.start_button, "clicked", G_CALLBACK (on_start_button_clicked), &td);
    g_signal_connect (td.stop_button, "clicked", G_CALLBACK (on_stop_button_clicked), &td);
    g_signal_connect (td.record_button, "clicked", G_CALLBACK (on_record_button_clicked), &td);
    g_signal_connect (td.download_button, "clicked", G_CALLBACK (on_download_button_clicked), &td);
    g_signal_connect (histogram_view, "changed", G_CALLBACK (on_histogram_changed), &td);
    g_signal_connect (window, "destroy", G_CALLBACK (on_destroy), &td);

    /* Layout */
    gtk_container_add (GTK_CONTAINER (gtk_builder_get_object (builder, "property-window")),
                       property_tree_view);
    gtk_box_pack_start (histogram_box, td.histogram_view, TRUE, TRUE, 6);
    gtk_box_reorder_child (histogram_box, GTK_WIDGET (td.histogram_button), 2);

    gtk_widget_show_all (window);
}

static void
on_button_proceed_clicked (GtkWidget *widget, gpointer data)
{
    GtkBuilder *builder = GTK_BUILDER (data);
    GtkWidget *choice_window = GTK_WIDGET (gtk_builder_get_object (builder, "choice-window"));
    GtkTreeView *treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "treeview-cameras"));
    GtkListStore *list_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "camera-types"));

    GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
    GList *selected_rows = gtk_tree_selection_get_selected_rows (selection, NULL);
    GtkTreeIter iter;

    gtk_widget_destroy (choice_window);
    gboolean valid = gtk_tree_model_get_iter (GTK_TREE_MODEL (list_store), &iter, selected_rows->data);

    if (valid) {
        gchar *data;
        gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter, 0, &data, -1);
        create_main_window (builder, data);
        g_free (data);
    }

    g_list_foreach (selected_rows, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected_rows);
}

static void
on_treeview_keypress (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event->keyval == GDK_KEY_Return)
        gtk_widget_grab_focus (GTK_WIDGET (data));
}

static void
create_choice_window (GtkBuilder *builder)
{
    GList *camera_types = uca_plugin_manager_get_available_cameras (plugin_manager);

    GtkWidget *choice_window = GTK_WIDGET (gtk_builder_get_object (builder, "choice-window"));
    GtkTreeView *treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "treeview-cameras"));
    GtkListStore *list_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "camera-types"));
    GtkButton *proceed_button = GTK_BUTTON (gtk_builder_get_object (builder, "proceed-button"));
    GtkTreeIter iter;

    for (GList *it = g_list_first (camera_types); it != NULL; it = g_list_next (it)) {
        gtk_list_store_append (list_store, &iter);
        gtk_list_store_set (list_store, &iter, 0, g_strdup ((gchar *) it->data), -1);
    }

    gboolean valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);

    if (valid) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
        gtk_tree_selection_unselect_all (selection);
        gtk_tree_selection_select_path (selection, gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter));
    }

    g_signal_connect (proceed_button, "clicked", G_CALLBACK (on_button_proceed_clicked), builder);
    g_signal_connect (treeview, "key-press-event", G_CALLBACK (on_treeview_keypress), proceed_button);
    gtk_widget_show_all (GTK_WIDGET (choice_window));

    g_list_foreach (camera_types, (GFunc) g_free, NULL);
    g_list_free (camera_types);
}

int
main (int argc, char *argv[])
{
    GtkBuilder *builder;
    GOptionContext *context;
    GError *error = NULL;
    static gchar *camera_name = NULL;

    static GOptionEntry entries[] =
    {
        { "mem-size", 'm', 0, G_OPTION_ARG_INT, &mem_size, "Memory in megabytes to allocate for frame storage", "M" },
        { "camera", 'c', 0, G_OPTION_ARG_STRING, &camera_name, "Default camera (skips choice window)", "NAME" },
        { NULL }
    };

    context = g_option_context_new ("- control libuca cameras");
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print ("Option parsing failed: %s\n", error->message);
        return 1;
    }

    g_thread_init (NULL);
    gdk_threads_init ();
    gtk_init (&argc, &argv);

    builder = gtk_builder_new ();

    if (!gtk_builder_add_from_file (builder, CONTROL_GLADE_PATH, &error)) {
        g_print ("Could not load UI file: %s\n", error->message);
        return 1;
    }

    plugin_manager = uca_plugin_manager_new ();
    gtk_builder_connect_signals (builder, NULL);

    if (camera_name != NULL)
        create_main_window (builder, camera_name);
    else
        create_choice_window (builder);

    gdk_threads_enter ();
    gtk_main ();
    gdk_threads_leave ();

    g_object_unref (plugin_manager);
    return 0;
}
