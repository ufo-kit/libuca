/* Copyright (C) 2012 Volker Kaiser <volker.kaiser@softwareschneiderei>
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
#include <libpyloncam/pylon_camera.h>
#include <gmodule.h>
#include <gio/gio.h>
#include "uca-pylon-camera.h"
#include "uca-pylon-enums.h"

#define UCA_PYLON_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_PYLON_CAMERA, UcaPylonCameraPrivate))

static void uca_pylon_camera_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaPylonCamera, uca_pylon_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_pylon_camera_initable_iface_init))

#define PROP_GAIN_MIN 300
#define PROP_GAIN_MAX 400
#define PROP_GAIN_DEFAULT PROP_GAIN_MAX

/**
 * UcapylonCameraError:
 * @UCA_PYLON_CAMERA_ERROR_LIBPYLON_INIT: Initializing libpylon failed
 * @UCA_PYLON_CAMERA_ERROR_LIBPYLON_GENERAL: General libpylon error
 * @UCA_PYLON_CAMERA_ERROR_UNSUPPORTED: Camera type is not supported
 * @UCA_PYLON_CAMERA_ERROR_FG_INIT: Framegrabber initialization failed
 * @UCA_PYLON_CAMERA_ERROR_FG_GENERAL: General framegrabber error
 * @UCA_PYLON_CAMERA_ERROR_FG_ACQUISITION: Framegrabber acquisition error
 */
GQuark uca_pylon_camera_error_quark()
{
    return g_quark_from_static_string("uca-pylon-camera-error-quark");
}

enum {
    PROP_ROI_WIDTH_DEFAULT = N_BASE_PROPERTIES,
    PROP_ROI_HEIGHT_DEFAULT,
    PROP_GAIN,
    PROP_BALANCE_WHITE_AUTO,
    PROP_EXPOSURE_AUTO,
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
    0
};

static GParamSpec *pylon_properties[N_PROPERTIES] = { NULL, };


struct _UcaPylonCameraPrivate {
    guint bit_depth;
    gsize num_bytes;

    guint width;
    guint height;
    guint16 roi_x;
    guint16 roi_y;
    guint16 roi_width;
    guint16 roi_height;
    GValueArray *binnings;
};


static void pylon_get_roi(GObject *object, GError** error)
{
    UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE(object);
    pylon_camera_get_roi(&priv->roi_x, &priv->roi_y, &priv->roi_width, &priv->roi_height, error);
}

static void pylon_set_roi(GObject *object, GError** error)
{
    UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE(object);
    pylon_camera_set_roi(priv->roi_x, priv->roi_y, priv->roi_width, priv->roi_height, error);
}

static void uca_pylon_camera_start_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_PYLON_CAMERA(camera));
    UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE(camera);

    priv->num_bytes = 2;
    pylon_camera_start_acquision(error);
}

static void uca_pylon_camera_stop_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_PYLON_CAMERA(camera));
    pylon_camera_stop_acquision(error);
}

static gboolean uca_pylon_camera_grab(UcaCamera *camera, gpointer data, GError **error)
{
    g_return_val_if_fail(UCA_IS_PYLON_CAMERA(camera), FALSE);

    pylon_camera_grab(data, error);
    return TRUE;
}

static void uca_pylon_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (UCA_IS_PYLON_CAMERA (object));
    UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE (object);
    GError* error = NULL;

    if (uca_camera_is_recording (UCA_CAMERA (object)) && !uca_camera_is_writable_during_acquisition (UCA_CAMERA (object), pspec->name)) {
        g_warning ("Property '%s' cant be changed during acquisition", pspec->name);
        return;
    }

    switch (property_id) {
    case PROP_SENSOR_HORIZONTAL_BINNING:
      /* intentional fall-through*/
    case PROP_SENSOR_VERTICAL_BINNING:
      /* intentional fall-through*/
    case PROP_TRIGGER_MODE:
        break;
    case PROP_BALANCE_WHITE_AUTO:
    {
        pylon_camera_set_int_attribute("BalanceWhiteAuto", g_value_get_enum(value), &error);
        break;
    }
    case PROP_EXPOSURE_AUTO:
    {
        pylon_camera_set_int_attribute("ExposureAuto", g_value_get_enum(value), &error);
        break;
    }
    case PROP_ROI_X:
    {
        priv->roi_x = g_value_get_uint(value);
        gint max_roi_width = priv->width - priv->roi_x;
        priv->roi_width = MIN(priv->roi_width, max_roi_width);
        pylon_set_roi(object, &error);
        break;
    }
    case PROP_ROI_Y:
    {
        priv->roi_y = g_value_get_uint(value);
        gint max_roi_height = priv->height - priv->roi_y;
        priv->roi_height = MIN(priv->roi_height, max_roi_height);
        pylon_set_roi(object, &error);
        break;
    }
    case PROP_ROI_WIDTH:
    {
        priv->roi_width = g_value_get_uint(value);
        pylon_set_roi(object, &error);
        break;
    }
    case PROP_ROI_HEIGHT:
    {
        priv->roi_height = g_value_get_uint(value);
        pylon_set_roi(object, &error);
        break;
    }
    case PROP_EXPOSURE_TIME:
        pylon_camera_set_exposure_time(g_value_get_double(value), &error);
        break;
    case PROP_GAIN:
        pylon_camera_set_gain(g_value_get_int(value), &error);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        return;
    }

    if (error) {
        if (error->message) {
            g_warning("failed to set property %d: %s", property_id, error->message);
        } else {
            g_warning("failed to set property %d", property_id);
        }
    }
}

