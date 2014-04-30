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

#define MEDIPIX_SENSOR_SIZE     256
#define CHIPS_PER_ROW           3
#define CHIPS_PER_COLUMN        2
#define NUM_CHIPS               CHIPS_PER_ROW * CHIPS_PER_COLUMN
#define NUM_FSRS                256

static void uca_xkit_camera_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaXkitCamera, uca_xkit_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_xkit_camera_initable_iface_init))

GQuark uca_xkit_camera_error_quark()
{
    return g_quark_from_static_string("uca-xkit-camera-error-quark");
}

enum {
    PROP_NUM_CHIPS = N_BASE_PROPERTIES,
    PROP_FLIP_CHIP_0,
    PROP_FLIP_CHIP_1,
    PROP_FLIP_CHIP_2,
    PROP_FLIP_CHIP_3,
    PROP_FLIP_CHIP_4,
    PROP_FLIP_CHIP_5,
    PROP_READOUT_BIG_ENDIAN,
    PROP_READOUT_MIRRORED,
    PROP_READOUT_INCREMENTAL_GREYSCALE,
    N_PROPERTIES
};

static gint base_overrideables[] = {
    PROP_NAME,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_MAX_FRAME_RATE,
    PROP_SENSOR_BITDEPTH,
    PROP_EXPOSURE_TIME,
    PROP_TRIGGER_MODE,
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
    GError *construct_error;
    gsize n_chips;
    gsize n_max_chips;
    guint16 **buffers;
    UcaCameraTrigger trigger;
    gboolean flip[NUM_CHIPS];
    gsize n_fsrs;
    gsize n_max_fsrs;
    guint16 **fsr_buffers;
    gboolean shift[NUM_FSRS];
    gdouble exposure_time;
    guint acq_time_cycles;
    gboolean endian;
    gboolean mirrored;
    gboolean incremental;
    guint readout;
    XKitAcquistionMode mode;
    XKitReadoutOptions options;
};

static gboolean
setup_xkit (UcaXkitCameraPrivate *priv)
{
    priv->n_max_chips = priv->n_chips = NUM_CHIPS;
    priv->buffers = g_new0 (guint16 *, priv->n_max_chips);

    priv->n_max_fsrs = priv->n_fsrs = NUM_FSRS;
    priv->fsr_buffers = g_new0 (guint16 *, priv->n_max_fsrs);

    for (guint i = 0; i < priv->n_max_chips; i++)
        priv->buffers[i] = (guint16 *) g_malloc0 (MEDIPIX_SENSOR_SIZE * MEDIPIX_SENSOR_SIZE * sizeof(guint16));

    for (guint j = 0; j < priv->n_max_fsrs; j++)
        priv->fsr_buffers[j] = (guint16 *) g_malloc0 (16 * 1 * sizeof(guint16));

    x_kit_init ();

    //guint ids = 0x00112340;
    //x_kit_read_FSR (priv->fsr_buffers, ids, priv->n_chips);

    x_kit_reset_sensors ();
    x_kit_initialize_matrix ();

    return TRUE;
}

static void
compose_image (guint16 *output, guint16 **buffers, gboolean *flip, gsize n)
{
    gsize dst_stride;
    gsize cx = 0;
    gsize cy = 0;

    dst_stride = CHIPS_PER_ROW * MEDIPIX_SENSOR_SIZE;

    for (gsize i = 0; i < n; i++) {
        guint16 *dst;
        guint16 *src;
        gsize src_stride;

        if (cx == CHIPS_PER_ROW) {
            cx = 0;
            cy++;
        }

        if (flip[i]) {
            src = buffers[i] + MEDIPIX_SENSOR_SIZE * MEDIPIX_SENSOR_SIZE;
            src_stride = -MEDIPIX_SENSOR_SIZE;
        }
        else {
            src = buffers[i];
            src_stride = MEDIPIX_SENSOR_SIZE;
        }

        dst = output + (cy * MEDIPIX_SENSOR_SIZE * dst_stride + cx * MEDIPIX_SENSOR_SIZE);

        for (gsize row = 0; row < 256; row++) {
            memcpy (dst, src, MEDIPIX_SENSOR_SIZE * sizeof (guint16));
            dst += dst_stride;
            src += src_stride;
        }

        cx++;
    }
}

