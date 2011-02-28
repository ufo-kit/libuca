#ifndef __UNIFIED_CAMERA_ACCESS_CAM_H
#define __UNIFIED_CAMERA_ACCESS_CAM_H

#include <stdint.h>

struct uca_camera_t;
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

/**
 * \brief Convert a property string to the corresponding ID
 */
enum uca_property_ids uca_get_property_id(const char *property_name);

/**
 * \brief Convert a property ID to the corresponding string 
 */
const char* uca_get_property_name(enum uca_property_ids property_id);

/**
 * \brief Return the full property structure for a given ID
 */
struct uca_property_t *uca_get_full_property(enum uca_property_ids property_id);


/* The property IDs must start with 0 and must be continuous. Whenever this
 * library is released, the IDs must not change to guarantee binary compatibility! */
enum uca_property_ids {
    UCA_PROP_NAME = 0,
    UCA_PROP_WIDTH,
    UCA_PROP_WIDTH_MIN,
    UCA_PROP_WIDTH_MAX,
    UCA_PROP_HEIGHT,
    UCA_PROP_HEIGHT_MIN,
    UCA_PROP_HEIGHT_MAX,
    UCA_PROP_X_OFFSET,
    UCA_PROP_Y_OFFSET,
    UCA_PROP_BITDEPTH,
    UCA_PROP_EXPOSURE,
    UCA_PROP_EXPOSURE_MIN,
    UCA_PROP_EXPOSURE_MAX,
    UCA_PROP_DELAY,
    UCA_PROP_DELAY_MIN,
    UCA_PROP_DELAY_MAX,
    UCA_PROP_FRAMERATE,
    UCA_PROP_TRIGGER_MODE,

    /* pco.edge specific */
    UCA_PROP_TIMESTAMP_MODE,
    UCA_PROP_SCAN_MODE,

    /* IPE camera specific */
    UCA_PROP_INTERLACE_SAMPLE_RATE,
    UCA_PROP_INTERLACE_PIXEL_THRESH,
    UCA_PROP_INTERLACE_ROW_THRESH,

    /* Photon Focus specific */
    UCA_PROP_CORRECTION_MODE,

    UCA_PROP_LAST
};

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

/**
 * \brief Describe a camera property
 */
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
    enum uca_cam_state      state;

    void *user; /**< private user data to be used by the camera driver */
};

#endif
