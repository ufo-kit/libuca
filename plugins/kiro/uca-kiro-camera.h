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

#ifndef __UCA_KIRO_CAMERA_H
#define __UCA_KIRO_CAMERA_H

#include <glib-object.h>
#include "uca-camera.h"

G_BEGIN_DECLS

#define UCA_TYPE_KIRO_CAMERA             (uca_kiro_camera_get_type())
#define UCA_KIRO_CAMERA(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_KIRO_CAMERA, UcaKiroCamera))
#define UCA_IS_KIRO_CAMERA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_KIRO_CAMERA))
#define UCA_KIRO_CAMERA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UCA_TYPE_KIRO_CAMERA, UcaKiroCameraClass))
#define UCA_IS_KIRO_CAMERA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_KIRO_CAMERA))
#define UCA_KIRO_CAMERA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_KIRO_CAMERA, UcaKiroCameraClass))

#define UCA_KIRO_CAMERA_ERROR    uca_kiro_camera_error_quark()

GQuark uca_kiro_camera_error_quark(void);

typedef enum {
    UCA_KIRO_CAMERA_ERROR_MISSING_TANGO_ADDRESS = UCA_CAMERA_ERROR_END_OF_STREAM,
    UCA_KIRO_CAMERA_ERROR_TANGO_CONNECTION_FAILED,
    UCA_KIRO_CAMERA_ERROR_KIRO_CONNECTION_FAILED,
    UCA_KIRO_CAMERA_ERROR_TANGO_EXCEPTION_OCCURED,
    UCA_KIRO_CAMERA_ERROR_BAD_CAMERA_INTERFACE
} UcaKiroCameraError;


typedef struct _UcaKiroCamera           UcaKiroCamera;
typedef struct _UcaKiroCameraClass      UcaKiroCameraClass;
typedef struct _UcaKiroCameraPrivate    UcaKiroCameraPrivate;

/**
 * UcaKiroCamera:
 *
 * Creates #UcaKiroCamera instances by loading corresponding shared objects. The
 * contents of the #UcaKiroCamera structure are private and should only be
 * accessed via the provided API.
 */
struct _UcaKiroCamera {
    /*< private >*/
    UcaCamera parent;

    UcaKiroCameraPrivate *priv;
};

/**
 * UcaKiroCameraClass:
 *
 * #UcaKiroCamera class
 */
struct _UcaKiroCameraClass {
    /*< private >*/
    UcaCameraClass parent;
};

G_END_DECLS


void uca_kiro_camera_clone_interface (const gchar* address, UcaKiroCamera *kiro_camera);


#endif
