/**
 * SECTION:uca-plugin-manager
 * @Short_description: Load an #UcaFilter from a shared object
 * @Title: UcaPluginManager
 *
 * The plugin manager opens shared object modules searched for in locations
 * specified with uca_plugin_manager_add_paths(). An #UcaFilter can be
 * instantiated with uca_plugin_manager_get_filter() with a one-to-one mapping
 * between filter name xyz and module name libfilterxyz.so. Any errors are
 * reported as one of #UcaPluginManagerError codes.
 */
#include <gmodule.h>
#include "uca-plugin-manager.h"

G_DEFINE_TYPE (UcaPluginManager, uca_plugin_manager, G_TYPE_OBJECT)

#define UCA_PLUGIN_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_PLUGIN_MANAGER, UcaPluginManagerPrivate))

struct _UcaPluginManagerPrivate {
    GList *search_paths;
};

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
 * @config: (allow-none): A #UcaConfiguration object or %NULL.
 *
 * Create a plugin manager object to instantiate filter objects. When a config
 * object is passed to the constructor, its search-path property is added to the
 * internal search paths.
 *
 * Return value: A new plugin manager object.
 */
UcaPluginManager *
uca_plugin_manager_new ()
{
    return UCA_PLUGIN_MANAGER (g_object_new (UCA_TYPE_PLUGIN_MANAGER, NULL));
}

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
get_camera_names_from_directory (const gchar *path)
{
    static const gchar *pattern_string = "libuca([A-Za-z]+)";
    GRegex *pattern; 
    GDir *dir;
    GList *result = NULL;
    
    pattern = g_regex_new (pattern_string, 0, 0, NULL);
    dir = g_dir_open (path, 0, NULL);

    if (dir != NULL) {
        GMatchInfo *match_info = NULL;
        const gchar *name = g_dir_read_name (dir); 

        while (name != NULL) {
            g_regex_match (pattern, name, 0, &match_info);

            if (g_match_info_matches (match_info)) {
                gchar *word = g_match_info_fetch (match_info, 1);

                if (word != NULL)
                    result = g_list_append (result, word);
            }

            name = g_dir_read_name (dir);
        }
    }

    return result;
}

GList *
uca_plugin_manager_get_available_cameras (UcaPluginManager *manager)
{
    UcaPluginManagerPrivate *priv;
    GList *camera_names = NULL;

    g_return_val_if_fail (UCA_IS_PLUGIN_MANAGER (manager), NULL);

    priv = manager->priv;

    for (GList *it = g_list_first (priv->search_paths); it != NULL; it = g_list_next (it)) {
        camera_names = g_list_concat (camera_names,
                                      get_camera_names_from_directory ((const gchar*) it->data));
    }

    return camera_names;
}

UcaCamera *
uca_plugin_manager_new_camera (UcaPluginManager   *manager,
                               const gchar        *name,
                               GError            **error)
{
    return NULL;
}

/**
 * uca_plugin_manager_get_filter:
 * @manager: A #UcaPluginManager
 * @name: Name of the plugin.
 * @error: return location for a GError or %NULL
 *
 * Load a #UcaFilter module and return an instance. The shared object name must
 * be * constructed as "libfilter@name.so".
 *
 * Returns: (transfer none) (allow-none): #UcaFilter or %NULL if module cannot be found
 *
 * Since: 0.2, the error parameter is available
 */
/* UcaFilter * */
/* uca_plugin_manager_get_filter (UcaPluginManager *manager, const gchar *name, GError **error) */
/* { */
/*     g_return_val_if_fail (UCA_IS_PLUGIN_MANAGER (manager) && (name != NULL), NULL); */
/*     UcaPluginManagerPrivate *priv = UCA_PLUGIN_MANAGER_GET_PRIVATE (manager); */
/*     UcaFilter *filter; */
/*     GetFilterFunc *func = NULL; */
/*     GModule *module = NULL; */
/*     gchar *module_name = NULL; */
/*     const gchar *entry_symbol_name = "uca_filter_plugin_new"; */

/*     func = g_hash_table_lookup (priv->filter_funcs, name); */

/*     if (func == NULL) { */
/*         module_name = g_strdup_printf ("libucafilter%s.so", name); */
/*         gchar *path = plugin_manager_get_path (priv, module_name); */

/*         if (path == NULL) { */
/*             g_set_error (error, UCA_PLUGIN_MANAGER_ERROR, UCA_PLUGIN_MANAGER_ERROR_MODULE_NOT_FOUND, */
/*                     "Module %s not found", module_name); */
/*             goto handle_error; */
/*         } */

/*         module = g_module_open (path, G_MODULE_BIND_LAZY); */
/*         g_free (path); */

/*         if (!module) { */
/*             g_set_error (error, UCA_PLUGIN_MANAGER_ERROR, UCA_PLUGIN_MANAGER_ERROR_MODULE_OPEN, */
/*                     "Module %s could not be opened: %s", module_name, g_module_error ()); */
/*             goto handle_error; */
/*         } */

/*         func = g_malloc0 (sizeof (GetFilterFunc)); */

/*         if (!g_module_symbol (module, entry_symbol_name, (gpointer *) func)) { */
/*             g_set_error (error, UCA_PLUGIN_MANAGER_ERROR, UCA_PLUGIN_MANAGER_ERROR_SYMBOL_NOT_FOUND, */
/*                     "%s is not exported by module %s: %s", entry_symbol_name, module_name, g_module_error ()); */
/*             g_free (func); */

/*             if (!g_module_close (module)) */
/*                 g_warning ("%s", g_module_error ()); */

/*             goto handle_error; */
/*         } */

/*         priv->modules = g_slist_append (priv->modules, module); */
/*         g_hash_table_insert (priv->filter_funcs, g_strdup (name), func); */
/*         g_free (module_name); */
/*     } */

/*     filter = (*func) (); */
/*     uca_filter_set_plugin_name (filter, name); */
/*     g_message ("UcaPluginManager: Created %s-%p", name, (gpointer) filter); */

/*     return filter; */

/* handle_error: */
/*     g_free (module_name); */
/*     return NULL; */
/* } */

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

    g_list_foreach (priv->search_paths, (GFunc) g_free, NULL);
    g_list_free (priv->search_paths);

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

    manager->priv = priv = UCA_PLUGIN_MANAGER_GET_PRIVATE (manager);
    priv->search_paths = NULL;

    uca_plugin_manager_add_path (manager, "/usr/lib/uca");
    uca_plugin_manager_add_path (manager, "/usr/lib64/uca");
    uca_plugin_manager_add_path (manager, "/usr/local/lib/uca");
    uca_plugin_manager_add_path (manager, "/usr/local/lib64/uca");
}
