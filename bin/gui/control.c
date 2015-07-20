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
#include <cairo.h>

#include "config.h"
#include "uca-camera.h"
#include "uca-plugin-manager.h"
#include "uca-ring-buffer.h"
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
    GdkPixbuf   *subpixbuf;
    GtkWidget   *image;
    GtkWidget   *start_button;
    GtkWidget   *stop_button;
    GtkWidget   *record_button;
    GtkWidget   *download_button;
    GtkWidget   *zoom_in_button;
    GtkWidget   *zoom_out_button;
    GtkWidget   *zoom_normal_button;
    GtkWidget   *rect_color_button;
    GtkWidget   *acquisition_expander;
    GtkWidget   *properties_expander;
    GtkWidget   *colormap_box;
    GtkWidget   *event_box;
    GtkLabel    *mean_label;
    GtkLabel    *sigma_label;
    GtkLabel    *max_label;
    GtkLabel    *min_label;
    GtkLabel    *x_label;
    GtkLabel    *y_label;
    GtkLabel    *val_label;
    GtkLabel    *roix_label;
    GtkLabel    *roiy_label;
    GtkLabel    *roiw_label;
    GtkLabel    *roih_label;

    GtkDialog       *download_dialog;
    GtkProgressBar  *download_progressbar;
    GtkAdjustment   *download_adjustment;
    GtkAdjustment   *count;
    GtkAdjustment   *hadjustment;
    GtkAdjustment   *vadjustment;
    GtkAdjustment   *x_adjustment;
    GtkAdjustment   *y_adjustment;
    GtkAdjustment   *width_adjustment;
    GtkAdjustment   *height_adjustment;

    GtkWidget       *histogram_view;
    GtkToggleButton *histogram_button;
    GtkToggleButton *log_button;
    GtkAdjustment   *frame_slider;

    UcaRingBuffer  *buffer;
    guchar      *shadow;
    guchar      *pixels;
    gint         display_width, display_height;
    gint         colormap;
    gdouble      zoom_factor;
    gdouble      zoom_before;
    State        state;
    guint        n_recorded;
    gboolean     data_in_camram;

    gint         timestamp;
    gint         width, height;
    gint         pixel_size;
    gint         ev_x, ev_y;
    gint         display_x, display_y;
    gint         tmp_fromx, tmp_fromy;
    gint         from_x, from_y;
    gint         to_x, to_y;
    gint         adj_width, adj_height;
    gint         rect_x, rect_y;
    gint         rect_evx, rect_evy;
    cairo_t      *cr;
    gdouble      red, green, blue;
} ThreadData;

static UcaPluginManager *plugin_manager;
static gsize mem_size = 2048;

static void update_pixbuf (ThreadData *data);

