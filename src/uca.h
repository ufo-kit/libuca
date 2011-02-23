#ifndef __UNIFIED_CAMERA_ACCESS_H
#define __UNIFIED_CAMERA_ACCESS_H

#include <stdint.h>

struct uca_t;

/*
 * \brief Camera probing and initialization
 * \return 0 if camera is not found or could not be initialized
 */
typedef uint32_t (*uca_cam_init) (struct uca_t *uca);

typedef uint32_t (*uca_cam_destroy) (struct uca_t *uca);

typedef uint32_t (*uca_cam_set_dimensions) (struct uca_t *uca, uint32_t *width, uint32_t *height);

typedef uint32_t (*uca_cam_set_bitdepth) (struct uca_t *uca, uint8_t *bitdepth);

typedef uint32_t (*uca_cam_set_exposure) (struct uca_t *uca, uint32_t *exposure);

typedef uint32_t (*uca_cam_set_delay) (struct uca_t *uca, uint32_t *delay);

typedef uint32_t (*uca_cam_acquire_image) (struct uca_t *uca, void *buffer);



#define UCA_ERR_INIT_NOT_FOUND          1   /**< camera probing failed */

#define UCA_ERR_DIMENSION_NOT_SUPPORTED 1

#define UCA_ERR_BITDEPTH_NOT_SUPPORTED  1


#define UCA_BIG_ENDIAN      1
#define UCA_LITTLE_ENDIAN   2

struct uca_t {
    /* These must be written by uca_cam_init() */
    unsigned int image_width;
    unsigned int image_height;
    unsigned int image_bitdepth;
    unsigned int image_flags;
    
    /* Function pointers to camera-specific methods */
    uca_cam_set_dimensions  cam_set_dimensions;
    uca_cam_set_bitdepth    cam_set_bitdepth;
    uca_cam_set_exposure    cam_set_exposure;
    uca_cam_set_delay       cam_set_delay;

    uca_cam_acquire_image   cam_acquire_image;

    /* Private */
    uca_cam_destroy cam_destroy;

    void *user; /**< private user data to be used by the camera driver */
};

struct uca_t *uca_init();
void uca_destroy(struct uca_t *uca);


#endif
