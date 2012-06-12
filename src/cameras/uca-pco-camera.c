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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libpco/libpco.h>
#include <libpco/sc2_defs.h>
#include <fgrab_struct.h>
#include <fgrab_prototyp.h>
#include "uca-camera.h"
#include "uca-pco-camera.h"
#include "uca-enums.h"

#define FG_TRY_PARAM(fg, camobj, param, val_addr, port)     \
    { int r = Fg_setParameter(fg, param, val_addr, port);   \
        if (r != FG_OK) {                                   \
            g_set_error(error, UCA_PCO_CAMERA_ERROR,        \
                    UCA_PCO_CAMERA_ERROR_FG_GENERAL,        \
                    "%s", Fg_getLastErrorDescription(fg));  \
            g_object_unref(camobj);                         \
            return NULL;                                    \
        } }

#define FG_SET_ERROR(err, fg, err_type)                 \
    if (err != FG_OK) {                                 \
        g_set_error(error, UCA_PCO_CAMERA_ERROR,        \
                err_type,                               \
                "%s", Fg_getLastErrorDescription(fg));  \
        return;                                         \
    }

#define HANDLE_PCO_ERROR(err)                       \
    if ((err) != PCO_NOERROR) {                     \
        g_set_error(error, UCA_PCO_CAMERA_ERROR,    \
                UCA_PCO_CAMERA_ERROR_LIBPCO_GENERAL,\
                "libpco error %x", err);            \
        return;                                     \
    }
    
#define UCA_PCO_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_PCO_CAMERA, UcaPcoCameraPrivate))

G_DEFINE_TYPE(UcaPcoCamera, uca_pco_camera, UCA_TYPE_CAMERA)

#define TIMEBASE_INVALID 0xDEAD

/**
 * UcaPcoCameraRecordMode:
 * @UCA_PCO_CAMERA_RECORD_MODE_SEQUENCE: Store all frames and stop if necessary
 * @UCA_PCO_CAMERA_RECORD_MODE_RING_BUFFER: Store frames in ring-buffer fashion
 *      and overwrite if necessary
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
    PROP_NAME = N_BASE_PROPERTIES,
    PROP_SENSOR_EXTENDED,
    PROP_SENSOR_WIDTH_EXTENDED,
    PROP_SENSOR_HEIGHT_EXTENDED,
    PROP_SENSOR_TEMPERATURE,
    PROP_SENSOR_PIXELRATES,
    PROP_SENSOR_PIXELRATE,
    PROP_SENSOR_ADCS,
    PROP_DELAY_TIME,
    PROP_HAS_DOUBLE_IMAGE_MODE,
    PROP_DOUBLE_IMAGE_MODE,
    PROP_OFFSET_MODE,
    PROP_RECORD_MODE,
    PROP_ACQUIRE_MODE,
    PROP_COOLING_POINT,
    PROP_NOISE_FILTER,
    PROP_TIMESTAMP_MODE,
    N_PROPERTIES
};

static gint base_overrideables[] = {
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_BITDEPTH,
    PROP_SENSOR_HORIZONTAL_BINNING,
    PROP_SENSOR_HORIZONTAL_BINNINGS,
    PROP_SENSOR_VERTICAL_BINNING,
    PROP_SENSOR_VERTICAL_BINNINGS,
    PROP_SENSOR_MAX_FRAME_RATE,
    PROP_EXPOSURE_TIME,
    PROP_TRIGGER_MODE,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    PROP_HAS_STREAMING,
    PROP_HAS_CAMRAM_RECORDING,
    0
};

static GParamSpec *pco_properties[N_PROPERTIES] = { NULL, };

/*
 * This structure defines type-specific properties of PCO cameras.
 */
typedef struct {
    int camera_type;
    const char *so_file;
    int cl_type;
    int cl_format;
    gfloat max_frame_rate;
    gboolean has_camram;
} pco_cl_map_entry;

struct _UcaPcoCameraPrivate {
    pco_handle pco;
    pco_cl_map_entry *camera_description;

    Fg_Struct *fg;
    guint fg_port;
    dma_mem *fg_mem;

    guint frame_width;
    guint frame_height;
    gsize num_bytes;

