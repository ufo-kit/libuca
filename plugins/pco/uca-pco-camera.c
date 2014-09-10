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

#include <gio/gio.h>
#include <gmodule.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libpco/libpco.h>
#include <libpco/sc2_defs.h>
#include <fgrab_prototyp.h>
#include "uca-camera.h"
#include "uca-pco-camera.h"
#include "uca-pco-enums.h"

#define FG_TRY_PARAM(fg, error, param, val_addr, port)     \
    { int r = Fg_setParameter(fg, param, val_addr, port);   \
        if (r != FG_OK) {                                   \
            g_set_error (error, UCA_PCO_CAMERA_ERROR,        \
                    UCA_PCO_CAMERA_ERROR_FG_GENERAL,        \
                    "%s", Fg_getLastErrorDescription(fg));  \
            return FALSE;                                    \
        } }

#define FG_SET_ERROR(err, fg, err_type)                 \
    if (err != FG_OK) {                                 \
        g_set_error(error, UCA_PCO_CAMERA_ERROR,        \
                err_type,                               \
                "%s", Fg_getLastErrorDescription(fg));  \
        return;                                         \
    }

#define CHECK_AND_RETURN_ON_PCO_ERROR(err)               \
    if ((err) != PCO_NOERROR) {                          \
        g_set_error (error, UCA_PCO_CAMERA_ERROR,        \
                     UCA_PCO_CAMERA_ERROR_LIBPCO_GENERAL,\
                     "libpco error %x", err);            \
        return;                                          \
    }

#define CHECK_AND_RETURN_VAL_ON_PCO_ERROR(err, val)         \
    if ((err) != PCO_NOERROR) {                             \
        g_set_error (error, UCA_PCO_CAMERA_ERROR,           \
                     UCA_PCO_CAMERA_ERROR_LIBPCO_GENERAL,   \
                     "libpco error %x", err);               \
        return val;                                         \
    }

#define UCA_PCO_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_PCO_CAMERA, UcaPcoCameraPrivate))

static void uca_pco_camera_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaPcoCamera, uca_pco_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_pco_camera_initable_iface_init))

#define TIMEBASE_INVALID 0xDEAD

/**
 * UcaPcoCameraRecordMode:
 * @UCA_PCO_CAMERA_RECORD_MODE_SEQUENCE: Store all frames and stop if necessary
 * @UCA_PCO_CAMERA_RECORD_MODE_RING_BUFFER: Store frames in ring-buffer fashion
 *      and overwrite if necessary
 */

/**
 * UcaPcoCameraStorageMode:
 * @UCA_PCO_CAMERA_STORAGE_MODE_RECORDER: Record all frames and output live
 *      preview frames in timely fashion, i.e. skipping those that cannot be handled.
 * @UCA_PCO_CAMERA_STORAGE_MODE_FIFO_BUFFER: Record frames in FIFO mode and
 *      return every frame in live view.
 */

/**
 * UcaPcoCameraAcquireMode:
 * @UCA_PCO_CAMERA_ACQUIRE_MODE_AUTO: Take all images
 * @UCA_PCO_CAMERA_ACQUIRE_MODE_EXTERNAL: Use <acq enbl> signal
 */

/**
 * UcaPcoCameraTimestamp:
 * @UCA_PCO_CAMERA_TIMESTAMP_NONE: Don't embed any timestamp
 * @UCA_PCO_CAMERA_TIMESTAMP_BINARY: Embed a BCD-coded timestamp in the first
 *     bytes
 * @UCA_PCO_CAMERA_TIMESTAMP_ASCII: Embed a visible ASCII timestamp in the image
 * @UCA_PCO_CAMERA_TIMESTAMP_BOTH: Embed both types of timestamps
 */

/**
 * UcaPcoCameraError:
 * @UCA_PCO_CAMERA_ERROR_LIBPCO_INIT: Initializing libpco failed
 * @UCA_PCO_CAMERA_ERROR_LIBPCO_GENERAL: General libpco error
 * @UCA_PCO_CAMERA_ERROR_UNSUPPORTED: Camera type is not supported
 * @UCA_PCO_CAMERA_ERROR_FG_INIT: Framegrabber initialization failed
 * @UCA_PCO_CAMERA_ERROR_FG_GENERAL: General framegrabber error
 * @UCA_PCO_CAMERA_ERROR_FG_ACQUISITION: Framegrabber acquisition error
 */
GQuark uca_pco_camera_error_quark()
{
    return g_quark_from_static_string("uca-pco-camera-error-quark");
}

enum {
    PROP_SENSOR_EXTENDED = N_BASE_PROPERTIES,
    PROP_SENSOR_WIDTH_EXTENDED,
    PROP_SENSOR_HEIGHT_EXTENDED,
    PROP_SENSOR_TEMPERATURE,
    PROP_SENSOR_PIXELRATES,
    PROP_SENSOR_PIXELRATE,
    PROP_SENSOR_ADCS,
    PROP_SENSOR_MAX_ADCS,
    PROP_HAS_DOUBLE_IMAGE_MODE,
    PROP_DOUBLE_IMAGE_MODE,
    PROP_OFFSET_MODE,
    PROP_RECORD_MODE,
    PROP_STORAGE_MODE,
    PROP_ACQUIRE_MODE,
    PROP_FAST_SCAN,
    PROP_COOLING_POINT,
    PROP_COOLING_POINT_MIN,
    PROP_COOLING_POINT_MAX,
    PROP_COOLING_POINT_DEFAULT,
    PROP_NOISE_FILTER,
    PROP_TIMESTAMP_MODE,
    N_PROPERTIES
};

static gint base_overrideables[] = {
    PROP_NAME,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_PIXEL_WIDTH,
    PROP_SENSOR_PIXEL_HEIGHT,
    PROP_SENSOR_BITDEPTH,
    PROP_SENSOR_HORIZONTAL_BINNING,
    PROP_SENSOR_HORIZONTAL_BINNINGS,
    PROP_SENSOR_VERTICAL_BINNING,
    PROP_SENSOR_VERTICAL_BINNINGS,
    PROP_EXPOSURE_TIME,
    PROP_FRAMES_PER_SECOND,
    PROP_TRIGGER_MODE,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    PROP_ROI_WIDTH_MULTIPLIER,
    PROP_ROI_HEIGHT_MULTIPLIER,
    PROP_HAS_STREAMING,
    PROP_HAS_CAMRAM_RECORDING,
    PROP_RECORDED_FRAMES,
    PROP_IS_RECORDING,
    0
};

static GParamSpec *pco_properties[N_PROPERTIES] = { NULL, };

/*
 * This structure defines type-specific properties of PCO cameras.
 */
typedef struct {
    int type;
    const char *so_file;
    int cl_type;
    int cl_format;
    gfloat max_frame_rate;
    gboolean has_camram;
} pco_cl_map_entry;

struct _UcaPcoCameraPrivate {
    GError *construct_error;
    pco_handle pco;
    pco_cl_map_entry *description;

    Fg_Struct *fg;
    guint fg_port;
    dma_mem *fg_mem;

    guint frame_width;
    guint frame_height;
    gsize buffer_size;
    guint16 *grab_buffer;

    guint16 width, height;
    guint16 width_ex, height_ex;
    guint16 binning_h, binning_v;
    guint16 roi_x, roi_y;
    guint16 roi_width, roi_height;
    guint16 roi_horizontal_steps, roi_vertical_steps;
    GValueArray *horizontal_binnings;
    GValueArray *vertical_binnings;
    GValueArray *pixelrates;

