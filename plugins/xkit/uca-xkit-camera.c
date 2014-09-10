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
#include <gio/gio.h>
#include <gmodule.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "uca-camera.h"
#include "uca-xkit-camera.h"


#define UCA_XKIT_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_XKIT_CAMERA, UcaXkitCameraPrivate))

#define XKIT_DEFAULT_IP         "192.168.1.115"
#define XKIT_DEFAULT_PORT       5002
#define XKIT_MESSAGE_SIZE       512

#define MEDIPIX_SENSOR_SIZE     256
#define CHIPS_PER_ROW           3
#define CHIPS_PER_COLUMN        1
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
    PROP_ADDR,
    PROP_PORT_CHIP_0,
    PROP_PORT_CHIP_1,
    PROP_PORT_CHIP_2,
    PROP_FLIP_CHIP_0,
    PROP_FLIP_CHIP_1,
    PROP_FLIP_CHIP_2,
    /* PROP_FLIP_CHIP_3, */
    /* PROP_FLIP_CHIP_4, */
    /* PROP_FLIP_CHIP_5, */
    PROP_READOUT_BIG_ENDIAN,
    PROP_READOUT_MIRRORED,
    PROP_READOUT_INCREMENTAL_GREYSCALE,
    N_PROPERTIES
};

static gint base_overrideables[] = {
    PROP_NAME,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_BITDEPTH,
    PROP_EXPOSURE_TIME,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    PROP_HAS_STREAMING,
    PROP_HAS_CAMRAM_RECORDING,
    0,
};

static GParamSpec *xkit_properties[N_PROPERTIES] = { NULL, };

enum {
    X_KIT_ACQUIRE_NORMAL                = 0x0,
    X_KIT_ACQUIRE_CONTINUOUS            = 0x1,
    X_KIT_ACQUIRE_TRIGGERED             = 0x2,
    X_KIT_ACQUIRE_CONTINUOUS_TRIGGERED  = 0x3,
};

enum {
    X_KIT_READOUT_DEFAULT               = 0,
    X_KIT_READOUT_BIG_ENDIAN            = 1 << 2,
    X_KIT_READOUT_MIRRORED              = 1 << 3,
    X_KIT_READOUT_INCREMENTAL_GREYSCALE = 1 << 4,
};

struct _UcaXkitCameraPrivate {
    GError *construct_error;
    gsize n_chips;
    gsize n_max_chips;
    guint16 **buffers;
    gboolean flip[NUM_CHIPS];

    gchar *addr;
    guint ports[NUM_CHIPS];
    GSocket* sockets[NUM_CHIPS];

    guint8 readout_mode;
    guint8 acquisition_mode;

    gdouble exposure_time;
    guint acq_time_cycles;
};

static gboolean
xkit_send_recv_ackd (GSocket *socket, gchar *buffer, gsize size, GError **error)
{
    gchar ack;

    g_socket_send (socket, buffer, size, NULL, error);
    g_socket_receive (socket, &ack, 1, NULL, error);
    return ack == 0x21;
}

static void
xkit_recv_chunked (GSocket *socket, gchar *buffer, gsize size, GError **error)
{
    gsize total_read = 0;

    while (total_read < size) {
        gssize read;

        read = g_socket_receive_with_blocking (socket, buffer + total_read, size - total_read,
                                               FALSE, NULL, NULL);

        if (read > 0)
            total_read += read;
    }
}

static gboolean
xkit_reset (GSocket *socket, GError **error)
{
    gchar buffer = 0x32;
    return xkit_send_recv_ackd (socket, &buffer, 1, error);
}

static gboolean
xkit_set_exposure_time (GSocket *socket, guint32 exposure_time, GError **error)
{
    struct {
        guint8 msg;
        guint32 time;
    } __attribute__((packed))
    buffer = {
        .msg = 0x38,
        .time = exposure_time
    };

    return xkit_send_recv_ackd (socket, (gchar *) &buffer, sizeof (buffer), error);
}

static gboolean
xkit_start_acquisition (GSocket *socket, guint8 acquisition_mode, GError **error)
{
    gchar msg[] = { 0x34, 0x00, acquisition_mode };
    return xkit_send_recv_ackd (socket, msg, 3, error);
}

static gboolean
xkit_serial_matrix_readout (GSocket *socket, gchar *data, guint8 acquisition_mode, GError **error)
{
    gchar msg[] = { 0x35, acquisition_mode };
    gchar ack;

    g_socket_send (socket, msg, 2, NULL, error);
    xkit_recv_chunked (socket, data, MEDIPIX_SENSOR_SIZE * MEDIPIX_SENSOR_SIZE * 2, error);
    g_socket_receive (socket, &ack, 1, NULL, error);
    return ack == 0x21;
}

