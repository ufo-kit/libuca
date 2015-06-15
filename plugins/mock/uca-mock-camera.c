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
#include "uca-mock-camera.h"

#define UCA_MOCK_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_MOCK_CAMERA, UcaMockCameraPrivate))

static void uca_mock_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaMockCamera, uca_mock_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_mock_initable_iface_init))

enum {
    PROP_CREATE_RANDOM = N_BASE_PROPERTIES,
    N_PROPERTIES
};

static const gint mock_overrideables[] = {
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

static GParamSpec *mock_properties[N_PROPERTIES] = { NULL, };

struct _UcaMockCameraPrivate {
    guint width;
    guint height;
    guint bits;
    guint bytes;
    guint max_val;
    guint roi_x, roi_y, roi_width, roi_height;
    gfloat max_frame_rate;
    gdouble exposure_time;
    guint8 *dummy_data;
    guint current_frame;
    guint readout_index;
    gboolean create_random;
    GRand *rand;

    guint8 *default_line;

    gboolean thread_running;

    GThread *grab_thread;
    GValueArray *binnings;
};

static const char g_digits[16][20] = {
    /* 0 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 1 */
    { 0x00, 0x00, 0xff, 0x00,
      0x00, 0xff, 0xff, 0x00,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0x00, 0xff, 0x00 },
    /* 2 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0xff, 0x00, 0x00,
      0xff, 0xff, 0xff, 0xff },
    /* 3 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0x00, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 4 */
    { 0xff, 0x00, 0x00, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0xff,
      0x00, 0x00, 0x00, 0xff },
    /* 5 */
    { 0xff, 0xff, 0xff, 0xff,
      0xff, 0x00, 0x00, 0x00,
      0x00, 0xff, 0xff, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0x00 },
    /* 6 */
    { 0x00, 0xff, 0xff, 0xff,
      0xff, 0x00, 0x00, 0x00,
      0xff, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 7 */
    { 0xff, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0xff,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0xff, 0x00, 0x00,
      0xff, 0x00, 0x00, 0x00 },
    /* 8 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 9 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0x00 },
    /* A */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0xff,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0x00, 0x00, 0xff },
    /* B */
    { 0xff, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0x00 },
    /* C */
    { 0x00, 0xff, 0xff, 0xff,
      0xff, 0x00, 0x00, 0x00,
      0xff, 0x00, 0x00, 0x00,
      0xff, 0x00, 0x00, 0x00,
      0x00, 0xff, 0xff, 0xff },
    /* D */
    { 0xff, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0x00 },
    /* E */
    { 0xff, 0xff, 0xff, 0xff,
      0xff, 0x00, 0x00, 0x00,
      0xff, 0xff, 0xff, 0xff,
      0xff, 0x00, 0x00, 0x00,
      0xff, 0xff, 0xff, 0xff },
    /* F */
    { 0xff, 0xff, 0xff, 0xff,
      0xff, 0x00, 0x00, 0x00,
      0xff, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0x00,
      0xff, 0x00, 0x00, 0x00 }
};

static const guint DIGIT_WIDTH = 4;
static const guint DIGIT_HEIGHT = 5;


static inline void
set_pixel (guint8 *buffer, guint x, guint y, guint value, guint n_bytes, guint mask, guint width)
{
    gulong offset = ((y * width) + x) * n_bytes;
    guint val = (value & mask);

    for (guint i = 0; i < n_bytes; i++) {
        buffer[offset + i] = (val >> (i * 8)) & 0xFF;
    }
}

static void
print_number (guint8 *buffer, guint number, guint x, guint y, guint n_bytes, guint mask, guint width)
{
    for (int i = 0; i < DIGIT_WIDTH; i++) {
        for (int j = 0; j < DIGIT_HEIGHT; j++) {
            guint val = g_digits[number][j*DIGIT_WIDTH+i];
            if (val > 0)
                val = mask;
            set_pixel (buffer, x+i, y+j, val, n_bytes, mask, width);
        }
    }
}

static void
print_current_frame (UcaMockCameraPrivate *priv, guint8 *buffer, gboolean prefix)
{
    guint divisor = 10000000;
    int x = 2;
    guint number = priv->current_frame;

    memset(buffer, 0, 15 * priv->roi_width * priv->bytes);

    if (prefix) {
        number = priv->readout_index;
        print_number(buffer, 11, x, 1, priv->bytes, priv->max_val, priv->roi_width);
        divisor = divisor / 10;
        x += DIGIT_WIDTH + 1;
    }

    while (divisor > 0) {
        /* max_val doubles as a bit mask */
        print_number(buffer, number / divisor, x, 1, priv->bytes, priv->max_val, priv->roi_width);
        number = number % divisor;
        divisor = divisor / 10;
        x += DIGIT_WIDTH + 1;
    }

    for (guint y = 16; y < priv->roi_height; y++) {
        guint index = y * priv->roi_width * priv->bytes;
        memcpy (&buffer[index], priv->default_line + (priv->roi_x * priv->width), priv->roi_width * priv->bytes);
    }

    if (priv->create_random) {
        //This block will fill a square at the center of the ROI with noraml
        //distributed random data
        const double mean = (double) ceil (priv->max_val / 2.);
        const double std = (double) ceil (priv->max_val / 8.);

        for (guint y = (priv->roi_height / 3); y < ((priv->roi_height * 2) / 3); y++) {
            for (guint x = (priv->roi_width / 3); x < ((priv->roi_width * 2) / 3); x++) {
                double u1 = g_rand_double (priv->rand);
                double u2 = g_rand_double (priv->rand);
                double r = sqrt (-2 * log(u1)) * cos(2 * G_PI * u2);
                set_pixel (buffer, x, y, round (r * std + mean), priv->bytes, priv->max_val, priv->roi_width);
            }
        }
    }
}

static gpointer
mock_grab_func(gpointer data)
{
    UcaMockCamera *mock_camera = UCA_MOCK_CAMERA(data);
    g_return_val_if_fail(UCA_IS_MOCK_CAMERA(mock_camera), NULL);

    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(mock_camera);
    UcaCamera *camera = UCA_CAMERA(mock_camera);
    gdouble fps = 0;
    g_object_get (G_OBJECT (data), "frames-per-second", &fps, NULL);
    const gulong sleep_time = (gulong) G_USEC_PER_SEC / fps;

    while (priv->thread_running) {
        camera->grab_func(priv->dummy_data, camera->user_data);
        g_usleep(sleep_time);
    }

    return NULL;
}

static void
uca_mock_camera_start_recording(UcaCamera *camera, GError **error)
{
    gboolean transfer_async = FALSE;
    UcaMockCameraPrivate *priv;
    g_return_if_fail(UCA_IS_MOCK_CAMERA(camera));

    priv = UCA_MOCK_CAMERA_GET_PRIVATE(camera);
    /* TODO: check that roi_x + roi_width < priv->width */
    priv->dummy_data = (guint8 *) g_malloc0(priv->roi_width * priv->roi_height * priv->bytes);

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
        priv->grab_thread = g_thread_create(mock_grab_func, camera, TRUE, &tmp_error);

        if (tmp_error != NULL) {
            priv->thread_running = FALSE;
            g_propagate_error(error, tmp_error);
        }
    }
}

