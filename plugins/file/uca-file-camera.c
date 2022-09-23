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
#include <tiffio.h>
#include "uca-file-camera.h"

#define UCA_FILE_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_FILE_CAMERA, UcaFileCameraPrivate))

static void uca_file_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaFileCamera, uca_file_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_file_initable_iface_init))

enum {
    PROP_PATH = N_BASE_PROPERTIES,
    N_PROPERTIES
};

static const gint file_overrideables[] = {
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

static GParamSpec *file_properties[N_PROPERTIES] = { NULL, };

struct _UcaFileCameraPrivate {
    gchar *path;
    guint width;
    guint height;
    guint bitdepth;
    GList *fnames;
    GList *current;
};

static gboolean
read_tiff_meta_data (UcaFileCameraPrivate *priv, const gchar *fname)
{
    TIFF *file;

    file = TIFFOpen (fname, "r");
    if (!file) {
        return FALSE;
    }

    TIFFGetField (file, TIFFTAG_BITSPERSAMPLE, &priv->bitdepth);
    TIFFGetField (file, TIFFTAG_IMAGEWIDTH, &priv->width);
    TIFFGetField (file, TIFFTAG_IMAGELENGTH, &priv->height);

    TIFFClose (file);

    return TRUE;
}

static gboolean
read_tiff_data (UcaFileCameraPrivate *priv, const gchar *fname, gpointer buffer)
{
    TIFF *file;
    guint16 bitdepth;
    guint width;
    guint height;
    tsize_t result;
    int offset = 0;
    int step = priv->width;

    file = TIFFOpen (fname, "r");
    if (!file) {
        return FALSE;
    }

    TIFFGetField (file, TIFFTAG_BITSPERSAMPLE, &bitdepth);
    TIFFGetField (file, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField (file, TIFFTAG_IMAGELENGTH, &height);

    if (priv->bitdepth != bitdepth || priv->width != width || priv->height != height) {
        g_warning ("Data format not compatible: %ux%u@%u [expected %ux%u@%u]",
                   width, height, bitdepth, priv->width, priv->height, priv->bitdepth);
        TIFFClose (file);
        return FALSE;
    }

    step *= priv->bitdepth / 8;

    for (guint32 i = 0; i < priv->height; i++) {
        result = TIFFReadScanline (file, ((gchar *) buffer) + offset, i, 0);

        if (result == -1)
            return FALSE;

        offset += step;
    }

    TIFFClose (file);
    return TRUE;
}

static gboolean
update_fnames (UcaFileCameraPrivate *priv)
{
    GDir *dir;
    const gchar *fname;
    GError *error = NULL;

    g_list_free_full (priv->fnames, g_free);
    priv->fnames = NULL;

    dir = g_dir_open (priv->path, 0, &error);

    if (dir == NULL) {
        g_warning ("%s", error->message);
        return FALSE;
    }

    while (1) {
        fname = g_dir_read_name (dir);

        if (fname == NULL)
            break;

        if (g_str_has_suffix (fname, ".tiff") || g_str_has_suffix (fname, ".tif"))
            priv->fnames = g_list_append (priv->fnames, g_build_filename (priv->path, fname, NULL));
    }

    priv->fnames = g_list_sort (priv->fnames, (GCompareFunc) g_strcmp0);
    priv->current = priv->fnames;

    if (priv->current != NULL) {
        while (TRUE) {
            fname = (const gchar *) priv->current->data;
            if (read_tiff_meta_data (priv, fname)) {
                break;
            } else {
                g_warning ("Cannot read %s", fname);
            }
            priv->current = g_list_next (priv->current);
            if (!priv->current) {
                g_error ("No valid tif files found");
            }
        }
    }

    g_dir_close (dir);
    return TRUE;
}

static void
uca_file_camera_start_recording(UcaCamera *camera, GError **error)
{
    UcaFileCameraPrivate *priv;
    g_return_if_fail (UCA_IS_FILE_CAMERA (camera));

    priv = UCA_FILE_CAMERA_GET_PRIVATE (camera);

    if (priv->fnames == NULL) {
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_END_OF_STREAM,
                     "No files found");
    }

    priv->current = priv->fnames;
}

static void
uca_file_camera_stop_recording(UcaCamera *camera, GError **error)
{
}

static void
uca_file_camera_trigger (UcaCamera *camera, GError **error)
{
}

static gboolean
uca_file_camera_grab (UcaCamera *camera, gpointer data, GError **error)
{
    UcaFileCameraPrivate *priv;
    g_return_val_if_fail (UCA_IS_FILE_CAMERA (camera), FALSE);

    priv = UCA_FILE_CAMERA_GET_PRIVATE (camera);

    if (priv->current == NULL) {
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_END_OF_STREAM,
                     "End of stream");
        return FALSE;
    }

    if (!read_tiff_data (priv, (const gchar *) priv->current->data, data)) {
        g_set_error (error, UCA_CAMERA_ERROR, UCA_CAMERA_ERROR_END_OF_STREAM,
                     "Error reading file");
        return FALSE;
    }

    priv->current = g_list_next (priv->current);
    return TRUE;
}

