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
    g_print ("Usage: gen-doc [ ");
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
    static const gchar *descriptions[] = {
        "",
        "Read-only",
        "Write-only",
        "Read / Write",
        ""
    };

    if (pspec->flags >= 3)
        return descriptions[3];

    return descriptions[pspec->flags];
}

static void
print_property_toc (GParamSpec **pspecs, guint n_props)
{
    g_print ("<h2>Properties</h2><ul id=\"toc\">");

    for (guint i = 0; i < n_props; i++) {
        GParamSpec *pspec = pspecs[i];
        const gchar *name = g_param_spec_get_name (pspec);

        g_print ("<li><code><a href=#%s>\"%s\"</a></code></li>", name, name);
    }

    g_print ("</ul>");
}

static void
print_value_info (GParamSpec *pspec)
{
    gchar *default_value = NULL;
    GString *range = g_string_new("");

#define MAKE_RANGE(spec_type, fmt)              \
    {                                           \
        spec_type *spec = (spec_type *) pspec;  \
        g_string_printf (range,                                     \
                         fmt" &#8804; <em>%s</em> &#8804; "fmt,     \
                         spec->minimum,                             \
                         g_param_spec_get_name (pspec),             \
                         spec->maximum);                            \
        default_value = g_strdup_printf (fmt, spec->default_value); \
    }

    switch (pspec->value_type) {
        case G_TYPE_BOOLEAN:
            {
                GParamSpecBoolean *spec = (GParamSpecBoolean *) pspec;
                default_value = spec->default_value ? g_strdup ("<code>TRUE</code>") : g_strdup ("<code>FALSE</code>");
            }
            break;

        case G_TYPE_UINT:
            MAKE_RANGE (GParamSpecUInt, "%i");
            break;

        case G_TYPE_FLOAT:
            MAKE_RANGE (GParamSpecFloat, "%.1e");
            break;

        case G_TYPE_DOUBLE:
            MAKE_RANGE (GParamSpecDouble, "%.1e");
            break;
    }

#undef MAKE_RANGE

    if (g_type_is_a (pspec->value_type, G_TYPE_ENUM)) {
        GParamSpecEnum *spec = (GParamSpecEnum *) pspec;

        if (spec->enum_class->n_values > 0) {
            g_string_printf (range, "<table><tr><th>Enum name</th><th>Value</th>");

            for (guint i = 0; i < spec->enum_class->n_values; i++) {
                GEnumValue *v = &spec->enum_class->values[i];
                g_string_append_printf (range,
                                        "<tr><td><code>%s</code></td><td>%i</td></tr>",
                                        v->value_name, v->value);
            }

            g_string_append_printf (range, "</table>");
        }
    }

    if (range->len > 0)
        g_print ("<p>Possible values: %s</p>", range->str);

    if (default_value != NULL) {
        g_print ("<p>Default value: %s</p>", default_value);
        g_free (default_value);
    }

    g_string_free (range, TRUE);
}

static void
print_property_descriptions (GParamSpec **pspecs, guint n_props)
{
    g_print ("<h2>Details</h2><dl>");

    for (guint i = 0; i < n_props; i++) {
        GParamSpec *pspec = pspecs[i];
        const gchar *name = g_param_spec_get_name (pspec);

        g_print ("<dt id=\"%s\"><a href=\"#toc\">%s</a></dt>\n", name, name);
        g_print ("<dd>");
        g_print ("<pre><code class=\"prop-type\">\"%s\" : %s : %s</code></pre>\n",
                 name,
                 g_type_name (pspec->value_type),
                 get_flags_description (pspec));
        g_print ("<p>%s</p>\n", g_param_spec_get_blurb (pspec));
        print_value_info (pspec);
        g_print ("</dd>");
    }

    g_print ("</dl>");
}

static void
print_properties (UcaCamera *camera)
{
    GObjectClass *oclass;
    GParamSpec **pspecs;
    guint n_props;

    oclass = G_OBJECT_GET_CLASS (camera);
    pspecs = g_object_class_list_properties (oclass, &n_props);

    print_property_toc (pspecs, n_props);
    print_property_descriptions (pspecs, n_props);

    g_free (pspecs);
}

static const gchar *html_header = "<html><head>\
<link rel=\"stylesheet\" href=\"style.css\" type=\"text/css\" />\
<link href='http://fonts.googleapis.com/css?family=Droid+Sans:400,700|Droid+Serif:400,400italic|Inconsolata' rel='stylesheet' type='text/css'>\
<title>%s &mdash; properties</title></head><body>";
static const gchar *html_footer = "</body></html>";

int main(int argc, char *argv[])
{
    UcaPluginManager *manager;
    UcaCamera *camera;
    gchar *name;
    GError *error = NULL;

    g_type_init();
    manager = uca_plugin_manager_new ();

    if (argc < 2) {
        name = g_strdup ("Basic camera");
        camera = g_object_new (UCA_TYPE_CAMERA, NULL);
    }
    else {
        name = argv[1];
        camera = uca_plugin_manager_get_camera (manager, name, &error);
    }

    if (camera == NULL) {
        g_print("Error during initialization: %s\n", error->message);
        print_usage();
        return 1;
    }

    g_print (html_header, name);
    g_print ("<div id=\"header\"><h1 class=\"title\">Property documentation of %s</h1>", name);
    print_properties (camera);
    g_print ("%s\n", html_footer);

    g_object_unref (camera);
    g_object_unref (manager);
}