    guint16 width, height;
    guint16 width_ex, height_ex;
    guint16 binning_h, binning_v;
    guint16 roi_x, roi_y;
    guint16 roi_width, roi_height;
    GValueArray *horizontal_binnings;
    GValueArray *vertical_binnings;
    GValueArray *pixelrates;

    /* The time bases are given as pco time bases (TIMEBASE_NS and so on) */
    guint16 delay_timebase;
    guint16 exposure_timebase;

    frameindex_t last_frame;
    guint16 active_segment;
    guint num_recorded_images;
    guint current_image;
};

static pco_cl_map_entry pco_cl_map[] = { 
    { CAMERATYPE_PCO_EDGE,       "libFullAreaGray8.so",  FG_CL_8BIT_FULL_10,        FG_GRAY,     30.0f, FALSE },
    { CAMERATYPE_PCO4000,        "libDualAreaGray16.so", FG_CL_SINGLETAP_16_BIT,    FG_GRAY16,    5.0f, TRUE },
    { CAMERATYPE_PCO_DIMAX_STD,  "libFullAreaGray16.so", FG_CL_SINGLETAP_8_BIT,     FG_GRAY16, 1279.0f, TRUE },
    { 0, NULL, 0, 0, 0.0f, FALSE }
};

static pco_cl_map_entry *get_pco_cl_map_entry(int camera_type)
{
    pco_cl_map_entry *entry = pco_cl_map;

    while (entry->camera_type != 0) {
        if (entry->camera_type == camera_type)
            return entry;
        entry++; 
    }

    return NULL;
}

static guint fill_binnings(UcaPcoCameraPrivate *priv)
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

static guint fill_pixelrates(UcaPcoCameraPrivate *priv)
{
    guint32 rates[4] = {0};
    gint num_rates = 0;
    guint err = pco_get_available_pixelrates(priv->pco, rates, &num_rates);
    GValue val = {0};
    g_value_init(&val, G_TYPE_UINT);
    priv->pixelrates = g_value_array_new(num_rates);

    if (err == PCO_NOERROR) {
        for (gint i = 0; i < num_rates; i++) {
            g_value_set_uint(&val, (guint) rates[i]); 
            g_value_array_append(priv->pixelrates, &val);
        }
    }

    return err;
}

static guint override_temperature_range(UcaPcoCameraPrivate *priv)
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
        g_warning("Unable to retrieve cooling range");

    return err;
}

static void override_maximum_adcs(UcaPcoCameraPrivate *priv)
{
    GParamSpecInt *spec = (GParamSpecInt *) pco_properties[PROP_SENSOR_ADCS];
    spec->maximum = pco_get_maximum_number_of_adcs(priv->pco);
}

static gdouble convert_timebase(guint16 timebase)
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

static void read_timebase(UcaPcoCameraPrivate *priv)
{
    pco_get_timebase(priv->pco, &priv->delay_timebase, &priv->exposure_timebase);
}

static gdouble get_suitable_timebase(gdouble time)
{
    if (time * 1e3 >= 1.0)
        return TIMEBASE_MS;
    if (time * 1e6 >= 1.0)
        return TIMEBASE_US;
    if (time * 1e9 >= 1.0)
        return TIMEBASE_NS;
    return TIMEBASE_INVALID;
}