static gboolean
connect_xkit (UcaXkitCameraPrivate *priv)
{
    GInetAddress *local_addr;

    local_addr = g_inet_address_new_from_string (priv->addr);

    for (guint i = 0; i < NUM_CHIPS; i++) {
        GSocketAddress *socket_addr;

        socket_addr = g_inet_socket_address_new (local_addr, priv->ports[i]);

        priv->sockets[i] = g_socket_new (G_SOCKET_FAMILY_IPV4,
                                         G_SOCKET_TYPE_DATAGRAM,
                                         G_SOCKET_PROTOCOL_UDP,
                                         &priv->construct_error);

        if (priv->sockets[i] == NULL)
            return FALSE;

        if (!g_socket_connect (priv->sockets[i], socket_addr, NULL, &priv->construct_error))
            return FALSE;
    }

    return TRUE;
}

static gboolean
setup_xkit (UcaXkitCameraPrivate *priv)
{
    priv->n_max_chips = priv->n_chips = NUM_CHIPS;
    priv->buffers = g_new0 (guint16 *, priv->n_max_chips);

    for (guint i = 0; i < priv->n_max_chips; i++)
        priv->buffers[i] = (guint16 *) g_malloc0 (MEDIPIX_SENSOR_SIZE * MEDIPIX_SENSOR_SIZE * sizeof(guint16));

    return connect_xkit (priv);
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
    UcaCameraTrigger trigger_mode;

    g_return_if_fail (UCA_IS_XKIT_CAMERA (camera));

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (camera);
    g_object_get (camera, "trigger-mode", &trigger_mode, NULL);

    if (trigger_mode == UCA_CAMERA_TRIGGER_AUTO)
        priv->acquisition_mode = X_KIT_ACQUIRE_NORMAL;
    else if (trigger_mode == UCA_CAMERA_TRIGGER_SOFTWARE)
        priv->acquisition_mode = X_KIT_ACQUIRE_NORMAL;
    else if (trigger_mode == UCA_CAMERA_TRIGGER_EXTERNAL)
        priv->acquisition_mode = X_KIT_ACQUIRE_TRIGGERED;

    priv->acquisition_mode |= priv->readout_mode;

    if (!xkit_reset (priv->sockets[0], error))
        return;

    if (!xkit_set_exposure_time (priv->sockets[0], 0x1000, error))
        return;

    xkit_start_acquisition (priv->sockets[0], priv->acquisition_mode, error);
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
    GTimer *timer;


    priv = UCA_XKIT_CAMERA_GET_PRIVATE (camera);
    timer = g_timer_new ();

    #pragma omp parallel for num_threads(NUM_CHIPS)
    for (guint i = 0; i < NUM_CHIPS; i++) {
        GError *local_error = NULL;

        xkit_serial_matrix_readout (priv->sockets[i],
                                    (gchar *) priv->buffers[i],
                                    priv->acquisition_mode,
                                    &local_error);

        if (local_error != NULL) {
            g_error_free (local_error);
            local_error = NULL;
        }
    }

    compose_image ((guint16 *) data, priv->buffers, priv->flip, priv->n_chips);
    g_timer_destroy (timer);

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
    UcaXkitCameraPrivate *priv;

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_EXPOSURE_TIME:
            priv->exposure_time = g_value_get_double(value);
            break;
        case PROP_FLIP_CHIP_0:
        case PROP_FLIP_CHIP_1:
        case PROP_FLIP_CHIP_2:
        /* case PROP_FLIP_CHIP_3: */
        /* case PROP_FLIP_CHIP_4: */
        /* case PROP_FLIP_CHIP_5: */
            priv->flip[property_id - PROP_FLIP_CHIP_0] = g_value_get_boolean (value);
            break;

        case PROP_READOUT_BIG_ENDIAN:
            if (g_value_get_boolean (value))
                priv->readout_mode |= X_KIT_READOUT_BIG_ENDIAN;
            else
                priv->readout_mode &= ~X_KIT_READOUT_BIG_ENDIAN;
            break;

        case PROP_READOUT_MIRRORED:
            if (g_value_get_boolean (value))
                priv->readout_mode |= X_KIT_READOUT_MIRRORED;
            else
                priv->readout_mode &= ~X_KIT_READOUT_MIRRORED;
            break;

        case PROP_READOUT_INCREMENTAL_GREYSCALE:
            if (g_value_get_boolean (value))
                priv->readout_mode |= X_KIT_READOUT_INCREMENTAL_GREYSCALE;
            else
                priv->readout_mode &= ~X_KIT_READOUT_INCREMENTAL_GREYSCALE;
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
        case PROP_SENSOR_WIDTH:
            g_value_set_uint (value, CHIPS_PER_ROW * MEDIPIX_SENSOR_SIZE);
            break;
        case PROP_SENSOR_HEIGHT:
            g_value_set_uint (value, CHIPS_PER_COLUMN * MEDIPIX_SENSOR_SIZE);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint (value, 14);
            break;
        case PROP_EXPOSURE_TIME:
            g_value_set_double (value, priv->exposure_time);
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
        case PROP_ADDR:
            g_value_set_string (value, priv->addr);
            break;
        case PROP_FLIP_CHIP_0:
        case PROP_FLIP_CHIP_1:
        case PROP_FLIP_CHIP_2:
        /* case PROP_FLIP_CHIP_3: */
        /* case PROP_FLIP_CHIP_4: */
        /* case PROP_FLIP_CHIP_5: */
            g_value_set_boolean (value, priv->flip[property_id - PROP_FLIP_CHIP_0]);
            break;
        case PROP_PORT_CHIP_0:
        case PROP_PORT_CHIP_1:
        case PROP_PORT_CHIP_2:
        /* case PROP_FLIP_CHIP_3: */
        /* case PROP_FLIP_CHIP_4: */
        /* case PROP_FLIP_CHIP_5: */
            g_value_set_uint (value, priv->ports[property_id - PROP_PORT_CHIP_0]);
            break;
        case PROP_READOUT_BIG_ENDIAN:
            g_value_set_boolean (value, priv->readout_mode & X_KIT_READOUT_BIG_ENDIAN);
            break;
        case PROP_READOUT_MIRRORED:
            g_value_set_boolean (value, priv->readout_mode & X_KIT_READOUT_MIRRORED);
            break;
        case PROP_READOUT_INCREMENTAL_GREYSCALE:
            g_value_set_boolean (value, priv->readout_mode & X_KIT_READOUT_INCREMENTAL_GREYSCALE);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
uca_xkit_camera_dispose (GObject *object)
{
    UcaXkitCameraPrivate *priv;

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (object);

    for (guint i = 0; i < NUM_CHIPS; i++) {
        if (priv->sockets[i] != NULL) {
            g_object_unref (priv->sockets[i]);
            priv->sockets[i] = NULL;
        }
    }

    G_OBJECT_CLASS (uca_xkit_camera_parent_class)->dispose (object);
}

static void
uca_xkit_camera_finalize (GObject *object)
{
    UcaXkitCameraPrivate *priv;

    priv = UCA_XKIT_CAMERA_GET_PRIVATE (object);
    g_clear_error (&priv->construct_error);

    for (guint i = 0; i < priv->n_max_chips; i++)
        g_free (priv->buffers[i]);

    g_free (priv->buffers);
    g_free (priv->addr);

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
    oclass->dispose = uca_xkit_camera_dispose;
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

    xkit_properties[PROP_ADDR] =
        g_param_spec_string ("address",
                             "Address of board",
                             "Address of board",
                             "192.168.1.115",
                             G_PARAM_READWRITE);

    xkit_properties[PROP_READOUT_BIG_ENDIAN] =
        g_param_spec_boolean("big-endian",
            "Big Endian Readout",
            "Big Endian Readout",
            FALSE,
            (GParamFlags) G_PARAM_READWRITE);

    xkit_properties[PROP_READOUT_MIRRORED] =
        g_param_spec_boolean("mirrored",
            "Mirrored Readout",
            "Mirrored Readout",
            FALSE,
            (GParamFlags) G_PARAM_READWRITE);

    xkit_properties[PROP_READOUT_INCREMENTAL_GREYSCALE] =
        g_param_spec_boolean("incremental-greyscale",
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

        prop_name = g_strdup_printf ("port-chip-%i", i);

        xkit_properties[PROP_PORT_CHIP_0 + i] =
            g_param_spec_uint (prop_name, "Connection port", "Connection port", 0, 65535, 5002 + i, (GParamFlags) G_PARAM_READWRITE);

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
    priv->exposure_time = 0.5;
    priv->addr = g_strdup (XKIT_DEFAULT_IP);
    priv->construct_error = NULL;

    for (guint i = 0; i < NUM_CHIPS; i++) {
        priv->sockets[i] = NULL;
        priv->flip[i] = FALSE;
        priv->ports[i] = XKIT_DEFAULT_PORT + i;
    }

    if (!setup_xkit (priv))
        return;
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_XKIT_CAMERA;
}
