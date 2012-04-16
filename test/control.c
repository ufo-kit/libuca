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
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "uca-camera.h"


typedef struct {
    gboolean running;
    gboolean store;
    
    guchar *buffer, *pixels;
    GdkPixbuf *pixbuf;
    GtkWidget *image;
    UcaCamera *camera;

    GtkStatusbar *statusbar;
    guint statusbar_context_id;

    int timestamp;
    int width;
    int height;
    int pixel_size;
    float scale;
} ThreadData;

typedef struct {
    ThreadData *thread_data;
    GtkTreeStore *tree_store;
} ValueCellData;

enum {
    COLUMN_NAME = 0,
    COLUMN_VALUE,
    COLUMN_UNIT,
    COLUMN_UCA_ID,
    NUM_COLUMNS
};


void convert_8bit_to_rgb(guchar *output, guchar *input, int width, int height)
{
    for (int i = 0, j = 0; i < width*height; i++) {
        output[j++] = input[i];
        output[j++] = input[i]; 
        output[j++] = input[i];
    }
}

void convert_16bit_to_rgb(guchar *output, guchar *input, int width, int height, float scale)
{
    guint16 *in = (guint16 *) input;
    for (int i = 0, j = 0; i < width*height; i++) {
        guchar val = (guint8) ((in[i]/scale)*256.0f);
        output[j++] = val;
        output[j++] = val;
        output[j++] = val;
    }
}

void reallocate_buffers(ThreadData *td, int width, int height)
{
    const size_t num_bytes = width * height * td->pixel_size;

    g_object_unref(td->pixbuf);
    g_free(td->buffer);

    td->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    td->buffer = (guchar *) g_malloc(num_bytes);
    td->width  = width;
    td->height = height;
    td->pixels = gdk_pixbuf_get_pixels(td->pixbuf);
    gtk_image_set_from_pixbuf(GTK_IMAGE(td->image), td->pixbuf);
    memset(td->buffer, 0, num_bytes);
}

void *grab_thread(void *args)
{
    ThreadData *data = (ThreadData *) args;
    gchar filename[FILENAME_MAX] = {0,};
    gint counter = 0;

    while (data->running) {
        uca_camera_grab(data->camera, (gpointer) &data->buffer, NULL);

        if (data->store) {
            snprintf(filename, FILENAME_MAX, "frame-%i-%08i.raw", data->timestamp, counter++);
            FILE *fp = fopen(filename, "wb");
            fwrite(data->buffer, data->width*data->height, data->pixel_size, fp);
            fclose(fp);
        }

        /* FIXME: We should actually check if this is really a new frame and
         * just do nothing if it is an already displayed one. */
        if (data->pixel_size == 1)
            convert_8bit_to_rgb(data->pixels, data->buffer, data->width, data->height);
        else if (data->pixel_size == 2)
            convert_16bit_to_rgb(data->pixels, data->buffer, data->width, data->height, data->scale);

        gdk_threads_enter();
        gdk_flush();
        gtk_image_clear(GTK_IMAGE(data->image));
        gtk_image_set_from_pixbuf(GTK_IMAGE(data->image), data->pixbuf);
        gtk_widget_queue_draw_area(data->image, 0, 0, data->width, data->height);
        gdk_threads_leave();
    }
    return NULL;
}

gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return FALSE;
}

void on_destroy(GtkWidget *widget, gpointer data)
{
    ThreadData *td = (ThreadData *) data;
    td->running = FALSE;
    g_object_unref(td->camera);
    gtk_main_quit();
}

void on_adjustment_scale_value_changed(GtkAdjustment* adjustment, gpointer user_data)
{
    ThreadData *data = (ThreadData *) user_data;
    data->scale = gtk_adjustment_get_value(adjustment);
}

void on_toolbutton_run_clicked(GtkWidget *widget, gpointer args)
{
    ThreadData *data = (ThreadData *) args;

    if (data->running)
        return;
    
    GError *error = NULL;
    data->running = TRUE;

    uca_camera_start_recording(data->camera, &error);

    if (error != NULL) {
        g_printerr("Failed to start recording: %s\n", error->message);
        return;
    }

    if (!g_thread_create(grab_thread, data, FALSE, &error)) {
        g_printerr("Failed to create thread: %s\n", error->message);
        return;
    }
}

void on_toolbutton_stop_clicked(GtkWidget *widget, gpointer args)
{
    ThreadData *data = (ThreadData *) args;
    data->running = FALSE;
    data->store = FALSE;
    uca_camera_stop_recording(data->camera, NULL);
}

