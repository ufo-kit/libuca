
#include <stdlib.h>
#include <string.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"

typedef struct dummy_cam {
    uint32_t bitdepth;
    uint32_t framerate;
    char*   buffer;
} dummy_cam_t;

#define GET_DUMMY(uca) ((struct dummy_cam *)(uca->user))
#define set_void(p, type, value) { *((type *) p) = value; }

static uint32_t uca_dummy_set_property(struct uca_camera *cam, enum uca_property_ids property, void *data)
{
    if (cam->state == UCA_CAM_RECORDING)
        return UCA_ERR_PROP_CAMERA_RECORDING;

    switch (property) {
        case UCA_PROP_WIDTH:
            cam->frame_width = *((uint32_t *) data);
            break;

        case UCA_PROP_HEIGHT:
            cam->frame_height = *((uint32_t *) data);
            break;

        default:
            return UCA_ERR_PROP_INVALID;
    }

    return UCA_NO_ERROR;
}

static uint32_t uca_dummy_get_property(struct uca_camera *cam, enum uca_property_ids property, void *data, size_t num)
{
    switch (property) {
        case UCA_PROP_NAME:
            strncpy((char *) data, "dummy", num);
            break;

        case UCA_PROP_WIDTH:
            set_void(data, uint32_t, cam->frame_width);
            break;

        case UCA_PROP_WIDTH_MIN:
            set_void(data, uint32_t, 1);
            break;

        case UCA_PROP_WIDTH_MAX:
            set_void(data, uint32_t, 4096);
            break;

        case UCA_PROP_HEIGHT:
            set_void(data, uint32_t, cam->frame_height);
            break;

        case UCA_PROP_HEIGHT_MIN:
            set_void(data, uint32_t, 1);
            break;

        case UCA_PROP_HEIGHT_MAX:
            set_void(data, uint32_t, 4096);
            break;

        case UCA_PROP_BITDEPTH:
            set_void(data, uint32_t, 8);
            break;

        default:
            return UCA_ERR_PROP_INVALID;
    }
    return UCA_NO_ERROR;
}

uint32_t uca_dummy_start_recording(struct uca_camera *cam)
{
    cam->current_frame = 0;
    cam->state = UCA_CAM_RECORDING;
    return UCA_NO_ERROR;
}

uint32_t uca_dummy_stop_recording(struct uca_camera *cam)
{
    cam->state = UCA_CAM_ARMED;
    return UCA_NO_ERROR;
}

static const char digits[10][20] = {
    /* 0 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 1 */
    { 0x00, 0x00, 0xff, 0x00,
      0x00, 0xff, 0xff, 0x00,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0x00, 0xff, 0x00 },
    /* 2 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0xff, 0x00, 0x00,
      0xff, 0xff, 0xff, 0xff },
    /* 3 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0x00, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 4 */
    { 0xff, 0x00, 0x00, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0xff,
      0x00, 0x00, 0x00, 0xff },
    /* 5 */
    { 0xff, 0xff, 0xff, 0xff,
      0xff, 0x00, 0x00, 0x00,
      0x00, 0xff, 0xff, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0x00 },
    /* 6 */
    { 0x00, 0xff, 0xff, 0xff,
      0xff, 0x00, 0x00, 0x00,
      0xff, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 7 */
    { 0xff, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0xff,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0xff, 0x00, 0x00,
      0xff, 0x00, 0x00, 0x00 },
    /* 8 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 9 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0x00 }
};

static void uca_dummy_print_number(struct dummy_cam *dummy, int number, int x, int y, int width)
{
    const int digit_width = 4;
    const int digit_height = 5;
    char *buffer = dummy->buffer;
    for (int i = 0; i < digit_width; i++) {
        for (int j = 0; j < digit_height; j++) {
            buffer[(y+j)*width + (x+i)] = digits[number][j*digit_width+i];
        }
    }
}

uint32_t uca_dummy_grab(struct uca_camera *cam, char *buffer)
{
    struct dummy_cam *dummy = GET_DUMMY(cam);
    dummy->buffer = buffer;

    /* print current frame number */
    unsigned int number = cam->current_frame;
    unsigned int divisor = 100000000;
    int x = 10;
    while (divisor > 1) {
        uca_dummy_print_number(dummy, number / divisor, x, 10, cam->frame_width);
        number = number % divisor;
        divisor = divisor / 10;
        x += 5;
    }
    cam->current_frame++;
    return UCA_NO_ERROR;
}

static uint32_t uca_dummy_destroy(struct uca_camera *cam)
{
    return UCA_NO_ERROR;
}

uint32_t uca_dummy_init(struct uca_camera **cam, struct uca_grabber *grabber)
{
    struct uca_camera *uca = (struct uca_camera *) malloc(sizeof(struct uca_camera));

    uca->destroy = &uca_dummy_destroy;
    uca->set_property = &uca_dummy_set_property;
    uca->get_property = &uca_dummy_get_property;
    uca->start_recording = &uca_dummy_start_recording;
    uca->stop_recording = &uca_dummy_stop_recording;
    uca->grab = &uca_dummy_grab;
    uca->state = UCA_CAM_CONFIGURABLE;
    uca->frame_width = 320;
    uca->frame_height = 240;
    uca->current_frame = 0;

    struct dummy_cam *dummy_cam = (struct dummy_cam *) malloc(sizeof(struct dummy_cam));
    dummy_cam->bitdepth = 8;
    dummy_cam->framerate = 100;
    dummy_cam->buffer = NULL;
    uca->user = dummy_cam;

    *cam = uca;

    return UCA_NO_ERROR;
}