UcaPcoCamera *uca_pco_camera_new(GError **error)
{
    pco_handle pco = pco_init();

    if (pco == NULL) {
        g_set_error(error, UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_LIBPCO_INIT,
                "Initializing libpco failed");
        return NULL;
    }

    UcaPcoCamera *camera = g_object_new(UCA_TYPE_PCO_CAMERA, NULL);
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);
    priv->pco = pco;

    pco_get_resolution(priv->pco, &priv->width, &priv->height, &priv->width_ex, &priv->height_ex);
    pco_get_binning(priv->pco, &priv->binning_h, &priv->binning_v);
    pco_set_storage_mode(pco, STORAGE_MODE_RECORDER);
    pco_set_auto_transfer(pco, 1);

    guint16 roi[4];
    pco_get_roi(priv->pco, roi);
    priv->roi_x = roi[0] - 1;
    priv->roi_y = roi[1] - 1;
    priv->roi_width = roi[2] - roi[0] + 1;
    priv->roi_height = roi[3] - roi[1] + 1;

    guint16 camera_type, camera_subtype;
    pco_get_camera_type(priv->pco, &camera_type, &camera_subtype);
    pco_cl_map_entry *map_entry = get_pco_cl_map_entry(camera_type);
    priv->camera_description = map_entry;

    if (map_entry == NULL) {
        g_set_error(error, UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_UNSUPPORTED,
                "Camera type is not supported");
        g_object_unref(camera);
        return NULL;
    }

    priv->fg_port = PORT_A;
    priv->fg = Fg_Init(map_entry->so_file, priv->fg_port);

    if (priv->fg == NULL) {
        g_set_error(error, UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_FG_INIT,
                "%s", Fg_getLastErrorDescription(priv->fg));
        g_object_unref(camera);
        return NULL;
    }

    const guint32 fg_height = priv->height;
    const guint32 fg_width = camera_type == CAMERATYPE_PCO_EDGE ? priv->width * 2 : priv->width;

    FG_TRY_PARAM(priv->fg, camera, FG_CAMERA_LINK_CAMTYP, &map_entry->cl_type, priv->fg_port);
    FG_TRY_PARAM(priv->fg, camera, FG_FORMAT, &map_entry->cl_format, priv->fg_port);
    FG_TRY_PARAM(priv->fg, camera, FG_WIDTH, &fg_width, priv->fg_port);
    FG_TRY_PARAM(priv->fg, camera, FG_HEIGHT, &fg_height, priv->fg_port);

    int val = FREE_RUN;
    FG_TRY_PARAM(priv->fg, camera, FG_TRIGGERMODE, &val, priv->fg_port);

    fill_binnings(priv);
    fill_pixelrates(priv);

    /*
     * Here we override property ranges because we didn't know them at property
     * installation time.
     */
    override_temperature_range(priv);
    override_maximum_adcs(priv);

    return camera;
}

static void uca_pco_camera_start_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_PCO_CAMERA(camera));
    guint err = PCO_NOERROR;

    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);

    guint16 binned_width = priv->width / priv->binning_h;
    guint16 binned_height = priv->height / priv->binning_v;

    if ((priv->roi_x + priv->roi_width > binned_width) || (priv->roi_y + priv->roi_height > binned_height)) {
        g_set_error(error, UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_UNSUPPORTED,
                "ROI of size %ix%i @ (%i, %i) is outside of (binned) sensor size %ix%i\n",
                priv->roi_width, priv->roi_height, priv->roi_x, priv->roi_y, binned_width, binned_height);
        return;
    }

    /*
     * All parameters are valid. Now, set them on the camera.
     */
    guint16 roi[4] = { priv->roi_x + 1, priv->roi_y + 1, priv->roi_x + priv->roi_width, priv->roi_y + priv->roi_height };
    pco_set_roi(priv->pco, roi);

    /*
     * FIXME: We cannot set the binning here as this breaks communication with
     * the camera. Setting the binning works _before_ initializing the frame
     * grabber though. However, it is rather inconvenient to initialize and
     * de-initialize the frame grabber for each recording sequence.
     */

    /* if (pco_set_binning(priv->pco, priv->binning_h, priv->binning_v) != PCO_NOERROR) */
    /*     g_warning("Cannot set binning\n"); */

    if (priv->frame_width != priv->roi_width || priv->frame_height != priv->roi_height || priv->fg_mem == NULL) {
        priv->frame_width = priv->roi_width;
        priv->frame_height = priv->roi_height;
        priv->num_bytes = 2;

        Fg_setParameter(priv->fg, FG_WIDTH, &priv->frame_width, priv->fg_port);
        Fg_setParameter(priv->fg, FG_HEIGHT, &priv->frame_height, priv->fg_port);

        if (priv->fg_mem)
            Fg_FreeMemEx(priv->fg, priv->fg_mem);

        const guint num_buffers = 2;
        priv->fg_mem = Fg_AllocMemEx(priv->fg, 
                num_buffers * priv->frame_width * priv->frame_height * sizeof(uint16_t), num_buffers);

        if (priv->fg_mem == NULL) {
            g_set_error(error, UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_FG_INIT,
                    "%s", Fg_getLastErrorDescription(priv->fg));
            g_object_unref(camera);
            return;
        }
    }

    if ((priv->camera_description->camera_type == CAMERATYPE_PCO_DIMAX_STD) ||
        (priv->camera_description->camera_type == CAMERATYPE_PCO4000))
        pco_clear_active_segment(priv->pco);

    priv->last_frame = 0;

    err = pco_arm_camera(priv->pco);
    HANDLE_PCO_ERROR(err);

    err = pco_start_recording(priv->pco);
    HANDLE_PCO_ERROR(err);

    err = Fg_AcquireEx(priv->fg, priv->fg_port, GRAB_INFINITE, ACQ_STANDARD, priv->fg_mem);
    FG_SET_ERROR(err, priv->fg, UCA_PCO_CAMERA_ERROR_FG_ACQUISITION);
}

