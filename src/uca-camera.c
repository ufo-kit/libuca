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

#include "config.h"

#ifdef WITH_PYTHON_MULTITHREADING
#include <Python.h>
#endif

#include <glib.h>
#include <string.h>
#include "compat.h"
#include "uca-camera.h"
#include "uca-ring-buffer.h"
#include "uca-enums.h"

#define UCA_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_CAMERA, UcaCameraPrivate))

G_DEFINE_TYPE(UcaCamera, uca_camera, G_TYPE_OBJECT)

/**
 * UcaCameraTriggerSource:
 * @UCA_CAMERA_TRIGGER_SOURCE_AUTO: Trigger automatically
 * @UCA_CAMERA_TRIGGER_SOURCE_EXTERNAL: Trigger from an external source
 * @UCA_CAMERA_TRIGGER_SOURCE_SOFTWARE: Trigger from software using
 *      #uca_camera_trigger
 */

/**
 * UcaCameraTriggerType:
 * @UCA_CAMERA_TRIGGER_TYPE_EDGE: Trigger on rising edge
 * @UCA_CAMERA_TRIGGER_TYPE_LEVEL: Trigger during level signal
 */

/**
 * UcaCameraError:
 * @UCA_CAMERA_ERROR_NOT_FOUND: Camera type is unknown
 * @UCA_CAMERA_ERROR_RECORDING: Camera is already recording
 * @UCA_CAMERA_ERROR_NOT_RECORDING: Camera is not recording
 * @UCA_CAMERA_ERROR_NO_GRAB_FUNC: No grab callback was set
 * @UCA_CAMERA_ERROR_NOT_IMPLEMENTED: Virtual function is not implemented
 * @UCA_CAMERA_ERROR_WRONG_WRITE_METADATA: Meta data specified in the name
 *  argument of the write method is not correct.
 * @UCA_CAMERA_ERROR_END_OF_STREAM: Data stream has ended.
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

GQuark uca_writable_quark ()
{
    return g_quark_from_static_string ("uca-writable-quark");
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
    "sensor-pixel-width",
    "sensor-pixel-height",
    "sensor-bitdepth",
    "sensor-horizontal-binning",
    "sensor-horizontal-binnings",
    "sensor-vertical-binning",
    "sensor-vertical-binnings",
    "trigger",
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
    "recorded-frames",
    "transfer-asynchronously",
    "is-recording",
    "is-readout",
    "buffered",
    "num-buffers",
};

static GParamSpec *camera_properties[N_BASE_PROPERTIES] = { NULL, };
static GStaticMutex access_lock = G_STATIC_MUTEX_INIT;

struct _UcaCameraPrivate {
    gboolean cancelling_recording;
    gboolean is_recording;
    gboolean is_readout;
    gboolean transfer_async;
    gboolean buffered;
    guint num_buffers;
    GThread *read_thread;
    UcaRingBuffer *ring_buffer;
    UcaCameraTriggerSource trigger_source;
    UcaCameraTriggerType trigger_type;
    GValueArray *h_binnings;
    GValueArray *v_binnings;
};

static void
uca_camera_set_property_unit (GParamSpec *pspec, UcaUnit unit)
{
    UcaUnit old_unit;

    old_unit = (UcaUnit) GPOINTER_TO_INT (g_param_spec_get_qdata (pspec, UCA_UNIT_QUARK));

    if (old_unit != unit && old_unit != UCA_UNIT_NA)
        g_warning ("::%s already has a different unit", pspec->name);
    else
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

        case PROP_TRIGGER_SOURCE:
            priv->trigger_source = g_value_get_enum (value);
            break;

        case PROP_TRIGGER_TYPE:
            priv->trigger_type = g_value_get_enum (value);
            break;

        case PROP_BUFFERED:
            priv->buffered = g_value_get_boolean (value);
            break;

        case PROP_NUM_BUFFERS:
            priv->num_buffers = g_value_get_uint (value);
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

        case PROP_TRIGGER_SOURCE:
            g_value_set_enum (value, priv->trigger_source);
            break;

        case PROP_TRIGGER_TYPE:
            g_value_set_enum (value, priv->trigger_type);
            break;

        case PROP_FRAMES_PER_SECOND:
            {
                gdouble exposure_time;

                g_object_get (object, "exposure-time", &exposure_time, NULL);

                if (exposure_time > 0.0)
                    g_value_set_double (value, 1. / exposure_time);
                else
                    g_warning ("Invalid `::exposure-time' set");
            }
            break;

        case PROP_RECORDED_FRAMES:
            g_value_set_uint (value, 0);
            break;

        case PROP_SENSOR_PIXEL_WIDTH:
            /* 10um is an arbitrary default, cameras should definitely override
             * this. */
            g_value_set_double (value, 10e-6);
            break;

        case PROP_SENSOR_PIXEL_HEIGHT:
            g_value_set_double (value, 10e-6);
            break;

        case PROP_SENSOR_HORIZONTAL_BINNING:
            g_value_set_uint (value, 1);
            break;

        case PROP_SENSOR_VERTICAL_BINNING:
            g_value_set_uint (value, 1);
            break;

        case PROP_SENSOR_HORIZONTAL_BINNINGS:
            g_value_set_boxed (value, priv->h_binnings);
            break;

        case PROP_SENSOR_VERTICAL_BINNINGS:
            g_value_set_boxed (value, priv->v_binnings);
            break;

        case PROP_ROI_WIDTH_MULTIPLIER:
            g_value_set_uint (value, 1);
            break;

        case PROP_ROI_HEIGHT_MULTIPLIER:
            g_value_set_uint (value, 1);
            break;

        case PROP_BUFFERED:
            g_value_set_boolean (value, priv->buffered);
            break;

        case PROP_NUM_BUFFERS:
            g_value_set_uint (value, priv->num_buffers);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
