/* Copyright (C) 2011, 2012 Mihael Koep <koep@softwareschneiderei.de>
   (Softwareschneiderei GmbH)

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

#include <string.h>
#include <gmodule.h>
#include "uca-dexela-camera.h"
#include "dexela/dexela_api.h"

#define UCA_DEXELA_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_DEXELA_CAMERA, UcaDexelaCameraPrivate))

G_DEFINE_TYPE(UcaDexelaCamera, uca_dexela_camera, UCA_TYPE_CAMERA)
/**
 * UcaDexelaCameraError:
 * @UCA_DEXELA_CAMERA_ERROR_LIBDEXELA_INIT: Initializing libdexela failed
 */
GQuark uca_dexela_camera_error_quark()
{
    return g_quark_from_static_string("uca-dexela-camera-error-quark");
}

enum {
    PROP_GAIN_MODE = N_BASE_PROPERTIES,
    PROP_TEST_MODE,
    N_PROPERTIES
};

static gint base_overrideables[] = {
    PROP_NAME,
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
    PROP_ROI_WIDTH_MULTIPLIER,
    PROP_ROI_HEIGHT_MULTIPLIER,
    PROP_HAS_STREAMING,
    PROP_HAS_CAMRAM_RECORDING,
    0
};

static GParamSpec *dexela_properties[N_PROPERTIES] = { NULL, };

static const gdouble MICROS_TO_SECONDS_FACTOR = 1e6d;

struct _UcaDexelaCameraPrivate {
    GValueArray *binnings;
    guint width;
    guint height;
    guint bits;
    gsize num_bytes;
};

/**
 * Hardcode possible binnings for now
 */
static void fill_binnings(UcaDexelaCameraPrivate *priv)
{
    GValue val = {0};
    g_value_init(&val, G_TYPE_UINT);

    priv->binnings = g_value_array_new(3);
    g_value_set_uint(&val, 1);
    g_value_array_append(priv->binnings, &val);
    g_value_set_uint(&val, 2);
    g_value_array_append(priv->binnings, &val);
    g_value_set_uint(&val, 4);
    g_value_array_append(priv->binnings, &val);
}

static void map_dexela_trigger_mode_to_uca(GValue* value, TriggerMode mode)
{
    if (mode == SOFTWARE) {
        g_value_set_enum(value, UCA_CAMERA_TRIGGER_SOFTWARE);
        return;
    }
    if (mode == EDGE) {
        g_value_set_enum(value, UCA_CAMERA_TRIGGER_EXTERNAL);
        return;
    }
    // XXX: this mapping is only a hack/guess
    if (mode == DURATION) {
        g_value_set_enum(value, UCA_CAMERA_TRIGGER_AUTO);
        return;
    }
    g_warning("Unsupported dexela trigger mode: %d", mode);
}

static void set_trigger_mode(UcaCameraTrigger mode)
{
    if (mode == UCA_CAMERA_TRIGGER_SOFTWARE) {
        dexela_set_trigger_mode(SOFTWARE);
        return;
    }
    if (mode == UCA_CAMERA_TRIGGER_EXTERNAL) {
        dexela_set_trigger_mode(EDGE);
        return;
    }
    if (mode == UCA_CAMERA_TRIGGER_AUTO) {
        dexela_set_trigger_mode(DURATION);
        return;
    }
    g_warning("Unsupported uca trigger mode: %d", mode);
}

static gboolean is_binning_allowed(UcaDexelaCameraPrivate *priv, guint binning)
{
    for (int i = 0; i < priv->binnings->n_values; i++) {
        guint allowedBinning = g_value_get_uint(g_value_array_get_nth(priv->binnings, i));
        if (binning == allowedBinning) {
            return TRUE;
        }
    }
    return FALSE;
}

