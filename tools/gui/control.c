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
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"
#include "uca-camera.h"
#include "uca-plugin-manager.h"
#include "egg-property-tree-view.h"
#include "egg-histogram-view.h"


typedef struct {
    UcaCamera   *camera;
    GdkPixbuf   *pixbuf;
    GtkWidget   *image;
    GtkWidget   *start_button;
    GtkWidget   *stop_button;
    GtkWidget   *record_button;
    GtkWidget   *histogram_view;
    GtkToggleButton *histogram_button;

    guchar      *buffer;
    guchar      *pixels;
    gboolean     running;
    gboolean     store;

    int          timestamp;
    int          width;
    int          height;
    int          pixel_size;
} ThreadData;

static UcaPluginManager *plugin_manager;


static void
convert_8bit_to_rgb (guchar *output, guchar *input, int width, int height)
{
    for (int i = 0, j = 0; i < width*height; i++) {
        output[j++] = input[i];
        output[j++] = input[i];
        output[j++] = input[i];
    }
}

static void
convert_16bit_to_rgb (guchar *output, guchar *input, int width, int height)
{
    guint16 *in = (guint16 *) input;
    guint16 min = G_MAXUINT16, max = 0;
    gfloat spread = 0.0f;

    for (int i = 0; i < width * height; i++) {
        guint16 v = in[i];
        if (v < min)
            min = v;
        if (v > max)
            max = v;
    }

    spread = (gfloat) max - min;

    if (spread > 0.0f) {
        for (int i = 0, j = 0; i < width*height; i++) {
            guchar val = (guint8) (((in[i] - min) / spread) * 255.0f);
            output[j++] = val;
            output[j++] = val;
            output[j++] = val;
        }
    }
}

static void *
grab_thread (void *args)
{
    ThreadData *data = (ThreadData *) args;
    gchar filename[FILENAME_MAX] = {0,};
    gint counter = 0;

    while (data->running) {
        uca_camera_grab (data->camera, (gpointer) &data->buffer, NULL);

        if (data->store) {
            snprintf (filename, FILENAME_MAX, "frame-%i-%08i.raw", data->timestamp, counter);
            FILE *fp = fopen (filename, "wb");
            fwrite (data->buffer, data->width*data->height, data->pixel_size, fp);
            fclose (fp);
        }

        /* FIXME: We should actually check if this is really a new frame and
         * just do nothing if it is an already displayed one. */
        if (data->pixel_size == 1)
            convert_8bit_to_rgb (data->pixels, data->buffer, data->width, data->height);
        else if (data->pixel_size == 2) {
            convert_16bit_to_rgb (data->pixels, data->buffer, data->width, data->height);
        }

        gdk_threads_enter ();

        gdk_flush ();
        gtk_image_clear (GTK_IMAGE (data->image));
        gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), data->pixbuf);
        gtk_widget_queue_draw_area (data->image, 0, 0, data->width, data->height);

        if (gtk_toggle_button_get_active (data->histogram_button))
            gtk_widget_queue_draw (data->histogram_view);

        gdk_threads_leave ();

        counter++;
    }
    return NULL;
}

gboolean
on_delete_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return FALSE;
}

void
on_destroy (GtkWidget *widget, gpointer data)
{
    ThreadData *td = (ThreadData *) data;
    td->running = FALSE;
    g_object_unref (td->camera);
    gtk_main_quit ();
}

static void
set_tool_button_state (ThreadData *data)
{
    gtk_widget_set_sensitive (data->start_button, !data->running);
    gtk_widget_set_sensitive (data->stop_button, data->running);
    gtk_widget_set_sensitive (data->record_button, !data->running);
}

static void
on_start_button_clicked (GtkWidget *widget, gpointer args)
{
    ThreadData *data = (ThreadData *) args;
    GError *error = NULL;

    data->running = TRUE;

    set_tool_button_state (data);
    uca_camera_start_recording (data->camera, &error);

    if (error != NULL) {
        g_printerr ("Failed to start recording: %s\n", error->message);
        return;
    }

    if (!g_thread_create (grab_thread, data, FALSE, &error)) {
        g_printerr ("Failed to create thread: %s\n", error->message);
        return;
    }
}

