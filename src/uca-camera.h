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
#define UCA_CAMERA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UCA_TYPE_CAMERA, UcaCameraClass))
#define UCA_IS_CAMERA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_CAMERA))
#define UCA_CAMERA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_CAMERA, UcaCameraClass))

#define UCA_CAMERA_ERROR uca_camera_error_quark()
typedef enum {
    UCA_CAMERA_ERROR_RECORDING,
    UCA_CAMERA_ERROR_NOT_RECORDING
} UcaCameraError;

typedef struct _UcaCamera           UcaCamera;
typedef struct _UcaCameraClass      UcaCameraClass;
typedef struct _UcaCameraPrivate    UcaCameraPrivate;

struct _UcaCamera {
    /*< private >*/
    GObject parent;

    UcaCameraPrivate *priv;
};

/**
 * UcaCameraInterface:
 *
 * Base interface for cameras.
 */
struct _UcaCameraClass {
    /*< private >*/
    GObjectClass parent;

    void (*start_recording) (UcaCamera *camera, GError **error);
    void (*stop_recording) (UcaCamera *camera, GError **error);
    void (*grab) (UcaCamera *camera, gchar *data, GError **error);

    void (*recording_started) (UcaCamera *camera);
    void (*recording_stopped) (UcaCamera *camera);
};

void uca_camera_start_recording(UcaCamera *camera, GError **error);
void uca_camera_stop_recording(UcaCamera *camera, GError **error);
void uca_camera_grab(UcaCamera *camera, gchar *data, GError **error);

GType uca_camera_get_type(void);

#endif
