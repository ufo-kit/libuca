
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"


static uint32_t uca_simple_set_property(struct uca_camera_priv *cam, enum uca_property_ids property, void *data)
{
    struct uca_grabber_priv *grabber = cam->grabber;
    int err = UCA_NO_ERROR;

    /* We try to set the property on the grabber. If it returns "invalid", we
     * also try it via the PF SDK. Else, there was a more serious error.
     *
     * FIXME: This is actually not that good for cases where only the grabber
     * should set a certain property and the camera itself is not able to do so. */
    err = grabber->set_property(grabber, property, data);
    if (((err & UCA_ERR_MASK_CODE) == UCA_ERR_INVALID) || (err == UCA_NO_ERROR))
        err = UCA_ERR_CAMERA | UCA_ERR_PROP;
    else
        return err;

    return UCA_NO_ERROR;
}


static uint32_t uca_simple_get_property(struct uca_camera_priv *cam, enum uca_property_ids property, void *data, size_t num)
{
    struct uca_grabber_priv *grabber = cam->grabber;
    uint32_t value32 = 0;

    /* Handle all special cases */
    switch (property) {
        case UCA_PROP_NAME:
            strncpy((char *) data, "Simple Framegrabber Access", num);
            break;

        case UCA_PROP_WIDTH:
            grabber->get_property(grabber, UCA_PROP_WIDTH, &value32);
            uca_set_void(data, uint32_t, value32);
            break;

        case UCA_PROP_HEIGHT:
            grabber->get_property(grabber, UCA_PROP_HEIGHT, &value32);
            uca_set_void(data, uint32_t, value32);
            break;

        case UCA_PROP_X_OFFSET:
            grabber->get_property(grabber, UCA_PROP_X_OFFSET, &value32);
            uca_set_void(data, uint32_t, value32);
            break;

        case UCA_PROP_Y_OFFSET:
            grabber->get_property(grabber, UCA_PROP_Y_OFFSET, &value32);
            uca_set_void(data, uint32_t, value32);
            break;

        case UCA_PROP_BITDEPTH:
            uca_set_void(data, uint32_t, 8);
            return UCA_NO_ERROR;

        default:
            break;
    }

    /* Try to get the property via frame grabber */
    return cam->grabber->get_property(cam->grabber, property, data);
}

static uint32_t uca_simple_start_recording(struct uca_camera_priv *cam)
{
    return cam->grabber->acquire(cam->grabber, -1);
}

static uint32_t uca_simple_stop_recording(struct uca_camera_priv *cam)
{
    return cam->grabber->stop_acquire(cam->grabber);
}

static uint32_t uca_simple_trigger(struct uca_camera_priv *cam)
{
    return cam->grabber->trigger(cam->grabber);
}

static uint32_t uca_simple_grab(struct uca_camera_priv *cam, char *buffer, void *metadata)
{
    uint16_t *frame;
    uint32_t err = cam->grabber->grab(cam->grabber, (void **) &frame, &cam->current_frame);
    if (err != UCA_NO_ERROR)
        return err;

    memcpy(buffer, frame, cam->frame_width*cam->frame_height);
    return UCA_NO_ERROR;
}

static uint32_t uca_simple_register_callback(struct uca_camera_priv *cam, uca_cam_grab_callback callback, void *user)
{
    if (cam->callback == NULL) {
        cam->callback = callback;
        cam->callback_user = user;
        return cam->grabber->register_callback(cam->grabber, callback, NULL, user);
    }
    return UCA_ERR_CAMERA | UCA_ERR_CALLBACK | UCA_ERR_ALREADY_REGISTERED;
}

static uint32_t uca_simple_destroy(struct uca_camera_priv *cam)
{
    return UCA_NO_ERROR;
}

uint32_t uca_simple_init(struct uca_camera_priv **cam, struct uca_grabber_priv *grabber)
{
    if (grabber == NULL)
        return UCA_ERR_CAMERA | UCA_ERR_INIT | UCA_ERR_NOT_FOUND;

    struct uca_camera_priv *uca = uca_cam_new();
    uca->grabber = grabber;
    uca->grabber->synchronous = false;

    /* Camera found, set function pointers... */
    uca->destroy = &uca_simple_destroy;
    uca->set_property = &uca_simple_set_property;
    uca->get_property = &uca_simple_get_property;
    uca->start_recording = &uca_simple_start_recording;
    uca->stop_recording = &uca_simple_stop_recording;
    uca->trigger = &uca_simple_trigger;
    uca->grab = &uca_simple_grab;
    uca->register_callback = &uca_simple_register_callback;

    /* Prepare frame grabber for recording */
    int val = UCA_CL_8BIT_FULL_8;
    grabber->set_property(grabber, UCA_GRABBER_CAMERALINK_TYPE, &val);

    val = UCA_FORMAT_GRAY8;
    grabber->set_property(grabber, UCA_GRABBER_FORMAT, &val);

    val = UCA_TRIGGER_AUTO;
    grabber->set_property(grabber, UCA_GRABBER_TRIGGER_MODE, &val);

    grabber->get_property(grabber, UCA_PROP_WIDTH, &uca->frame_width);
    grabber->get_property(grabber, UCA_PROP_HEIGHT, &uca->frame_height);

    uca->state = UCA_CAM_CONFIGURABLE;
    *cam = uca;

    return UCA_NO_ERROR;
}
