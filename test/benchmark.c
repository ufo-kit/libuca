
#define __USE_BSD
#include <unistd.h>
#undef __USE_BSD
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include "uca.h"
#include "uca-cam.h"


static __suseconds_t time_diff(struct timeval *start, struct timeval *stop)
{
    return (stop->tv_sec - start->tv_sec)*1000000 + (stop->tv_usec - start->tv_usec);
}

void grab_callback_raw(uint32_t image_number, void *buffer, void *meta_data, void *user)
{
    uint32_t *count = (uint32_t *) user;
    *count = image_number;
}

void benchmark_cam(struct uca_camera *cam)
{
    char name[256];
    uca_cam_get_property(cam, UCA_PROP_NAME, name, 256);
    
    uint32_t val = 5000;
    uca_cam_set_property(cam, UCA_PROP_EXPOSURE, &val);
    val = 0;
    uca_cam_set_property(cam, UCA_PROP_DELAY, &val);

    uint32_t width, height, bits;
    uca_cam_get_property(cam, UCA_PROP_WIDTH, &width, 0);
    uca_cam_get_property(cam, UCA_PROP_HEIGHT, &height, 0);
    uca_cam_get_property(cam, UCA_PROP_BITDEPTH, &bits, 0);
    int pixel_size = bits == 8 ? 1 : 2;

    struct timeval start, stop;

    for (int i = 0; i < 2; i++) {
        char *buffer = (char *) malloc(width*height*pixel_size);
        uca_cam_set_property(cam, UCA_PROP_HEIGHT, &height);
        uca_cam_alloc(cam, 20);

        /*
         * Experiment 1: Grab n frames manually
         */
        gettimeofday(&start, NULL);
        uca_cam_start_recording(cam);
        for (int i = 0; i < 1000; i++)
            uca_cam_grab(cam, (char *) buffer, NULL);
            
        uca_cam_stop_recording(cam);
        gettimeofday(&stop, NULL);

        float seconds = time_diff(&start, &stop) / 1000000.0;
        printf("%f,%s;%i;%i;1;%.2f;%.2f\n", seconds,name, width, height, 1000.0 / seconds, width*height*pixel_size*1000.0/ (1024*1024*seconds));

        height /= 2;
        free(buffer);
    }
}

int main(int argc, char *argv[])
{
    struct uca *u = uca_init(NULL);
    if (u == NULL) {
        printf("Couldn't find a camera\n");
        return 1;
    }

    printf("# camera;width;height;experiment;frames-per-second;throughput in MB/s\n");

    /* take first camera */
    struct uca_camera *cam = u->cameras;
    while (cam != NULL) {
        benchmark_cam(cam);
        cam = cam->next;
    }

    uca_destroy(u);
    return 0;
}
