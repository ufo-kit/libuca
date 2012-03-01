/* Copyright (C) 2011, 2012 Matthias Vogelgesang <matthias.vogelgesang@kit.edu>
   (Karlsruhe Institute of Technology)

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published by the
   Free Software Foundation; either version 2.1 of the License, or (at your
   option) any later version.

   This library is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
   details.

   You should have received a copy of the GNU Lesser General Public License along
   with this library; if not, write to the Free Software Foundation, Inc., 51
   Franklin St, Fifth Floor, Boston, MA 02110, USA */

#ifndef __UCA_CAMERA_H
#define __UCA_CAMERA_H

#include <glib-object.h>

#define UCA_TYPE_CAMERA             (uca_camera_get_type())
#define UCA_CAMERA(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_CAMERA, UcaCamera))
#define UCA_IS_CAMERA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_CAMERA))
#define UCA_CAMERA_GET_INTERFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE((obj), UCA_TYPE_CAMERA, UcaCameraInterface))

typedef struct _UcaCamera           UcaCamera;
typedef struct _UcaCameraInterface  UcaCameraInterface;

/**
 * UcaCameraInterface:
 *
 * Base interface for cameras.
 */
struct _UcaCameraInterface {
    /*< private >*/
    GTypeInterface parent;

    void (*start_recording) (UcaCamera *camera);
    void (*stop_recording) (UcaCamera *camera);
    void (*grab) (UcaCamera *camera, gchar *data);
};

void uca_camera_start_recording(UcaCamera *camera);
void uca_camera_stop_recording(UcaCamera *camera);
void uca_camera_grab(UcaCamera *camera, gchar *data);

GType uca_camera_get_type(void);

#endif