static void uca_pco_camera_stop_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_PCO_CAMERA(camera));
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);

    guint err = pco_stop_recording(priv->pco);
    HANDLE_PCO_ERROR(err);

    err = Fg_stopAcquireEx(priv->fg, priv->fg_port, priv->fg_mem, STOP_SYNC);
    FG_SET_ERROR(err, priv->fg, UCA_PCO_CAMERA_ERROR_FG_ACQUISITION);

    err = Fg_setStatusEx(priv->fg, FG_UNBLOCK_ALL, 0, priv->fg_port, priv->fg_mem);
    if (err == FG_INVALID_PARAMETER)
        g_warning(" Unable to unblock all\n");
}

static void uca_pco_camera_start_readout(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_PCO_CAMERA(camera));
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);

    /* 
     * TODO: Check if readout mode is possible. This is not the case for the
     * edge. 
     */

    guint err = pco_get_active_segment(priv->pco, &priv->active_segment);
    HANDLE_PCO_ERROR(err);

    err = pco_get_num_images(priv->pco, priv->active_segment, &priv->num_recorded_images);
    HANDLE_PCO_ERROR(err);

    priv->current_image = 1;
}

static void uca_pco_camera_trigger(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_PCO_CAMERA(camera));
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);

    /* TODO: Check if we can trigger */
    guint32 success = 0;
    pco_force_trigger(priv->pco, &success);

    if (!success)
        g_set_error(error, UCA_PCO_CAMERA_ERROR, UCA_PCO_CAMERA_ERROR_LIBPCO_GENERAL,
                "Could not trigger frame acquisition"); 
}

static void uca_pco_camera_grab(UcaCamera *camera, gpointer *data, GError **error)
{
    static const gint MAX_TIMEOUT = G_MAXINT;

    g_return_if_fail(UCA_IS_PCO_CAMERA(camera));
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(camera);

    gboolean is_readout = FALSE;
    g_object_get(G_OBJECT(camera), "is-readout", &is_readout, NULL);

    if (is_readout) {
        if (priv->current_image == priv->num_recorded_images) {
            *data = NULL;
            return;
        }

        /*
         * No joke, the pco firmware allows to read a range of images but
         * implements only reading single images ...
         */
        pco_read_images(priv->pco, priv->active_segment, 
                priv->current_image, priv->current_image);
        priv->current_image++;
    }

    pco_request_image(priv->pco);
    priv->last_frame = Fg_getLastPicNumberBlockingEx(priv->fg, priv->last_frame+1, priv->fg_port, MAX_TIMEOUT, priv->fg_mem);
    
    if (priv->last_frame <= 0) {
        guint err = FG_OK + 1;
        FG_SET_ERROR(err, priv->fg, UCA_PCO_CAMERA_ERROR_FG_GENERAL);
    }

    guint16 *frame = Fg_getImagePtrEx(priv->fg, priv->last_frame, priv->fg_port, priv->fg_mem);

    if (*data == NULL)
        *data = g_malloc0(priv->frame_width * priv->frame_height * priv->num_bytes); 

    if (priv->camera_description->camera_type == CAMERATYPE_PCO_EDGE)
        pco_get_reorder_func(priv->pco)((guint16 *) *data, frame, priv->frame_width, priv->frame_height);
    else
        memcpy((gchar *) *data, (gchar *) frame, priv->frame_width * priv->frame_height * priv->num_bytes);
}

