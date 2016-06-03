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
    guint n_props;

    oclass = G_OBJECT_GET_CLASS (camera);
    pspecs = g_object_class_list_properties (oclass, &n_props);

    for (guint i = 0; i < n_props; i++) {
        GParamSpec *pspec;
        GValue value= { 0, { { 0 } } };
        gchar *value_string;
        const gchar *name;

        pspec = pspecs[i];
        name = g_param_spec_get_name (pspec);
        g_value_init (&value, pspec->value_type);

        g_object_get_property (G_OBJECT (camera), name, &value);
        value_string = g_strdup_value_contents (&value);

        g_print (" %s | %-26s | %s\n", get_flags_description (pspec), name, value_string);

        g_free (value_string);
        g_value_unset (&value);
    }

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
        name = g_strdup ("Basic camera");
        camera = g_object_new (UCA_TYPE_CAMERA, NULL);
    }
    else {
        name = argv[1];
        camera = uca_plugin_manager_get_camera (manager, name, &error, NULL);
    }

    if (camera == NULL) {
        g_print("Error during initialization: %s\n", error->message);
        print_usage();
        return 1;
    }

    print_properties (camera);

    g_object_unref (camera);
    g_object_unref (manager);
}
