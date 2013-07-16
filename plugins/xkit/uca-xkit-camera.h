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

#ifndef __UCA_XKIT_CAMERA_H
#define __UCA_XKIT_CAMERA_H

#include <glib-object.h>
#include "uca-camera.h"

G_BEGIN_DECLS

#define UCA_TYPE_XKIT_CAMERA             (uca_xkit_camera_get_type())
#define UCA_XKIT_CAMERA(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_XKIT_CAMERA, UcaXkitCamera))
#define UCA_IS_XKIT_CAMERA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_XKIT_CAMERA))
#define UCA_XKIT_CAMERA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UCA_TYPE_XKIT_CAMERA, UcaXkitCameraClass))
#define UCA_IS_XKIT_CAMERA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_XKIT_CAMERA))
#define UCA_XKIT_CAMERA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_XKIT_CAMERA, UcaXkitCameraClass))

#define UCA_XKIT_CAMERA_ERROR uca_xkit_camera_error_quark()
typedef enum {
    UCA_XKIT_CAMERA_ERROR_INIT,
    UCA_XKIT_CAMERA_ERROR_START_RECORDING,
    UCA_XKIT_CAMERA_ERROR_STOP_RECORDING,
    UCA_XKIT_CAMERA_ERROR_TRIGGER,
    UCA_XKIT_CAMERA_ERROR_NEXT_EVENT,
    UCA_XKIT_CAMERA_ERROR_NO_DATA,
    UCA_XKIT_CAMERA_ERROR_MAYBE_CORRUPTED
} UcaXkitCameraError;

typedef struct _UcaXkitCamera           UcaXkitCamera;
typedef struct _UcaXkitCameraClass      UcaXkitCameraClass;
typedef struct _UcaXkitCameraPrivate    UcaXkitCameraPrivate;

/**
 * UcaXkitCamera:
 *
 * Creates #UcaXkitCamera instances by loading corresponding shared objects. The
 * contents of the #UcaXkitCamera structure are private and should only be
 * accessed via the provided API.
 */
struct _UcaXkitCamera {
    /*< private >*/
    UcaCamera parent;

    UcaXkitCameraPrivate *priv;
};

/**
 * UcaXkitCameraClass:
 *
 * #UcaXkitCamera class
 */
struct _UcaXkitCameraClass {
    /*< private >*/
    UcaCameraClass parent;
};

GType uca_xkit_camera_get_type(void);

G_END_DECLS

#endif
