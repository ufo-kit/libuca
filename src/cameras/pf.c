
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <libpf/pfcam.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"

struct uca_pf_map {
    enum uca_property_ids uca_prop;
    const char *pf_prop;
};

static struct uca_pf_map uca_to_pf[] = {
    { UCA_PROP_NAME,        "CameraName" },
    { UCA_PROP_WIDTH,       "Window.W" },
    { UCA_PROP_WIDTH_MIN,   "Window.W.Min" },
    { UCA_PROP_WIDTH_MAX,   "Window.W.Max" },
    { UCA_PROP_HEIGHT,      "Window.H" },
    { UCA_PROP_HEIGHT_MIN,  "Window.H.Min" },
    { UCA_PROP_HEIGHT_MAX,  "Window.H.Max" },
    { UCA_PROP_X_OFFSET,    "Window.X" },
    { UCA_PROP_X_OFFSET_MIN,"Window.X.Min" },
    { UCA_PROP_X_OFFSET_MAX,"Window.X.Max" },
    { UCA_PROP_Y_OFFSET,    "Window.Y" },
    { UCA_PROP_Y_OFFSET_MIN,"Window.Y.Min" },
    { UCA_PROP_Y_OFFSET_MAX,"Window.Y.Max" },
    { UCA_PROP_EXPOSURE,    "ExposureTime" },
    { UCA_PROP_EXPOSURE_MIN,"ExposureTime.Min" },
    { UCA_PROP_EXPOSURE_MAX,"ExposureTime.Max" },
    { UCA_PROP_DELAY,       "Trigger.Delay" },
    { UCA_PROP_DELAY_MIN,   "Trigger.Delay.Min" },
    { UCA_PROP_DELAY_MAX,   "Trigger.Delay.Max" },
    { UCA_PROP_FRAMERATE,   "FrameRate" },
    { UCA_PROP_TRIGGER_MODE,"Trigger.Source" },
    { -1, NULL }
};

static int uca_pf_set_uint32_property(TOKEN token, void *data, uint32_t *update_var)
{
    PFValue value;
    value.type = PF_INT;
    value.value.i = *((uint32_t *) data);
    if (update_var != NULL)
        *update_var = value.value.i;
    return pfDevice_SetProperty(0, token, &value);
}

static uint32_t uca_pf_set_property(struct uca_camera *cam, enum uca_property_ids property, void *data)
{
    struct uca_grabber *grabber = cam->grabber;
    TOKEN token = INVALID_TOKEN;
    int i = 0;
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

    /* Find a valid pf token for the property */
    while (uca_to_pf[i].uca_prop != -1) {
        if (uca_to_pf[i].uca_prop == property) {
            token = pfProperty_ParseName(0, uca_to_pf[i].pf_prop);
            break;
        }
        i++;
    }
    if (token == INVALID_TOKEN)
        return err | UCA_ERR_INVALID;

    PFValue value;

    switch (property) {
        case UCA_PROP_WIDTH:
            if (uca_pf_set_uint32_property(token, data, &cam->frame_width) < 0)
                return err | UCA_ERR_OUT_OF_RANGE;
            break;

        case UCA_PROP_HEIGHT:
            if (uca_pf_set_uint32_property(token, data, &cam->frame_height) < 0)
                return err | UCA_ERR_OUT_OF_RANGE;
            break;

        case UCA_PROP_EXPOSURE:
            /* I haven't found a specification but it looks like PF uses milli
             * seconds. We also by-pass the frame grabber... */
            value.type = PF_FLOAT;
            value.value.f = (float) *((uint32_t *) data) / 1000.0;
            if (pfDevice_SetProperty(0, token, &value) < 0)
                return err | UCA_ERR_OUT_OF_RANGE;
            break;

        default:
            return err | UCA_ERR_INVALID;
    }
    return UCA_NO_ERROR;
}


