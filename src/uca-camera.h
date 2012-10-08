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

G_BEGIN_DECLS

#define UCA_TYPE_CAMERA             (uca_camera_get_type())
#define UCA_CAMERA(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_CAMERA, UcaCamera))
#define UCA_IS_CAMERA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_CAMERA))
#define UCA_CAMERA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UCA_TYPE_CAMERA, UcaCameraClass))
#define UCA_IS_CAMERA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_CAMERA))
#define UCA_CAMERA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_CAMERA, UcaCameraClass))

#define UCA_CAMERA_ERROR uca_camera_error_quark()
GQuark uca_camera_error_quark(void);

typedef enum {
    UCA_CAMERA_ERROR_NOT_FOUND,
    UCA_CAMERA_ERROR_RECORDING,
    UCA_CAMERA_ERROR_NOT_RECORDING,
    UCA_CAMERA_ERROR_NO_GRAB_FUNC,
    UCA_CAMERA_ERROR_NOT_IMPLEMENTED
} UcaCameraError;

typedef enum {
    UCA_CAMERA_TRIGGER_AUTO,
    UCA_CAMERA_TRIGGER_INTERNAL,
    UCA_CAMERA_TRIGGER_EXTERNAL
} UcaCameraTrigger;

typedef struct _UcaCamera           UcaCamera;
typedef struct _UcaCameraClass      UcaCameraClass;
typedef struct _UcaCameraPrivate    UcaCameraPrivate;

enum {
    PROP_0 = 0,
    PROP_NAME,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_BITDEPTH,
    PROP_SENSOR_HORIZONTAL_BINNING,
    PROP_SENSOR_HORIZONTAL_BINNINGS,
    PROP_SENSOR_VERTICAL_BINNING,
    PROP_SENSOR_VERTICAL_BINNINGS,
    PROP_SENSOR_MAX_FRAME_RATE,
    PROP_TRIGGER_MODE,
    PROP_EXPOSURE_TIME,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    PROP_ROI_WIDTH_MULTIPLIER,
    PROP_ROI_HEIGHT_MULTIPLIER,
    PROP_HAS_STREAMING,
    PROP_HAS_CAMRAM_RECORDING,

    /* These properties are handled internally */
    PROP_TRANSFER_ASYNCHRONOUSLY,
    PROP_IS_RECORDING,
    PROP_IS_READOUT,
    N_BASE_PROPERTIES
};

extern const gchar *uca_camera_props[N_BASE_PROPERTIES];

/**
 * UcaCameraGrabFunc:
 * @data: a pointer to the raw data
 * @user_data: user data passed to the function
 *
 * A function receiving the data when streaming in asynchronous mode.
 */
typedef void (*UcaCameraGrabFunc) (gpointer data, gpointer user_data);

struct _UcaCamera {
    /*< private >*/
    GObject parent;

    UcaCameraGrabFunc grab_func;
    gpointer user_data;

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
    void (*stop_recording)  (UcaCamera *camera, GError **error);
    void (*start_readout)   (UcaCamera *camera, GError **error);
    void (*trigger)         (UcaCamera *camera, GError **error);
    void (*grab)            (UcaCamera *camera, gpointer *data, GError **error);

    void (*recording_started) (UcaCamera *camera);
    void (*recording_stopped) (UcaCamera *camera);
};

UcaCamera * uca_camera_new              (const gchar        *type,
                                         GError            **error);
void        uca_camera_start_recording  (UcaCamera          *camera,
                                         GError            **error);
void        uca_camera_stop_recording   (UcaCamera          *camera,
                                         GError            **error);
void        uca_camera_start_readout    (UcaCamera          *camera,
                                         GError            **error);
void        uca_camera_trigger          (UcaCamera          *camera,
                                         GError            **error);
void        uca_camera_grab             (UcaCamera          *camera,
                                         gpointer           *data,
                                         GError            **error);
void        uca_camera_set_grab_func    (UcaCamera          *camera,
                                         UcaCameraGrabFunc   func,
                                         gpointer            user_data);

GType uca_camera_get_type(void);

G_END_DECLS

#endif