    frameindex_t last_frame;
    guint16 active_segment;
    guint num_recorded_images;
    guint current_image;

    guint16 delay_timebase;
    guint16 exposure_timebase;
};

static pco_cl_map_entry pco_cl_map[] = {
    { CAMERATYPE_PCO_EDGE,       "libFullAreaGray8.so",  FG_CL_8BIT_FULL_10,        FG_GRAY,     30.0f, FALSE },
    { CAMERATYPE_PCO4000,        "libDualAreaGray16.so", FG_CL_SINGLETAP_16_BIT,    FG_GRAY16,    5.0f, TRUE  },
    { CAMERATYPE_PCO_DIMAX_STD,  "libDualAreaGray16.so", FG_CL_SINGLETAP_16_BIT,    FG_GRAY16, 1279.0f, TRUE  },
    { 0, NULL, 0, 0, 0.0f, FALSE }
};

static pco_cl_map_entry *get_pco_cl_map_entry(int camera_type)
{
    pco_cl_map_entry *entry = pco_cl_map;

    while (entry->type != 0) {
        if (entry->type == camera_type)
            return entry;
        entry++;
    }

    return NULL;
}

static guint
fill_binnings(UcaPcoCameraPrivate *priv)
{
    uint16_t *horizontal = NULL;
    uint16_t *vertical = NULL;
    guint num_horizontal, num_vertical;

    guint err = pco_get_possible_binnings(priv->pco,
            &horizontal, &num_horizontal,
            &vertical, &num_vertical);

    GValue val = {0};
    g_value_init(&val, G_TYPE_UINT);

    if (err == PCO_NOERROR) {
        priv->horizontal_binnings = g_value_array_new(num_horizontal);
        priv->vertical_binnings = g_value_array_new(num_vertical);

        for (guint i = 0; i < num_horizontal; i++) {
            g_value_set_uint(&val, horizontal[i]);
            g_value_array_append(priv->horizontal_binnings, &val);
        }

        for (guint i = 0; i < num_vertical; i++) {
            g_value_set_uint(&val, vertical[i]);
            g_value_array_append(priv->vertical_binnings, &val);
        }
    }

    free(horizontal);
    free(vertical);
    return err;
}

static void
fill_pixelrates(UcaPcoCameraPrivate *priv, guint32 rates[4], gint num_rates)
{
    GValue val = {0};
    g_value_init(&val, G_TYPE_UINT);
    priv->pixelrates = g_value_array_new (num_rates);

    for (gint i = 0; i < num_rates; i++) {
        g_value_set_uint(&val, (guint) rates[i]);
        g_value_array_append(priv->pixelrates, &val);
    }
}

static guint
override_temperature_range(UcaPcoCameraPrivate *priv)
{
    int16_t default_temp, min_temp, max_temp;
    guint err = pco_get_cooling_range(priv->pco, &default_temp, &min_temp, &max_temp);

    if (err == PCO_NOERROR) {
        GParamSpecInt *spec = (GParamSpecInt *) pco_properties[PROP_COOLING_POINT];
        spec->minimum = min_temp;
        spec->maximum = max_temp;
        spec->default_value = default_temp;
    }
    else
        g_warning ("Unable to retrieve cooling range");

    return err;
}

static void
property_override_default_guint_value (GObjectClass *oclass, const gchar *property_name, guint new_default)
{
    GParamSpecUInt *pspec = G_PARAM_SPEC_UINT (g_object_class_find_property (oclass, property_name));

    if (pspec != NULL)
        pspec->default_value = new_default;
    else
        g_warning ("pspec for %s not found\n", property_name);
}

static void
override_maximum_adcs(UcaPcoCameraPrivate *priv)
{
    GParamSpecInt *spec = (GParamSpecInt *) pco_properties[PROP_SENSOR_ADCS];
    spec->maximum = pco_get_maximum_number_of_adcs(priv->pco);
}

static gdouble
convert_timebase(guint16 timebase)
{
    switch (timebase) {
        case TIMEBASE_NS:
            return 1e-9;
        case TIMEBASE_US:
            return 1e-6;
        case TIMEBASE_MS:
            return 1e-3;
        default:
            g_warning("Unknown timebase");
    }
    return 1e-3;
}

static void
read_timebase(UcaPcoCameraPrivate *priv)
{
    pco_get_timebase(priv->pco, &priv->delay_timebase, &priv->exposure_timebase);
}

static gboolean
check_timebase (gdouble time, gdouble scale)
{
    const gdouble EPSILON = 1e-3;
    gdouble scaled = time * scale;
    return scaled >= 1.0 && (scaled - ((int) scaled)) < EPSILON;
}

static guint16
get_suitable_timebase(gdouble time)
{
    if (check_timebase (time, 1e3))
        return TIMEBASE_MS;
    if (check_timebase (time, 1e6))
        return TIMEBASE_US;
    if (check_timebase (time, 1e9))
        return TIMEBASE_NS;
    return TIMEBASE_INVALID;
}

static gdouble
get_internal_delay (UcaPcoCamera *camera)
{
    if (camera->priv->description->type == CAMERATYPE_PCO_DIMAX_STD) {
        guint sensor_rate;
        g_object_get (camera, "sensor-pixelrate", &sensor_rate, NULL);

        if (sensor_rate == 55000000.0)
            return 0.000079;
        else if (sensor_rate == 62500000.0)
            return 0.000036;
    }

    return 0.0;
}

static int
fg_callback(frameindex_t frame, struct fg_apc_data *apc)
{
    UcaCamera *camera = UCA_CAMERA(apc);
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);
    gpointer data = Fg_getImagePtrEx(priv->fg, frame, priv->fg_port, priv->fg_mem);

    if (priv->description->type != CAMERATYPE_PCO_EDGE)
        camera->grab_func(data, camera->user_data);
    else {
        pco_get_reorder_func(priv->pco)(priv->grab_buffer, data, priv->frame_width, priv->frame_height);
        camera->grab_func(priv->grab_buffer, camera->user_data);
    }

    return 0;
}

static gboolean
setup_fg_callback(UcaCamera *camera)
{
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);
    struct FgApcControl ctrl;

    /* Jeez, as if a NULL pointer would not be good enough. */
    ctrl.data = (struct fg_apc_data *) camera;
    ctrl.version = 0;
    ctrl.func = &fg_callback;
    ctrl.flags = FG_APC_DEFAULTS;
    ctrl.timeout = 1;

    if (priv->grab_buffer)
        g_free (priv->grab_buffer);

    priv->grab_buffer = g_malloc0 (priv->buffer_size);

    return Fg_registerApcHandler(priv->fg, priv->fg_port, &ctrl, FG_APC_CONTROL_BASIC) == FG_OK;
}

static void
check_pco_property_error (guint err, guint property_id)
{
    if (err != PCO_NOERROR) {
        g_warning ("Call to libpco failed with error code %x for property `%s'",
                   err, pco_properties[property_id]->name);
    }
}

static gboolean
is_type (UcaPcoCameraPrivate *priv, int type)
{
    return priv->description->type == type;
}