static void
up_and_down_scale (ThreadData *data, gpointer buffer)
{
    gdouble min;
    gdouble max;
    gdouble factor;
    gdouble dval = 0.0;
    gboolean do_log;
    guint8 *output;
    gint i = 0;
    gint zoom;
    gint stride;
    gint offset = 0;
    gint start_wval;
    gint start_hval;

    egg_histogram_get_range (EGG_HISTOGRAM_VIEW (data->histogram_view), &min, &max);
    factor = 255.0 / (max - min);
    output = data->pixels;
    zoom = (gint) data->zoom_factor;
    stride = (gint) 1 / data->zoom_factor;
    gint page_width = gtk_adjustment_get_page_size (GTK_ADJUSTMENT (data->hadjustment));
    gint page_height = gtk_adjustment_get_page_size (GTK_ADJUSTMENT (data->vadjustment));
    do_log = gtk_toggle_button_get_active (data->log_button);

    if (data->state == RUNNING) {
        if ((data->adj_width > 0) && (data->adj_height > 0)) {
            start_wval = data->from_x;
            start_hval = data->from_y;
            page_width = data->to_x;
            page_height = data->to_y;
        }
        else {
            start_wval = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->hadjustment));
            start_hval = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->vadjustment));
            page_width += start_wval;
            page_height += start_hval;
        }
    }
    else {
        start_wval = 0;
        start_hval = 0;
        page_width = data->display_width;
        page_height = data->display_height;
    }

    if (data->pixel_size == 1) {
        guint8 *input = (guint8 *) buffer;

        for (gint y = 0; y < data->display_height; y++) {
            if (zoom <= 1) {
                offset = y * stride * data->width;
            }

            for (gint x = 0; x < data->display_width; x++) {
                if (zoom <= 1)
                    offset += stride;
                else
                    offset = ((gint) (y / zoom) * data->width) + ((gint) (x / zoom));

                if (y >= start_hval && y < page_height) {
                    if (x >= start_wval && x < page_width) {

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
            if (zoom <= 1) {
                offset = y * stride * data->width;
            }

            for (gint x = 0; x < data->display_width; x++) {
                if (zoom <= 1)
                    offset += stride;
                else
                    offset = ((gint) (y / zoom) * data->width) + ((gint) (x / zoom));

                if (y >= start_hval && y < page_height)  {
                    if (x >= start_wval && x < page_width) {

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
        guint8 *input = (guint8 *) uca_ring_buffer_peek_pointer (data->buffer);

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
        guint16 *input = (guint16 *) uca_ring_buffer_peek_pointer (data->buffer);

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
        *mean = log (sum/n);
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
on_motion_notify (GtkWidget *event_box, GdkEventMotion *event, ThreadData *data)
{
    gint page_width = gtk_adjustment_get_page_size(GTK_ADJUSTMENT(data->hadjustment));
    gint page_height = gtk_adjustment_get_page_size(GTK_ADJUSTMENT(data->vadjustment));
    gint start_wval = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->hadjustment));
    gint start_hval = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->vadjustment));
    page_width += start_wval;
    page_height += start_hval;
    data->rect_evx = event->x;
    data->rect_evy = event->y;

    if ((data->display_width < page_width) && (data->display_height < page_height)) {
        gint startx = (page_width - data->display_width) / 2;
        gint starty = (page_height - data->display_height) / 2;

        data->ev_x = event->x - startx;
        data->ev_y = event->y - starty;

        if ((data->adj_width > 0) && (data->adj_height > 0)) {
            data->display_x = event->x - ((page_width - data->adj_width) / 2) + data->from_x;
            data->display_y = event->y - ((page_height - data->adj_height) / 2) + data->from_y;
        }
        else {
            data->display_x = data->ev_x;
            data->display_y = data->ev_y;
        }
    }
    else {
        data->ev_x = event->x;
        data->ev_y = event->y;

        if ((data->adj_width > 0) && (data->adj_height > 0)) {
            if (data->adj_width >= page_width)
                data->display_x = event->x + data->from_x;
            else
                data->display_x = event->x - ((page_width - data->adj_width) / 2) + data->from_x;

            if (data->adj_height >= page_height)
                data->display_y = event->y + data->from_y;
            else
                data->display_y = event->y - ((page_height - data->adj_height) / 2) + data->from_y;
        }
        else {
            data->display_x = data->ev_x;
            data->display_y = data->ev_y;
        }
    }

    if ((data->state != RUNNING) || ((data->ev_x >= 0 && data->ev_y >= 0) && (data->ev_y <= data->display_height && data->ev_x <= data->display_width))) {
        gpointer *buffer;
        GString *string;

        buffer = uca_ring_buffer_peek_pointer (data->buffer);
        string = g_string_new_len (NULL, 32);
        gint i = (data->display_y / data->zoom_factor) * data->width + data->display_x / data->zoom_factor;

        if (data->pixel_size == 1) {
            guint8 *input = (guint8 *) buffer;
            guint8 val = input[i];
            g_string_printf (string, "val = %i", val);
            gtk_label_set_text (data->val_label, string->str);
        }
        else if (data->pixel_size == 2) {
            guint16 *input = (guint16 *) buffer;
            guint16 val = input[i];
            g_string_printf (string, "val = %i", val);
            gtk_label_set_text (data->val_label, string->str);
        }

        g_string_printf (string, "x = %i", data->display_x);
        gtk_label_set_text (data->x_label, string->str);

        g_string_printf (string, "y = %i", data->display_y);
        gtk_label_set_text (data->y_label, string->str);

        g_string_free (string, TRUE);
    }
    if (data->cr != NULL) {
        gdouble dash = 5.0;
        cairo_set_source_rgb (data->cr, data->red, data->green, data->blue);
        gint rect_width = data->rect_evx - data->rect_x;
        gint rect_height = data->rect_evy - data->rect_y;
        cairo_rectangle (data->cr, data->rect_x, data->rect_y, rect_width, rect_height);
        cairo_set_dash (data->cr, &dash, 1, 0);
        cairo_stroke (data->cr);
        gtk_widget_queue_draw (event_box);
    }
}

static void
normalize_event_coords (ThreadData *data)
{
    if (data->ev_x < 0)
        data->ev_x = 0;

    if (data->ev_y < 0)
        data->ev_y = 0;

    if (data->ev_x > data->display_width)
        data->ev_x = data->display_width;

    if (data->ev_y > data->display_height)
        data->ev_y = data->display_height;
}

static void
on_button_press (GtkWidget *event_box, GdkEventMotion *event, ThreadData *data)
{
    data->cr = gdk_cairo_create(event_box->window);
    data->rect_x = event->x;
    data->rect_y = event->y;

    normalize_event_coords (data);

    gtk_adjustment_set_upper (GTK_ADJUSTMENT (data->x_adjustment), data->display_width);
    gtk_adjustment_set_upper (GTK_ADJUSTMENT (data->y_adjustment), data->display_height);
    gtk_adjustment_set_value (GTK_ADJUSTMENT (data->x_adjustment), data->ev_x);
    gtk_adjustment_set_value (GTK_ADJUSTMENT (data->y_adjustment), data->ev_y);

    data->tmp_fromx = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->x_adjustment));
    data->tmp_fromy = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->y_adjustment));

    data->zoom_before = data->zoom_factor;
}

