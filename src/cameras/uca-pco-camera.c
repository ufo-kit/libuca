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
#include <libpco/libpco.h>
#include <libpco/sc2_defs.h>
#include "uca-camera.h"
#include "uca-pco-camera.h"

#define UCA_PCO_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_PCO_CAMERA, UcaPcoCameraPrivate))

static void uca_pco_camera_interface_init(UcaCameraInterface *iface);

G_DEFINE_TYPE_WITH_CODE(UcaPcoCamera, uca_pco_camera, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(UCA_TYPE_CAMERA,
                                              uca_pco_camera_interface_init));

enum {
    PROP_0,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    N_PROPERTIES
};

static const gchar *pco_overrideables[N_PROPERTIES] = {
    "sensor-width",
    "sensor-height"
};

struct _UcaPcoCameraPrivate {
    pco_handle pco;
};

UcaPcoCamera *uca_pco_camera_new(GError **error)
{
    pco_handle pco = pco_init();

    if (pco == NULL) {
        /* TODO: throw error */ 
        return NULL;
    }

    UcaPcoCamera *camera = g_object_new(UCA_TYPE_PCO_CAMERA, NULL);
    camera->priv->pco = pco;
    return camera;
}

static void uca_pco_camera_start_recording(UcaCamera *camera)
{
    g_return_if_fail(UCA_IS_PCO_CAMERA(camera));
}

static void uca_pco_camera_stop_recording(UcaCamera *camera)
{
    g_return_if_fail(UCA_IS_PCO_CAMERA(camera));
}

static void uca_pco_camera_grab(UcaCamera *camera, gchar *data)
{
    g_return_if_fail(UCA_IS_PCO_CAMERA(camera));
}

static void uca_pco_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void uca_pco_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaPcoCameraPrivate *priv = UCA_PCO_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_SENSOR_WIDTH: 
            {
                uint16_t width_std, height_std, width_ex, height_ex;
                if (pco_get_resolution(priv->pco, &width_std, &height_std, &width_ex, &height_ex) == PCO_NOERROR)
                    g_value_set_uint(value, width_std);
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

    pco_destroy(priv->pco);

    G_OBJECT_CLASS(uca_pco_camera_parent_class)->finalize(object);
}

static void uca_pco_camera_interface_init(UcaCameraInterface *iface)
{
    iface->start_recording = uca_pco_camera_start_recording;
    iface->stop_recording = uca_pco_camera_stop_recording;
    iface->grab = uca_pco_camera_grab;
}

static void uca_pco_camera_class_init(UcaPcoCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_pco_camera_set_property;
    gobject_class->get_property = uca_pco_camera_get_property;
    gobject_class->finalize = uca_pco_camera_finalize;

    for (guint property_id = PROP_0+1; property_id < N_PROPERTIES; property_id++)
        g_object_class_override_property(gobject_class, property_id, pco_overrideables[property_id-1]);

    g_type_class_add_private(klass, sizeof(UcaPcoCameraPrivate));
}

static void uca_pco_camera_init(UcaPcoCamera *self)
{
    self->priv = UCA_PCO_CAMERA_GET_PRIVATE(self);
}
