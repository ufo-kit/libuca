
#include <stdlib.h>
#include <string.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"

/* TODO: REMOVE THIS ASAP */
#include <fgrab_struct.h>

#define set_void(p, type, value) { *((type *) p) = value; }


static uint32_t uca_pf_set_bitdepth(struct uca_camera_t *cam, uint8_t *bitdepth)
{
    /* TODO: it's not possible via CameraLink so do it via frame grabber */
    return 0;
}

static uint32_t uca_pf_acquire_image(struct uca_camera_t *cam, void *buffer)
{
    return UCA_NO_ERROR;
}

static uint32_t uca_pf_destroy(struct uca_camera_t *cam)
{
    return UCA_NO_ERROR;
}

static uint32_t uca_pf_set_property(struct uca_camera_t *cam, enum uca_property_ids property, void *data)
{
    struct uca_grabber_t *grabber = cam->grabber;

    switch (property) {
        case UCA_PROP_WIDTH:
            if (grabber->set_property(grabber, FG_WIDTH, (uint32_t *) data) != UCA_NO_ERROR)
                return UCA_ERR_PROP_VALUE_OUT_OF_RANGE;
            cam->frame_width = *((uint32_t *) data);
            break;

        case UCA_PROP_HEIGHT:
            if (grabber->set_property(grabber, FG_HEIGHT, (uint32_t *) data) != UCA_NO_ERROR)
                return UCA_ERR_PROP_VALUE_OUT_OF_RANGE;
            cam->frame_height = *((uint32_t *) data);
            break;

        case UCA_PROP_X_OFFSET:
            if (grabber->set_property(grabber, FG_XOFFSET, (uint32_t *) data) != UCA_NO_ERROR)
                return UCA_ERR_PROP_VALUE_OUT_OF_RANGE;
            break;

        case UCA_PROP_Y_OFFSET:
            if (grabber->set_property(grabber, FG_YOFFSET, (uint32_t *) data) != UCA_NO_ERROR)
                return UCA_ERR_PROP_VALUE_OUT_OF_RANGE;
            break;

        case UCA_PROP_EXPOSURE:
            if (grabber->set_property(grabber, FG_EXPOSURE, (uint32_t *) data) != UCA_NO_ERROR)
                return UCA_ERR_PROP_VALUE_OUT_OF_RANGE;
            break;

        default:
            return UCA_ERR_PROP_INVALID;
    }
    return UCA_NO_ERROR;
}


static uint32_t uca_pf_get_property(struct uca_camera_t *cam, enum uca_property_ids property, void *data)
{
    struct uca_grabber_t *grabber = cam->grabber;

    switch (property) {
        case UCA_PROP_NAME: 
            /* FIXME: read name from camera */
            strcpy((char *) data, "Photonfocus MV2-D1280-640");
            break;

        case UCA_PROP_WIDTH:
            set_void(data, uint32_t, cam->frame_width);
            break;

        case UCA_PROP_WIDTH_MIN:
            set_void(data, uint32_t, 1);
            break;

        case UCA_PROP_HEIGHT:
            set_void(data, uint32_t, cam->frame_height);
            break;

        case UCA_PROP_HEIGHT_MIN:
            set_void(data, uint32_t, 1);
            break;

        case UCA_PROP_X_OFFSET:
            if (grabber->get_property(grabber, FG_XOFFSET, (uint32_t *) data) != UCA_NO_ERROR)
                return UCA_ERR_PROP_GENERAL;
            break;

        case UCA_PROP_Y_OFFSET:
            if (grabber->get_property(grabber, FG_YOFFSET, (uint32_t *) data) != UCA_NO_ERROR)
                return UCA_ERR_PROP_GENERAL;
            break;

        case UCA_PROP_BITDEPTH:
            set_void(data, uint8_t, 8);
            break;

        default:
            return UCA_ERR_PROP_INVALID;
    }
    return UCA_NO_ERROR;
}

uint32_t uca_pf_start_recording(struct uca_camera_t *cam)
{
    return cam->grabber->acquire(cam->grabber, -1);
}

uint32_t uca_pf_stop_recording(struct uca_camera_t *cam)
{
    return UCA_NO_ERROR;
}

uint32_t uca_pf_grab(struct uca_camera_t *cam, char *buffer)
{
    uint16_t *frame;
    uint32_t err = cam->grabber->grab(cam->grabber, (void **) &frame);
    if (err != UCA_NO_ERROR)
        return err;
    /* FIXME: choose according to data format */
    memcpy(buffer, frame, cam->frame_width*cam->frame_height);
    return UCA_NO_ERROR;
}

uint32_t uca_pf_init(struct uca_camera_t **cam, struct uca_grabber_t *grabber)
{
    struct uca_camera_t *uca = (struct uca_camera_t *) malloc(sizeof(struct uca_camera_t));
    uca->grabber = grabber;
    uca->grabber->asynchronous = false;

    /* Camera found, set function pointers... */
    uca->destroy = &uca_pf_destroy;
    uca->set_property = &uca_pf_set_property;
    uca->get_property = &uca_pf_get_property;
    uca->start_recording = &uca_pf_start_recording;
    uca->stop_recording = &uca_pf_stop_recording;
    uca->grab = &uca_pf_grab;

    /* Prepare frame grabber for recording */
    int val = FG_CL_8BIT_FULL_8;
    grabber->set_property(grabber, FG_CAMERA_LINK_CAMTYP, &val);

    val = FG_GRAY;
    grabber->set_property(grabber, FG_FORMAT, &val);

    val = FREE_RUN;
    grabber->set_property(grabber, FG_TRIGGERMODE, &val);

    uca->frame_width = 1280;
    uca->frame_height = 1024;

    grabber->set_property(grabber, FG_WIDTH, &uca->frame_width);
    grabber->set_property(grabber, FG_HEIGHT, &uca->frame_height);

    uca->state = UCA_CAM_CONFIGURABLE;
    *cam = uca;

    return UCA_NO_ERROR;
}
