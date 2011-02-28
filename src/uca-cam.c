
#include <stdlib.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"

uint32_t uca_cam_alloc(struct uca_camera_t *cam, uint32_t n_buffers)
{
    cam->grabber->alloc(cam->grabber, n_buffers);
}

enum uca_cam_state uca_cam_get_state(struct uca_camera_t *cam)
{
    return cam->state;
}

