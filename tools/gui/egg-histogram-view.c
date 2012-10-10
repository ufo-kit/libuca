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

#include <math.h>
#include "egg-histogram-view.h"

G_DEFINE_TYPE (EggHistogramView, egg_histogram_view, GTK_TYPE_DRAWING_AREA)

#define EGG_HISTOGRAM_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EGG_TYPE_HISTOGRAM_VIEW, EggHistogramViewPrivate))

#define MIN_WIDTH   128
#define MIN_HEIGHT  128
#define BORDER      2

struct _EggHistogramViewPrivate
{
    GdkCursorType cursor_type;
    gboolean      grabbing;

    /* This could be moved into a real histogram class */
    guint n_bins;
    gint *bins;
    gint  min_value;
    gint  max_value;
    gint  min_border;
    gint  max_border;

    gpointer data;
    gint     n_elements;
    gint     n_bits;
};

enum
{
    PROP_0,
    N_PROPERTIES
};

static GParamSpec *egg_histogram_view_properties[N_PROPERTIES] = { NULL, };


GtkWidget *
egg_histogram_view_new (void)
{
    EggHistogramView *view;

    view = EGG_HISTOGRAM_VIEW (g_object_new (EGG_TYPE_HISTOGRAM_VIEW, NULL));
    return GTK_WIDGET (view);
}

void
egg_histogram_view_set_data (EggHistogramView *view,
                             gpointer data,
                             guint n_elements,
                             guint n_bits,
                             guint n_bins)
{
    EggHistogramViewPrivate *priv;

    priv = view->priv;

    if (priv->bins != NULL)
        g_free (priv->bins);

    priv->data = data;
    priv->bins = g_malloc0 (n_bins * sizeof (guint));
    priv->n_bins = n_bins;
    priv->n_bits = n_bits;
    priv->n_elements = n_elements;

    priv->min_value = 0;
    priv->max_value = (gint) pow(2, n_bits) - 1;

    priv->min_border = 20;
    priv->max_border = priv->max_value - 20;
}

static void
compute_histogram (EggHistogramViewPrivate *priv)
{
    for (guint i = 0; i < priv->n_bins; i++)
        priv->bins[i] = 0;

    if (priv->n_bits == 8) {
        guint8 *data = (guint8 *) priv->data;

        for (guint i = 0; i < priv->n_elements; i++) {
            guint8 v = data[i];
            guint index = (guint) round (((gdouble) v) / priv->max_value * (priv->n_bins - 1));
            priv->bins[index]++;
        }
    }
    else if (priv->n_bits == 16) {
        guint16 *data = (guint16 *) priv->data;

        for (guint i = 0; i < priv->n_elements; i++) {
            guint16 v = data[i];
            guint index = (guint) floor (((gdouble ) v) / priv->max_value * (priv->n_bins - 1));
            priv->bins[index]++;
        }
    }
    else
        g_warning ("%i number of bits unsupported", priv->n_bits);
}

static void
set_cursor_type (EggHistogramView *view, GdkCursorType cursor_type)
{
    if (cursor_type != view->priv->cursor_type) {
        GdkCursor *cursor = gdk_cursor_new (cursor_type);

        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET(view)), cursor);
        gdk_cursor_unref (cursor);
        view->priv->cursor_type = cursor_type;
    }
}

static void
egg_histogram_view_size_request (GtkWidget *widget,
                                 GtkRequisition *requisition)
{
    requisition->width = MIN_WIDTH;
    requisition->height = MIN_HEIGHT;
}

static gboolean
egg_histogram_view_expose (GtkWidget *widget,
                           GdkEventExpose *event)
{
    EggHistogramViewPrivate *priv;
    GtkAllocation allocation;
    GtkStyle *style;
    cairo_t *cr;
    gint width, height;
    gint max_value = 0;

    priv = EGG_HISTOGRAM_VIEW_GET_PRIVATE (widget);
    cr = gdk_cairo_create (gtk_widget_get_window (widget));

    gdk_cairo_region (cr, event->region);
    cairo_clip (cr);

    style = gtk_widget_get_style (widget);
    gdk_cairo_set_source_color (cr, &style->base[GTK_STATE_NORMAL]);
    cairo_paint (cr);

    /* Draw the background */
    gdk_cairo_set_source_color (cr, &style->base[GTK_STATE_NORMAL]);
    cairo_paint (cr);

    gtk_widget_get_allocation (widget, &allocation);
    width  = allocation.width - 2 * BORDER;
    height = allocation.height - 2 * BORDER;

    gdk_cairo_set_source_color (cr, &style->dark[GTK_STATE_NORMAL]);
    cairo_set_line_width (cr, 1.0);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
    cairo_translate (cr, 0.5, 0.5);
    cairo_rectangle (cr, BORDER, BORDER, width - 1, height - 1);
    cairo_stroke (cr);

    if (priv->bins == NULL)
        goto cleanup;

    compute_histogram (priv);

    /* Draw border areas */
    gdk_cairo_set_source_color (cr, &style->dark[GTK_STATE_NORMAL]);

    if (priv->min_border > 0) {
        cairo_rectangle (cr, BORDER, BORDER, priv->min_border + 0.5, height - 1);
        cairo_fill (cr);
    }

    /* Draw spikes */
    for (guint i = 0; i < priv->n_bins; i++) {
        if (priv->bins[i] > max_value)
            max_value = priv->bins[i];
    }

    if (max_value == 0)
        goto cleanup;

    gdk_cairo_set_source_color (cr, &style->black);

    if (width > priv->n_bins) {
        gdouble skip = ((gdouble) width) / priv->n_bins;
        gdouble x = 1;

        for (guint i = 0; i < priv->n_bins; i++, x += skip) {
            if (priv->bins[i] == 0)
                continue;

            gint y = (gint) ((height - 2) * priv->bins[i]) / max_value;
            cairo_move_to (cr, round (x + BORDER), height + BORDER - 1);
            cairo_line_to (cr, round (x + BORDER), height + BORDER - 1 - y);
            cairo_stroke (cr);
        }
    }

cleanup:
    cairo_destroy (cr);
    return FALSE;
}

