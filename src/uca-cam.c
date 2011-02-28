
#include <stdlib.h>
#include "uca.h"
#include "uca-cam.h"

enum uca_cam_state uca_get_camera_state(struct uca_camera_t *cam)
{
    return cam->state;
}

