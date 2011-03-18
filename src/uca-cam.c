
#include <stdlib.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"

uint32_t uca_cam_alloc(struct uca_camera *cam, uint32_t n_buffers)
{
    uint32_t bitdepth;
    cam->get_property(cam, UCA_PROP_BITDEPTH, &bitdepth, 0);
    const int pixel_size = bitdepth == 8 ? 1 : 2;
    if (cam->grabber != NULL)
        return cam->grabber->alloc(cam->grabber, pixel_size, n_buffers);
    return UCA_NO_ERROR;
}

enum uca_cam_state uca_cam_get_state(struct uca_camera *cam)
{
    return cam->state;
}

struct uca_camera *uca_cam_new(void)
{
    struct uca_camera *cam = (struct uca_camera *) malloc(sizeof(struct uca_camera));

    cam->next = NULL;

    /* Set all function pointers to NULL so we know early on, if something has
     * not been implemented. */
    cam->set_property = NULL;
    cam->get_property = NULL;
    cam->start_recording = NULL;
    cam->stop_recording = NULL;
    cam->grab = NULL;
    cam->register_callback = NULL;
    cam->destroy = NULL;

    cam->user = NULL;

    cam->grabber = NULL;
    cam->state = UCA_CAM_CONFIGURABLE;
    cam->current_frame = 0;

    /* No callbacks and user data associated yet */
    cam->callback = NULL;
    cam->callback_user = NULL;

    return cam;
}