static void
uca_mock_camera_stop_recording(UcaCamera *camera, GError **error)
{
    gboolean transfer_async = FALSE;
    UcaMockCameraPrivate *priv;
    g_return_if_fail(UCA_IS_MOCK_CAMERA(camera));

    priv = UCA_MOCK_CAMERA_GET_PRIVATE(camera);
    g_free(priv->dummy_data);
    priv->dummy_data = NULL;

    g_object_get(G_OBJECT(camera),
            "transfer-asynchronously", &transfer_async,
            NULL);

    if (transfer_async) {
        priv->thread_running = FALSE;
        g_thread_join(priv->grab_thread);
    }
}

static void
uca_mock_camera_trigger (UcaCamera *camera, GError **error)
{
}

static gboolean
uca_mock_camera_grab (UcaCamera *camera, gpointer data, GError **error)
{
    gdouble exposure_time;

    g_return_val_if_fail (UCA_IS_MOCK_CAMERA(camera), FALSE);

    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE (camera);

    g_object_get (G_OBJECT (camera), "exposure-time", &exposure_time, NULL);
    g_usleep (G_USEC_PER_SEC * exposure_time);

    print_current_frame (priv, priv->dummy_data, FALSE);
    priv->current_frame++;

    g_memmove (data, priv->dummy_data, priv->roi_width * priv->roi_height * priv->bytes);

    return TRUE;
}

static gboolean
uca_mock_camera_readout (UcaCamera *camera, gpointer data, guint index, GError **error)
{
    g_return_val_if_fail (UCA_IS_MOCK_CAMERA(camera), FALSE);

    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE (camera);

    priv->readout_index = index;
    print_current_frame (priv, priv->dummy_data, TRUE);

    g_memmove (data, priv->dummy_data, priv->roi_width * priv->roi_height * priv->bytes);

    return TRUE;
}

static void
uca_mock_camera_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (UCA_IS_MOCK_CAMERA (object));
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE (object);

    if (uca_camera_is_recording (UCA_CAMERA (object)) && !uca_camera_is_writable_during_acquisition (UCA_CAMERA (object), pspec->name)) {
        g_warning ("Property '%s' cant be changed during acquisition", pspec->name);
        return;
    }

    switch (property_id) {
        case PROP_SENSOR_WIDTH:
            priv->width = g_value_get_uint (value);
            break;
        case PROP_SENSOR_HEIGHT:
            priv->height = g_value_get_uint (value);
            break;
        case PROP_SENSOR_BITDEPTH:
            priv->bits = g_value_get_uint (value);
            break;
        case PROP_EXPOSURE_TIME:
            priv->exposure_time = g_value_get_double (value);
            break;
        case PROP_ROI_X:
            priv->roi_x = g_value_get_uint (value);
            break;
        case PROP_ROI_Y:
            priv->roi_y = g_value_get_uint (value);
            break;
        case PROP_ROI_WIDTH:
            priv->roi_width = g_value_get_uint (value);
            break;
        case PROP_ROI_HEIGHT:
            priv->roi_height = g_value_get_uint (value);
            break;
        case PROP_CREATE_RANDOM:
            priv->create_random = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            return;
    }
}