uca_camera_dispose (GObject *object)
{
    UcaCameraPrivate *priv;

    priv = UCA_CAMERA_GET_PRIVATE (object);

    if (priv->is_recording) {
        GError *error = NULL;

        uca_camera_stop_recording (UCA_CAMERA (object), &error);

        if (error != NULL) {
            g_warning ("Could not stop recording: %p", error->message);
            g_error_free (error);
        }
    }

    if (priv->ring_buffer != NULL) {
        g_object_unref (priv->ring_buffer);
        priv->ring_buffer = NULL;
    }

    G_OBJECT_CLASS (uca_camera_parent_class)->dispose (object);
}

static void
uca_camera_finalize (GObject *object)
{
    UcaCameraPrivate *priv;
    GParamSpec **props;
    guint n_props;

    priv = UCA_CAMERA_GET_PRIVATE (object);
    g_value_array_free (priv->h_binnings);
    g_value_array_free (priv->v_binnings);

    /* We will reset property units of all subclassed objects  */
    props = g_object_class_list_properties (G_OBJECT_GET_CLASS (object), &n_props);

    for (guint i = 0; i < n_props; i++) {
        g_param_spec_set_qdata (props[i], UCA_UNIT_QUARK, NULL);
    }

    g_free (props);

    G_OBJECT_CLASS (uca_camera_parent_class)->finalize (object);
}

