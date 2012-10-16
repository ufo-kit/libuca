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

/**
 * SECTION:uca-camera
 * @Short_description: Base class representing a camera
 * @Title: UcaCamera
 *
 * UcaCamera is the base camera from which a real hardware camera derives from.
 */

#include <glib.h>
#include "config.h"
#include "uca-camera.h"
#include "uca-enums.h"

#define UCA_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_CAMERA, UcaCameraPrivate))

G_DEFINE_TYPE(UcaCamera, uca_camera, G_TYPE_OBJECT)

/**
 * UcaCameraTrigger:
 * @UCA_CAMERA_TRIGGER_AUTO: Trigger automatically
 * @UCA_CAMERA_TRIGGER_EXTERNAL: Trigger from an external source
 * @UCA_CAMERA_TRIGGER_SOFTWARE: Trigger from software using
 *      #uca_camera_trigger
 */

/**
 * UcaCameraError:
 * @UCA_CAMERA_ERROR_NOT_FOUND: Camera type is unknown
 * @UCA_CAMERA_ERROR_RECORDING: Camera is already recording
 * @UCA_CAMERA_ERROR_NOT_RECORDING: Camera is not recording
 * @UCA_CAMERA_ERROR_NO_GRAB_FUNC: No grab callback was set
 * @UCA_CAMERA_ERROR_NOT_IMPLEMENTED: Virtual function is not implemented
 */
GQuark uca_camera_error_quark()
{
    return g_quark_from_static_string ("uca-camera-error-quark");
}

/**
 * UcaUnit:
 * @UCA_UNIT_NA: Not applicable
 * @UCA_UNIT_METER: Length in SI meter
 * @UCA_UNIT_SECOND: Time in SI second
 * @UCA_UNIT_PIXEL: Number of pixels in one dimension
 * @UCA_UNIT_DEGREE_CELSIUS: Temperature in degree Celsius
 * @UCA_UNIT_COUNT: Generic number
 *
 * Units should be registered by camera implementations using
 * uca_camera_register_unit() and can be queried by client programs with
 * uca_camera_get_unit().
 */

GQuark uca_unit_quark ()
{
    return g_quark_from_static_string ("uca-unit-quark");
}

enum {
    LAST_SIGNAL
};

/*
 * These strings must UNDER ALL CIRCUMSTANCES match the property id's in the
 * public header! Everyone relies on this relationship.
 */
const gchar *uca_camera_props[N_BASE_PROPERTIES] = {
    NULL,
    "name",
    "sensor-width",
    "sensor-height",
    "sensor-bitdepth",
    "sensor-horizontal-binning",
    "sensor-horizontal-binnings",
    "sensor-vertical-binning",
    "sensor-vertical-binnings",
    "sensor-max-frame-rate",
    "trigger-mode",
    "exposure-time",
    "frames-per-second",
    "roi-x0",
    "roi-y0",
    "roi-width",
    "roi-height",
    "roi-width-multiplier",
    "roi-height-multiplier",
    "has-streaming",
    "has-camram-recording",
    "transfer-asynchronously",
    "is-recording",
    "is-readout"
};

static GParamSpec *camera_properties[N_BASE_PROPERTIES] = { NULL, };

struct _UcaCameraPrivate {
    gboolean is_recording;
    gboolean is_readout;
    gboolean transfer_async;
};

static void
uca_camera_set_property_unit (GParamSpec *pspec, UcaUnit unit)
{
    g_param_spec_set_qdata (pspec, UCA_UNIT_QUARK, GINT_TO_POINTER (unit));
}

