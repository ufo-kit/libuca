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

#include <string.h>
#include "uca-mock-camera.h"

#define UCA_MOCK_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_MOCK_CAMERA, UcaMockCameraPrivate))

G_DEFINE_TYPE(UcaMockCamera, uca_mock_camera, UCA_TYPE_CAMERA)

enum {
    PROP_0,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_BITDEPTH,
    N_INTERFACE_PROPERTIES,

    N_PROPERTIES
};

static const gchar *mock_overrideables[N_PROPERTIES] = {
    "sensor-width",
    "sensor-height",
    "sensor-bitdepth"
};

struct _UcaMockCameraPrivate {
    guint width;
    guint height;
    guint16 *dummy_data;
};

UcaMockCamera *uca_mock_camera_new(GError **error)
{
    UcaMockCamera *camera = g_object_new(UCA_TYPE_MOCK_CAMERA, NULL);
    return camera;
}

static void uca_mock_camera_start_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_MOCK_CAMERA(camera));
}

static void uca_mock_camera_stop_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_MOCK_CAMERA(camera));
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(camera);
}

static void uca_mock_camera_grab(UcaCamera *camera, gchar *data, GError **error)
{
    g_return_if_fail(UCA_IS_MOCK_CAMERA(camera));
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(camera);
    g_memmove(data, priv->dummy_data, priv->width * priv->height);
}

static void uca_mock_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void uca_mock_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_SENSOR_WIDTH:
            g_value_set_uint(value, priv->width);
            break;
        case PROP_SENSOR_HEIGHT:
            g_value_set_uint(value, priv->height);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint(value, 8);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void uca_mock_camera_finalize(GObject *object)
{
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(object);

    g_free(priv->dummy_data);

    G_OBJECT_CLASS(uca_mock_camera_parent_class)->finalize(object);
}

static void uca_mock_camera_class_init(UcaMockCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_mock_camera_set_property;
    gobject_class->get_property = uca_mock_camera_get_property;
    gobject_class->finalize = uca_mock_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_mock_camera_start_recording;
    camera_class->stop_recording = uca_mock_camera_stop_recording;
    camera_class->grab = uca_mock_camera_grab;

    for (guint id = PROP_0 + 1; id < N_INTERFACE_PROPERTIES; id++)
        g_object_class_override_property(gobject_class, id, mock_overrideables[id-1]);

    g_type_class_add_private(klass, sizeof(UcaMockCameraPrivate));
}

static void uca_mock_camera_init(UcaMockCamera *self)
{
    self->priv = UCA_MOCK_CAMERA_GET_PRIVATE(self);
    self->priv->width = 640;
    self->priv->height = 480;
    self->priv->dummy_data = (guint16 *) g_malloc0(self->priv->width * self->priv->height);
}
