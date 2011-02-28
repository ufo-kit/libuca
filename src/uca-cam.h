#ifndef __UNIFIED_CAMERA_ACCESS_CAM_H
#define __UNIFIED_CAMERA_ACCESS_CAM_H

#include <stdint.h>

struct uca_camera_t;
struct uca_grabber_t;
struct uca_property_t;

enum uca_property_ids;

/*
 * --- non-virtual methods ----------------------------------------------------
 */
enum uca_cam_state uca_get_camera_state(struct uca_camera_t *cam);


/*
 * --- virtual methods --------------------------------------------------------
 */

/**
 * \brief Camera probing and initialization
 * \return UCA_ERR_INIT_NOT_FOUND if camera is not found or could not be initialized
 */
typedef uint32_t (*uca_cam_init) (struct uca_camera_t **cam, struct uca_grabber_t *grabber);

/**
 * \brief Free camera resouces
 */
typedef uint32_t (*uca_cam_destroy) (struct uca_camera_t *cam);

/**
 * \brief Set a property
 * \param[in] property_name Name of the property as defined in XXX
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera or
 * UCA_ERR_PROP_VALUE_OUT_OF_RANGE if value cannot be set.
 */
typedef uint32_t (*uca_cam_set_property) (struct uca_camera_t *cam, enum uca_property_ids property, void *data);

/**
 * \brief Set a property
 * \param[in] property_name Name of the property as defined in XXX
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera
 */
typedef uint32_t (*uca_cam_get_property) (struct uca_camera_t *cam, enum uca_property_ids property, void *data);

/** \brief Allocate number of buffers
 *
 * The size of each buffer is width x height x bits
 */
typedef uint32_t (*uca_cam_alloc) (struct uca_camera_t *cam, uint32_t n_buffers);

/**
 * \brief Acquire one frame
 */
typedef uint32_t (*uca_cam_acquire_image) (struct uca_camera_t *cam, void *buffer);



enum uca_cam_state {
    UCA_CAM_ERROR,
    UCA_CAM_CONFIGURABLE,
    UCA_CAM_ARMED,
    UCA_CAM_RECORDING,
};

struct uca_camera_t {
    struct uca_camera_t     *next;

    /* Function pointers to camera-specific methods */
    uca_cam_set_property    set_property;
    uca_cam_get_property    get_property;
    uca_cam_acquire_image   acquire_image;
    uca_cam_alloc           alloc;

    /* Private */
    uca_cam_destroy         destroy;

    struct uca_grabber_t    *grabber;
    enum uca_cam_state      state;

    void *user; /**< private user data to be used by the camera driver */
};

#endif
