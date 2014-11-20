/* Copyright (C) 2011, 2012 Karlsruhe Institute of Technology

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

#include <glib-object.h>
#include <gio/gio.h>
#include <gmodule.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "atcore.h"
#include "uca-camera.h"
#include "uca-andor-camera.h"

#define UCA_ANDOR_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_ANDOR_CAMERA, UcaAndorCameraPrivate))

static void uca_andor_initable_iface_init(GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaAndorCamera, uca_andor_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, uca_andor_initable_iface_init))

#define TIMEBASE_INVALID 0xDEAD
#define NUM_BUFFERS             10

struct _UcaAndorCameraPrivate {
    guint camera_number;
    AT_H handle;
    GError *construct_error;

    gchar* name;
    gchar* model;
    guint64 aoi_left;
    guint64 aoi_top;
    guint64 aoi_width;
    guint64 aoi_height;
    guint64 aoi_stride;
    guint64 sensor_width;
    guint64 sensor_height;
    gdouble pixel_width;
    gdouble pixel_height;
    gint trigger_mode;
    gdouble frame_rate;
    gdouble exp_time;
    gdouble sensor_temperature;
    gdouble target_sensor_temperature;
    gint fan_speed;
    gchar* cycle_mode;
    gboolean is_sim_cam;
    gboolean is_cam_acquiring;

    AT_U8* image_buffer;
    AT_U8* aligned_buffer;
    AT_64 image_size;               /* image memory size in bytes */

    int rand;
};

static gint andor_overrideables [] = {
    PROP_NAME,
    PROP_EXPOSURE_TIME,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_PIXEL_WIDTH,
    PROP_SENSOR_PIXEL_HEIGHT,
    PROP_TRIGGER_MODE,
    PROP_IS_RECORDING,
    PROP_SENSOR_BITDEPTH,
    PROP_HAS_CAMRAM_RECORDING,
    PROP_HAS_STREAMING,
    0,
};

enum {
    PROP_ROI_STRIDE = N_BASE_PROPERTIES,
    PROP_SENSOR_TEMPERATURE,
    PROP_TARGET_SENSOR_TEMPERATURE,
    PROP_FAN_SPEED,
    PROP_CYCLE_MODE,
    PROP_FRAMERATE,
    N_PROPERTIES
};

static GParamSpec *andor_properties [N_PROPERTIES] = { NULL, };

GQuark
uca_andor_camera_error_quark (void)
{
    return g_quark_from_static_string ("uca-andor-camera-error-quark");
}

gboolean
check_error (int error_number, const char* message, GError **error)
{
    if (error_number != AT_SUCCESS) {
        g_set_error (error, UCA_ANDOR_CAMERA_ERROR, UCA_ANDOR_CAMERA_ERROR_LIBANDOR_GENERAL,
                     "Andor communication error `%s' (%i)", message, error_number);
        return FALSE;
    }

    return TRUE;
}

static gboolean
write_integer (UcaAndorCameraPrivate *priv, const AT_WC* property, gint64 value)
{
    if (AT_SetInt (priv->handle, property, value) != AT_SUCCESS) {
        g_warning ("Could not write `%s'", (const gchar *) property);
        return FALSE;
    }

    return TRUE;
}

static gboolean
read_integer (UcaAndorCameraPrivate *priv, const AT_WC* property, gint64 *value)
{
    gint64 temp;

    if (AT_GetInt (priv->handle, property, (AT_64 *) &temp) != AT_SUCCESS) {
        g_warning ("Could not read `%s'", (const gchar *) property);
        return FALSE;
    }

    *value = temp;
    return TRUE;
}

static gboolean
write_double (UcaAndorCameraPrivate *priv, const AT_WC* property, double value)
{
    if (AT_SetFloat (priv->handle, property, value) != AT_SUCCESS) {
        g_warning ("Could not write `%s'", (const gchar *) property);
        return FALSE;
    }

    return TRUE;
}

static gboolean
read_double (UcaAndorCameraPrivate *priv, const AT_WC* property, double *value)
{
    double temp;

    if (AT_GetFloat (priv->handle, property, &temp) != AT_SUCCESS) {
        g_warning ("Could not read `%s'", (const gchar *) property);
        return FALSE;
    }

    *value = temp;
    return TRUE;
}