static void
uca_pco_camera_start_recording (UcaCamera *camera, GError **error)
{
    UcaPcoCameraPrivate *priv;
    guint16 binned_width;
    guint16 binned_height;
    gboolean use_extended;
    gboolean transfer_async;
    guint err;

    g_return_if_fail (UCA_IS_PCO_CAMERA (camera));

    priv = UCA_PCO_CAMERA_GET_PRIVATE (camera);

    g_object_get (camera,
                  "sensor-extended", &use_extended,
                  "transfer-asynchronously", &transfer_async,
                  NULL);

    if (use_extended) {
        binned_width = priv->width_ex;
        binned_height = priv->height_ex;
    }
    else {
        binned_width = priv->width;
        binned_height = priv->height;
    }

    binned_width /= priv->binning_h;
    binned_height /= priv->binning_v;

    if ((priv->roi_x + priv->roi_width > binned_width) || (priv->roi_y + priv->roi_height > binned_height)) {
        g_set_error (error, UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_UNSUPPORTED,
                     "ROI of size %ix%i @ (%i, %i) is outside of (binned) sensor size %ix%i\n",
                     priv->roi_width, priv->roi_height, priv->roi_x, priv->roi_y, binned_width, binned_height);
        return;
    }

    /*
     * All parameters are valid. Now, set them on the camera.
     */
    guint16 roi[4] = { priv->roi_x + 1, priv->roi_y + 1, priv->roi_x + priv->roi_width, priv->roi_y + priv->roi_height };

    err = pco_set_roi (priv->pco, roi);
    CHECK_AND_RETURN_ON_PCO_ERROR (err);

    /*
     * FIXME: We cannot set the binning here as this breaks communication with
     * the camera. Setting the binning works _before_ initializing the frame
     * grabber though. However, it is rather inconvenient to initialize and
     * de-initialize the frame grabber for each recording sequence.
     */

    /* if (pco_set_binning(priv->pco, priv->binning_h, priv->binning_v) != PCO_NOERROR) */
    /*     g_warning("Cannot set binning\n"); */

    if (priv->frame_width != priv->roi_width || priv->frame_height != priv->roi_height || priv->fg_mem == NULL) {
        guint fg_width = priv->description->type == CAMERATYPE_PCO_EDGE ? 2 * priv->roi_width : priv->roi_width;

        priv->frame_width = priv->roi_width;
        priv->frame_height = priv->roi_height;
        priv->buffer_size = 2 * priv->frame_width * priv->frame_height;

        Fg_setParameter(priv->fg, FG_WIDTH, &fg_width, priv->fg_port);
        Fg_setParameter(priv->fg, FG_HEIGHT, &priv->frame_height, priv->fg_port);

        if (priv->fg_mem)
            Fg_FreeMemEx(priv->fg, priv->fg_mem);

        const guint num_buffers = 2;
        priv->fg_mem = Fg_AllocMemEx(priv->fg, num_buffers * priv->buffer_size, num_buffers);

        if (priv->fg_mem == NULL) {
            g_set_error(error, UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_FG_INIT,
                    "%s", Fg_getLastErrorDescription(priv->fg));
            g_object_unref(camera);
            return;
        }
    }

    if (transfer_async)
        setup_fg_callback (camera);

    if (is_type (priv, CAMERATYPE_PCO_DIMAX_STD) || is_type (priv, CAMERATYPE_PCO4000)) {
        err = pco_clear_active_segment (priv->pco);
        CHECK_AND_RETURN_ON_PCO_ERROR (err);
    }

    /*
     * Set the storage mode to FIFO buffer otherwise pco.4000 skips
     * frames that it is not able to send in time.
     */
    if (is_type (priv, CAMERATYPE_PCO4000)) {
        err = pco_set_storage_mode (priv->pco, STORAGE_MODE_FIFO_BUFFER);
        CHECK_AND_RETURN_ON_PCO_ERROR (err);
    }

    priv->last_frame = 0;

    err = pco_arm_camera (priv->pco);
    CHECK_AND_RETURN_ON_PCO_ERROR (err);

    err = pco_start_recording (priv->pco);
    CHECK_AND_RETURN_ON_PCO_ERROR (err);

    err = Fg_AcquireEx (priv->fg, priv->fg_port, GRAB_INFINITE, ACQ_STANDARD, priv->fg_mem);
    FG_SET_ERROR (err, priv->fg, UCA_PCO_CAMERA_ERROR_FG_ACQUISITION);
}

static void
uca_pco_camera_stop_recording (UcaCamera *camera, GError **error)
{
    UcaPcoCameraPrivate *priv;
    guint err;

    g_return_if_fail (UCA_IS_PCO_CAMERA (camera));

    priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);

    err = pco_stop_recording (priv->pco);
    CHECK_AND_RETURN_ON_PCO_ERROR (err);

    err = Fg_stopAcquireEx(priv->fg, priv->fg_port, priv->fg_mem, STOP_SYNC);
    FG_SET_ERROR (err, priv->fg, UCA_PCO_CAMERA_ERROR_FG_ACQUISITION);

    err = Fg_setStatusEx(priv->fg, FG_UNBLOCK_ALL, 0, priv->fg_port, priv->fg_mem);
    FG_SET_ERROR (err, priv->fg, UCA_PCO_CAMERA_ERROR_FG_ACQUISITION);
}

static void
uca_pco_camera_start_readout(UcaCamera *camera, GError **error)
{
    UcaPcoCameraPrivate *priv;
    guint err;

    g_return_if_fail (UCA_IS_PCO_CAMERA (camera));

    priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);

    /*
     * TODO: Check if readout mode is possible. This is not the case for the
     * edge.
     */

    err = pco_get_num_images (priv->pco, priv->active_segment, &priv->num_recorded_images);
    CHECK_AND_RETURN_ON_PCO_ERROR (err);

    err = Fg_AcquireEx (priv->fg, priv->fg_port, GRAB_INFINITE, ACQ_STANDARD, priv->fg_mem);
    FG_SET_ERROR (err, priv->fg, UCA_PCO_CAMERA_ERROR_FG_ACQUISITION);

    priv->last_frame = 0;
    priv->current_image = 1;
}

static void
uca_pco_camera_stop_readout(UcaCamera *camera, GError **error)
{
    UcaPcoCameraPrivate *priv;
    guint err;

    g_return_if_fail(UCA_IS_PCO_CAMERA(camera));

    priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);

    err = Fg_stopAcquireEx (priv->fg, priv->fg_port, priv->fg_mem, STOP_SYNC);
    FG_SET_ERROR (err, priv->fg, UCA_PCO_CAMERA_ERROR_FG_GENERAL);

    err = Fg_setStatusEx (priv->fg, FG_UNBLOCK_ALL, 0, priv->fg_port, priv->fg_mem);
    FG_SET_ERROR (err, priv->fg, UCA_PCO_CAMERA_ERROR_FG_GENERAL);
}

static void
uca_pco_camera_trigger (UcaCamera *camera, GError **error)
{
    UcaPcoCameraPrivate *priv;
    guint32 success;
    guint err;

    g_return_if_fail (UCA_IS_PCO_CAMERA (camera));

    priv = UCA_PCO_CAMERA_GET_PRIVATE (camera);

    /* TODO: Check if we can trigger */
    err = pco_force_trigger(priv->pco, &success);
    CHECK_AND_RETURN_ON_PCO_ERROR (err);

    if (!success) {
        g_set_error(error, UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_LIBPCO_GENERAL,
                    "Could not trigger frame acquisition");
    }
}

