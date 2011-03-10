#ifndef __UNIFIED_CAMERA_ACCESS_IPE_H
#define __UNIFIED_CAMERA_ACCESS_IPE_H

struct uca_camera_t;
struct uca_grabber_t;

uint32_t uca_ipe_init(struct uca_camera_t **uca, struct uca_grabber_t *grabber);

#endif
