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
#include "egg-property-cell-renderer.h"

G_DEFINE_TYPE (EggPropertyCellRenderer, egg_property_cell_renderer, GTK_TYPE_CELL_RENDERER)

#define EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EGG_TYPE_PROPERTY_CELL_RENDERER, EggPropertyCellRendererPrivate))


struct _EggPropertyCellRendererPrivate
{
    GObject         *object;
    GtkListStore    *list_store;
    GtkCellRenderer *renderer;
    GtkCellRenderer *text_renderer;
    GtkCellRenderer *spin_renderer;
    GtkCellRenderer *toggle_renderer;
};

enum
{
    PROP_0,
    PROP_PROP_NAME,
    N_PROPERTIES
};

static GParamSpec *egg_property_cell_renderer_properties[N_PROPERTIES] = { NULL, };

GtkCellRenderer *
egg_property_cell_renderer_new (GObject         *object,
                                GtkListStore    *list_store)
{
    EggPropertyCellRenderer *renderer;

    renderer = EGG_PROPERTY_CELL_RENDERER (g_object_new (EGG_TYPE_PROPERTY_CELL_RENDERER, NULL));
    renderer->priv->object = object;
    renderer->priv->list_store = list_store;
    return GTK_CELL_RENDERER (renderer);
}

static GParamSpec *
get_pspec_from_object (GObject *object, const gchar *prop_name)
{
    GObjectClass *oclass = G_OBJECT_GET_CLASS (object);
    return g_object_class_find_property (oclass, prop_name);
}

static void
get_string_double_repr (GObject *object, const gchar *prop_name, gchar **text, gdouble *number)
{
    GParamSpec *pspec;
    GValue from = { 0 };
    GValue to_string = { 0 };
    GValue to_double = { 0 };

    pspec = get_pspec_from_object (object, prop_name);
    g_value_init (&from, pspec->value_type);
    g_value_init (&to_string, G_TYPE_STRING);
    g_value_init (&to_double, G_TYPE_DOUBLE);
    g_object_get_property (object, prop_name, &from);

    if (g_value_transform (&from, &to_string))
        *text = g_strdup (g_value_get_string (&to_string));
    else
        g_warning ("Could not convert from %s gchar*\n", g_type_name (pspec->value_type));

    if (g_value_transform (&from, &to_double))
        *number = g_value_get_double (&to_double);
    else
        g_warning ("Could not convert from %s to gdouble\n", g_type_name (pspec->value_type));
}

static void
clear_adjustment (GObject *object)
{
    GtkAdjustment *adjustment;

    g_object_get (object,
            "adjustment", &adjustment,
            NULL);

    if (adjustment)
        g_object_unref (adjustment);

    g_object_set (object,
            "adjustment", NULL,
            NULL);
}