static gboolean
uca_pco_camera_grab(UcaCamera *camera, gpointer data, GError **error)
{
    static const gint MAX_TIMEOUT = 5;
    UcaPcoCameraPrivate *priv;
    gboolean is_readout;
    guint16 *frame;
    guint err;

    g_return_val_if_fail (UCA_IS_PCO_CAMERA(camera), FALSE);

    priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);

    g_object_get (G_OBJECT (camera), "is-readout", &is_readout, NULL);

    if (is_readout) {
        if (priv->current_image == priv->num_recorded_images)
            return FALSE;

        /*
         * No joke, the pco firmware allows to read a range of images but
         * implements only reading single images ...
         */
        pco_read_images(priv->pco, priv->active_segment, priv->current_image, priv->current_image);
        priv->current_image++;
    }
    else {
        err = pco_request_image (priv->pco);
        CHECK_AND_RETURN_VAL_ON_PCO_ERROR (err, FALSE);
    }

    priv->last_frame = Fg_getLastPicNumberBlockingEx(priv->fg, priv->last_frame + 1,
                                                     priv->fg_port, MAX_TIMEOUT, priv->fg_mem);

    if (priv->last_frame <= 0) {
        g_set_error (error, UCA_PCO_CAMERA_ERROR,
                     UCA_PCO_CAMERA_ERROR_FG_GENERAL,
                     "%s", Fg_getLastErrorDescription (priv->fg));
        return FALSE;
    }

    frame = Fg_getImagePtrEx (priv->fg, priv->last_frame, priv->fg_port, priv->fg_mem);

    if (priv->description->type == CAMERATYPE_PCO_EDGE)
        pco_get_reorder_func(priv->pco)((guint16 *) data, frame, priv->frame_width, priv->frame_height);
    else
        memcpy((gchar *) data, (gchar *) frame, priv->buffer_size);

    return TRUE;
}

static void
uca_pco_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(object);
    guint err = PCO_NOERROR;

    switch (property_id) {
        case PROP_SENSOR_EXTENDED:
            {
                guint16 format = g_value_get_boolean (value) ? SENSORFORMAT_EXTENDED : SENSORFORMAT_STANDARD;
                err = pco_set_sensor_format(priv->pco, format);
            }
            break;

        case PROP_ROI_X:
            priv->roi_x = g_value_get_uint(value);
            break;

        case PROP_ROI_Y:
            priv->roi_y = g_value_get_uint(value);
            break;

        case PROP_ROI_WIDTH:
            {
                guint width = g_value_get_uint(value);

                if (width % priv->roi_horizontal_steps)
                    g_warning("ROI width %i is not a multiple of %i", width, priv->roi_horizontal_steps);
                else
                    priv->roi_width = width;
            }
            break;

        case PROP_ROI_HEIGHT:
            {
                guint height = g_value_get_uint(value);

                if (height % priv->roi_vertical_steps)
                    g_warning("ROI height %i is not a multiple of %i", height, priv->roi_vertical_steps);
                else
                    priv->roi_height = height;
            }
            break;

        case PROP_SENSOR_HORIZONTAL_BINNING:
            priv->binning_h = g_value_get_uint(value);
            break;

        case PROP_SENSOR_VERTICAL_BINNING:
            priv->binning_v = g_value_get_uint(value);
            break;

        case PROP_EXPOSURE_TIME:
            {
                if (priv->description->type == CAMERATYPE_PCO4000) {
                    const gdouble time = g_value_get_double(value);

                    if (priv->exposure_timebase == TIMEBASE_INVALID)
                        read_timebase(priv);

                    /*
                     * Lets check if we can express the time in the current time
                     * base. If not, we need to adjust that.
                     */
                    guint16 suitable_timebase = get_suitable_timebase(time);

                    if (suitable_timebase == TIMEBASE_INVALID) {
                        g_warning("Cannot set such a small exposure time");
                    }
                    else {
                        if (suitable_timebase != priv->exposure_timebase) {
                            priv->exposure_timebase = suitable_timebase;
                            err = pco_set_timebase(priv->pco, priv->delay_timebase, suitable_timebase);
                            break;
                        }

                        gdouble timebase = convert_timebase(suitable_timebase);
                        guint32 timesteps = time / timebase;
                        err = pco_set_exposure_time(priv->pco, timesteps);
                    }
                }
                else {
                    uint32_t exposure;
                    uint32_t framerate;

                    err = pco_get_framerate (priv->pco, &framerate, &exposure);
                    exposure = (uint32_t) (g_value_get_double (value) * 1000 * 1000 * 1000);
                    err = pco_set_framerate (priv->pco, framerate, exposure, false);
                }
            }
            break;

        case PROP_FRAMES_PER_SECOND:
            {
                if (priv->description->type == CAMERATYPE_PCO4000) {
                    gdouble n_frames_per_second;
                    gdouble exposure_time;
                    gdouble delay;

                    /*
                     * We want to expose n frames in one second, each frame takes
                     * exposure time + delay time. Thus we have
                     *
                     * 1s = n * (t_exp + t_delay) <=> t_exp = 1s/n - t_delay.
                     */
                    delay = get_internal_delay (UCA_PCO_CAMERA (object));
                    n_frames_per_second = g_value_get_double (value);
                    exposure_time = 1.0 / n_frames_per_second - delay;

                    if (exposure_time <= 0.0)
                        g_warning ("Too many frames per second requested.");
                    else
                        g_object_set (object, "exposure-time", exposure_time, NULL);
                }
                else {
                    uint32_t exposure;
                    uint32_t framerate;

                    err = pco_get_framerate (priv->pco, &framerate, &exposure);
                    framerate = (uint32_t) (g_value_get_double (value) * 1000);
                    err = pco_set_framerate (priv->pco, framerate, exposure, true);
                }
            }
            break;

        case PROP_SENSOR_ADCS:
            {
                const guint num_adcs = g_value_get_uint(value);
                err = pco_set_adc_mode(priv->pco, num_adcs);
            }
            break;

        case PROP_SENSOR_PIXELRATE:
            {
                guint desired_pixel_rate = g_value_get_uint(value);
                guint32 pixel_rate = 0;

                for (guint i = 0; i < priv->pixelrates->n_values; i++) {
                    if (g_value_get_uint(g_value_array_get_nth(priv->pixelrates, i)) == desired_pixel_rate) {
                        pixel_rate = desired_pixel_rate;
                        break;
                    }
                }

                if (pixel_rate != 0) {
                    err = pco_set_pixelrate(priv->pco, pixel_rate);

                    if (err != PCO_NOERROR)
                        err = pco_reset(priv->pco);
                }
                else
                    g_warning("%i Hz is not possible. Please check the \"sensor-pixelrates\" property", desired_pixel_rate);
            }
            break;

        case PROP_DOUBLE_IMAGE_MODE:
            if (!pco_is_double_image_mode_available(priv->pco))
                g_warning("Double image mode is not available on this pco model");
            else
                err = pco_set_double_image_mode(priv->pco, g_value_get_boolean(value));
            break;

        case PROP_OFFSET_MODE:
            err = pco_set_offset_mode(priv->pco, g_value_get_boolean(value));
            break;

        case PROP_COOLING_POINT:
            {
                int16_t temperature = (int16_t) g_value_get_int(value);
                err = pco_set_cooling_temperature(priv->pco, temperature);
            }
            break;

        case PROP_RECORD_MODE:
            {
                /* TODO: setting this is not possible for the edge */
                UcaPcoCameraRecordMode mode = (UcaPcoCameraRecordMode) g_value_get_enum(value);

                if (mode == UCA_PCO_CAMERA_RECORD_MODE_SEQUENCE)
                    err = pco_set_record_mode(priv->pco, RECORDER_SUBMODE_SEQUENCE);
                else if (mode == UCA_PCO_CAMERA_RECORD_MODE_RING_BUFFER)
                    err = pco_set_record_mode(priv->pco, RECORDER_SUBMODE_RINGBUFFER);
                else
                    g_warning("Unknown record mode");
            }
            break;

        case PROP_STORAGE_MODE:
            {
                /* TODO: setting this is not possible for the edge */
                UcaPcoCameraStorageMode mode = (UcaPcoCameraStorageMode) g_value_get_enum(value);

                if (mode == UCA_PCO_CAMERA_STORAGE_MODE_FIFO_BUFFER)
                    err = pco_set_storage_mode (priv->pco, STORAGE_MODE_FIFO_BUFFER);
                else if (mode == UCA_PCO_CAMERA_STORAGE_MODE_RECORDER)
                    err = pco_set_storage_mode (priv->pco, STORAGE_MODE_RECORDER);
                else
                    g_warning("Unknown record mode");
            }
            break;

        case PROP_ACQUIRE_MODE:
            {
                UcaPcoCameraAcquireMode mode = (UcaPcoCameraAcquireMode) g_value_get_enum(value);

                if (mode == UCA_PCO_CAMERA_ACQUIRE_MODE_AUTO)
                    err = pco_set_acquire_mode(priv->pco, ACQUIRE_MODE_AUTO);
                else if (mode == UCA_PCO_CAMERA_ACQUIRE_MODE_EXTERNAL)
                    err = pco_set_acquire_mode(priv->pco, ACQUIRE_MODE_EXTERNAL);
                else
                    g_warning("Unknown acquire mode");
            }
            break;

        case PROP_FAST_SCAN:
            {
                guint32 mode;

                mode = g_value_get_boolean (value) ? PCO_SCANMODE_FAST : PCO_SCANMODE_SLOW;
                pco_set_scan_mode (priv->pco, mode);
            }
            break;

        case PROP_TRIGGER_MODE:
            {
                UcaCameraTrigger trigger_mode = (UcaCameraTrigger) g_value_get_enum(value);

                switch (trigger_mode) {
                    case UCA_CAMERA_TRIGGER_AUTO:
                        err = pco_set_trigger_mode(priv->pco, TRIGGER_MODE_AUTOTRIGGER);
                        break;
                    case UCA_CAMERA_TRIGGER_SOFTWARE:
                        err = pco_set_trigger_mode(priv->pco, TRIGGER_MODE_SOFTWARETRIGGER);
                        break;
                    case UCA_CAMERA_TRIGGER_EXTERNAL:
                        err = pco_set_trigger_mode(priv->pco, TRIGGER_MODE_EXTERNALTRIGGER);
                        break;
                }
            }
            break;

        case PROP_NOISE_FILTER:
            {
                guint16 filter_mode = g_value_get_boolean(value) ? NOISE_FILTER_MODE_ON : NOISE_FILTER_MODE_OFF;
                err = pco_set_noise_filter_mode(priv->pco, filter_mode);
            }
            break;

        case PROP_TIMESTAMP_MODE:
            {
                guint16 table[] = {
                    TIMESTAMP_MODE_OFF,
                    TIMESTAMP_MODE_BINARY,
                    TIMESTAMP_MODE_BINARYANDASCII,
                    TIMESTAMP_MODE_ASCII,
                };

                err = pco_set_timestamp_mode(priv->pco, table[g_value_get_enum(value)]);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }

    check_pco_property_error (err, property_id);
}

