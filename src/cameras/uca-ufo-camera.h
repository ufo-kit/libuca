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

#ifndef __UCA_UFO_CAMERA_H
#define __UCA_UFO_CAMERA_H

#include <glib-object.h>
#include "uca-camera.h"

G_BEGIN_DECLS

#define UCA_TYPE_UFO_CAMERA             (uca_ufo_camera_get_type())
#define UCA_UFO_CAMERA(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_UFO_CAMERA, UcaUfoCamera))
#define UCA_IS_UFO_CAMERA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_UFO_CAMERA))
#define UCA_UFO_CAMERA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UCA_TYPE_UFO_CAMERA, UcaUfoCameraClass))
#define UCA_IS_UFO_CAMERA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_UFO_CAMERA))
#define UCA_UFO_CAMERA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_UFO_CAMERA, UcaUfoCameraClass))

#define UCA_UFO_CAMERA_ERROR uca_ufo_camera_error_quark()
typedef enum {
    UCA_UFO_CAMERA_ERROR_INIT,
    UCA_UFO_CAMERA_ERROR_START_RECORDING,
    UCA_UFO_CAMERA_ERROR_STOP_RECORDING,
    UCA_UFO_CAMERA_ERROR_TRIGGER,
    UCA_UFO_CAMERA_ERROR_NEXT_EVENT,
    UCA_UFO_CAMERA_ERROR_NO_DATA,
    UCA_UFO_CAMERA_ERROR_MAYBE_CORRUPTED
} UcaUfoCameraError;

typedef struct _UcaUfoCamera           UcaUfoCamera;
typedef struct _UcaUfoCameraClass      UcaUfoCameraClass;
typedef struct _UcaUfoCameraPrivate    UcaUfoCameraPrivate;

/**
 * UcaUfoCamera:
 *
 * Creates #UcaUfoCamera instances by loading corresponding shared objects. The
 * contents of the #UcaUfoCamera structure are private and should only be
 * accessed via the provided API.
 */
struct _UcaUfoCamera {
    /*< private >*/
    UcaCamera parent;

    UcaUfoCameraPrivate *priv;
};

/**
 * UcaUfoCameraClass:
 *
 * #UcaUfoCamera class
 */
struct _UcaUfoCameraClass {
    /*< private >*/
    UcaCameraClass parent;
};

UcaUfoCamera *uca_ufo_camera_new(GError **error);

GType uca_ufo_camera_get_type(void);

G_END_DECLS

#endif
