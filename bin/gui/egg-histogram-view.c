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
    /* gdouble *grabbed; */
    enum {
        GRAB_MIN,
        GRAB_MAX
    } grabbed;

    gdouble max;
    gdouble min_value;    /* lowest value of the first bin */
    gdouble max_value;    /* highest value of the last bin */

    gint     n_elements;
    gint     n_bits;
};

enum
{
    PROP_0,
    PROP_MINIMUM_BIN_VALUE,
    PROP_MAXIMUM_BIN_VALUE,
    N_PROPERTIES
};

enum
{
    CHANGED,
    LAST_SIGNAL
};

static GParamSpec *egg_histogram_view_properties[N_PROPERTIES] = { NULL, };

static guint egg_histogram_view_signals[LAST_SIGNAL] = { 0 };


GtkWidget *
egg_histogram_view_new (guint n_elements,
                        guint n_bits,
                        guint n_bins)
{
    EggHistogramView *view;
    EggHistogramViewPrivate *priv;

    view = EGG_HISTOGRAM_VIEW (g_object_new (EGG_TYPE_HISTOGRAM_VIEW, NULL));
    priv = view->priv;

    priv->bins = g_malloc0 (n_bins * sizeof (guint));
    priv->n_bins = n_bins;
    priv->n_bits = n_bits;
    priv->n_elements = n_elements;

    priv->min_value = 0.0;
    priv->max_value = priv->max = (gint) pow (2, n_bits) - 1;

    return GTK_WIDGET (view);
}

void
egg_histogram_view_update (EggHistogramView *view,
                           gpointer buffer)
{
    EggHistogramViewPrivate *priv;
    guint n_bins;

    g_return_if_fail (EGG_IS_HISTOGRAM_VIEW (view));
    priv = view->priv;
    n_bins = priv->n_bins - 1;

    for (guint i = 0; i < priv->n_bins; i++)
        priv->bins[i] = 0;

    if (priv->n_bits == 8) {
        guint8 *data = (guint8 *) buffer;

        for (guint i = 0; i < priv->n_elements; i++) {
            guint8 v = data[i];

            guint index = (guint) round (((gdouble) v) / priv->max * n_bins);
            priv->bins[index]++;
        }
    }
    else {
        guint16 *data = (guint16 *) buffer;

        for (guint i = 0; i < priv->n_elements; i++) {
            guint16 v = data[i];

            guint index = (guint) floor (((gdouble ) v) / priv->max * n_bins);
            priv->bins[index]++;
        }
    }
}

void
egg_histogram_get_range (EggHistogramView *view,
                         gdouble *min,
                         gdouble *max)
{
    EggHistogramViewPrivate *priv;
    GtkAllocation allocation;
    gdouble width;

    g_return_if_fail (EGG_IS_HISTOGRAM_VIEW (view));

    gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);
    width = (gdouble) allocation.width - 2 * BORDER;
    priv = view->priv;

    *min = priv->min_value;
    *max = priv->max_value;
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

static void
draw_bins (EggHistogramViewPrivate *priv,
           cairo_t *cr,
           gint width,
           gint height)
{
    gdouble skip = ((gdouble) width) / priv->n_bins;
    gdouble x = BORDER;
    gdouble ys = height + BORDER - 1;
    gint max_value = 0;

    for (guint i = 0; i < priv->n_bins; i++) {
        if (priv->bins[i] > max_value)
            max_value = priv->bins[i];
    }

    if (max_value == 0)
        return;

    for (guint i = 0; i < priv->n_bins && x < (width - BORDER); i++, x += skip) {
        if (priv->bins[i] == 0)
            continue;

        gint y = (gint) ((height - 2) * priv->bins[i]) / max_value;
        cairo_move_to (cr, round (x), ys);
        cairo_line_to (cr, round (x), ys - y);
        cairo_stroke (cr);
    }
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
    gdouble left, right;

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

    /* Draw border areas */
    gdk_cairo_set_source_color (cr, &style->dark[GTK_STATE_NORMAL]);

    left = ((gint) (priv->min_value / priv->max * width)) + 0.5;
    cairo_rectangle (cr, BORDER, BORDER, left, height - 1);
    cairo_fill (cr);

    right = ((gint) (priv->max_value / priv->max * width)) + 0.5;

    cairo_rectangle (cr, right, BORDER, width - right, height - 1);
    cairo_fill (cr);

    /* Draw spikes */
    gdk_cairo_set_source_color (cr, &style->black);
    draw_bins (priv, cr, width, height);

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
    EggHistogramViewPrivate *priv;

    g_return_if_fail (EGG_IS_HISTOGRAM_VIEW (object));
    priv = EGG_HISTOGRAM_VIEW_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MINIMUM_BIN_VALUE:
            {
                gdouble v = g_value_get_double (value);

                if (v > priv->max_value)
                    g_warning ("Minimum value `%f' larger than maximum value `%f'",
                               v, priv->max_value);
                else {
                    priv->min_value = v;
                    gtk_widget_queue_draw (GTK_WIDGET (object));
                }
            }
            break;

        case PROP_MAXIMUM_BIN_VALUE:
            {
                gdouble v = g_value_get_double (value);

                if (v < priv->min_value)
                    g_warning ("Maximum value `%f' larger than minimum value `%f'",
                               v, priv->min_value);
                else {
                    priv->max_value = v;
                    gtk_widget_queue_draw (GTK_WIDGET (object));
                }
            }
            break;

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
    EggHistogramViewPrivate *priv;

    g_return_if_fail (EGG_IS_HISTOGRAM_VIEW (object));
    priv = EGG_HISTOGRAM_VIEW_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MINIMUM_BIN_VALUE:
            g_value_set_double (value, priv->min_value);
            break;

        case PROP_MAXIMUM_BIN_VALUE:
            g_value_set_double (value, priv->max_value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            return;
    }
}

