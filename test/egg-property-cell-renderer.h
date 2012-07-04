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

#ifndef EGG_PROPERTY_CELL_RENDERER_H
#define EGG_PROPERTY_CELL_RENDERER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_PROPERTY_CELL_RENDERER             (egg_property_cell_renderer_get_type())
#define EGG_PROPERTY_CELL_RENDERER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), EGG_TYPE_PROPERTY_CELL_RENDERER, EggPropertyCellRenderer))
#define EGG_IS_PROPERTY_CELL_RENDERER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), EGG_TYPE_PROPERTY_CELL_RENDERER))
#define EGG_PROPERTY_CELL_RENDERER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), EGG_TYPE_PROPERTY_CELL_RENDERER, EggPropertyCellRendererClass))
#define EGG_IS_PROPERTY_CELL_RENDERER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), EGG_TYPE_PROPERTY_CELL_RENDERER))
#define EGG_PROPERTY_CELL_RENDERER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), EGG_TYPE_PROPERTY_CELL_RENDERER, EggPropertyCellRendererClass))

typedef struct _EggPropertyCellRenderer           EggPropertyCellRenderer;
typedef struct _EggPropertyCellRendererClass      EggPropertyCellRendererClass;
typedef struct _EggPropertyCellRendererPrivate    EggPropertyCellRendererPrivate;

struct _EggPropertyCellRenderer
{
    GtkCellRenderer parent_instance;

    /*< private >*/
    EggPropertyCellRendererPrivate *priv;
};

struct _EggPropertyCellRendererClass
{
    GtkCellRendererClass parent_class;
};

GType               egg_property_cell_renderer_get_type (void);
GtkCellRenderer*    egg_property_cell_renderer_new      (GObject         *object,
                                                         GtkListStore    *list_store);

G_END_DECLS

#endif