static void
uca_pco_camera_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaPcoCameraPrivate *priv;
    guint err = PCO_NOERROR;

    priv = UCA_PCO_CAMERA_GET_PRIVATE(object);

    /* Should fix #20 */
    if (uca_camera_is_recording (UCA_CAMERA (object))) {
        if (priv->description->type == CAMERATYPE_PCO_EDGE ||
            priv->description->type == CAMERATYPE_PCO4000) {
            return;
        }
    }

    switch (property_id) {
        case PROP_SENSOR_EXTENDED:
            {
                guint16 format;
                err = pco_get_sensor_format(priv->pco, &format);
                g_value_set_boolean(value, format == SENSORFORMAT_EXTENDED);
            }
            break;

        case PROP_SENSOR_WIDTH:
            g_value_set_uint(value, priv->width);
            break;

        case PROP_SENSOR_HEIGHT:
            g_value_set_uint(value, priv->height);
            break;

        case PROP_SENSOR_PIXEL_WIDTH:
            switch (priv->description->type) {
                case CAMERATYPE_PCO_EDGE:
                    g_value_set_double (value, 0.0000065);
                    break;
                case CAMERATYPE_PCO_DIMAX_STD:
                    g_value_set_double (value, 0.0000110);
                    break;
                case CAMERATYPE_PCO4000:
                    g_value_set_double (value, 0.0000090);
                    break;
            }
            break;

        case PROP_SENSOR_PIXEL_HEIGHT:
            switch (priv->description->type) {
                case CAMERATYPE_PCO_EDGE:
                    g_value_set_double (value, 0.0000065);
                    break;
                case CAMERATYPE_PCO_DIMAX_STD:
                    g_value_set_double (value, 0.0000110);
                    break;
                case CAMERATYPE_PCO4000:
                    g_value_set_double (value, 0.0000090);
                    break;
            }
            break;

        case PROP_SENSOR_WIDTH_EXTENDED:
            g_value_set_uint(value, priv->width_ex < priv->width ? priv->width : priv->width_ex);
            break;

        case PROP_SENSOR_HEIGHT_EXTENDED:
            g_value_set_uint(value, priv->height_ex < priv->height ? priv->height : priv->height_ex);
            break;

        case PROP_SENSOR_HORIZONTAL_BINNING:
            g_value_set_uint(value, priv->binning_h);
            break;

        case PROP_SENSOR_HORIZONTAL_BINNINGS:
            g_value_set_boxed(value, priv->horizontal_binnings);
            break;

        case PROP_SENSOR_VERTICAL_BINNING:
            g_value_set_uint(value, priv->binning_v);
            break;

        case PROP_SENSOR_VERTICAL_BINNINGS:
            g_value_set_boxed(value, priv->vertical_binnings);
            break;

        case PROP_SENSOR_BITDEPTH:
            switch (priv->description->type) {
                case CAMERATYPE_PCO4000:
                    g_value_set_uint(value, 14);
                    break;
                case CAMERATYPE_PCO_EDGE:
                    g_value_set_uint(value, 16);
                    break;
                case CAMERATYPE_PCO_DIMAX_STD:
                    g_value_set_uint(value, 12);
                    break;
            }
            break;

        case PROP_SENSOR_TEMPERATURE:
            {
                gint32 ccd, camera, power;
                err = pco_get_temperature(priv->pco, &ccd, &camera, &power);
                g_value_set_double(value, ccd / 10.0);
            }
            break;

        case PROP_SENSOR_ADCS:
            {
                /*
                 * Up to now, the ADC mode corresponds directly to the number of
                 * ADCs in use.
                 */
                pco_adc_mode mode;
                err = pco_get_adc_mode(priv->pco, &mode);
                g_value_set_uint(value, mode);
            }
            break;

        case PROP_SENSOR_MAX_ADCS:
            {
                GParamSpecUInt *spec = (GParamSpecUInt *) pco_properties[PROP_SENSOR_ADCS];
                g_value_set_uint(value, spec->maximum);
            }
            break;

        case PROP_SENSOR_PIXELRATES:
            g_value_set_boxed(value, priv->pixelrates);
            break;

        case PROP_SENSOR_PIXELRATE:
            {
                guint32 pixelrate;
                err = pco_get_pixelrate(priv->pco, &pixelrate);
                g_value_set_uint(value, pixelrate);
            }
            break;

        case PROP_EXPOSURE_TIME:
            {
                if (priv->description->type == CAMERATYPE_PCO4000) {
                    uint32_t exposure_time;
                    err = pco_get_exposure_time(priv->pco, &exposure_time);

                    if (priv->exposure_timebase == TIMEBASE_INVALID)
                        read_timebase(priv);

                    g_value_set_double(value, convert_timebase(priv->exposure_timebase) * exposure_time);
                }
                else {
                    uint32_t exposure;
                    uint32_t framerate;

                    err = pco_get_framerate (priv->pco, &framerate, &exposure);
                    g_value_set_double (value, exposure / 1000. / 1000. / 1000.);
                }
            }
            break;

        case PROP_FRAMES_PER_SECOND:
            {
                if (priv->description->type == CAMERATYPE_PCO4000) {
                    gdouble exposure_time;
                    gdouble delay;

                    delay = get_internal_delay (UCA_PCO_CAMERA (object));
                    g_object_get (object, "exposure-time", &exposure_time, NULL);
                    g_value_set_double (value, 1.0 / (exposure_time + delay));
                }
                else {
                    uint32_t exposure;
                    uint32_t framerate;

                    err = pco_get_framerate (priv->pco, &framerate, &exposure);
                    g_value_set_double (value, framerate / 1000.);
                }
            }
            break;

        case PROP_HAS_DOUBLE_IMAGE_MODE:
            g_value_set_boolean(value, pco_is_double_image_mode_available(priv->pco));
            break;

        case PROP_DOUBLE_IMAGE_MODE:
            if (!pco_is_double_image_mode_available(priv->pco))
                g_warning("Double image mode is not available on this pco model");
            else {
                bool on;
                err = pco_get_double_image_mode(priv->pco, &on);
                g_value_set_boolean(value, on);
            }
            break;

        case PROP_OFFSET_MODE:
            {
                bool on;
                err = pco_get_offset_mode(priv->pco, &on);
                g_value_set_boolean(value, on);
            }
            break;

        case PROP_HAS_STREAMING:
            g_value_set_boolean(value, TRUE);
            break;

        case PROP_HAS_CAMRAM_RECORDING:
            g_value_set_boolean(value, priv->description->has_camram);
            break;

        case PROP_RECORDED_FRAMES:
            err = pco_get_num_images (priv->pco, priv->active_segment, &priv->num_recorded_images);
            g_value_set_uint(value, priv->num_recorded_images);
            break;

        case PROP_RECORD_MODE:
            {
                guint16 mode;
                err = pco_get_record_mode(priv->pco, &mode);

                if (mode == RECORDER_SUBMODE_SEQUENCE)
                    g_value_set_enum(value, UCA_PCO_CAMERA_RECORD_MODE_SEQUENCE);
                else if (mode == RECORDER_SUBMODE_RINGBUFFER)
                    g_value_set_enum(value, UCA_PCO_CAMERA_RECORD_MODE_RING_BUFFER);
                else
                    g_warning("pco storage mode not handled");
            }
            break;

        case PROP_STORAGE_MODE:
            {
                guint16 mode;
                err = pco_get_storage_mode (priv->pco, &mode);

                if (mode == STORAGE_MODE_RECORDER)
                    g_value_set_enum (value, UCA_PCO_CAMERA_STORAGE_MODE_RECORDER);
                else if (mode == STORAGE_MODE_FIFO_BUFFER)
                    g_value_set_enum (value, UCA_PCO_CAMERA_STORAGE_MODE_FIFO_BUFFER);
                else
                    g_warning("pco record mode not handled");
            }
            break;

        case PROP_ACQUIRE_MODE:
            {
                guint16 mode;
                err = pco_get_acquire_mode(priv->pco, &mode);

                if (mode == ACQUIRE_MODE_AUTO)
                    g_value_set_enum(value, UCA_PCO_CAMERA_ACQUIRE_MODE_AUTO);
                else if (mode == ACQUIRE_MODE_EXTERNAL)
                    g_value_set_enum(value, UCA_PCO_CAMERA_ACQUIRE_MODE_EXTERNAL);
                else
                    g_warning("pco acquire mode not handled");
            }
            break;

        case PROP_FAST_SCAN:
            {
                guint32 mode;
                err = pco_get_scan_mode (priv->pco, &mode);
                g_value_set_boolean (value, mode == PCO_SCANMODE_FAST);
            }
            break;

        case PROP_TRIGGER_MODE:
            {
                guint16 mode;
                err = pco_get_trigger_mode(priv->pco, &mode);

                switch (mode) {
                    case TRIGGER_MODE_AUTOTRIGGER:
                        g_value_set_enum(value, UCA_CAMERA_TRIGGER_AUTO);
                        break;
                    case TRIGGER_MODE_SOFTWARETRIGGER:
                        g_value_set_enum(value, UCA_CAMERA_TRIGGER_SOFTWARE);
                        break;
                    case TRIGGER_MODE_EXTERNALTRIGGER:
                        g_value_set_enum(value, UCA_CAMERA_TRIGGER_EXTERNAL);
                        break;
                    default:
                        g_warning("pco trigger mode not handled");
                }
            }
            break;

        case PROP_ROI_X:
            g_value_set_uint(value, priv->roi_x);
            break;

        case PROP_ROI_Y:
            g_value_set_uint(value, priv->roi_y);
            break;

        case PROP_ROI_WIDTH:
            g_value_set_uint(value, priv->roi_width);
            break;

        case PROP_ROI_HEIGHT:
            g_value_set_uint(value, priv->roi_height);
            break;

        case PROP_ROI_WIDTH_MULTIPLIER:
            g_value_set_uint(value, priv->roi_horizontal_steps);
            break;

        case PROP_ROI_HEIGHT_MULTIPLIER:
            g_value_set_uint(value, priv->roi_vertical_steps);
            break;

        case PROP_NAME:
            {
                gchar *name = NULL;
                err = pco_get_name (priv->pco, &name);
                g_value_set_string (value, name);
                g_free(name);
            }
            break;

        case PROP_COOLING_POINT:
            {
                int16_t temperature;
                err = pco_get_cooling_temperature(priv->pco, &temperature);
                g_value_set_int(value, temperature);
            }
            break;

        case PROP_COOLING_POINT_MIN:
            {
                GParamSpecInt *spec = (GParamSpecInt *) pco_properties[PROP_COOLING_POINT];
                g_value_set_int(value, spec->minimum);
            }
            break;

        case PROP_COOLING_POINT_MAX:
            {
                GParamSpecInt *spec = (GParamSpecInt *) pco_properties[PROP_COOLING_POINT];
                g_value_set_int(value, spec->maximum);
            }
            break;

        case PROP_COOLING_POINT_DEFAULT:
            {
                GParamSpecInt *spec = (GParamSpecInt *) pco_properties[PROP_COOLING_POINT];
                g_value_set_int(value, spec->default_value);
            }
            break;

        case PROP_NOISE_FILTER:
            {
                guint16 mode;
                err = pco_get_noise_filter_mode(priv->pco, &mode);
                g_value_set_boolean(value, mode != NOISE_FILTER_MODE_OFF);
            }
            break;

        case PROP_TIMESTAMP_MODE:
            {
                guint16 mode;
                UcaPcoCameraTimestamp table[] = {
                    UCA_PCO_CAMERA_TIMESTAMP_NONE,
                    UCA_PCO_CAMERA_TIMESTAMP_BINARY,
                    UCA_PCO_CAMERA_TIMESTAMP_BOTH,
                    UCA_PCO_CAMERA_TIMESTAMP_ASCII
                };

                err = pco_get_timestamp_mode (priv->pco, &mode);
                g_value_set_enum (value, table[mode]);
            }
            break;

        case PROP_IS_RECORDING:
            {
                bool is_recording;

                err = pco_is_recording (priv->pco, &is_recording);
                g_value_set_boolean (value, (gboolean) is_recording);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }

    check_pco_property_error (err, property_id);
}

