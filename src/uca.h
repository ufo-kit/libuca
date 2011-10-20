#ifndef __UNIFIED_CAMERA_ACCESS_H
#define __UNIFIED_CAMERA_ACCESS_H

#include <stdint.h>

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
    UCA_PROP_BINNING_X,
    UCA_PROP_BINNING_Y,
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

    /* grabber specific */
    UCA_PROP_GRAB_TIMEOUT,
    UCA_PROP_GRAB_SYNCHRONOUS,

    /* pco.edge specific */
    UCA_PROP_TIMESTAMP_MODE,
    UCA_PROP_SCAN_MODE,
    UCA_PROP_HOTPIXEL_CORRECTION,

    /* IPE camera specific */
    UCA_PROP_INTERLACE_SAMPLE_RATE,
    UCA_PROP_INTERLACE_PIXEL_THRESH,
    UCA_PROP_INTERLACE_ROW_THRESH,

    /* Photon Focus specific */
    UCA_PROP_CORRECTION_MODE,

    UCA_PROP_LAST
};

/* Possible timestamp modes for UCA_PROP_TIMESTAMP_MODE */
#define UCA_TIMESTAMP_NONE      0x00
#define UCA_TIMESTAMP_ASCII     0x01
#define UCA_TIMESTAMP_BINARY    0x02

/* Trigger mode for UCA_PROP_TRIGGER_MODE */
#define UCA_TRIGGER_AUTO        0   /**< free-run mode */
#define UCA_TRIGGER_SOFTWARE    1   /**< software trigger via uca_cam_trigger() */
#define UCA_TRIGGER_EXTERNAL    2   /**< external hardware trigger */
#define UCA_TRIGGER_EXTERNAL_EXPOSURE 3     /**< hardware trigger controlling exposure */

#define UCA_TRIGGER_EXP_CAMERA  1   /**< camera-controlled exposure time */
#define UCA_TRIGGER_EXP_LEVEL   2   /**< level-controlled (trigger signal) exposure time */

/* Correction modes for UCA_PROP_CORRECTION_MODE */
#define UCA_CORRECT_OFFSET      0x01
#define UCA_CORRECT_HOTPIXEL    0x02
#define UCA_CORRECT_GAIN        0x04

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
    uca_bool,   /**< 1 or 0 for true and false */
    uca_na      /**< no unit available (for example modes) */
};

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
};

/**
 * Access rights determine if uca_cam_get_property() and/or
 * uca_cam_set_property() can be used with this property.
 */
enum uca_access_rights {
    uca_read = 0x01,                /**< property can be read */
    uca_write = 0x02,               /**< property can be written */
    uca_readwrite = 0x01 | 0x02     /**< property can be read and written */
};

/**
 * Describes the current state of the camera.
 */
enum uca_cam_state {
    UCA_CAM_CONFIGURABLE,   /**< Camera can be configured and is not recording */
    UCA_CAM_ARMED,          /**< Camera is ready for recording */
    UCA_CAM_RECORDING,      /**< Camera is currently recording */
};


/**
 * A uca_property_t describes a vendor-independent property used by cameras and
 * frame grabbers. It basically consists of a human-readable name, a physical
 * unit, a type and some access rights.
 */
typedef struct uca_property {
    /**
     * A human-readable string for this property.
     *
     * A name is defined in a tree-like structure, to build some form of
     * hierarchical namespace. To define a parent-child-relationship a dot '.'
     * is used. For example "image.width.min" might be the name for the minimal
     * acceptable frame width.
     */
    const char *name;

    enum uca_unit unit;
    enum uca_types type;
    enum uca_access_rights access;

} uca_property_t;

union uca_value {
    uint32_t u32;
    uint8_t u8;
    char *string;
};


