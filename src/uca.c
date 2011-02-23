#include <stdlib.h>

#include "config.h"
#include "uca.h"

#ifdef HAVE_PCO_EDGE
#include "cameras/uca_pco.h"
#endif

#ifdef HAVE_PHOTON_FOCUS
#include "cameras/uca_pf.h"
#endif

#ifdef HAVE_IPE_CAM
#include "cameras/uca_ipe.h"
#endif


struct uca_t *uca_init()
{
    struct uca_t *uca = (struct uca_t *) malloc(sizeof(struct uca_t));

    uca_cam_init inits[] = {
#ifdef HAVE_PCO_EDGE
        uca_pco_init,
#elif HAVE_PHOTON_FOCUS
        uca_pf_init,
#elif HAVE_IPE_CAM
        uca_ipe_init,
#endif
    NULL };

    /* Set all function pointers to NULL and thus make the class abstract */
    uca->cam_destroy = NULL;
    uca->cam_set_dimensions = NULL;
    uca->cam_set_bitdepth = NULL;
    uca->cam_set_delay = NULL;
    uca->cam_set_exposure = NULL;
    uca->cam_acquire_image = NULL;

    uca->camera_name = NULL;

    int i = 0;
    while (inits[i] != NULL) {
        uca_cam_init init = inits[i];
        if (init(uca) != UCA_ERR_INIT_NOT_FOUND)
            return uca;
        i++;
    }

    /* No camera found then indicate error */
    free(uca);
    return NULL;
}

void uca_destroy(struct uca_t *uca)
{
    if (uca != NULL) {
        uca->cam_destroy(uca);
        free(uca);
    }
}
