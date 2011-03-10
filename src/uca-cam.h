#ifndef __UNIFIED_CAMERA_ACCESS_CAM_H
#define __UNIFIED_CAMERA_ACCESS_CAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uca_camera_t;
struct uca_grabber_t;
struct uca_property_t;

enum uca_property_ids;

enum uca_cam_state {
    UCA_CAM_ERROR,
    UCA_CAM_CONFIGURABLE,
    UCA_CAM_ARMED,
    UCA_CAM_RECORDING,
};


/*
 * --- non-virtual methods ----------------------------------------------------
 */

/**
 * \brief Allocates buffer memory for the internal frame grabber
 * \param[in] n_buffers Number of sub-buffers with size frame_width*frame_height
 */
uint32_t uca_cam_alloc(struct uca_camera_t *cam, uint32_t n_buffers);

enum uca_cam_state uca_cam_get_state(struct uca_camera_t *cam);


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
 * \param[in] property ID of the property as defined in XXX
 * \param[out] data Where to read the property's value from
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera or
 * UCA_ERR_PROP_VALUE_OUT_OF_RANGE if value cannot be set.
 */
typedef uint32_t (*uca_cam_set_property) (struct uca_camera_t *cam, enum uca_property_ids property, void *data);

/**
 * \brief Get a property
 * \param[in] property ID of the property as defined in XXX
 * \param[out] data Where to store the property's value
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera
 */
typedef uint32_t (*uca_cam_get_property) (struct uca_camera_t *cam, enum uca_property_ids property, void *data);

/**
 * \brief Begin recording
 *
 * Usually this also involves the frame acquisition of the frame grabber but is
 * hidden by this function.
 */
typedef uint32_t (*uca_cam_start_recording) (struct uca_camera_t *cam);

typedef uint32_t (*uca_cam_stop_recording) (struct uca_camera_t *cam);

/**
 * \brief Grab one image from the camera
 * 
 * The grabbing involves a memory copy because we might have to decode the image
 * coming from the camera, which the frame grabber is not able to do.
 *
 * \param[in] buffer Destination buffer
 */
typedef uint32_t (*uca_cam_grab) (struct uca_camera_t *cam, char *buffer);


struct uca_camera_t {
    struct uca_camera_t     *next;

    /* Function pointers to camera-specific methods */
    uca_cam_set_property    set_property;
    uca_cam_get_property    get_property;
    uca_cam_start_recording start_recording;
    uca_cam_stop_recording  stop_recording;
    uca_cam_grab            grab;

    /* Private */
    uca_cam_destroy         destroy;

    struct uca_grabber_t    *grabber;
    enum uca_cam_state      state;
    uint32_t                frame_width;
    uint32_t                frame_height;

    void *user; /**< private user data to be used by the camera driver */
};

#ifdef __cplusplus
}
#endif

#endif
