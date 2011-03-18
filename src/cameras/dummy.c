#include <stdlib.h>
#include <string.h>
#define __USE_BSD
#include <unistd.h>
#undef __USE_BSD
#include <sys/time.h>
#include <assert.h>

#include "config.h"
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"

#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif

/**
 * User structure for the dummy camera.
 */
typedef struct dummy_cam {
    uint32_t bitdepth;
    uint32_t frame_rate;
#ifdef HAVE_PTHREADS
    pthread_t   grab_thread;
    bool        thread_running;
    char        *buffer;
#endif
} dummy_cam_t;


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

const int DIGIT_WIDTH = 4;
const int DIGIT_HEIGHT = 5;

#define GET_DUMMY(uca) ((struct dummy_cam *)(uca->user))
#define set_void(p, type, value) { *((type *) p) = value; }

static void uca_dummy_print_number(char *buffer, int number, int x, int y, int width)
{
    for (int i = 0; i < DIGIT_WIDTH; i++) {
        for (int j = 0; j < DIGIT_HEIGHT; j++) {
            buffer[(y+j)*width + (x+i)] = digits[number][j*DIGIT_WIDTH+i];
        }
    }
}

static void uca_dummy_memcpy(struct uca_camera *cam, char *buffer)
{
    /* print current frame number */
    unsigned int number = cam->current_frame;
    unsigned int divisor = 100000000;
    int x = 10;
    while (divisor > 1) {
        uca_dummy_print_number(buffer, number / divisor, x, 10, cam->frame_width);
        number = number % divisor;
        divisor = divisor / 10;
        x += 5;
    }
}

static __suseconds_t uca_dummy_time_diff(struct timeval *start, struct timeval *stop)
{
    return (stop->tv_sec - start->tv_sec)*1000000 + (stop->tv_usec - start->tv_usec);
}

static void *uca_dummy_grab_thread(void *arg)
{
    struct uca_camera *cam = ((struct uca_camera *) arg);
    struct dummy_cam *dc = GET_DUMMY(cam);

    assert(dc->frame_rate > 0);
    const __useconds_t sleep_time = (unsigned int) 1000000.0f / dc->frame_rate;
    __suseconds_t call_time;
    struct timeval start, stop;

    while (dc->thread_running) {
        uca_dummy_memcpy(cam, dc->buffer);
        gettimeofday(&start, NULL);
        cam->callback(cam->current_frame, dc->buffer, NULL, cam->callback_user);
        gettimeofday(&stop, NULL);

        call_time = uca_dummy_time_diff(&start, &stop);
        if (call_time < sleep_time) {
            cam->current_frame++;
            usleep(sleep_time - call_time);
        }
        else
            cam->current_frame += call_time / sleep_time;
    }
    return NULL;
}


/*
 * --- interface implementations ----------------------------------------------
 */
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

        case UCA_PROP_FRAMERATE:
            GET_DUMMY(cam)->frame_rate = *((uint32_t *) data);
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
    if (cam->callback != NULL) {
#ifdef HAVE_PTHREADS
        struct dummy_cam *dc = GET_DUMMY(cam);
        /* FIXME: handle return value */
        dc->thread_running = true;
        dc->buffer = (char *) malloc(cam->frame_width * cam->frame_height);
        pthread_create(&dc->grab_thread, NULL, &uca_dummy_grab_thread, cam);
#endif
    }
    cam->current_frame = 0;
    cam->state = UCA_CAM_RECORDING;
    return UCA_NO_ERROR;
}

uint32_t uca_dummy_stop_recording(struct uca_camera *cam)
{
    struct dummy_cam *dc = GET_DUMMY(cam);
    if (cam->callback != NULL) {
        dc->thread_running = false;
        free(dc->buffer);
        dc->buffer = NULL;
    }
    cam->state = UCA_CAM_ARMED;
    return UCA_NO_ERROR;
}

uint32_t uca_dummy_register_callback(struct uca_camera *cam, uca_cam_grab_callback cb, void *user)
{
    if (cam->callback == NULL) {
        cam->callback = cb;
        cam->callback_user = user;
    }
    else
        return UCA_ERR_GRABBER_CALLBACK_ALREADY_REGISTERED;

    return UCA_NO_ERROR;
}

uint32_t uca_dummy_grab(struct uca_camera *cam, char *buffer, void *meta_data)
{
    if (cam->callback != NULL)
        return UCA_ERR_GRABBER_CALLBACK_ALREADY_REGISTERED;

    uca_dummy_memcpy(cam, buffer);
    cam->current_frame++;
    return UCA_NO_ERROR;
}

static uint32_t uca_dummy_destroy(struct uca_camera *cam)
{
    struct dummy_cam *dc = GET_DUMMY(cam);
    free(dc->buffer);
    free(dc);
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
    uca->register_callback = &uca_dummy_register_callback;

    uca->state = UCA_CAM_CONFIGURABLE;
    uca->frame_width = 320;
    uca->frame_height = 240;
    uca->current_frame = 0;
    uca->grabber = NULL;
    uca->callback = NULL;
    uca->callback_user = NULL;

    struct dummy_cam *dummy_cam = (struct dummy_cam *) malloc(sizeof(struct dummy_cam));
    dummy_cam->bitdepth = 8;
    dummy_cam->frame_rate = 100;
    uca->user = dummy_cam;

    *cam = uca;

    return UCA_NO_ERROR;
}