/**
 * Grab callback.
 * 
 * Register such a callback function with uca_cam_register_callback() to
 * receive data as soon as it is delivered.
 *
 * \param[in] image_number Current frame number
 * \param[in] buffer Image data
 * \param[in] meta_data Meta data provided by the camera specifying per-frame
 *   data.
 * \param[in] user User data registered in uca_cam_register_callback()
 *
 * \note The meta data parameter is not yet specified but just a place holder.
 */
typedef void (*uca_cam_grab_callback) (uint64_t image_number, void *buffer, void *meta_data, void *user);

extern const char *uca_unit_map[];      /**< maps unit numbers to corresponding strings */


/**
 * An error code is a 32 bit integer with the following format (x:y means x bits
 * for purpose y):
 *
 *    [ 31 (MSB) ...                     ... 0 (LSB) ]
 *    [ 4:lvl | 4:rsv | 4:class | 4:source | 16:code ]
 *
 * where
 *
 *  - lvl describes severity such as warning or failure,
 *  - rsv is reserved,
 *  - class describes the general class of the error,
 *  - source describes where the error occured and
 *  - code is the actual error condition
 *
 * UCA_ERR_MASK_*s can be used to mask the desired field of the error code.
 * 
 */

#define UCA_NO_ERROR            0x00000000

#define UCA_ERR_MASK_CODE       0xF000FFFF
#define UCA_ERR_MASK_SOURCE     0x000F0000
#define UCA_ERR_MASK_TYPE       0x00F00000
#define UCA_ERR_MASK_RESRV      0x0F000000
#define UCA_ERR_MASK_LEVEL      0xF0000000

#define UCA_ERR_GRABBER         0x00010000
#define UCA_ERR_CAMERA          0x00020000

#define UCA_ERR_INIT            0x00100000  /**< error during initialization */
#define UCA_ERR_PROP            0x00200000  /**< error while setting/getting property */
#define UCA_ERR_CALLBACK        0x00300000  /**< callback-related errors */
#define UCA_ERR_TRIGGER         0x00400000  /**< errors concerning trigger */
#define UCA_ERR_CONFIGURATION   0x00500000  /**< errors related to configuration steps */

#define UCA_ERR_FAILURE         0x10000000
#define UCA_ERR_WARNING         0x20000000

#define UCA_ERR_UNCLASSIFIED    0x10000001
#define UCA_ERR_NOT_FOUND       0x10000002
#define UCA_ERR_INVALID         0x10000003
#define UCA_ERR_NO_MEMORY       0x10000004
#define UCA_ERR_OUT_OF_RANGE    0x10000005
#define UCA_ERR_ACQUIRE         0x10000006
#define UCA_ERR_IS_RECORDING    0x10000007 /**< error because device is recording */
#define UCA_ERR_NOT_RECORDING   0x10000008
#define UCA_ERR_FRAME_TRANSFER  0x10000009
#define UCA_ERR_ALREADY_REGISTERED 0x1000000A

struct uca_camera_priv;
/**
 * uca_camera is an opaque structure that is only accessed with the uca_cam_*
 * methods.
 */
struct uca_camera {
    struct uca_camera *next;
    struct uca_camera_priv* priv;
};

struct uca_grabber_priv;
struct uca_grabber {
    struct uca_grabber *next;
    struct uca_grabber_priv* priv;
};

/**
 * Keeps a list of cameras and grabbers.
 */
typedef struct uca {
    struct uca_camera *cameras;
    struct uca_grabber *grabbers;
} uca_t;

/**
 * Initialize the unified camera access interface.
 *
 * \param[in] config_filename Configuration file in JSON format for cameras
 *   relying on external calibration data. It is ignored when no JSON parser can
 *   be found at compile time or config_filename is NULL.
 *
 * \return Pointer to a uca structure
 *
 * \note uca_init() is thread-safe if a Pthread-implementation is available.
 */
struct uca *uca_init(const char *config_filename);

/**
 * Free resources of the unified camera access interface
 *
 * \note uca_destroy() is thread-safe if a Pthread-implementation is available.
 */
void uca_destroy(struct uca *u);

