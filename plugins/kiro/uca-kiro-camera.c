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

#include <gmodule.h>
#include <gio/gio.h>
#include <string.h>
#include <math.h>
#include <kiro/kiro-trb.h>
#include <kiro/kiro-client.h>
#include "uca-kiro-camera.h"

#define UCA_KIRO_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_KIRO_CAMERA, UcaKiroCameraPrivate))
#undef __CREATE_RANDOM_IMAGE_DATA__

static void uca_kiro_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaKiroCamera, uca_kiro_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_kiro_initable_iface_init))

enum {
    PROP_FRAMERATE = N_BASE_PROPERTIES,
    PROP_KIRO_ADDRESS,
    PROP_KIRO_PORT,
    N_PROPERTIES
};

static const gint kiro_overrideables[] = {
    PROP_NAME,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_BITDEPTH,
    PROP_EXPOSURE_TIME,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    PROP_SENSOR_MAX_FRAME_RATE,
    PROP_HAS_STREAMING,
    PROP_HAS_CAMRAM_RECORDING,
    0,
};

static GParamSpec *kiro_properties[N_PROPERTIES] = { NULL, };

struct _UcaKiroCameraPrivate {
    guint width;
    guint height;
    guint roi_x, roi_y, roi_width, roi_height;
    gfloat frame_rate;
    gfloat max_frame_rate;
    gdouble exposure_time;
    guint8 *dummy_data;
    guint current_frame;
    GRand *rand;
    gchar* address;
    gchar* port;

    gboolean thread_running;
    gboolean kiroclient_started;

    GThread *grab_thread;
    GValueArray *binnings;
    KiroTrb *receiveBuffer;
    KiroClient *kiroReceiver;
};

static gpointer
kiro_grab_func(gpointer data)
{
    UcaKiroCamera *kiro_camera = UCA_KIRO_CAMERA(data);
    g_return_val_if_fail(UCA_IS_KIRO_CAMERA(kiro_camera), NULL);

    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE(kiro_camera);
    UcaCamera *camera = UCA_CAMERA(kiro_camera);
    const gulong sleep_time = (gulong) G_USEC_PER_SEC / priv->frame_rate;

    while (priv->thread_running) {
        camera->grab_func(priv->dummy_data, camera->user_data);
        g_usleep(sleep_time);
    }

    return NULL;
}

static void
uca_kiro_camera_start_recording(UcaCamera *camera, GError **error)
{
    gboolean transfer_async = FALSE;
    UcaKiroCameraPrivate *priv;
    g_return_if_fail(UCA_IS_KIRO_CAMERA(camera));

    priv = UCA_KIRO_CAMERA_GET_PRIVATE(camera);
    
    kiro_client_sync(priv->kiroReceiver);
    kiro_trb_adopt(priv->receiveBuffer, kiro_client_get_memory(priv->kiroReceiver));
        
    g_object_get(G_OBJECT(camera),
            "transfer-asynchronously", &transfer_async,
            NULL);

    /*
     * In case asynchronous transfer is requested, we start a new thread that
     * invokes the grab callback, otherwise nothing will be done here.
     */
    if (transfer_async) {
        GError *tmp_error = NULL;
        priv->thread_running = TRUE;
        priv->grab_thread = g_thread_create(kiro_grab_func, camera, TRUE, &tmp_error);

        if (tmp_error != NULL) {
            priv->thread_running = FALSE;
            g_propagate_error(error, tmp_error);
        }
    }
}

static void
uca_kiro_camera_stop_recording(UcaCamera *camera, GError **error)
{
    gboolean transfer_async = FALSE;
    UcaKiroCameraPrivate *priv;
    g_return_if_fail(UCA_IS_KIRO_CAMERA(camera));

    priv = UCA_KIRO_CAMERA_GET_PRIVATE(camera);
    g_free(priv->dummy_data);
    kiro_trb_purge(priv->receiveBuffer, FALSE);

    g_object_get(G_OBJECT(camera),
            "transfer-asynchronously", &transfer_async,
            NULL);

    if (transfer_async) {
        priv->thread_running = FALSE;
        g_thread_join(priv->grab_thread);
    }
}

static void
uca_kiro_camera_trigger (UcaCamera *camera, GError **error)
{
}

static gboolean
uca_kiro_camera_grab (UcaCamera *camera, gpointer data, GError **error)
{
    g_return_val_if_fail (UCA_IS_KIRO_CAMERA(camera), FALSE);

    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (camera);

    kiro_client_sync(priv->kiroReceiver);
    kiro_trb_refresh(priv->receiveBuffer);
    priv->current_frame++;
    //Element 0 is always about to be written, Element -1 is currently being written
    //Therefore, we take Element -2, to be sure this one is finished
    size_t index = kiro_trb_get_max_elements(priv->receiveBuffer) - 2;
    g_memmove (data, kiro_trb_get_element(priv->receiveBuffer, index), priv->roi_width * priv->roi_height);

    return TRUE;
}

static void
uca_kiro_camera_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail(UCA_IS_KIRO_CAMERA(object));
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_EXPOSURE_TIME:
            priv->exposure_time = g_value_get_double(value);
            break;
        case PROP_FRAMERATE:
            priv->frame_rate = g_value_get_float(value);
            break;
        case PROP_ROI_X:
            priv->roi_x = g_value_get_uint(value);
            break;
        case PROP_ROI_Y:
            priv->roi_y = g_value_get_uint(value);
            break;
        case PROP_ROI_WIDTH:
            priv->roi_width = g_value_get_uint(value);
            break;
        case PROP_ROI_HEIGHT:
            priv->roi_height = g_value_get_uint(value);
            break;
        case PROP_KIRO_ADDRESS:
            g_free(priv->address);
            priv->address = g_strdup(g_value_get_string(value));
            break;
        case PROP_KIRO_PORT:
            g_free(priv->port);
            priv->port = g_strdup(g_value_get_string(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }
}

