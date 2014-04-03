/* Copyright (C) 2011-2013 Matthias Vogelgesang <matthias.vogelgesang@kit.edu>
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
#include <xkit/dll_api.h>

#undef FALSE
#undef TRUE

#include <gio/gio.h>
#include <gmodule.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "uca-camera.h"
#include "uca-xkit-camera.h"


#define UCA_XKIT_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_XKIT_CAMERA, UcaXkitCameraPrivate))

#define MEDIPIX_SENSOR_SIZE   256

static void uca_xkit_camera_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaXkitCamera, uca_xkit_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_xkit_camera_initable_iface_init))

GQuark uca_xkit_camera_error_quark()
{
    return g_quark_from_static_string("uca-xkit-camera-error-quark");
}

enum {
    PROP_NUM_CHIPS = N_BASE_PROPERTIES,
    N_PROPERTIES
};

static gint base_overrideables[] = {
    PROP_NAME,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_MAX_FRAME_RATE,
    PROP_SENSOR_BITDEPTH,
    PROP_EXPOSURE_TIME,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    PROP_HAS_STREAMING,
    PROP_HAS_CAMRAM_RECORDING,
    0,
};

static GParamSpec *xkit_properties[N_PROPERTIES] = { NULL, };     

struct _UcaXkitCameraPrivate {
    GError  *construct_error;
    gsize    n_chips;
};

static gboolean
setup_xkit (UcaXkitCameraPrivate *priv)
{
    priv->n_chips = 1;

    x_kit_init ();
    x_kit_reset_sensors ();
    x_kit_initialize_matrix ();

    return TRUE;
}

static void
uca_xkit_camera_start_recording (UcaCamera *camera,
                                 GError **error)
{
    g_return_if_fail (UCA_IS_XKIT_CAMERA (camera));
    x_kit_start_acquisition (X_KIT_ACQUIRE_CONTINUOUS);
}

static void
uca_xkit_camera_stop_recording (UcaCamera *camera,
                                GError **error)
{
    g_return_if_fail (UCA_IS_XKIT_CAMERA (camera));
}

static void
uca_xkit_camera_start_readout (UcaCamera *camera,
                               GError **error)
{
    g_return_if_fail( UCA_IS_XKIT_CAMERA (camera));
}

static void
uca_xkit_camera_stop_readout (UcaCamera *camera,
                              GError **error)
{
    g_return_if_fail (UCA_IS_XKIT_CAMERA (camera));
}

static gboolean
uca_xkit_camera_grab (UcaCamera *camera,
                      gpointer data,
                      GError **error)
{
    g_return_val_if_fail (UCA_IS_XKIT_CAMERA (camera), FALSE);
    UcaXkitCameraPrivate *priv;

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (camera);

    x_kit_serial_matrix_readout ((guint16 **) &data, &priv->n_chips, X_KIT_READOUT_DEFAULT);
    return TRUE;
}

static void
uca_xkit_camera_trigger (UcaCamera *camera,
                         GError **error)
{
    g_return_if_fail (UCA_IS_XKIT_CAMERA (camera));
}

static void
uca_xkit_camera_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            return;
    }
}

static void
uca_xkit_camera_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UcaXkitCameraPrivate *priv;

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NAME:
            g_value_set_string (value, "xkit");
            break;
        case PROP_SENSOR_MAX_FRAME_RATE:
            g_value_set_float (value, 150.0f);
            break;
        case PROP_SENSOR_WIDTH:
            g_value_set_uint (value, priv->n_chips * MEDIPIX_SENSOR_SIZE);
            break;
        case PROP_SENSOR_HEIGHT:
            g_value_set_uint (value, MEDIPIX_SENSOR_SIZE);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint (value, 11);
            break;
        case PROP_EXPOSURE_TIME:
            g_value_set_double (value, 0.5);
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
            g_value_set_uint (value, 256);
            break;
        case PROP_ROI_HEIGHT:
            g_value_set_uint (value, 256);
            break;
        case PROP_NUM_CHIPS:
            g_value_set_uint (value, priv->n_chips);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
uca_xkit_camera_finalize(GObject *object)
{
    UcaXkitCameraPrivate *priv;

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (object);
    g_clear_error (&priv->construct_error);

    G_OBJECT_CLASS (uca_xkit_camera_parent_class)->finalize (object);
}

static gboolean
ufo_xkit_camera_initable_init (GInitable *initable,
                               GCancellable *cancellable,
                               GError **error)
{
    UcaXkitCamera *camera;
    UcaXkitCameraPrivate *priv;

    g_return_val_if_fail (UCA_IS_XKIT_CAMERA (initable), FALSE);

    if (cancellable != NULL) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             "Cancellable initialization not supported");
        return FALSE;
    }

    camera = UCA_XKIT_CAMERA (initable);
    priv = camera->priv;

    if (priv->construct_error != NULL) {
        if (error)
            *error = g_error_copy (priv->construct_error);

        return FALSE;
    }

    return TRUE;
}

static void
uca_xkit_camera_initable_iface_init (GInitableIface *iface)
{
    iface->init = ufo_xkit_camera_initable_init;
}

static void
uca_xkit_camera_class_init (UcaXkitCameraClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    UcaCameraClass *camera_class = UCA_CAMERA_CLASS (klass);

    oclass->set_property = uca_xkit_camera_set_property;
    oclass->get_property = uca_xkit_camera_get_property;
    oclass->finalize = uca_xkit_camera_finalize;

    camera_class->start_recording = uca_xkit_camera_start_recording;
    camera_class->stop_recording = uca_xkit_camera_stop_recording;
    camera_class->start_readout = uca_xkit_camera_start_readout;
    camera_class->stop_readout = uca_xkit_camera_stop_readout;
    camera_class->grab = uca_xkit_camera_grab;
    camera_class->trigger = uca_xkit_camera_trigger;

    for (guint i = 0; base_overrideables[i] != 0; i++)
        g_object_class_override_property (oclass, base_overrideables[i], uca_camera_props[base_overrideables[i]]);

    xkit_properties[PROP_NUM_CHIPS] =
        g_param_spec_uint("num-sensor-chips",
            "Number of sensor chips",
            "Number of sensor chips",
            1, 6, 1,
            G_PARAM_READABLE);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property (oclass, id, xkit_properties[id]);

    g_type_class_add_private (klass, sizeof(UcaXkitCameraPrivate));
}

static void
uca_xkit_camera_init (UcaXkitCamera *self)
{
    UcaXkitCameraPrivate *priv;

    self->priv = priv = UCA_XKIT_CAMERA_GET_PRIVATE (self);
    priv->construct_error = NULL;

    if (!setup_xkit (priv))
        return;
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_XKIT_CAMERA;
}