static void uca_pylon_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE(object);
    GError* error = NULL;

    switch (property_id) {
        case PROP_BALANCE_WHITE_AUTO:
          {
            gint enum_value = UCA_CAMERA_BALANCE_WHITE_OFF;
            pylon_camera_get_int_attribute("BalanceWhiteAuto", &enum_value, &error);
            UcaCameraBalanceWhiteAuto mode = enum_value;
            g_value_set_enum(value, mode);
            break;
          }
        case PROP_EXPOSURE_AUTO:
          {
            gint enum_value = UCA_CAMERA_EXPOSURE_AUTO_OFF;
            pylon_camera_get_int_attribute("ExposureAuto", &enum_value, &error);
            UcaCameraExposureAuto mode = enum_value;
            g_value_set_enum(value, mode);
            break;
          }
        case PROP_SENSOR_WIDTH:
            pylon_camera_get_sensor_size(&priv->width, &priv->height, &error);
            g_value_set_uint(value, priv->width);
            break;

        case PROP_SENSOR_HEIGHT:
            pylon_camera_get_sensor_size(&priv->width, &priv->height, &error);
            g_value_set_uint(value, priv->height);
            break;

        case PROP_SENSOR_BITDEPTH:
            pylon_camera_get_bit_depth(&priv->bit_depth, &error);
            g_value_set_uint(value, priv->bit_depth);
            break;

        case PROP_SENSOR_HORIZONTAL_BINNING:
            g_value_set_uint(value, 1);
            break;

        case PROP_SENSOR_HORIZONTAL_BINNINGS:
            g_value_set_boxed(value, priv->binnings);
            break;

        case PROP_SENSOR_VERTICAL_BINNING:
            g_value_set_uint(value, 1);
            break;

        case PROP_SENSOR_VERTICAL_BINNINGS:
            g_value_set_boxed(value, priv->binnings);
            break;

        case PROP_TRIGGER_MODE:
            g_value_set_enum(value, UCA_CAMERA_TRIGGER_AUTO);
            break;

        case PROP_HAS_STREAMING:
            g_value_set_boolean(value, FALSE);
            break;

        case PROP_HAS_CAMRAM_RECORDING:
            g_value_set_boolean(value, FALSE);
            break;

        case PROP_ROI_X:
            {
                pylon_get_roi(object, &error);
                g_value_set_uint(value, priv->roi_x);
            }
            break;

        case PROP_ROI_Y:
            {
                pylon_get_roi(object, &error);
                g_value_set_uint(value, priv->roi_y);
            }
            break;

        case PROP_ROI_WIDTH:
            {
                pylon_get_roi(object, &error);
                g_value_set_uint(value, priv->roi_width);
            }
            break;

        case PROP_ROI_HEIGHT:
            {
                pylon_get_roi(object, &error);
                g_value_set_uint(value, priv->roi_height);
            }
            break;

        case PROP_ROI_WIDTH_DEFAULT:
            pylon_camera_get_sensor_size(&priv->width, &priv->height, &error);
            g_value_set_uint(value, priv->width);
            break;

        case PROP_ROI_HEIGHT_DEFAULT:
            pylon_camera_get_sensor_size(&priv->width, &priv->height, &error);
            g_value_set_uint(value, priv->height);
            break;

        case PROP_GAIN:
            {
                gint gain=0;
                pylon_camera_get_gain(&gain, &error);
                g_value_set_int(value, gain);
            }
            break;

        case PROP_ROI_WIDTH_MULTIPLIER:
            g_value_set_uint(value, 1);
            break;

        case PROP_ROI_HEIGHT_MULTIPLIER:
            g_value_set_uint(value, 1);
            break;

        case PROP_EXPOSURE_TIME:
            {
                gdouble exp_time = 0.0;
                pylon_camera_get_exposure_time(&exp_time, &error);
                g_value_set_double(value, exp_time);
            }
            break;

        case PROP_NAME:
        {
          const gchar* name = NULL;
          pylon_camera_get_string_attribute("ModelName", &name, &error);
          g_value_set_string(value, name);
        }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
    if (error) {
      if(error->message) {
    g_warning("failed to get property %d: %s", property_id, error->message);
      } else {
    g_warning("failed to get property %d", property_id);
      }
    }
    //g_debug("pylon_get_property end\n");
}

