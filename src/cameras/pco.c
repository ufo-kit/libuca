
#include <stdlib.h>
#include <string.h>
#include <libpco/libpco.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"
#include "pco.h"

/* TODO: REMOVE THIS ASAP */
#include <fgrab_struct.h>

#define GET_PCO(uca) ((struct pco_edge_t *)(uca->user))

#define set_void(p, type, value) { *((type *) p) = value; }


static uint32_t uca_pco_set_bitdepth(struct uca_camera_t *cam, uint8_t *bitdepth)
{
    /* TODO: it's not possible via CameraLink so do it via frame grabber */
    return 0;
}

static uint32_t uca_pco_set_exposure(struct uca_camera_t *cam, uint32_t *exposure)
{
    uint32_t e, d;
    if (pco_get_delay_exposure(GET_PCO(cam), &d, &e) != PCO_NOERROR)
        return UCA_ERR_PROP_GENERAL;
    if (pco_set_delay_exposure(GET_PCO(cam), d, *exposure) != PCO_NOERROR)
        return UCA_ERR_PROP_GENERAL;
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_set_delay(struct uca_camera_t *cam, uint32_t *delay)
{
    uint32_t e, d;
    if (pco_get_delay_exposure(GET_PCO(cam), &d, &e) != PCO_NOERROR)
        return UCA_ERR_PROP_GENERAL;
    if (pco_set_delay_exposure(GET_PCO(cam), *delay, e) != PCO_NOERROR)
        return UCA_ERR_PROP_GENERAL;
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_acquire_image(struct uca_camera_t *cam, void *buffer)
{
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_destroy(struct uca_camera_t *cam)
{
    pco_destroy(GET_PCO(cam));
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_set_property(struct uca_camera_t *cam, enum uca_property_ids property, void *data)
{
    struct uca_grabber_t *grabber = cam->grabber;

    switch (property) {
        case UCA_PROP_WIDTH:
            if (grabber->set_property(grabber, FG_WIDTH, (uint32_t *) data) != UCA_NO_ERROR)
                return UCA_ERR_PROP_VALUE_OUT_OF_RANGE;
            break;

        case UCA_PROP_HEIGHT:
            if (grabber->set_property(grabber, FG_HEIGHT, (uint32_t *) data) != UCA_NO_ERROR)
                return UCA_ERR_PROP_VALUE_OUT_OF_RANGE;
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
            return uca_pco_set_exposure(cam, (uint32_t *) data);

        case UCA_PROP_DELAY:
            return uca_pco_set_delay(cam, (uint32_t *) data);

        default:
            return UCA_ERR_PROP_INVALID;
    }
    return UCA_NO_ERROR;
}


static uint32_t uca_pco_get_property(struct uca_camera_t *cam, enum uca_property_ids property, void *data)
{
    struct pco_edge_t *pco = GET_PCO(cam);
    struct uca_grabber_t *grabber = cam->grabber;

    switch (property) {
        case UCA_PROP_NAME: 
            {
                /* FIXME: how to ensure, that buffer is large enough? */
                SC2_Camera_Name_Response name;
                if (pco_read_property(pco, GET_CAMERA_NAME, &name, sizeof(name)) == PCO_NOERROR)
                    strcpy((char *) data, name.szName);
            }
            break;

        case UCA_PROP_WIDTH:
            {
                int w, h;
                if (pco_get_actual_size(pco, &w, &h) == PCO_NOERROR)
                    set_void(data, uint32_t, w);
            }
            break;

        case UCA_PROP_WIDTH_MIN:
            set_void(data, uint32_t, 1);
            break;

        case UCA_PROP_WIDTH_MAX:
            set_void(data, uint32_t, pco->description.wMaxHorzResStdDESC);
            break;

        case UCA_PROP_HEIGHT:
            {
                int w, h;
                if (pco_get_actual_size(pco, &w, &h) == PCO_NOERROR)
                    set_void(data, uint32_t, h);
            }
            break;

        case UCA_PROP_HEIGHT_MIN:
            set_void(data, uint32_t, 1);
            break;

        case UCA_PROP_HEIGHT_MAX:
            set_void(data, uint32_t, pco->description.wMaxVertResStdDESC);
            break;

        case UCA_PROP_X_OFFSET:
            if (grabber->get_property(grabber, FG_XOFFSET, (uint32_t *) data) != UCA_NO_ERROR)
                return UCA_ERR_PROP_GENERAL;
            break;

        case UCA_PROP_Y_OFFSET:
            if (grabber->get_property(grabber, FG_YOFFSET, (uint32_t *) data) != UCA_NO_ERROR)
                return UCA_ERR_PROP_GENERAL;
            break;

        case UCA_PROP_DELAY_MIN:
            set_void(data, uint32_t, pco->description.dwMinDelayDESC);
            break;

        case UCA_PROP_DELAY_MAX:
            set_void(data, uint32_t, pco->description.dwMaxDelayDESC);
            break;

        case UCA_PROP_EXPOSURE_MIN:
            set_void(data, uint32_t, pco->description.dwMinExposureDESC);
            break;

        case UCA_PROP_EXPOSURE_MAX:
            set_void(data, uint32_t, pco->description.dwMaxExposureDESC);
            break;

        case UCA_PROP_BITDEPTH:
            set_void(data, uint8_t, 16);
            break;

        default:
            return UCA_ERR_PROP_INVALID;
    }
    return UCA_NO_ERROR;
}

uint32_t uca_pco_alloc(struct uca_camera_t *cam, uint32_t n_buffers)
{

}

uint32_t uca_pco_init(struct uca_camera_t **cam, struct uca_grabber_t *grabber)
{
    struct pco_edge_t *pco = pco_init();

    if (pco == NULL) {
        return UCA_ERR_INIT_NOT_FOUND;
    }

    if ((pco->serial_ref == NULL) || !pco_active(pco)) {
        pco_destroy(pco);
        return UCA_ERR_INIT_NOT_FOUND;
    }

    struct uca_camera_t *uca = (struct uca_camera_t *) malloc(sizeof(struct uca_camera_t));
    uca->user = pco;
    uca->grabber = grabber;

    /* Camera found, set function pointers... */
    uca->destroy = &uca_pco_destroy;
    uca->set_property = &uca_pco_set_property;
    uca->get_property = &uca_pco_get_property;
    uca->alloc = &uca_pco_alloc;
    uca->acquire_image = &uca_pco_acquire_image;

    /* Prepare camera for recording */
    pco_set_rec_state(pco, 0);
    pco_set_timestamp_mode(pco, 2);
    pco_set_timebase(pco, 1, 1); 
    pco_arm_camera(pco);

    if (pco->transfer.DataFormat != (SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER | PCO_CL_DATAFORMAT_5x16))
        pco->transfer.DataFormat = SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER | PCO_CL_DATAFORMAT_5x16;

    /* Prepare frame grabber for recording */
    int val = FG_CL_8BIT_FULL_10;
    grabber->set_property(grabber, FG_CAMERA_LINK_CAMTYP, &val);

    val = FG_GRAY;
    grabber->set_property(grabber, FG_FORMAT, &val);

    val = FREE_RUN;
    grabber->set_property(grabber, FG_TRIGGERMODE, &val);

    int width, height;
    pco_get_actual_size(pco, &width, &height);

    /* Yes, we really have to take an image twice as large because we set the
     * CameraLink interface to 8-bit 10 Taps, but are actually using 5x16 bits. */
    width *= 2;
    grabber->set_property(grabber, FG_WIDTH, &width);
    grabber->set_property(grabber, FG_HEIGHT, &height);

    uca->state = UCA_CAM_CONFIGURABLE;
    *cam = uca;

    return UCA_NO_ERROR;
}