static void
uca_mock_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(object);

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
            g_value_set_uint(value, priv->bits);
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
        case PROP_HAS_STREAMING:
            g_value_set_boolean(value, TRUE);
            break;
        case PROP_HAS_CAMRAM_RECORDING:
            g_value_set_boolean(value, FALSE);
            break;
        case PROP_CREATE_RANDOM:
            g_value_set_boolean(value, priv->create_random);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
uca_mock_camera_finalize(GObject *object)
{
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(object);

    g_rand_free (priv->rand);

    if (priv->thread_running) {
        priv->thread_running = FALSE;
        g_thread_join (priv->grab_thread);
    }

    g_free (priv->dummy_data);
    g_value_array_free (priv->binnings);

    g_free (priv->default_line);

    G_OBJECT_CLASS (uca_mock_camera_parent_class)->finalize(object);
}

static gboolean
ufo_mock_camera_initable_init (GInitable *initable,
                               GCancellable *cancellable,
                               GError **error)
{
    g_return_val_if_fail (UCA_IS_MOCK_CAMERA (initable), FALSE);
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE (UCA_MOCK_CAMERA (initable));

    g_object_get (G_OBJECT (initable),
                "sensor-width", &priv->width,
                "sensor-height", &priv->height,
                "sensor-bitdepth", &priv->bits,
                NULL);

    priv->roi_width = priv->width;
    priv->roi_height = priv->height;
    priv->bytes = ceil (priv->bits / 8.);

    priv->max_val = 0;
    for (guint i = 0; i < priv->bits; i++) {
        priv->max_val |= 1 << i;
    }

    priv->default_line = g_malloc0 (priv->width * priv->bytes);
    gdouble step_size = ((gdouble) priv->max_val) / priv->width;
    for (guint p = 0; p < priv->roi_width; p++) {
        guint val = round (p * step_size);
        set_pixel (priv->default_line, p, 0, val, priv->bytes, priv->max_val, priv->width);
    }

    return TRUE;
}

static void
uca_mock_initable_iface_init (GInitableIface *iface)
{
    iface->init = ufo_mock_camera_initable_init;
}

static void
uca_mock_camera_class_init(UcaMockCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_mock_camera_set_property;
    gobject_class->get_property = uca_mock_camera_get_property;
    gobject_class->finalize = uca_mock_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_mock_camera_start_recording;
    camera_class->stop_recording = uca_mock_camera_stop_recording;
    camera_class->grab = uca_mock_camera_grab;
    camera_class->readout = uca_mock_camera_readout;
    camera_class->trigger = uca_mock_camera_trigger;

    for (guint i = 0; mock_overrideables[i] != 0; i++)
        g_object_class_override_property(gobject_class, mock_overrideables[i], uca_camera_props[mock_overrideables[i]]);

    mock_properties[PROP_CREATE_RANDOM] =
        g_param_spec_boolean("create-random",
            "Add random data to picture",
            "If TRUE, fills a rectangle in the center of the picture with random data",
            TRUE,
            G_PARAM_READWRITE);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property(gobject_class, id, mock_properties[id]);

    uca_camera_pspec_set_writable (g_object_class_find_property (gobject_class, uca_camera_props[PROP_EXPOSURE_TIME]), TRUE);
    uca_camera_pspec_set_writable (mock_properties[PROP_CREATE_RANDOM], TRUE);

    g_type_class_add_private(klass, sizeof(UcaMockCameraPrivate));
}

static void
uca_mock_camera_init(UcaMockCamera *self)
{
    self->priv = UCA_MOCK_CAMERA_GET_PRIVATE(self);
    self->priv->roi_x = 0;
    self->priv->roi_y = 0;
    self->priv->max_frame_rate = 100000.0f;
    self->priv->grab_thread = NULL;
    self->priv->current_frame = 0;
    self->priv->exposure_time = 0.05;
    self->priv->create_random = TRUE;

    self->priv->binnings = g_value_array_new(1);
    self->priv->rand = g_rand_new ();

    GValue val = {0};
    g_value_init(&val, G_TYPE_UINT);
    g_value_set_uint(&val, 1);
    g_value_array_append(self->priv->binnings, &val);


    /* will be set in initable_init */
    self->priv->width = self->priv->roi_width = 0;
    self->priv->height = self->priv->roi_height = 0;
    self->priv->bits = 0;
    self->priv->bytes = 0;
    self->priv->max_val = 0;
    self->priv->default_line = NULL;
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_MOCK_CAMERA;
}