static void
uca_camera_class_init (UcaCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_camera_set_property;
    gobject_class->get_property = uca_camera_get_property;
    gobject_class->dispose = uca_camera_dispose;
    gobject_class->finalize = uca_camera_finalize;

    klass->start_recording = NULL;
    klass->stop_recording = NULL;
    klass->grab = NULL;
    klass->write = NULL;

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

    camera_properties[PROP_SENSOR_PIXEL_WIDTH] =
        g_param_spec_double (uca_camera_props[PROP_SENSOR_PIXEL_WIDTH],
            "Width of sensor pixel in meters",
            "Width of sensor pixel in meters",
            G_MINDOUBLE, G_MAXDOUBLE, 10e-6,
            G_PARAM_READABLE);

    camera_properties[PROP_SENSOR_PIXEL_HEIGHT] =
        g_param_spec_double (uca_camera_props[PROP_SENSOR_PIXEL_HEIGHT],
            "Height of sensor pixel in meters",
            "Height of sensor pixel in meters",
            G_MINDOUBLE, G_MAXDOUBLE, 10e-6,
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

    camera_properties[PROP_TRIGGER_SOURCE] =
        g_param_spec_enum("trigger-source",
            "Trigger source",
            "Trigger source",
            UCA_TYPE_CAMERA_TRIGGER_SOURCE, UCA_CAMERA_TRIGGER_SOURCE_AUTO,
            G_PARAM_READWRITE);

    camera_properties[PROP_TRIGGER_TYPE] =
        g_param_spec_enum("trigger-type",
            "Trigger type",
            "Trigger type",
            UCA_TYPE_CAMERA_TRIGGER_TYPE, UCA_CAMERA_TRIGGER_TYPE_EDGE,
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
            G_MINDOUBLE, G_MAXDOUBLE, 1.0,
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

    /**
     * UcaCamera:recorded-frames
     *
     * Number of frames that are recorded into internal camera memory.
     *
     * Since: 1.1
     */
    camera_properties[PROP_RECORDED_FRAMES] =
        g_param_spec_uint(uca_camera_props[PROP_RECORDED_FRAMES],
            "Number of frames recorded into internal camera memory",
            "Number of frames recorded into internal camera memory",
            0, G_MAXUINT, 0,
            G_PARAM_READABLE);

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

    camera_properties[PROP_BUFFERED] =
        g_param_spec_boolean(uca_camera_props[PROP_BUFFERED],
            "TRUE if libuca should buffer frames",
            "TRUE if libuca should buffer frames",
            FALSE, G_PARAM_READWRITE);

    camera_properties[PROP_NUM_BUFFERS] =
        g_param_spec_uint(uca_camera_props[PROP_NUM_BUFFERS],
            "Number of frame buffers in the ring buffer ",
            "Number of frame buffers in the ring buffer ",
            0, G_MAXUINT, 4,
            G_PARAM_READWRITE);

    for (guint id = PROP_0 + 1; id < N_BASE_PROPERTIES; id++)
        g_object_class_install_property(gobject_class, id, camera_properties[id]);

    g_type_class_add_private(klass, sizeof(UcaCameraPrivate));
}

static void
uca_camera_init (UcaCamera *camera)
{
    GValue val = {0};

    camera->grab_func = NULL;

    camera->priv = UCA_CAMERA_GET_PRIVATE(camera);
    camera->priv->cancelling_recording = FALSE;
    camera->priv->is_recording = FALSE;
    camera->priv->is_readout = FALSE;
    camera->priv->transfer_async = FALSE;
    camera->priv->trigger_source = UCA_CAMERA_TRIGGER_SOURCE_AUTO;
    camera->priv->trigger_type = UCA_CAMERA_TRIGGER_TYPE_EDGE;
    camera->priv->h_binnings = g_value_array_new (1);
    camera->priv->v_binnings = g_value_array_new (1);
    camera->priv->buffered = FALSE;
    camera->priv->num_buffers = 4;
    camera->priv->ring_buffer = NULL;

    g_value_init (&val, G_TYPE_UINT);
    g_value_set_uint (&val, 1);
    g_value_array_append (camera->priv->h_binnings, &val);
    g_value_array_append (camera->priv->v_binnings, &val);

    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_WIDTH], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_HEIGHT], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_PIXEL_WIDTH], UCA_UNIT_METER);
    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_PIXEL_HEIGHT], UCA_UNIT_METER);
    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_BITDEPTH], UCA_UNIT_COUNT);
    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_HORIZONTAL_BINNING], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_SENSOR_VERTICAL_BINNING], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_EXPOSURE_TIME], UCA_UNIT_SECOND);
    uca_camera_set_property_unit (camera_properties[PROP_FRAMES_PER_SECOND], UCA_UNIT_COUNT);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_X], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_Y], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_WIDTH], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_HEIGHT], UCA_UNIT_PIXEL);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_WIDTH_MULTIPLIER], UCA_UNIT_COUNT);
    uca_camera_set_property_unit (camera_properties[PROP_ROI_HEIGHT_MULTIPLIER], UCA_UNIT_COUNT);
    uca_camera_set_property_unit (camera_properties[PROP_RECORDED_FRAMES], UCA_UNIT_COUNT);

