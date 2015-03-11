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
#include <pcilib.h>
#include <errno.h>
#include <math.h>
#include "uca-camera.h"
#include "uca-ufo-camera.h"

#define PCILIB_SET_ERROR(err, err_type)                 \
    if (err != 0) {                                     \
        g_set_error(error, UCA_UFO_CAMERA_ERROR,        \
                err_type,                               \
                "%s:%i pcilib: %s (errcode = %d)",      \
                __FILE__, __LINE__, strerror(err), err);\
        return;                                         \
    }

#define PCILIB_SET_ERROR_RETURN_FALSE(err, err_type)    \
    if (err != 0) {                                     \
        g_set_error(error, UCA_UFO_CAMERA_ERROR,        \
                err_type,                               \
                "%s:%i pcilib: %s (errcode = %d)",      \
                __FILE__, __LINE__, strerror(err), err);\
        return FALSE;                                   \
    }

#define PCILIB_WARN_ON_ERROR(err)                       \
    if (err != 0) {                                     \
       g_warning ("%s:%i pcilib: %s (errcode = %d)",    \
                  __FILE__, __LINE__, strerror(err), err); \
    }

#define UCA_UFO_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_UFO_CAMERA, UcaUfoCameraPrivate))

static void uca_ufo_camera_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaUfoCamera, uca_ufo_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_ufo_camera_initable_iface_init))

static const guint SENSOR_WIDTH = 2048;
static const gdouble EXPOSURE_TIME_SCALE = 2.69e6;

/**
 * UcaUfoCameraError:
 * @UCA_UFO_CAMERA_ERROR_INIT: Initializing pcilib failed
 * @UCA_UFO_CAMERA_ERROR_START_RECORDING: Starting failed
 * @UCA_UFO_CAMERA_ERROR_STOP_RECORDING: Stopping failed
 * @UCA_UFO_CAMERA_ERROR_TRIGGER: Triggering a frame failed
 * @UCA_UFO_CAMERA_ERROR_NEXT_EVENT: No event happened
 * @UCA_UFO_CAMERA_ERROR_NO_DATA: No data was transmitted
 * @UCA_UFO_CAMERA_ERROR_MAYBE_CORRUPTED: Data is potentially corrupted
 */
GQuark uca_ufo_camera_error_quark()
{
    return g_quark_from_static_string("uca-ufo-camera-error-quark");
}

enum {
    PROP_SENSOR_TEMPERATURE = N_BASE_PROPERTIES,
    PROP_FPGA_TEMPERATURE,
    PROP_UFO_START,
    N_MAX_PROPERTIES = 512
};

static gint base_overrideables[] = {
    PROP_NAME,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_BITDEPTH,
    PROP_EXPOSURE_TIME,
    PROP_FRAMES_PER_SECOND,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    PROP_HAS_STREAMING,
    PROP_HAS_CAMRAM_RECORDING,
    0,
};

typedef struct _RegisterInfo {
    gchar  *name;
    guint   cached_value;
} RegisterInfo;

static GParamSpec *ufo_properties[N_MAX_PROPERTIES] = { NULL, };

static guint N_PROPERTIES;

struct _UcaUfoCameraPrivate {
    GError             *construct_error;
    GHashTable         *property_table; /* maps from prop_id to RegisterInfo* */
    GThread            *async_thread;
    pcilib_t           *handle;
    pcilib_timeout_t    timeout;
    guint               n_bits;
    guint               height;
    enum {
        FPGA_48MHZ = 0,
        FPGA_40MHZ
    }                   frequency;
};

static void
error_handler (const char *format, ...)
{
    va_list args;
    gchar *message;

    va_start (args, format);
    message = g_strdup_vprintf (format, args);
    g_warning ("%s", message);
    va_end (args);
}

static guint
read_register_value (pcilib_t *handle, const gchar *name)
{
    pcilib_register_value_t reg_value;
    int err;

    err = pcilib_read_register(handle, NULL, name, &reg_value);
    PCILIB_WARN_ON_ERROR (err);
    return (guint) reg_value;
}

static int
event_callback(pcilib_event_id_t event_id, pcilib_event_info_t *info, void *user)
{
    UcaCamera *camera = UCA_CAMERA(user);
    UcaUfoCameraPrivate *priv = UCA_UFO_CAMERA_GET_PRIVATE(camera);
    size_t error = 0;

    void *buffer = pcilib_get_data(priv->handle, event_id, PCILIB_EVENT_DATA, &error);

    if (buffer == NULL)
        return PCILIB_STREAMING_CONTINUE;

    camera->grab_func (buffer, camera->user_data);
    pcilib_return_data (priv->handle, event_id, PCILIB_EVENT_DATA, buffer);

    return PCILIB_STREAMING_CONTINUE;
}

