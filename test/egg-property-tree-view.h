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

#ifndef EGG_PROPERTY_TREE_VIEW_H
#define EGG_PROPERTY_TREE_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_PROPERTY_TREE_VIEW             (egg_property_tree_view_get_type())
#define EGG_PROPERTY_TREE_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), EGG_TYPE_PROPERTY_TREE_VIEW, EggPropertyTreeView))
#define EGG_IS_PROPERTY_TREE_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), EGG_TYPE_PROPERTY_TREE_VIEW))
#define EGG_PROPERTY_TREE_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), EGG_TYPE_PROPERTY_TREE_VIEW, EggPropertyTreeViewClass))
#define EGG_IS_PROPERTY_TREE_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), EGG_TYPE_PROPERTY_TREE_VIEW))
#define EGG_PROPERTY_TREE_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), EGG_TYPE_PROPERTY_TREE_VIEW, EggPropertyTreeViewClass))

typedef struct _EggPropertyTreeView           EggPropertyTreeView;
typedef struct _EggPropertyTreeViewClass      EggPropertyTreeViewClass;
typedef struct _EggPropertyTreeViewPrivate    EggPropertyTreeViewPrivate;

struct _EggPropertyTreeView
{
    GtkTreeView parent_instance;

    /*< private >*/
    EggPropertyTreeViewPrivate *priv;
};

struct _EggPropertyTreeViewClass
{
    GtkTreeViewClass parent_class;
};

GType           egg_property_tree_view_get_type (void)              G_GNUC_CONST;
GtkWidget*      egg_property_tree_view_new      (GObject *object);

G_END_DECLS

#endif