#ifdef WITH_PYTHON_MULTITHREADING
    if (!PyEval_ThreadsInitialized ()) {
        PyEval_InitThreads ();
    }
#endif
}

static gpointer
buffer_thread (UcaCamera *camera)
{
    UcaCameraClass *klass;
    GError *error = NULL;

    klass = UCA_CAMERA_GET_CLASS (camera);

    while (!camera->priv->cancelling_recording) {
        gpointer buffer;

        buffer = uca_ring_buffer_get_write_pointer (camera->priv->ring_buffer);

        if (!(*klass->grab) (camera, buffer, &error))
            break;

        uca_ring_buffer_write_advance (camera->priv->ring_buffer);
    }

    return error;
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
    UcaCameraPrivate *priv;
    GError *tmp_error = NULL;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_if_fail (UCA_IS_CAMERA (camera));

    klass = UCA_CAMERA_GET_CLASS (camera);

    g_return_if_fail (klass != NULL);
    g_return_if_fail (klass->start_recording != NULL);

    priv = camera->priv;

    g_static_mutex_lock (&mutex);

    if (priv->is_recording) {
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_RECORDING,
                     "Camera is already recording");
        goto start_recording_unlock;
    }

    if (priv->transfer_async && (camera->grab_func == NULL)) {
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NO_GRAB_FUNC,
                     "No grab callback function set");
        goto start_recording_unlock;
    }

    g_static_mutex_lock (&access_lock);
    (*klass->start_recording)(camera, &tmp_error);
    g_static_mutex_unlock (&access_lock);

    if (tmp_error == NULL) {
        priv->is_readout = FALSE;
        priv->is_recording = TRUE;
        priv->cancelling_recording = FALSE;

        /* TODO: we should depend on GLib 2.26 and use g_object_notify_by_pspec */
        g_object_notify (G_OBJECT (camera), "is-recording");
    }
    else
        g_propagate_error (error, tmp_error);

    if (priv->buffered) {
        guint width, height, bitdepth;
        guint pixel_size;

        g_object_get (camera,
                      "roi-width", &width,
                      "roi-height", &height,
                      "sensor-bitdepth", &bitdepth,
                      NULL);

        pixel_size = bitdepth <= 8 ? 1 : 2;
        priv->ring_buffer = uca_ring_buffer_new (width * height * pixel_size,
                                                         priv->num_buffers);

        /* Let's read out the frames from another thread */
        priv->read_thread = g_thread_new ("read-thread", (GThreadFunc) buffer_thread, camera);
    }

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
    UcaCameraPrivate *priv;
    GError *tmp_error = NULL;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_if_fail (UCA_IS_CAMERA (camera));

    klass = UCA_CAMERA_GET_CLASS (camera);

    g_return_if_fail (klass != NULL);
    g_return_if_fail (klass->stop_recording != NULL);

    priv = camera->priv;

    g_static_mutex_lock (&mutex);

    if (!priv->is_recording) {
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_RECORDING,
                     "Camera is not recording");
        goto error_stop_recording;
    }

    priv->cancelling_recording = TRUE;

    if (priv->buffered) {
        g_thread_join (priv->read_thread);
        priv->read_thread = NULL;
    }

    g_static_mutex_lock (&access_lock);

    (*klass->stop_recording)(camera, &tmp_error);
    priv->cancelling_recording = FALSE;

    g_static_mutex_unlock (&access_lock);

    if (tmp_error == NULL) {
        priv->is_recording = FALSE;
        priv->is_readout = FALSE;
        /* TODO: we should depend on GLib 2.26 and use g_object_notify_by_pspec */
        g_object_notify (G_OBJECT (camera), "is-recording");
    }
    else
        g_propagate_error (error, tmp_error);

    if (camera->priv->ring_buffer != NULL) {
        g_object_unref (camera->priv->ring_buffer);
        camera->priv->ring_buffer = NULL;
    }

