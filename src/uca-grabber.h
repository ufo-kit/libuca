#ifndef __UNIFIED_CAMERA_ACCESS_GRABBER_H
#define __UNIFIED_CAMERA_ACCESS_GRABBER_H

#include <stdbool.h>

enum uca_grabber_constants {
    UCA_GRABBER_INVALID = -1,
    /* properties */
    UCA_GRABBER_WIDTH = 0,
    UCA_GRABBER_HEIGHT,
    UCA_GRABBER_WIDTH_MAX,
    UCA_GRABBER_WIDTH_MIN,
    UCA_GRABBER_OFFSET_X,
    UCA_GRABBER_OFFSET_Y,
    UCA_GRABBER_EXPOSURE,
    UCA_GRABBER_FORMAT,
    UCA_GRABBER_TRIGGER_MODE,
    UCA_GRABBER_CAMERALINK_TYPE,

    /* values */
    UCA_FORMAT_GRAY8,
    UCA_FORMAT_GRAY16,

    UCA_CL_8BIT_FULL_8,
    UCA_CL_8BIT_FULL_10,

    UCA_TRIGGER_FREERUN
};

/*
 * --- virtual methods --------------------------------------------------------
 */

/**
 * \brief Camera probing and initialization
 * \return UCA_ERR_INIT_NOT_FOUND if camera is not found or could not be initialized
 */
typedef uint32_t (*uca_grabber_init) (struct uca_grabber_t **grabber);

/**
 * \brief Free camera resouces
 */
typedef uint32_t (*uca_grabber_destroy) (struct uca_grabber_t *grabber);

/**
 * \brief Set a property
 * \param[in] property_name Name of the property as defined in XXX
 * \return UCA_ERR_PROP_INVALID if property is not supported on the frame
 * grabber or UCA_ERR_PROP_VALUE_OUT_OF_RANGE if value cannot be set.
 */
typedef uint32_t (*uca_grabber_set_property) (struct uca_grabber_t *grabber, enum uca_grabber_constants prop, void *data);

/**
 * \brief Set a property
 * \param[in] property_name Name of the property as defined in XXX
 * \return UCA_ERR_PROP_INVALID if property is not supported on the frame grabber 
 */
typedef uint32_t (*uca_grabber_get_property) (struct uca_grabber_t *grabber, enum uca_grabber_constants prop, void *data);

/**
 * \brief Allocate buffers with current width, height and bitdepth
 * \note Subsequent changes of width and height might corrupt memory
 */
typedef uint32_t (*uca_grabber_alloc) (struct uca_grabber_t *grabber, uint32_t pixel_size, uint32_t n_buffers);

/**
 * \brief Begin acquiring frames
 * \param[in] n_frames Number of frames to acquire, -1 means infinite number
 * \param[in] async Grab asynchronous if true
 */
typedef uint32_t (*uca_grabber_acquire) (struct uca_grabber_t *grabber, int32_t n_frames);

/**
 * \brief Stop acquiring frames
 */
typedef uint32_t (*uca_grabber_stop_acquire) (struct uca_grabber_t *grabber);

/**
 * \brief Grab a frame
 *
 * This method is usually called through the camera interface and not directly.
 *
 * \param[in] buffer The pointer of the frame buffer is set here
 */
typedef uint32_t (*uca_grabber_grab) (struct uca_grabber_t *grabber, void **buffer);


struct uca_grabber_t {
    struct uca_grabber_t    *next;

    /* Function pointers to grabber-specific methods */
    uca_grabber_destroy      destroy;
    uca_grabber_set_property set_property;
    uca_grabber_get_property get_property;
    uca_grabber_alloc        alloc;
    uca_grabber_acquire      acquire;
    uca_grabber_stop_acquire stop_acquire;
    uca_grabber_grab         grab;

    /* Private */
    bool asynchronous;
    void *user;
};

#endif
