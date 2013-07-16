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

#include <dll_api.h>

#undef FALSE
#undef TRUE

#include <gio/gio.h>
#include <gmodule.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "uca-camera.h"
#include "uca-xkit-camera.h"


#define UCA_XKIT_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_XKIT_CAMERA, UcaXkitCameraPrivate))

static void uca_xkit_camera_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaXkitCamera, uca_xkit_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_xkit_camera_initable_iface_init))

GQuark uca_xkit_camera_error_quark()
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
    PROP_SENSOR_BITDEPTH,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    0,
};

static GParamSpec *ufo_properties[N_MAX_PROPERTIES] = { NULL, };

static guint N_PROPERTIES;

struct _UcaXkitCameraPrivate {
    GError  *construct_error;
};


static gboolean
setup_xkit (UcaXkitCameraPrivate *priv)
{
    Mpx2Interface *interface;

    interface = getMpx2Interface();

    return TRUE;
}

static void
uca_xkit_camera_start_recording (UcaCamera *camera,
                                 GError **error)
{
    g_return_if_fail (UCA_IS_XKIT_CAMERA (camera));
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
    /* UcaXkitCameraPrivate *priv = UCA_XKIT_CAMERA_GET_PRIVATE(object); */

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
    /* UcaXkitCameraPrivate *priv = UCA_XKIT_CAMERA_GET_PRIVATE(object); */

    switch (property_id) {
        case PROP_ROI_WIDTH:
            g_value_set_uint (value, 512);
            break;
        case PROP_ROI_HEIGHT:
            g_value_set_uint (value, 512);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint (value, 11);
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
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_xkit_camera_set_property;
    gobject_class->get_property = uca_xkit_camera_get_property;
    gobject_class->finalize = uca_xkit_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_xkit_camera_start_recording;
    camera_class->stop_recording = uca_xkit_camera_stop_recording;
    camera_class->start_readout = uca_xkit_camera_start_readout;
    camera_class->stop_readout = uca_xkit_camera_stop_readout;
    camera_class->grab = uca_xkit_camera_grab;
    camera_class->trigger = uca_xkit_camera_trigger;

    for (guint i = 0; base_overrideables[i] != 0; i++)
        g_object_class_override_property(gobject_class, base_overrideables[i], uca_camera_props[base_overrideables[i]]);

    g_type_class_add_private(klass, sizeof(UcaXkitCameraPrivate));
}

static void
uca_xkit_camera_init(UcaXkitCamera *self)
{
    UcaCamera *camera;
    UcaXkitCameraPrivate *priv;
    GObjectClass *oclass;

    self->priv = priv = UCA_XKIT_CAMERA_GET_PRIVATE(self);
    priv->construct_error = NULL;

    if (!setup_xkit (priv))
        return;

    oclass = G_OBJECT_GET_CLASS (self);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property(oclass, id, ufo_properties[id]);
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_XKIT_CAMERA;
}