error_stop_recording:
    g_static_mutex_unlock (&mutex);
}

/**
 * uca_camera_is_recording:
 * @camera: A #UcaCamera object
 *
 * Convenience function to ask the current recording status
 *
 * Return value: %TRUE if recording is ongoing
 * Since: 1.5
 */
gboolean
uca_camera_is_recording (UcaCamera *camera)
{
    g_return_val_if_fail (UCA_IS_CAMERA (camera), FALSE);
    return camera->priv->is_recording;
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

        g_static_mutex_lock (&access_lock);
        (*klass->start_readout) (camera, &tmp_error);
        g_static_mutex_unlock (&access_lock);

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
 * uca_camera_stop_readout:
 * @camera: A #UcaCamera object
 * @error: Location to store a #UcaCameraError error or %NULL
 *
 * Stop reading out frames.
 *
 * Since: 1.1
 */
void
uca_camera_stop_readout (UcaCamera *camera, GError **error)
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

        g_static_mutex_lock (&access_lock);
        (*klass->stop_readout) (camera, &tmp_error);
        g_static_mutex_unlock (&access_lock);

        if (tmp_error == NULL) {
            camera->priv->is_readout = FALSE;
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
 * @func: (scope call): A #UcaCameraGrabFunc callback function
 * @user_data: (closure): Data that is passed on to #func
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
    else {
        g_static_mutex_lock (&access_lock);
        (*klass->trigger) (camera, error);
        g_static_mutex_unlock (&access_lock);
    }

    g_static_mutex_unlock (&mutex);
}

/**
 * uca_camera_write:
 * @camera: A #UcaCamera object
 * @name: String that identifies the written data.
 * @data: (type gulong): Pointer to suitably sized data buffer. Must not be
 *  %NULL.
 * @size: Size of the data buffer in bytes.
 * @error: Location to store a #UcaCameraError or %NULL.
 *
 * Writes camera-specific @data containing @size bytes and identified by @name.
 */
void
uca_camera_write (UcaCamera *camera, const gchar *name, gpointer data, gsize size, GError **error)
{
    UcaCameraClass *klass;

    g_return_if_fail (UCA_IS_CAMERA (camera));

    klass = UCA_CAMERA_GET_CLASS (camera);

    g_return_if_fail (klass != NULL);

    if (klass->write == NULL) {
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_IMPLEMENTED,
                     "`%s' does not provide a `write' method",
                     G_OBJECT_TYPE_NAME (G_OBJECT (camera)));
    }
    else {
        (*klass->write) (camera, name, data, size, error);
    }
}

/**
 * uca_camera_grab:
 * @camera: A #UcaCamera object
 * @data: (type gulong): Pointer to suitably sized data buffer. Must not be
 *  %NULL.
 * @error: Location to store a #UcaCameraError error or %NULL
 *
 * Grab a frame a single frame and store the result in @data.
 *
 * You must have called uca_camera_start_recording() before, otherwise you will
 * get a #UCA_CAMERA_ERROR_NOT_RECORDING error.
 */
