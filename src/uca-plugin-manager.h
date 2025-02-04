#ifndef __UCA_PLUGIN_MANAGER_H
#define __UCA_PLUGIN_MANAGER_H

#include <glib-object.h>
#include "uca-camera.h"

G_BEGIN_DECLS

#define UCA_TYPE_PLUGIN_MANAGER             (uca_plugin_manager_get_type())
G_DECLARE_FINAL_TYPE(UcaPluginManager, uca_plugin_manager, UCA, PLUGIN_MANAGER, GObject)

#define UCA_PLUGIN_MANAGER_ERROR uca_plugin_manager_error_quark()
GQuark uca_plugin_manager_error_quark(void);

typedef enum {
    UCA_PLUGIN_MANAGER_ERROR_MODULE_NOT_FOUND,
    UCA_PLUGIN_MANAGER_ERROR_MODULE_OPEN,
    UCA_PLUGIN_MANAGER_ERROR_SYMBOL_NOT_FOUND
} UcaPluginManagerError;

/**
 * UcaPluginManager:
 *
 * Creates #UcaFilter instances by loading corresponding shared objects. The
 * contents of the #UcaPluginManager structure are private and should only be
 * accessed via the provided API.
 */

UcaPluginManager    *uca_plugin_manager_new         (void);
void                 uca_plugin_manager_add_path    (UcaPluginManager   *manager,
                                                     const gchar        *path);
GList               *uca_plugin_manager_get_available_cameras
                                                    (UcaPluginManager   *manager);
UcaCamera           *uca_plugin_manager_get_camerah (UcaPluginManager   *manager,
                                                     const gchar        *name,
                                                     GHashTable         *parameters,
                                                     GError            **error);
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

G_END_DECLS

#endif