static void
uca_pco_camera_finalize(GObject *object)
{
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(object);

    if (priv->horizontal_binnings)
        g_value_array_free (priv->horizontal_binnings);

    if (priv->vertical_binnings)
        g_value_array_free (priv->vertical_binnings);

    if (priv->pixelrates)
        g_value_array_free (priv->pixelrates);

    if (priv->fg) {
        if (priv->fg_mem)
            Fg_FreeMemEx(priv->fg, priv->fg_mem);

        Fg_FreeGrabber (priv->fg);
    }

    if (priv->pco)
        pco_destroy (priv->pco);

    g_free (priv->grab_buffer);
    g_clear_error (&priv->construct_error);

    G_OBJECT_CLASS (uca_pco_camera_parent_class)->finalize (object);
}

static gboolean
uca_pco_camera_initable_init (GInitable *initable,
                              GCancellable *cancellable,
                              GError **error)
{
    UcaPcoCamera *camera;
    UcaPcoCameraPrivate *priv;

    g_return_val_if_fail (UCA_IS_PCO_CAMERA (initable), FALSE);

    if (cancellable != NULL) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             "Cancellable initialization not supported");
        return FALSE;
    }

    camera = UCA_PCO_CAMERA (initable);
    priv = camera->priv;

    if (priv->construct_error != NULL) {
        if (error)
            *error = g_error_copy (priv->construct_error);

        return FALSE;
    }

    return TRUE;
}