static gdouble
screen_to_histogram_coordinate (EggHistogramViewPrivate *priv,
                                GtkAllocation *allocation,
                                gint x)
{
    return (((gdouble) x) / (allocation->width - BORDER)) * priv->max;
}

static gboolean
is_on_border (EggHistogramViewPrivate *priv,
              GtkAllocation *allocation,
              gint x)
{
    gdouble coord;

    coord = screen_to_histogram_coordinate (priv, allocation , x);
    return (ABS (coord - priv->min_value) < 6) || (ABS (coord - priv->max_value) < 6);
}

static void
set_grab_value (EggHistogramView *view,
                gdouble value)
{
    if (view->priv->grabbed == GRAB_MIN) {
        view->priv->min_value = value;
        g_object_notify_by_pspec (G_OBJECT (view),
                                  egg_histogram_view_properties[PROP_MINIMUM_BIN_VALUE]);
    }
    else {
        view->priv->max_value = value;
        g_object_notify_by_pspec (G_OBJECT (view),
                                  egg_histogram_view_properties[PROP_MAXIMUM_BIN_VALUE]);
    }
}

static gboolean
egg_histogram_view_motion_notify (GtkWidget *widget,
                                  GdkEventMotion *event)
{
    EggHistogramView *view;
    EggHistogramViewPrivate *priv;
    GtkAllocation allocation;

    view = EGG_HISTOGRAM_VIEW (widget);
    priv = view->priv;
    gtk_widget_get_allocation (widget, &allocation);

    if (priv->grabbing) {
        gdouble coord;

        coord = screen_to_histogram_coordinate (priv, &allocation, event->x);

        if (ABS (priv->max_value - priv->min_value) > 8.0)
            set_grab_value (view, coord);

        gtk_widget_queue_draw (widget);
    }
    else {
        if (is_on_border (priv, &allocation, event->x))
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

    view = EGG_HISTOGRAM_VIEW (widget);
    set_cursor_type (view, GDK_ARROW);
    view->priv->grabbing = FALSE;

    g_signal_emit (widget, egg_histogram_view_signals[CHANGED], 0);

    return TRUE;
}

static gboolean
egg_histogram_view_button_press (GtkWidget *widget,
                                 GdkEventButton *event)
{
    EggHistogramView *view;
    EggHistogramViewPrivate *priv;
    GtkAllocation allocation;

    view = EGG_HISTOGRAM_VIEW (widget);
    priv = view->priv;
    gtk_widget_get_allocation (widget, &allocation);

    if (is_on_border (priv, &allocation, event->x)) {
        gdouble coord;

        priv->grabbing = TRUE;
        coord = screen_to_histogram_coordinate (priv, &allocation, event->x);

        if (ABS (coord - priv->min_value < 6))
            priv->grabbed = GRAB_MIN;
        else if (ABS (coord - priv->max_value < 6))
            priv->grabbed = GRAB_MAX;

        set_grab_value (view, coord);
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

    egg_histogram_view_properties[PROP_MINIMUM_BIN_VALUE] =
        g_param_spec_double ("minimum-bin-value",
                             "Smallest possible bin value",
                             "Smallest possible bin value, everything below is discarded.",
                             0.0, G_MAXDOUBLE, 0.0,
                             G_PARAM_READWRITE);

    egg_histogram_view_properties[PROP_MAXIMUM_BIN_VALUE] =
        g_param_spec_double ("maximum-bin-value",
                             "Largest possible bin value",
                             "Largest possible bin value, everything above is discarded.",
                             0.0, G_MAXDOUBLE, 256.0,
                             G_PARAM_READWRITE);

    g_object_class_install_property (object_class, PROP_MINIMUM_BIN_VALUE, egg_histogram_view_properties[PROP_MINIMUM_BIN_VALUE]);
    g_object_class_install_property (object_class, PROP_MAXIMUM_BIN_VALUE, egg_histogram_view_properties[PROP_MAXIMUM_BIN_VALUE]);

    egg_histogram_view_signals[CHANGED] =
        g_signal_new ("changed",
                      G_OBJECT_CLASS_TYPE (klass),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                      G_STRUCT_OFFSET (EggHistogramViewClass, changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    g_type_class_add_private (klass, sizeof (EggHistogramViewPrivate));
}

static void
egg_histogram_view_init (EggHistogramView *view)
{
    EggHistogramViewPrivate *priv;

    view->priv = priv = EGG_HISTOGRAM_VIEW_GET_PRIVATE (view);

    priv->bins = NULL;
    priv->n_bins = 0;
    priv->n_elements = 0;
    priv->min_value = 0;
    priv->max_value = 256;

    priv->cursor_type = GDK_ARROW;
    priv->grabbing = FALSE;

    gtk_widget_add_events (GTK_WIDGET (view),
                           GDK_BUTTON_PRESS_MASK   |
                           GDK_BUTTON_RELEASE_MASK |
                           GDK_BUTTON1_MOTION_MASK |
                           GDK_POINTER_MOTION_MASK);
}
