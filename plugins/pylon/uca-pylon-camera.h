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

#ifndef __UCA_PYLON_CAMERA_H
#define __UCA_PYLON_CAMERA_H

#include <glib-object.h>
#include "uca-camera.h"

G_BEGIN_DECLS

#define UCA_TYPE_PYLON_CAMERA             (uca_pylon_camera_get_type())
#define UCA_PYLON_CAMERA(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_PYLON_CAMERA, UcaPylonCamera))
#define UCA_IS_PYLON_CAMERA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_PYLON_CAMERA))
#define UCA_PYLON_CAMERA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_PYLON_CAMERA, UfoPylonCameraClass))
#define UCA_IS_PYLON_CAMERA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_PYLON_CAMERA))
#define UCA_PYLON_CAMERA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_PYLON_CAMERA, UcaPylonCameraClass))

#define UCA_PYLON_CAMERA_ERROR uca_pylon_camera_error_quark()
typedef enum {
    UCA_PYLON_CAMERA_ERROR_LIBPYLON_INIT,
    UCA_PYLON_CAMERA_ERROR_LIBPYLON_GENERAL,
    UCA_PYLON_CAMERA_ERROR_UNSUPPORTED,
} UcaPylonCameraError;

typedef enum {
    UCA_CAMERA_BALANCE_WHITE_OFF,
    UCA_CAMERA_BALANCE_WHITE_ONCE,
    UCA_CAMERA_BALANCE_WHITE_CONTINUOUSLY
} UcaCameraBalanceWhiteAuto;

typedef enum {
    UCA_CAMERA_EXPOSURE_AUTO_OFF,
    UCA_CAMERA_EXPOSURE_AUTO_ONCE,
    UCA_CAMERA_EXPOSURE_AUTO_CONTINUOUSLY
} UcaCameraExposureAuto;

typedef struct _UcaPylonCamera           UcaPylonCamera;
typedef struct _UcaPylonCameraClass      UcaPylonCameraClass;
typedef struct _UcaPylonCameraPrivate    UcaPylonCameraPrivate;

/**
 * UcaPylonCamera:
 *
 * Creates #UcaPylonCamera instances by loading corresponding shared objects. The
 * contents of the #UcaPylonCamera structure are private and should only be
 * accessed via the provided API.
 */
struct _UcaPylonCamera {
    /*< private >*/
    UcaCamera parent;

    UcaPylonCameraPrivate *priv;
};

/**
 * UcaPylonCameraClass:
 *
 * #UcaPylonCamera class
 */
struct _UcaPylonCameraClass {
    /*< private >*/
    UcaCameraClass parent;
};

GType uca_pylon_camera_get_type(void);

G_END_DECLS

#endif
