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

#include <glib.h>
#include "config.h"
#include "uca-camera.h"
#include "uca-enums.h"

/* #ifdef HAVE_PCO_CL */
/* #include "cameras/uca-pco-camera.h" */
/* #endif */

/* #ifdef HAVE_PYLON_CAMERA */
/* #include "cameras/uca-pylon-camera.h" */
/* #endif */

/* #ifdef HAVE_MOCK_CAMERA */
/* #include "cameras/uca-mock-camera.h" */
/* #endif */

/* #ifdef HAVE_UFO_CAMERA */
/* #include "cameras/uca-ufo-camera.h" */
/* #endif */

/* #ifdef HAVE_PHOTON_FOCUS */
/* #include "cameras/uca-pf-camera.h" */
/* #endif */

#define UCA_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_CAMERA, UcaCameraPrivate))

G_DEFINE_TYPE(UcaCamera, uca_camera, G_TYPE_OBJECT)

/**
 * UcaCameraTrigger:
 * @UCA_CAMERA_TRIGGER_AUTO: Trigger automatically
 * @UCA_CAMERA_TRIGGER_EXTERNAL: Trigger from an external source
 * @UCA_CAMERA_TRIGGER_INTERNAL: Trigger internally from software using
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
    return g_quark_from_static_string("uca-camera-error-quark");
}

/* static gchar *uca_camera_types[] = { */
/* #ifdef HAVE_PCO_CL */
/*         "pco", */
/* #endif */
/* #ifdef HAVE_PYLON_CAMERA */
/*         "pylon", */
/* #endif */
/* #ifdef HAVE_MOCK_CAMERA */
/*         "mock", */
/* #endif */
/* #ifdef HAVE_UFO_CAMERA */
/*         "ufo", */
/* #endif */
/* #ifdef HAVE_PHOTON_FOCUS */
/*         "pf", */
/* #endif */
/*         NULL */
/* }; */

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

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
uca_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaCameraPrivate *priv = UCA_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_IS_RECORDING:
            g_value_set_boolean(value, priv->is_recording);
            break;

        case PROP_IS_READOUT:
            g_value_set_boolean(value, priv->is_readout);
            break;

        case PROP_TRANSFER_ASYNCHRONOUSLY:
            g_value_set_boolean(value, priv->transfer_async);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
uca_camera_class_init(UcaCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_camera_set_property;
    gobject_class->get_property = uca_camera_get_property;

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

static void uca_camera_init(UcaCamera *camera)
{
    camera->grab_func = NULL;

    camera->priv = UCA_CAMERA_GET_PRIVATE(camera);
    camera->priv->is_recording = FALSE;
    camera->priv->is_readout = FALSE;
    camera->priv->transfer_async = FALSE;

    /*
     * This here would be the best place to instantiate the tango server object,
     * along these lines:
     *
     * // I'd prefer if you expose a single C method, so we don't have to
     * // compile uca-camera.c with g++
     * tango_handle = tango_server_new(camera);
     *
     * void tango_server_new(UcaCamera *camera)
     * {
     *     // Do whatever is necessary. In the end you will have some kind of
     *     // Tango object t which needs to somehow hook up to the properties. A
     *     // list of all available properties can be enumerated with
     *     // g_object_class_list_properties(G_OBJECT_CLASS(camera),
     *     //     &n_properties);
     *
     *     // For setting/getting properties, use g_object_get/set_property() or
     *     // g_object_get/set() whatever is more suitable.
     * }
     */
}

/* static UcaCamera *uca_camera_new_from_type(const gchar *type, GError **error) */
/* { */
/* #ifdef HAVE_MOCK_CAMERA */
/*     if (!g_strcmp0(type, "mock")) */
/*         return UCA_CAMERA(uca_mock_camera_new(error)); */
/* #endif */

/* #ifdef HAVE_PCO_CL */
/*     if (!g_strcmp0(type, "pco")) */
/*         return UCA_CAMERA(uca_pco_camera_new(error)); */
/* #endif */

/* #ifdef HAVE_PYLON_CAMERA */
/*     if (!g_strcmp0(type, "pylon")) */
/*         return UCA_CAMERA(uca_pylon_camera_new(error)); */
/* #endif */

/* #ifdef HAVE_UFO_CAMERA */
/*     if (!g_strcmp0(type, "ufo")) */
/*         return UCA_CAMERA(uca_ufo_camera_new(error)); */
/* #endif */

/* #ifdef HAVE_PHOTON_FOCUS */
/*     if (!g_strcmp0(type, "pf")) */
/*         return UCA_CAMERA(uca_pf_camera_new(error)); */
/* #endif */

/*     return NULL; */
/* } */

/* /** */
/*  * uca_camera_get_types: */
/*  * */
/*  * Enumerate all camera types that can be instantiated with uca_camera_new(). */
/*  * */
/*  * Returns: An array of strings with camera types. The list should be freed with */
/*  * g_strfreev(). */
/*  *1/ */
/* gchar **uca_camera_get_types() */
/* { */
/*     return g_strdupv(uca_camera_types); */
/* } */

/**
 * uca_camera_new:
 * @type: Type name of the camera
 * @error: Location to store an error or %NULL
 *
 * Factory method for instantiating cameras by names listed in
 * uca_camera_get_types().
 *
 * Returns: A new #UcaCamera of the correct type or %NULL if type was not found
 */
/* UcaCamera *uca_camera_new(const gchar *type, GError **error) */
/* { */
/*     UcaCamera *camera = NULL; */
/*     GError *tmp_error = NULL; */

/*     camera = uca_camera_new_from_type(type, &tmp_error); */

/*     if (tmp_error != NULL) { */
/*         g_propagate_error(error, tmp_error); */
/*         return NULL; */
/*     } */

/*     if ((tmp_error == NULL) && (camera == NULL)) { */
/*         g_set_error(error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_FOUND, */
/*                 "Camera type %s not found", type); */
/*         return NULL; */
/*     } */

/*     return camera; */
/* } */

/**
 * uca_camera_start_recording:
 * @camera: A #UcaCamera object
 * @error: Location to store a #UcaCameraError error or %NULL
 *
 * Initiate recording of frames. If UcaCamera:grab-asynchronous is %TRUE and a
 * #UcaCameraGrabFunc callback is set, frames are automatically transfered to
 * the client program, otherwise you must use uca_camera_grab().
 */
void uca_camera_start_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_CAMERA(camera));

    UcaCameraClass *klass = UCA_CAMERA_GET_CLASS(camera);

    g_return_if_fail(klass != NULL);
    g_return_if_fail(klass->start_recording != NULL);

    if (camera->priv->is_recording) {
        g_set_error(error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_RECORDING,
                "Camera is already recording");
        return;
    }

    if (camera->priv->transfer_async && (camera->grab_func == NULL)) {
        g_set_error(error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NO_GRAB_FUNC,
                "No grab callback function set");
        return;
    }

    GError *tmp_error = NULL;
    (*klass->start_recording)(camera, &tmp_error);

    if (tmp_error == NULL) {
        camera->priv->is_readout = FALSE;
        camera->priv->is_recording = TRUE;
        /* TODO: we should depend on GLib 2.26 and use g_object_notify_by_pspec */
        g_object_notify(G_OBJECT(camera), "is-recording");
    }
    else
        g_propagate_error(error, tmp_error);
}

