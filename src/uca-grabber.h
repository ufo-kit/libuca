#ifndef __UNIFIED_CAMERA_ACCESS_GRABBER_H
#define __UNIFIED_CAMERA_ACCESS_GRABBER_H

/*
 * --- virtual methods --------------------------------------------------------
 */

/**
 * \brief Camera probing and initialization
 * \return UCA_ERR_INIT_NOT_FOUND if camera is not found or could not be initialized
 */
typedef uint32_t (*uca_grabber_init) (struct uca_grabber_t **grabber);

/**
 * \brief Free camera resouces
 */
typedef uint32_t (*uca_grabber_destroy) (struct uca_grabber_t *grabber);

/**
 * \brief Set a property
 * \param[in] property_name Name of the property as defined in XXX
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera or
 * UCA_ERR_PROP_VALUE_OUT_OF_RANGE if value cannot be set.
 */
typedef uint32_t (*uca_grabber_set_property) (struct uca_grabber_t *grabber, enum uca_property_ids property, void *data);

/**
 * \brief Set a property
 * \param[in] property_name Name of the property as defined in XXX
 * \return UCA_ERR_PROP_INVALID if property is not supported on the camera
 */
typedef uint32_t (*uca_grabber_get_property) (struct uca_grabber_t *grabber, enum uca_property_ids property, void *data);


struct uca_grabber_t {
    struct uca_grabber_t    *next;

    /* Function pointers to grabber-specific methods */
    uca_grabber_set_property set_property;
    uca_grabber_get_property get_property;

    /* Private */
    void *user;
};

#endif