static void
on_stop_button_clicked (GtkWidget *widget, gpointer args)
{
    ThreadData *data = (ThreadData *) args;
    GError *error = NULL;

    data->running = FALSE;
    data->store = FALSE;

    set_tool_button_state (data);
    uca_camera_stop_recording (data->camera, &error);

    if (error != NULL)
        g_printerr ("Failed to stop: %s\n", error->message);
}

static void
on_record_button_clicked (GtkWidget *widget, gpointer args)
{
    ThreadData *data = (ThreadData *) args;
    GError *error = NULL;

    data->timestamp = (int) time (0);
    data->store = TRUE;
    data->running = TRUE;

    set_tool_button_state (data);
    uca_camera_start_recording (data->camera, &error);

    if (!g_thread_create (grab_thread, data, FALSE, &error))
        g_printerr ("Failed to create thread: %s\n", error->message);
}

static void
create_main_window (GtkBuilder *builder, const gchar* camera_name)
{
    GtkWidget *window;
    GtkWidget *image;
    GtkWidget *property_tree_view;
    GdkPixbuf *pixbuf;
    GtkBox    *histogram_box;
    GtkContainer *scrolled_property_window;
    static ThreadData td;

    GError *error = NULL;
    UcaCamera *camera = uca_plugin_manager_get_camera (plugin_manager, camera_name, &error);

    if ((camera == NULL) || (error != NULL)) {
        g_error ("%s\n", error->message);
        gtk_main_quit ();
    }

    guint bits_per_sample;
    g_object_get (camera,
                  "roi-width", &td.width,
                  "roi-height", &td.height,
                  "sensor-bitdepth", &bits_per_sample,
                  NULL);

    property_tree_view = egg_property_tree_view_new (G_OBJECT (camera));
    scrolled_property_window = GTK_CONTAINER (gtk_builder_get_object (builder, "scrolledwindow2"));
    gtk_container_add (scrolled_property_window, property_tree_view);

    image = GTK_WIDGET (gtk_builder_get_object (builder, "image"));
    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, td.width, td.height);
    gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);

    td.pixel_size = bits_per_sample > 8 ? 2 : 1;
    td.image  = image;
    td.pixbuf = pixbuf;
    td.buffer = (guchar *) g_malloc (td.pixel_size * td.width * td.height);
    td.pixels = gdk_pixbuf_get_pixels (pixbuf);
    td.running = FALSE;
    td.store = FALSE;
    td.camera = camera;
    td.histogram_view = egg_histogram_view_new ();
    td.histogram_button = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "histogram-checkbutton"));

    histogram_box = GTK_BOX (gtk_builder_get_object (builder, "histogram-box"));
    gtk_box_pack_start (histogram_box, td.histogram_view, TRUE, TRUE, 6);
    egg_histogram_view_set_data (EGG_HISTOGRAM_VIEW (td.histogram_view), td.buffer, td.width * td.height, bits_per_sample, 256);

    window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
    g_signal_connect (window, "destroy", G_CALLBACK (on_destroy), &td);

    td.start_button = GTK_WIDGET (gtk_builder_get_object (builder, "start-button"));
    td.stop_button = GTK_WIDGET (gtk_builder_get_object (builder, "stop-button"));
    td.record_button = GTK_WIDGET (gtk_builder_get_object (builder, "record-button"));
    set_tool_button_state (&td);

    g_signal_connect (td.start_button, "clicked", G_CALLBACK (on_start_button_clicked), &td);
    g_signal_connect (td.stop_button, "clicked", G_CALLBACK (on_stop_button_clicked), &td);
    g_signal_connect (td.record_button, "clicked", G_CALLBACK (on_record_button_clicked), &td);

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
    GError *error = NULL;

    g_thread_init (NULL);
    gdk_threads_init ();
    gtk_init (&argc, &argv);

    GtkBuilder *builder = gtk_builder_new ();

    if (!gtk_builder_add_from_file (builder, CONTROL_GLADE_PATH, &error)) {
        g_print ("Error: %s\n", error->message);
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