static guint
update_properties (UcaUfoCameraPrivate *priv)
{
    guint prop;
    pcilib_model_description_t *description;

    prop = PROP_UFO_START;
    description = pcilib_get_model_description (priv->handle);

    for (guint i = 0; description->registers[i].name != NULL; i++) {
        GParamFlags flags = 0;
        RegisterInfo *reg_info;
        gchar *prop_name;
        pcilib_register_description_t *reg;
        pcilib_register_value_t value;
        gint err;

        reg = &description->registers[i];

        switch (reg->mode) {
            case PCILIB_REGISTER_R:
                flags = G_PARAM_READABLE;
                break;
            case PCILIB_REGISTER_W:
            case PCILIB_REGISTER_W1C:
                flags = G_PARAM_WRITABLE;
                break;
            case PCILIB_REGISTER_RW:
            case PCILIB_REGISTER_RW1C:
                flags = G_PARAM_READWRITE;
                break;
        }

        err = pcilib_read_register (priv->handle, NULL, reg->name, &value);
        PCILIB_WARN_ON_ERROR (err);

        reg_info = g_new0 (RegisterInfo, 1);
        reg_info->name = g_strdup (reg->name);
        reg_info->cached_value = (guint32) value;

        g_hash_table_insert (priv->property_table, GINT_TO_POINTER (prop), reg_info);
        prop_name = g_strdup_printf ("ufo-%s", reg->name);

        ufo_properties[prop++] = g_param_spec_uint (
                prop_name, reg->description, reg->description,
                0, G_MAXUINT, reg->defvalue, flags);

        g_free (prop_name);
    }

    return prop;
}

static gboolean
setup_pcilib (UcaUfoCameraPrivate *priv)
{
    pcilib_model_t model;
    guint adc_resolution;

    model = PCILIB_MODEL_DETECT;
    priv->handle = pcilib_open("/dev/fpga0", model);

    if (priv->handle == NULL) {
        g_set_error (&priv->construct_error,
                     UCA_UFO_CAMERA_ERROR, UCA_UFO_CAMERA_ERROR_INIT,
                     "Initializing pcilib failed");
        return FALSE;
    }

    pcilib_set_error_handler (&error_handler, &error_handler);

    priv->property_table = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                  NULL, g_free);
    N_PROPERTIES = update_properties (priv);
    priv->height = read_register_value (priv->handle, "cmosis_number_lines") + 1;
    priv->frequency = read_register_value (priv->handle, "control") >> 31;
    adc_resolution = read_register_value (priv->handle, "adc_resolution");

    switch (adc_resolution) {
        case 0:
            priv->n_bits = 10;
            break;
        case 1:
            priv->n_bits = 11;
            break;
        case 2:
            priv->n_bits = 12;
            break;
    }

    return TRUE;
}

static void
set_control_bit (UcaUfoCameraPrivate *priv, guint bit, gboolean set)
{
    static const gchar *name = "control";
    pcilib_register_value_t flags;
    pcilib_register_value_t mask;
    gint err;

    err = pcilib_read_register (priv->handle, NULL, name, &flags);
    PCILIB_WARN_ON_ERROR (err);

    mask = 1 << bit;

    if (set)
        flags |= mask;
    else
        flags = flags & ~mask;

    err = pcilib_write_register(priv->handle, NULL, name, flags);
    PCILIB_WARN_ON_ERROR (err);
}

static void
set_streaming (UcaUfoCameraPrivate *priv, gboolean enable)
{
    set_control_bit (priv, 11, enable);
}

static gpointer
stream_async (UcaCamera *camera)
{
    UcaUfoCameraPrivate *priv;
    priv = UCA_UFO_CAMERA_GET_PRIVATE (camera);
    pcilib_stream (priv->handle, &event_callback, camera);

    return NULL;
}