static void
uca_file_camera_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail(UCA_IS_FILE_CAMERA(object));
    UcaFileCameraPrivate *priv = UCA_FILE_CAMERA_GET_PRIVATE(object);

    if (uca_camera_is_recording (UCA_CAMERA(object)) &&
        !uca_camera_is_writable_during_acquisition (UCA_CAMERA (object), pspec->name)) {
        g_warning ("Property '%s' cant be changed during acquisition", pspec->name);
        return;
    }

    switch (property_id) {
        case PROP_PATH:
            g_free (priv->path);
            priv->path = g_strdup (g_value_get_string (value));
            priv->path = g_strstrip (priv->path);
            update_fnames (priv);

            g_object_notify (object, "roi-width");
            g_object_notify (object, "roi-height");
            g_object_notify (object, "sensor-bitdepth");
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }
}

static void
uca_file_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (UCA_IS_FILE_CAMERA (object));
    UcaFileCameraPrivate *priv = UCA_FILE_CAMERA_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NAME:
            g_value_set_string (value, "file camera");
            break;
        case PROP_SENSOR_WIDTH:
            g_value_set_uint (value, priv->width);
            break;
        case PROP_SENSOR_HEIGHT:
            g_value_set_uint (value, priv->height);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint (value, priv->bitdepth);
            break;
        case PROP_ROI_X:
            g_value_set_uint (value, 0);
            break;
        case PROP_ROI_Y:
            g_value_set_uint (value, 0);
            break;
        case PROP_ROI_WIDTH:
            g_value_set_uint (value, priv->width);
            break;
        case PROP_ROI_HEIGHT:
            g_value_set_uint (value, priv->height);
            break;
        case PROP_EXPOSURE_TIME:
            g_value_set_double (value, 0.1);
            break;
        case PROP_HAS_STREAMING:
            g_value_set_boolean (value, TRUE);
            break;
        case PROP_HAS_CAMRAM_RECORDING:
            g_value_set_boolean (value, FALSE);
            break;
        case PROP_PATH:
            g_value_set_string (value, priv->path);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
uca_file_camera_finalize(GObject *object)
{
    UcaFileCameraPrivate *priv;

    priv = UCA_FILE_CAMERA_GET_PRIVATE(object);

    g_list_free_full (priv->fnames, g_free);
    priv->fnames = NULL;

    G_OBJECT_CLASS(uca_file_camera_parent_class)->finalize(object);
}

static gboolean
ufo_file_camera_initable_init (GInitable *initable,
                               GCancellable *cancellable,
                               GError **error)
{
    g_return_val_if_fail (UCA_IS_FILE_CAMERA (initable), FALSE);
    return TRUE;
}

static void
uca_file_initable_iface_init (GInitableIface *iface)
{
    iface->init = ufo_file_camera_initable_init;
}

static void
uca_file_camera_class_init(UcaFileCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->set_property = uca_file_camera_set_property;
    gobject_class->get_property = uca_file_camera_get_property;
    gobject_class->finalize = uca_file_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS (klass);
    camera_class->start_recording = uca_file_camera_start_recording;
    camera_class->stop_recording = uca_file_camera_stop_recording;
    camera_class->grab = uca_file_camera_grab;
    camera_class->trigger = uca_file_camera_trigger;

    for (guint i = 0; file_overrideables[i] != 0; i++)
        g_object_class_override_property (gobject_class, file_overrideables[i], uca_camera_props[file_overrideables[i]]);

    file_properties[PROP_PATH] =
        g_param_spec_string ("path",
                "Path to directory containing TIFF files",
                "Path to directory containing TIFF files",
                ".",
                G_PARAM_READWRITE);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property (gobject_class, id, file_properties[id]);

    g_type_class_add_private (klass, sizeof(UcaFileCameraPrivate));
}

static void
uca_file_camera_init(UcaFileCamera *self)
{
    UcaFileCameraPrivate *priv;

    self->priv = priv = UCA_FILE_CAMERA_GET_PRIVATE(self);
    priv->path = g_strdup (".");
    priv->width = 512;
    priv->height = 512;
    priv->bitdepth = 8;

    priv->fnames = NULL;
    update_fnames (priv);
}

G_MODULE_EXPORT GType
camera_plugin_get_type (void)
{
    return UCA_TYPE_FILE_CAMERA;
}
