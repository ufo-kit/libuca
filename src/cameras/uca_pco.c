
#include <stdlib.h>
#include <string.h>
#include <clser.h>
#include <fgrab_struct.h>
#include <fgrab_prototyp.h>
#include <libpco/libpco.h>
#include "uca.h"
#include "uca_pco.h"

struct pco_cam_t {
    struct pco_edge_t *pco;
    Fg_Struct *fg;
};

#define GET_PCO(uca) (((struct pco_cam_t *)(uca->user))->pco)
#define GET_FG(uca) (((struct pco_cam_t *)(uca->user))->fg)


static uint32_t uca_pco_set_bitdepth(struct uca_t *uca, uint8_t *bitdepth)
{
    /* TODO: it's not possible via CameraLink so do it via frame grabber */
    return 0;
}

static uint32_t uca_pco_set_exposure(struct uca_t *uca, uint32_t *exposure)
{
    uint32_t e, d;
    pco_get_delay_exposure(GET_PCO(uca), &d, &e);
    pco_set_delay_exposure(GET_PCO(uca), d, *exposure);
    return 0;
}

static uint32_t uca_pco_set_delay(struct uca_t *uca, uint32_t *delay)
{
    uint32_t e, d;
    pco_get_delay_exposure(GET_PCO(uca), &d, &e);
    pco_set_delay_exposure(GET_PCO(uca), *delay, e);
    return 0;
}

static uint32_t uca_pco_acquire_image(struct uca_t *uca, void *buffer)
{
    return 0;
}

static uint32_t uca_pco_destroy(struct uca_t *uca)
{
    Fg_FreeGrabber(GET_FG(uca));
    pco_destroy(GET_PCO(uca));
    free(uca->user);
}

static uint32_t uca_pco_set_property(struct uca_t *uca, int32_t property, void *data)
{
    switch (property) {
        case UCA_PROP_WIDTH:
            Fg_setParameter(GET_FG(uca), FG_WIDTH, (uint32_t *) data, PORT_A);
            break;

        case UCA_PROP_HEIGHT:
            Fg_setParameter(GET_FG(uca), FG_HEIGHT, (uint32_t *) data, PORT_A);
            break;

        case UCA_PROP_EXPOSURE:
            uca_pco_set_exposure(uca, (uint32_t *) data);
            break;

        case UCA_PROP_FRAMERATE:
            break;

        default:
            return UCA_ERR_PROP_INVALID;
    }
    return UCA_NO_ERROR;
}

static uint32_t uca_pco_get_property(struct uca_t *uca, int32_t property, void *data)
{
    struct pco_edge_t *pco = GET_PCO(uca);

    switch (property) {
        /* FIXME: how to ensure, that data is large enough? */
        case UCA_PROP_NAME: 
            {
                SC2_Camera_Name_Response name;
                if (pco_read_property(pco, GET_CAMERA_NAME, &name, sizeof(name)) == PCO_NOERROR)
                    strcpy((char *) data, name.szName);
            }
            break;

        case UCA_PROP_MAX_WIDTH:
            {
                uint32_t w, h;
                pco_get_actual_size(pco, &w, &h);
                *((uint32_t *) data) = w;
            }

        case UCA_PROP_MAX_HEIGHT:
            {
                int w, h;
                pco_get_actual_size(pco, &w, &h);
                *((uint32_t *) data) = h;
            }

        default:
            return UCA_ERR_PROP_INVALID;
    }
    return UCA_NO_ERROR;
}

uint32_t uca_pco_init(struct uca_t *uca)
{
    uca->user = (struct pco_cam_t *) malloc(sizeof(struct pco_cam_t));

    struct pco_cam_t *pco_cam = uca->user;
    struct pco_edge_t *pco = pco_cam->pco = pco_init();

    if ((pco->serial_ref == NULL) || !pco_active(pco)) {
        pco_destroy(pco);
        return UCA_ERR_INIT_NOT_FOUND;
    }

    Fg_Struct *fg = pco_cam->fg = Fg_Init("libFullAreaGray8.so", 0);

    pco_scan_and_set_baud_rate(pco);

    /* Camera found, set function pointers... */
    uca->cam_destroy = &uca_pco_destroy;
    uca->cam_set_property = &uca_pco_set_property;
    uca->cam_get_property = &uca_pco_get_property;
    uca->cam_acquire_image = &uca_pco_acquire_image;

    /* Prepare camera for recording */
    pco_set_rec_state(pco, 0);
    pco_set_timestamp_mode(pco, 2);
    pco_set_timebase(pco, 1, 1); 
    pco_arm_camera(pco);

    if (pco->transfer.DataFormat != (SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER | PCO_CL_DATAFORMAT_5x16))
        pco->transfer.DataFormat = SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER | PCO_CL_DATAFORMAT_5x16;

    /* Prepare frame grabber for recording */
    int val = FG_CL_8BIT_FULL_10;
    Fg_setParameter(fg, FG_CAMERA_LINK_CAMTYP, &val, PORT_A);

    val = FG_GRAY;
    Fg_setParameter(fg, FG_FORMAT, &val, PORT_A);

    val = FREE_RUN;
    Fg_setParameter(fg, FG_TRIGGERMODE, &val, PORT_A);

    int width, height;
    pco_get_actual_size(pco, &width, &height);

    /* Yes, we really have to take an image twice as large because we set the
     * CameraLink interface to 8-bit 10 Taps, but are actually using 5x16 bits. */
    width *= 2;
    height *= 2;
    Fg_setParameter(fg, FG_WIDTH, &width, PORT_A);
    Fg_setParameter(fg, FG_HEIGHT, &height, PORT_A);

    pco_set_rec_state(pco, 1);

    return 0;
}
