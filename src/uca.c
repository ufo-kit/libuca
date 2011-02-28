#include <stdlib.h>

#include "config.h"
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"

#ifdef HAVE_ME4
#include "grabbers/me4.h"
#endif

#ifdef HAVE_PCO_EDGE
#include "cameras/pco.h"
#endif

#ifdef HAVE_PHOTON_FOCUS
#include "cameras/pf.h"
#endif

#ifdef HAVE_IPE_CAM
#include "cameras/ipe.h"
#endif

#ifdef HAVE_PHOTRON_FASTCAM
#include "cameras/photron.h"
#endif


struct uca_t *uca_init()
{
    struct uca_t *uca = (struct uca_t *) malloc(sizeof(struct uca_t));
    uca->cameras = NULL;

    uca_cam_init inits[] = {
#ifdef HAVE_PCO_EDGE
        uca_pco_init,
#endif
#ifdef HAVE_PHOTON_FOCUS
        uca_pf_init,
#endif
#ifdef HAVE_IPE_CAM
        uca_ipe_init,
#endif
#ifdef HAVE_PH
        uca_photron_init,
#endif
    NULL };

    int i = 0;
    struct uca_camera_t* current = NULL;

    while (inits[i] != NULL) {
        struct uca_camera_t *cam = NULL;
        uca_cam_init init = inits[i];
        if (init(&cam) != UCA_ERR_INIT_NOT_FOUND) {
            if (current == NULL) 
                uca->cameras = current = cam;
            else {
                current->next = cam;
                current = cam;
            }
            current->next = NULL;
        }
        i++;
    }

    if (uca->cameras == NULL) {
        free(uca);
        return NULL;
    }
    return uca;
}

void uca_destroy(struct uca_t *uca)
{
    if (uca != NULL) {
        struct uca_camera_t *current = uca->cameras, *tmp;
        while (current != NULL) {
            tmp = current;
            current->destroy(current);
            current = current->next;
            free(tmp);
        }
        free(uca);
    }
}

