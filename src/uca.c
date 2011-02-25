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
    uca->cam_set_property = NULL;
    uca->cam_get_property = NULL;
    uca->cam_alloc = NULL;
    uca->cam_acquire_image = NULL;

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

static const char* property_map[] = {
    "name",
    "width",
    "width.min",
    "width.max",
    "height",
    "height.min",
    "height.max",
    "offset.x",
    "offset.y",
    "bit-depth",
    "exposure",
    "exposure.min",
    "exposure.max",
    "delay",
    "delay.min",
    "delay.max",
    "trigger-mode",
    "frame-rate",
    "timestamp-mode",
    "scan-mode",
    "interlace.sample-rate",
    "interlace.threshold.pixel",
    "interlace.threshold.row",
    "correction-mode",
    NULL
};

int32_t uca_get_property_id(const char *property_name)
{
    char *name;
    int i = 0;
    while (property_map[i] != NULL) {
        if (!strcmp(property_map[i], property_name))
            return i;
        i++;
    }
    return UCA_PROP_INVALID;
}

const char* uca_get_property_name(int32_t property_id)
{
    /* TODO: guard that thing */
    return property_map[property_id];
}
