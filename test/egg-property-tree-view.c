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


#include "egg-property-tree-view.h"
#include "egg-property-cell-renderer.h"

G_DEFINE_TYPE (EggPropertyTreeView, egg_property_tree_view, GTK_TYPE_TREE_VIEW)

#define EGG_PROPERTY_TREE_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EGG_TYPE_PROPERTY_TREE_VIEW, EggPropertyTreeViewPrivate))

struct _EggPropertyTreeViewPrivate
{
    GtkListStore *list_store;
};

enum
{
    COLUMN_PROP_NAME,
    COLUMN_PROP_ROW,
    COLUMN_PROP_ADJUSTMENT,
    N_COLUMNS
};

static void
egg_property_tree_view_populate_model_with_properties (GtkListStore *model, GObject *object)
{
    GParamSpec **pspecs;
    GObjectClass *oclass;
    guint n_properties;
    GtkTreeIter iter;

    oclass = G_OBJECT_GET_CLASS (object);
    pspecs = g_object_class_list_properties (oclass, &n_properties);

    for (guint i = 0; i < n_properties; i++) {
        if (pspecs[i]->flags & G_PARAM_READABLE) {
            GtkObject *adjustment;

            adjustment = gtk_adjustment_new (5, 0, 1000, 1, 10, 0);

            gtk_list_store_append (model, &iter);
            gtk_list_store_set (model, &iter,
                    COLUMN_PROP_NAME, pspecs[i]->name,
                    COLUMN_PROP_ROW, FALSE,
                    COLUMN_PROP_ADJUSTMENT, adjustment,
                    -1);
        }
    }

    g_free (pspecs);
}

GtkWidget *
egg_property_tree_view_new (GObject *object)
{
    EggPropertyTreeView *property_tree_view;
    GtkTreeView *tree_view;
    GtkTreeViewColumn *prop_column, *value_column;
    GtkCellRenderer *prop_renderer, *value_renderer;
    GtkListStore *list_store;

    property_tree_view = EGG_PROPERTY_TREE_VIEW (g_object_new (EGG_TYPE_PROPERTY_TREE_VIEW, NULL));
    list_store = property_tree_view->priv->list_store;
    tree_view = GTK_TREE_VIEW (property_tree_view);

    egg_property_tree_view_populate_model_with_properties (list_store, object);
    gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));

    prop_renderer = gtk_cell_renderer_text_new ();
    prop_column = gtk_tree_view_column_new_with_attributes ("Property", prop_renderer,
            "text", COLUMN_PROP_NAME,
            NULL);

    value_renderer = egg_property_cell_renderer_new (object, list_store);
    value_column = gtk_tree_view_column_new_with_attributes ("Value", value_renderer,
            "prop-name", COLUMN_PROP_NAME,
            NULL);

    gtk_tree_view_append_column (tree_view, prop_column);
    gtk_tree_view_append_column (tree_view, value_column);

    return GTK_WIDGET (tree_view);
}

static void
egg_property_tree_view_class_init (EggPropertyTreeViewClass *klass)
{
    g_type_class_add_private (klass, sizeof (EggPropertyTreeViewPrivate));
}

static void
egg_property_tree_view_init (EggPropertyTreeView *tree_view)
{
    EggPropertyTreeViewPrivate *priv = EGG_PROPERTY_TREE_VIEW_GET_PRIVATE (tree_view);

    tree_view->priv = priv = EGG_PROPERTY_TREE_VIEW_GET_PRIVATE (tree_view);
    priv->list_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_BOOLEAN, GTK_TYPE_ADJUSTMENT);
}