static void
uca_camera_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    UcaCameraPrivate *priv = UCA_CAMERA_GET_PRIVATE(object);

    if (priv->is_recording) {
        g_warning("You cannot change properties during data acquisition");
        return;
    }

    switch (property_id) {
        case PROP_TRANSFER_ASYNCHRONOUSLY:
            priv->transfer_async = g_value_get_boolean(value);
            break;

        case PROP_FRAMES_PER_SECOND:
            {
                gdouble frames_per_second;

                frames_per_second = g_value_get_double (value);
                g_object_set (object, "exposure-time", 1. / frames_per_second, NULL);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
uca_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaCameraPrivate *priv = UCA_CAMERA_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_IS_RECORDING:
            g_value_set_boolean (value, priv->is_recording);
            break;

        case PROP_IS_READOUT:
            g_value_set_boolean (value, priv->is_readout);
            break;

        case PROP_TRANSFER_ASYNCHRONOUSLY:
            g_value_set_boolean (value, priv->transfer_async);
            break;

        case PROP_TRIGGER_MODE:
            g_value_set_enum (value, UCA_CAMERA_TRIGGER_AUTO);
            break;

        case PROP_FRAMES_PER_SECOND:
            {
                gdouble exposure_time;

                g_object_get (object, "exposure-time", &exposure_time, NULL);
                g_value_set_double (value, 1. / exposure_time);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
uca_camera_finalize (GObject *object)
{
    G_OBJECT_CLASS (uca_camera_parent_class)->finalize (object);
}

static void
uca_camera_class_init (UcaCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_camera_set_property;
    gobject_class->get_property = uca_camera_get_property;
    gobject_class->finalize = uca_camera_finalize;

    klass->start_recording = NULL;
    klass->stop_recording = NULL;
    klass->grab = NULL;

    camera_properties[PROP_NAME] =
        g_param_spec_string("name",
            "Name of the camera",
            "Name of the camera",
            "", G_PARAM_READABLE);

    camera_properties[PROP_SENSOR_WIDTH] =
        g_param_spec_uint(uca_camera_props[PROP_SENSOR_WIDTH],
            "Width of sensor",
            "Width of the sensor in pixels",
            1, G_MAXUINT, 1,
            G_PARAM_READABLE);

    camera_properties[PROP_SENSOR_HEIGHT] =
        g_param_spec_uint(uca_camera_props[PROP_SENSOR_HEIGHT],
            "Height of sensor",
            "Height of the sensor in pixels",
            1, G_MAXUINT, 1,
            G_PARAM_READABLE);

    camera_properties[PROP_SENSOR_BITDEPTH] =
        g_param_spec_uint(uca_camera_props[PROP_SENSOR_BITDEPTH],
            "Number of bits per pixel",
            "Number of bits per pixel",
            1, 32, 1,
            G_PARAM_READABLE);

    camera_properties[PROP_SENSOR_HORIZONTAL_BINNING] =
        g_param_spec_uint(uca_camera_props[PROP_SENSOR_HORIZONTAL_BINNING],
            "Horizontal binning",
            "Number of sensor ADCs that are combined to one pixel in horizontal direction",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    camera_properties[PROP_SENSOR_HORIZONTAL_BINNINGS] =
        g_param_spec_value_array(uca_camera_props[PROP_SENSOR_HORIZONTAL_BINNINGS],
            "Array of possible binnings",
            "Array of possible binnings in horizontal direction",
            g_param_spec_uint(
                uca_camera_props[PROP_SENSOR_HORIZONTAL_BINNING],
                "Number of ADCs",
                "Number of ADCs that make up one pixel",
                1, G_MAXUINT, 1,
                G_PARAM_READABLE), G_PARAM_READABLE);

    camera_properties[PROP_SENSOR_VERTICAL_BINNING] =
        g_param_spec_uint(uca_camera_props[PROP_SENSOR_VERTICAL_BINNING],
            "Vertical binning",
            "Number of sensor ADCs that are combined to one pixel in vertical direction",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    camera_properties[PROP_SENSOR_VERTICAL_BINNINGS] =
        g_param_spec_value_array(uca_camera_props[PROP_SENSOR_VERTICAL_BINNINGS],
            "Array of possible binnings",
            "Array of possible binnings in vertical direction",
            g_param_spec_uint(
                uca_camera_props[PROP_SENSOR_VERTICAL_BINNING],
                "Number of ADCs",
                "Number of ADCs that make up one pixel",
                1, G_MAXUINT, 1,
                G_PARAM_READABLE), G_PARAM_READABLE);

    camera_properties[PROP_SENSOR_MAX_FRAME_RATE] =
        g_param_spec_float(uca_camera_props[PROP_SENSOR_MAX_FRAME_RATE],
            "Maximum frame rate",
            "Maximum frame rate at full frame resolution",
            0.0f, G_MAXFLOAT, 1.0f,
            G_PARAM_READABLE);

    camera_properties[PROP_TRIGGER_MODE] =
        g_param_spec_enum("trigger-mode",
            "Trigger mode",
            "Trigger mode",
            UCA_TYPE_CAMERA_TRIGGER, UCA_CAMERA_TRIGGER_AUTO,
            G_PARAM_READWRITE);

    camera_properties[PROP_ROI_X] =
        g_param_spec_uint(uca_camera_props[PROP_ROI_X],
            "Horizontal coordinate",
            "Horizontal coordinate",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    camera_properties[PROP_ROI_Y] =
        g_param_spec_uint(uca_camera_props[PROP_ROI_Y],
            "Vertical coordinate",
            "Vertical coordinate",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    camera_properties[PROP_ROI_WIDTH] =
        g_param_spec_uint(uca_camera_props[PROP_ROI_WIDTH],
            "Width",
            "Width of the region of interest",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    camera_properties[PROP_ROI_HEIGHT] =
        g_param_spec_uint(uca_camera_props[PROP_ROI_HEIGHT],
            "Height",
            "Height of the region of interest",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    camera_properties[PROP_ROI_WIDTH_MULTIPLIER] =
        g_param_spec_uint(uca_camera_props[PROP_ROI_WIDTH_MULTIPLIER],
            "Horizontal ROI multiplier",
            "Minimum possible step size of horizontal ROI",
            1, G_MAXUINT, 1,
            G_PARAM_READABLE);

    camera_properties[PROP_ROI_HEIGHT_MULTIPLIER] =
        g_param_spec_uint(uca_camera_props[PROP_ROI_HEIGHT_MULTIPLIER],
            "Vertical ROI multiplier",
            "Minimum possible step size of vertical ROI",
            1, G_MAXUINT, 1,
            G_PARAM_READABLE);

    camera_properties[PROP_EXPOSURE_TIME] =
        g_param_spec_double(uca_camera_props[PROP_EXPOSURE_TIME],
            "Exposure time in seconds",
            "Exposure time in seconds",
            0.0, G_MAXDOUBLE, 1.0,
            G_PARAM_READWRITE);

    camera_properties[PROP_FRAMES_PER_SECOND] =
        g_param_spec_double(uca_camera_props[PROP_FRAMES_PER_SECOND],
            "Frames per second",
            "Frames per second",
            0.0, G_MAXDOUBLE, 1.0,
            G_PARAM_READWRITE);

    camera_properties[PROP_HAS_STREAMING] =
        g_param_spec_boolean(uca_camera_props[PROP_HAS_STREAMING],
            "Streaming capability",
            "Is the camera able to stream the data",
            TRUE, G_PARAM_READABLE);

    camera_properties[PROP_HAS_CAMRAM_RECORDING] =
        g_param_spec_boolean(uca_camera_props[PROP_HAS_CAMRAM_RECORDING],
            "Cam-RAM capability",
            "Is the camera able to record the data in-camera",
            FALSE, G_PARAM_READABLE);

    camera_properties[PROP_TRANSFER_ASYNCHRONOUSLY] =
        g_param_spec_boolean(uca_camera_props[PROP_TRANSFER_ASYNCHRONOUSLY],
            "Specify whether data should be transfered asynchronously",
            "Specify whether data should be transfered asynchronously using a specified callback",
            FALSE, G_PARAM_READWRITE);

    camera_properties[PROP_IS_RECORDING] =
        g_param_spec_boolean(uca_camera_props[PROP_IS_RECORDING],
            "Is camera recording",
            "Is the camera currently recording",
            FALSE, G_PARAM_READABLE);

    camera_properties[PROP_IS_READOUT] =
        g_param_spec_boolean(uca_camera_props[PROP_IS_READOUT],
            "Is camera in readout mode",
            "Is camera in readout mode",
            FALSE, G_PARAM_READABLE);

    for (guint id = PROP_0 + 1; id < N_BASE_PROPERTIES; id++)
        g_object_class_install_property(gobject_class, id, camera_properties[id]);

    g_type_class_add_private(klass, sizeof(UcaCameraPrivate));
}

static void
uca_camera_init (UcaCamera *camera)
{
    camera->grab_func = NULL;

    camera->priv = UCA_CAMERA_GET_PRIVATE(camera);
    camera->priv->is_recording = FALSE;
    camera->priv->is_readout = FALSE;
    camera->priv->transfer_async = FALSE;

    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_WIDTH], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_HEIGHT], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_BITDEPTH], UCA_UNIT_COUNT);
    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_HORIZONTAL_BINNING], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_VERTICAL_BINNING], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_MAX_FRAME_RATE], UCA_UNIT_COUNT);
    uca_camera_set_property_unit (camera_properties[PROP_EXPOSURE_TIME], UCA_UNIT_SECOND);
    uca_camera_set_property_unit (camera_properties[PROP_FRAMES_PER_SECOND], UCA_UNIT_COUNT);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_X], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_Y], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_WIDTH], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_HEIGHT], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_WIDTH_MULTIPLIER], UCA_UNIT_COUNT);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_HEIGHT_MULTIPLIER], UCA_UNIT_COUNT);
}

