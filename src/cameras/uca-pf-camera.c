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
            g_object_unref(camobj);                         \
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

G_DEFINE_TYPE(UcaPfCamera, uca_pf_camera, UCA_TYPE_CAMERA)

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
    PROP_HAS_STREAMING,
    PROP_HAS_CAMRAM_RECORDING,
    0
};

enum {
    PROP_NAME = N_BASE_PROPERTIES,
    N_PROPERTIES
};

static GParamSpec *pf_properties[N_PROPERTIES] = { NULL, };


struct _UcaPfCameraPrivate {
    guint roi_width;
    guint roi_height;
    guint last_frame;

    Fg_Struct *fg;
    guint fg_port;
    dma_mem *fg_mem;
};

UcaPfCamera *uca_pf_camera_new(GError **error)
{
    static const gchar *so_file = "libFullAreaGray8.so";
    static const int camera_link_type = FG_CL_8BIT_FULL_10;
    static const int camera_format = FG_GRAY;

    /* gint num_ports; */
    /* if (pfPortInit(&num_ports) < 0) { */
    /*     g_set_error(error, UCA_PF_CAMERA_ERROR, UCA_PF_CAMERA_ERROR_INIT, */
    /*             "Could not initialize ports"); */ 
    /*     return NULL; */
    /* } */

    /* g_print("We have %i ports\n", num_ports); */
    /* gchar vendor[256]; */
    /* gint vendor_size; */
    /* gchar name[256]; */
    /* gint name_size; */
    /* int version, type; */
    /* for (guint i = 0; i < num_ports; i++) { */
    /*     int result = pfPortInfo(i, vendor, &vendor_size, name, &name_size, &version, &type); */
    /*     if (result < 0) { */
    /*         g_print("[%i] could not retrieve information\n", i); */ 
    /*     } */
    /*     else { */
    /*         int baudrate; */
    /*         pfGetBaudRate(i, &baudrate); */
    /*         g_print("[%i] %s %s %i %i at %i bps\n", i, vendor, name, version, type, baudrate); */
    /*     } */
    /* } */

    /* if (pfDeviceOpen(0) != 0) { */
    /*     g_set_error(error, UCA_PF_CAMERA_ERROR, UCA_PF_CAMERA_ERROR_INIT, */
    /*             "Could not open device"); */ 
    /*     return NULL; */
    /* } */

    UcaPfCamera *camera = g_object_new(UCA_TYPE_PF_CAMERA, NULL);
    UcaPfCameraPrivate *priv = UCA_PF_CAMERA_GET_PRIVATE(camera);

    priv->fg_port = PORT_A;
    priv->fg = Fg_Init(so_file, priv->fg_port);

    /* TODO: get this from the camera */
    priv->roi_width = 1280;
    priv->roi_height = 640;

    if (priv->fg == NULL) {
        g_set_error(error, UCA_PF_CAMERA_ERROR, UCA_PF_CAMERA_ERROR_INIT,
                "%s", Fg_getLastErrorDescription(priv->fg));
        g_object_unref(camera);
        return NULL;
    }

    FG_TRY_PARAM(priv->fg, camera, FG_CAMERA_LINK_CAMTYP, &camera_link_type, priv->fg_port);
    FG_TRY_PARAM(priv->fg, camera, FG_FORMAT, &camera_format, priv->fg_port);
    FG_TRY_PARAM(priv->fg, camera, FG_WIDTH, &priv->roi_width, priv->fg_port);
    FG_TRY_PARAM(priv->fg, camera, FG_HEIGHT, &priv->roi_height, priv->fg_port);

    return camera;
}

/*
 * We just embed our private structure here.
 */
struct {
    UcaCamera *camera;
} fg_apc_data;

static int me4_callback(frameindex_t frame, struct fg_apc_data *apc)
{
    UcaCamera *camera = UCA_CAMERA(apc);
    UcaPfCameraPrivate *priv = UCA_PF_CAMERA_GET_PRIVATE(camera);
    camera->grab_func(Fg_getImagePtrEx(priv->fg, frame, priv->fg_port, priv->fg_mem), camera->user_data);
    return 0;
}

static void uca_pf_camera_start_recording(UcaCamera *camera, GError **error)
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

static void uca_pf_camera_stop_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_PF_CAMERA(camera));
    UcaPfCameraPrivate *priv = UCA_PF_CAMERA_GET_PRIVATE(camera);

    guint err = Fg_stopAcquireEx(priv->fg, priv->fg_port, priv->fg_mem, STOP_SYNC);
    FG_SET_ERROR(err, priv->fg, UCA_PF_CAMERA_ERROR_FG_ACQUISITION);

    err = Fg_setStatusEx(priv->fg, FG_UNBLOCK_ALL, 0, priv->fg_port, priv->fg_mem);
    if (err == FG_INVALID_PARAMETER)
        g_print(" Unable to unblock all\n");
}

static void uca_pf_camera_start_readout(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_PF_CAMERA(camera));
    g_set_error(error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_NOT_IMPLEMENTED,
            "This photon focus camera does not support recording to internal memory");
}

static void uca_pf_camera_grab(UcaCamera *camera, gpointer *data, GError **error)
{
    g_return_if_fail(UCA_IS_PF_CAMERA(camera));
    UcaPfCameraPrivate *priv = UCA_PF_CAMERA_GET_PRIVATE(camera);

    priv->last_frame = Fg_getLastPicNumberBlockingEx(priv->fg, priv->last_frame+1, priv->fg_port, 5, priv->fg_mem);
    
    if (priv->last_frame <= 0) {
        guint err = FG_OK + 1;
        FG_SET_ERROR(err, priv->fg, UCA_PF_CAMERA_ERROR_FG_GENERAL);
    }

    guint16 *frame = Fg_getImagePtrEx(priv->fg, priv->last_frame, priv->fg_port, priv->fg_mem);

    if (*data == NULL)
        *data = g_malloc0(priv->roi_width * priv->roi_height);

    memcpy((gchar *) *data, (gchar *) frame, priv->roi_width * priv->roi_height);
}

static void uca_pf_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }
}

static void uca_pf_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
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

        case PROP_NAME: 
            g_value_set_string(value, "Photon Focus MV2-D1280-640-CL");
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void uca_pf_camera_finalize(GObject *object)
{
    G_OBJECT_CLASS(uca_pf_camera_parent_class)->finalize(object);
}

static void uca_pf_camera_class_init(UcaPfCameraClass *klass)
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

    for (guint i = 0; base_overrideables[i] != 0; i++)
        g_object_class_override_property(gobject_class, base_overrideables[i], uca_camera_props[base_overrideables[i]]);

    pf_properties[PROP_NAME] = 
        g_param_spec_string("name",
            "Name of the camera",
            "Name of the camera",
            "", G_PARAM_READABLE);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property(gobject_class, id, pf_properties[id]);

    g_type_class_add_private(klass, sizeof(UcaPfCameraPrivate));
}

static void uca_pf_camera_init(UcaPfCamera *self)
{
    self->priv = UCA_PF_CAMERA_GET_PRIVATE(self);
    self->priv->fg = NULL;
    self->priv->fg_mem = NULL;
    self->priv->last_frame = 0;
}