static void uca_pco_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_SENSOR_EXTENDED:
            {
                guint16 format = g_value_get_boolean(value) ? SENSORFORMAT_EXTENDED : SENSORFORMAT_STANDARD; 
                pco_set_sensor_format(priv->pco, format);
            }
            break;

        case PROP_ROI_X:
            priv->roi_x = g_value_get_uint(value);
            break;

        case PROP_ROI_Y:
            priv->roi_y = g_value_get_uint(value);
            break;

        case PROP_ROI_WIDTH:
            priv->roi_width = g_value_get_uint(value);
            break;

        case PROP_ROI_HEIGHT:
            priv->roi_height = g_value_get_uint(value);
            break;

        case PROP_SENSOR_HORIZONTAL_BINNING:
            priv->binning_h = g_value_get_uint(value);
            break;

        case PROP_SENSOR_VERTICAL_BINNING:
            priv->binning_v = g_value_get_uint(value);
            break;

        case PROP_EXPOSURE_TIME:
            {
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
                        if (pco_set_timebase(priv->pco, priv->delay_timebase, suitable_timebase) != PCO_NOERROR)
                            g_warning("Cannot set exposure time base");
                    }

                    gdouble timebase = convert_timebase(suitable_timebase);
                    guint32 timesteps = time / timebase;
                    if (pco_set_exposure_time(priv->pco, timesteps) != PCO_NOERROR)
                        g_warning("Cannot set exposure time");
                }
            }
            break;

        case PROP_DELAY_TIME:
            {
                const gdouble time = g_value_get_double(value);
                 
                if (priv->delay_timebase == TIMEBASE_INVALID)
                    read_timebase(priv);

                /*
                 * Lets check if we can express the time in the current time
                 * base. If not, we need to adjust that.
                 */
                guint16 suitable_timebase = get_suitable_timebase(time);

                if (suitable_timebase == TIMEBASE_INVALID) {
                    if (time == 0.0) {
                        /*
                         * If we want to suppress any pre-exposure delays, we
                         * can set the 0 seconds in whatever time base that is
                         * currently active.
                         */
                        if (pco_set_delay_time(priv->pco, 0) != PCO_NOERROR)
                            g_warning("Cannot set zero delay time");
                    }
                    else
                        g_warning("Cannot set such a small exposure time"); 
                }
                else {
                    if (suitable_timebase != priv->delay_timebase) {
                        priv->delay_timebase = suitable_timebase;
                        if (pco_set_timebase(priv->pco, suitable_timebase, priv->exposure_timebase) != PCO_NOERROR)
                            g_warning("Cannot set delay time base");
                    }

                    gdouble timebase = convert_timebase(suitable_timebase);
                    guint32 timesteps = time / timebase;
                    if (pco_set_delay_time(priv->pco, timesteps) != PCO_NOERROR)
                        g_warning("Cannot set delay time");
                }
            }
            break;

        case PROP_SENSOR_ADCS:
            {
                const guint num_adcs = g_value_get_uint(value); 
                if (pco_set_adc_mode(priv->pco, num_adcs) != PCO_NOERROR)
                    g_warning("Cannot set the number of ADCs per pixel\n");
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
                    if (pco_set_pixelrate(priv->pco, pixel_rate) != PCO_NOERROR)
                        g_warning("Cannot set pixel rate");
                }
                else
                    g_warning("%i Hz is not possible. Please check the \"sensor-pixelrates\" property", desired_pixel_rate);
            }
            break;

        case PROP_DOUBLE_IMAGE_MODE:
            if (!pco_is_double_image_mode_available(priv->pco))
                g_warning("Double image mode is not available on this pco model");
            else
                pco_set_double_image_mode(priv->pco, g_value_get_boolean(value));
            break;

        case PROP_OFFSET_MODE:
            pco_set_offset_mode(priv->pco, g_value_get_boolean(value));
            break;

        case PROP_COOLING_POINT:
            {
                int16_t temperature = (int16_t) g_value_get_int(value); 
                pco_set_cooling_temperature(priv->pco, temperature);
            }
            break;

        case PROP_RECORD_MODE:
            {
                /* TODO: setting this is not possible for the edge */
                UcaPcoCameraRecordMode mode = (UcaPcoCameraRecordMode) g_value_get_enum(value);

                if (mode == UCA_PCO_CAMERA_RECORD_MODE_SEQUENCE)
                    pco_set_record_mode(priv->pco, RECORDER_SUBMODE_SEQUENCE);
                else if (mode == UCA_PCO_CAMERA_RECORD_MODE_RING_BUFFER)
                    pco_set_record_mode(priv->pco, RECORDER_SUBMODE_RINGBUFFER);
                else
                    g_warning("Unknown record mode");
            }
            break;

        case PROP_ACQUIRE_MODE:
            {
                UcaPcoCameraAcquireMode mode = (UcaPcoCameraAcquireMode) g_value_get_enum(value);
                unsigned int err = PCO_NOERROR;

                if (mode == UCA_PCO_CAMERA_ACQUIRE_MODE_AUTO)
                    err = pco_set_acquire_mode(priv->pco, ACQUIRE_MODE_AUTO);
                else if (mode == UCA_PCO_CAMERA_ACQUIRE_MODE_EXTERNAL)
                    err = pco_set_acquire_mode(priv->pco, ACQUIRE_MODE_EXTERNAL);
                else
                    g_warning("Unknown acquire mode");

                if (err != PCO_NOERROR)
                    g_warning("Cannot set acquire mode");
            }
            break;

        case PROP_TRIGGER_MODE:
            {
                UcaCameraTrigger trigger_mode = (UcaCameraTrigger) g_value_get_enum(value);

                switch (trigger_mode) {
                    case UCA_CAMERA_TRIGGER_AUTO:
                        pco_set_trigger_mode(priv->pco, TRIGGER_MODE_AUTOTRIGGER);
                        break;
                    case UCA_CAMERA_TRIGGER_INTERNAL:
                        pco_set_trigger_mode(priv->pco, TRIGGER_MODE_SOFTWARETRIGGER);
                        break;
                    case UCA_CAMERA_TRIGGER_EXTERNAL:
                        pco_set_trigger_mode(priv->pco, TRIGGER_MODE_EXTERNALTRIGGER);
                        break;
                }
            }
            break;

        case PROP_NOISE_FILTER:
            {
                guint16 filter_mode = g_value_get_boolean(value) ? NOISE_FILTER_MODE_ON : NOISE_FILTER_MODE_OFF;
                pco_set_noise_filter_mode(priv->pco, filter_mode);
            }
            break;

        case PROP_TIMESTAMP_MODE:
            {
                guint16 modes[] = {
                    TIMESTAMP_MODE_OFF,             /* 0 */ 
                    TIMESTAMP_MODE_BINARY,          /* 1 = 1 << 0 */ 
                    TIMESTAMP_MODE_ASCII,           /* 2 = 1 << 1 */ 
                    TIMESTAMP_MODE_BINARYANDASCII,  /* 3 = 1 << 0 | 1 << 1 */ 
                }; 
                pco_set_timestamp_mode(priv->pco, modes[g_value_get_flags(value)]);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }
}

