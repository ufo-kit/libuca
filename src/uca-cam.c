
#include <stdlib.h>
#include <string.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"


struct uca_camera_priv *uca_cam_new(void)
{
    struct uca_camera_priv *cam = (struct uca_camera_priv *) malloc(sizeof(struct uca_camera_priv));

    /* Set all function pointers to NULL so we know early on, if something has
     * not been implemented. */
    memset(cam, 0, sizeof(struct uca_camera_priv));

    cam->state = UCA_CAM_CONFIGURABLE;
    cam->current_frame = 0;
    return cam;
}
