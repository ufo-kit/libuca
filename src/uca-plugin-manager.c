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

/**
 * SECTION:uca-plugin-manager
 * @Short_description: Load an #UcaFilter from a shared object
 * @Title: UcaPluginManager
 *
 * The plugin manager opens shared object modules searched for in locations
 * specified with uca_plugin_manager_add_path(). A #UcaCamera can be
 * instantiated with uca_plugin_manager_new_camera() with a one-to-one mapping
 * between filter name xyz and module name libucaxyz.so. Any errors are reported
 * as one of #UcaPluginManagerError codes. To get a list of available camera
 * names call uca_plugin_manager_get_available_cameras().
 *
 * By default, any path listed in the %UCA_CAMERA_PATH environment variable is
 * added to the search path.
 *
 * Since: 1.1
 */

#include "config.h"

#include <gio/gio.h>
#include <gmodule.h>
#include "uca-plugin-manager.h"

G_DEFINE_TYPE (UcaPluginManager, uca_plugin_manager, G_TYPE_OBJECT)

#define UCA_PLUGIN_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_PLUGIN_MANAGER, UcaPluginManagerPrivate))

struct _UcaPluginManagerPrivate {
    GList *search_paths;
    GList *funcs;
};

#ifdef _WIN32
    static const gchar *MODULE_PATTERN = "uca([A-Za-z0-9]+)";
#else
    static const gchar *MODULE_PATTERN = "libuca([A-Za-z0-9]+)";
#endif

typedef GType (*GetTypeFunc) (void);

typedef struct {
    GParameter  *p;
    guint       idx;
} ParamArray;


/**
 * UcaPluginManagerError:
 * @UCA_PLUGIN_MANAGER_ERROR_MODULE_NOT_FOUND: The module could not be found
 * @UCA_PLUGIN_MANAGER_ERROR_MODULE_OPEN: Module could not be opened
 * @UCA_PLUGIN_MANAGER_ERROR_SYMBOL_NOT_FOUND: Necessary entry symbol was not
 *      found
 *
 * Possible errors that uca_plugin_manager_get_filter() can return.
 */
GQuark
uca_plugin_manager_error_quark (void)
{
    return g_quark_from_static_string ("uca-plugin-manager-error-quark");
}

/**
 * uca_plugin_manager_new:
 *
 * Create a plugin manager object to instantiate camera objects.
 *
 * Return value: A new plugin manager object.
 */
UcaPluginManager *
uca_plugin_manager_new ()
{
    return UCA_PLUGIN_MANAGER (g_object_new (UCA_TYPE_PLUGIN_MANAGER, NULL));
}

/**
 * uca_plugin_manager_add_path:
 * @manager: A #UcaPluginManager
 * @path: Path to look for camera modules
 *
 * Add a search path to the plugin manager.
 */
void
uca_plugin_manager_add_path (UcaPluginManager   *manager,
                             const gchar        *path)
{
    g_return_if_fail (UCA_IS_PLUGIN_MANAGER (manager));

    if (g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
        UcaPluginManagerPrivate *priv;

        priv = manager->priv;
        priv->search_paths = g_list_append (priv->search_paths,
                                            g_strdup (path));
    }
}

static GList *
get_camera_module_paths (const gchar *path)
{
    GRegex *pattern;
    GDir *dir;
    GList *result = NULL;

    pattern = g_regex_new (MODULE_PATTERN, 0, 0, NULL);
    dir = g_dir_open (path, 0, NULL);

    if (dir != NULL) {
        GMatchInfo *match_info = NULL;
        const gchar *name = g_dir_read_name (dir);

        while (name != NULL) {
            if (g_regex_match (pattern, name, 0, &match_info))
                result = g_list_append (result, g_build_filename (path, name, NULL));

            g_match_info_free (match_info);
            name = g_dir_read_name (dir);
        }
    }

    g_dir_close (dir);
    g_regex_unref (pattern);

    return result;
}

static GList *
scan_search_paths (GList *search_paths)
{
    GList *camera_paths = NULL;

    for (GList *it = g_list_first (search_paths); it != NULL; it = g_list_next (it)) {
        camera_paths = g_list_concat (camera_paths,
                                      get_camera_module_paths ((const gchar*) it->data));
    }

    return camera_paths;
}

static void
transform_camera_module_path_to_name (gchar *path, GList **result)
{
    GRegex *pattern;
    GMatchInfo *match_info;

    pattern = g_regex_new (MODULE_PATTERN, 0, 0, NULL);
    g_regex_match (pattern, path, 0, &match_info);

    *result = g_list_append (*result, g_match_info_fetch (match_info, 1));
    g_match_info_free (match_info);
    g_regex_unref (pattern);
}

