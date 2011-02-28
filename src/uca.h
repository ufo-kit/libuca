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


#define UCA_NO_ERROR                    0
#define UCA_ERR_INIT_NOT_FOUND          1   /**< camera probing or initialization failed */
#define UCA_ERR_PROP_INVALID            2   /**< the requested property is not supported by the camera */
#define UCA_ERR_PROP_GENERAL            3   /**< error occured reading/writing the property */
#define UCA_ERR_PROP_VALUE_OUT_OF_RANGE 4   /**< error occured writing the property */

struct uca_t {
    struct uca_camera_t *cameras;
    struct uca_grabber_t *grabbers;
};


#endif
