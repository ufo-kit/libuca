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

#include "uca-camera.h"
#include "uca-mock-camera.h"

#define UCA_MOCK_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_MOCK_CAMERA, UcaMockCameraPrivate))

static void uca_mock_camera_interface_init(UcaCameraInterface *iface);

G_DEFINE_TYPE_WITH_CODE(UcaMockCamera, uca_mock_camera, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(UCA_TYPE_CAMERA,
                                              uca_mock_camera_interface_init));

enum {
    PROP_0,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    N_PROPERTIES
};

static const gchar *mock_overrideables[N_PROPERTIES] = {
    "sensor-width",
    "sensor-height"
};

struct _UcaMockCameraPrivate {
    guint width;
    guint height;
    guint16 *dummy_data;
};

static void uca_mock_camera_start_recording(UcaCamera *camera)
{
    g_return_if_fail(UCA_IS_MOCK_CAMERA(camera));
    g_print("start recording\n");
}

static void uca_mock_camera_stop_recording(UcaCamera *camera)
{
    g_return_if_fail(UCA_IS_MOCK_CAMERA(camera));
    g_print("stop recording\n");
}

static void uca_mock_camera_grab(UcaCamera *camera, gchar *data)
{
    g_return_if_fail(UCA_IS_MOCK_CAMERA(camera));
    /* g_memmove(data, camera->priv->dummy_data, camera->priv->width * camera->priv->height * 2); */
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
            g_value_set_uint(value, 1024);
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

static void uca_mock_camera_interface_init(UcaCameraInterface *iface)
{
    iface->start_recording = uca_mock_camera_start_recording;
    iface->stop_recording = uca_mock_camera_stop_recording;
    iface->grab = uca_mock_camera_grab;
}

static void uca_mock_camera_class_init(UcaMockCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_mock_camera_set_property;
    gobject_class->get_property = uca_mock_camera_get_property;
    gobject_class->finalize = uca_mock_camera_finalize;

    for (guint property_id = PROP_0+1; property_id < N_PROPERTIES; property_id++)
        g_object_class_override_property(gobject_class, property_id, mock_overrideables[property_id-1]);

    g_type_class_add_private(klass, sizeof(UcaMockCameraPrivate));
}

static void uca_mock_camera_init(UcaMockCamera *self)
{
    self->priv = UCA_MOCK_CAMERA_GET_PRIVATE(self);
    self->priv->width = 1024;
    self->priv->height = 768;
    self->priv->dummy_data = (guint16 *) g_malloc0(self->priv->width * self->priv->height * 2);
}