/**
 * uca_camera_stop_recording:
 * @camera: A #UcaCamera object
 * @error: Location to store a #UcaCameraError error or %NULL
 *
 * Stop recording.
 */
void uca_camera_stop_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_CAMERA(camera));

    UcaCameraClass *klass = UCA_CAMERA_GET_CLASS(camera);

    g_return_if_fail(klass != NULL);
    g_return_if_fail(klass->stop_recording != NULL);

    if (!camera->priv->is_recording) {
        g_set_error(error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_RECORDING,
                "Camera is not recording");
        return;
    }

    GError *tmp_error = NULL;
    (*klass->stop_recording)(camera, &tmp_error);

    if (tmp_error == NULL) {
        camera->priv->is_readout = FALSE;
        camera->priv->is_recording = FALSE;
        /* TODO: we should depend on GLib 2.26 and use g_object_notify_by_pspec */
        g_object_notify(G_OBJECT(camera), "is-recording");
    }
    else
        g_propagate_error(error, tmp_error);
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
void uca_camera_start_readout(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_CAMERA(camera));

    UcaCameraClass *klass = UCA_CAMERA_GET_CLASS(camera);

    g_return_if_fail(klass != NULL);
    g_return_if_fail(klass->start_readout != NULL);

    if (camera->priv->is_recording) {
        g_set_error(error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_RECORDING,
                "Camera is still recording");
        return;
    }

    GError *tmp_error = NULL;
    (*klass->start_readout)(camera, &tmp_error);

    if (tmp_error == NULL) {
        camera->priv->is_readout = TRUE;
        /* TODO: we should depend on GLib 2.26 and use g_object_notify_by_pspec */
        g_object_notify(G_OBJECT(camera), "is-readout");
    }
    else
        g_propagate_error(error, tmp_error);
}

/**
 * uca_camera_set_grab_func:
 * @camera: A #UcaCamera object
 * @func: A #UcaCameraGrabFunc callback function
 * @user_data: Data that is passed on to #func
 *
 * Set the grab function that is called whenever a frame is readily transfered.
 */
void uca_camera_set_grab_func(UcaCamera *camera, UcaCameraGrabFunc func, gpointer user_data)
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
void uca_camera_trigger(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_CAMERA(camera));

    UcaCameraClass *klass = UCA_CAMERA_GET_CLASS(camera);

    g_return_if_fail(klass != NULL);
    g_return_if_fail(klass->trigger != NULL);

    if (!camera->priv->is_recording) {
        g_set_error(error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_RECORDING,
                "Camera is not recording");
        return;
    }

    (*klass->trigger)(camera, error);
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
void uca_camera_grab(UcaCamera *camera, gpointer *data, GError **error)
{
    g_return_if_fail(UCA_IS_CAMERA(camera));

    UcaCameraClass *klass = UCA_CAMERA_GET_CLASS(camera);

    g_return_if_fail(klass != NULL);
    g_return_if_fail(klass->grab != NULL);
    g_return_if_fail(data != NULL);

    if (!camera->priv->is_recording && !camera->priv->is_readout) {
        g_set_error(error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_RECORDING,
                "Camera is neither recording nor in readout mode");
        return;
    }

    (*klass->grab)(camera, data, error);
}