/**
 * uca_plugin_manager_get_available_cameras:
 * @manager: A #UcaPluginManager
 *
 * Return value: (element-type utf8) (transfer full): A list with strings of
 * available camera names. You have to free the individual strings with
 * g_list_foreach(list, (GFunc) g_free, NULL) and the list itself with
 * g_list_free.
 */
GList *
uca_plugin_manager_get_available_cameras (UcaPluginManager *manager)
{
    UcaPluginManagerPrivate *priv;
    GList *camera_paths;
    GList *camera_names = NULL;

    g_return_val_if_fail (UCA_IS_PLUGIN_MANAGER (manager), NULL);

    priv = manager->priv;
    camera_paths = scan_search_paths (priv->search_paths);

    g_list_foreach (camera_paths, (GFunc) transform_camera_module_path_to_name, &camera_names);
    g_list_free_full (camera_paths, g_free);

    return camera_names;
}

static gchar *
find_camera_module_path (GList *search_paths, const gchar *name)
{
    gchar *result = NULL;
    gchar *modname;
    GList *paths;

#ifdef _WIN32
    modname = g_strdup_printf ("uca%s.dll", name);
#else
    modname = g_strdup_printf ("libuca%s.so", name);
#endif
    paths = scan_search_paths (search_paths);

    for (GList *it = g_list_first (paths); it != NULL; it = g_list_next (it)) {
        gchar *path = (gchar *) it->data;
        gchar *basename = g_path_get_basename ((gchar *) path);

        if (g_strrstr (basename, modname)) {
            result = g_strdup (path);
            g_free (basename);
            break;
        }

        g_free (basename);
    }

    g_free (modname);
    g_list_free_full (paths, g_free);
    return result;
}

static GType
get_camera_type (UcaPluginManagerPrivate *priv,
                 const gchar *name,
                 GError **error)
{
    GModule *module;
    gchar *module_path;
    GetTypeFunc *func;
    const gchar *symbol_name = "camera_plugin_get_type";

    module_path = find_camera_module_path (priv->search_paths, name);
    g_debug ("Trying to load `%s' from %s.", name, module_path);

    if (module_path == NULL) {
        g_set_error (error, UCA_PLUGIN_MANAGER_ERROR, UCA_PLUGIN_MANAGER_ERROR_MODULE_NOT_FOUND,
                     "Camera module `%s' not found", name);
        return G_TYPE_NONE;
    }

    module = g_module_open (module_path, G_MODULE_BIND_LAZY);
    g_free (module_path);

    if (!module) {
        g_set_error (error, UCA_PLUGIN_MANAGER_ERROR, UCA_PLUGIN_MANAGER_ERROR_MODULE_OPEN,
                     "Camera module `%s' could not be opened: %s", name, g_module_error ());
        return G_TYPE_NONE;
    }

    func = g_malloc0 (sizeof (GetTypeFunc));

    if (!g_module_symbol (module, symbol_name, (gpointer *) func)) {
        g_set_error (error, UCA_PLUGIN_MANAGER_ERROR, UCA_PLUGIN_MANAGER_ERROR_SYMBOL_NOT_FOUND,
                     "%s", g_module_error ());
        g_free (func);

        if (!g_module_close (module))
            g_warning ("%s", g_module_error ());

        return G_TYPE_NONE;
    }

    priv->funcs = g_list_append (priv->funcs, func);

    return (*func) ();
}

/**
 * transform_hash_entry_to_gparameter: (skip)
 * @key: #gpointer to the key value of the hash entry
 * @value: #gpointer to the value of the hash entry
 * @parameter: (out caller-allocates): Pointer to the #GParameter where key and value
 * should be filled into
 *
 * Takes a Key/Value pair from a #GHashTable and puts its values into the given
 * #GParameter.
 * Note that this function uses g_strdup() to set the #GParameters name value. This
 * needs to be cleaned up by the caller using g_free()
 *
 * Since: 1.7
 */
static void
transform_hash_entry_to_gparameter (gpointer key,
                                    gpointer value,
                                    ParamArray *params)
{
    GParameter *parameter = &(params->p[params->idx]);

    if (value)
        parameter->value = *((GValue*)value);
    else
        g_value_set_gtype (&(parameter->value), G_TYPE_NONE);
    parameter->name = g_strdup ((const gchar*)key);
    params->idx++;
}

/**
 * uca_plugin_manager_get_camerah:
 * @manager: A #UcaPluginManager
 * @name: Name of the camera module, that maps to libuca<name>.so
 * @parameters: (element-type utf8 GValue) (transfer none) (allow-none): a pointer to a #GHashTable containing parameters or %NULL
 * @error: (allow-none): Location for a #GError or %NULL
 *
 * Create a new camera instance with camera @name.
 *
 * Returns: (transfer full): A new #UcaCamera object.
 * Since: 1.7
 */
