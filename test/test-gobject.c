#include <glib-object.h>
#include "uca-camera.h"
#include "uca-mock-camera.h"

int main(int argc, char **argv)
{
    g_type_init();

    gchar **types = uca_camera_get_types();

    for (guint i = 0; i < g_strv_length(types); i++) {
        g_print("Camera: %s\n", types[i]);
    }

    g_strfreev(types);
}
