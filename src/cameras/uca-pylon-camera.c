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
#include <stdio.h>
#include <string.h>
#include "uca-camera.h"
#include "uca-pylon-camera.h"
#include "pylon_camera.h"


/*#define HANDLE_PYLON_ERROR(err)                       \
    if ((err) != PYLON_NOERROR) {                     \
        g_set_error(error, UCA_PYLON_CAMERA_ERROR,    \
                UCA_PYLON_CAMERA_ERROR_LIBPYLON_GENERAL,\
                "libpylon error %i", err);            \
        return;                                     \
    }*/
    
#define UCA_PYLON_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_PYLON_CAMERA, UcaPylonCameraPrivate))

G_DEFINE_TYPE(UcaPylonCamera, uca_pylon_camera, UCA_TYPE_CAMERA)

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
    N_PROPERTIES = N_BASE_PROPERTIES
};

static gint base_overrideables[] = {
    PROP_NAME,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_BITDEPTH,
//    PROP_SENSOR_HORIZONTAL_BINNING,
//    PROP_SENSOR_HORIZONTAL_BINNINGS,
//    PROP_SENSOR_VERTICAL_BINNING,
//    PROP_SENSOR_VERTICAL_BINNINGS,
//    PROP_SENSOR_MAX_FRAME_RATE,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
//    PROP_HAS_STREAMING,
//    PROP_HAS_CAMRAM_RECORDING,
    0
};


static GParamSpec *pylon_properties[N_PROPERTIES] = { NULL, };


struct _UcaPylonCameraPrivate {
    guint bit_depth;
    gsize num_bytes;

    guint width;
    guint height;
    guint16 roi_x, roi_y;
    guint16 roi_width, roi_height;
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

UcaPylonCamera *uca_pylon_camera_new(GError **error)
{
  UcaPylonCamera *camera = g_object_new(UCA_TYPE_PYLON_CAMERA, NULL);
  UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE(camera);

  pylon_camera_new("/usr/local/lib64", "141.52.111.110", error);
  if (*error) {
    g_print("Error when calling pylon_camera_new %s\n", (*error)->message);
    return NULL;
  }

  pylon_camera_get_sensor_size(&priv->width, &priv->height, error);
  if (*error) {
    g_print("Error when calling pylon_camera_get_sensor_size %s\n", (*error)->message);
    return NULL;
  }
  return camera;
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

static void uca_pylon_camera_grab(UcaCamera *camera, gpointer *data, GError **error)
{
    g_return_if_fail(UCA_IS_PYLON_CAMERA(camera));
    UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE(camera);

    if (*data == NULL) {
        *data = g_malloc0(priv->width * priv->height * priv->num_bytes); 
    }
    pylon_camera_grab(data, error);
}

static void uca_pylon_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE(object);
    GError* error = NULL;

    switch (property_id) {

        case PROP_ROI_X:
            {
              priv->roi_x = g_value_get_uint(value);
              pylon_set_roi(object, &error);
            }
            break;

        case PROP_ROI_Y:
            {
              priv->roi_y = g_value_get_uint(value);
              pylon_set_roi(object, &error);
            }
            break;

        case PROP_ROI_WIDTH:
            {
              priv->roi_width = g_value_get_uint(value);
              pylon_set_roi(object, &error);
            }
            break;

        case PROP_ROI_HEIGHT:
            {
              priv->roi_height = g_value_get_uint(value);
              pylon_set_roi(object, &error);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }
}

static void uca_pylon_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  fprintf(stderr, "pylon_get_property\n");
    UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE(object);
    GError* error = NULL;

    switch (property_id) {
      
        case PROP_SENSOR_WIDTH: 
            pylon_camera_get_sensor_size(&priv->width, &priv->height, &error);
            g_value_set_uint(value, priv->width);
            g_print("pylon_get_property sensor width %d\n", priv->width);
            break;

        case PROP_SENSOR_HEIGHT: 
            pylon_camera_get_sensor_size(&priv->width, &priv->height, &error);
            g_value_set_uint(value, priv->height);
            g_print("pylon_get_property sensor height %d\n", priv->height);
            break;

            /*
        case PROP_SENSOR_MAX_FRAME_RATE:
            g_value_set_float(value, priv->camera_description->max_frame_rate);
            break;
            */

        case PROP_SENSOR_BITDEPTH:
            pylon_camera_get_bit_depth(&priv->bit_depth, &error);
            g_value_set_uint(value, priv->bit_depth);
            g_print("pylon_get_property depth %d\n", priv->bit_depth);
            break;

            /*
        case PROP_HAS_STREAMING:
            g_value_set_boolean(value, TRUE);
            break;

        case PROP_HAS_CAMRAM_RECORDING:
            g_value_set_boolean(value, priv->camera_description->has_camram);
            break;
            */

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

        case PROP_NAME: 
            {
                //char *name = NULL;
                //pylon_get_name(priv->pylon, &name);
                g_value_set_string(value, "Pylon Camera");
                //free(name);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void uca_pylon_camera_finalize(GObject *object)
{
    /*UcaPylonCameraPrivate *priv = UCA_PYLON_CAMERA_GET_PRIVATE(object);

    if (priv->horizontal_binnings)
        g_value_array_free(priv->horizontal_binnings);

    if (priv->vertical_binnings)
        g_value_array_free(priv->vertical_binnings);

    if (priv->fg) {
        if (priv->fg_mem)
            Fg_FreeMem(priv->fg, priv->fg_port);

        Fg_FreeGrabber(priv->fg);
    }

    if (priv->pylon)
        pylon_destroy(priv->pylon);
        */

    G_OBJECT_CLASS(uca_pylon_camera_parent_class)->finalize(object);
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


    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property(gobject_class, id, pylon_properties[id]);

    g_type_class_add_private(klass, sizeof(UcaPylonCameraPrivate));
}

static void uca_pylon_camera_init(UcaPylonCamera *self)
{
    self->priv = UCA_PYLON_CAMERA_GET_PRIVATE(self);
}

