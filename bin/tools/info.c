/* Copyright (C) 2012 Matthias Vogelgesang <matthias.vogelgesang@kit.edu>
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

#include <glib-object.h>
#include <string.h>
#include "uca-plugin-manager.h"
#include "uca-camera.h"


static void
print_usage (void)
{
    GList *types;
    UcaPluginManager *manager;

    manager = uca_plugin_manager_new ();
    g_print ("Usage: uca-info [ ");
    types = uca_plugin_manager_get_available_cameras (manager);

    if (types == NULL) {
        g_print ("] -- no camera plugin found\n");
        return;
    }

    for (GList *it = g_list_first (types); it != NULL; it = g_list_next (it)) {
        gchar *name = (gchar *) it->data;
        if (g_list_next (it) == NULL)
            g_print ("%s ]\n", name);
        else
            g_print ("%s, ", name);
    }
}

static const gchar *
get_flags_description (GParamSpec *pspec)
{
    static const gchar *descriptions[] = { "", "RO", "WO", "RW", "" };

    if (pspec->flags >= 3)
        return descriptions[3];

    return descriptions[pspec->flags];
}

static void
print_properties (UcaCamera *camera)
{
    GObjectClass *oclass;
    GParamSpec **pspecs;
    GHashTable *map;
    GList *names;
    gchar *fmt_string;
    guint n_props;
    guint max_length = 0;

    oclass = G_OBJECT_GET_CLASS (camera);
    pspecs = g_object_class_list_properties (oclass, &n_props);

    for (guint i = 0; i < n_props; i++)
        max_length = MAX (max_length, strlen (g_param_spec_get_name (pspecs[i])));

    map = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

    for (guint i = 0; i < n_props; i++) {
        GParamSpec *pspec;
        GValue value= { 0, { { 0 } } };
        const gchar *name;

        pspec = pspecs[i];
        name = g_param_spec_get_name (pspec);
        g_value_init (&value, pspec->value_type);

        g_object_get_property (G_OBJECT (camera), name, &value);
        g_hash_table_insert (map, (gpointer) name, g_strdup_value_contents (&value));
        g_value_unset (&value);
    }

    fmt_string = g_strdup_printf (" %%s | %%-%us | %%s\n", max_length);
    names = g_list_sort (g_hash_table_get_keys (map), (GCompareFunc) g_strcmp0);

    for (GList *it = g_list_first (names); it != NULL; it = g_list_next (it)) {
        GParamSpec *pspec;
        const gchar *name;
        gchar* value;

        name = (const gchar *) it->data;
        pspec = g_object_class_find_property (oclass, name);
        value = g_hash_table_lookup (map, name);
        g_print (fmt_string, get_flags_description (pspec), name, value);
    }

    g_list_free (names);
    g_hash_table_destroy (map);
    g_free (fmt_string);
    g_free (pspecs);
}

int main(int argc, char *argv[])
{
    UcaPluginManager *manager;
    UcaCamera *camera;
    gchar *name;
    GError *error = NULL;

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init();
#endif
    manager = uca_plugin_manager_new ();

    if (argc < 2) {
        print_usage();
        return 0;
    }
    else {
        name = argv[1];
        camera = uca_plugin_manager_get_camera (manager, name, &error, NULL);
    }

    if (camera == NULL) {
        g_printerr ("Error during initialization: %s\n", error->message);
        print_usage();
        return 1;
    }

    print_properties (camera);

    g_object_unref (camera);
    g_object_unref (manager);
}
