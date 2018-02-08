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
#include <string.h>

#include "uca-camera.h"
#include "uca-plugin-manager.h"
#include "uca-ring-buffer.h"
#include "egg-property-tree-view.h"
#include "egg-histogram-view.h"
#include "resources.h"

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
    GtkWidget   *zoom_in_button;
    GtkWidget   *zoom_out_button;
    GtkWidget   *zoom_normal_button;
    GtkWidget   *rect_color_button;
    GtkWidget   *acquisition_expander;
    GtkWidget   *properties_expander;
    GtkWidget   *histogram_view;
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

    GtkAdjustment   *download_adjustment;
    GtkAdjustment   *count;
    GtkAdjustment   *hadjustment;
    GtkAdjustment   *vadjustment;
    GtkAdjustment   *x_adjustment;
    GtkAdjustment   *y_adjustment;
    GtkAdjustment   *width_adjustment;
    GtkAdjustment   *height_adjustment;
    GtkAdjustment   *frame_slider;

    GtkDialog       *download_dialog;
    GtkProgressBar  *download_progressbar;
    GtkToggleButton *histogram_button;
    GtkToggleButton *log_button;
    UcaRingBuffer   *buffer;
    guchar          *shadow;
    guchar          *pixels;
    cairo_t         *cr;
    State           state;
    guint           n_recorded;
    gboolean        data_in_camram;
    gboolean        stopped;

    gint         display_width, display_height;
    gint         pixbuf_width, pixbuf_height;
    gint         page_width, page_height;
    gint         adj_width, adj_height;
    gint         sub_width, sub_height;
    gint         width, height;
    gint         display_x, display_y;
    gint         tmp_fromx, tmp_fromy;
    gint         rect_evx, rect_evy;
    gint         side_x, side_y;
    gint         rect_x, rect_y;
    gint         from_x, from_y;
    gint         to_x, to_y;
    gint         ev_x, ev_y;
    gint         colormap;
    gint         timestamp;
    gint         pixel_size;
    gint         resize_counter;
    gint         mm_width, mm_height;

    gdouble      zoom_factor;
    gdouble      zoom_before;
    gdouble      red, green, blue;
    gdouble      percent_width, percent_height;
} ThreadData;

static UcaPluginManager *plugin_manager;
static gsize mem_size = 2048;

