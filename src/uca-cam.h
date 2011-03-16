#ifndef __UNIFIED_CAMERA_ACCESS_CAM_H
#define __UNIFIED_CAMERA_ACCESS_CAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file uca-cam.h
 * \brief Abstract camera model
 *
 * The uca_camera_t structure represents a common interface for cameras regardless of
 * their connectivity. Each camera that adheres to this model must provide an
 * initialization function implementing uca_cam_init() that probes the device
 * and sets all function pointers to their respective implementations.
 */

struct uca_camera_t;
struct uca_grabber_t;
struct uca_property_t;

enum uca_property_ids;

/**
 * Describes the current state of the camera.
 */
enum uca_cam_state {
    UCA_CAM_ERROR,          /**< Camera is not working correctly */
    UCA_CAM_CONFIGURABLE,   /**< Camera can be configured and is not recording */
    UCA_CAM_ARMED,          /**< Camera is ready for recording */
    UCA_CAM_RECORDING,      /**< Camera is currently recording */
};


/*
 * --- non-virtual methods ----------------------------------------------------
 */

/**
 * Allocates buffer memory for the internal frame grabber.
 *
 * The allocation is just a hint to the underlying camera driver. It might
 * ignore this or pass this information on to a related frame grabber.
 *
 * \param[in] cam A camera structure.
 * 
 * \param[in] n_buffers Number of sub-buffers with size frame_width*frame_height.
 */
uint32_t uca_cam_alloc(struct uca_camera_t *cam, uint32_t n_buffers);

/**
 * Retrieve current state of the camera.
 *
 * \param[in] cam A camera structure.
 *
 * \return A value from the uca_cam_state enum representing the current state of
 *   the camera.
 */
enum uca_cam_state uca_cam_get_state(struct uca_camera_t *cam);


/*
 * --- virtual methods --------------------------------------------------------
 */

/**
 * Function pointer for camera probing and initialization. 
 *
 * \param[out] cam On success, *cam holds the newly created uca_camera_t
 *   structure.
 *
 * \param[in] grabber Grabber structure to access the cam. Can be NULL to
 *   specify devices without frame grabber access.
 *
 * \return 
 *   UCA_ERR_INIT_NOT_FOUND if camera is not found or could not be initialized
 *
 * \note This is a private function and should be called exclusively by uca_init().
 */
typedef uint32_t (*uca_cam_init) (struct uca_camera_t **cam, struct uca_grabber_t *grabber);

/**
 * \brief Function pointer to free camera resources.
 *
 * \param[in] cam The camera to close.
 *
 * \note This is a private function and should be called exclusively by uca_init().
 */
typedef uint32_t (*uca_cam_destroy) (struct uca_camera_t *cam);

/**
 * Function pointer to set a camera property.
 *
 * \param[in] cam The camera whose properties are to be set.
 *
 * \param[in] property ID of the property as defined in XXX
 *
 * \param[out] data Where to read the property's value from
 *
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera or
 *   UCA_ERR_PROP_VALUE_OUT_OF_RANGE if value cannot be set.
 */
typedef uint32_t (*uca_cam_set_property) (struct uca_camera_t *cam, enum uca_property_ids property, void *data);

/**
 * Function pointer to get a property.
 *
 * \param[in] cam The camera whose properties are to be retrieved.
 *
 * \param[in] property ID of the property as defined in XXX
 *
 * \param[out] data Where to store the property's value
 *
 * \param[in] num Number of bytes of string storage. Ignored for uca_uint8t
 *   and uca_uint32t properties.
 *
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera
 */
typedef uint32_t (*uca_cam_get_property) (struct uca_camera_t *cam, enum uca_property_ids property, void *data, size_t num);

/**
 * Begin recording.
 *
 * Usually this also involves the frame acquisition of the frame grabber but is
 * hidden by this function.
 */
typedef uint32_t (*uca_cam_start_recording) (struct uca_camera_t *cam);

/**
 * Stop recording.
 */
typedef uint32_t (*uca_cam_stop_recording) (struct uca_camera_t *cam);

/**
 * \brief Grab one image from the camera
 * 
 * The grabbing involves a memory copy because we might have to decode the image
 * coming from the camera, which the frame grabber is not able to do.
 *
 * \param[in] buffer Destination buffer
 *
 */
typedef uint32_t (*uca_cam_grab) (struct uca_camera_t *cam, char *buffer);


/**
 * Represents a camera abstraction, that concrete cameras must implement.
 */
struct uca_camera_t {
    /**
     * Points to the next available camera in a linked-list fashion.
     *
     * End of list is specified with next == NULL.
     */
    struct uca_camera_t     *next;

    /* Function pointers to camera-specific methods */
    /**
     * Method to set a property.
     * \see uca_cam_set_property
     */
    uca_cam_set_property    set_property;

    /**
     * Method to get a property.
     * \see uca_cam_get_property
     */
    uca_cam_get_property    get_property;

    /**
     * Method to start recording.
     * \see uca_cam_start_recording
     */
    uca_cam_start_recording start_recording;

    /**
     * Method to stop recording.
     * \see uca_cam_stop_recording
     */
    uca_cam_stop_recording  stop_recording;

    /**
     * Method to grab a frame.
     * \see uca_cam_grab
     */
    uca_cam_grab            grab;

    /* Private */
    /**
     * Method to close the camera.
     * \see uca_cam_destroy
     */
    uca_cam_destroy         destroy;

    struct uca_grabber_t    *grabber;   /**< grabber associated with this camera */
    enum uca_cam_state      state;      /**< camera state */
    uint32_t                frame_width;    /**< current frame width */
    uint32_t                frame_height;   /**< current frame height */
    uint32_t                current_frame;  /**< last grabbed frame number */

    void *user; /**< private user data to be used by the camera driver */
};

#ifdef __cplusplus
}
#endif

#endif
