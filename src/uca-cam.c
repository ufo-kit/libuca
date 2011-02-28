
#include <stdlib.h>
#include "uca.h"
#include "uca-cam.h"

enum uca_cam_state uca_get_camera_state(struct uca_camera_t *cam)
{
    return cam->state;
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
