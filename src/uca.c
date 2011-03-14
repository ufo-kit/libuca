#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"
#include "cameras/dummy.h"

#ifdef HAVE_ME4
#include "grabbers/me4.h"
#endif

#ifdef HAVE_PCO_EDGE
#include "cameras/pco.h"
#endif

#ifdef HAVE_PHOTON_FOCUS
#include "cameras/pf.h"
#endif

#ifdef HAVE_IPE_CAMERA
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
    "fps",
    "°C",
    "" 
};

static struct uca_property_t property_map[UCA_PROP_LAST+1] = {
    { "General.Name",           uca_na,     uca_string,  uca_read }, 
    { "Image.Width",            uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "Image.Width.Min",        uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Width.Max",        uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Height",           uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "Image.Height.Min",       uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Height.Max",       uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Offset.x",         uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "Image.Offset.x.Min",     uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Offset.x.Max",     uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Offset.y",         uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "Image.Offset.y.Min",     uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Offset.y.Max",     uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Bitdepth",         uca_bits,   uca_uint32t, uca_read}, 
    { "Time.Exposure",          uca_us,     uca_uint32t, uca_readwrite }, 
    { "Time.Exposure.Min",      uca_us,     uca_uint32t, uca_read }, 
    { "Time.Exposure.Max",      uca_us,     uca_uint32t, uca_read }, 
    { "Time.Delay",             uca_us,     uca_uint32t, uca_readwrite }, 
    { "Time.Delay.Min",         uca_us,     uca_uint32t, uca_read }, 
    { "Time.Delay.Max",         uca_us,     uca_uint32t, uca_read }, 
    { "Time.Framerate",         uca_fps,    uca_uint32t, uca_read }, 
    { "Temperature.Sensor",     uca_dc,     uca_uint32t, uca_read },
    { "Temperature.Camera",     uca_dc,     uca_uint32t, uca_read },
    { "Trigger.Mode",           uca_na,     uca_uint32t, uca_readwrite }, 
    { "Trigger.Exposure",       uca_na,     uca_uint32t, uca_readwrite },
    { "Gain.PGA",               uca_na,     uca_uint32t, uca_readwrite },
    { "Gain.PGA.Min",           uca_na,     uca_uint32t, uca_read },
    { "Gain.PGA.Max",           uca_na,     uca_uint32t, uca_read },
    { "Gain.PGA.Step",          uca_na,     uca_uint32t, uca_read },
    { "Gain.ADC",               uca_na,     uca_uint32t, uca_readwrite },
    { "Gain.ADC.Min",           uca_na,     uca_uint32t, uca_read },
    { "Gain.ADC.Max",           uca_na,     uca_uint32t, uca_read },
    { "Gain.ADC.Step",          uca_na,     uca_uint32t, uca_read },
    { "Mode.Timestamp",         uca_na,     uca_uint32t, uca_readwrite }, 
    { "Mode.Scan",              uca_na,     uca_uint32t, uca_readwrite }, 
    { "Interlace.Samplerate",   uca_na,     uca_uint32t, uca_readwrite }, 
    { "Interlace.Threshold.Pixel", uca_na,  uca_uint32t, uca_readwrite }, 
    { "Interlace.Threshold.Row", uca_na,    uca_uint32t, uca_readwrite },
    { "Mode.correction",        uca_na,     uca_uint32t, uca_readwrite }, 
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
#ifdef HAVE_IPE_CAMERA
        uca_ipe_init,
#endif
#ifdef HAVE_PH
        uca_photron_init,
#endif
        uca_dummy_init,
        NULL
    };

    /* Probe each frame grabber that is configured */
    int i = 0;
    struct uca_grabber_t *grabber = NULL;
    while (grabber_inits[i] != NULL) {
        uca_grabber_init init = grabber_inits[i];
        /* FIXME: we don't only want to take the first one */
        if (init(&grabber) != UCA_ERR_GRABBER_NOT_FOUND)
            break;
        i++;
    }

    /* XXX: We could have no grabber (aka NULL) which is good anyway, since
     * some cameras don't need a grabber device (such as the IPE camera),
     * therefore we also probe each camera against the NULL grabber. However,
     * each camera must make sure to check for such a situation. */

    uca->grabbers = grabber;
    if (grabber != NULL)
        grabber->next = NULL;

    /* Probe each camera that is configured */
    i = 0;
    struct uca_camera_t *current = NULL;
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
    return UCA_NO_ERROR;
}
