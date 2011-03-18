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
/* FIXME: put this into vtable?! */
uint32_t uca_cam_alloc(struct uca_camera *cam, uint32_t n_buffers);


/**
 * Retrieve current state of the camera.
 *
 * \param[in] cam A camera structure.
 *
 * \return A value from the uca_cam_state enum representing the current state of
 *   the camera.
 */
enum uca_cam_state uca_cam_get_state(struct uca_camera *cam);

/**
 * Allocates memory for a new uca_camera structure and initializes all fields to
 * sane values.
 *
 * \return Pointer to block of memory for a uca_camera structure
 *
 * \note This is is a utility function used internally by drivers
 */
struct uca_camera *uca_cam_new(void);

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
typedef uint32_t (*uca_cam_init) (struct uca_camera **cam, struct uca_grabber *grabber);

/**
 * \brief Function pointer to free camera resources.
 *
 * \param[in] cam The camera to close.
 *
 * \note This is a private function and should be called exclusively by uca_init().
 */
typedef uint32_t (*uca_cam_destroy) (struct uca_camera *cam);

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
typedef uint32_t (*uca_cam_set_property) (struct uca_camera *cam, enum uca_property_ids property, void *data);

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
typedef uint32_t (*uca_cam_get_property) (struct uca_camera *cam, enum uca_property_ids property, void *data, size_t num);

/**
 * Begin recording.
 *
 * Usually this also involves the frame acquisition of the frame grabber but is
 * hidden by this function.
 */
typedef uint32_t (*uca_cam_start_recording) (struct uca_camera *cam);

/**
 * Stop recording.
 */
typedef uint32_t (*uca_cam_stop_recording) (struct uca_camera *cam);

/**
 * Function pointer to a grab callback.
 * 
 * Register such a callback function with uca_cam_register_callback() to
 * receive data as soon as it is delivered.
 *
 * \param[in] image_number Current frame number
 *
 * \param[in] buffer Image data
 *
 * \param[in] meta_data Meta data provided by the camera specifying per-frame
 *   data.
 *
 * \param[in] user User data registered in uca_cam_register_callback()
 *
 * \note The meta data parameter is not yet specified but just a place holder.
 */
typedef void (*uca_cam_grab_callback) (uint32_t image_number, void *buffer, void *meta_data, void *user);

/**
 * Register callback for given frame grabber. To actually start receiving
 * frames, call uca_grabber_acquire().
 *
 * \param[in] grabber The grabber for which the callback should be installed
 *
 * \param[in] callback Callback function for when a frame arrived
 *
 * \param[in] user User data that is passed to the callback function
 */
typedef uint32_t (*uca_cam_register_callback) (struct uca_camera *cam, uca_cam_grab_callback callback, void *user);

/**
 * \brief Grab one image from the camera
 * 
 * The grabbing involves a memory copy because we might have to decode the image
 * coming from the camera, which the frame grabber is not able to do.
 *
 * \param[in] buffer Destination buffer
 *
 * \param[in] meta_data Meta data provided by the camera specifying per-frame
 *   data.
 *
 * \note The meta data parameter is not yet specified but just a place holder.
 *
 */
typedef uint32_t (*uca_cam_grab) (struct uca_camera *cam, char *buffer, void *meta_data);


/**
 * Represents a camera abstraction, that concrete cameras must implement.
 */
typedef struct uca_camera {
    /**
     * Points to the next available camera in a linked-list fashion.
     *
     * End of list is specified with next == NULL.
     */
    struct uca_camera     *next;

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

    /**
     * Method to register an frame acquisition callback.
     *
     * \see uca_cam_register_callback
     */
    uca_cam_register_callback   register_callback;

    /* Private */
    /**
     * Method to close the camera.
     * \see uca_cam_destroy
     */
    uca_cam_destroy         destroy;

    /* private */
    struct uca_grabber      *grabber;       /**< grabber associated with this camera */
    enum uca_cam_state      state;          /**< camera state */
    uint32_t                frame_width;    /**< current frame width */
    uint32_t                frame_height;   /**< current frame height */
    uint32_t                current_frame;  /**< last grabbed frame number */

    uca_cam_grab_callback   callback;
    void                    *callback_user; /**< user data for callback */

    void                    *user;          /**< private user data to be used by the camera driver */
} uca_camera_t;


#ifdef __cplusplus
}
#endif

#endif
