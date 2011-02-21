#include <stdlib.h>

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

    int i = 0;
    while (inits[i] != NULL) {
        uca_cam_init init = inits[i];
        if (init(uca))
            return uca;
        i++;
    }

    /* No camera found then return nothing */
    free(uca);
    return NULL;
}

void uca_destroy(struct uca_t *uca)
{
    uca->cam_destroy(uca);
    free(uca);
}