static void
uca_xkit_camera_start_recording (UcaCamera *camera,
                                 GError **error)
{
    UcaXkitCameraPrivate *priv;

    g_return_if_fail (UCA_IS_XKIT_CAMERA (camera));

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (camera);
    g_object_get (camera, "trigger-mode", &priv->trigger, NULL);
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
    UcaXkitCameraPrivate *priv;

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (camera);

    if (priv->mode == X_KIT_ACQUIRE_TRIGGERED) {
        priv->exposure_time = x_kit_set_exposure_time(priv->acq_time_cycles);
     /**   '6' SET FSR if hardware
        x_kit_set_fast_shift_register (priv->n_fsrs, priv->n_chips);
           '9' WRITE MATRIX if hardware
        x_kit_write_matrix (priv->buffers, &priv->n_chips, priv->mode);*/
    }
    
    x_kit_start_acquisition (priv->mode);
    priv->options = (XKitReadoutOptions) priv->readout;
    x_kit_serial_matrix_readout (priv->buffers, &priv->n_chips, priv->options);
    compose_image ((guint16 *) data, priv->buffers, priv->flip, priv->n_chips);

    return TRUE;
}

static void
uca_xkit_camera_trigger (UcaCamera *camera,
                         GError **error)
{
    g_return_if_fail (UCA_IS_XKIT_CAMERA (camera));

    UcaXkitCameraPrivate *priv;
    priv = UCA_XKIT_CAMERA_GET_PRIVATE (camera);
    x_kit_start_acquisition (priv->mode);
}