static gboolean
write_enum_index (UcaAndorCameraPrivate *priv, const AT_WC* property, int value)
{
    int count;

    if (AT_GetEnumCount (priv->handle, property, &count) != AT_SUCCESS) {
        g_warning ("Cannot read enum count `%s'", (const gchar *) property);
        return FALSE;
    }

    if (value >= count || value < 0) {
        g_warning ("Enumeration value out of range [0, %i]", count - 1);
        return FALSE;
    }

    if (AT_SetEnumIndex (priv->handle, property, value) != AT_SUCCESS) {
        g_warning ("Could not set enum `%s'", (const gchar *) property);
        return FALSE;
    }

    return TRUE;
}

static gboolean
read_enum_index (UcaAndorCameraPrivate *priv, const AT_WC* property, int *value)
{
    int temp;

    if (AT_GetEnumIndex (priv->handle, property, &temp) != AT_SUCCESS) {
        g_warning ("Could not read `%s'", (const gchar *) property);
        return FALSE;
    }

    *value = temp;
    return TRUE;
}

static gboolean
write_string (UcaAndorCameraPrivate *priv, const AT_WC *property, const gchar *value)
{
    AT_WC* wide_value;
    size_t len;
    gboolean result;

    result = TRUE;
    len = strlen (value);
    wide_value = g_malloc0 ((len + 1) * sizeof (AT_WC));
    mbstowcs (wide_value, value, len);

    if (AT_SetEnumString (priv->handle, property, wide_value) != AT_SUCCESS) {
        g_warning ("Could not write `%s' to `%s'", value, (const gchar *) property);
        result = FALSE;
    }

    g_free (wide_value);
    return result;
}

static gboolean
read_string (UcaAndorCameraPrivate *priv, const AT_WC *property, gchar **value)
{
    AT_WC* wide_value;
    int index;
    gboolean result;

    result = TRUE;

    if (AT_GetEnumIndex (priv->handle, property, &index) != AT_SUCCESS) {
        g_warning ("Could not get index for `%s'", (const gchar *) property);
        return FALSE;
    }

    /* TODO: we should actually get the correct size, but oh well ... */
    wide_value = g_malloc0 (1023 * sizeof (AT_WC));

    if (AT_GetEnumStringByIndex (priv->handle, property, index, wide_value, 1023) != AT_SUCCESS) {
        g_warning ("Could not read string for `%s'", (const gchar *) property);
        result = FALSE;
        goto free_read_string_resources;
    }

    *value = g_malloc0 ((wcslen (wide_value) + 1) * sizeof (gchar));
    wcstombs (*value, wide_value, wcslen (wide_value));

free_read_string_resources:
    g_free (wide_value);
    return result;
}

static void
uca_andor_camera_start_recording (UcaCamera *camera, GError **error)
{
    UcaAndorCameraPrivate *priv;

    g_return_if_fail (UCA_IS_ANDOR_CAMERA(camera));

    priv = UCA_ANDOR_CAMERA_GET_PRIVATE (camera);
    priv->rand = 0;

    AT_GetInt (priv->handle, L"ImageSizeBytes", &priv->image_size);

    if (priv->image_buffer != NULL)
        g_free (priv->image_buffer);

    priv->image_buffer = g_malloc0 ((NUM_BUFFERS * priv->image_size + 8) * sizeof (gchar));
    priv->aligned_buffer = (AT_U8*) (((unsigned long) priv->image_buffer + 7) & ~0x7);

    for (int i = 0; i < NUM_BUFFERS; i++)
        AT_QueueBuffer (priv->handle, priv->aligned_buffer + i * priv->image_size, priv->image_size);

    AT_SetEnumString(priv->handle, L"CycleMode", L"Continuous");

    if (!check_error (AT_Command (priv->handle, L"AcquisitionStart"),
                      "Could not start acquisition", error))
        return;

    check_error (AT_GetBool (priv->handle, L"CameraAcquiring", &priv->is_cam_acquiring),
                 "Could not read CameraAcquiring", error);
}