/**
 * uca_camera_start_recording:
 * @camera: A #UcaCamera object
 * @error: Location to store a #UcaCameraError error or %NULL
 *
 * Initiate recording of frames. If UcaCamera:grab-asynchronous is %TRUE and a
 * #UcaCameraGrabFunc callback is set, frames are automatically transfered to
 * the client program, otherwise you must use uca_camera_grab().
 */
void
uca_camera_start_recording (UcaCamera *camera, GError **error)
{
    UcaCameraClass *klass;
    GError *tmp_error = NULL;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_if_fail (UCA_IS_CAMERA (camera));

    klass = UCA_CAMERA_GET_CLASS (camera);

    g_return_if_fail (klass != NULL);
    g_return_if_fail (klass->start_recording != NULL);

    g_static_mutex_lock (&mutex);

    if (camera->priv->is_recording) {
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_RECORDING,
                "Camera is already recording");
        goto start_recording_unlock;
    }

    if (camera->priv->transfer_async && (camera->grab_func == NULL)) {
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NO_GRAB_FUNC,
                "No grab callback function set");
        goto start_recording_unlock;
    }

    (*klass->start_recording)(camera, &tmp_error);

    if (tmp_error == NULL) {
        camera->priv->is_readout = FALSE;
        camera->priv->is_recording = TRUE;
        /* TODO: we should depend on GLib 2.26 and use g_object_notify_by_pspec */
        g_object_notify (G_OBJECT (camera), "is-recording");
    }
    else
        g_propagate_error (error, tmp_error);

