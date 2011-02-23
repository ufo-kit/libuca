#ifndef __UNIFIED_CAMERA_ACCESS_H
#define __UNIFIED_CAMERA_ACCESS_H

/**
 * \file uca.h
 * \brief Abstract camera model
 *
 * The uca_t structure represents a common interface for cameras regardless of
 * their connectivity. Each camera that adheres to this model must provide an
 * initialization function that probes the device and sets all function pointers
 * to their respective implementation.
 */

#include <stdint.h>

struct uca_t;

/**
 * \brief Camera probing and initialization
 * \return UCA_ERR_INIT_NOT_FOUND if camera is not found or could not be initialized
 */
typedef uint32_t (*uca_cam_init) (struct uca_t *uca);

/**
 * \brief Free camera resouces
 */
typedef uint32_t (*uca_cam_destroy) (struct uca_t *uca);

/**
 * \brief Set dimension of grabbed images
 * \param[in] width Width of the image
 * \param[in] height Height of the image
 * \note input parameters might be changed if dimensions couldn't be set
 */
typedef uint32_t (*uca_cam_set_dimensions) (struct uca_t *uca, uint32_t *width, uint32_t *height);

/**
 * \brief Set bitdepth of grabbed images
 */
typedef uint32_t (*uca_cam_set_bitdepth) (struct uca_t *uca, uint8_t *bitdepth);

/**
 * \brief Set exposure time in milliseconds
 */
typedef uint32_t (*uca_cam_set_exposure) (struct uca_t *uca, uint32_t *exposure);

/**
 * \brief Set delay time in milliseconds
 */
typedef uint32_t (*uca_cam_set_delay) (struct uca_t *uca, uint32_t *delay);

/**
 * \brief Acquire one frame
 */
typedef uint32_t (*uca_cam_acquire_image) (struct uca_t *uca, void *buffer);


#define UCA_NO_ERROR                    0

#define UCA_ERR_INIT_NOT_FOUND          1   /**< camera probing or initialization failed */

#define UCA_ERR_DIMENSION_NOT_SUPPORTED 1

#define UCA_ERR_BITDEPTH_NOT_SUPPORTED  1

#define UCA_BIG_ENDIAN      1
#define UCA_LITTLE_ENDIAN   2

struct uca_t {
    /* These must be set by uca_*_init() */
    unsigned int image_width;
    unsigned int image_height;
    unsigned int image_bitdepth;
    unsigned int image_flags;

    char *camera_name;
    
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
