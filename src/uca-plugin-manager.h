#ifndef __UCA_PLUGIN_MANAGER_H
#define __UCA_PLUGIN_MANAGER_H

#include <glib-object.h>
#include "uca-camera.h"

G_BEGIN_DECLS

#define UCA_TYPE_PLUGIN_MANAGER             (uca_plugin_manager_get_type())
#define UCA_PLUGIN_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_PLUGIN_MANAGER, UcaPluginManager))
#define UCA_IS_PLUGIN_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_PLUGIN_MANAGER))
#define UCA_PLUGIN_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UCA_TYPE_PLUGIN_MANAGER, UcaPluginManagerClass))
#define UCA_IS_PLUGIN_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_PLUGIN_MANAGER))
#define UCA_PLUGIN_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_PLUGIN_MANAGER, UcaPluginManagerClass))

#define UCA_PLUGIN_MANAGER_ERROR uca_plugin_manager_error_quark()
GQuark uca_plugin_manager_error_quark(void);

typedef enum {
    UCA_PLUGIN_MANAGER_ERROR_MODULE_NOT_FOUND,
    UCA_PLUGIN_MANAGER_ERROR_MODULE_OPEN,
    UCA_PLUGIN_MANAGER_ERROR_SYMBOL_NOT_FOUND
} UcaPluginManagerError;

typedef struct _UcaPluginManager           UcaPluginManager;
typedef struct _UcaPluginManagerClass      UcaPluginManagerClass;
typedef struct _UcaPluginManagerPrivate    UcaPluginManagerPrivate;

/**
 * UcaPluginManager:
 *
 * Creates #UcaFilter instances by loading corresponding shared objects. The
 * contents of the #UcaPluginManager structure are private and should only be
 * accessed via the provided API.
 */
struct _UcaPluginManager {
    /*< private >*/
    GObject parent_instance;

    UcaPluginManagerPrivate *priv;
};

/**
 * UcaPluginManagerClass:
 *
 * #UcaPluginManager class
 */
struct _UcaPluginManagerClass {
    /*< private >*/
    GObjectClass parent_class;
};

UcaPluginManager    *uca_plugin_manager_new         (void);
void                 uca_plugin_manager_add_path    (UcaPluginManager   *manager,
                                                     const gchar        *path);
GList               *uca_plugin_manager_get_available_cameras
                                                    (UcaPluginManager   *manager);
UcaCamera           *uca_plugin_manager_get_camerav (UcaPluginManager   *manager,
                                                     const gchar        *name,
                                                     guint               n_parameters,
                                                     GParameter         *parameters,
                                                     GError            **error);
UcaCamera           *uca_plugin_manager_get_camera  (UcaPluginManager   *manager,
                                                     const gchar        *name,
                                                     GError            **error,
                                                     const gchar        *first_prop_name,
                                                     ...);
GType                uca_plugin_manager_get_type    (void);

G_END_DECLS

#endif
