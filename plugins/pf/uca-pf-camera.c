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

#include <gio/gio.h>
#include <gmodule.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fgrab_struct.h>
#include <fgrab_prototyp.h>
#include <libpf/pfcam.h>
#include <errno.h>
#include "uca-camera.h"
#include "uca-pf-camera.h"

#define FG_TRY_PARAM(fg, camobj, param, val_addr, port)     \
    { int r = Fg_setParameter(fg, param, val_addr, port);   \
        if (r != FG_OK) {                                   \
            g_set_error(error, UCA_PF_CAMERA_ERROR,         \
                    UCA_PF_CAMERA_ERROR_FG_GENERAL,         \
                    "%s", Fg_getLastErrorDescription(fg));  \
            return NULL;                                    \
        } }

#define FG_SET_ERROR(err, fg, err_type)                 \
    if (err != FG_OK) {                                 \
        g_set_error(error, UCA_PF_CAMERA_ERROR,         \
                err_type,                               \
                "%s", Fg_getLastErrorDescription(fg));  \
        return;                                         \
    }

#define UCA_PF_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_PF_CAMERA, UcaPfCameraPrivate))

static void uca_pf_camera_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaPfCamera, uca_pf_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_pf_camera_initable_iface_init))

/**
 * UcaPfCameraError:
 * @UCA_PF_CAMERA_ERROR_INIT: Initializing pcilib failed
 * @UCA_PF_CAMERA_ERROR_FG_GENERAL: Frame grabber errors
 * @UCA_PF_CAMERA_ERROR_FG_ACQUISITION: Acquisition errors related to the frame
 *      grabber
 * @UCA_PF_CAMERA_ERROR_START_RECORDING: Starting failed
 * @UCA_PF_CAMERA_ERROR_STOP_RECORDING: Stopping failed
 */
GQuark uca_pf_camera_error_quark()
{
    return g_quark_from_static_string("uca-pf-camera-error-quark");
}

static gint base_overrideables[] = {
    PROP_NAME,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_BITDEPTH,
    PROP_SENSOR_HORIZONTAL_BINNING,
    PROP_SENSOR_HORIZONTAL_BINNINGS,
    PROP_SENSOR_VERTICAL_BINNING,
    PROP_SENSOR_VERTICAL_BINNINGS,
    PROP_SENSOR_MAX_FRAME_RATE,
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

enum {
    PROP_FOO = N_BASE_PROPERTIES,
    N_PROPERTIES
};


struct _UcaPfCameraPrivate {
    GError *construct_error;
    guint roi_width;
    guint roi_height;
    guint last_frame;

    Fg_Struct *fg;
    guint fg_port;
    dma_mem *fg_mem;
};

/*
 * We just embed our private structure here.
 */
struct {
    UcaCamera *camera;
} fg_apc_data;

static int
me4_callback(frameindex_t frame, struct fg_apc_data *apc)
{
    UcaCamera *camera = UCA_CAMERA(apc);
    UcaPfCameraPrivate *priv = UCA_PF_CAMERA_GET_PRIVATE(camera);
    camera->grab_func(Fg_getImagePtrEx(priv->fg, frame, priv->fg_port, priv->fg_mem), camera->user_data);
    return 0;
}

static void
uca_pf_camera_start_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_PF_CAMERA(camera));
    UcaPfCameraPrivate *priv = UCA_PF_CAMERA_GET_PRIVATE(camera);
    guint err = FG_OK;

    if (priv->fg_mem == NULL) {
        const guint num_buffers = 2;
        priv->fg_mem = Fg_AllocMemEx(priv->fg, num_buffers * priv->roi_width * priv->roi_height, num_buffers);

        if (priv->fg_mem == NULL) {
            g_set_error(error, UCA_PF_CAMERA_ERROR, UCA_PF_CAMERA_ERROR_FG_ACQUISITION,
                    "%s", Fg_getLastErrorDescription(priv->fg));
            g_object_unref(camera);
            return;
        }
    }

    gboolean transfer_async = FALSE;
    g_object_get(G_OBJECT(camera),
            "transfer-asynchronously", &transfer_async,
            NULL);

    if (transfer_async) {
        struct FgApcControl ctrl = {
            .version = 0,
            .data = (struct fg_apc_data *) camera,
            .func = &me4_callback,
            .flags = FG_APC_DEFAULTS,
            .timeout = 1
        };

        err = Fg_registerApcHandler(priv->fg, priv->fg_port, &ctrl, FG_APC_CONTROL_BASIC);
        FG_SET_ERROR(err, priv->fg, UCA_PF_CAMERA_ERROR_FG_ACQUISITION);
    }

    err = Fg_AcquireEx(priv->fg, priv->fg_port, GRAB_INFINITE, ACQ_STANDARD, priv->fg_mem);
    FG_SET_ERROR(err, priv->fg, UCA_PF_CAMERA_ERROR_FG_ACQUISITION);
}