static void
uca_xkit_camera_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UcaXkitCameraPrivate *priv;

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_EXPOSURE_TIME:
            priv->exposure_time = g_value_get_double(value);
            break;

        case PROP_TRIGGER_MODE:
            if (priv->trigger == UCA_CAMERA_TRIGGER_AUTO)
                priv->mode = X_KIT_ACQUIRE_CONTINUOUS;
            else if (priv->trigger == UCA_CAMERA_TRIGGER_SOFTWARE)
                priv->mode = X_KIT_ACQUIRE_NORMAL;
            else if (priv->trigger == UCA_CAMERA_TRIGGER_EXTERNAL)
                priv->mode = X_KIT_ACQUIRE_TRIGGERED;
            break;

        case PROP_FLIP_CHIP_0:
        case PROP_FLIP_CHIP_1:
        case PROP_FLIP_CHIP_2:
        case PROP_FLIP_CHIP_3:
        case PROP_FLIP_CHIP_4:
        case PROP_FLIP_CHIP_5:
            priv->flip[property_id - PROP_FLIP_CHIP_0] = g_value_get_boolean (value);
            break;

        case PROP_READOUT_BIG_ENDIAN:
            priv->endian = g_value_get_boolean (value);
            if (priv->endian == TRUE)
                priv->readout |= X_KIT_READOUT_BIG_ENDIAN;
            else
                priv->readout &= ~X_KIT_READOUT_BIG_ENDIAN;
            break;

        case PROP_READOUT_MIRRORED:
            priv->mirrored = g_value_get_boolean (value);
            if (priv->mirrored == TRUE)
                priv->readout |= X_KIT_READOUT_MIRRORED;
            else
                priv->readout &= ~X_KIT_READOUT_MIRRORED;
            break;

        case PROP_READOUT_INCREMENTAL_GREYSCALE:
            priv->incremental = g_value_get_boolean (value);
            if (priv->incremental == TRUE)
                priv->readout |= X_KIT_READOUT_INCREMENTAL_GREYSCALE;
            else
                priv->readout &= ~X_KIT_READOUT_INCREMENTAL_GREYSCALE;
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
    UcaXkitCameraPrivate *priv;

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NAME:
            g_value_set_string (value, "xkit");
            break;
        case PROP_SENSOR_MAX_FRAME_RATE:
            g_value_set_float (value, 150.0f);
            break;
        case PROP_SENSOR_WIDTH:
            g_value_set_uint (value, CHIPS_PER_ROW * MEDIPIX_SENSOR_SIZE);
            break;
        case PROP_SENSOR_HEIGHT:
            g_value_set_uint (value, CHIPS_PER_COLUMN * MEDIPIX_SENSOR_SIZE);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint (value, 16);
            break;
        case PROP_EXPOSURE_TIME:
            g_value_set_double (value, priv->exposure_time);
            break;
        case PROP_TRIGGER_MODE:
            g_value_set_enum (value, priv->trigger);
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
            g_value_set_uint (value, CHIPS_PER_ROW * MEDIPIX_SENSOR_SIZE);
            break;
        case PROP_ROI_HEIGHT:
            g_value_set_uint (value, CHIPS_PER_COLUMN * MEDIPIX_SENSOR_SIZE);
            break;
        case PROP_NUM_CHIPS:
            g_value_set_uint (value, priv->n_chips);
            break;

        case PROP_FLIP_CHIP_0:
        case PROP_FLIP_CHIP_1:
        case PROP_FLIP_CHIP_2:
        case PROP_FLIP_CHIP_3:
        case PROP_FLIP_CHIP_4:
        case PROP_FLIP_CHIP_5:
            g_value_set_boolean (value, priv->flip[property_id - PROP_FLIP_CHIP_0]);
            break;

        case PROP_READOUT_BIG_ENDIAN:
            g_value_set_boolean (value, priv->endian);
            break;
        case PROP_READOUT_MIRRORED:
            g_value_set_boolean (value, priv->mirrored);
            break;
        case PROP_READOUT_INCREMENTAL_GREYSCALE:
            g_value_set_boolean (value, priv->incremental);
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

    for (guint i = 0; i < priv->n_max_chips; i++)
        g_free (priv->buffers[i]);

    for (guint j = 0; j < priv->n_max_fsrs; j++)
        g_free (priv->fsr_buffers[j]);

    g_free (priv->buffers);
    g_free (priv->fsr_buffers);

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

    xkit_properties[PROP_NUM_CHIPS] =
        g_param_spec_uint("num-sensor-chips",
            "Number of sensor chips",
            "Number of sensor chips",
            1, 6, 6,
            G_PARAM_READABLE);

    xkit_properties[PROP_READOUT_BIG_ENDIAN] =
        g_param_spec_boolean("big-endian-readout",
            "Big Endian Readout",
            "Big Endian Readout",
            FALSE,
            (GParamFlags) G_PARAM_READWRITE);

    xkit_properties[PROP_READOUT_MIRRORED] =
        g_param_spec_boolean("mirrored-readout",
            "Mirrored Readout",
            "Mirrored Readout",
            FALSE, 
            (GParamFlags) G_PARAM_READWRITE);

    xkit_properties[PROP_READOUT_INCREMENTAL_GREYSCALE] =
        g_param_spec_boolean("incremental-greyscale-readout",
            "Incremental Greyscale Readout",
            "Incremental Greyscale Readout",
            FALSE,
            (GParamFlags) G_PARAM_READWRITE);

    for (guint i = 0; i < NUM_CHIPS; i++) {
        gchar *prop_name;

        prop_name = g_strdup_printf ("flip-chip-%i", i);

        xkit_properties[PROP_FLIP_CHIP_0 + i] =
            g_param_spec_boolean (prop_name, "Flip chip", "Flip chip", FALSE, (GParamFlags) G_PARAM_READWRITE);

        g_free (prop_name);
    }

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property (oclass, id, xkit_properties[id]);

    g_type_class_add_private (klass, sizeof(UcaXkitCameraPrivate));
}

static void
uca_xkit_camera_init (UcaXkitCamera *self)
{
    UcaXkitCameraPrivate *priv;

    self->priv = priv = UCA_XKIT_CAMERA_GET_PRIVATE (self);
    self->priv->exposure_time = 0.5;
    priv->construct_error = NULL;

    for (guint i = 0; i < NUM_CHIPS; i++)
        priv->flip[i] = FALSE;

    for (guint j = 0; j < NUM_FSRS; j++)
        priv->shift[j] = FALSE;

    if (!setup_xkit (priv))
        return;
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_XKIT_CAMERA;
}
