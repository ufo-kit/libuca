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

struct uca_t;
struct uca_camera_t;
struct uca_property_t;

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
 * \brief Describe a property used by cameras and frame grabbers
 */
struct uca_property_t {
    const char *name;

    enum uca_unit {
        uca_pixel = 0,
        uca_bits,
        uca_ns,
        uca_us,
        uca_ms,
        uca_s,
        uca_rows,
        uca_fps,
        uca_na
    } unit;

    enum uca_types {
        uca_uint32t,
        uca_uint8t,
        uca_string
    } type;

    enum uca_access_rights {
        uca_read = 0x01,
        uca_write = 0x02,
        uca_readwrite = 0x01 | 0x02
    } access;
};

extern const char *uca_unit_map[];      /**< maps unit numbers to corresponding strings */

enum uca_errors {
    UCA_NO_ERROR = 0,
    UCA_ERR_GRABBER_NOT_FOUND,
    UCA_ERR_CAM_NOT_FOUND,              /**< camera probing or initialization failed */
    UCA_ERR_PROP_INVALID,               /**< the requested property is not supported by the camera */
    UCA_ERR_PROP_GENERAL,               /**< error occured reading/writing the property */
    UCA_ERR_PROP_VALUE_OUT_OF_RANGE,    /**< error occured writing the property */

    UCA_ERR_CAM_ARM,                    /**< camera is not armed */
    UCA_ERR_CAM_RECORD,                 /**< could not record */

    UCA_ERR_GRABBER_ACQUIRE,            /**< grabber couldn't acquire a frame */
    UCA_ERR_GRABBER_NOMEM               /**< no memory was allocated using uca_grabber->alloc() */
};

struct uca_t {
    struct uca_camera_t *cameras;
    struct uca_grabber_t *grabbers;
};


#endif
