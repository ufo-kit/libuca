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

#ifndef __UCA_MOCK_CAMERA_H
#define __UCA_MOCK_CAMERA_H

#include <glib-object.h>
#include "uca-camera.h"

G_BEGIN_DECLS

#define UCA_TYPE_MOCK_CAMERA             (uca_mock_camera_get_type())
#define UCA_MOCK_CAMERA(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_MOCK_CAMERA, UcaMockCamera))
#define UCA_IS_MOCK_CAMERA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_MOCK_CAMERA))
#define UCA_MOCK_CAMERA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_MOCK_CAMERA, UfoMockCameraClass))
#define UCA_IS_MOCK_CAMERA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_MOCK_CAMERA))
#define UCA_MOCK_CAMERA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_MOCK_CAMERA, UcaMockCameraClass))

typedef struct _UcaMockCamera           UcaMockCamera;
typedef struct _UcaMockCameraClass      UcaMockCameraClass;
typedef struct _UcaMockCameraPrivate    UcaMockCameraPrivate;

/**
 * UcaMockCamera:
 *
 * Creates #UcaMockCamera instances by loading corresponding shared objects. The
 * contents of the #UcaMockCamera structure are private and should only be
 * accessed via the provided API.
 */
struct _UcaMockCamera {
    /*< private >*/
    UcaCamera parent;

    UcaMockCameraPrivate *priv;
};

/**
 * UcaMockCameraClass:
 *
 * #UcaMockCamera class
 */
struct _UcaMockCameraClass {
    /*< private >*/
    UcaCameraClass parent;
};

G_END_DECLS

#endif