static void
on_button_release (GtkWidget *event_box, GdkEventMotion *event, ThreadData *data)
{
    cairo_destroy (data->cr);
    data->cr = NULL;

    normalize_event_coords (data);

    gtk_adjustment_set_upper (GTK_ADJUSTMENT (data->width_adjustment), data->display_width);
    gtk_adjustment_set_upper (GTK_ADJUSTMENT (data->height_adjustment), data->display_height);
    gtk_adjustment_set_value (GTK_ADJUSTMENT (data->width_adjustment), data->ev_x);
    gtk_adjustment_set_value (GTK_ADJUSTMENT (data->height_adjustment), data->ev_y);

    gint tmp_tox = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->width_adjustment));
    gint tmp_toy = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->height_adjustment));

    if (data->tmp_fromx > tmp_tox) {
        data->from_x = tmp_tox;
        data->to_x = data->tmp_fromx;
    }
    else {
        data->from_x = data->tmp_fromx;
        data->to_x = tmp_tox;
    }
    if (data->tmp_fromy > tmp_toy) {
        data->from_y = tmp_toy;
        data->to_y = data->tmp_fromy;
    }
    else {
        data->from_y = data->tmp_fromy;
        data->to_y = tmp_toy;
    }

    data->adj_width = data->to_x - data->from_x;
    data->adj_height = data->to_y - data->from_y;

    update_pixbuf (data);
}

static gboolean
on_expose (GtkWidget *event_box, GdkEventExpose *event, ThreadData *data)
{
    if (data->cr != NULL) {
        gdouble dash = 5.0;
        cairo_set_source_rgb (data->cr, data->red, data->green, data->blue);
        gint rect_width = data->rect_evx - data->rect_x;
        gint rect_height = data->rect_evy - data->rect_y;
        cairo_rectangle (data->cr, data->rect_x, data->rect_y, rect_width, rect_height);
        cairo_set_dash (data->cr, &dash, 1, 0);
        cairo_stroke (data->cr);
        gtk_widget_queue_draw (event_box);
    }
    return FALSE;
}

