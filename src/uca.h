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
 * \brief Set a property
 * \param[in] property_name Name of the property as defined in XXX
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera
 */
typedef uint32_t (*uca_cam_set_property) (struct uca_t *uca, int32_t property, void *data);

/**
 * \brief Set a property
 * \param[in] property_name Name of the property as defined in XXX
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera
 */
typedef uint32_t (*uca_cam_get_property) (struct uca_t *uca, int32_t property, void *data);

/**
 * \brief Acquire one frame
 */
typedef uint32_t (*uca_cam_acquire_image) (struct uca_t *uca, void *buffer);

/**
 * \brief Initialize the unified camera access interface
 * \return Pointer to a uca_t structure
 */
struct uca_t *uca_init();

/**
 * \brief Free resources of the unified camera access interface
 */
void uca_destroy(struct uca_t *uca);

/**
 * \brief Convert a property string to the corresponding ID
 */
int32_t uca_get_property_id(const char *property_name);

/**
 * \brief Convert a property ID to the corresponding string 
 */
const char* uca_get_property_name(int32_t property_id);


#define UCA_NO_ERROR                    0
#define UCA_ERR_INIT_NOT_FOUND          1   /**< camera probing or initialization failed */
#define UCA_ERR_PROP_INVALID            1   /**< the requested property is not supported by the camera */

/* The property IDs must start with 0 and must be continuous. Whenever this
 * library is released, the IDs must not change to guarantee binary compatibility! */
#define UCA_PROP_INVALID        -1
#define UCA_PROP_NAME           0
#define UCA_PROP_WIDTH          1
#define UCA_PROP_HEIGHT         2
#define UCA_PROP_X_OFFSET       3
#define UCA_PROP_Y_OFFSET       4
#define UCA_PROP_MAX_WIDTH      5
#define UCA_PROP_MAX_HEIGHT     6
#define UCA_PROP_BITDEPTH       7
#define UCA_PROP_EXPOSURE       8
#define UCA_PROP_DELAY          9
#define UCA_PROP_FRAMERATE      10 
#define UCA_PROP_TRIGGER_MODE   11

#define UCA_PROP_INTERLACE_SAMPLE_RATE  12
#define UCA_PROP_INTERLACE_PIXEL_THRESH 13
#define UCA_PROP_INTERLACE_ROW_THRESH   14

#define UCA_PROP_CORRECTION_MODE        15

/* Trigger mode for UCA_PROP_TRIGGERMODE */
#define UCA_TRIGGER_AUTO        1
#define UCA_TRIGGER_INTERNAL    2
#define UCA_TRIGGER_EXTERNAL    3

/* Correction modes for UCA_PROP_CORRECTION_MODE */
#define UCA_CORRECT_OFFSET      0x01
#define UCA_CORRECT_HOTPIXEL    0x02
#define UCA_CORRECT_GAIN        0x04

struct uca_t {
    /* Function pointers to camera-specific methods */
    uca_cam_set_property    cam_set_property;
    uca_cam_get_property    cam_get_property;
    uca_cam_acquire_image   cam_acquire_image;

    /* Private */
    uca_cam_destroy         cam_destroy;

    void *user; /**< private user data to be used by the camera driver */
};


#endif
