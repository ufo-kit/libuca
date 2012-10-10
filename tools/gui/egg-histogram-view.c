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

#include <stdlib.h>
#include "egg-histogram-view.h"

G_DEFINE_TYPE (EggHistogramView, egg_histogram_view, GTK_TYPE_CELL_RENDERER)

#define EGG_HISTOGRAM_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EGG_TYPE_HISTOGRAM_VIEW, EggHistogramViewPrivate))


struct _EggHistogramViewPrivate
{
    guint foo;
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

static void
egg_histogram_view_dispose (GObject *object)
{
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

static void
egg_histogram_view_class_init (EggHistogramViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = egg_histogram_view_set_property;
    gobject_class->get_property = egg_histogram_view_get_property;
    gobject_class->dispose = egg_histogram_view_dispose;

    g_type_class_add_private (klass, sizeof (EggHistogramViewPrivate));
}

static void
egg_histogram_view_init (EggHistogramView *renderer)
{
    renderer->priv = EGG_HISTOGRAM_VIEW_GET_PRIVATE (renderer);
}
