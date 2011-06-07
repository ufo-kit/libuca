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
 * The uca_camera_priv_t structure represents a common interface for cameras regardless of
 * their connectivity. Each camera that adheres to this model must provide an
 * initialization function implementing uca_cam_init() that probes the device
 * and sets all function pointers to their respective implementations.
 */

enum uca_property_ids;

/**
 * Describes the current state of the camera.
 */
enum uca_cam_state {
    UCA_CAM_CONFIGURABLE,   /**< Camera can be configured and is not recording */
    UCA_CAM_ARMED,          /**< Camera is ready for recording */
    UCA_CAM_RECORDING,      /**< Camera is currently recording */
};

/*
 * --- non-virtual methods ----------------------------------------------------
 */
typedef uint32_t (*uca_cam_init) (struct uca_camera_priv **cam, struct uca_grabber_priv *grabber);

/**
 * Allocates memory for a new uca_camera_priv structure and initializes all fields to
 * sane values.
 *
 * \return Pointer to block of memory for a uca_camera_priv structure
 *
 * \note This is is a utility function used internally by drivers
 */
struct uca_camera_priv *uca_cam_new(void);

/**
 * Represents a camera abstraction, that concrete cameras must implement.
 */
typedef struct uca_camera_priv {
    /* virtual methods to be overridden by concrete cameras */
    uint32_t (*destroy) (struct uca_camera_priv *cam);
    uint32_t (*set_property) (struct uca_camera_priv *cam, enum uca_property_ids property, void *data);
    uint32_t (*get_property) (struct uca_camera_priv *cam, enum uca_property_ids property, void *data, size_t num);
    uint32_t (*start_recording) (struct uca_camera_priv *cam);
    uint32_t (*stop_recording) (struct uca_camera_priv *cam);
    uint32_t (*trigger) (struct uca_camera_priv *cam);
    uint32_t (*register_callback) (struct uca_camera_priv *cam, uca_cam_grab_callback callback, void *user);
    uint32_t (*grab) (struct uca_camera_priv *cam, char *buffer, void *meta_data);

    struct uca_grabber_priv *grabber;       /**< grabber associated with this camera */
    enum uca_cam_state      state;          /**< camera state handled in uca.c */
    uint32_t                frame_width;    /**< current frame width */
    uint32_t                frame_height;   /**< current frame height */
    uint64_t                current_frame;  /**< last grabbed frame number */

    uca_cam_grab_callback   callback;
    void                    *callback_user; /**< user data for callback */

    void                    *user;          /**< private user data to be used by the camera driver */
} uca_camera_priv_t;


#ifdef __cplusplus
}
#endif

#endif
