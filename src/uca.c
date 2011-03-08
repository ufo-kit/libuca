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

const char *uca_unit_map[] = {
    "px",
    "bits",
    "ns",
    "µs",
    "ms",
    "s",
    "rows",
    "fps"
    "" 
};

static struct uca_property_t property_map[UCA_PROP_LAST+1] = {
    { "general.name",           uca_na,     uca_string,  uca_read }, 
    { "image.width",            uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "image.width.min",        uca_pixel,  uca_uint32t, uca_read }, 
    { "image.width.max",        uca_pixel,  uca_uint32t, uca_read }, 
    { "image.height",           uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "image.height.min",       uca_pixel,  uca_uint32t, uca_read }, 
    { "image.height.max",       uca_pixel,  uca_uint32t, uca_read }, 
    { "image.offset.x",         uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "image.offset.y",         uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "image.bitdepth",         uca_bits,   uca_uint8t,  uca_read}, 
    { "time.exposure",          uca_us,     uca_uint32t, uca_readwrite }, 
    { "time.exposure.min",      uca_ns,     uca_uint32t, uca_read }, 
    { "time.exposure.max",      uca_ms,     uca_uint32t, uca_read }, 
    { "time.delay",             uca_us,     uca_uint32t, uca_readwrite }, 
    { "time.delay.min",         uca_ns,     uca_uint32t, uca_read }, 
    { "time.delay.max",         uca_ms,     uca_uint32t, uca_read }, 
    { "time.framerate",         uca_fps,    uca_uint32t, uca_read }, 
    { "mode.trigger",           uca_na,     uca_uint32t, uca_readwrite }, 
    { "mode.timestamp",         uca_na,     uca_uint32t, uca_readwrite }, 
    { "mode.scan",              uca_na,     uca_uint32t, uca_readwrite }, 
    { "ipe.interlace.samplerate", uca_na,   uca_uint32t, uca_readwrite }, 
    { "ipe.interlace.threshold.pixel", uca_na, uca_uint32t, uca_readwrite }, 
    { "ipe.interlace.threshold.row", uca_na, uca_uint32t,   uca_readwrite },
    { "mode.correction",        uca_na,     uca_uint32t, uca_readwrite }, 
    { NULL, 0, 0, 0 }
};

struct uca_t *uca_init(void)
{
    struct uca_t *uca = (struct uca_t *) malloc(sizeof(struct uca_t));
    uca->cameras = NULL;

    uca_grabber_init grabber_inits[] = {
#ifdef HAVE_ME4
        uca_me4_init,
#endif
        NULL
    };

    uca_cam_init cam_inits[] = {
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
        NULL
    };

    int i = 0;
    struct uca_camera_t *current = NULL;

    /* Probe each frame grabber that is configured */
    struct uca_grabber_t *grabber = NULL;
    while (grabber_inits[i] != NULL) {
        uca_grabber_init init = grabber_inits[i];
        /* FIXME: we don't only want to take the first one */
        if (init(&grabber) != UCA_ERR_GRABBER_NOT_FOUND)
            break;
        i++;
    }

    if (grabber == NULL) {
        free(uca);
        return NULL;
    }
    uca->grabbers = grabber;
    grabber->next = NULL;

    /* Probe each camera that is configured */
    i = 0;
    while (cam_inits[i] != NULL) {
        struct uca_camera_t *cam = NULL;
        uca_cam_init init = cam_inits[i];
        if (init(&cam, grabber) != UCA_ERR_CAM_NOT_FOUND) {
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
        struct uca_camera_t *cam = uca->cameras, *tmp;
        while (cam != NULL) {
            tmp = cam;
            cam->destroy(cam);
            cam = cam->next;
            free(tmp);
        }

        struct uca_grabber_t *grabber = uca->grabbers, *tmpg;
        while (grabber != NULL) {
            tmpg = grabber;
            grabber->destroy(grabber);
            grabber = grabber->next;
            free(grabber);
        }

        free(uca);
    }
}

enum uca_property_ids uca_get_property_id(const char *property_name)
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

struct uca_property_t *uca_get_full_property(enum uca_property_ids property_id)
{
    if ((property_id >= 0) && (property_id < UCA_PROP_LAST))
        return &property_map[property_id];
    return NULL;
}

const char* uca_get_property_name(enum uca_property_ids property_id)
{
    if ((property_id >= 0) && (property_id < UCA_PROP_LAST))
        return property_map[property_id].name;
}