static void
uca_pf_camera_stop_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_PF_CAMERA(camera));
    UcaPfCameraPrivate *priv = UCA_PF_CAMERA_GET_PRIVATE(camera);

    guint err = Fg_stopAcquireEx(priv->fg, priv->fg_port, priv->fg_mem, STOP_SYNC);
    FG_SET_ERROR(err, priv->fg, UCA_PF_CAMERA_ERROR_FG_ACQUISITION);

    err = Fg_setStatusEx(priv->fg, FG_UNBLOCK_ALL, 0, priv->fg_port, priv->fg_mem);
    if (err == FG_INVALID_PARAMETER)
        g_print(" Unable to unblock all\n");
}

static void
uca_pf_camera_start_readout(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_PF_CAMERA(camera));
    g_set_error(error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_IMPLEMENTED,
            "This photon focus camera does not support recording to internal memory");
}

static gboolean
uca_pf_camera_grab(UcaCamera *camera, gpointer data, GError **error)
{
    g_return_val_if_fail (UCA_IS_PF_CAMERA (camera), FALSE);
    UcaPfCameraPrivate *priv = UCA_PF_CAMERA_GET_PRIVATE(camera);

    priv->last_frame = Fg_getLastPicNumberBlockingEx (priv->fg, priv->last_frame+1,
                                                      priv->fg_port, 5, priv->fg_mem);

    if (priv->last_frame <= 0) {
        g_set_error (error, UCA_PF_CAMERA_ERROR,
                     UCA_PF_CAMERA_ERROR_FG_ACQUISITION,
                     "%s", Fg_getLastErrorDescription(priv->fg));
        return FALSE;
    }

    guint16 *frame = Fg_getImagePtrEx (priv->fg, priv->last_frame,
                                       priv->fg_port, priv->fg_mem);

    memcpy((gchar *) data, (gchar *) frame, priv->roi_width * priv->roi_height);
    return TRUE;
}

static void
uca_pf_camera_trigger(UcaCamera *camera, GError **error)
{
}

static void
uca_pf_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }
}

static void
uca_pf_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        case PROP_SENSOR_WIDTH:
            g_value_set_uint(value, 1280);
            break;
        case PROP_SENSOR_HEIGHT:
            g_value_set_uint(value, 1024);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint(value, 8);
            break;
        case PROP_SENSOR_HORIZONTAL_BINNING:
            break;
        case PROP_SENSOR_HORIZONTAL_BINNINGS:
            break;
        case PROP_SENSOR_VERTICAL_BINNING:
            break;
        case PROP_SENSOR_VERTICAL_BINNINGS:
            break;
        case PROP_SENSOR_MAX_FRAME_RATE:
            g_value_set_float(value, 488.0);
            break;
        case PROP_HAS_STREAMING:
            g_value_set_boolean(value, TRUE);
            break;
        case PROP_HAS_CAMRAM_RECORDING:
            g_value_set_boolean(value, FALSE);
            break;
        case PROP_EXPOSURE_TIME:
            g_value_set_double(value, 1. / 488.0);
            break;
        case PROP_ROI_X:
            g_value_set_uint(value, 0);
            break;
        case PROP_ROI_Y:
            g_value_set_uint(value, 0);
            break;
        case PROP_ROI_WIDTH:
            g_value_set_uint(value, 1280);
            break;
        case PROP_ROI_HEIGHT:
            g_value_set_uint(value, 1024);
            break;
        case PROP_ROI_WIDTH_MULTIPLIER:
            g_value_set_uint(value, 1);
            break;
        case PROP_ROI_HEIGHT_MULTIPLIER:
            g_value_set_uint(value, 1);
            break;
        case PROP_NAME:
            g_value_set_string(value, "Photon Focus MV2-D1280-640-CL");
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
uca_pf_camera_finalize(GObject *object)
{
    UcaPfCameraPrivate *priv = UCA_PF_CAMERA_GET_PRIVATE(object);

    if (priv->fg) {
        if (priv->fg_mem)
            Fg_FreeMemEx(priv->fg, priv->fg_mem);

        Fg_FreeGrabber(priv->fg);
    }

    g_clear_error (&priv->construct_error);

    G_OBJECT_CLASS(uca_pf_camera_parent_class)->finalize(object);
}

