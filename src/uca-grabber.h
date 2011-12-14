#ifndef __UNIFIED_CAMERA_ACCESS_GRABBER_H
#define __UNIFIED_CAMERA_ACCESS_GRABBER_H

#include <stdbool.h>
#include "uca.h"
#include "uca-cam.h"

/**
 * \file uca-grabber.h
 * \brief Abstract frame grabber model
 */

enum uca_grabber_constants {
    UCA_GRABBER_INVALID = -1,

    /* properties */
    UCA_GRABBER_FORMAT = UCA_PROP_LAST + 1,
    UCA_GRABBER_TRIGGER_MODE,
    UCA_GRABBER_CAMERALINK_TYPE,

    /* values */
    UCA_FORMAT_GRAY8,
    UCA_FORMAT_GRAY16,

    UCA_CL_SINGLE_TAP_8,
    UCA_CL_SINGLE_TAP_16,
    UCA_CL_8BIT_FULL_8,
    UCA_CL_8BIT_FULL_10
};


/*
 * --- virtual methods --------------------------------------------------------
 */

/**
 * Camera probing and initialization.
 *
 * \return UCA_ERR_INIT_NOT_FOUND if grabber is not found or could not be initialized
 */
typedef uint32_t (*uca_grabber_init) (struct uca_grabber_priv **grabber);

/**
 * Free frame grabber resouces.
 */
typedef uint32_t (*uca_grabber_destroy) (struct uca_grabber_priv *grabber);

/**
 * Set a frame grabber property.
 *
 * \param[in] prop Name of the property as defined in uca_grabber_priv_constants
 *
 * \return UCA_ERR_PROP_INVALID if property is not supported on the frame
 *   grabber or UCA_ERR_PROP_VALUE_OUT_OF_RANGE if value cannot be set.
 */
typedef uint32_t (*uca_grabber_set_property) (struct uca_grabber_priv *grabber, enum uca_grabber_constants prop, void *data);

/**
 * Get a frame grabber property.
 *
 * \param[in] prop Name of the property as defined in uca_grabber_priv_constants
 * 
 * \return UCA_ERR_PROP_INVALID if property is not supported on the frame grabber 
 */
typedef uint32_t (*uca_grabber_get_property) (struct uca_grabber_priv *grabber, enum uca_grabber_constants prop, void *data);

/**
 * Allocate buffers with current width, height and bitdepth.
 *
 * \warning Subsequent changes of width and height might corrupt memory.
 */
typedef uint32_t (*uca_grabber_alloc) (struct uca_grabber_priv *grabber, uint32_t pixel_size, uint32_t n_buffers);

/**
 * Begin acquiring frames.
 *
 * \param[in] n_frames Number of frames to acquire, -1 means infinite number
 *
 * \param[in] async Grab asynchronous if true
 */
typedef uint32_t (*uca_grabber_acquire) (struct uca_grabber_priv *grabber, int32_t n_frames);

/**
 * Stop acquiring frames.
 */
typedef uint32_t (*uca_grabber_stop_acquire) (struct uca_grabber_priv *grabber);

/**
 * Issue a software trigger signal.
 */
typedef uint32_t (*uca_grabber_trigger) (struct uca_grabber_priv *grabber);

/**
 * Grab a frame.
 *
 * This method is usually called through the camera interface and not directly.
 *
 * \param[in] buffer The pointer of the frame buffer is set here
 *
 * \param[out] frame_number Number of the grabbed frame
 */
typedef uint32_t (*uca_grabber_grab) (struct uca_grabber_priv *grabber, void **buffer, uint64_t *frame_number);


/**
 * Register callback for given frame grabber. To actually start receiving
 * frames, call uca_grabber_priv_acquire().
 *
 * \param[in] grabber The grabber for which the callback should be installed
 *
 * \param[in] cb Callback function for when a frame arrived
 */
typedef uint32_t (*uca_grabber_register_callback) (struct uca_grabber_priv *grabber, uca_cam_grab_callback cb, void *meta_data, void *user);


/**
 * Represents a frame grabber abstraction, that concrete frame grabber
 * implementations must implement.
 *
 * A uca_grabber_priv_t structure is never used directly but only via the
 * uca_camera_t interface in order to keep certain duplicated properties in sync
 * (e.g. image dimensions can be set on frame grabber and camera).
 */
typedef struct uca_grabber_priv {
    struct uca_grabber_priv    *next;

    /* Function pointers to grabber-specific methods */
    uca_grabber_destroy      destroy;
    uca_grabber_set_property set_property;
    uca_grabber_get_property get_property;
    uca_grabber_alloc        alloc;
    uca_grabber_acquire      acquire;
    uca_grabber_stop_acquire stop_acquire;
    uca_grabber_trigger      trigger;
    uca_grabber_grab         grab;
    uca_grabber_register_callback register_callback;

    /* Private */
    uca_cam_grab_callback   callback;
    bool synchronous;   /**< if true uca_grabber_priv_grab() blocks until image is transferred */
    void *user;
} uca_grabber_priv_t;



#endif