UcaCamera *
uca_plugin_manager_get_camerah (UcaPluginManager *manager,
                                const gchar *name,
                                GHashTable *parameters,
                                GError **error)
{
    /* No parameters. Just create the camera */
    if (!parameters)
        return uca_plugin_manager_get_camera (manager, name, error, NULL);

    /* If we reach this point, we have parameters. Construct GParameters for
     * them and use uca_plugin_manager_get_camerav to create the camera. */
    guint n_parameters = g_hash_table_size (parameters);
    ParamArray params;
    params.p = g_malloc0 (sizeof(GParameter) * n_parameters);
    params.idx = 0;

    g_hash_table_foreach (parameters, (GHFunc) transform_hash_entry_to_gparameter, &params);

    return uca_plugin_manager_get_camerav (manager, name, n_parameters, params.p, error);
}

/**
 * uca_plugin_manager_get_camerav:
 * @manager: A #UcaPluginManager
 * @name: Name of the camera module, that maps to libuca<name>.so
 * @n_parameters: number of parameters in @parameters
 * @parameters: (array length=n_parameters): the parameters to use to construct
 *      the camera
 * @error: Location for a #GError or %NULL
 *
 * Create a new camera instance with camera @name.
 *
 * Returns: (transfer full): A new #UcaCamera object.
 * Since: 1.2
 */
UcaCamera *
uca_plugin_manager_get_camerav (UcaPluginManager *manager,
                                const gchar *name,
                                guint n_parameters,
                                GParameter *parameters,
                                GError **error)
{
    UcaPluginManagerPrivate *priv;
    UcaCamera *camera;
    GType type;

    g_return_val_if_fail (UCA_IS_PLUGIN_MANAGER (manager) && (name != NULL), NULL);

    priv = manager->priv;
    type = get_camera_type (priv, name, error);

    if (type == G_TYPE_NONE)
        return NULL;

    camera = (UcaCamera *) g_initable_newv (type, n_parameters, parameters,
                                            NULL, error);

    return camera;
}

/**
 * uca_plugin_manager_get_camera: (skip)
 * @manager: A #UcaPluginManager
 * @name: Name of the camera module, that maps to libuca<name>.so
 * @error: Location for a #GError
 * @first_prop_name: (allow-none): name of the first property, or %NULL if no
 *      properties
 * @...: value of the first property, followed by and other property value
 *      pairs, and ended by %NULL.
 *
 * Create a new camera instance with camera @name.
 *
 * Returns: (transfer full): A new #UcaCamera object.
 * Since: 1.2: Pass construction properties.
 */
UcaCamera *
uca_plugin_manager_get_camera (UcaPluginManager *manager,
                               const gchar *name,
                               GError **error,
                               const gchar *first_prop_name,
                               ...)
{
    UcaPluginManagerPrivate *priv;
    UcaCamera *camera;
    GType type;
    va_list var_args;

    g_return_val_if_fail (UCA_IS_PLUGIN_MANAGER (manager) && (name != NULL), NULL);

    priv = manager->priv;
    type = get_camera_type (priv, name, error);

    if (type == G_TYPE_NONE)
        return NULL;

    va_start (var_args, first_prop_name);
    camera = (UcaCamera *) g_initable_new (type, NULL, error,
                                           first_prop_name, var_args);
    va_end (var_args);

    return camera;
}

static void
uca_plugin_manager_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
uca_plugin_manager_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
uca_plugin_manager_dispose (GObject *object)
{
    G_OBJECT_CLASS (uca_plugin_manager_parent_class)->dispose (object);
}

static void
uca_plugin_manager_finalize (GObject *object)
{
    UcaPluginManagerPrivate *priv = UCA_PLUGIN_MANAGER_GET_PRIVATE (object);

    g_list_free_full (priv->search_paths, g_free);
    g_list_free_full (priv->funcs, g_free);

    G_OBJECT_CLASS (uca_plugin_manager_parent_class)->finalize (object);
}

static void
uca_plugin_manager_class_init (UcaPluginManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->get_property = uca_plugin_manager_get_property;
    gobject_class->set_property = uca_plugin_manager_set_property;
    gobject_class->dispose      = uca_plugin_manager_dispose;
    gobject_class->finalize     = uca_plugin_manager_finalize;

    g_type_class_add_private (klass, sizeof (UcaPluginManagerPrivate));
}

static void
uca_plugin_manager_init (UcaPluginManager *manager)
{
    UcaPluginManagerPrivate *priv;
    const gchar *uca_camera_path;

    manager->priv = priv = UCA_PLUGIN_MANAGER_GET_PRIVATE (manager);
    priv->search_paths = NULL;
    priv->funcs = NULL;

    uca_camera_path = g_getenv ("UCA_CAMERA_PATH");

    if (uca_camera_path != NULL)
        uca_plugin_manager_add_path (manager, uca_camera_path);

    uca_plugin_manager_add_path (manager, UCA_PLUGINDIR);
}
