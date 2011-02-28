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

static struct uca_property_t property_map[UCA_PROP_LAST+1] = {
    { "name",           uca_na,     uca_string }, 
    { "width",          uca_pixel,  uca_uint32t }, 
    { "width.min",      uca_pixel,  uca_uint32t }, 
    { "width.max",      uca_pixel,  uca_uint32t }, 
    { "height",         uca_pixel,  uca_uint32t }, 
    { "height.min",     uca_pixel,  uca_uint32t }, 
    { "height.max",     uca_pixel,  uca_uint32t }, 
    { "offset.x",       uca_pixel,  uca_uint32t }, 
    { "offset.y",       uca_pixel,  uca_uint32t }, 
    { "bitdepth",       uca_bits,   uca_uint8t }, 
    { "exposure",       uca_us,     uca_uint32t }, 
    { "exposure.min",   uca_ns,     uca_uint32t }, 
    { "exposure.max",   uca_ms,     uca_uint32t }, 
    { "delay",          uca_us,     uca_uint32t }, 
    { "delay.min",      uca_ns,     uca_uint32t }, 
    { "delay.max",      uca_ms,     uca_uint32t }, 
    { "framerate",      uca_na,     uca_uint32t }, 
    { "triggermode",    uca_na,     uca_uint32t }, 
    { "timestampmode",  uca_na,     uca_uint32t }, 
    { "scan-mode",      uca_na,     uca_uint32t }, 
    { "interlace.samplerate", uca_na, uca_uint32t }, 
    { "interlace.threshold.pixel", uca_na, uca_uint32t }, 
    { "interlace.threshold.row", uca_na, uca_uint32t }, 
    { "correctionmode", uca_na, uca_uint32t }, 
    { NULL, 0, 0 }
};

int32_t uca_get_property_id(const char *property_name)
{
    char *name;
    int i = 0;
    while (property_map[i].name != NULL) {
        if (!strcmp(property_map[i].name, property_name))
            return i;
        i++;
    }
    return UCA_ERR_PROP_INVALID;
}

struct uca_property_t *uca_get_full_property(int32_t property_id)
{
    if ((property_id >= 0) && (property_id < UCA_PROP_LAST))
        return &property_map[property_id];
    return NULL;
}

const char* uca_get_property_name(int32_t property_id)
{
    if ((property_id >= 0) && (property_id < UCA_PROP_LAST))
        return property_map[property_id].name;
}