static void uca_pylon_camera_finalize(GObject *object)
{
    UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE(object);
    g_value_array_free(priv->binnings);

    G_OBJECT_CLASS(uca_pylon_camera_parent_class)->finalize(object);
}

static gboolean uca_pylon_camera_initable_init(GInitable *initable, GCancellable *cancellable, GError **error)
{
    g_return_val_if_fail (UCA_IS_PYLON_CAMERA (initable), FALSE);

    if (cancellable != NULL) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             "Cancellable initialization not supported");
        return FALSE;
    }

    UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE(initable);

    gchar* pylon_camera_ip = getenv("PYLON_CAMERA_IP");
    if(!pylon_camera_ip) {
        g_error("no environment variable PYLON_CAMERA_IP found");
    }

    pylon_camera_new(pylon_camera_ip, error);
    if (*error != NULL) {
        return FALSE;
    }

    pylon_camera_get_sensor_size(&priv->width, &priv->height, error);
    if (*error != NULL) {
        return FALSE;
    }

    return TRUE;
}

static void uca_pylon_camera_initable_iface_init(GInitableIface *iface)
{
    iface->init = uca_pylon_camera_initable_init;
}


static void uca_pylon_camera_class_init(UcaPylonCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_pylon_camera_set_property;
    gobject_class->get_property = uca_pylon_camera_get_property;
    gobject_class->finalize = uca_pylon_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_pylon_camera_start_recording;
    camera_class->stop_recording = uca_pylon_camera_stop_recording;
    camera_class->grab = uca_pylon_camera_grab;

    for (guint i = 0; base_overrideables[i] != 0; i++)
        g_object_class_override_property(gobject_class, base_overrideables[i], uca_camera_props[base_overrideables[i]]);

    pylon_properties[PROP_NAME] =
        g_param_spec_string("name",
            "Name of the camera",
            "Name of the camera",
            "", G_PARAM_READABLE);

    pylon_properties[PROP_ROI_WIDTH_DEFAULT] =
        g_param_spec_uint("roi-width-default",
            "ROI width default value",
            "ROI width default value",
            0, G_MAXUINT, 0,
            G_PARAM_READABLE);

    pylon_properties[PROP_ROI_HEIGHT_DEFAULT] =
        g_param_spec_uint("roi-height-default",
            "ROI height default value",
            "ROI height default value",
            0, G_MAXUINT, 0,
            G_PARAM_READABLE);

    pylon_properties[PROP_GAIN] =
        g_param_spec_int("gain",
            "gain",
            "gain",
            PROP_GAIN_MIN, PROP_GAIN_MAX, PROP_GAIN_DEFAULT,
            G_PARAM_READWRITE);
    pylon_properties[PROP_BALANCE_WHITE_AUTO] =
        g_param_spec_enum("balance-white-auto",
            "Balance White Auto mode",
            "White balance mode  (0: Off, 1: Once, 2: Continuous)",
            UCA_TYPE_CAMERA_BALANCE_WHITE_AUTO, UCA_CAMERA_BALANCE_WHITE_OFF,
            G_PARAM_READWRITE);
    pylon_properties[PROP_EXPOSURE_AUTO] =
        g_param_spec_enum("exposure-auto",
            "Exposure Auto mode",
            "Exposure auto mode  (0: Off, 1: Once, 2: Continuous)",
            UCA_TYPE_CAMERA_EXPOSURE_AUTO, UCA_CAMERA_EXPOSURE_AUTO_OFF,
            G_PARAM_READWRITE);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property(gobject_class, id, pylon_properties[id]);

    g_type_class_add_private(klass, sizeof(UcaPylonCameraPrivate));
}

static void uca_pylon_camera_init(UcaPylonCamera *self)
{

    self->priv = UCA_PYLON_CAMERA_GET_PRIVATE(self);

    /* binnings */
    GValue val = {0};
    g_value_init(&val, G_TYPE_UINT);
    g_value_set_uint(&val, 1);
    self->priv->binnings  = g_value_array_new(1);
    g_value_array_append(self->priv->binnings, &val);
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_PYLON_CAMERA;
}
