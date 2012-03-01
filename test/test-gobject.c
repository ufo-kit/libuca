#include <glib-object.h>
#include "uca-camera.h"
#include "uca-pco-camera.h"

int main(int argc, char **argv)
{
    g_type_init();

    GError *error = NULL;
    UcaPcoCamera *cam = uca_pco_camera_new(&error);

    if (cam == NULL) {
        g_error("Camera could not be initialized\n");
    }

    guint width, height;
    g_object_get(cam,
            "sensor-width", &width,
            "sensor-height", &height,
            NULL);
    g_print("resolution %ix%i\n", width, height);

    g_object_unref(cam);
}