UcaDexelaCamera *uca_dexela_camera_new(GError **error)
{
    UcaDexelaCamera *camera = g_object_new(UCA_TYPE_DEXELA_CAMERA, NULL);
    UcaDexelaCameraPrivate *priv = UCA_DEXELA_CAMERA_GET_PRIVATE(camera);
    fill_binnings(priv);
    /*
    * Here we override property ranges because we didn't know them at property
    * installation time.
    */
    GObjectClass *camera_class = G_OBJECT_CLASS (UCA_CAMERA_GET_CLASS (camera));
    // TODO implement error checking
    dexela_open_detector(DEFAULT_FMT_FILE_PATH);
    dexela_init_serial_connection();
    priv->bits = dexela_get_bit_depth();
    priv->width = dexela_get_width();
    priv->height = dexela_get_height();
    priv->num_bytes = 2;
    return camera;
}

static void uca_dexela_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaDexelaCameraPrivate *priv = UCA_DEXELA_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_NAME:
        {
            gchar* model = dexela_get_model();
            g_value_set_string(value, g_strdup_printf("Dexela %s", model));
            g_free(model);
            break;
        }
        case PROP_EXPOSURE_TIME:
        {
            g_value_set_double(value, dexela_get_exposure_time_micros() / MICROS_TO_SECONDS_FACTOR);
            break;
        }
        case PROP_HAS_CAMRAM_RECORDING:
        {
            g_value_set_boolean(value, FALSE);
            break;
        }
        case PROP_HAS_STREAMING:
        {
            g_value_set_boolean(value, FALSE);
            break;
        }
        case PROP_SENSOR_BITDEPTH:
        {
            g_value_set_uint(value, priv->bits);
            break;
        }
        case PROP_SENSOR_WIDTH:
        {
            g_value_set_uint(value, priv->width);
            break;
        }
        case PROP_SENSOR_HEIGHT:
        {
            g_value_set_uint(value, priv->height);
            break;
        }
        case PROP_ROI_WIDTH:
        {
            g_value_set_uint(value, dexela_get_width());
            break;
        }
        case PROP_ROI_HEIGHT:
        {
            g_value_set_uint(value, dexela_get_height());
            break;
        }
        case PROP_SENSOR_HORIZONTAL_BINNING:
        {
            g_value_set_uint(value, dexela_get_binning_mode_horizontal());
            break;
        }
        case PROP_SENSOR_HORIZONTAL_BINNINGS:
        {
            g_value_set_boxed(value, priv->binnings);
            break;
        }
        case PROP_SENSOR_VERTICAL_BINNING:
        {
            g_value_set_uint(value, dexela_get_binning_mode_vertical());
            break;
        }
        case PROP_SENSOR_VERTICAL_BINNINGS:
        {
            g_value_set_boxed(value, priv->binnings);
            break;
        }
        case PROP_SENSOR_MAX_FRAME_RATE:
        {
            // TODO: we do not know how to compute the correct value, so just return 0 for now
            g_value_set_float(value, 0.0f);
            break;
        }
        case PROP_GAIN_MODE:
        {
            g_value_set_uint(value, dexela_get_gain());
            break;
        }
        case PROP_TEST_MODE:
        {
            g_value_set_boolean(value, dexela_get_control_register() & 1);
            break;
        }
        case PROP_TRIGGER_MODE:
        {
            map_dexela_trigger_mode_to_uca(value, dexela_get_trigger_mode());
            break;
        }
        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
        }
    }
}