static void
uca_pco_camera_initable_iface_init (GInitableIface *iface)
{
    iface->init = uca_pco_camera_initable_init;
}

static void
uca_pco_camera_class_init(UcaPcoCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_pco_camera_set_property;
    gobject_class->get_property = uca_pco_camera_get_property;
    gobject_class->finalize = uca_pco_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_pco_camera_start_recording;
    camera_class->stop_recording = uca_pco_camera_stop_recording;
    camera_class->start_readout = uca_pco_camera_start_readout;
    camera_class->stop_readout = uca_pco_camera_stop_readout;
    camera_class->trigger = uca_pco_camera_trigger;
    camera_class->grab = uca_pco_camera_grab;

    for (guint i = 0; base_overrideables[i] != 0; i++)
        g_object_class_override_property(gobject_class, base_overrideables[i], uca_camera_props[base_overrideables[i]]);

    /**
     * UcaPcoCamera:sensor-extended:
     *
     * Activate larger sensor area that contains surrounding pixels for dark
     * references and dummies. Use #UcaPcoCamera:sensor-width-extended and
     * #UcaPcoCamera:sensor-height-extended to query the resolution of the
     * larger area.
     */
    pco_properties[PROP_SENSOR_EXTENDED] =
        g_param_spec_boolean("sensor-extended",
            "Use extended sensor format",
            "Use extended sensor format",
            FALSE, G_PARAM_READWRITE);

    pco_properties[PROP_SENSOR_WIDTH_EXTENDED] =
        g_param_spec_uint("sensor-width-extended",
            "Width of extended sensor",
            "Width of the extended sensor in pixels",
            1, G_MAXUINT, 1,
            G_PARAM_READABLE);

    pco_properties[PROP_SENSOR_HEIGHT_EXTENDED] =
        g_param_spec_uint("sensor-height-extended",
            "Height of extended sensor",
            "Height of the extended sensor in pixels",
            1, G_MAXUINT, 1,
            G_PARAM_READABLE);

    /**
     * UcaPcoCamera:sensor-pixelrate:
     *
     * Read and write the pixel rate or clock of the sensor in terms of Hertz.
     * Make sure to query the possible pixel rates through the
     * #UcaPcoCamera:sensor-pixelrates property. Any other value will be
     * rejected by the camera.
     */
    pco_properties[PROP_SENSOR_PIXELRATE] =
        g_param_spec_uint("sensor-pixelrate",
            "Pixel rate",
            "Pixel rate",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    pco_properties[PROP_SENSOR_PIXELRATES] =
        g_param_spec_value_array("sensor-pixelrates",
            "Array of possible sensor pixel rates",
            "Array of possible sensor pixel rates",
            pco_properties[PROP_SENSOR_PIXELRATE],
            G_PARAM_READABLE);

    pco_properties[PROP_NAME] =
        g_param_spec_string("name",
            "Name of the camera",
            "Name of the camera",
            "", G_PARAM_READABLE);

    pco_properties[PROP_SENSOR_TEMPERATURE] =
        g_param_spec_double("sensor-temperature",
            "Temperature of the sensor",
            "Temperature of the sensor in degree Celsius",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
            G_PARAM_READABLE);

    pco_properties[PROP_HAS_DOUBLE_IMAGE_MODE] =
        g_param_spec_boolean("has-double-image-mode",
            "Is double image mode supported by this model",
            "Is double image mode supported by this model",
            FALSE, G_PARAM_READABLE);

    pco_properties[PROP_DOUBLE_IMAGE_MODE] =
        g_param_spec_boolean("double-image-mode",
            "Use double image mode",
            "Use double image mode",
            FALSE, G_PARAM_READWRITE);

    pco_properties[PROP_OFFSET_MODE] =
        g_param_spec_boolean("offset-mode",
            "Use offset mode",
            "Use offset mode",
            FALSE, G_PARAM_READWRITE);

    pco_properties[PROP_RECORD_MODE] =
        g_param_spec_enum("record-mode",
            "Record mode",
            "Record mode",
            UCA_TYPE_PCO_CAMERA_RECORD_MODE, UCA_PCO_CAMERA_RECORD_MODE_SEQUENCE,
            G_PARAM_READWRITE);

    pco_properties[PROP_STORAGE_MODE] =
        g_param_spec_enum("storage-mode",
            "Storage mode",
            "Storage mode",
            UCA_TYPE_PCO_CAMERA_STORAGE_MODE, UCA_PCO_CAMERA_STORAGE_MODE_FIFO_BUFFER,
            G_PARAM_READWRITE);

    pco_properties[PROP_ACQUIRE_MODE] =
        g_param_spec_enum("acquire-mode",
            "Acquire mode",
            "Acquire mode",
            UCA_TYPE_PCO_CAMERA_ACQUIRE_MODE, UCA_PCO_CAMERA_ACQUIRE_MODE_AUTO,
            G_PARAM_READWRITE);

    pco_properties[PROP_FAST_SCAN] =
        g_param_spec_boolean("fast-scan",
            "Use fast scan mode",
            "Use fast scan mode with less dynamic range",
            FALSE, G_PARAM_READWRITE);

    pco_properties[PROP_NOISE_FILTER] =
        g_param_spec_boolean("noise-filter",
            "Noise filter",
            "Noise filter",
            FALSE, G_PARAM_READWRITE);

    /**
     * UcaPcoCamera:cooling-point:
     *
     * The value range is set arbitrarily, because we are not yet connected to
     * the camera and just don't know the cooling range. We override these
     * values in #uca_pco_camera_new().
     */
    pco_properties[PROP_COOLING_POINT] =
        g_param_spec_int("cooling-point",
            "Cooling point of the camera",
            "Cooling point of the camera in degree celsius",
            0, 10, 5, G_PARAM_READWRITE);

    pco_properties[PROP_COOLING_POINT_MIN] =
        g_param_spec_int("cooling-point-min",
            "Minimum cooling point",
            "Minimum cooling point in degree celsius",
            G_MININT, G_MAXINT, 0, G_PARAM_READABLE);

    pco_properties[PROP_COOLING_POINT_MAX] =
        g_param_spec_int("cooling-point-max",
            "Maximum cooling point",
            "Maximum cooling point in degree celsius",
            G_MININT, G_MAXINT, 0, G_PARAM_READABLE);

    pco_properties[PROP_COOLING_POINT_DEFAULT] =
        g_param_spec_int("cooling-point-default",
            "Default cooling point",
            "Default cooling point in degree celsius",
            G_MININT, G_MAXINT, 0, G_PARAM_READABLE);

    pco_properties[PROP_SENSOR_ADCS] =
        g_param_spec_uint("sensor-adcs",
            "Number of ADCs to use",
            "Number of ADCs to use",
            1, 2, 1,
            G_PARAM_READWRITE);

    pco_properties[PROP_SENSOR_MAX_ADCS] =
        g_param_spec_uint("sensor-max-adcs",
            "Maximum number of ADCs",
            "Maximum number of ADCs that can be set with \"sensor-adcs\"",
            1, G_MAXUINT, 1,
            G_PARAM_READABLE);

    pco_properties[PROP_TIMESTAMP_MODE] =
        g_param_spec_enum("timestamp-mode",
            "Timestamp mode",
            "Timestamp mode",
            UCA_TYPE_PCO_CAMERA_TIMESTAMP, UCA_PCO_CAMERA_TIMESTAMP_NONE,
            G_PARAM_READWRITE);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property(gobject_class, id, pco_properties[id]);

    g_type_class_add_private(klass, sizeof(UcaPcoCameraPrivate));
}

static gboolean
setup_pco_camera (UcaPcoCameraPrivate *priv)
{
    pco_cl_map_entry *map_entry;
    guint16 roi[4];
    guint16 camera_type;
    guint16 camera_subtype;

    priv->pco = pco_init();

    if (priv->pco == NULL) {
        g_set_error (&priv->construct_error,
                     UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_LIBPCO_INIT,
                     "Initializing libpco failed");
        return FALSE;
    }

    pco_get_camera_type (priv->pco, &camera_type, &camera_subtype);
    map_entry = get_pco_cl_map_entry (camera_type);

    if (map_entry == NULL) {
        g_set_error (&priv->construct_error,
                     UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_UNSUPPORTED,
                     "Camera type is not supported");
        return FALSE;
    }

    priv->description = map_entry;

    pco_get_active_segment (priv->pco, &priv->active_segment);
    pco_get_resolution (priv->pco, &priv->width, &priv->height, &priv->width_ex, &priv->height_ex);
    pco_get_binning (priv->pco, &priv->binning_h, &priv->binning_v);
    pco_set_storage_mode (priv->pco, STORAGE_MODE_FIFO_BUFFER);
    pco_set_auto_transfer (priv->pco, 1);

    pco_get_roi (priv->pco, roi);
    pco_get_roi_steps (priv->pco, &priv->roi_horizontal_steps, &priv->roi_vertical_steps);

    priv->roi_x = roi[0] - 1;
    priv->roi_y = roi[1] - 1;
    priv->roi_width = roi[2] - roi[0] + 1;
    priv->roi_height = roi[3] - roi[1] + 1;
    priv->num_recorded_images = 0;

    return TRUE;
}

static gboolean
setup_frame_grabber (UcaPcoCameraPrivate *priv)
{
    guint32 fg_width;
    guint32 fg_height;
    int val;

    priv->fg_port = PORT_A;
    priv->fg = Fg_Init (priv->description->so_file, priv->fg_port);

    if (priv->fg == NULL) {
        g_set_error (&priv->construct_error, UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_FG_INIT,
                     "%s", Fg_getLastErrorDescription(priv->fg));
        return FALSE;
    }

    FG_TRY_PARAM (priv->fg, &priv->construct_error, FG_CAMERA_LINK_CAMTYP, &priv->description->cl_type, priv->fg_port);
    FG_TRY_PARAM (priv->fg, &priv->construct_error, FG_FORMAT, &priv->description->cl_format, priv->fg_port);

    fg_width = priv->description->type == CAMERATYPE_PCO_EDGE ? priv->width * 2 : priv->width;
    FG_TRY_PARAM (priv->fg, &priv->construct_error, FG_WIDTH, &fg_width, priv->fg_port);

    fg_height = priv->height;
    FG_TRY_PARAM (priv->fg, &priv->construct_error, FG_HEIGHT, &fg_height, priv->fg_port);

    val = FREE_RUN;
    FG_TRY_PARAM (priv->fg, &priv->construct_error, FG_TRIGGERMODE, &val, priv->fg_port);

    return TRUE;
}

static void
override_property_ranges (UcaPcoCamera *camera)
{
    UcaPcoCameraPrivate *priv;
    GObjectClass *oclass;

    priv = UCA_PCO_CAMERA_GET_PRIVATE (camera);
    oclass = G_OBJECT_CLASS (UCA_PCO_CAMERA_GET_CLASS (camera));
    property_override_default_guint_value (oclass, "roi-width", priv->width);
    property_override_default_guint_value (oclass, "roi-height", priv->height);

    guint32 rates[4] = {0};
    gint num_rates = 0;

    if (pco_get_available_pixelrates (priv->pco, rates, &num_rates) == PCO_NOERROR) {
        fill_pixelrates (priv, rates, num_rates);
        property_override_default_guint_value (oclass, "sensor-pixelrate", rates[0]);
    }

    override_temperature_range (priv);
    override_maximum_adcs (priv);
}

static void
uca_pco_camera_init (UcaPcoCamera *self)
{
    UcaCamera *camera;
    UcaPcoCameraPrivate *priv;

    self->priv = priv = UCA_PCO_CAMERA_GET_PRIVATE (self);

    priv->fg_mem = NULL;
    priv->description = NULL;
    priv->last_frame = 0;
    priv->grab_buffer = NULL;
    priv->delay_timebase = TIMEBASE_INVALID;
    priv->exposure_timebase = TIMEBASE_INVALID;
    priv->construct_error = NULL;

    if (!setup_pco_camera (priv))
        return;

    if (!setup_frame_grabber (priv))
        return;

    fill_binnings (priv);
    override_property_ranges (self);

    camera = UCA_CAMERA (self);
    uca_camera_register_unit (camera, "sensor-width-extended", UCA_UNIT_PIXEL);
    uca_camera_register_unit (camera, "sensor-height-extended", UCA_UNIT_PIXEL);
    uca_camera_register_unit (camera, "sensor-temperature", UCA_UNIT_DEGREE_CELSIUS);
    uca_camera_register_unit (camera, "cooling-point", UCA_UNIT_DEGREE_CELSIUS);
    uca_camera_register_unit (camera, "cooling-point-min", UCA_UNIT_DEGREE_CELSIUS);
    uca_camera_register_unit (camera, "cooling-point-max", UCA_UNIT_DEGREE_CELSIUS);
    uca_camera_register_unit (camera, "cooling-point-default", UCA_UNIT_DEGREE_CELSIUS);
    uca_camera_register_unit (camera, "sensor-adcs", UCA_UNIT_COUNT);
    uca_camera_register_unit (camera, "sensor-max-adcs", UCA_UNIT_COUNT);
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_PCO_CAMERA;
}