static void
update_pixbuf (ThreadData *data)
{
    GString *string;
    gdouble mean;
    gdouble sigma;
    guint min;
    guint max;
    guint width;
    guint height;
    guint x = 0;
    guint y = 0;

    gdk_flush ();

    if (data->adj_width > 0 && data->adj_height > 0) {
        data->subpixbuf = gdk_pixbuf_new_subpixbuf (data->pixbuf, data->from_x, data->from_y, data->adj_width, data->adj_height);
        gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), data->subpixbuf);
    }
    else {
        gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), data->pixbuf);
    }

    gtk_widget_queue_draw_area (data->image, 0, 0, data->display_width, data->display_height);

    if ((data->adj_width > 0) && (data->adj_height > 0)) {
        x = data->from_x;
        y = data->from_y;
        width = data->adj_width;
        height = data->adj_height;
    }
    else {
        width = data->display_width;
        height = data->display_height;
    }

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

    g_string_printf (string, "x = %i", x);
    gtk_label_set_text (data->roix_label, string->str);

    g_string_printf (string, "y = %i", y);
    gtk_label_set_text (data->roiy_label, string->str);

    g_string_printf (string, "width = %i", width);
    gtk_label_set_text (data->roiw_label, string->str);

    g_string_printf (string, "height = %i", height);
    gtk_label_set_text (data->roih_label, string->str);

    g_string_free (string, TRUE);

    if (gtk_toggle_button_get_active (data->histogram_button))
        gtk_widget_queue_draw (data->histogram_view);
}

static void
update_pixbuf_dimensions (ThreadData *data)
{
    if (data->pixbuf != NULL)
        g_object_unref (data->pixbuf);

    if (data->adj_width > 0 && data->adj_height > 0) {
        gdouble zoom = data->zoom_factor / data->zoom_before;

        data->from_x = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->x_adjustment)) * zoom;
        data->from_y = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->y_adjustment)) * zoom;
        data->to_x = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->width_adjustment)) * zoom;
        data->to_y = gtk_adjustment_get_value (GTK_ADJUSTMENT (data->height_adjustment)) * zoom;

        gint adj_x = data->from_x;
        gint adj_y = data->from_y;

        if (data->from_x > data->to_x) {
            data->from_x = data->to_x;
            data->to_x = adj_x;
        }
        if (data->from_y > data->to_y) {
            data->from_y = data->to_y;
            data->to_y = adj_y;
        }
        data->adj_width = data->to_x - data->from_x;
        data->adj_height = data->to_y - data->from_y;
    }

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
    gtk_widget_set_sensitive (data->colormap_box,
                              data->state == IDLE);
}

static gpointer
grab_async (void *args)
{
    ThreadData *data = (ThreadData *) args;
    GError *error = NULL;

    while (data->state == RUNNING) {
        uca_camera_grab (data->camera, data->shadow, &error);

        if (error != NULL)
            print_and_free_error (&error);
    }

    return NULL;
}