static void uca_dexela_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    UcaDexelaCameraPrivate *priv = UCA_DEXELA_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_EXPOSURE_TIME:
        {
            const gdouble exposureTimeInSeconds = g_value_get_double(value);
            dexela_set_exposure_time_micros((gint) (exposureTimeInSeconds * MICROS_TO_SECONDS_FACTOR));
            break;
        }
        case PROP_SENSOR_HORIZONTAL_BINNING:
        {
            const guint horizontalBinning = g_value_get_uint(value);
            if (!is_binning_allowed(priv, horizontalBinning)) {
                g_warning("Tried to set illegal horizontal binning: %d", horizontalBinning);
                return;
            }
            dexela_set_binning_mode(horizontalBinning, dexela_get_binning_mode_vertical());
            break;
        }
        case PROP_SENSOR_VERTICAL_BINNING:
        {
            const guint verticalBinning = g_value_get_uint(value);
            if (!is_binning_allowed(priv, verticalBinning)) {
                g_warning("Tried to set illegal vertical binning: %d", verticalBinning);
                return;
            }
            dexela_set_binning_mode(dexela_get_binning_mode_horizontal(), verticalBinning);
            break;
        }
        case PROP_GAIN_MODE:
        {
            const guint gain = g_value_get_uint(value);
            if (gain == 0) {
                dexela_set_gain(LOW);
                return;
            }
            if (gain == 1) {
                dexela_set_gain(HIGH);
                return;
            }
            g_warning("Illegal attempt to set gain: %d", gain);
            break;
        }
        case PROP_TEST_MODE:
        {
            if (g_value_get_boolean(value)) {
                dexela_set_control_register(dexela_get_control_register() | 1);
                return;
            }
            dexela_set_control_register(dexela_get_control_register() & 0xFFFE);
            break;
        }
        case PROP_TRIGGER_MODE:
        {
            set_trigger_mode(g_value_get_enum(value));
            break;
        }
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }
}

static void uca_dexela_camera_start_recording(UcaCamera *camera, GError **error)
{
    g_debug("start recording called");
    dexela_start_acquisition();
}

static void uca_dexela_camera_stop_recording(UcaCamera *camera, GError **error)
{
    g_debug("stop recording called");
    dexela_stop_acquisition();
}

static void uca_dexela_camera_grab(UcaCamera *camera, gpointer *data, GError **error)
{
    g_debug("grab called");
    g_return_if_fail(UCA_IS_DEXELA_CAMERA(camera));
    UcaDexelaCameraPrivate *priv = UCA_DEXELA_CAMERA_GET_PRIVATE(camera);
    if (*data == NULL) {
        g_debug("Allocating buffer");
        *data = g_malloc0(priv->width * priv->height * priv->num_bytes);
    }
    // TODO: copy to the data buffer
    memcpy((gchar *) *data, dexela_grab(), priv->width * priv->height * priv->num_bytes);
}

static void uca_dexela_camera_finalize(GObject *object)
{
    UcaDexelaCameraPrivate *priv = UCA_DEXELA_CAMERA_GET_PRIVATE(object);
    g_value_array_free(priv->binnings);

    G_OBJECT_CLASS(uca_dexela_camera_parent_class)->finalize(object);
}

static void uca_dexela_camera_class_init(UcaDexelaCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_dexela_camera_set_property;
    gobject_class->get_property = uca_dexela_camera_get_property;
    gobject_class->finalize = uca_dexela_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_dexela_camera_start_recording;
    camera_class->stop_recording = uca_dexela_camera_stop_recording;
    camera_class->grab = uca_dexela_camera_grab;

    for (guint i = 0; base_overrideables[i] != 0; i++) {
        g_object_class_override_property(gobject_class, base_overrideables[i], uca_camera_props[base_overrideables[i]]);
    }
    dexela_properties[PROP_GAIN_MODE] = 
        g_param_spec_uint("gain-mode",
            "High or Low Full Well",
            "High (1) or Low (0) Full Well",
            0, 1, 0, G_PARAM_READWRITE);
    dexela_properties[PROP_TEST_MODE] = 
        g_param_spec_boolean("test-mode",
            "Enable or disable test mode",
            "Enable (true) or disable (false) test mode",
            FALSE, G_PARAM_READWRITE);
    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++) {
        g_object_class_install_property(gobject_class, id, dexela_properties[id]);
    }
    g_type_class_add_private(klass, sizeof(UcaDexelaCameraPrivate));
}

static void uca_dexela_camera_init(UcaDexelaCamera *self)
{
    self->priv = UCA_DEXELA_CAMERA_GET_PRIVATE(self);
}

G_MODULE_EXPORT UcaCamera *
uca_camera_impl_new (GError **error)
{
    return UCA_CAMERA(uca_dexela_camera_new(error));
}