static void uca_pco_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_SENSOR_EXTENDED:
            {
                guint16 format; 
                pco_get_sensor_format(priv->pco, &format);
                g_value_set_boolean(value, format == SENSORFORMAT_EXTENDED);
            }
            break;

        case PROP_SENSOR_WIDTH: 
            g_value_set_uint(value, priv->width);
            break;

        case PROP_SENSOR_HEIGHT: 
            g_value_set_uint(value, priv->height);
            break;

        case PROP_SENSOR_WIDTH_EXTENDED: 
            g_value_set_uint(value, priv->width_ex);
            break;

        case PROP_SENSOR_HEIGHT_EXTENDED: 
            g_value_set_uint(value, priv->height_ex);
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

        case PROP_SENSOR_MAX_FRAME_RATE:
            g_value_set_float(value, priv->camera_description->max_frame_rate);
            break;

        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint(value, 16);
            break;

        case PROP_SENSOR_TEMPERATURE:
            {
                guint32 ccd, camera, power;                 
                pco_get_temperature(priv->pco, &ccd, &camera, &power);
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
                if (pco_get_adc_mode(priv->pco, &mode) != PCO_NOERROR)
                    g_warning("Cannot read number of ADCs per pixel");
                g_value_set_uint(value, mode);
            }
            break;

        case PROP_SENSOR_PIXELRATES:
            g_value_set_boxed(value, priv->pixelrates);
            break;

        case PROP_SENSOR_PIXELRATE:
            {
                guint32 pixelrate; 
                pco_get_pixelrate(priv->pco, &pixelrate);
                g_value_set_uint(value, pixelrate);
            }
            break;

        case PROP_EXPOSURE_TIME:
            {
                uint32_t exposure_time;
                pco_get_exposure_time(priv->pco, &exposure_time);

                if (priv->exposure_timebase == TIMEBASE_INVALID)
                    read_timebase(priv);

                g_value_set_double(value, convert_timebase(priv->exposure_timebase) * exposure_time);
            }
            break;

        case PROP_DELAY_TIME:
            {
                uint32_t delay_time;
                pco_get_delay_time(priv->pco, &delay_time);

                if (priv->delay_timebase == TIMEBASE_INVALID)
                    read_timebase(priv);

                g_value_set_double(value, convert_timebase(priv->delay_timebase) * delay_time);
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
                pco_get_double_image_mode(priv->pco, &on);
                g_value_set_boolean(value, on);
            }
            break;

        case PROP_OFFSET_MODE:
            {
                bool on;
                pco_get_offset_mode(priv->pco, &on);
                g_value_set_boolean(value, on);
            }
            break;

        case PROP_HAS_STREAMING:
            g_value_set_boolean(value, TRUE);
            break;

        case PROP_HAS_CAMRAM_RECORDING:
            g_value_set_boolean(value, priv->camera_description->has_camram);
            break;

        case PROP_RECORD_MODE:
            {
                guint16 mode;
                pco_get_record_mode(priv->pco, &mode);

                if (mode == RECORDER_SUBMODE_SEQUENCE)
                    g_value_set_enum(value, UCA_PCO_CAMERA_RECORD_MODE_SEQUENCE);
                else if (mode == RECORDER_SUBMODE_RINGBUFFER)
                    g_value_set_enum(value, UCA_PCO_CAMERA_RECORD_MODE_RING_BUFFER);
                else
                    g_warning("pco record mode not handled");
            }
            break;

        case PROP_ACQUIRE_MODE:
            {
                guint16 mode;
                pco_get_acquire_mode(priv->pco, &mode);

                if (mode == ACQUIRE_MODE_AUTO)
                    g_value_set_enum(value, UCA_PCO_CAMERA_ACQUIRE_MODE_AUTO);
                else if (mode == ACQUIRE_MODE_EXTERNAL)
                    g_value_set_enum(value, UCA_PCO_CAMERA_ACQUIRE_MODE_EXTERNAL);
                else
                    g_warning("pco acquire mode not handled");
            }
            break;

        case PROP_TRIGGER_MODE:
            {
                guint16 mode;                 
                pco_get_trigger_mode(priv->pco, &mode);

                switch (mode) {
                    case TRIGGER_MODE_AUTOTRIGGER:
                        g_value_set_enum(value, UCA_CAMERA_TRIGGER_AUTO);
                        break;
                    case TRIGGER_MODE_SOFTWARETRIGGER:
                        g_value_set_enum(value, UCA_CAMERA_TRIGGER_INTERNAL);
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

        case PROP_NAME: 
            {
                gchar *name = NULL;
                pco_get_name(priv->pco, &name);
                g_value_set_string(value, name);
                g_free(name);
            }
            break;

        case PROP_COOLING_POINT:
            {
                int16_t temperature; 
                if (pco_get_cooling_temperature(priv->pco, &temperature) != PCO_NOERROR)
                    g_warning("Cannot read cooling temperature\n");
                g_value_set_int(value, temperature);
            }
            break;

        case PROP_NOISE_FILTER:
            {
                guint16 mode;
                pco_get_noise_filter_mode(priv->pco, &mode);
                g_value_set_boolean(value, mode != NOISE_FILTER_MODE_OFF);
            }
            break;

        case PROP_TIMESTAMP_MODE:
            {
                guint16 mode; 
                pco_get_timestamp_mode(priv->pco, &mode);

                switch (mode) {
                    case TIMESTAMP_MODE_OFF:
                        g_value_set_flags(value, UCA_PCO_CAMERA_TIMESTAMP_NONE);
                        break;
                    case TIMESTAMP_MODE_BINARY:
                        g_value_set_flags(value, UCA_PCO_CAMERA_TIMESTAMP_BINARY);
                        break;
                    case TIMESTAMP_MODE_BINARYANDASCII:
                        g_value_set_flags(value, 
                                UCA_PCO_CAMERA_TIMESTAMP_BINARY | UCA_PCO_CAMERA_TIMESTAMP_ASCII);
                        break;
                    case TIMESTAMP_MODE_ASCII:
                        g_value_set_flags(value, UCA_PCO_CAMERA_TIMESTAMP_ASCII);
                        break;
                }
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void uca_pco_camera_finalize(GObject *object)
{
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(object);

    if (priv->horizontal_binnings)
        g_value_array_free(priv->horizontal_binnings);

    if (priv->vertical_binnings)
        g_value_array_free(priv->vertical_binnings);

    if (priv->pixelrates)
        g_value_array_free(priv->pixelrates);

    if (priv->fg) {
        if (priv->fg_mem)
            Fg_FreeMemEx(priv->fg, priv->fg_mem);

        Fg_FreeGrabber(priv->fg);
    }

    if (priv->pco)
        pco_destroy(priv->pco);

    G_OBJECT_CLASS(uca_pco_camera_parent_class)->finalize(object);
}

static void uca_pco_camera_class_init(UcaPcoCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_pco_camera_set_property;
    gobject_class->get_property = uca_pco_camera_get_property;
    gobject_class->finalize = uca_pco_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_pco_camera_start_recording;
    camera_class->stop_recording = uca_pco_camera_stop_recording;
    camera_class->start_readout = uca_pco_camera_start_readout;
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

    pco_properties[PROP_ACQUIRE_MODE] = 
        g_param_spec_enum("acquire-mode", 
            "Acquire mode",
            "Acquire mode",
            UCA_TYPE_PCO_CAMERA_ACQUIRE_MODE, UCA_PCO_CAMERA_ACQUIRE_MODE_AUTO,
            G_PARAM_READWRITE);
    
    pco_properties[PROP_DELAY_TIME] =
        g_param_spec_double("delay-time",
            "Delay time",
            "Delay before starting actual exposure",
            0.0, G_MAXDOUBLE, 0.0,
            G_PARAM_READWRITE);

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
            "Cooling point of the camera",
            0, 10, 5, G_PARAM_READWRITE);
    
    pco_properties[PROP_SENSOR_ADCS] = 
        g_param_spec_uint("sensor-adcs",
            "Number of ADCs to use",
            "Number of ADCs to use",
            1, 2, 1, 
            G_PARAM_READWRITE);

    pco_properties[PROP_TIMESTAMP_MODE] =
        g_param_spec_flags("timestamp-mode",
            "Timestamp mode",
            "Timestamp mode",
            UCA_TYPE_PCO_CAMERA_TIMESTAMP, UCA_PCO_CAMERA_TIMESTAMP_NONE,
            G_PARAM_READWRITE);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property(gobject_class, id, pco_properties[id]);

    g_type_class_add_private(klass, sizeof(UcaPcoCameraPrivate));
}

static void uca_pco_camera_init(UcaPcoCamera *self)
{
    self->priv = UCA_PCO_CAMERA_GET_PRIVATE(self);
    self->priv->fg = NULL;
    self->priv->fg_mem = NULL;
    self->priv->pco = NULL;
    self->priv->horizontal_binnings = NULL;
    self->priv->vertical_binnings = NULL;
    self->priv->pixelrates = NULL;
    self->priv->camera_description = NULL;
    self->priv->last_frame = 0;

    self->priv->delay_timebase = TIMEBASE_INVALID;
    self->priv->exposure_timebase = TIMEBASE_INVALID;
}
