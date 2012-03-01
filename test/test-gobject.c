#include <glib-object.h>
#include "uca-camera.h"
#include "uca-mock-camera.h"

int main(int argc, char **argv)
{
    g_type_init();

    UcaMockCamera *cam = (UcaMockCamera *) g_object_new(UCA_TYPE_MOCK_CAMERA, NULL);

    guint width;
    g_object_get(cam,
            "sensor-width", &width,
            NULL);
    g_print("width = %i\n", width);

    uca_camera_start_recording(UCA_CAMERA(cam)); 

    g_object_unref(cam);
}
