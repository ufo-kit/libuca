
#include <stdlib.h>
#include <string.h>
#include <libpco/libpco.h>
#include <libpco/sc2_defs.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"
#include "pco.h"

typedef struct pco_desc {
    pco_handle pco;
    uint16_t type, subtype;
    uint16_t roi[4];
    
    uint16_t active_segment;
    uint32_t current_image;
    uint32_t num_recorded_images;
} pco_desc_t;

#define GET_PCO_DESC(cam) ((struct pco_desc *) cam->user)
#define GET_PCO(cam) (((struct pco_desc *)(cam->user))->pco)

#define uca_set_void(p, type, value) { *((type *) p) = (type) value; }


static uint32_t uca_pco_set_exposure(struct uca_camera_priv *cam, uint32_t *exposure)
{
    uint32_t err = UCA_ERR_CAMERA | UCA_ERR_PROP;
    uint32_t e, d;
    if (pco_get_delay_exposure(GET_PCO(cam), &d, &e) != PCO_NOERROR)
        return err | UCA_ERR_INVALID;
    if (pco_set_delay_exposure(GET_PCO(cam), d, *exposure) != PCO_NOERROR)
        return err | UCA_ERR_INVALID;
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_set_delay(struct uca_camera_priv *cam, uint32_t *delay)
{
    uint32_t err = UCA_ERR_CAMERA | UCA_ERR_PROP;
    uint32_t e, d;
    if (pco_get_delay_exposure(GET_PCO(cam), &d, &e) != PCO_NOERROR)
        return err | UCA_ERR_INVALID;
    if (pco_set_delay_exposure(GET_PCO(cam), *delay, e) != PCO_NOERROR)
        return err | UCA_ERR_INVALID;
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_destroy(struct uca_camera_priv *cam)
{
    cam->grabber->stop_acquire(cam->grabber);
    pco_set_rec_state(GET_PCO(cam), 0);
    pco_destroy(GET_PCO(cam));
    free(GET_PCO_DESC(cam));
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_set_property(struct uca_camera_priv *cam, enum uca_property_ids property, void *data)
{
    struct uca_grabber_priv *grabber = cam->grabber;
    struct pco_desc *pco_d = GET_PCO_DESC(cam);
    uint32_t err = UCA_ERR_CAMERA | UCA_ERR_PROP;

    /* We try to set the property on the grabber. If it returns "invalid", we
     * also try it via the libpco. Else, there was a more serious error. */
    err = grabber->set_property(grabber, property, data);
    if (((err & UCA_ERR_MASK_CODE) == UCA_ERR_INVALID) || (err == UCA_NO_ERROR))
        err = UCA_ERR_CAMERA | UCA_ERR_PROP;
    else
        return err;

    switch (property) {
        case UCA_PROP_WIDTH:
            cam->frame_width = *((uint32_t *) data);
            pco_d->roi[2] = cam->frame_width;
            if (pco_set_roi(pco_d->pco, pco_d->roi) != PCO_NOERROR)
                return err | UCA_ERR_OUT_OF_RANGE;

            /* Twice the width because of 16 bits per pixel */
            uint32_t w = cam->frame_width * 2;
            grabber->set_property(grabber, UCA_PROP_WIDTH, &w);
            break;

        case UCA_PROP_HEIGHT:
            cam->frame_height = *((uint32_t *) data);
            pco_d->roi[3] = cam->frame_height;
            if (pco_set_roi(pco_d->pco, pco_d->roi) == PCO_NOERROR)
                return err | UCA_ERR_OUT_OF_RANGE;
            break;

        case UCA_PROP_EXPOSURE:
            return uca_pco_set_exposure(cam, (uint32_t *) data);

        case UCA_PROP_DELAY:
            return uca_pco_set_delay(cam, (uint32_t *) data);

        case UCA_PROP_GRAB_AUTO:
            return pco_set_auto_transfer(GET_PCO(cam), *(uint32_t *) data);

        case UCA_PROP_TRIGGER_MODE:
            /* XXX: We have a 1:1 mapping between UCA_TRIGGER_* and
             * TRIGGER_MODE_*
             */
            return pco_set_trigger_mode(GET_PCO(cam), (uint16_t) *(uint32_t *) data);

        case UCA_PROP_TIMESTAMP_MODE:
            {
                uint32_t mode = *((uint32_t *) data);
                if (mode & UCA_TIMESTAMP_ASCII) {
                    if (mode & UCA_TIMESTAMP_BINARY)
                        return pco_set_timestamp_mode(GET_PCO(cam), TIMESTAMP_MODE_BINARYANDASCII);
                    else
                        return pco_set_timestamp_mode(GET_PCO(cam), TIMESTAMP_MODE_ASCII);
                } 
                else if (mode & UCA_TIMESTAMP_BINARY)
                    return pco_set_timestamp_mode(GET_PCO(cam), TIMESTAMP_MODE_BINARY);
                else
                    return pco_set_timestamp_mode(GET_PCO(cam), TIMESTAMP_MODE_OFF);
            }

        case UCA_PROP_HOTPIXEL_CORRECTION:
            return pco_set_hotpixel_correction(GET_PCO(cam), *(uint32_t *) data);

        default:
            return err | UCA_ERR_INVALID;
    }
    return UCA_NO_ERROR;
}


static uint32_t uca_pco_get_property(struct uca_camera_priv *cam, enum uca_property_ids property, void *data, size_t num)
{
    pco_handle pco = GET_PCO(cam);
    struct uca_grabber_priv *grabber = cam->grabber;

    switch (property) {
        case UCA_PROP_NAME: 
            {
                /* FIXME: This is _not_ a mistake. For some reason (which I
                 * still have to figure out), it is sometimes not possible to
                 * read the camera name... unless the same call precedes that
                 * one.*/
                char *name = NULL;
                pco_get_name(pco, &name);
                strncpy((char *) data, name, num);
                free(name);
            }
            break;

        case UCA_PROP_TEMPERATURE_SENSOR:
            {
                uint32_t t1, t2, t3;
                if (pco_get_temperature(pco, &t1, &t2, &t3) == PCO_NOERROR)
                    uca_set_void(data, uint32_t, t1 / 10);
            }
            break;

        case UCA_PROP_TEMPERATURE_CAMERA:
            {
                uint32_t t1, t2, t3;
                if (pco_get_temperature(pco, &t1, &t2, &t3) == PCO_NOERROR)
                    uca_set_void(data, uint32_t, t2);
            }
            break;

        case UCA_PROP_WIDTH:
            uca_set_void(data, uint32_t, cam->frame_width);
            break;

        case UCA_PROP_WIDTH_MIN:
            uca_set_void(data, uint32_t, 1);
            break;

        case UCA_PROP_HEIGHT:
            uca_set_void(data, uint32_t, cam->frame_height);
            break;

        case UCA_PROP_HEIGHT_MIN:
            uca_set_void(data, uint32_t, 1);
            break;

        case UCA_PROP_X_OFFSET:
            return grabber->get_property(grabber, UCA_PROP_X_OFFSET, (uint32_t *) data);

        case UCA_PROP_Y_OFFSET:
            return grabber->get_property(grabber, UCA_PROP_Y_OFFSET, (uint32_t *) data);

        case UCA_PROP_DELAY:
            {
                uint32_t exposure;
                pco_get_delay_exposure(pco, (uint32_t *) data, &exposure);
            }
            break;

        case UCA_PROP_DELAY_MIN:
            {
                uint32_t delay = 12341234;
                uca_set_void(data, uint32_t, delay);
            }
            break;

        case UCA_PROP_DELAY_MAX:
            {
                uint32_t delay = 12341234;
                uca_set_void(data, uint32_t, delay);
            }
            break;

        case UCA_PROP_EXPOSURE:
            {
                uint32_t delay;
                pco_get_delay_exposure(pco, &delay, (uint32_t *) data);
            }
            break;

        case UCA_PROP_EXPOSURE_MIN:
            {
                uint32_t exposure = 12341234;
                uca_set_void(data, uint32_t, exposure);
            }
            break;

        case UCA_PROP_EXPOSURE_MAX:
            {
                uint32_t exposure = 12341234;
                uca_set_void(data, uint32_t, exposure);
            }
            break;

        case UCA_PROP_BITDEPTH:
            uca_set_void(data, uint32_t, 16);
            break;

        case UCA_PROP_GRAB_AUTO:
            {
                int value = 0;
                uint32_t err = pco_get_auto_transfer(pco, &value);
                if (err != PCO_NOERROR)
                    return UCA_ERR_CAMERA | UCA_ERR_PROP | UCA_ERR_INVALID;
                uca_set_void(data, uint32_t, value);
            }
            break;

        case UCA_PROP_GRAB_TIMEOUT:
            {
                uint32_t timeout;
                uint32_t err = cam->grabber->get_property(cam->grabber, UCA_PROP_GRAB_TIMEOUT, &timeout);
                if (err != UCA_NO_ERROR)
                    return err;
                uca_set_void(data, uint32_t, timeout);
            }
            break;

        default:
            return UCA_ERR_CAMERA | UCA_ERR_PROP | UCA_ERR_INVALID;
    }
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_start_recording(struct uca_camera_priv *cam)
{
    uint32_t err = UCA_ERR_CAMERA | UCA_ERR_INIT;
    pco_handle pco = GET_PCO(cam);

    if (GET_PCO_DESC(cam)->type == CAMERATYPE_PCO_DIMAX_STD)
        pco_clear_active_segment(pco);

    if (pco_arm_camera(pco) != PCO_NOERROR)
        return err | UCA_ERR_UNCLASSIFIED;

    if (pco_set_rec_state(pco, 1) != PCO_NOERROR)
        return err | UCA_ERR_UNCLASSIFIED;

    return cam->grabber->acquire(cam->grabber, -1);
}

static uint32_t uca_pco_stop_recording(struct uca_camera_priv *cam)
{
    if (pco_set_rec_state(GET_PCO(cam), 0) != PCO_NOERROR)
        return UCA_ERR_CAMERA | UCA_ERR_INIT | UCA_ERR_UNCLASSIFIED;
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_trigger(struct uca_camera_priv *cam)
{
    /* TODO: is this correct? */
    uint32_t success = 0;
    pco_force_trigger(GET_PCO(cam), &success);
    return cam->grabber->trigger(cam->grabber);
}

static uint32_t uca_pco_grab(struct uca_camera_priv *cam, char *buffer, void *meta_data)
{
    uint16_t *frame;
    pco_desc_t *pco_d = GET_PCO_DESC(cam);
    pco_handle pco = pco_d->pco;

    if (cam->state == UCA_CAM_READOUT && pco_d->type == CAMERATYPE_PCO_DIMAX_STD) {
        if (pco_d->current_image == pco_d->num_recorded_images)
            return UCA_ERR_NO_MORE_IMAGES;
        
        /* Ok, this is pco's way of requesting multiple frames... you have to do
         * it one by one :/ */
        pco_read_images(pco, pco_d->active_segment, pco_d->current_image, pco_d->current_image);
        pco_d->current_image++;
    }

    pco_request_image(GET_PCO(cam));
    uint32_t err = cam->grabber->grab(cam->grabber, (void **) &frame, &cam->current_frame);
    if (err != UCA_NO_ERROR)
        return err;

    /* Copy data into user buffer */
    if (pco_d->type == CAMERATYPE_PCO_EDGE)
        pco_get_reorder_func(GET_PCO(cam))((uint16_t *) buffer, frame, cam->frame_width, cam->frame_height);
    else
        memcpy(buffer, (char *) frame, cam->frame_width * cam->frame_height * 2);

    return UCA_NO_ERROR;
}

static uint32_t uca_pco_readout(struct uca_camera_priv *cam)
{
    pco_desc_t *pco_d = GET_PCO_DESC(cam);
    /* TODO: error handling */
    pco_handle pco = GET_PCO(cam);
    pco_get_active_segment(pco, &pco_d->active_segment);
    pco_get_num_images(pco, pco_d->active_segment, &pco_d->num_recorded_images);
    pco_d->current_image = 1;
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_register_callback(struct uca_camera_priv *cam, uca_cam_grab_callback callback, void *user)
{
    if (cam->callback == NULL) {
        cam->callback = callback;
        cam->callback_user = user;
        return cam->grabber->register_callback(cam->grabber, callback, NULL, user);
    }
    return UCA_ERR_CAMERA | UCA_ERR_CALLBACK | UCA_ERR_ALREADY_REGISTERED;
}

uint32_t uca_pco_init(struct uca_camera_priv **cam, struct uca_grabber_priv *grabber)
{
    uint32_t err = UCA_ERR_CAMERA | UCA_ERR_INIT;
    if (grabber == NULL)
        return err | UCA_ERR_NOT_FOUND;

    pco_handle pco = pco_init();
    if (pco == NULL)
        return err | UCA_ERR_NOT_FOUND;

    if (!pco_is_active(pco)) {
        pco_destroy(pco);
        return err | UCA_ERR_NOT_FOUND;
    }

    struct uca_camera_priv *uca = uca_cam_new();

    /* Prepare user data */
    struct pco_desc *pco_d = (struct pco_desc *) malloc(sizeof(struct pco_desc));
    uca->user = pco_d;
    pco_d->pco = pco;
    pco_get_camera_type(pco, &pco_d->type, &pco_d->subtype);

    uca->grabber = grabber;
    uca->grabber->synchronous = true;

    /* Camera found, set function pointers... */
    uca->destroy = &uca_pco_destroy;
    uca->set_property = &uca_pco_set_property;
    uca->get_property = &uca_pco_get_property;
    uca->start_recording = &uca_pco_start_recording;
    uca->stop_recording = &uca_pco_stop_recording;
    uca->trigger = &uca_pco_trigger;
    uca->grab = &uca_pco_grab;
    uca->readout = &uca_pco_readout;
    uca->register_callback = &uca_pco_register_callback;

    /* Prepare camera for recording */
    if (pco_d->type == CAMERATYPE_PCO_EDGE)
        pco_set_scan_mode(pco, PCO_SCANMODE_SLOW);

    pco_set_rec_state(pco, 0);
    pco_set_timestamp_mode(pco, TIMESTAMP_MODE_ASCII);
    pco_set_timebase(pco, 1, 1); 
    /* pco_arm_camera(pco); */

    /* Prepare frame grabber for recording */
    int val = 0;
    
    if (pco_d->type == CAMERATYPE_PCO_EDGE)
        val = UCA_CL_8BIT_FULL_10;
    else if (pco_d->type == CAMERATYPE_PCO_DIMAX_STD)
        val = UCA_CL_SINGLE_TAP_8;
    grabber->set_property(grabber, UCA_GRABBER_CAMERALINK_TYPE, &val);

    val = 0;
    if (pco_d->type == CAMERATYPE_PCO_EDGE)
        val = UCA_FORMAT_GRAY8;
    else if (pco_d->type == CAMERATYPE_PCO_DIMAX_STD)
        val = UCA_FORMAT_GRAY16;
    grabber->set_property(grabber, UCA_GRABBER_FORMAT, &val);

    val = UCA_TRIGGER_AUTO;
    grabber->set_property(grabber, UCA_GRABBER_TRIGGER_MODE, &val);

    uint32_t width, height;
    pco_get_actual_size(pco, &width, &height);
    uca->frame_width = width;
    uca->frame_height = height;
    pco_d->roi[0] = pco_d->roi[1] = 1;
    pco_d->roi[2] = width;
    pco_d->roi[3] = height;

    /* Yes, we really have to take an image twice as large because we set the
     * CameraLink interface to 8-bit 10 Taps, but are actually using 5x16 bits. */
    if (pco_d->type == CAMERATYPE_PCO_EDGE)
        width *= 2;
    grabber->set_property(grabber, UCA_PROP_WIDTH, &width);
    grabber->set_property(grabber, UCA_PROP_HEIGHT, &height);

    uca->state = UCA_CAM_CONFIGURABLE;
    *cam = uca;

    return UCA_NO_ERROR;
}