static void
uca_ufo_camera_start_recording(UcaCamera *camera, GError **error)
{
    UcaUfoCameraPrivate *priv;
    UcaCameraTrigger trigger;
    gdouble exposure_time;
    gboolean transfer_async;
    int err;

    g_return_if_fail(UCA_IS_UFO_CAMERA(camera));

    priv = UCA_UFO_CAMERA_GET_PRIVATE(camera);

    g_object_get (G_OBJECT(camera),
                  "transfer-asynchronously", &transfer_async,
                  "exposure-time", &exposure_time,
                  "trigger-mode", &trigger,
                  NULL);

    err = pcilib_start(priv->handle, PCILIB_EVENT_DATA, PCILIB_EVENT_FLAGS_DEFAULT);
    PCILIB_SET_ERROR(err, UCA_UFO_CAMERA_ERROR_START_RECORDING);

    if (trigger == UCA_CAMERA_TRIGGER_AUTO)
        set_streaming (priv, TRUE);

    priv->timeout = ((pcilib_timeout_t) (exposure_time * 1000 + 50.0) * 1000);

    if (transfer_async)
        priv->async_thread = g_thread_create ((GThreadFunc) stream_async, camera, TRUE, error);
}

static void
uca_ufo_camera_stop_recording(UcaCamera *camera, GError **error)
{
    UcaUfoCameraPrivate *priv;
    UcaCameraTrigger trigger;
    g_return_if_fail(UCA_IS_UFO_CAMERA(camera));

    priv = UCA_UFO_CAMERA_GET_PRIVATE(camera);

    g_object_get (G_OBJECT (camera), "trigger-mode", &trigger, NULL);

    if (priv->async_thread) {
        int err = pcilib_stop(priv->handle, PCILIB_EVENT_FLAG_STOP_ONLY);
        PCILIB_SET_ERROR(err, UCA_UFO_CAMERA_ERROR_STOP_RECORDING);
        g_thread_join(priv->async_thread);
        priv->async_thread = NULL;
    }

    int err = pcilib_stop (priv->handle, PCILIB_EVENT_FLAGS_DEFAULT);
    PCILIB_SET_ERROR(err, UCA_UFO_CAMERA_ERROR_STOP_RECORDING);

    if (trigger == UCA_CAMERA_TRIGGER_AUTO)
        set_streaming (priv, FALSE);
}

static void
uca_ufo_camera_start_readout(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_UFO_CAMERA(camera));
}

static void
uca_ufo_camera_stop_readout(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_UFO_CAMERA(camera));
}

static gboolean
uca_ufo_camera_grab(UcaCamera *camera, gpointer data, GError **error)
{
    g_return_val_if_fail (UCA_IS_UFO_CAMERA(camera), FALSE);
    UcaUfoCameraPrivate *priv = UCA_UFO_CAMERA_GET_PRIVATE(camera);
    pcilib_event_id_t   event_id;
    pcilib_event_info_t event_info;
    int err;

    const gsize size = SENSOR_WIDTH * priv->height * sizeof(guint16);

    err = pcilib_get_next_event (priv->handle, priv->timeout, &event_id, sizeof(pcilib_event_info_t), &event_info);
    PCILIB_SET_ERROR_RETURN_FALSE (err, UCA_UFO_CAMERA_ERROR_NEXT_EVENT);

    gpointer src = pcilib_get_data (priv->handle, event_id, PCILIB_EVENT_DATA, (size_t *) &err);

    if (src == NULL)
        PCILIB_SET_ERROR_RETURN_FALSE (err, UCA_UFO_CAMERA_ERROR_NO_DATA);

    /*
     * Apparently, we checked that err equals total size in previous version.
     * This is problematic because errno is positive and size could be equal
     * even when an error condition is met, e.g. with a very small ROI. However,
     * we don't know if src will always be NULL when an error occured.
     */
    /* assert(err == size); */

    memcpy (data, src, size);

    /*
     * Another problem here. What does this help us? At this point we have
     * already overwritten the original buffer but can only know here if the
     * data is corrupted.
     */
    err = pcilib_return_data (priv->handle, event_id, PCILIB_EVENT_DATA, data);
    PCILIB_SET_ERROR_RETURN_FALSE (err, UCA_UFO_CAMERA_ERROR_MAYBE_CORRUPTED);

    return TRUE;
}

static void
uca_ufo_camera_trigger (UcaCamera *camera, GError **error)
{
    UcaUfoCameraPrivate *priv;
    g_return_if_fail (UCA_IS_UFO_CAMERA(camera));

    priv = UCA_UFO_CAMERA_GET_PRIVATE(camera);

    /* XXX: What is PCILIB_EVENT0? */
    pcilib_trigger (priv->handle, PCILIB_EVENT0, 0, NULL);
}

