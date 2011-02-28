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
struct uca_camera_t;
struct uca_property_t;

/**
 * \brief Camera probing and initialization
 * \return UCA_ERR_INIT_NOT_FOUND if camera is not found or could not be initialized
 */
typedef uint32_t (*uca_cam_init) (struct uca_camera_t **cam);

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
typedef uint32_t (*uca_cam_set_property) (struct uca_camera_t *cam, int32_t property, void *data);

/**
 * \brief Set a property
 * \param[in] property_name Name of the property as defined in XXX
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera
 */
typedef uint32_t (*uca_cam_get_property) (struct uca_camera_t *cam, int32_t property, void *data);

/** \brief Allocate number of buffers
 *
 * The size of each buffer is width x height x bits
 */
typedef uint32_t (*uca_cam_alloc) (struct uca_camera_t *cam, uint32_t n_buffers);

/**
 * \brief Acquire one frame
 */
typedef uint32_t (*uca_cam_acquire_image) (struct uca_camera_t *cam, void *buffer);

/**
 * \brief Initialize the unified camera access interface
 * \return Pointer to a uca_t structure
 */
struct uca_t *uca_init(void);

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

struct uca_property_t *uca_get_full_property(int32_t property_id);

#define UCA_NO_ERROR                    0
#define UCA_ERR_INIT_NOT_FOUND          1   /**< camera probing or initialization failed */
#define UCA_ERR_PROP_INVALID            2   /**< the requested property is not supported by the camera */
#define UCA_ERR_PROP_GENERAL            3   /**< error occured reading/writing the property */
#define UCA_ERR_PROP_VALUE_OUT_OF_RANGE 4   /**< error occured writing the property */


/* The property IDs must start with 0 and must be continuous. Whenever this
 * library is released, the IDs must not change to guarantee binary compatibility! */
#define UCA_PROP_NAME           0
#define UCA_PROP_WIDTH          1
#define UCA_PROP_WIDTH_MIN      2
#define UCA_PROP_WIDTH_MAX      3
#define UCA_PROP_HEIGHT         4
#define UCA_PROP_HEIGHT_MIN     5
#define UCA_PROP_HEIGHT_MAX     6
#define UCA_PROP_X_OFFSET       7
#define UCA_PROP_Y_OFFSET       8
#define UCA_PROP_BITDEPTH       9
#define UCA_PROP_EXPOSURE       10
#define UCA_PROP_EXPOSURE_MIN   11
#define UCA_PROP_EXPOSURE_MAX   12
#define UCA_PROP_DELAY          13
#define UCA_PROP_DELAY_MIN      14
#define UCA_PROP_DELAY_MAX      15
#define UCA_PROP_FRAMERATE      16 
#define UCA_PROP_TRIGGER_MODE   17

/* pco.edge specific */
#define UCA_PROP_TIMESTAMP_MODE 18
#define UCA_PROP_SCAN_MODE      19

/* IPE camera specific */
#define UCA_PROP_INTERLACE_SAMPLE_RATE  20
#define UCA_PROP_INTERLACE_PIXEL_THRESH 21
#define UCA_PROP_INTERLACE_ROW_THRESH   22

/* Photon Focus specific */
#define UCA_PROP_CORRECTION_MODE        23

#define UCA_PROP_LAST                   24

/* Possible timestamp modes for UCA_PROP_TIMESTAMP_MODE */
#define UCA_TIMESTAMP_ASCII     0x01
#define UCA_TIMESTAMP_BINARY    0x02

/* Trigger mode for UCA_PROP_TRIGGERMODE */
#define UCA_TRIGGER_AUTO        1
#define UCA_TRIGGER_INTERNAL    2
#define UCA_TRIGGER_EXTERNAL    3

/* Correction modes for UCA_PROP_CORRECTION_MODE */
#define UCA_CORRECT_OFFSET      0x01
#define UCA_CORRECT_HOTPIXEL    0x02
#define UCA_CORRECT_GAIN        0x04

struct uca_property_t {
    const char *name;

    enum uca_unit {
        uca_pixel,
        uca_bits,
        uca_ns,
        uca_us,
        uca_ms,
        uca_s,
        uca_rows,
        uca_na
    } unit;

    enum uca_types {
        uca_uint32t,
        uca_uint8t,
        uca_string
    } type;
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

    void *user; /**< private user data to be used by the camera driver */
};

struct uca_t {
    struct uca_camera_t *cameras;
};


#endif
