
#include <stdlib.h>
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


static uint32_t uca_pco_set_dimensions(struct uca_t *uca, uint32_t *width, uint32_t *height)
{
    Fg_Struct *fg = GET_FG(uca);
    Fg_setParameter(fg, FG_WIDTH, width, PORT_A);
    Fg_setParameter(fg, FG_HEIGHT, height, PORT_A);
    return 0;
}

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

uint32_t uca_pco_init(struct uca_t *uca)
{
    uca->user = (struct pco_cam_t *) malloc(sizeof(struct pco_cam_t));


    struct pco_cam_t *pco_cam = uca->user;
    struct pco_edge_t *pco = pco_cam->pco = pco_init();

    if ((pco->serial_ref == NULL) || !pco_active(pco)) {
        pco_destroy(pco);
        return UCA_ERR_INIT_NOT_FOUND;
    }

    pco_cam->fg = Fg_Init("libFullAreaGray8.so", 0);

    pco_scan_and_set_baud_rate(pco);

    /* Camera found, set function pointers... */
    uca->cam_destroy = &uca_pco_destroy;
    uca->cam_set_dimensions = &uca_pco_set_dimensions;
    uca->cam_set_bitdepth = &uca_pco_set_bitdepth;
    uca->cam_set_exposure = &uca_pco_set_exposure;
    uca->cam_set_delay = &uca_pco_set_delay;
    uca->cam_acquire_image = &uca_pco_acquire_image;

    /* ... and some properties */
    pco_get_actual_size(pco, &uca->image_width, &uca->image_height);

    /* Prepare camera for recording. */
    pco_set_rec_state(pco, 0);
    pco_set_timestamp_mode(pco, 2);
    pco_set_timebase(pco, 1, 1); 
    pco_arm_camera(pco);

    return 0;
}
