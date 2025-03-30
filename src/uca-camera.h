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
#include "uca-api.h"

G_BEGIN_DECLS

#define UCA_TYPE_CAMERA             (uca_camera_get_type())
#define UCA_CAMERA(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_CAMERA, UcaCamera))
#define UCA_IS_CAMERA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_CAMERA))
#define UCA_CAMERA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UCA_TYPE_CAMERA, UcaCameraClass))
#define UCA_IS_CAMERA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_CAMERA))
#define UCA_CAMERA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_CAMERA, UcaCameraClass))

#define UCA_CAMERA_ERROR    uca_camera_error_quark()
#define UCA_UNIT_QUARK      uca_unit_quark()
#define UCA_WRITABLE_QUARK  uca_writable_quark()

UCA_API GQuark uca_camera_error_quark(void);
UCA_API GQuark uca_unit_quark(void);
UCA_API GQuark uca_writable_quark(void);

typedef enum {
    UCA_CAMERA_ERROR_NOT_FOUND,
    UCA_CAMERA_ERROR_RECORDING,
    UCA_CAMERA_ERROR_NOT_RECORDING,
    UCA_CAMERA_ERROR_NO_GRAB_FUNC,
    UCA_CAMERA_ERROR_NOT_IMPLEMENTED,
    UCA_CAMERA_ERROR_WRONG_WRITE_METADATA,
    UCA_CAMERA_ERROR_END_OF_STREAM,
    UCA_CAMERA_ERROR_TIMEOUT,
    UCA_CAMERA_ERROR_DEVICE,
} UcaCameraError;

typedef enum {
    UCA_CAMERA_TRIGGER_SOURCE_AUTO,
    UCA_CAMERA_TRIGGER_SOURCE_SOFTWARE,
    UCA_CAMERA_TRIGGER_SOURCE_EXTERNAL
} UcaCameraTriggerSource;

typedef enum {
    UCA_CAMERA_TRIGGER_TYPE_EDGE,
    UCA_CAMERA_TRIGGER_TYPE_LEVEL
} UcaCameraTriggerType;

typedef enum {
    UCA_UNIT_NA = 0,
    UCA_UNIT_METER,
    UCA_UNIT_SECOND,
    UCA_UNIT_PIXEL,
    UCA_UNIT_DEGREE_CELSIUS,
    UCA_UNIT_COUNT
} UcaUnit;

typedef struct _UcaCamera           UcaCamera;
typedef struct _UcaCameraClass      UcaCameraClass;
typedef struct _UcaCameraPrivate    UcaCameraPrivate;

enum {
    PROP_0 = 0,
    PROP_NAME,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_PIXEL_WIDTH,
    PROP_SENSOR_PIXEL_HEIGHT,
    PROP_SENSOR_BITDEPTH,
    PROP_SENSOR_HORIZONTAL_BINNING,
    PROP_SENSOR_VERTICAL_BINNING,
    PROP_TRIGGER_SOURCE,
    PROP_TRIGGER_TYPE,
    PROP_EXPOSURE_TIME,
    PROP_FRAMES_PER_SECOND,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    PROP_ROI_WIDTH_MULTIPLIER,
    PROP_ROI_HEIGHT_MULTIPLIER,
    PROP_HAS_STREAMING,
    PROP_HAS_CAMRAM_RECORDING,
    PROP_RECORDED_FRAMES,

    /* These properties are handled internally */
    PROP_TRANSFER_ASYNCHRONOUSLY,
    PROP_IS_RECORDING,
    PROP_IS_READOUT,

    PROP_BUFFERED,
    PROP_NUM_BUFFERS,
    PROP_MIRROR,
    PROP_ROTATE,
    N_BASE_PROPERTIES
};

UCA_API extern const gchar *uca_camera_props[N_BASE_PROPERTIES];

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
    void (*stop_readout)    (UcaCamera *camera, GError **error);
    void (*trigger)         (UcaCamera *camera, GError **error);
    void (*write)           (UcaCamera *camera, const gchar *name, gpointer data, gsize size, GError **error);
    gboolean (*grab)        (UcaCamera *camera, gpointer data, GError **error);
    gboolean (*readout)     (UcaCamera *camera, gpointer data, guint index, GError **error);
};

UCA_API UcaCamera * uca_camera_new      (const gchar        *type,
                                         GError            **error);
UCA_API gboolean    uca_camera_parse_arg_props
                                        (UcaCamera          *camera,
                                         gchar             **argv,
                                         guint               argc,
                                         GError            **error);
UCA_API void        uca_camera_start_recording
                                        (UcaCamera          *camera,
                                         GError            **error);
UCA_API void        uca_camera_stop_recording
                                        (UcaCamera          *camera,
                                         GError            **error);
UCA_API gboolean    uca_camera_is_recording
                                        (UcaCamera          *camera);
UCA_API gboolean    uca_camera_stopped_recording
                                        (UcaCamera          *camera);
UCA_API void        uca_camera_start_readout
                                        (UcaCamera          *camera,
                                         GError            **error);
UCA_API void        uca_camera_stop_readout
                                        (UcaCamera          *camera,
                                         GError            **error);
UCA_API void        uca_camera_trigger  (UcaCamera          *camera,
                                         GError            **error);
UCA_API void        uca_camera_write    (UcaCamera          *camera,
                                         const gchar        *name,
                                         gpointer            data,
                                         gsize               size,
                                         GError            **error);
UCA_API gboolean    uca_camera_grab     (UcaCamera          *camera,
                                         gpointer            data,
                                         GError            **error);
UCA_API gboolean    uca_camera_readout  (UcaCamera          *camera,
                                         gpointer            data,
                                         guint               index,
                                         GError            **error);
UCA_API void        uca_camera_set_grab_func
                                        (UcaCamera          *camera,
                                         UcaCameraGrabFunc   func,
                                         gpointer            user_data);
UCA_API void        uca_camera_register_unit
                                        (UcaCamera          *camera,
                                         const gchar        *prop_name,
                                         UcaUnit             unit);
UCA_API UcaUnit     uca_camera_get_unit (UcaCamera          *camera,
                                         const gchar        *prop_name);
UCA_API void        uca_camera_set_writable
                                        (UcaCamera          *camera,
                                         const gchar        *prop_name,
                                         gboolean            writable);
UCA_API void        uca_camera_pspec_set_writable
                                        (GParamSpec         *pspec,
                                         gboolean            writable);
UCA_API gboolean    uca_camera_is_writable_during_acquisition
                                        (UcaCamera          *camera,
                                         const gchar        *prop_name);
UCA_API GType       uca_camera_get_type (void);

G_END_DECLS

#endif
