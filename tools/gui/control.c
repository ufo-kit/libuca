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
    GdkPixbuf   *pixbuf;
    GtkWidget   *image;
    GtkWidget   *start_button;
    GtkWidget   *stop_button;
    GtkWidget   *record_button;
    GtkWidget   *download_button;
    GtkComboBox *zoom_box;

    GtkWidget       *histogram_view;
    GtkToggleButton *histogram_button;
    GtkAdjustment   *frame_slider;

    RingBuffer  *buffer;
    guchar      *pixels;
    gint         display_width, display_height;
    gdouble      zoom_factor;
    State        state;
    gboolean     data_in_camram;

    gint         timestamp;
    gint         width, height;
    gint         pixel_size;
} ThreadData;

static UcaPluginManager *plugin_manager;
static gsize mem_size = 2048;

static void
convert_grayscale_to_rgb (ThreadData *data, gpointer buffer)
{
    gdouble min;
    gdouble max;
    gdouble factor;
    guint8 *output;
    gint i = 0;
    gint stride;

    egg_histogram_get_visible_range (EGG_HISTOGRAM_VIEW (data->histogram_view), &min, &max);
    factor = 255.0 / (max - min);
    output = data->pixels;
    stride = (gint) 1 / data->zoom_factor;

    if (data->pixel_size == 1) {
        guint8 *input = (guint8 *) buffer;

        for (gint y = 0; y < data->display_height; y++) {
            gint offset = y * stride * data->width;

            for (gint x = 0; x < data->display_width; x++, offset += stride) {
                guchar val = (guchar) ((input[offset] - min) * factor);

                output[i++] = val;
                output[i++] = val;
                output[i++] = val;
            }
        }
    }
    else if (data->pixel_size == 2) {
        guint16 *input = (guint16 *) buffer;

        for (gint y = 0; y < data->display_height; y++) {
            gint offset = y * stride * data->width;

            for (gint x = 0; x < data->display_width; x++, offset += stride) {
                guchar val = (guchar) ((input[offset] - min) * factor);

                output[i++] = val;
                output[i++] = val;
                output[i++] = val;
            }
        }
    }
}

static void
update_pixbuf (ThreadData *data)
{
    gdk_flush ();
    gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), data->pixbuf);
    gtk_widget_queue_draw_area (data->image, 0, 0, data->display_width, data->display_height);

    if (gtk_toggle_button_get_active (data->histogram_button))
        gtk_widget_queue_draw (data->histogram_view);
}

static void
update_pixbuf_dimensions (ThreadData *data)
{
    if (data->pixbuf != NULL)
        g_object_unref (data->pixbuf);

    data->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, data->display_width, data->display_height);
    data->pixels = gdk_pixbuf_get_pixels (data->pixbuf);
    gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), data->pixbuf);
}

static gpointer
preview_frames (void *args)
{
    ThreadData *data = (ThreadData *) args;
    gint counter = 0;

    while (data->state == RUNNING) {
        gpointer buffer;

        buffer = ring_buffer_get_current_pointer (data->buffer);
        uca_camera_grab (data->camera, &buffer, NULL);
        convert_grayscale_to_rgb (data, buffer);

        gdk_threads_enter ();
        update_pixbuf (data);
        gdk_threads_leave ();

        counter++;
    }
    return NULL;
}

