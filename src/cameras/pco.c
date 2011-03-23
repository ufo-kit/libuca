
#include <stdlib.h>
#include <string.h>
#include <libpco/libpco.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"
#include "pco.h"

#define GET_PCO(uca) ((struct pco_edge *)(uca->user))

#define set_void(p, type, value) { *((type *) p) = (type) value; }


static uint32_t uca_pco_set_exposure(struct uca_camera *cam, uint32_t *exposure)
{
    uint32_t err = UCA_ERR_CAMERA | UCA_ERR_PROP;
    uint32_t e, d;
    if (pco_get_delay_exposure(GET_PCO(cam), &d, &e) != PCO_NOERROR)
        return err | UCA_ERR_INVALID;
    if (pco_set_delay_exposure(GET_PCO(cam), d, *exposure) != PCO_NOERROR)
        return err | UCA_ERR_INVALID;
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_set_delay(struct uca_camera *cam, uint32_t *delay)
{
    uint32_t err = UCA_ERR_CAMERA | UCA_ERR_PROP;
    uint32_t e, d;
    if (pco_get_delay_exposure(GET_PCO(cam), &d, &e) != PCO_NOERROR)
        return err | UCA_ERR_INVALID;
    if (pco_set_delay_exposure(GET_PCO(cam), *delay, e) != PCO_NOERROR)
        return err | UCA_ERR_INVALID;
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_destroy(struct uca_camera *cam)
{
    pco_set_rec_state(GET_PCO(cam), 0);
    pco_destroy(GET_PCO(cam));
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_set_property(struct uca_camera *cam, enum uca_property_ids property, void *data)
{
    struct uca_grabber *grabber = cam->grabber;
    uint32_t err = UCA_ERR_CAMERA | UCA_ERR_PROP;

    switch (property) {
        case UCA_PROP_WIDTH:
            if (grabber->set_property(grabber, UCA_GRABBER_WIDTH, (uint32_t *) data) != UCA_NO_ERROR)
                return err | UCA_ERR_OUT_OF_RANGE;
            cam->frame_width = *((uint32_t *) data);
            break;

        case UCA_PROP_HEIGHT:
            if (grabber->set_property(grabber, UCA_GRABBER_HEIGHT, (uint32_t *) data) != UCA_NO_ERROR)
                return err | UCA_ERR_OUT_OF_RANGE;
            cam->frame_height = *((uint32_t *) data);
            break;

        case UCA_PROP_X_OFFSET:
            if (grabber->set_property(grabber, UCA_GRABBER_OFFSET_X, (uint32_t *) data) != UCA_NO_ERROR)
                return err | UCA_ERR_OUT_OF_RANGE;
            break;

        case UCA_PROP_Y_OFFSET:
            if (grabber->set_property(grabber, UCA_GRABBER_OFFSET_Y, (uint32_t *) data) != UCA_NO_ERROR)
                return err | UCA_ERR_OUT_OF_RANGE;
            break;

        case UCA_PROP_EXPOSURE:
            return uca_pco_set_exposure(cam, (uint32_t *) data);

        case UCA_PROP_DELAY:
            return uca_pco_set_delay(cam, (uint32_t *) data);

        case UCA_PROP_TIMESTAMP_MODE:
            return pco_set_timestamp_mode(GET_PCO(cam), *((uint16_t *) data));

        case UCA_PROP_GRAB_TIMEOUT:
            if (grabber->set_property(grabber, UCA_GRABBER_TIMEOUT, data) != UCA_NO_ERROR)
                return err | UCA_ERR_OUT_OF_RANGE;
            break;

        default:
            return err | UCA_ERR_INVALID;
    }
    return UCA_NO_ERROR;
}


static uint32_t uca_pco_get_property(struct uca_camera *cam, enum uca_property_ids property, void *data, size_t num)
{
    struct pco_edge *pco = GET_PCO(cam);
    struct uca_grabber *grabber = cam->grabber;

    switch (property) {
        case UCA_PROP_NAME: 
            {
                SC2_Camera_Name_Response name;

                /* FIXME: This is _not_ a mistake. For some reason (which I
                 * still have to figure out), it is sometimes not possible to
                 * read the camera name... unless the same call precedes that
                 * one.*/
                pco_read_property(pco, GET_CAMERA_NAME, &name, sizeof(name));
                pco_read_property(pco, GET_CAMERA_NAME, &name, sizeof(name));
                strncpy((char *) data, name.szName, num);
            }
            break;

        case UCA_PROP_TEMPERATURE_SENSOR:
            {
                SC2_Temperature_Response temperature;
                if (pco_read_property(pco, GET_TEMPERATURE, &temperature, sizeof(temperature)) == PCO_NOERROR)
                    set_void(data, uint32_t, temperature.sCCDtemp / 10);
            }
            break;

        case UCA_PROP_TEMPERATURE_CAMERA:
            {
                SC2_Temperature_Response temperature;
                if (pco_read_property(pco, GET_TEMPERATURE, &temperature, sizeof(temperature)) == PCO_NOERROR)
                    set_void(data, uint32_t, temperature.sCamtemp);
            }
            break;

        case UCA_PROP_WIDTH:
            set_void(data, uint32_t, cam->frame_width);
            break;

        case UCA_PROP_WIDTH_MIN:
            set_void(data, uint32_t, 1);
            break;

        case UCA_PROP_WIDTH_MAX:
            set_void(data, uint32_t, pco->description.wMaxHorzResStdDESC);
            break;

        case UCA_PROP_HEIGHT:
            set_void(data, uint32_t, cam->frame_height);
            break;

        case UCA_PROP_HEIGHT_MIN:
            set_void(data, uint32_t, 1);
            break;

        case UCA_PROP_HEIGHT_MAX:
            set_void(data, uint32_t, pco->description.wMaxVertResStdDESC);
            break;

        case UCA_PROP_X_OFFSET:
            return grabber->get_property(grabber, UCA_GRABBER_OFFSET_X, (uint32_t *) data);

        case UCA_PROP_Y_OFFSET:
            return grabber->get_property(grabber, UCA_GRABBER_OFFSET_Y, (uint32_t *) data);

        case UCA_PROP_DELAY:
            {
                uint32_t exposure;
                return pco_get_delay_exposure(pco, (uint32_t *) data, &exposure);
            }
            break;

        case UCA_PROP_DELAY_MIN:
            set_void(data, uint32_t, pco->description.dwMinDelayDESC);
            break;

        case UCA_PROP_DELAY_MAX:
            set_void(data, uint32_t, pco->description.dwMaxDelayDESC);
            break;

        case UCA_PROP_EXPOSURE:
            {
                uint32_t delay;
                return pco_get_delay_exposure(pco, &delay, (uint32_t *) data);
            }
            break;

        case UCA_PROP_EXPOSURE_MIN:
            set_void(data, uint32_t, pco->description.dwMinExposureDESC);
            break;

        case UCA_PROP_EXPOSURE_MAX:
            set_void(data, uint32_t, pco->description.dwMaxExposureDESC);
            break;

        case UCA_PROP_BITDEPTH:
            set_void(data, uint32_t, 16);
            break;

        case UCA_PROP_GRAB_TIMEOUT:
            {
                uint32_t timeout;
                uint32_t err = cam->grabber->get_property(cam->grabber, UCA_GRABBER_TIMEOUT, &timeout);
                if (err != UCA_NO_ERROR)
                    return err;
                set_void(data, uint32_t, timeout);
            }
            break;

        default:
            return UCA_ERR_CAMERA | UCA_ERR_PROP | UCA_ERR_INVALID;
    }
    return UCA_NO_ERROR;
}

uint32_t uca_pco_start_recording(struct uca_camera *cam)
{
    uint32_t err = UCA_ERR_CAMERA | UCA_ERR_INIT;
    struct pco_edge *pco = GET_PCO(cam);
    if (pco_arm_camera(pco) != PCO_NOERROR)
        return err | UCA_ERR_UNCLASSIFIED;
    if (pco_set_rec_state(pco, 1) != PCO_NOERROR)
        return err | UCA_ERR_UNCLASSIFIED;
    return cam->grabber->acquire(cam->grabber, -1);
}

uint32_t uca_pco_stop_recording(struct uca_camera *cam)
{
    if (pco_set_rec_state(GET_PCO(cam), 0) != PCO_NOERROR)
        return UCA_ERR_CAMERA | UCA_ERR_INIT | UCA_ERR_UNCLASSIFIED;
    return UCA_NO_ERROR;
}

uint32_t uca_pco_grab(struct uca_camera *cam, char *buffer, void *meta_data)
{
    uint16_t *frame;
    uint32_t err = cam->grabber->grab(cam->grabber, (void **) &frame, &cam->current_frame);
    if (err != UCA_NO_ERROR)
        return err;

    GET_PCO(cam)->reorder_image((uint16_t *) buffer, frame, cam->frame_width, cam->frame_height);
    return UCA_NO_ERROR;
}

uint32_t uca_pco_register_callback(struct uca_camera *cam, uca_cam_grab_callback callback, void *user)
{
    if (cam->callback == NULL) {
        cam->callback = callback;
        cam->callback_user = user;
        return cam->grabber->register_callback(cam->grabber, callback, NULL, user);
    }
    return UCA_ERR_CAMERA | UCA_ERR_CALLBACK | UCA_ERR_ALREADY_REGISTERED;
}

uint32_t uca_pco_init(struct uca_camera **cam, struct uca_grabber *grabber)
{
    uint32_t err = UCA_ERR_CAMERA | UCA_ERR_INIT;
    if (grabber == NULL)
        return err | UCA_ERR_NOT_FOUND;

    struct pco_edge *pco = pco_init();
    if (pco == NULL)
        return err | UCA_ERR_NOT_FOUND;

    if ((pco->serial_ref == NULL) || !pco_is_active(pco)) {
        pco_destroy(pco);
        return err | UCA_ERR_NOT_FOUND;
    }

    struct uca_camera *uca = uca_cam_new();
    uca->user = pco;
    uca->grabber = grabber;
    uca->grabber->asynchronous = true;

    /* Camera found, set function pointers... */
    uca->destroy = &uca_pco_destroy;
    uca->set_property = &uca_pco_set_property;
    uca->get_property = &uca_pco_get_property;
    uca->start_recording = &uca_pco_start_recording;
    uca->stop_recording = &uca_pco_stop_recording;
    uca->grab = &uca_pco_grab;

    /* Prepare camera for recording */
    pco_set_scan_mode(pco, PCO_SCANMODE_SLOW);
    pco_set_rec_state(pco, 0);
    pco_set_timestamp_mode(pco, 2);
    pco_set_timebase(pco, 1, 1); 
    pco_arm_camera(pco);

    /* Prepare frame grabber for recording */
    int val = UCA_CL_8BIT_FULL_10;
    grabber->set_property(grabber, UCA_GRABBER_CAMERALINK_TYPE, &val);

    val = UCA_FORMAT_GRAY8;
    grabber->set_property(grabber, UCA_GRABBER_FORMAT, &val);

    val = UCA_TRIGGER_FREERUN;
    grabber->set_property(grabber, UCA_GRABBER_TRIGGER_MODE, &val);

    uint32_t width, height;
    pco_get_actual_size(pco, &width, &height);
    uca->frame_width = width;
    uca->frame_height = height;

    /* Yes, we really have to take an image twice as large because we set the
     * CameraLink interface to 8-bit 10 Taps, but are actually using 5x16 bits. */
    width *= 2;
    grabber->set_property(grabber, UCA_GRABBER_WIDTH, &width);
    grabber->set_property(grabber, UCA_GRABBER_HEIGHT, &height);

    uca->state = UCA_CAM_CONFIGURABLE;
    *cam = uca;

    return UCA_NO_ERROR;
}