start_recording_unlock:
    g_static_mutex_unlock (&mutex);
}

/**
 * uca_camera_stop_recording:
 * @camera: A #UcaCamera object
 * @error: Location to store a #UcaCameraError error or %NULL
 *
 * Stop recording.
 */
void
uca_camera_stop_recording (UcaCamera *camera, GError **error)
{
    UcaCameraClass *klass;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_if_fail (UCA_IS_CAMERA (camera));

    klass = UCA_CAMERA_GET_CLASS (camera);

    g_return_if_fail (klass != NULL);
    g_return_if_fail (klass->stop_recording != NULL);

    g_static_mutex_lock (&mutex);

    if (!camera->priv->is_recording) {
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_RECORDING,
                     "Camera is not recording");
    }
    else {
        GError *tmp_error = NULL;

        (*klass->stop_recording)(camera, &tmp_error);

        if (tmp_error == NULL) {
            camera->priv->is_readout = FALSE;
            camera->priv->is_recording = FALSE;
            /* TODO: we should depend on GLib 2.26 and use g_object_notify_by_pspec */
            g_object_notify (G_OBJECT (camera), "is-recording");
        }
        else
            g_propagate_error (error, tmp_error);
    }

    g_static_mutex_unlock (&mutex);
}

/**
 * uca_camera_start_readout:
 * @camera: A #UcaCamera object
 * @error: Location to store a #UcaCameraError error or %NULL
 *
 * Initiate readout mode for cameras that support
 * #UcaCamera:has-camram-recording after calling
 * uca_camera_stop_recording(). Frames have to be picked up with
 * ufo_camera_grab().
 */
void
uca_camera_start_readout (UcaCamera *camera, GError **error)
{
    UcaCameraClass *klass;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_if_fail (UCA_IS_CAMERA(camera));

    klass = UCA_CAMERA_GET_CLASS(camera);

    g_return_if_fail (klass != NULL);
    g_return_if_fail (klass->start_readout != NULL);

    g_static_mutex_lock (&mutex);

    if (camera->priv->is_recording) {
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_RECORDING,
                     "Camera is still recording");
    }
    else {
        GError *tmp_error = NULL;

        (*klass->start_readout) (camera, &tmp_error);

        if (tmp_error == NULL) {
            camera->priv->is_readout = TRUE;
            /* TODO: we should depend on GLib 2.26 and use g_object_notify_by_pspec */
            g_object_notify (G_OBJECT (camera), "is-readout");
        }
        else
            g_propagate_error (error, tmp_error);
    }

    g_static_mutex_unlock (&mutex);
}

/**
 * uca_camera_set_grab_func:
 * @camera: A #UcaCamera object
 * @func: A #UcaCameraGrabFunc callback function
 * @user_data: Data that is passed on to #func
 *
 * Set the grab function that is called whenever a frame is readily transfered.
 */
