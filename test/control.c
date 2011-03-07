#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "uca.h"
#include "uca-cam.h"

struct ThreadData {
    guchar *buffer, *pixels;
    GtkWidget *image;
    GdkPixbuf *pixbuf;
    int width;
    int height;

    struct uca_camera_t *cam;
};

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return FALSE;
}

/* Another callback */
static void destroy(GtkWidget *widget, gpointer data)
{
    struct uca_t *uca = (struct uca_t *) data;
    uca_destroy(uca);
    gtk_main_quit ();
}

void grey_to_rgb(guchar *output, guchar *input, int width, int height)
{
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            const int off = y*width + x;
            output[off*3] = input[off];
            output[off*3+1] = input[off];
            output[off*3+2] = input[off];
        }
    }
}

void *grab_thread(void *args)
{
    struct ThreadData *data = (struct ThreadData *) args;
    struct uca_camera_t *cam = data->cam;

    while (TRUE) {
        cam->grab(cam, (char *) data->buffer);
        grey_to_rgb(data->pixels, data->buffer, data->width, data->height);

        gdk_threads_enter();
        gdk_flush();
        gtk_image_clear(GTK_IMAGE(data->image));
        gtk_image_set_from_pixbuf(GTK_IMAGE(data->image), data->pixbuf);
        gtk_widget_queue_draw_area(data->image, 0, 0, data->width, data->height);
        gdk_threads_leave();
    }
}

void fill_tree_store(GtkTreeStore *tree_store, struct uca_camera_t *cam)
{
    GtkTreeIter iter, child;
    struct uca_property_t *property;
    gchar *value_string = g_malloc(256);
    guint8 value_8;
    guint32 value_32;

    gtk_tree_store_append(tree_store, &iter, NULL);
    for (int prop_id = 0; prop_id < UCA_PROP_LAST; prop_id++) {
        property = uca_get_full_property(prop_id);
        switch (property->type) {
            case uca_string:
                cam->get_property(cam, prop_id, value_string);
                break;

            case uca_uint8t:
                cam->get_property(cam, prop_id, &value_8);
                g_sprintf(value_string, "%d", value_8);
                break;

            case uca_uint32t:
                cam->get_property(cam, prop_id, &value_32);
                g_sprintf(value_string, "%d", value_32);
                break;
        }
        gtk_tree_store_set(tree_store, &iter, 
                0, property->name,
                1, value_string,
                -1);
        gtk_tree_store_append(tree_store, &iter, NULL);
    }

    g_free(value_string);
}

int main(int argc, char *argv[])
{
    struct uca_t *uca = uca_init();
    if (uca == NULL) {
        g_print("Couldn't initialize frame grabber and/or cameras\n");
        return 1;
    }

    int width, height, bits_per_sample;
    struct uca_camera_t *cam = uca->cameras;
    cam->get_property(cam, UCA_PROP_WIDTH, &width);
    cam->get_property(cam, UCA_PROP_HEIGHT, &height);
    cam->get_property(cam, UCA_PROP_BITDEPTH, &bits_per_sample);
    bits_per_sample = 8;

    g_thread_init(NULL);
    gdk_threads_init();
    gtk_init (&argc, &argv);

    GtkBuilder *builder = gtk_builder_new();
    GError *error = NULL;
    if (!gtk_builder_add_from_file(builder, "control.glade", &error)) {
        g_print("Couldn't load UI file!\n");
        g_print("Message: %s\n", error->message);
        g_free(error);
    }

    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    GtkWidget *image = GTK_WIDGET(gtk_builder_get_object(builder, "image"));
    GtkTreeStore *tree_store = GTK_TREE_STORE(gtk_builder_get_object(builder, "cameraproperties"));
    fill_tree_store(tree_store, cam);

    g_signal_connect (window, "delete-event",
              G_CALLBACK (delete_event), NULL);
    
    g_signal_connect (window, "destroy",
              G_CALLBACK (destroy), uca);
    
    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, bits_per_sample, width, height);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
    
    gtk_widget_show(image);
    gtk_widget_show(window);

    /* start grabbing and thread */
    struct ThreadData td;
    uca_cam_alloc(cam, 20);
    td.image  = image;
    td.pixbuf = pixbuf;
    td.buffer = (guchar *) g_malloc(width * height);
    td.pixels = gdk_pixbuf_get_pixels(pixbuf);
    td.width  = width;
    td.height = height;
    td.cam    = cam;
    cam->start_recording(cam);
    if (!g_thread_create(grab_thread, &td, FALSE, &error)) {
        g_printerr("Failed to create thread: %s\n", error->message);
        uca_destroy(uca);
        return 1;
    }
    
    gdk_threads_enter();
    gtk_main ();
    gdk_threads_leave();
    
    return 0;
}