static gdouble
total_readout_time (UcaUfoCamera *camera)
{
    gdouble clock_period, foo;
    gdouble exposure_time, image_readout_time, overhead_time;
    guint output_mode;
    guint roi_height;

    g_object_get (G_OBJECT (camera),
                  "exposure-time", &exposure_time,
                  "ufo-cmosis-output-mode", &output_mode,
                  "roi-height", &roi_height,
                  NULL);

    clock_period = camera->priv->frequency == FPGA_40MHZ ? 1 / 40.0 : 1 / 48.0;
    foo = pow (2, output_mode);
    image_readout_time = (129 * clock_period * foo) * roi_height;
    overhead_time = (10 /* reg73 */ + 2 * foo) * 129 * clock_period;

    return exposure_time + (overhead_time + image_readout_time) / 1000 / 1000;
}

static void
uca_ufo_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (UCA_IS_UFO_CAMERA (object));
    UcaUfoCameraPrivate *priv = UCA_UFO_CAMERA_GET_PRIVATE (object);

    if (uca_camera_is_recording (UCA_CAMERA (object)) && !uca_camera_is_writable_during_acquisition (UCA_CAMERA (object), pspec->name)) {
        g_warning ("Property '%s' cant be changed during acquisition", pspec->name);
        return;
    }

    switch (property_id) {
        case PROP_EXPOSURE_TIME:
            {
                const guint frequency = priv->frequency == FPGA_40MHZ ? 40 : 48;
                const gdouble user_exposure_time = g_value_get_double(value);
                gint err;

                pcilib_register_value_t reg_value = (pcilib_register_value_t) (1e6 * user_exposure_time * frequency / 129.0 - 0.43 * 10);
                err = pcilib_write_register(priv->handle, NULL, "cmosis_exp_time", reg_value);
                PCILIB_WARN_ON_ERROR (err);
            }
            break;
        case PROP_FRAMES_PER_SECOND:
            {
                gdouble readout_time;
                gdouble frame_period;
                guint32 trigger_period;

                frame_period = 1.0 / g_value_get_double (value);
                readout_time = total_readout_time (UCA_UFO_CAMERA (object));

                if (frame_period < readout_time) {
                    g_warning ("Frame period higher than readout time %f\n", readout_time);
                    break;
                }

                trigger_period = (guint32) ((frame_period - readout_time) / (8.0 * 1e-9));
                g_object_set (object, "ufo-trigger-period", trigger_period, NULL);
            }
            break;
        case PROP_ROI_X:
        case PROP_ROI_Y:
        case PROP_ROI_WIDTH:
        case PROP_ROI_HEIGHT:
            g_debug("ROI feature not implemented yet");
            break;

        default:
            {
                RegisterInfo *reg_info;

                reg_info = g_hash_table_lookup (priv->property_table,
                                                GINT_TO_POINTER (property_id));

                if (reg_info != NULL) {
                    pcilib_register_value_t reg_value = 0;
                    gint err;

                    reg_value = g_value_get_uint (value);

                    err = pcilib_write_register (priv->handle, NULL, reg_info->name, reg_value);
                    PCILIB_WARN_ON_ERROR (err);

                    err = pcilib_read_register (priv->handle, NULL, reg_info->name, &reg_value);
                    PCILIB_WARN_ON_ERROR (err);

                    reg_info->cached_value = (guint) reg_value;
                }
                else
                    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            }
            return;
    }
}

static void
uca_ufo_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaUfoCameraPrivate *priv = UCA_UFO_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_SENSOR_WIDTH:
            g_value_set_uint (value, SENSOR_WIDTH);
            break;
        case PROP_SENSOR_HEIGHT:
            g_value_set_uint (value, priv->height);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint (value, priv->n_bits);
            break;
        case PROP_SENSOR_TEMPERATURE:
            {
                const double a = priv->frequency == FPGA_48MHZ ? 0.3 : 0.25;
                const double b = priv->frequency == FPGA_48MHZ ? 1000 : 1200;
                guint32 temperature;

                temperature = read_register_value (priv->handle, "sensor_temperature");
                g_value_set_double (value, a * (temperature - b));
            }
            break;
        case PROP_FPGA_TEMPERATURE:
            {
                const double a = 503.975 / 1024.0;
                const double b = 273.15;
                guint32 temperature;

                temperature = read_register_value (priv->handle, "fpga_temperature");
                g_value_set_double (value, a * temperature - b);
            }
            break;
        case PROP_EXPOSURE_TIME:
            {
                const gdouble frequency = priv->frequency == FPGA_40MHZ ? 40.0 : 48.0;
                g_value_set_double (value, (read_register_value (priv->handle, "cmosis_exp_time") + 0.43 * 10) * 129.0 / frequency / 1e6);
            }
            break;
        case PROP_FRAMES_PER_SECOND:
            {
                gdouble delay_time;
                gdouble framerate;
                guint32 trigger_period;

                g_object_get (object, "ufo-trigger-period", &trigger_period, NULL);

                delay_time = trigger_period * 8.0 * 1e-9;
                framerate = 1.0 / (total_readout_time (UCA_UFO_CAMERA (object)) + delay_time);
                g_value_set_double(value, framerate);
            }
            break;
        case PROP_HAS_STREAMING:
            g_value_set_boolean (value, TRUE);
            break;
        case PROP_HAS_CAMRAM_RECORDING:
            g_value_set_boolean (value, FALSE);
            break;
        case PROP_ROI_X:
            g_value_set_uint (value, 0);
            break;
        case PROP_ROI_Y:
            g_value_set_uint (value, 0);
            break;
        case PROP_ROI_WIDTH:
            g_value_set_uint (value, SENSOR_WIDTH);
            break;
        case PROP_ROI_HEIGHT:
            g_value_set_uint (value, priv->height);
            break;
        case PROP_NAME:
            g_value_set_string (value, "Ufo Camera w/ CMOSIS CMV2000");
            break;
        default:
            {
                RegisterInfo *reg_info = g_hash_table_lookup (priv->property_table, GINT_TO_POINTER (property_id));

                if (reg_info != NULL)
                    g_value_set_uint (value, reg_info->cached_value);
                else
                    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            }
            break;
    }
}