static void
uca_andor_camera_stop_recording (UcaCamera *camera, GError **error)
{
    UcaAndorCameraPrivate *priv;

    g_return_if_fail (UCA_IS_ANDOR_CAMERA(camera));
    priv = UCA_ANDOR_CAMERA_GET_PRIVATE (camera);

    if (!check_error (AT_Command (priv->handle, L"AcquisitionStop"), "Cannot stop acquisition", error))
        return;

    AT_Flush (priv->handle);

    check_error (AT_GetBool (priv->handle, L"CameraAcquiring", &priv->is_cam_acquiring),
                 "Could not read CameraAcquiring", error);
}

static gboolean
uca_andor_camera_grab (UcaCamera *camera, gpointer data, GError **error)
{
    UcaAndorCameraPrivate *priv;
    AT_U8* buffer;
    int size;
    int error_number;

    g_return_val_if_fail (UCA_IS_ANDOR_CAMERA(camera), FALSE);
    priv = UCA_ANDOR_CAMERA_GET_PRIVATE(camera);

    error_number = AT_WaitBuffer (priv->handle, &buffer, &size, 10000);

    if (!check_error (error_number, "Could not grab frame", error))
        return FALSE;

    g_memmove (data, buffer, priv->image_size);
    AT_QueueBuffer (priv->handle, buffer, priv->image_size);

    return TRUE;
}

