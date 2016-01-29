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

#ifndef EGG_HISTOGRAM_VIEW_H
#define EGG_HISTOGRAM_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_HISTOGRAM_VIEW             (egg_histogram_view_get_type())
#define EGG_HISTOGRAM_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), EGG_TYPE_HISTOGRAM_VIEW, EggHistogramView))
#define EGG_IS_HISTOGRAM_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), EGG_TYPE_HISTOGRAM_VIEW))
#define EGG_HISTOGRAM_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), EGG_TYPE_HISTOGRAM_VIEW, EggHistogramViewClass))
#define EGG_IS_HISTOGRAM_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), EGG_TYPE_HISTOGRAM_VIEW))
#define EGG_HISTOGRAM_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), EGG_TYPE_HISTOGRAM_VIEW, EggHistogramViewClass))

typedef struct _EggHistogramView           EggHistogramView;
typedef struct _EggHistogramViewClass      EggHistogramViewClass;
typedef struct _EggHistogramViewPrivate    EggHistogramViewPrivate;

struct _EggHistogramView
{
    GtkDrawingArea           parent_instance;

    /*< private >*/
    EggHistogramViewPrivate *priv;
};

struct _EggHistogramViewClass
{
    GtkDrawingAreaClass      parent_class;

    /* signals */
    void (* changed) (EggHistogramView *view);
};

GType         egg_histogram_view_get_type (void);
GtkWidget   * egg_histogram_view_new      (guint             n_elements,
                                           guint             n_bits,
                                           guint             n_bins);
void          egg_histogram_view_update   (EggHistogramView *view,
                                           gpointer          data);
void          egg_histogram_get_range     (EggHistogramView *view,
                                           gdouble          *min,
                                           gdouble          *max);
void          egg_histogram_view_set_max  (EggHistogramView *view,
                                           guint             max);

G_END_DECLS

#endif
