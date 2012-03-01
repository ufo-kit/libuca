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

#include <glib.h>
#include "uca-camera.h"

G_DEFINE_INTERFACE(UcaCamera, uca_camera, G_TYPE_OBJECT)

enum {
    PROP_0 = 0,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_BITDEPTH,
    PROP_SENSOR_HORIZONTAL_BINNING,
    PROP_SENSOR_VERTICAL_BINNING,
    N_PROPERTIES
};

static GParamSpec *uca_camera_properties[N_PROPERTIES] = { NULL, };

static void uca_camera_default_init(UcaCameraInterface *klass)
{
    klass->start_recording = NULL;
    klass->stop_recording = NULL;
    klass->grab = NULL;

    uca_camera_properties[PROP_SENSOR_WIDTH] = 
        g_param_spec_uint("sensor-width",
            "Width of sensor",
            "Width of the sensor in pixels",
            1, G_MAXUINT, 1,
            G_PARAM_READABLE);

    uca_camera_properties[PROP_SENSOR_HEIGHT] = 
        g_param_spec_uint("sensor-height",
            "Height of sensor",
            "Height of the sensor in pixels",
            1, G_MAXUINT, 1,
            G_PARAM_READABLE);

    uca_camera_properties[PROP_SENSOR_BITDEPTH] = 
        g_param_spec_uint("sensor-bitdepth",
            "Number of bits per pixel",
            "Number of bits per pixel",
            1, 32, 1,
            G_PARAM_READABLE);

    uca_camera_properties[PROP_SENSOR_HORIZONTAL_BINNING] = 
        g_param_spec_uint("sensor-horizontal-binning",
            "Horizontal binning",
            "Number of sensor ADCs that are combined to one pixel in horizontal direction",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    uca_camera_properties[PROP_SENSOR_VERTICAL_BINNING] = 
        g_param_spec_uint("sensor-vertical-binning",
            "Vertical binning",
            "Number of sensor ADCs that are combined to one pixel in vertical direction",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    for (int i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_interface_install_property(klass, uca_camera_properties[i]);
}

void uca_camera_start_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_CAMERA(camera));

    UcaCameraInterface *iface = UCA_CAMERA_GET_INTERFACE(camera);

    g_return_if_fail(iface != NULL);
    g_return_if_fail(iface->start_recording != NULL);

    (*iface->start_recording)(camera, error);
}

void uca_camera_stop_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_CAMERA(camera));

    UcaCameraInterface *iface = UCA_CAMERA_GET_INTERFACE(camera);

    g_return_if_fail(iface != NULL);
    g_return_if_fail(iface->start_recording != NULL);

    (*iface->stop_recording)(camera, error);
}

void uca_camera_grab(UcaCamera *camera, gchar *data, GError **error)
{
    g_return_if_fail(UCA_IS_CAMERA(camera));

    UcaCameraInterface *iface = UCA_CAMERA_GET_INTERFACE(camera);

    g_return_if_fail(iface != NULL);
    g_return_if_fail(iface->start_recording != NULL);

    (*iface->grab)(camera, data, error);
}