static gpointer
preview_frames (void *args)
{
    ThreadData *data = (ThreadData *) args;
    gint counter = 0;
    GError *error = NULL;

    data->n_recorded = 0;
    data->shadow = g_malloc (uca_ring_buffer_get_block_size (data->buffer));

    uca_camera_grab (data->camera, data->shadow, &error);

    if (!g_thread_create (grab_async, data, FALSE, &error)) {
        g_printerr ("Failed to create thread: %s\n", error->message);
        data->state = IDLE;
        set_tool_button_state (data);
        return NULL;
    }

    while (data->state == RUNNING) {
        up_and_down_scale (data, data->shadow);

        gdk_threads_enter ();

        update_pixbuf (data);
        egg_histogram_view_update (EGG_HISTOGRAM_VIEW (data->histogram_view), data->shadow);

        if ((data->ev_x >= 0) && (data->ev_y >= 0) &&
            (data->ev_y <= data->display_height) && (data->ev_x <= data->display_width)) {
            GString *string;
            string = g_string_new_len (NULL, 32);
            gint i = (data->display_y / data->zoom_factor) * data->width + data->display_x / data->zoom_factor;

            if (data->pixel_size == 1) {
                guint8 *input = (guint8 *) data->shadow;
                guint8 val = input[i];
                g_string_printf (string, "val = %i", val);
                gtk_label_set_text (data->val_label, string->str);
            }
            else if (data->pixel_size == 2) {
                guint16 *input = (guint16 *) data->shadow;
                guint16 val = input[i];
                g_string_printf (string, "val = %i", val);
                gtk_label_set_text (data->val_label, string->str);
            }

            g_string_printf (string, "x = %i", data->display_x);
            gtk_label_set_text (data->x_label, string->str);

            g_string_printf (string, "y = %i", data->display_y);
            gtk_label_set_text (data->y_label, string->str);

            g_string_free (string, TRUE);
        }

        gdk_threads_leave ();

        counter++;
    }

    up_and_down_scale (data, data->shadow);
    gdk_threads_enter ();
    update_pixbuf (data);
    gdk_threads_leave ();

    g_free (data->shadow);
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
    uca_ring_buffer_reset (data->buffer);

    data->n_recorded = 0;
    n_max = (guint) gtk_adjustment_get_value (data->count);

    while (1) {
        if (data->state != RECORDING)
            break;

        if (n_max > 0 && n_frames >= n_max)
            break;

        buffer = uca_ring_buffer_get_write_pointer (data->buffer);
        uca_camera_grab (data->camera, buffer, NULL);
        uca_ring_buffer_write_advance (data->buffer);

        if (error == NULL) {
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

    n_frames = uca_ring_buffer_get_num_blocks (data->buffer);

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
    g_object_unref (data->buffer);
    gtk_main_quit ();
}

static void
update_current_frame (ThreadData *data)
{
    gpointer buffer;
    guint index;
    guint n_max;

    index = (guint) gtk_adjustment_get_value (data->frame_slider);
    n_max = uca_ring_buffer_get_num_blocks (data->buffer);

    if (n_max > 0 && data->n_recorded > 0) {
        /* Shift index so that we always show the oldest frames first */
        index = (index + data->n_recorded - n_max) % n_max;
        buffer = uca_ring_buffer_get_pointer (data->buffer, index);
    }
    else {
        /* we were in preview mode. Grab the 'next' frame in the buffer */
        uca_ring_buffer_write_advance (data->buffer);
        buffer = uca_ring_buffer_get_read_pointer (data->buffer);
    }

    egg_histogram_view_update (EGG_HISTOGRAM_VIEW (data->histogram_view), buffer);
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
write_raw_file (const gchar *filename, UcaRingBuffer *buffer)
{
    FILE *fp;
    guint n_blocks;
    gsize size;

    fp = fopen (filename, "wb");

    if (fp == NULL)
        return FALSE;

    n_blocks = uca_ring_buffer_get_num_blocks (buffer);
    size = uca_ring_buffer_get_block_size (buffer);

    for (guint i = 0; i < n_blocks; i++)
        fwrite (uca_ring_buffer_get_pointer (buffer, i), size , 1, fp);

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

    uca_ring_buffer_reset (data->buffer);

    while (error == NULL) {
        buffer = uca_ring_buffer_get_write_pointer (data->buffer);
        uca_camera_grab (data->camera, buffer, &error);
        uca_ring_buffer_write_advance (data->buffer);

        gdk_threads_enter ();
        gtk_adjustment_set_value (data->download_adjustment, current_frame++);
        gdk_threads_leave ();
    }

    if (error->code == UCA_CAMERA_ERROR_END_OF_STREAM) {
        guint n_frames = uca_ring_buffer_get_num_blocks (data->buffer);

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
update_zoomed_pixbuf (ThreadData *data)
{
    if (data->state == RUNNING) {
        up_and_down_scale (data, uca_ring_buffer_peek_pointer (data->buffer));
        update_pixbuf (data);
        update_pixbuf_dimensions (data);
    }
    else {
        update_pixbuf_dimensions (data);
        up_and_down_scale (data, uca_ring_buffer_peek_pointer (data->buffer));
        update_pixbuf (data);
    }
}

static void
on_zoom_in_button_clicked (GtkWidget *widget, ThreadData *data)
{
    data->zoom_factor *= 2;
    update_zoomed_pixbuf (data);
}

static void
on_zoom_out_button_clicked (GtkWidget *widget, ThreadData *data)
{
    data->zoom_factor /= 2;
    update_zoomed_pixbuf (data);
}

static void
on_zoom_normal_button_clicked (GtkWidget *widget, ThreadData *data)
{
    data->zoom_factor = 1.0;
    update_zoomed_pixbuf (data);
}

static void
on_rect_color_button_clicked (GtkWidget *widget, ThreadData *data)
{
    if ((data->red == 0.0 && data->green == 0.0) && data->blue == 0.0) {
        data->red = 1.0;
        data->green = 1.0;
        data->blue = 1.0;
    }
    else {
        data->red = 0.0;
        data->green = 0.0;
        data->blue = 0.0;
    }
}

static void
on_histogram_changed (EggHistogramView *view, ThreadData *data)
{
    if (data->state == IDLE)
        update_current_frame (data);
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

    up_and_down_scale (data, uca_ring_buffer_peek_pointer (data->buffer));
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
    UcaRingBuffer   *ring_buffer;
    gsize image_size;
    guint n_frames;
    guint bits_per_sample;
    guint pixel_size;
    guint width, height;
    GError  *error = NULL;

    camera = uca_plugin_manager_get_camera (plugin_manager, camera_name, &error, NULL);

    if ((camera == NULL) || (error != NULL)) {
        if (error) {
            g_error ("%s\n", error->message);
        }
        else {
            g_error ("Failed to load '%s' camera plugin for an unknown reason\n", camera_name);
        }
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

    td.colormap_box     = GTK_WIDGET (gtk_builder_get_object (builder, "colormap-box"));
    td.start_button     = GTK_WIDGET (gtk_builder_get_object (builder, "start-button"));
    td.stop_button      = GTK_WIDGET (gtk_builder_get_object (builder, "stop-button"));
    td.record_button    = GTK_WIDGET (gtk_builder_get_object (builder, "record-button"));
    td.download_button  = GTK_WIDGET (gtk_builder_get_object (builder, "download-button"));
    td.zoom_in_button   = GTK_WIDGET (gtk_builder_get_object (builder, "zoom-in-button"));
    td.zoom_out_button  = GTK_WIDGET (gtk_builder_get_object (builder, "zoom-out-button"));
    td.zoom_normal_button = GTK_WIDGET (gtk_builder_get_object (builder, "zoom-normal-button"));
    td.rect_color_button = GTK_WIDGET (gtk_builder_get_object (builder, "rectangle-button"));
    td.download_button  = GTK_WIDGET (gtk_builder_get_object (builder, "download-button"));
    td.histogram_button = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "histogram-checkbutton"));
    td.log_button       = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "logarithm-checkbutton"));
    td.frame_slider     = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "frames-adjustment"));
    td.count            = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "acquisitions-adjustment"));

    td.hadjustment      = GTK_ADJUSTMENT (gtk_builder_get_object(builder, "hadjustment"));
    td.vadjustment      = GTK_ADJUSTMENT (gtk_builder_get_object(builder, "vadjustment"));
    td.x_adjustment      = GTK_ADJUSTMENT (gtk_builder_get_object(builder, "x_adjustment"));
    td.y_adjustment      = GTK_ADJUSTMENT (gtk_builder_get_object(builder, "y_adjustment"));
    td.width_adjustment  = GTK_ADJUSTMENT (gtk_builder_get_object(builder, "width_adjustment"));
    td.height_adjustment = GTK_ADJUSTMENT (gtk_builder_get_object(builder, "height_adjustment"));
    td.event_box        = GTK_WIDGET (gtk_builder_get_object(builder, "eventbox"));

    td.mean_label       = GTK_LABEL (gtk_builder_get_object (builder, "mean-label"));
    td.sigma_label      = GTK_LABEL (gtk_builder_get_object (builder, "sigma-label"));
    td.max_label        = GTK_LABEL (gtk_builder_get_object (builder, "max-label"));
    td.min_label        = GTK_LABEL (gtk_builder_get_object (builder, "min-label"));

    td.x_label          = GTK_LABEL (gtk_builder_get_object (builder, "x-label1"));
    td.y_label          = GTK_LABEL (gtk_builder_get_object (builder, "y-label1"));
    td.val_label        = GTK_LABEL (gtk_builder_get_object (builder, "val-label1"));
    td.roix_label        = GTK_LABEL (gtk_builder_get_object (builder, "roix-label"));
    td.roiy_label        = GTK_LABEL (gtk_builder_get_object (builder, "roiy-label"));
    td.roiw_label        = GTK_LABEL (gtk_builder_get_object (builder, "roiw-label"));
    td.roih_label        = GTK_LABEL (gtk_builder_get_object (builder, "roih-label"));

    td.download_dialog  = GTK_DIALOG (gtk_builder_get_object (builder, "download-dialog"));
    td.download_adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "download-adjustment"));

    /* Set initial data */
    pixel_size  = bits_per_sample > 8 ? 2 : 1;
    image_size  = pixel_size * width * height;
    n_frames    = mem_size * 1024 * 1024 / image_size;
    ring_buffer = uca_ring_buffer_new (image_size, n_frames);

    egg_histogram_view_update (EGG_HISTOGRAM_VIEW (histogram_view),
                               uca_ring_buffer_peek_pointer (ring_buffer));

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

    g_signal_connect (gtk_builder_get_object (builder, "colormap-box"),
                      "changed", G_CALLBACK (on_colormap_changed), &td);

    g_signal_connect (td.event_box, "motion-notify-event", G_CALLBACK (on_motion_notify), &td);
    g_signal_connect (td.event_box, "button-press-event", G_CALLBACK (on_button_press), &td);
    g_signal_connect (td.event_box, "button-release-event", G_CALLBACK (on_button_release), &td);
    g_signal_connect (td.event_box, "expose-event", G_CALLBACK (on_expose), &td);
    g_signal_connect (td.frame_slider, "value-changed", G_CALLBACK (on_frame_slider_changed), &td);
    g_signal_connect (td.start_button, "clicked", G_CALLBACK (on_start_button_clicked), &td);
    g_signal_connect (td.stop_button, "clicked", G_CALLBACK (on_stop_button_clicked), &td);
    g_signal_connect (td.record_button, "clicked", G_CALLBACK (on_record_button_clicked), &td);
    g_signal_connect (td.download_button, "clicked", G_CALLBACK (on_download_button_clicked), &td);
    g_signal_connect (td.zoom_in_button, "clicked", G_CALLBACK (on_zoom_in_button_clicked), &td);
    g_signal_connect (td.zoom_out_button, "clicked", G_CALLBACK (on_zoom_out_button_clicked), &td);
    g_signal_connect (td.zoom_normal_button, "clicked", G_CALLBACK (on_zoom_normal_button_clicked), &td);
    g_signal_connect (td.rect_color_button, "clicked", G_CALLBACK (on_rect_color_button_clicked), &td);
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
        g_warning ("Cannot load UI: `%s'. Trying in current working directory.", error->message);
        g_error_free (error);
        error = NULL;

        if (!gtk_builder_add_from_file (builder, "control.glade", &error)) {
            g_warning ("Cannot load UI: `%s'.", error->message);
            return 1;
        }
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