static gboolean
uca_pf_camera_initable_init (GInitable *initable,
                             GCancellable *cancellable,
                             GError **error)
{
    UcaPfCamera *camera;
    UcaPfCameraPrivate *priv;

    g_return_val_if_fail (UCA_IS_PF_CAMERA (initable), FALSE);

    if (cancellable != NULL) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             "Cancellable initialization not supported");
        return FALSE;
    }

    camera = UCA_PF_CAMERA (initable);
    priv = camera->priv;

    if (priv->construct_error != NULL) {
        if (error)
            *error = g_error_copy (priv->construct_error);

        return FALSE;
    }

    if (priv->fg == NULL) {
        g_set_error (error, UCA_PF_CAMERA_ERROR, UCA_PF_CAMERA_ERROR_INIT,
                     "%s", Fg_getLastErrorDescription (NULL));
        return FALSE;
    }

    return TRUE;
}

static void
uca_pf_camera_initable_iface_init (GInitableIface *iface)
{
    iface->init = uca_pf_camera_initable_init;
}

static void
uca_pf_camera_class_init(UcaPfCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_pf_camera_set_property;
    gobject_class->get_property = uca_pf_camera_get_property;
    gobject_class->finalize = uca_pf_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_pf_camera_start_recording;
    camera_class->stop_recording = uca_pf_camera_stop_recording;
    camera_class->start_readout = uca_pf_camera_start_readout;
    camera_class->grab = uca_pf_camera_grab;
    camera_class->trigger = uca_pf_camera_trigger;

    for (guint i = 0; base_overrideables[i] != 0; i++)
        g_object_class_override_property(gobject_class, base_overrideables[i], uca_camera_props[base_overrideables[i]]);

    g_type_class_add_private(klass, sizeof(UcaPfCameraPrivate));
}

static gboolean
try_fg_param (UcaPfCameraPrivate *priv,
              int param,
              const gpointer value)
{
    if (Fg_setParameter (priv->fg, param, value, priv->fg_port) != FG_OK) {
        g_set_error (&priv->construct_error, UCA_PF_CAMERA_ERROR,
                     UCA_PF_CAMERA_ERROR_FG_GENERAL,
                     "%s", Fg_getLastErrorDescription (priv->fg));
        return FALSE;
    }

    return TRUE;
}

static void
uca_pf_camera_init (UcaPfCamera *self)
{
    UcaPfCameraPrivate *priv;
    static const gchar *so_file = "libFullAreaGray8.so";
    static const int link_type = FG_CL_8BIT_FULL_8;
    static const int format = FG_GRAY;

    self->priv = priv = UCA_PF_CAMERA_GET_PRIVATE(self);

    priv->construct_error = NULL;
    priv->fg_port = PORT_A;
    priv->fg = Fg_Init (so_file, priv->fg_port);
    priv->fg_mem = NULL;
    priv->last_frame = 0;
    priv->roi_width = 1280;
    priv->roi_height = 1024;

    if (priv->fg != NULL) {
        if (!try_fg_param (priv, FG_CAMERA_LINK_CAMTYP, (const gpointer) &link_type))
            return;

        if (!try_fg_param (priv, FG_FORMAT, (const gpointer) &format))
            return;

        if (!try_fg_param (priv, FG_WIDTH, &priv->roi_width))
            return;

        if (!try_fg_param (priv, FG_HEIGHT, &priv->roi_height))
            return;
    }
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_PF_CAMERA;
}
