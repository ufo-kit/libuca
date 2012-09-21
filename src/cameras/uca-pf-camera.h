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

#ifndef __UCA_PF_CAMERA_H
#define __UCA_PF_CAMERA_H

#include <glib-object.h>
#include "uca-camera.h"

G_BEGIN_DECLS

#define UCA_TYPE_PF_CAMERA             (uca_pf_camera_get_type())
#define UCA_PF_CAMERA(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_PF_CAMERA, UcaPfCamera))
#define UCA_IS_PF_CAMERA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_PF_CAMERA))
#define UCA_PF_CAMERA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UCA_TYPE_PF_CAMERA, UcaPfCameraClass))
#define UCA_IS_PF_CAMERA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_PF_CAMERA))
#define UCA_PF_CAMERA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_PF_CAMERA, UcaPfCameraClass))

#define UCA_PF_CAMERA_ERROR uca_pf_camera_error_quark()
typedef enum {
    UCA_PF_CAMERA_ERROR_INIT,
    UCA_PF_CAMERA_ERROR_FG_GENERAL,
    UCA_PF_CAMERA_ERROR_FG_ACQUISITION,
    UCA_PF_CAMERA_ERROR_START_RECORDING,
    UCA_PF_CAMERA_ERROR_STOP_RECORDING
} UcaPfCameraError;

typedef struct _UcaPfCamera           UcaPfCamera;
typedef struct _UcaPfCameraClass      UcaPfCameraClass;
typedef struct _UcaPfCameraPrivate    UcaPfCameraPrivate;

/**
 * UcaPfCamera:
 *
 * Creates #UcaPfCamera instances by loading corresponding shared objects. The
 * contents of the #UcaPfCamera structure are private and should only be
 * accessed via the provided API.
 */
struct _UcaPfCamera {
    /*< private >*/
    UcaCamera parent;

    UcaPfCameraPrivate *priv;
};

/**
 * UcaPfCameraClass:
 *
 * #UcaPfCamera class
 */
struct _UcaPfCameraClass {
    /*< private >*/
    UcaCameraClass parent;
};

GType uca_pf_camera_get_type(void);

G_END_DECLS

#endif