static uint32_t uca_pf_get_property(struct uca_camera *cam, enum uca_property_ids property, void *data, size_t num)
{
    TOKEN t;    /* You gotta love developers who name types capitalized... */
    PFValue value;

    /* Handle all special cases */
    switch (property) {
        case UCA_PROP_BITDEPTH:
            uca_set_void(data, uint32_t, 8);
            return UCA_NO_ERROR;

        default:
            break;
    }

    int i = 0;
    while (uca_to_pf[i].uca_prop != -1) {
        if (uca_to_pf[i].uca_prop == property) {
            t = pfProperty_ParseName(0, uca_to_pf[i].pf_prop);
            if (t == INVALID_TOKEN || (pfDevice_GetProperty(0, t, &value) < 0))
                return UCA_ERR_CAMERA | UCA_ERR_PROP | UCA_ERR_INVALID;

            switch (value.type) {
                case PF_INT:
                    uca_set_void(data, uint32_t, value.value.i);
                    break;

                case PF_FLOAT:
                    uca_set_void(data, uint32_t, (uint32_t) floor((value.value.f * 1000.0)+0.5));
                    break;

                case PF_STRING:
                    if (property == UCA_PROP_FRAMERATE) {
                        uca_set_void(data, uint32_t, (uint32_t) floor(atof(value.value.p)+0.5));
                    }
                    else {
                        strncpy((char *) data, value.value.p, num);
                    }
                    break;

                case PF_MODE:
                    uca_set_void(data, uint32_t, (uint32_t) value.value.i);
                    break;

                default:
                    break;
            }
            return UCA_NO_ERROR;
        }
        i++;
    }

    /* Try to get the property via frame grabber */
    return cam->grabber->get_property(cam->grabber, property, data);
}

static uint32_t uca_pf_start_recording(struct uca_camera *cam)
{
    return cam->grabber->acquire(cam->grabber, -1);
}

static uint32_t uca_pf_stop_recording(struct uca_camera *cam)
{
    return cam->grabber->stop_acquire(cam->grabber);
}

static uint32_t uca_pf_trigger(struct uca_camera *cam)
{
    return cam->grabber->trigger(cam->grabber);
}

static uint32_t uca_pf_grab(struct uca_camera *cam, char *buffer, void *metadata)
{
    uint16_t *frame;
    uint32_t err = cam->grabber->grab(cam->grabber, (void **) &frame, &cam->current_frame);
    if (err != UCA_NO_ERROR)
        return err;

    memcpy(buffer, frame, cam->frame_width*cam->frame_height);
    return UCA_NO_ERROR;
}

static uint32_t uca_pf_register_callback(struct uca_camera *cam, uca_cam_grab_callback callback, void *user)
{
    if (cam->callback == NULL) {
        cam->callback = callback;
        cam->callback_user = user;
        return cam->grabber->register_callback(cam->grabber, callback, NULL, user);
    }
    return UCA_ERR_CAMERA | UCA_ERR_CALLBACK | UCA_ERR_ALREADY_REGISTERED;
}

static uint32_t uca_pf_destroy(struct uca_camera *cam)
{
    pfDeviceClose(0);
    return UCA_NO_ERROR;
}

uint32_t uca_pf_init(struct uca_camera **cam, struct uca_grabber *grabber)
{
    int num_ports;
    if ((grabber == NULL) || (pfPortInit(&num_ports) < 0) || (pfDeviceOpen(0) < 0))
        return UCA_ERR_CAMERA | UCA_ERR_INIT | UCA_ERR_NOT_FOUND;

    /* We could check if a higher baud rate is supported, but... forget about
     * it. We don't need high speed configuration. */

    struct uca_camera *uca = uca_cam_new();
    uca->grabber = grabber;
    uca->grabber->synchronous = false;

    /* Camera found, set function pointers... */
    uca->destroy = &uca_pf_destroy;
    uca->set_property = &uca_pf_set_property;
    uca->get_property = &uca_pf_get_property;
    uca->start_recording = &uca_pf_start_recording;
    uca->stop_recording = &uca_pf_stop_recording;
    uca->trigger = &uca_pf_trigger;
    uca->grab = &uca_pf_grab;
    uca->register_callback = &uca_pf_register_callback;

    /* Prepare frame grabber for recording */
    int val = UCA_CL_8BIT_FULL_8;
    grabber->set_property(grabber, UCA_GRABBER_CAMERALINK_TYPE, &val);

    val = UCA_FORMAT_GRAY8;
    grabber->set_property(grabber, UCA_GRABBER_FORMAT, &val);

    val = UCA_TRIGGER_AUTO;
    grabber->set_property(grabber, UCA_GRABBER_TRIGGER_MODE, &val);

    uca_pf_get_property(uca, UCA_PROP_WIDTH, &uca->frame_width, 0);
    uca_pf_get_property(uca, UCA_PROP_HEIGHT, &uca->frame_height, 0);

    grabber->set_property(grabber, UCA_PROP_WIDTH, &uca->frame_width);
    grabber->set_property(grabber, UCA_PROP_HEIGHT, &uca->frame_height);

    uca->state = UCA_CAM_CONFIGURABLE;
    *cam = uca;

    return UCA_NO_ERROR;
}