static void
egg_histogram_view_finalize (GObject *object)
{
    EggHistogramViewPrivate *priv;

    priv = EGG_HISTOGRAM_VIEW_GET_PRIVATE (object);

    if (priv->bins)
        g_free (priv->bins);

    G_OBJECT_CLASS (egg_histogram_view_parent_class)->finalize (object);
}

static void
egg_histogram_view_dispose (GObject *object)
{
    G_OBJECT_CLASS (egg_histogram_view_parent_class)->dispose (object);
}

static void
egg_histogram_view_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    g_return_if_fail (EGG_IS_HISTOGRAM_VIEW (object));

    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            return;
    }
}

static void
egg_histogram_view_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    g_return_if_fail (EGG_IS_HISTOGRAM_VIEW (object));

    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            return;
    }
}

static gint
get_mouse_distance (EggHistogramViewPrivate *priv,
                    gint x)
{
    return (priv->min_border + BORDER) - x;
}

static gboolean
egg_histogram_view_motion_notify (GtkWidget *widget,
                                  GdkEventMotion *event)
{
    EggHistogramView *view;
    EggHistogramViewPrivate *priv;

    view = EGG_HISTOGRAM_VIEW (widget);
    priv = view->priv;

    if (priv->grabbing) {
        priv->min_border = event->x + BORDER;
        gtk_widget_queue_draw (widget);
    }
    else {
        gint distance = get_mouse_distance (priv, event->x);

        if (ABS(distance) < 6)
            set_cursor_type (view, GDK_FLEUR);
        else
            set_cursor_type (view, GDK_ARROW);
    }

    return TRUE;
}

static gboolean
egg_histogram_view_button_release (GtkWidget *widget,
                                   GdkEventButton *event)
{
    EggHistogramView *view;
    EggHistogramViewPrivate *priv;
    GtkAllocation allocation;
    gint width;

    gtk_widget_get_allocation (widget, &allocation);
    width  = allocation.width - 2 * BORDER;

    view = EGG_HISTOGRAM_VIEW (widget);
    priv = view->priv;

    set_cursor_type (view, GDK_ARROW);
    priv->grabbing = FALSE;

    return TRUE;
}

static gboolean
egg_histogram_view_button_press (GtkWidget *widget,
                                 GdkEventButton *event)
{
    EggHistogramView *view;
    EggHistogramViewPrivate *priv;
    gint distance;

    view = EGG_HISTOGRAM_VIEW (widget);
    priv = view->priv;
    distance = get_mouse_distance (priv, event->x);

    if (ABS (distance) < 6) {
        priv->grabbing = TRUE;
        set_cursor_type (view, GDK_FLEUR);
    }

    return TRUE;
}

static void
egg_histogram_view_class_init (EggHistogramViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->set_property = egg_histogram_view_set_property;
    object_class->get_property = egg_histogram_view_get_property;
    object_class->dispose      = egg_histogram_view_dispose;
    object_class->finalize     = egg_histogram_view_finalize;

    widget_class->size_request = egg_histogram_view_size_request;
    widget_class->expose_event = egg_histogram_view_expose;
    widget_class->button_press_event   = egg_histogram_view_button_press;
    widget_class->button_release_event = egg_histogram_view_button_release;
    widget_class->motion_notify_event  = egg_histogram_view_motion_notify;

    g_type_class_add_private (klass, sizeof (EggHistogramViewPrivate));
}

static void
egg_histogram_view_init (EggHistogramView *view)
{
    EggHistogramViewPrivate *priv;

    view->priv = priv = EGG_HISTOGRAM_VIEW_GET_PRIVATE (view);

    priv->bins = NULL;
    priv->data = NULL;
    priv->n_bins = 0;
    priv->n_elements = 0;

    priv->cursor_type = GDK_ARROW;
    priv->grabbing = FALSE;

    gtk_widget_add_events (GTK_WIDGET (view),
                           GDK_BUTTON_PRESS_MASK   |
                           GDK_BUTTON_RELEASE_MASK |
                           GDK_BUTTON1_MOTION_MASK |
                           GDK_POINTER_MOTION_MASK);
}
