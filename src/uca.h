#ifndef __UNIFIED_CAMERA_ACCESS_H
#define __UNIFIED_CAMERA_ACCESS_H

struct uca_t;

/*
 * \brief Camera probing and initialization
 * \return 0 if camera is not found or could not be initialized
 */
typedef int (*uca_cam_init) (struct uca_t *uca);

typedef void (*uca_cam_destroy) (struct uca_t *uca);

#define UCA_BIG_ENDIAN      1
#define UCA_LITTLE_ENDIAN   2

struct uca_t {
    /* These must be written by uca_cam_init() */
    unsigned int image_width;
    unsigned int image_height;
    unsigned int image_bitdepth;
    unsigned int image_flags;
    
    /* Function pointers to camera-specific methods */
    uca_cam_destroy cam_destroy;
};

struct uca_t *uca_init();
void uca_destroy(struct uca_t *uca);


#endif