void
uca_camera_set_grab_func(UcaCamera *camera, UcaCameraGrabFunc func, gpointer user_data)
{
    camera->grab_func = func;
    camera->user_data = user_data;
}

/**
 * uca_camera_trigger:
 * @camera: A #UcaCamera object
 * @error: Location to store a #UcaCameraError error or %NULL
 *
 * Trigger from software if supported by camera.
 *
 * You must have called uca_camera_start_recording() before, otherwise you will
 * get a #UCA_CAMERA_ERROR_NOT_RECORDING error.
 */
void
uca_camera_trigger (UcaCamera *camera, GError **error)
{
    UcaCameraClass *klass;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_if_fail (UCA_IS_CAMERA (camera));

    klass = UCA_CAMERA_GET_CLASS (camera);

    g_return_if_fail (klass != NULL);
    g_return_if_fail (klass->trigger != NULL);

    g_static_mutex_lock (&mutex);

    if (!camera->priv->is_recording)
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_RECORDING, "Camera is not recording");
    else
        (*klass->trigger) (camera, error);

    g_static_mutex_unlock (&mutex);
}

/**
 * uca_camera_grab:
 * @camera: A #UcaCamera object
 * @data: Pointer to pointer to the data. Must not be %NULL.
 * @error: Location to store a #UcaCameraError error or %NULL
 *
 * Grab a frame a single frame and store the result in @data. If the pointer
 * pointing to the data is %NULL, memory will be allocated otherwise it will be
 * used to store the frame. If memory is allocated by uca_camera_grab() it must
 * be freed by the caller.
 *
 * You must have called uca_camera_start_recording() before, otherwise you will
 * get a #UCA_CAMERA_ERROR_NOT_RECORDING error.
 */
void
uca_camera_grab (UcaCamera *camera, gpointer *data, GError **error)
{
    UcaCameraClass *klass;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_if_fail (UCA_IS_CAMERA(camera));

    klass = UCA_CAMERA_GET_CLASS (camera);

    g_return_if_fail (klass != NULL);
    g_return_if_fail (klass->grab != NULL);
    g_return_if_fail (data != NULL);

    g_static_mutex_lock (&mutex);

    if (!camera->priv->is_recording && !camera->priv->is_readout)
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_RECORDING, "Camera is neither recording nor in readout mode");
    else
        (*klass->grab) (camera, data, error);

    g_static_mutex_unlock (&mutex);
}

/**
 * uca_camera_register_unit:
 * @camera: A #UcaCamera object
 * @prop_name: Name of a property
 * @unit: #UcaUnit
 *
 * Associates @prop_name of @camera with a specific @unit.
 *
 * Since: 1.1
 */
void
uca_camera_register_unit (UcaCamera *camera,
                          const gchar *prop_name,
                          UcaUnit unit)
{
    UcaCameraClass *klass;
    GObjectClass *oclass;
    GParamSpec *pspec;

    klass  = UCA_CAMERA_GET_CLASS (camera);
    oclass = G_OBJECT_CLASS (klass);
    pspec  = g_object_class_find_property (oclass, prop_name);

    if (pspec == NULL) {
        g_warning ("Camera does not have property `%s'", prop_name);
        return;
    }

    uca_camera_set_property_unit (pspec, unit);
}

/**
 * uca_camera_get_unit:
 * @camera: A #UcaCamera object
 * @prop_name: Name of a property
 *
 * Returns the unit associated with @prop_name of @camera.
 *
 * Returns: A #UcaUnit value associated with @prop_name. If there is none, the
 * value will be #UCA_UNIT_NA.
 * Since: 1.1
 */
UcaUnit
uca_camera_get_unit (UcaCamera *camera,
                     const gchar *prop_name)
{
    UcaCameraClass *klass;
    GObjectClass *oclass;
    GParamSpec *pspec;
    gpointer data;

    klass  = UCA_CAMERA_GET_CLASS (camera);
    oclass = G_OBJECT_CLASS (klass);
    pspec  = g_object_class_find_property (oclass, prop_name);

    if (pspec == NULL) {
        g_warning ("Camera does not have property `%s'", prop_name);
        return UCA_UNIT_NA;
    }

    data = g_param_spec_get_qdata (pspec, UCA_UNIT_QUARK);
    return data == NULL ? UCA_UNIT_NA : GPOINTER_TO_INT (data);
}