static void update_pixbuf (ThreadData *data, gpointer buffer);
static void update_pixbuf_dimensions (ThreadData *data);

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
    gint min_x;
    gint min_y;
    gint max_x;
    gint max_y;
    gint current_x;
    gint current_y;
    gint current_width;
    gint current_height;

    current_width = gtk_adjustment_get_page_size (data->hadjustment);
    current_height = gtk_adjustment_get_page_size (data->vadjustment);
    if (((current_width > data->page_width || current_height > data->page_height)
     && ((current_width - data->page_width) >= 15 || (current_height - data->page_height) >= 15))
     || ((current_width < data->page_width || current_height < data->page_height)
     && ((data->page_width - current_width) >= 15 || (data->page_height - current_height) >= 15))) {
        data->resize_counter += 1;
        if (data->resize_counter > 30) {
            update_pixbuf_dimensions (data);
        }
    }

    egg_histogram_get_range (EGG_HISTOGRAM_VIEW (data->histogram_view), &min, &max);
    factor = 255.0 / (max - min);
    output = data->pixels;
    zoom = (gint) data->zoom_factor;
    stride = (gint) 1 / data->zoom_factor;
    do_log = gtk_toggle_button_get_active (data->log_button);
    min_x = gtk_adjustment_get_value (data->hadjustment);
    min_y = gtk_adjustment_get_value (data->vadjustment);
    current_x = min_x;
    current_y = min_y;

    if (data->adj_width > 0 && data->adj_height > 0) {
        if (data->stopped == TRUE) {
            min_x = data->from_x;
            min_y = data->from_y;
        }
        else {
            min_x = data->from_x + current_x;
            min_y = data->from_y + current_y;
        }
        max_x = data->pixbuf_width + min_x;
        max_y = data->pixbuf_height + min_y;
    }
    else if (data->stopped == FALSE) {
        if (data->display_width <= data->page_width || data->pixbuf_width == data->display_width)
            max_x = data->display_width;
        else
            max_x = data->pixbuf_width + min_x;

        if (data->display_height <= data->page_height || data->pixbuf_height == data->display_height)
            max_y = data->display_height;
        else
            max_y = data->pixbuf_height + min_y;
    }
    else {
        min_x = 0;
        min_y = 0;
        max_x = data->display_width;
        max_y = data->display_height;
    }

    if (max_x > data->display_width) {
        gint diff_x = max_x - data->display_width;
        if ((min_x - diff_x) >= 0) {
            min_x -= diff_x;
            max_x -= diff_x;
        }
        else {
            min_x = 0;
            max_x = data->pixbuf_width;
        }
    }

    if (max_y > data->display_height) {
        gint diff_y = max_y - data->display_height;
        if ((min_y - diff_y) >= 0) {
            min_y -= diff_y;
            max_y -= diff_y;
        }
        else {
            min_y = 0;
            max_y = data->pixbuf_height;
        }
    }

    if ((data->display_width <= data->page_width) || (data->page_width == 1) ||
        (data->adj_width > 0 && data->adj_height > 0 && data->adj_width <= data->page_width))
        data->percent_width = 0.5;
    else if ((min_x == 0 && data->display_width >= data->page_width) || (data->adj_width > 0 && data->adj_height > 0 && data->adj_width > data->page_width && data->stopped == TRUE))
        data->percent_width = 0.0;
    else if (data->adj_width > 0 && data->adj_height > 0 && data->adj_width > data->page_width)
        data->percent_width = current_x / (gdouble) (data->adj_width - data->page_width);
    else
        data->percent_width = min_x / (gdouble) (data->display_width - data->page_width);

    if ((data->display_height<= data->page_height) || (data->page_height == 1) ||
        (data->adj_width > 0 && data->adj_height > 0 && data->adj_height <= data->page_height))
        data->percent_height = 0.5;
    else if ((min_y == 0 && data->display_height >= data->page_height) || (data->adj_width > 0 && data->adj_height > 0 && data->adj_width > data->page_width && data->stopped == TRUE))
        data->percent_height = 0.0;
    else if (data->adj_width > 0 && data->adj_height > 0 && data->adj_height > data->page_height)
        data->percent_height = current_y / (gdouble) (data->adj_height - data->page_height);
    else
        data->percent_height = min_y / (gdouble) (data->display_height - data->page_height);

    gtk_misc_set_alignment (GTK_MISC(data->image), data->percent_width, data->percent_height);

    if (data->pixel_size == 1) {
        guint8 *input = (guint8 *) buffer;
        for (gint y = min_y; y < max_y; y++) {
            for (gint x = min_x; x < max_x; x++) {
                if (zoom <= 1)
                    offset = (y * stride * data->width) + (x * stride);
                else
                    offset = ((gint) (y / zoom) * data->width) + ((gint) (x / zoom));

                if (do_log)
                    dval = log ((input[offset] - min) * factor);
                else
                    dval = (input[offset] - min) * factor;

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

        for (gint y = min_y; y < max_y; y++) {
            for (gint x = min_x; x < max_x; x++) {
                if (zoom <= 1)
                    offset = (y * stride * data->width) + (x * stride);
                else
                    offset = ((gint) (y / zoom) * data->width) + ((gint) (x / zoom));

                if (do_log)
                    dval = log ((input[offset] - min) * factor);
                else
                    dval = (input[offset] - min) * factor;

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
get_statistics (ThreadData *data, gdouble *mean, gdouble *sigma, guint *_max, guint *_min, gpointer buffer)
{
    gdouble sum = 0.0;
    gdouble squared_sum = 0.0;
    guint min = G_MAXUINT;
    guint max = 0;
    guint n = data->width * data->height;

    if (data->pixel_size == 1) {
        guint8 *input = (guint8 *) buffer;

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
        guint16 *input = (guint16 *) buffer;

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
update_sidebar (ThreadData *data, gpointer buffer)
{
    gchar string[32];

    if (data->display_x < 0)
        data->side_x = 0;
    else if (data->display_x > data->display_width - 1)
        data->side_x = data->width - 1;
    else
        data->side_x = data->display_x / data->zoom_factor;

    if (data->display_y < 0)
        data->side_y = 0;
    else if (data->display_y > data->display_height - 1)
        data->side_y = data->height - 1;
    else
        data->side_y = data->display_y / data->zoom_factor;

    gint i = data->side_y * data->width + data->side_x;

    if (data->pixel_size == 1) {
        guint8 *input = (guint8 *) buffer;
        guint8 val = input[i];
        g_snprintf (string, 32, "val = %i", val);
        gtk_label_set_text (data->val_label, string);
    }
    else if (data->pixel_size == 2) {
        guint16 *input = (guint16 *) buffer;
        guint16 val = input[i];
        g_snprintf (string, 32, "val = %i", val);
        gtk_label_set_text (data->val_label, string);
    }

    g_snprintf (string, 32, "x = %i", data->side_x);
    gtk_label_set_text (data->x_label, string);

    g_snprintf (string, 32, "y = %i", data->side_y);
    gtk_label_set_text (data->y_label, string);
}

static void
on_motion_notify (GtkWidget *event_box, GdkEventMotion *event, ThreadData *data)
{
    gint page_width = gtk_adjustment_get_page_size (data->hadjustment);
    gint page_height = gtk_adjustment_get_page_size (data->vadjustment);
    gint start_wval = gtk_adjustment_get_value (data->hadjustment);
    gint start_hval = gtk_adjustment_get_value (data->vadjustment);

    page_width += start_wval;
    page_height += start_hval;
    data->rect_evx = event->x;
    data->rect_evy = event->y;

    if (data->display_width < page_width) {
        gint startx = (page_width - data->display_width) / 2;
        data->ev_x = event->x - startx;
        if ((data->adj_width > 0) && (data->adj_height > 0))
            data->display_x = event->x - ((page_width - data->adj_width) / 2) + data->from_x;
        else
            data->display_x = data->ev_x;
    }
    else {
        data->ev_x = event->x;
        if ((data->adj_width > 0) && (data->adj_height > 0)) {
            if (data->adj_width >= page_width)
                data->display_x = event->x + data->from_x;
            else
                data->display_x = event->x - ((page_width - data->adj_width) / 2) + data->from_x;
        }
        else {
            data->display_x = data->ev_x;
        }
    }

    if (data->display_height < page_height) {
        gint starty = (page_height - data->display_height) / 2;
        data->ev_y = event->y - starty;
        if ((data->adj_width > 0) && (data->adj_height > 0))
            data->display_y = event->y - ((page_height - data->adj_height) / 2) + data->from_y;
        else
            data->display_y = data->ev_y;
    }
    else {
        data->ev_y = event->y;
        if ((data->adj_width > 0) && (data->adj_height > 0)) {
            if (data->adj_height >= page_height)
                data->display_y = event->y + data->from_y;
            else
                data->display_y = event->y - ((page_height - data->adj_height) / 2) + data->from_y;
        }
        else {
            data->display_y = data->ev_y;
        }
    }

    if ((data->state != RUNNING) || ((data->ev_x >= 0 && data->ev_y >= 0) && (data->ev_y <= data->display_height && data->ev_x <= data->display_width))) {
        update_sidebar (data, uca_ring_buffer_peek_pointer (data->buffer));
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

    gtk_adjustment_set_upper (data->x_adjustment, data->display_width);
    gtk_adjustment_set_upper (data->y_adjustment, data->display_height);
    if (data->adj_width > 0 && data->adj_height > 0) {
        gtk_adjustment_set_value (data->x_adjustment, data->display_x);
        gtk_adjustment_set_value (data->y_adjustment, data->display_y);
    }
    else {
        gtk_adjustment_set_value (data->x_adjustment, data->ev_x);
        gtk_adjustment_set_value (data->y_adjustment, data->ev_y);
    }

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

    gtk_adjustment_set_upper (data->width_adjustment, data->display_width);
    gtk_adjustment_set_upper (data->height_adjustment, data->display_height);
    if (data->adj_width > 0 && data->adj_height > 0) {
        gtk_adjustment_set_value (data->width_adjustment, data->display_x);
        gtk_adjustment_set_value (data->height_adjustment, data->display_y);
    }
    else {
        gtk_adjustment_set_value (data->width_adjustment, data->ev_x);
        gtk_adjustment_set_value (data->height_adjustment, data->ev_y);
    }

    gint tmp_tox = gtk_adjustment_get_value (data->width_adjustment);
    gint tmp_toy = gtk_adjustment_get_value (data->height_adjustment);

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

    update_pixbuf_dimensions (data);
    up_and_down_scale (data, uca_ring_buffer_peek_pointer (data->buffer));
    update_pixbuf (data, uca_ring_buffer_peek_pointer (data->buffer));
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
update_pixbuf (ThreadData *data, gpointer buffer)
{
    gchar string[32];
    gdouble mean;
    gdouble sigma;
    guint min;
    guint max;
    guint width;
    guint height;
    guint x = 0;
    guint y = 0;
    gint sub_x = 0;
    gint sub_y = 0;
    gint sub_width = 0;
    gint sub_height = 0;

    gdk_flush ();

    if (data->sub_width != 0 || data->sub_height != 0) {
        sub_width = data->sub_width;
        sub_height = data->sub_height;
    }
    else {
        sub_x = gtk_adjustment_get_value (data->hadjustment);
        sub_y = gtk_adjustment_get_value (data->vadjustment);
        if (sub_x == 0 && data->pixbuf_width == data->display_width)
            sub_width = data->pixbuf_width - 1;
        if (sub_y == 0 && data->pixbuf_height == data->display_height)
            sub_height = data->pixbuf_height - 1;
        if (sub_width != 0 && sub_height == 0)
            sub_height = data->pixbuf_height;
        if (sub_width == 0 && sub_height != 0)
            sub_width = data->pixbuf_width;
    }

    if ((sub_width > 0 && sub_height > 0) && (data->pixbuf_width < data->page_width || data->pixbuf_height < data->page_height)) {
        GdkPixbuf *subpixbuf = gdk_pixbuf_new_subpixbuf (data->pixbuf, sub_x, sub_y, sub_width, sub_height);
        gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), subpixbuf);
    }
    else {
        gtk_image_set_from_pixbuf (GTK_IMAGE (data->image), data->pixbuf);
    }
    gtk_widget_queue_draw (data->image);

    if (data->adj_width > 0 && data->adj_height > 0) {
        x = data->from_x;
        y = data->from_y;
        width = data->pixbuf_width;
        height = data->pixbuf_height;
    }
    else {
        width = data->display_width;
        height = data->display_height;
    }

    get_statistics (data, &mean, &sigma, &max, &min, buffer);

    g_snprintf (string, 32, "\u03bc = %3.2f", mean);
    gtk_label_set_text (data->mean_label, string);

    g_snprintf (string, 32, "\u03c3 = %3.2f", sigma);
    gtk_label_set_text (data->sigma_label, string);

    g_snprintf (string, 32, "min = %i", min);
    gtk_label_set_text (data->min_label, string);

    g_snprintf (string, 32, "max = %i", max);
    gtk_label_set_text (data->max_label, string);

    g_snprintf (string, 32, "x = %i", x);
    gtk_label_set_text (data->roix_label, string);

    g_snprintf (string, 32, "y = %i", y);
    gtk_label_set_text (data->roiy_label, string);

    g_snprintf (string, 32, "width = %i", width);
    gtk_label_set_text (data->roiw_label, string);

    g_snprintf (string, 32, "height = %i", height);
    gtk_label_set_text (data->roih_label, string);

    if (gtk_toggle_button_get_active (data->histogram_button))
        gtk_widget_queue_draw (data->histogram_view);
}

static void
update_pixbuf_dimensions (ThreadData *data)
{
    if (data->pixbuf != NULL)
        g_object_unref (data->pixbuf);

    gint filling_up;
    gint width_remainder;
    gint height_remainder;
    gint event_width;
    gint event_height;

    data->display_width = (gint) data->width * data->zoom_factor;
    data->display_height = (gint) data->height * data->zoom_factor;
    filling_up = data->pixel_size * 4;
    data->resize_counter = 0;
    data->sub_width = 0;
    data->sub_height = 0;

    if ((gint) data->zoom_factor == 32)
        gtk_widget_set_sensitive (data->zoom_in_button, FALSE);
    else
        gtk_widget_set_sensitive (data->zoom_in_button, TRUE);

    if (data->adj_width > 0 && data->adj_height > 0) {
        gdouble zoom = data->zoom_factor / data->zoom_before;
        data->from_x = gtk_adjustment_get_value (data->x_adjustment) * zoom;
        data->from_y = gtk_adjustment_get_value (data->y_adjustment) * zoom;
        data->to_x = gtk_adjustment_get_value (data->width_adjustment) * zoom;
        data->to_y = gtk_adjustment_get_value (data->height_adjustment) * zoom;

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

        if ((data->adj_width > data->page_width) && (data->stopped == FALSE))
            data->pixbuf_width = data->page_width;
        else
            data->pixbuf_width = data->adj_width;

        if ((data->adj_height > data->page_height) && (data->stopped == FALSE))
            data->pixbuf_height = data->page_height;
        else
            data->pixbuf_height = data->adj_height;
    }
    else {
        data->page_width = gtk_adjustment_get_page_size (data->hadjustment);
        data->page_height = gtk_adjustment_get_page_size (data->vadjustment);

        if (data->display_width <= data->page_width || data->stopped == TRUE || data->page_width == 1)
            data->pixbuf_width = data->display_width - 1;
        else
            data->pixbuf_width = data->page_width;

        if (data->display_height <= data->page_height || data->stopped == TRUE || data->page_height == 1)
            data->pixbuf_height = data->display_height - 1;
        else
            data->pixbuf_height = data->page_height;
    }

    width_remainder = data->pixbuf_width % filling_up;
    height_remainder = data->pixbuf_height % filling_up;

    if (width_remainder != 0 || height_remainder != 0) {
        data->sub_width = data->pixbuf_width;
        data->sub_height = data->pixbuf_height;
    }

    if (width_remainder != 0)
        data->pixbuf_width += (filling_up - width_remainder);
    if (height_remainder != 0)
        data->pixbuf_height += (filling_up - height_remainder);

    event_width = data->pixbuf_width;
    event_height = data->pixbuf_height;
    if ((data->display_width > event_width) && (data->adj_width <= 0 || data->adj_height <= 0))
        event_width = data->display_width;
    else if ((data->adj_width > event_width) && (data->adj_width > 0 && data->adj_height > 0 && data->stopped == FALSE))
        event_width = data->adj_width;

    if ((data->display_height > event_height) && (data->adj_width <= 0 || data->adj_height <= 0))
        event_height = data->display_height;
    else if ((data->adj_height > event_height) && (data->adj_width > 0 && data->adj_height > 0 && data->stopped == FALSE))
        event_height = data->adj_height;

    gtk_widget_set_size_request (data->event_box, event_width, event_height);
    data->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, data->pixbuf_width, data->pixbuf_height);
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
preview_frames (void *args)
{
    ThreadData *data = (ThreadData *) args;
    gint counter = 0;
    GError *error = NULL;

    data->n_recorded = 0;
    data->shadow = g_malloc (uca_ring_buffer_get_block_size (data->buffer));

    uca_camera_grab (data->camera, data->shadow, &error);

    while (data->state == RUNNING) {
        up_and_down_scale (data, data->shadow);
        uca_camera_grab (data->camera, data->shadow, &error);

        gdk_threads_enter ();

        update_pixbuf (data, data->shadow);
        egg_histogram_view_update (EGG_HISTOGRAM_VIEW (data->histogram_view), data->shadow);

        if ((data->ev_x >= 0) && (data->ev_y >= 0) && (data->ev_y <= data->display_height) && (data->ev_x <= data->display_width)) {
            update_sidebar (data, data->shadow);
        }

        gdk_threads_leave ();
        counter++;
    }

    up_and_down_scale (data, data->shadow);
    gdk_threads_enter ();
    update_pixbuf (data, data->shadow);
    gdk_threads_leave ();

    gpointer buffer = uca_ring_buffer_get_write_pointer (data->buffer);
    memcpy (buffer, data->shadow, uca_ring_buffer_get_block_size (data->buffer));
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
    update_pixbuf (data, buffer);
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
    data->stopped = FALSE;
    update_pixbuf_dimensions (data);

    uca_camera_start_recording (data->camera, &error);

    if (error != NULL) {
        g_printerr ("Failed to start recording: %s\n", error->message);
        return;
    }

    data->state = RUNNING;
    set_tool_button_state (data);

    /* FIXME: clean up struct */
    g_thread_new (NULL, preview_frames, data);
}

static void
on_stop_button_clicked (GtkWidget *widget, ThreadData *data, GtkAdjustment *adjustment)
{
    GError *error = NULL;

    g_object_get (data->camera, "has-camram-recording", &data->data_in_camram, NULL);
    data->state = IDLE;
    set_tool_button_state (data);
    uca_camera_stop_recording (data->camera, &error);
    data->stopped = TRUE;
    update_pixbuf_dimensions (data);

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

    /* FIXME: clean up struct */
    g_thread_new (NULL, record_frames, data);
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
    /* FIXME: clean up thread struct somewhere */
    g_thread_new (NULL, (GThreadFunc) download_frames, data);

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
        update_pixbuf (data, uca_ring_buffer_peek_pointer (data->buffer));
        update_pixbuf_dimensions (data);
    }
    else {
        update_pixbuf_dimensions (data);
        up_and_down_scale (data, uca_ring_buffer_peek_pointer (data->buffer));
        update_pixbuf (data, uca_ring_buffer_peek_pointer (data->buffer));
    }

    gint min_x;
    gint min_y;
    gint zoomed_min_x;
    gint zoomed_min_y;

    min_x = gtk_adjustment_get_value(data->hadjustment);
    min_y = gtk_adjustment_get_value(data->vadjustment);

    if ((data->percent_width != 0.0) || (data->percent_height != 0.0)) {
        if (min_x != 0) {
            zoomed_min_x = data->percent_width * (data->width * data->zoom_factor - data->page_width);
            gtk_adjustment_set_value (data->hadjustment, zoomed_min_x);
        }
        if (min_y != 0) {
            zoomed_min_y = data->percent_height * (data->height * data->zoom_factor - data->page_height);
            gtk_adjustment_set_value (data->vadjustment, zoomed_min_y);
        }
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
    update_pixbuf (data, uca_ring_buffer_peek_pointer (data->buffer));
}

static void
update_ring_buffer_dimensions (ThreadData *data)
{
    gsize image_size;
    guint num_frames;

    image_size = data->pixel_size * data->width * data->height;
    num_frames = mem_size  * 1024 * 1024 / image_size;

    if (data->buffer != NULL)
        g_object_unref (data->buffer);

    data->buffer = uca_ring_buffer_new (image_size, num_frames);
    g_message ("Allocated memory for %d frames", num_frames);
}

static void
on_roi_width_changed (GObject *object, GParamSpec *pspec, ThreadData *data)
{
    g_object_get (object, "roi-width", &data->width, NULL);
    update_ring_buffer_dimensions (data);
    update_pixbuf_dimensions (data);
}

static void
on_roi_height_changed (GObject *object, GParamSpec *pspec, ThreadData *data)
{
    g_object_get (object, "roi-height", &data->height, NULL);
    update_ring_buffer_dimensions (data);
    update_pixbuf_dimensions (data);
}

static void
on_sensor_bitdepth_changed (GObject *object, GParamSpec *pspec, ThreadData *data)
{
    guint bitdepth;
    gdouble max_value;
    g_object_get (object, "sensor-bitdepth", &bitdepth, NULL);
    data->pixel_size = bitdepth > 8 ? 2 : 1;
    max_value = pow (2, bitdepth);
    egg_histogram_view_set_max (EGG_HISTOGRAM_VIEW (data->histogram_view), max_value);
    update_ring_buffer_dimensions (data);
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
    guint bits_per_sample;
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

    memset (&td, 0, sizeof (td));

    g_signal_connect (camera, "notify::roi-width", (GCallback) on_roi_width_changed, &td);
    g_signal_connect (camera, "notify::roi-height", (GCallback) on_roi_height_changed, &td);
    g_signal_connect (camera, "notify::sensor-bitdepth", (GCallback) on_sensor_bitdepth_changed, &td);

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
    td.pixel_size = bits_per_sample > 8 ? 2 : 1;
    td.width  = td.display_width = width;
    td.height = td.display_height = height;
    update_ring_buffer_dimensions (&td);

    egg_histogram_view_update (EGG_HISTOGRAM_VIEW (histogram_view),
                               uca_ring_buffer_peek_pointer (td.buffer));

    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);

    gtk_adjustment_set_value (max_bin_adjustment, pow (2, bits_per_sample) - 1);
    gtk_adjustment_set_upper (td.count, (gdouble) G_MAXULONG);

    td.image  = image;
    td.state  = IDLE;
    td.camera = camera;
    td.zoom_factor = 1.0;
    td.colormap = 1;
    td.histogram_view = histogram_view;
    td.data_in_camram = FALSE;
    td.stopped = TRUE;
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

static gboolean
builder_add_from_resource (GtkBuilder *builder, const gchar *resource_path, GError **error)
{
    GBytes *data;
    const gchar *buffer;
    gsize length;
    gboolean ret;

    g_assert (error && *error == NULL);
    data = g_resources_lookup_data (resource_path, 0, error);

    if (data == NULL)
        return FALSE;

    length = 0;
    buffer = g_bytes_get_data (data, &length);
    g_assert (buffer != NULL);

    ret = gtk_builder_add_from_string (builder, buffer, length, error) > 0;
    g_bytes_unref (data);
    return ret;
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

    gdk_threads_init ();
    gtk_init (&argc, &argv);

    builder = gtk_builder_new ();

    if (!builder_add_from_resource (builder, "/edu/kit/ipe/uca-camera-control/uca-camera-control.glade", &error)) {
        g_warning ("Could not load UI: %s", error->message);
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
