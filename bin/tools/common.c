/* Copyright (C) 2011-2013 Matthias Vogelgesang <matthias.vogelgesang@kit.edu>
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

#include "common.h"

static gchar **uca_prop_assignment_array = NULL;

static gchar *
get_camera_list (UcaPluginManager *manager)
{
    GList *types;
    GString *str;

    manager = uca_plugin_manager_new ();
    types = uca_plugin_manager_get_available_cameras (manager);
    str = g_string_new ("[ ");

    if (types != NULL) {
        for (GList *it = g_list_first (types); it != NULL; it = g_list_next (it)) {
            gchar *name = (gchar *) it->data;

            if (g_list_next (it) == NULL)
                g_string_append_printf (str, "%s ]", name);
            else
                g_string_append_printf (str, "%s, ", name);
        }
    }
    else {
        g_string_append (str, "]");
    }

    g_list_free_full (types, g_free);
    g_object_unref (manager);
    return g_string_free (str, FALSE);
}

GOptionContext *
uca_common_context_new (UcaPluginManager *manager)
{
    GOptionContext *context;
    GOptionGroup *common;
    gchar *camera_list;

    static GOptionEntry entries[] = {
        { "prop", 'p', 0, G_OPTION_ARG_STRING_ARRAY, &uca_prop_assignment_array, "Property assignment via `name=value'", NULL },
        { NULL }
    };

    camera_list = get_camera_list (manager);
    context = g_option_context_new (camera_list);
    g_free (camera_list);

    common = g_option_group_new ("properties", "Property options", "Show help for property assignment", NULL, NULL);
    g_option_group_add_entries (common, entries);
    g_option_context_add_group (context, common);

    return context;
}

UcaCamera *
uca_common_get_camera (UcaPluginManager *manager, const gchar *name, GError **error)
{
    UcaCamera *camera;
    guint n_props;

    n_props = uca_prop_assignment_array != NULL ? g_strv_length (uca_prop_assignment_array) : 0;

    camera = uca_plugin_manager_get_camera (manager, name, error, NULL);

    if (camera != NULL) {
        if (!uca_camera_parse_arg_props (camera, uca_prop_assignment_array, n_props, error)) {
            g_object_unref (camera);
            return NULL;
        }
    }

    return camera;
}