/**
 * Convert a property string to the corresponding ID
 */
uint32_t uca_get_property_id(const char *property_name, enum uca_property_ids *prop_id);

/**
 * Convert a property ID to the corresponding string 
 */
const char* uca_get_property_name(enum uca_property_ids property_id);

/**
 * Return the full property structure for a given ID
 */
uca_property_t *uca_get_full_property(enum uca_property_ids property_id);

/**
 * Allocates buffer memory for the internal frame grabber.
 *
 * The allocation is just a hint to the underlying camera driver. It might
 * ignore this or pass this information on to a related frame grabber.
 *
 * \param[in] cam A uca_camera object
 * \param[in] n_buffers Number of sub-buffers with size frame_width*frame_height.
 * \return Error code
 */
uint32_t uca_cam_alloc(struct uca_camera *cam, uint32_t n_buffers);

/**
 * Retrieve current state of the camera.
 *
 * \param[in] cam A uca_camera object
 * \return A value from the uca_cam_state enum representing the current state of
 *   the camera.
 */
enum uca_cam_state uca_cam_get_state(struct uca_camera *cam);


/**
 * Set a camera property.
 *
 * \param[in] cam The camera whose properties are to be set.
 * \param[in] cam A uca_camera object
 * \param[in] property ID of the property as defined in XXX
 * \param[out] data Where to read the property's value from
 *
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera or
 *   UCA_ERR_PROP_VALUE_OUT_OF_RANGE if value cannot be set.
 */
uint32_t uca_cam_set_property(struct uca_camera *cam, enum uca_property_ids property, void *data);

/**
 * Get a property.
 *
 * \param[in] cam A uca_camera object
 * \param[in] property ID of the property as defined in XXX
 * \param[out] data Where to store the property's value
 * \param[in] num Number of bytes of string storage. Ignored for uca_uint8t
 *   and uca_uint32t properties.
 *
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera
 */
uint32_t uca_cam_get_property(struct uca_camera *cam, enum uca_property_ids property, void *data, size_t num);

/**
 * Begin recording.
 *
 * Usually this also involves the frame acquisition of the frame grabber but is
 * hidden by this function.
 *
 * \param[in] cam A uca_camera object
 * \return Error code
 */
uint32_t uca_cam_start_recording(struct uca_camera *cam);

/**
 * Stop recording.
 *
 * \param[in] cam A uca_camera object
 * \return Error code
 */
uint32_t uca_cam_stop_recording(struct uca_camera *cam);

/**
 * Send a software trigger signal to start a sensor read-out.
 *
 * This method is only useful when UCA_PROP_TRIGGER_MODE is set to
 * UCA_TRIGGER_SOFTWARE.
 *
 * \param[in] cam A uca_camera object
 * \return Error code
 */
uint32_t uca_cam_trigger(struct uca_camera *cam);


/**
 * Register callback for given frame grabber. To actually start receiving
 * frames, call uca_grabber_acquire().
 *
 * \param[in] cam A uca_camera object
 * \param[in] callback Callback function for when a frame arrived
 * \param[in] user User data that is passed to the callback function
 * \return Error code
 */
uint32_t uca_cam_register_callback(struct uca_camera *cam, uca_cam_grab_callback callback, void *user);

/**
 * \brief Grab one image from the camera
 * 
 * The grabbing involves a memory copy because we might have to decode the image
 * coming from the camera, which the frame grabber is not able to do.
 *
 * \param[in] cam A uca_camera object
 * \param[in] buffer Destination buffer
 * \param[in] meta_data Meta data provided by the camera specifying per-frame
 *   data.
 * \return Error code
 *
 * \note The meta data parameter is not yet specified but just a place holder.
 *
 */
uint32_t uca_cam_grab(struct uca_camera *cam, char *buffer, void *meta_data);

#define uca_set_void(p, type, value) { *((type *) p) = (type) value; }

#ifdef __cplusplus
}
#endif

#endif