static void
uca_andor_camera_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (UCA_IS_ANDOR_CAMERA(object));
    UcaAndorCameraPrivate *priv = UCA_ANDOR_CAMERA_GET_PRIVATE(object);
    guint val_uint = 0;
    double val_double = 0.0;
    gint val_enum = 0;

    switch (property_id) {
        case PROP_EXPOSURE_TIME:
            val_double = g_value_get_double (value);
            if (write_double (priv, L"ExposureTime", val_double))
                priv->exp_time = (gdouble) val_double;
            break;
        case PROP_ROI_WIDTH:
            val_uint = g_value_get_uint (value);
            if (write_integer (priv, L"AOIWidth", val_uint))
                priv->aoi_width = (guint) val_uint;
            break;
        case PROP_ROI_HEIGHT:
            val_uint = g_value_get_uint (value);
            if (write_integer (priv, L"AOIHeight", val_uint))
                priv->aoi_height = (guint) val_uint;
            break;
        case PROP_ROI_X:
            val_uint = g_value_get_uint (value);
            if (write_integer (priv, L"AOILeft", val_uint))
                priv->aoi_left = val_uint;
            break;
        case PROP_ROI_Y:
            val_uint = g_value_get_uint (value);
            if (write_integer (priv, L"AOITop", val_uint))
                priv->aoi_top = val_uint;
            break;
        case PROP_SENSOR_WIDTH:
            val_uint = g_value_get_uint (value);
            if (write_integer (priv, L"SensorWidth", val_uint))
                priv->sensor_width = val_uint;
            break;
        case PROP_SENSOR_HEIGHT:
            val_uint = g_value_get_uint (value);
            if (write_integer (priv, L"SensorHeight", val_uint))
                priv->sensor_height = val_uint;
            break;
        case PROP_SENSOR_PIXEL_WIDTH:
            val_double = g_value_get_double (value);
            if (write_double (priv, L"PixelWidth", val_double))
                priv->pixel_width = val_double;
            break;
        case PROP_SENSOR_PIXEL_HEIGHT:
            val_double = g_value_get_double (value);
            if (write_double (priv, L"PixelHeight", val_double))
                priv->pixel_height = val_double;
            break;
        case PROP_SENSOR_BITDEPTH:
            //placeholder
            break;
        case PROP_TRIGGER_MODE:
            val_enum = g_value_get_enum (value);
            if (write_enum_index (priv, L"TriggerMode", val_enum))
                priv->trigger_mode = val_enum;
            break;
        case PROP_FRAMERATE:
            val_double = g_value_get_double (value);
            if (write_double (priv, L"FrameRate", val_double))
                priv->frame_rate = g_value_get_float (value);
            break;
        case PROP_TARGET_SENSOR_TEMPERATURE:
            val_double = g_value_get_double (value);
            if (write_double (priv, L"TargetSensorTemperature", val_double))
                priv->target_sensor_temperature = val_double;
            break;
        case PROP_FAN_SPEED:
            val_enum = g_value_get_int (value);
            if (write_enum_index (priv, L"FanSpeed", val_enum))
                priv->fan_speed = val_enum;
            break;
        case PROP_CYCLE_MODE:
            if (write_string (priv, L"CycleMode", g_value_get_string (value))) {
                g_free (priv->cycle_mode);
                priv->cycle_mode = g_value_dup_string (value);
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
uca_andor_camera_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (UCA_IS_ANDOR_CAMERA(object));
    UcaAndorCameraPrivate *priv = UCA_ANDOR_CAMERA_GET_PRIVATE(object);
    gint64 val_uint = 0;
    double val_double = 0.0;
    gint val_enum = 0;
    gchar* val_char;

    switch (property_id) {
        case PROP_NAME:
            g_value_set_string (value, priv->name);
            break;
        case PROP_EXPOSURE_TIME:
            if (read_double (priv, L"ExposureTime", &val_double))
                g_value_set_double (value, val_double);
            break;
        case PROP_ROI_WIDTH:
            if (read_integer (priv, L"AOIWidth", &val_uint))
                g_value_set_uint (value, val_uint);
            break;
        case PROP_ROI_HEIGHT:
            if (read_integer (priv, L"AOIHeight", &val_uint))
                g_value_set_uint (value, val_uint);
            break;
        case PROP_ROI_X:
            if (read_integer (priv, L"AOILeft", &val_uint))
                g_value_set_uint (value, val_uint);
            break;
        case PROP_ROI_Y:
            if (read_integer (priv, L"AOITop", &val_uint))
                g_value_set_uint (value, val_uint);
            break;
        case PROP_SENSOR_WIDTH:
            if (read_integer (priv, L"SensorWidth", &val_uint))
                g_value_set_uint (value, val_uint);
            break;
        case PROP_SENSOR_HEIGHT:
            if (read_integer (priv, L"SensorHeight", &val_uint))
                g_value_set_uint (value, val_uint);
            break;
        case PROP_SENSOR_PIXEL_WIDTH:
            if (read_double (priv, L"PixelWidth", &val_double))
                g_value_set_double (value, val_double);
            break;
        case PROP_SENSOR_PIXEL_HEIGHT:
            if (read_double (priv, L"PixelHeight", &val_double))
                g_value_set_double (value, val_double);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint (value, 16);
            break;
        case PROP_TRIGGER_MODE:
            if (read_enum_index (priv, L"TriggerMode", &val_enum))
                g_value_set_enum (value, val_enum);
            break;
        case PROP_ROI_STRIDE:
            if (read_integer (priv, L"AOIStride", &val_uint))
                g_value_set_uint (value, val_uint);
            break;
        case PROP_FRAMERATE:
            if (read_double (priv, L"FrameRate", &val_double))
                g_value_set_float (value, (float) val_double);
            break;
        case PROP_SENSOR_TEMPERATURE:
            if (read_double (priv, L"SensorTemperature", &val_double))
                g_value_set_double (value, val_double);
            break;
        case PROP_TARGET_SENSOR_TEMPERATURE:
            if (read_double (priv, L"TargetSensorTemperature", &val_double))
                g_value_set_double (value, val_double);
            break;
        case PROP_FAN_SPEED:
            if (read_enum_index (priv, L"FanSpeed", &val_enum))
                g_value_set_int (value, val_enum);
            break;
        case PROP_CYCLE_MODE:
            if (read_string (priv, L"CycleMode", &val_char)) {
                g_value_set_string (value, val_char);
                g_free (val_char);
            }
            break;
        case PROP_IS_RECORDING:
            g_value_set_boolean (value, priv->is_cam_acquiring);
            break;
        case PROP_HAS_CAMRAM_RECORDING:
            g_value_set_boolean (value, FALSE);
            break;
        case PROP_HAS_STREAMING:
            g_value_set_boolean (value, TRUE);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static gboolean
ufo_andor_camera_initable_init (GInitable *initable,
                                GCancellable *cancellable,
                                GError **error)
{
    UcaAndorCamera *cam;
    UcaAndorCameraPrivate *priv;

    cam = UCA_ANDOR_CAMERA (initable);
    priv = cam->priv;

    g_return_val_if_fail (UCA_IS_ANDOR_CAMERA(initable), FALSE);

    if (priv->construct_error != NULL) {
        if (error) {
            *error = g_error_copy (priv->construct_error);
        }

        return FALSE;
    }

    return TRUE;
}

static void
uca_andor_initable_iface_init (GInitableIface *iface)
{
    iface->init = ufo_andor_camera_initable_init;
}

static void
uca_andor_camera_finalize (GObject *object)
{
    UcaAndorCameraPrivate *priv;

    g_return_if_fail (UCA_IS_ANDOR_CAMERA(object));
    priv = UCA_ANDOR_CAMERA_GET_PRIVATE(object);

    if (AT_Close (priv->handle) != AT_SUCCESS)
        return;

    if (AT_FinaliseLibrary () != AT_SUCCESS)
        return;

    g_free (priv->image_buffer);
    g_free (priv->model);
    g_free (priv->name);

    if (priv->construct_error != NULL)
        g_clear_error (&(priv->construct_error));

    G_OBJECT_CLASS (uca_andor_camera_parent_class)->finalize (object);
}

static void
uca_andor_camera_trigger (UcaCamera *camera, GError **error)
{
}

static void
uca_andor_camera_class_init (UcaAndorCameraClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS(klass);
    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);

    oclass->set_property = uca_andor_camera_set_property;
    oclass->get_property = uca_andor_camera_get_property;
    oclass->finalize = uca_andor_camera_finalize;

    camera_class->start_recording = uca_andor_camera_start_recording;
    camera_class->stop_recording = uca_andor_camera_stop_recording;
    camera_class->grab = uca_andor_camera_grab;
    camera_class->trigger = uca_andor_camera_trigger;

    for (guint i = 0; andor_overrideables [i] != 0; i++)
        g_object_class_override_property (oclass, andor_overrideables [i], uca_camera_props [andor_overrideables [i]]);

    andor_properties [PROP_ROI_STRIDE] =
        g_param_spec_uint ("roi-stride",
                "ROI Stride",
                "The stride of the region (or area) of interest",
                0,G_MAXINT, 1,
                G_PARAM_READABLE);

    andor_properties [PROP_SENSOR_TEMPERATURE] =
        g_param_spec_double ("sensor-temperature",
                "sensor-temp",
                "The current temperature of the sensor",
                -100.0, 100.0, 20.0,
                G_PARAM_READABLE);

    andor_properties [PROP_TARGET_SENSOR_TEMPERATURE] =
        g_param_spec_double ("target-sensor-temperature",
                "target-sensor-temp",
                "The temperature that is to be reached before acquisition may start",
                -100.0, 100.0, 20.0,
                G_PARAM_READWRITE);

    andor_properties [PROP_FAN_SPEED] =
        g_param_spec_int ("fan-speed",
                "fan-speed",
                "The speed by which the fan is rotating",
                -100, 100, 0,
                G_PARAM_READWRITE);

    andor_properties [PROP_CYCLE_MODE] =
        g_param_spec_string ("cycle-mode",
                "cylce mode",
                "The currently used cycle mode for the acquisition",
                "F",
                G_PARAM_READWRITE);

    andor_properties [PROP_FRAMERATE] =
        g_param_spec_float ("frame-rate",
                "frame rate",
                "The current frame rate of the camera",
                1.0f, 100.0f, 100.0f,
                G_PARAM_READWRITE);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property (oclass, id, andor_properties [id]);

    g_type_class_add_private (klass, sizeof (UcaAndorCameraPrivate));
}

static void
uca_andor_camera_init (UcaAndorCamera *self)
{
    UcaAndorCameraPrivate *priv;
    AT_H handle;
    AT_WC *model;
    AT_BOOL cam_acq;
    GError **error;
    int error_number;

    self->priv = priv = UCA_ANDOR_CAMERA_GET_PRIVATE (self);
    priv->construct_error = NULL;
    priv->image_buffer = NULL;

    error = &(priv->construct_error);
    error_number = AT_InitialiseLibrary ();

    if (!check_error (error_number, "Could not initialize library", error))
        return;

    priv->camera_number = 0;
    priv->is_sim_cam = FALSE;

    error_number = AT_Open (priv->camera_number, &handle);
    if (!check_error (error_number, "Opening Handle", error)) return;

    priv->handle = handle;
    priv->model = g_malloc0 (1023 * sizeof (gchar));
    model = g_malloc0 (1023 * sizeof (AT_WC));

    error_number = AT_GetString (handle, L"CameraModel", model, 1023);

    if (!check_error (error_number, "Cannot read CameraModel", error))
        return;

    gchar* modelchar = g_malloc0 ((wcslen (model) + 1) * sizeof (gchar));
    wcstombs (modelchar, model, wcslen (model));
    strcpy (priv->model, modelchar);

    priv->is_sim_cam = strcmp (modelchar, "SIMCAM CMOS") == 0;

    if (priv->is_sim_cam == FALSE) {
        AT_WC* name = g_malloc0 (1023*sizeof (AT_WC));
        error_number = AT_GetString (handle, L"CameraName", name, 1023);
        priv->name = g_strdup (priv->model);
    }
    else {
        priv->name = g_strdup ("SIMCAM CMOS (model)");
    }

    error_number = AT_GetFloat (handle, L"ExposureTime", &priv->exp_time);
    if (!check_error (error_number, "Cannot read ExposureTime", error)) return;

    error_number = AT_GetInt (handle, L"AOIWidth", (AT_64 *) &priv->aoi_width);
    if (!check_error (error_number, "Cannot read AOIWidth", error)) return;

    error_number = AT_GetInt (handle, L"AOIHeight", (AT_64 *) &priv->aoi_height);
    if (!check_error (error_number, "Cannot read AOIHeight", error)) return;

    error_number = AT_GetInt (handle, L"AOILeft", (AT_64 *) &priv->aoi_left);
    if (!check_error (error_number, "Cannot read AOILeft", error)) return;

    error_number = AT_GetInt (handle, L"AOITop", (AT_64 *) &priv->aoi_top);
    if (!check_error (error_number, "Cannot read AOITop", error)) return;

    error_number = AT_GetInt (handle, L"AOIStride", (AT_64 *) &priv->aoi_stride);
    if (!check_error (error_number, "Cannot read AOIStride", error)) return;

    error_number = AT_GetInt (handle, L"SensorWidth", (AT_64 *) &priv->sensor_width);
    if (!check_error (error_number, "Cannot read SensorWidth", error)) return;

    error_number = AT_GetInt (handle, L"SensorHeight", (AT_64 *) &priv->sensor_height);
    if (!check_error (error_number, "Cannot read SensorHeight", error)) return;

    error_number = AT_GetFloat (handle, L"PixelWidth", &priv->pixel_width);
    if (!check_error (error_number, "Cannot read PixelWidth", error)) return;

    error_number = AT_GetFloat (handle, L"PixelHeight", &priv->pixel_height);
    if (!check_error (error_number, "Cannot read PixelHeight", error)) return;

    error_number = AT_GetEnumIndex (handle, L"TriggerMode", &priv->trigger_mode);
    if (!check_error (error_number, "Cannot read TriggerMode", error)) return;

    error_number = AT_GetFloat (handle, L"FrameRate", &priv->frame_rate);
    if (!check_error (error_number, "Cannot read FrameRate", error)) return;

    error_number = AT_GetFloat (handle, L"SensorTemperature", &priv->sensor_temperature);
    if (!check_error (error_number, "Cannot read SensorTemperature", error)) return;

    error_number = AT_GetFloat (handle, L"TargetSensorTemperature", &priv->target_sensor_temperature);
    if (!check_error (error_number, "Cannot read TargetSensorTemperature", error)) return;

    error_number = AT_GetEnumIndex (handle, L"FanSpeed", &priv->fan_speed);
    if (!check_error (error_number, "Cannot read FanSpeed", error)) return;

    priv->cycle_mode = NULL;

    error_number = AT_GetBool (handle, L"CameraAcquiring", &cam_acq);
    if (!check_error (error_number, "Cannot read CameraAcquiring", error)) return;
    priv->is_cam_acquiring = (gboolean) cam_acq;

    uca_camera_register_unit (UCA_CAMERA (self), "frame-rate", UCA_UNIT_COUNT);

    g_free (model);
    g_free (modelchar);
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_ANDOR_CAMERA;
}