void on_toolbutton_record_clicked(GtkWidget *widget, gpointer args)
{
    ThreadData *data = (ThreadData *) args;
    data->timestamp = (int) time(0);
    data->store = TRUE;
    GError *error = NULL;
    
    gtk_statusbar_push(data->statusbar, data->statusbar_context_id, "Recording...");
    
    if (data->running != TRUE) {
        data->running = TRUE;
        uca_camera_start_recording(data->cam, &error);

        if (!g_thread_create(grab_thread, data, FALSE, &error)) {
            g_printerr("Failed to create thread: %s\n", error->message);
        }
    }
}

static void create_main_window(GtkBuilder *builder, const gchar* camera_name)
{
    static ThreadData td;

    GError *error = NULL;
    UcaCamera *camera = uca_camera_new(camera_name, &error);

    if (error != NULL) {
        g_error("%s\n", error->message);
        gtk_main_quit();
    }

    guint bits_per_sample;
    g_object_get(camera,
            "roi-width", &td.width,
            "roi-height", &td.height,
            "sensor-bitdepth", &bits_per_sample,
            NULL);

    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    GtkWidget *image = GTK_WIDGET(gtk_builder_get_object(builder, "image"));
    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, td.width, td.height);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);

    td.pixel_size = bits_per_sample == 16 ? 2 : 1;
    td.image  = image;
    td.pixbuf = pixbuf;
    td.buffer = (guchar *) g_malloc(td.pixel_size * td.width * td.height);
    td.pixels = gdk_pixbuf_get_pixels(pixbuf);
    td.running = FALSE;
    td.scale = 65535.0f;
    td.statusbar = GTK_STATUSBAR(gtk_builder_get_object(builder, "statusbar"));
    td.statusbar_context_id = gtk_statusbar_get_context_id(td.statusbar, "Recording Information");
    td.store = FALSE;
    td.camera = camera;

    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), &td);
    g_signal_connect(gtk_builder_get_object(builder, "toolbutton_run"),
            "clicked", G_CALLBACK(on_toolbutton_run_clicked), &td);
    g_signal_connect(gtk_builder_get_object(builder, "toolbutton_stop"),
            "clicked", G_CALLBACK(on_toolbutton_stop_clicked), &td);
    g_signal_connect(gtk_builder_get_object(builder, "toolbutton_record"),
            "clicked", G_CALLBACK(on_toolbutton_record_clicked), &td);

    gtk_widget_show(image);
    gtk_widget_show(window);
}

static void on_button_proceed_clicked(GtkWidget *widget, gpointer data)
{
    GtkBuilder *builder = GTK_BUILDER(data);
    GtkWidget *choice_window = GTK_WIDGET(gtk_builder_get_object(builder, "choice-window"));
    GtkTreeView *treeview = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview-cameras"));
    GtkListStore *list_store = GTK_LIST_STORE(gtk_builder_get_object(builder, "camera-types"));

    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    GList *selected_rows = gtk_tree_selection_get_selected_rows(selection, NULL);
    GtkTreeIter iter;

    gtk_widget_destroy(choice_window);
    gboolean valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(list_store), &iter, selected_rows->data);

    if (valid) {
        gchar *data; 
        gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, 0, &data, -1);
        create_main_window(builder, data);
        g_free(data);
    }

    g_list_foreach(selected_rows, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(selected_rows);
}

static void create_choice_window(GtkBuilder *builder)
{
    gchar **camera_types = uca_camera_get_types();

    GtkWidget *choice_window = GTK_WIDGET(gtk_builder_get_object(builder, "choice-window"));
    GtkTreeView *treeview = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview-cameras"));
    GtkListStore *list_store = GTK_LIST_STORE(gtk_builder_get_object(builder, "camera-types"));
    GtkButton *proceed_button = GTK_BUTTON(gtk_builder_get_object(builder, "button-proceed"));
    GtkTreeIter iter;

    for (guint i = 0; camera_types[i] != NULL; i++) {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, camera_types[i], -1);
    }

    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list_store), &iter);

    if (valid) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
        gtk_tree_selection_unselect_all(selection);
        gtk_tree_selection_select_path(selection, gtk_tree_model_get_path(GTK_TREE_MODEL(list_store), &iter));
    }

    g_strfreev(camera_types);
    g_signal_connect(proceed_button, "clicked", G_CALLBACK(on_button_proceed_clicked), builder);
    gtk_widget_show_all(GTK_WIDGET(choice_window));
}

int main(int argc, char *argv[])
{
    GError *error = NULL;

    g_thread_init(NULL);
    gdk_threads_init();
    gtk_init(&argc, &argv);

    GtkBuilder *builder = gtk_builder_new();

    if (!gtk_builder_add_from_file(builder, "control.glade", &error)) {
        g_print("Error: %s\n", error->message);
        return 1;
    }

    create_choice_window(builder);
    gtk_builder_connect_signals(builder, NULL);

    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();
    
    return 0;
}