gboolean
uca_camera_grab (UcaCamera *camera, gpointer data, GError **error)
{
    UcaCameraClass *klass;
    gboolean result = FALSE;

    /* FIXME: this prevents accessing two independent cameras simultanously. */
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_val_if_fail (UCA_IS_CAMERA(camera), FALSE);

    klass = UCA_CAMERA_GET_CLASS (camera);

    g_return_val_if_fail (klass != NULL, FALSE);
    g_return_val_if_fail (klass->grab != NULL, FALSE);
    g_return_val_if_fail (data != NULL, FALSE);

    if (!camera->priv->buffered) {
        g_static_mutex_lock (&mutex);

        if (!camera->priv->is_recording && !camera->priv->is_readout) {
            g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_RECORDING,
                         "Camera is neither recording nor in readout mode");
        }
        else {
            g_static_mutex_lock (&access_lock);

#ifdef WITH_PYTHON_MULTITHREADING
            if (Py_IsInitialized ()) {
                PyGILState_STATE state = PyGILState_Ensure ();
                Py_BEGIN_ALLOW_THREADS

                result = (*klass->grab) (camera, data, error);

                Py_END_ALLOW_THREADS
                PyGILState_Release (state);
            }
            else {
                result = (*klass->grab) (camera, data, error);
            }
#else
            result = (*klass->grab) (camera, data, error);
#endif

            g_static_mutex_unlock (&access_lock);
        }

        g_static_mutex_unlock (&mutex);
    }
    else {
        gpointer buffer;

        if (camera->priv->ring_buffer == NULL)
            return FALSE;

        /*
         * Spin-lock until we can read something. This shouldn't happen to
         * often, as buffering is usually used in those cases when the camera is
         * faster than the software.
         */
        while (!uca_ring_buffer_available (camera->priv->ring_buffer))
            ;

        buffer = uca_ring_buffer_get_read_pointer (camera->priv->ring_buffer);

        if (buffer == NULL) {
            g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_END_OF_STREAM,
                         "Ring buffer is empty");
        }
        else {
            memcpy (data, buffer, uca_ring_buffer_get_block_size (camera->priv->ring_buffer));
            result = TRUE;
        }
    }
    return result;
}

static GParamSpec *
get_param_spec_by_name (UcaCamera *camera,
                        const gchar *prop_name)
{
    UcaCameraClass *klass;
    GObjectClass *oclass;
    GParamSpec *pspec;

    klass  = UCA_CAMERA_GET_CLASS (camera);
    oclass = G_OBJECT_CLASS (klass);
    pspec  = g_object_class_find_property (oclass, prop_name);

    if (pspec == NULL) {
        g_warning ("Camera does not have property `%s'", prop_name);
        return NULL;
    }

    return pspec;
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
    GParamSpec *pspec;

    pspec = get_param_spec_by_name (camera, prop_name);

    if (pspec != NULL)
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
    GParamSpec *pspec;
    gpointer data;

    pspec  = get_param_spec_by_name (camera, prop_name);

    if (pspec == NULL)
        return UCA_UNIT_NA;

    data = g_param_spec_get_qdata (pspec, UCA_UNIT_QUARK);
    return data == NULL ? UCA_UNIT_NA : GPOINTER_TO_INT (data);
}

/**
 * uca_camera_set_writable:
 * @camera: A #UcaCamera object
 * @prop_name: Name of property
 * @writable: %TRUE if property can be written during acquisition
 *
 * Sets a flag that defines if @prop_name can be written during an acquisition.
 *
 * Since: 1.6
 */
void
uca_camera_set_writable (UcaCamera *camera,
                         const gchar *prop_name,
                         gboolean writable)
{
    GParamSpec *pspec;

    pspec = get_param_spec_by_name (camera, prop_name);

    if (pspec != NULL) {
        if (g_param_spec_get_qdata (pspec, UCA_WRITABLE_QUARK) != NULL)
            g_warning ("::%s is already fixed", pspec->name);
        else
            g_param_spec_set_qdata (pspec, UCA_WRITABLE_QUARK, GINT_TO_POINTER (writable));
    }
}

/**
 * uca_camera_is_writable_during_acquisition:
 * @camera: A #UcaCamera object
 * @prop_name: Name of property
 *
 * Check if @prop_name can be written at run-time. This is %FALSE if the
 * property is read-only, if uca_camera_set_writable() has not been called or
 * uca_camera_set_writable() was called with %FALSE.
 *
 * Returns: %TRUE if the property can be written at acquisition time.
 * Since: 1.6
 */
gboolean
uca_camera_is_writable_during_acquisition (UcaCamera *camera,
                                           const gchar *prop_name)
{
    GParamSpec *pspec;

    pspec = get_param_spec_by_name (camera, prop_name);

    return (pspec->flags & G_PARAM_WRITABLE) &&
            g_param_spec_get_qdata (pspec, UCA_WRITABLE_QUARK);
}
