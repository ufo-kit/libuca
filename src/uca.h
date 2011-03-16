#ifndef __UNIFIED_CAMERA_ACCESS_H
#define __UNIFIED_CAMERA_ACCESS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file uca.h
 * \brief Abstract camera model
 *
 * The uca_t structure represents a common interface for cameras regardless of
 * their connectivity. Each camera that adheres to this model must provide an
 * initialization function that probes the device and sets all function pointers
 * to their respective implementation.
 */

/**
 * \mainpage
 *
 * \section intro_sec Introduction
 *
 * libuca is a thin wrapper to make different cameras and their access
 * interfaces (via CameraLink, PCIe, Thunderbolt …) accessible in an easy way.
 * It builds support for cameras, when it can find the necessary dependencies,
 * so there is no need to have camera SDKs installed when you don't own a
 * camera. 
 *
 * \section intro_quickstart Quick Start
 *
 * First you would create a new uca_t structure
 *
 * \code struct uca_t *uca = uca_init() \endcode
 *
 * and see if it is not NULL. If it is NULL, no camera or frame grabber was
 * found. If you build with HAVE_DUMMY_CAMERA, there will always be at least the
 * dummy camera available.
 *
 * You can then iterate through all available cameras using
 * 
 * \code
 * struct uca_camera_t *i = uca->cameras;
 * while (i != NULL) {
 *     // do something with i
 *     i = i->next;
 * }
 * \endcode
 *
 * With such a uca_camera_t structure, you can set properties, retrieve
 * properties or start grabbing frames. Be aware, to check bit depth and frame 
 * dimensions in order to allocate enough memory.
 *
 * \section intro_usage Adding new cameras
 *
 * Up to now, new cameras have to be integrated into libuca directly. Later on,
 * we might provide a plugin mechanism. To add a new camera, add
 * cameras/new-cam.c and cameras/new-cam.h to the source tree and change
 * CMakeLists.txt to include these files. Furthermore, if this camera relies on
 * external dependencies, these have to be found first via the CMake system.
 *
 * The new camera must export exactly one function: uca_new_camera_init() which
 * checks if (given the grabber) the camera is available and sets the function
 * pointers to access the camera accordingly.
 */

struct uca_t;
struct uca_camera_t;
struct uca_property_t;

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
    UCA_PROP_X_OFFSET_MIN,
    UCA_PROP_X_OFFSET_MAX,
    UCA_PROP_Y_OFFSET,
    UCA_PROP_Y_OFFSET_MIN,
    UCA_PROP_Y_OFFSET_MAX,
    UCA_PROP_BITDEPTH,
    UCA_PROP_EXPOSURE,
    UCA_PROP_EXPOSURE_MIN,
    UCA_PROP_EXPOSURE_MAX,
    UCA_PROP_DELAY,
    UCA_PROP_DELAY_MIN,
    UCA_PROP_DELAY_MAX,
    UCA_PROP_FRAMERATE,
    UCA_PROP_TEMPERATURE_SENSOR,
    UCA_PROP_TEMPERATURE_CAMERA,
    UCA_PROP_TRIGGER_MODE,
    UCA_PROP_TRIGGER_EXPOSURE,

    UCA_PROP_PGA_GAIN,
    UCA_PROP_PGA_GAIN_MIN,
    UCA_PROP_PGA_GAIN_MAX,
    UCA_PROP_PGA_GAIN_STEPS,
    UCA_PROP_ADC_GAIN,
    UCA_PROP_ADC_GAIN_MIN,
    UCA_PROP_ADC_GAIN_MAX,
    UCA_PROP_ADC_GAIN_STEPS,

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

/**
 * Initialize the unified camera access interface.
 *
 * \param[in] config_filename Configuration file in JSON format for cameras
 *   relying on external calibration data. It is ignored when no JSON parser can
 *   be found at compile time or config_filename is NULL.
 *
 * \return Pointer to a uca_t structure
 */
struct uca_t *uca_init(const char *config_filename);

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



/* Possible timestamp modes for UCA_PROP_TIMESTAMP_MODE */
#define UCA_TIMESTAMP_ASCII     0x01
#define UCA_TIMESTAMP_BINARY    0x02

/* Trigger mode for UCA_PROP_TRIGGER_MODE */
#define UCA_TRIGGER_AUTO        1   /**< free-run mode */
#define UCA_TRIGGER_SOFTWARE    2
#define UCA_TRIGGER_EXTERNAL    3

#define UCA_TRIGGER_EXP_CAMERA  1   /**< camera-controlled exposure time */
#define UCA_TRIGGER_EXP_LEVEL   2   /**< level-controlled (trigger signal) exposure time */

/* Correction modes for UCA_PROP_CORRECTION_MODE */
#define UCA_CORRECT_OFFSET      0x01
#define UCA_CORRECT_HOTPIXEL    0x02
#define UCA_CORRECT_GAIN        0x04


/**
 * A uca_property_t describes a vendor-independent property used by cameras and
 * frame grabbers. It basically consists of a human-readable name, a physical
 * unit, a type and some access rights.
 */
struct uca_property_t {
    /**
     * A human-readable string for this property.
     *
     * A name is defined in a tree-like structure, to build some form of
     * hierarchical namespace. To define a parent-child-relationship a dot '.'
     * is used. For example "image.width.min" might be the name for the minimal
     * acceptable frame width.
     */
    const char *name;

    /**
     * The physical unit of this property.
     *
     * This is important in order to let the camera drivers know, how to convert
     * the values into their respective target value. It is also used for human
     * interfaces.
     */
    enum uca_unit {
        uca_pixel,  /**< number of pixels */
        uca_bits,   /**< number of bits */
        uca_ns,     /**< nanoseconds */
        uca_us,     /**< microseconds */
        uca_ms,     /**< milliseconds */
        uca_s,      /**< seconds */
        uca_rows,   /**< number of rows */
        uca_fps,    /**< frames per second */
        uca_dc,     /**< degree celsius */
        uca_na      /**< no unit available (for example modes) */
    } unit;

    /**
     * The data type of this property.
     *
     * When using uca_cam_set_property() and uca_cam_get_property() this field
     * must be respected and correct data transfered, as the values are
     * interpreted like defined here.
     */
    enum uca_types {
        uca_uint32t,
        uca_uint8t,
        uca_string
    } type;

    /**
     * Access rights determine if uca_cam_get_property() and/or
     * uca_cam_set_property() can be used with this property.
     */
    enum uca_access_rights {
        uca_read = 0x01,                /**< property can be read */
        uca_write = 0x02,               /**< property can be written */
        uca_readwrite = 0x01 | 0x02     /**< property can be read and written */
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

/**
 * Keeps a list of cameras and grabbers.
 */
struct uca_t {
    struct uca_camera_t *cameras;

    /* private */
    struct uca_grabber_t *grabbers;
};

#ifdef __cplusplus
}
#endif

#endif
