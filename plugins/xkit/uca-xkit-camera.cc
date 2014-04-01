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

#define MEDIPIX_SENSOR_HEIGHT   256

static void uca_xkit_camera_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaXkitCamera, uca_xkit_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_xkit_camera_initable_iface_init))

GQuark uca_xkit_camera_error_quark()
{
    return g_quark_from_static_string("uca-xkit-camera-error-quark");
}

enum {
    PROP_POSITIVE_POLARITY = N_BASE_PROPERTIES,
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
    Mpx2Interface *interface;
    gint devices[4];
    gint n_devices;
    gint device;

    DevInfo info;
    AcqParams acq;
};


static gboolean
setup_xkit (UcaXkitCameraPrivate *priv)
{

    priv->interface = getMpx2Interface();
    priv->interface->findDevices (priv->devices, &priv->n_devices);

    if (priv->n_devices > 0)
        priv->interface->init (priv->devices[0]);

    priv->device = priv->devices[0];
    priv->interface->getDevInfo (priv->device, &priv->info);

    /* TODO: find some sensible defaults */
    priv->acq.useHwTimer = TRUE;
    priv->acq.enableCst = FALSE;
    priv->acq.polarityPositive = TRUE;
    priv->acq.mode = ACQMODE_ACQSTART_TIMERSTOP;
    priv->acq.acqCount = 1;
    priv->acq.time = 1.0;

    return TRUE;
}

static void
uca_xkit_camera_start_recording (UcaCamera *camera,
                                 GError **error)
{
    UcaXkitCameraPrivate *priv;

    g_return_if_fail (UCA_IS_XKIT_CAMERA (camera));
    priv = UCA_XKIT_CAMERA_GET_PRIVATE (camera);

    if (priv->interface->setAcqPars (priv->device, &priv->acq)) {
        g_set_error_literal (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_RECORDING,
                             "Could not set acquisition parameters");
    }
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
    UcaXkitCameraPrivate *priv;
    guint32 size;
    g_return_val_if_fail (UCA_IS_XKIT_CAMERA (camera), FALSE);

    /* XXX: For now we trigger on our own because the X-KIT chip does not
     * provide auto triggering */

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (camera);
    size = priv->info.pixCount;

    if (priv->interface->startAcquisition (priv->device)) {
        g_set_error_literal (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_RECORDING,
                             "Could not pre-trigger");
        return FALSE;
    }

    if (priv->interface->readMatrix (priv->device, (gint16 *) data, size)) {
        g_set_error_literal (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_RECORDING,
                             "Could not grab frame");
        return FALSE;
    }

    if (priv->interface->stopAcquisition (priv->device)) {
        g_set_error_literal (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_RECORDING,
                             "Could stop acquisition");
        return FALSE;
    }

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
    UcaXkitCameraPrivate *priv = UCA_XKIT_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_EXPOSURE_TIME:
            priv->acq.time = g_value_get_double (value);
            break;
        case PROP_POSITIVE_POLARITY:
            priv->acq.polarityPositive = g_value_get_boolean (value);
            break;
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
    UcaXkitCameraPrivate *priv = UCA_XKIT_CAMERA_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NAME:
            g_value_set_string (value, priv->info.ifaceName);
            break;
        case PROP_SENSOR_MAX_FRAME_RATE:
            /* TODO: pretty arbitrary, huh? */
            g_value_set_float (value, 150.0f);
            break;
        case PROP_SENSOR_WIDTH:
            g_value_set_uint (value, priv->info.rowLen);
            break;
        case PROP_SENSOR_HEIGHT:
            g_value_set_uint (value, priv->info.numberOfRows * MEDIPIX_SENSOR_HEIGHT);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint (value, 11);
            break;
        case PROP_EXPOSURE_TIME:
            g_value_set_double (value, priv->acq.time);
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
            g_value_set_uint (value, priv->info.rowLen);
            break;
        case PROP_ROI_HEIGHT:
            g_value_set_uint (value, priv->info.numberOfRows * MEDIPIX_SENSOR_HEIGHT);
            break;
        case PROP_POSITIVE_POLARITY:
            g_value_set_boolean (value, priv->acq.polarityPositive);
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

    xkit_properties[PROP_POSITIVE_POLARITY] =
        g_param_spec_boolean ("positive-polarity",
                              "Is polarity positive",
                              "Is polarity positive",
                              TRUE, (GParamFlags) G_PARAM_READWRITE);

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
