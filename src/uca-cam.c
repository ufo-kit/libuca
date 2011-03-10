
#include <stdlib.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"

uint32_t uca_cam_alloc(struct uca_camera_t *cam, uint32_t n_buffers)
{
    uint32_t bitdepth;
    cam->get_property(cam, UCA_PROP_BITDEPTH, &bitdepth);
    const int pixel_size = bitdepth == 8 ? 1 : 2;
    if (cam->grabber != NULL)
        return cam->grabber->alloc(cam->grabber, pixel_size, n_buffers);
    return UCA_NO_ERROR;
}

enum uca_cam_state uca_cam_get_state(struct uca_camera_t *cam)
{
    return cam->state;
}