static void
uca_kiro_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_NAME:
            g_value_set_string(value, "mock camera");
            break;
        case PROP_SENSOR_WIDTH:
            g_value_set_uint(value, priv->width);
            break;
        case PROP_SENSOR_HEIGHT:
            g_value_set_uint(value, priv->height);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint(value, 8);
            break;
        case PROP_EXPOSURE_TIME:
            g_value_set_double(value, priv->exposure_time);
            break;
        case PROP_ROI_X:
            g_value_set_uint(value, priv->roi_x);
            break;
        case PROP_ROI_Y:
            g_value_set_uint(value, priv->roi_y);
            break;
        case PROP_ROI_WIDTH:
            g_value_set_uint(value, priv->roi_width);
            break;
        case PROP_ROI_HEIGHT:
            g_value_set_uint(value, priv->roi_height);
            break;
        case PROP_SENSOR_MAX_FRAME_RATE:
            g_value_set_float(value, priv->max_frame_rate);
            break;
        case PROP_HAS_STREAMING:
            g_value_set_boolean(value, TRUE);
            break;
        case PROP_HAS_CAMRAM_RECORDING:
            g_value_set_boolean(value, FALSE);
            break;
        case PROP_FRAMERATE:
            g_value_set_float(value, priv->frame_rate);
            break;
        case PROP_KIRO_ADDRESS:
            g_value_set_string(value, priv->address);
            break;
        case PROP_KIRO_PORT:
            g_value_set_string(value, priv->port);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
uca_kiro_camera_finalize(GObject *object)
{
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE(object);

    g_rand_free (priv->rand);

    if (priv->thread_running) {
        priv->thread_running = FALSE;
        g_thread_join(priv->grab_thread);
    }

    g_free(priv->dummy_data);
    g_value_array_free(priv->binnings);

    G_OBJECT_CLASS(uca_kiro_camera_parent_class)->finalize(object);
}

static gboolean
ufo_kiro_camera_initable_init (GInitable *initable,
                               GCancellable *cancellable,
                               GError **error)
{
    g_return_val_if_fail (UCA_IS_KIRO_CAMERA (initable), FALSE);
    return TRUE;
}

static void
uca_kiro_initable_iface_init (GInitableIface *iface)
{
    iface->init = ufo_kiro_camera_initable_init;
}

static void
uca_kiro_camera_class_init(UcaKiroCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_kiro_camera_set_property;
    gobject_class->get_property = uca_kiro_camera_get_property;
    gobject_class->finalize = uca_kiro_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_kiro_camera_start_recording;
    camera_class->stop_recording = uca_kiro_camera_stop_recording;
    camera_class->grab = uca_kiro_camera_grab;
    camera_class->trigger = uca_kiro_camera_trigger;

    for (guint i = 0; kiro_overrideables[i] != 0; i++)
        g_object_class_override_property(gobject_class, kiro_overrideables[i], uca_camera_props[kiro_overrideables[i]]);

    kiro_properties[PROP_FRAMERATE] =
        g_param_spec_float("frame-rate",
                "Frame rate",
                "Number of frames per second that are taken",
                1.0f, 100.0f, 100.0f,
                G_PARAM_READWRITE);
                
    kiro_properties[PROP_KIRO_ADDRESS] =
        g_param_spec_string("kiro-address",
                "KIRO Server Address",
                "Address of the KIRO Server to grab images from",
                "127.0.0.1",
                G_PARAM_READWRITE);
                
    kiro_properties[PROP_KIRO_PORT] =
        g_param_spec_string("kiro-port",
                "KIRO Server Port",
                "Port of the KIRO Server to grab images from",
                "60010",
                G_PARAM_READWRITE);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property(gobject_class, id, kiro_properties[id]);

    g_type_class_add_private(klass, sizeof(UcaKiroCameraPrivate));
}

static void
uca_kiro_camera_init(UcaKiroCamera *self)
{
    self->priv = UCA_KIRO_CAMERA_GET_PRIVATE(self);
    self->priv->roi_x = 0;
    self->priv->roi_y = 0;
    self->priv->width = self->priv->roi_width = 512;
    self->priv->height = self->priv->roi_height = 512;
    self->priv->frame_rate = self->priv->max_frame_rate = 100000.0f;
    self->priv->grab_thread = NULL;
    self->priv->current_frame = 0;
    self->priv->exposure_time = 0.05;
    self->priv->address = g_strdup("192.168.11.61");
    self->priv->port = g_strdup("60010");
    
    self->priv->binnings = g_value_array_new(1);
    self->priv->rand = g_rand_new ();

    self->priv->receiveBuffer = g_object_new(KIRO_TYPE_TRB, NULL);
    self->priv->kiroReceiver = g_object_new(KIRO_TYPE_CLIENT, NULL);
    kiro_client_connect(self->priv->kiroReceiver, self->priv->address, self->priv->port);

    GValue val = {0};
    g_value_init(&val, G_TYPE_UINT);
    g_value_set_uint(&val, 1);
    g_value_array_append(self->priv->binnings, &val);

    uca_camera_register_unit (UCA_CAMERA (self), "frame-rate", UCA_UNIT_COUNT);
        
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_KIRO_CAMERA;
}