static gpointer
record_frames (gpointer args)
{
    ThreadData *data;
    gpointer buffer;
    guint n_frames = 0;

    data = (ThreadData *) args;
    ring_buffer_reset (data->buffer);

    while (data->state == RECORDING) {
        buffer = ring_buffer_get_current_pointer (data->buffer);
        uca_camera_grab (data->camera, &buffer, NULL);
        ring_buffer_proceed (data->buffer);
        n_frames++;
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
    gtk_widget_set_sensitive (GTK_WIDGET (data->zoom_box),
                              data->state == IDLE);
}

static void
update_current_frame (ThreadData *data)
{
    gpointer buffer;
    gint index;

    index = (gint) gtk_adjustment_get_value (data->frame_slider);
    buffer = ring_buffer_get_pointer (data->buffer, index);
    convert_grayscale_to_rgb (data, buffer);
    update_pixbuf (data);
}

static void
on_frame_slider_changed (GtkAdjustment *adjustment, ThreadData *data)
{
    if (data->state == IDLE)
        update_current_frame (data);
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
on_stop_button_clicked (GtkWidget *widget, ThreadData *data)
{
    GError *error = NULL;

    g_object_get (data->camera, "has-camram-recording", &data->data_in_camram, NULL);
    data->state = IDLE;
    set_tool_button_state (data);
    uca_camera_stop_recording (data->camera, &error);

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

static void
on_download_button_clicked (GtkWidget *widget, ThreadData *data)
{
    gpointer buffer;
    GError *error = NULL;

    uca_camera_start_readout (data->camera, &error);

    if (error != NULL) {
        g_printerr ("Failed to start read out of camera memory: %s\n", error->message);
        return;
    }

    ring_buffer_reset (data->buffer);

    while (error == NULL) {
        buffer = ring_buffer_get_current_pointer (data->buffer);
        uca_camera_grab (data->camera, &buffer, &error);
        ring_buffer_proceed (data->buffer);
    }

    if (error->code == UCA_CAMERA_ERROR_END_OF_STREAM) {
        guint n_frames = ring_buffer_get_num_blocks (data->buffer);

        gtk_adjustment_set_upper (data->frame_slider, n_frames - 1);
        gtk_adjustment_set_value (data->frame_slider, n_frames - 1);
    }
    else
        g_printerr ("Error while reading out frames: %s\n", error->message);

    g_error_free (error);
    uca_camera_stop_readout (data->camera, &error);

    if (error != NULL)
        g_printerr ("Failed to stop reading out of camera memory: %s\n", error->message);
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

    data->display_width = (gint) data->width * factor;
    data->display_height = (gint) data->height * factor;
    data->zoom_factor = factor;
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
    GtkContainer    *property_window;
    GtkAdjustment   *max_bin_adjustment;
    RingBuffer      *ring_buffer;
    gsize image_size;
    guint n_frames;
    guint bits_per_sample;
    guint pixel_size;
    guint width, height;
    GError  *error = NULL;

    camera = uca_plugin_manager_get_camera (plugin_manager, camera_name, &error);

    if ((camera == NULL) || (error != NULL)) {
        g_error ("%s\n", error->message);
        gtk_main_quit ();
    }

    g_object_get (camera,
                  "roi-width", &width,
                  "roi-height", &height,
                  "sensor-bitdepth", &bits_per_sample,
                  NULL);

    histogram_view      = egg_histogram_view_new ();
    property_tree_view  = egg_property_tree_view_new (G_OBJECT (camera));
    property_window     = GTK_CONTAINER (gtk_builder_get_object (builder, "property-window"));
    image               = GTK_WIDGET (gtk_builder_get_object (builder, "image"));
    histogram_box       = GTK_BOX (gtk_builder_get_object (builder, "histogram-box"));
    window              = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
    max_bin_adjustment  = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "max-bin-value-adjustment"));

    td.zoom_box         = GTK_COMBO_BOX (gtk_builder_get_object (builder, "zoom-box"));
    td.start_button     = GTK_WIDGET (gtk_builder_get_object (builder, "start-button"));
    td.stop_button      = GTK_WIDGET (gtk_builder_get_object (builder, "stop-button"));
    td.record_button    = GTK_WIDGET (gtk_builder_get_object (builder, "record-button"));
    td.download_button  = GTK_WIDGET (gtk_builder_get_object (builder, "download-button"));
    td.histogram_button = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "histogram-checkbutton"));
    td.frame_slider     = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "frames-adjustment"));

    /* Set initial data */
    pixel_size  = bits_per_sample > 8 ? 2 : 1;
    image_size  = pixel_size * width * height;
    n_frames    = mem_size * 1024 * 1024 / image_size;
    ring_buffer = ring_buffer_new (image_size, n_frames);

    egg_histogram_view_set_data (EGG_HISTOGRAM_VIEW (histogram_view),
                                 ring_buffer_get_current_pointer (ring_buffer),
                                 width * height, bits_per_sample, 256);

    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);

    gtk_adjustment_set_value (max_bin_adjustment, pow (2, bits_per_sample) - 1);

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
    td.histogram_view = histogram_view;
    td.data_in_camram = FALSE;

    update_pixbuf_dimensions (&td);
    set_tool_button_state (&td);

    /* Hook up signals */
    g_object_bind_property (gtk_builder_get_object (builder, "min-bin-value-adjustment"), "value",
                            td.histogram_view, "minimum-bin-value",
                            G_BINDING_DEFAULT);

    g_object_bind_property (max_bin_adjustment, "value",
                            td.histogram_view, "maximum-bin-value",
                            G_BINDING_DEFAULT);

    g_signal_connect (td.frame_slider, "value-changed", G_CALLBACK (on_frame_slider_changed), &td);
    g_signal_connect (td.start_button, "clicked", G_CALLBACK (on_start_button_clicked), &td);
    g_signal_connect (td.stop_button, "clicked", G_CALLBACK (on_stop_button_clicked), &td);
    g_signal_connect (td.record_button, "clicked", G_CALLBACK (on_record_button_clicked), &td);
    g_signal_connect (td.download_button, "clicked", G_CALLBACK (on_download_button_clicked), &td);
    g_signal_connect (td.zoom_box, "changed", G_CALLBACK (on_zoom_changed), &td);
    g_signal_connect (histogram_view, "changed", G_CALLBACK (on_histogram_changed), &td);
    g_signal_connect (window, "destroy", G_CALLBACK (on_destroy), &td);

    /* Layout */
    gtk_container_add (property_window, property_tree_view);
    gtk_box_pack_start (histogram_box, td.histogram_view, TRUE, TRUE, 6);

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

    static GOptionEntry entries[] =
    {
        { "mem-size", 'm', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_INT, &mem_size, "Memory in megabytes to allocate for frame storage", "M" },
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
    create_choice_window (builder);
    gtk_builder_connect_signals (builder, NULL);

    gdk_threads_enter ();
    gtk_main ();
    gdk_threads_leave ();

    g_object_unref (plugin_manager);
    return 0;
}