static void
uca_ufo_camera_finalize(GObject *object)
{
    UcaUfoCameraPrivate *priv;

    priv = UCA_UFO_CAMERA_GET_PRIVATE (object);

    pcilib_close (priv->handle);
    g_clear_error (&priv->construct_error);

    G_OBJECT_CLASS (uca_ufo_camera_parent_class)->finalize (object);
}

static gboolean
ufo_ufo_camera_initable_init (GInitable *initable,
                              GCancellable *cancellable,
                              GError **error)
{
    UcaUfoCamera *camera;
    UcaUfoCameraPrivate *priv;

    g_return_val_if_fail (UCA_IS_UFO_CAMERA (initable), FALSE);

    if (cancellable != NULL) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             "Cancellable initialization not supported");
        return FALSE;
    }

    camera = UCA_UFO_CAMERA (initable);
    priv = camera->priv;

    if (priv->construct_error != NULL) {
        if (error)
            *error = g_error_copy (priv->construct_error);

        return FALSE;
    }

    return TRUE;
}

static void
uca_ufo_camera_initable_iface_init (GInitableIface *iface)
{
    iface->init = ufo_ufo_camera_initable_init;
}

static void
uca_ufo_camera_class_init(UcaUfoCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_ufo_camera_set_property;
    gobject_class->get_property = uca_ufo_camera_get_property;
    gobject_class->finalize = uca_ufo_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_ufo_camera_start_recording;
    camera_class->stop_recording = uca_ufo_camera_stop_recording;
    camera_class->start_readout = uca_ufo_camera_start_readout;
    camera_class->stop_readout = uca_ufo_camera_stop_readout;
    camera_class->grab = uca_ufo_camera_grab;
    camera_class->trigger = uca_ufo_camera_trigger;

    for (guint i = 0; base_overrideables[i] != 0; i++)
        g_object_class_override_property(gobject_class, base_overrideables[i], uca_camera_props[base_overrideables[i]]);

    ufo_properties[PROP_SENSOR_TEMPERATURE] =
        g_param_spec_double("sensor-temperature",
            "Temperature of the sensor",
            "Temperature of the sensor in degree Celsius",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
            G_PARAM_READABLE);

    ufo_properties[PROP_FPGA_TEMPERATURE] =
        g_param_spec_double("fpga-temperature",
            "Temperature of the FPGA",
            "Temperature of the FPGA in degree Celsius",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
            G_PARAM_READABLE);

    g_type_class_add_private(klass, sizeof(UcaUfoCameraPrivate));
}

static void
uca_ufo_camera_init(UcaUfoCamera *self)
{
    UcaCamera *camera;
    UcaUfoCameraPrivate *priv;
    GObjectClass *oclass;

    self->priv = priv = UCA_UFO_CAMERA_GET_PRIVATE(self);
    priv->construct_error = NULL;
    priv->async_thread = NULL;

    if (!setup_pcilib (priv))
        return;

    oclass = G_OBJECT_GET_CLASS (self);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property(oclass, id, ufo_properties[id]);

    camera = UCA_CAMERA (self);
    uca_camera_register_unit (camera, "sensor-temperature", UCA_UNIT_DEGREE_CELSIUS);
    uca_camera_register_unit (camera, "fpga-temperature", UCA_UNIT_DEGREE_CELSIUS);
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_UFO_CAMERA;
}