static void
egg_property_cell_renderer_set_renderer (EggPropertyCellRenderer    *renderer,
                                         const gchar                *prop_name)
{
    EggPropertyCellRendererPrivate *priv;
    GParamSpec *pspec;
    gchar *text = NULL;
    gdouble number;

    priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (renderer);
    pspec = get_pspec_from_object (priv->object, prop_name);

    /*
     * Set this renderers mode, so that any actions can be forwarded to our
     * child renderers.
     */
    switch (pspec->value_type) {
        /* toggle renderers */
        case G_TYPE_BOOLEAN:
            priv->renderer = priv->toggle_renderer;
            g_object_set (renderer, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
            break;

        /* spin renderers */
        case G_TYPE_FLOAT:
        case G_TYPE_DOUBLE:
            priv->renderer = priv->spin_renderer;
            g_object_set (renderer, "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
            g_object_set (priv->renderer, "digits", 5, NULL);
            break;

        case G_TYPE_INT:
        case G_TYPE_UINT:
        case G_TYPE_LONG:
        case G_TYPE_ULONG:
        case G_TYPE_INT64:
        case G_TYPE_UINT64:
            priv->renderer = priv->spin_renderer;
            g_object_set (renderer, "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
            g_object_set (priv->renderer, "digits", 0, NULL);
            break;

        /* text renderers */
        case G_TYPE_POINTER:
        case G_TYPE_STRING:
            priv->renderer = priv->text_renderer;
            g_object_set (renderer, "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
            break;

        /* combo renderers */
        default:
            /* g_print ("no idea how to handle %s -> %s\n", prop_name, g_type_name (pspec->value_type)); */
            break;
    }

    /*
     * Set the content from the objects property.
     */
    switch (pspec->value_type) {
        case G_TYPE_BOOLEAN:
            {
                gboolean val;

                g_object_get (priv->object, prop_name, &val, NULL);
                g_object_set (priv->renderer,
                        "active", val,
                        "activatable", pspec->flags & G_PARAM_WRITABLE ? TRUE : FALSE,
                        NULL);
                break;
            }

        case G_TYPE_INT:
        case G_TYPE_UINT:
        case G_TYPE_LONG:
        case G_TYPE_ULONG:
        case G_TYPE_INT64:
        case G_TYPE_UINT64:
        case G_TYPE_FLOAT:
        case G_TYPE_DOUBLE:
            get_string_double_repr (priv->object, prop_name, &text, &number);
            break;

        case G_TYPE_STRING:
            g_object_get (priv->object, prop_name, &text, NULL);
            break;

        case G_TYPE_POINTER:
            {
                gpointer val;

                g_object_get (priv->object, prop_name, &val, NULL);
                text = g_strdup_printf ("0x%x", GPOINTER_TO_INT (val));
            }
            break;

        default:
            break;
    }

    if (pspec->flags & G_PARAM_WRITABLE) {
        if (GTK_IS_CELL_RENDERER_TOGGLE (priv->renderer))
            g_object_set (priv->renderer, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
        else
            g_object_set (priv->renderer, "foreground", "#000000", NULL);

        if (GTK_IS_CELL_RENDERER_TEXT (priv->renderer)) {
            g_object_set (priv->renderer,
                    "editable", TRUE,
                    "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
                    NULL);
        }

        if (GTK_IS_CELL_RENDERER_SPIN (priv->renderer)) {
            GtkObject *adjustment = NULL;

#define gtk_typed_adjustment_new(type, pspec, val, step_inc, page_inc) \
    gtk_adjustment_new (val, ((type *) pspec)->minimum, ((type *) pspec)->maximum, step_inc, page_inc, 0)

            switch (pspec->value_type) {
                case G_TYPE_INT:
                    adjustment = gtk_typed_adjustment_new (GParamSpecInt, pspec, number, 1, 10);
                    break;
                case G_TYPE_UINT:
                    adjustment = gtk_typed_adjustment_new (GParamSpecUInt, pspec, number, 1, 10);
                    break;
                case G_TYPE_LONG:
                    adjustment = gtk_typed_adjustment_new (GParamSpecLong, pspec, number, 1, 10);
                    break;
                case G_TYPE_ULONG:
                    adjustment = gtk_typed_adjustment_new (GParamSpecULong, pspec, number, 1, 10);
                    break;
                case G_TYPE_INT64:
                    adjustment = gtk_typed_adjustment_new (GParamSpecInt64, pspec, number, 1, 10);
                    break;
                case G_TYPE_UINT64:
                    adjustment = gtk_typed_adjustment_new (GParamSpecUInt64, pspec, number, 1, 10);
                    break;
                case G_TYPE_FLOAT:
                    adjustment = gtk_typed_adjustment_new (GParamSpecFloat, pspec, number, 0.05, 10);
                    break;
                case G_TYPE_DOUBLE:
                    adjustment = gtk_typed_adjustment_new (GParamSpecDouble, pspec, number, 0.05, 10);
                    break;
            }

            clear_adjustment (G_OBJECT (priv->renderer));
            g_object_set (priv->renderer, "adjustment", adjustment, NULL);
        }
    }
    else {
        g_object_set (priv->renderer, "mode", GTK_CELL_RENDERER_MODE_INERT, NULL);

        if (!GTK_IS_CELL_RENDERER_TOGGLE (priv->renderer))
            g_object_set (priv->renderer, "foreground", "#aaaaaa", NULL);
    }

    if (text != NULL) {
        g_object_set (priv->renderer, "text", text, NULL);
        g_free (text);
    }
}

static gchar *
get_prop_name_from_tree_model (GtkTreeModel *model, const gchar *path)
{
    GtkTreeIter iter;
    gchar *prop_name = NULL;

    /* TODO: don't assume column 0 to contain the prop name */
    if (gtk_tree_model_get_iter_from_string (model, &iter, path))
        gtk_tree_model_get (model, &iter, 0, &prop_name, -1);

    return prop_name;
}

static void
egg_property_cell_renderer_toggle_cb (GtkCellRendererToggle *renderer,
                                      gchar                 *path,
                                      gpointer               user_data)
{
    EggPropertyCellRendererPrivate *priv;
    gchar *prop_name;

    priv = (EggPropertyCellRendererPrivate *) user_data;
    prop_name = get_prop_name_from_tree_model (GTK_TREE_MODEL (priv->list_store), path);

    if (prop_name != NULL) {
        gboolean activated;

        g_object_get (priv->object, prop_name, &activated, NULL);
        g_object_set (priv->object, prop_name, !activated, NULL);
        g_free (prop_name);
    }
}

static void
egg_property_cell_renderer_text_edited_cb (GtkCellRendererText  *renderer,
                                           gchar                *path,
                                           gchar                *new_text,
                                           gpointer              user_data)
{
    EggPropertyCellRendererPrivate *priv;
    gchar *prop_name;

    priv = (EggPropertyCellRendererPrivate *) user_data;
    prop_name = get_prop_name_from_tree_model (GTK_TREE_MODEL (priv->list_store), path);

    if (prop_name != NULL) {
        g_object_set (priv->object, prop_name, new_text, NULL);
        g_free (prop_name);
    }
}

static void
egg_property_cell_renderer_spin_edited_cb (GtkCellRendererText  *renderer,
                                           gchar                *path,
                                           gchar                *new_text,
                                           gpointer              user_data)
{
    EggPropertyCellRendererPrivate *priv;
    gchar *prop_name;

    priv = (EggPropertyCellRendererPrivate *) user_data;
    prop_name = get_prop_name_from_tree_model (GTK_TREE_MODEL (priv->list_store), path);

    if (prop_name != NULL) {
        GParamSpec *pspec;
        GValue from = { 0 };
        GValue to = { 0 };

        pspec = get_pspec_from_object (priv->object, prop_name);

        g_value_init (&from, G_TYPE_DOUBLE);
        g_value_init (&to, pspec->value_type);
        g_value_set_double (&from, strtod (new_text, NULL));

        if (g_value_transform (&from, &to))
            g_object_set_property (priv->object, prop_name, &to);
        else
            g_warning ("Could not transform %s to %s\n",
                    g_value_get_string (&from), g_type_name (pspec->value_type));

        g_free (prop_name);
    }
}

static void
egg_property_cell_renderer_get_size (GtkCellRenderer    *cell,
                                     GtkWidget          *widget,
                                     GdkRectangle       *cell_area,
                                     gint               *x_offset,
                                     gint               *y_offset,
                                     gint               *width,
                                     gint               *height)
{

    EggPropertyCellRendererPrivate *priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (cell);

    gtk_cell_renderer_get_size (priv->renderer, widget, cell_area, x_offset, y_offset, width, height);
}

static void
egg_property_cell_renderer_render (GtkCellRenderer      *cell,
                                   GdkDrawable          *window,
                                   GtkWidget            *widget,
                                   GdkRectangle         *background_area,
                                   GdkRectangle         *cell_area,
                                   GdkRectangle         *expose_area,
                                   GtkCellRendererState  flags)
{
    EggPropertyCellRendererPrivate *priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (cell);
    gtk_cell_renderer_render (priv->renderer, window, widget, background_area, cell_area, expose_area, flags);
}

static gboolean
egg_property_cell_renderer_activate (GtkCellRenderer        *cell,
                                     GdkEvent               *event,
                                     GtkWidget              *widget,
                                     const gchar            *path,
                                     GdkRectangle           *background_area,
                                     GdkRectangle           *cell_area,
                                     GtkCellRendererState    flags)
{
    EggPropertyCellRendererPrivate *priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (cell);
    return gtk_cell_renderer_activate (priv->renderer, event, widget, path, background_area, cell_area, flags);
}

static GtkCellEditable *
egg_property_cell_renderer_start_editing (GtkCellRenderer        *cell,
                                          GdkEvent               *event,
                                          GtkWidget              *widget,
                                          const gchar            *path,
                                          GdkRectangle           *background_area,
                                          GdkRectangle           *cell_area,
                                          GtkCellRendererState    flags)
{
    EggPropertyCellRendererPrivate *priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (cell);
    return gtk_cell_renderer_start_editing (priv->renderer, event, widget, path, background_area, cell_area, flags);
}

static void
egg_property_cell_renderer_set_property (GObject        *object,
                                         guint           property_id,
                                         const GValue   *value,
                                         GParamSpec     *pspec)
{
    g_return_if_fail (EGG_IS_PROPERTY_CELL_RENDERER (object));
    EggPropertyCellRenderer *renderer = EGG_PROPERTY_CELL_RENDERER (object);

    switch (property_id) {
        case PROP_PROP_NAME:
            egg_property_cell_renderer_set_renderer (renderer, g_value_get_string (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            return;
    }
}

static void
egg_property_cell_renderer_get_property (GObject    *object,
                                         guint       property_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
    g_return_if_fail (EGG_IS_PROPERTY_CELL_RENDERER (object));

    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            return;
    }
}

static void
egg_property_cell_renderer_class_init (EggPropertyCellRendererClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkCellRendererClass *cellrenderer_class = GTK_CELL_RENDERER_CLASS (klass);

    gobject_class->set_property = egg_property_cell_renderer_set_property;
    gobject_class->get_property = egg_property_cell_renderer_get_property;

    cellrenderer_class->render = egg_property_cell_renderer_render;
    cellrenderer_class->get_size = egg_property_cell_renderer_get_size;
    cellrenderer_class->activate = egg_property_cell_renderer_activate;
    cellrenderer_class->start_editing = egg_property_cell_renderer_start_editing;

    egg_property_cell_renderer_properties[PROP_PROP_NAME] =
            g_param_spec_string("prop-name",
                                "Property name", "Property name", "",
                                G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_PROP_NAME, egg_property_cell_renderer_properties[PROP_PROP_NAME]);

    g_type_class_add_private (klass, sizeof (EggPropertyCellRendererPrivate));
}

static void
egg_property_cell_renderer_init (EggPropertyCellRenderer *renderer)
{
    EggPropertyCellRendererPrivate *priv;

    renderer->priv = priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (renderer);

    priv->text_renderer = gtk_cell_renderer_text_new ();
    priv->spin_renderer = gtk_cell_renderer_spin_new ();
    priv->toggle_renderer = gtk_cell_renderer_toggle_new ();
    priv->renderer = priv->text_renderer;

    g_object_set (priv->text_renderer,
            "editable", TRUE,
            NULL);

    g_object_set (priv->spin_renderer,
            "editable", TRUE,
            NULL);

    g_object_set (priv->toggle_renderer,
            "xalign", 0.0f,
            "activatable", TRUE,
            "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
            NULL);

    g_signal_connect (priv->spin_renderer, "edited",
            G_CALLBACK (egg_property_cell_renderer_spin_edited_cb), priv);

    g_signal_connect (priv->text_renderer, "edited",
            G_CALLBACK (egg_property_cell_renderer_text_edited_cb), NULL);

    g_signal_connect (priv->toggle_renderer, "toggled",
            G_CALLBACK (egg_property_cell_renderer_toggle_cb), priv);
}
