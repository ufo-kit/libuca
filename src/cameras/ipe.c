
#include <stdlib.h>
#include <string.h>
#include <pcilib.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"

#define set_void(p, type, value) { *((type *) p) = value; }
#define GET_HANDLE(cam) ((pcilib_t *) cam->user)

static void uca_ipe_handle_error(const char *format, ...)
{
    /* Do nothing, we just check errno. */
}

static uint32_t uca_ipe_set_property(struct uca_camera_t *cam, enum uca_property_ids property, void *data)
{
    return UCA_NO_ERROR;
}

static uint32_t uca_ipe_get_property(struct uca_camera_t *cam, enum uca_property_ids property, void *data)
{
    pcilib_t *handle = GET_HANDLE(cam);
    pcilib_register_value_t value = 0;

    switch (property) {
        case UCA_PROP_NAME:
            strcpy((char *) data, "IPE PCIe");
            break;

        case UCA_PROP_TEMPERATURE_SENSOR:
            pcilib_read_register(handle, NULL, "cmosis_temperature", &value);
            set_void(data, uint32_t, (uint32_t) value);
            break;

        default:
            return UCA_ERR_PROP_INVALID;
    }
    return UCA_NO_ERROR;
}

static uint32_t uca_ipe_start_recording(struct uca_camera_t *cam)
{
    return UCA_NO_ERROR;
}

static uint32_t uca_ipe_stop_recording(struct uca_camera_t *cam)
{
    return UCA_NO_ERROR;
}

static uint32_t uca_ipe_grab(struct uca_camera_t *cam, char *buffer)
{
    return UCA_NO_ERROR;
}

static uint32_t uca_ipe_destroy(struct uca_camera_t *cam)
{
    pcilib_close(GET_HANDLE(cam));
    return UCA_NO_ERROR;
}

uint32_t uca_ipe_init(struct uca_camera_t **cam, struct uca_grabber_t *grabber)
{
    pcilib_model_t model = PCILIB_MODEL_DETECT;
    pcilib_bar_t bar = PCILIB_BAR_DETECT;
    pcilib_t *handle = pcilib_open("/dev/fpga0", model);
    if (handle < 0)
        return UCA_ERR_CAM_NOT_FOUND;

    pcilib_set_error_handler(&uca_ipe_handle_error);
    model = pcilib_get_model(handle);

    struct uca_camera_t *uca = (struct uca_camera_t *) malloc(sizeof(struct uca_camera_t));

    /* Camera found, set function pointers... */
    uca->destroy = &uca_ipe_destroy;
    uca->set_property = &uca_ipe_set_property;
    uca->get_property = &uca_ipe_get_property;
    uca->start_recording = &uca_ipe_start_recording;
    uca->stop_recording = &uca_ipe_stop_recording;
    uca->grab = &uca_ipe_grab;

    uca->state = UCA_CAM_CONFIGURABLE;
    uca->user = handle;
    *cam = uca;

    return UCA_NO_ERROR;
}
