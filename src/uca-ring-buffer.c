/* Copyright (C) 2013 Matthias Vogelgesang <matthias.vogelgesang@kit.edu>
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

#include <math.h>
#include "uca-ring-buffer.h"

#define UCA_RING_BUFFER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_RING_BUFFER, UcaRingBufferPrivate))

G_DEFINE_TYPE(UcaRingBuffer, uca_ring_buffer, G_TYPE_OBJECT)

struct _UcaRingBufferPrivate {
    guchar  *data;
    gsize    block_size;
    guint    n_blocks_total;
    guint    write_index;
    guint    read_index;
    guint    read;
    guint    written;
    gboolean full;
};

enum {
    PROP_0,
    PROP_BLOCK_SIZE,
    PROP_NUM_BLOCKS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UcaRingBuffer *
uca_ring_buffer_new (gsize block_size,
                     guint n_blocks)
{
    UcaRingBuffer *buffer;

    buffer = g_object_new (UCA_TYPE_RING_BUFFER,
                           "block-size", (guint64) block_size,
                           "num-blocks", n_blocks,
                           NULL);
    return buffer;
}

void
uca_ring_buffer_reset (UcaRingBuffer *buffer)
{
    g_return_if_fail (UCA_IS_RING_BUFFER (buffer));

    buffer->priv->write_index = 0;
    buffer->priv->read_index = 0;
    buffer->priv->full = FALSE;
}

gsize
uca_ring_buffer_get_block_size (UcaRingBuffer *buffer)
{
    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), 0);
    return buffer->priv->block_size;
}

gboolean
uca_ring_buffer_available (UcaRingBuffer *buffer)
{
    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), FALSE);
    return buffer->priv->read_index < buffer->priv->write_index;
}

/**
 * uca_ring_buffer_get_current_pointer:
 * @buffer: A #UcaRingBuffer object
 *
 * Get a pointer to the data for the block that is currently in use, that means
 * the number uca_ring_buffer_proceed() has been called modulo the number of
 * total blocks.
 *
 * Return value: (transfer none): Pointer to data block
 */
gpointer
uca_ring_buffer_get_read_pointer (UcaRingBuffer *buffer)
{
    UcaRingBufferPrivate *priv;
    gpointer data;

    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), NULL);
    priv = buffer->priv;
    data = priv->data + (priv->read_index % priv->n_blocks_total) * priv->block_size;
    priv->read_index++;
    return data;
}

gpointer
uca_ring_buffer_get_write_pointer (UcaRingBuffer *buffer)
{
    UcaRingBufferPrivate *priv;
    gpointer data;

    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), NULL);

    priv = buffer->priv;
    data = priv->data + (priv->write_index % priv->n_blocks_total) * priv->block_size;
    priv->write_index++;

    if ((priv->write_index - priv->read_index) == priv->n_blocks_total)
        priv->full = TRUE;

    return data;
}

gpointer
uca_ring_buffer_peek_pointer (UcaRingBuffer *buffer)
{
    UcaRingBufferPrivate *priv;
    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), NULL);
    priv = buffer->priv;
    return ((guint8 *) priv->data) + ((priv->write_index % priv->n_blocks_total) * priv->block_size);
}

gpointer
uca_ring_buffer_get_pointer (UcaRingBuffer *buffer,
                             guint          index)
{
    UcaRingBufferPrivate *priv;
    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), NULL);
    priv = buffer->priv;
    return ((guint8 *) priv->data) + (((priv->read_index + index) % priv->n_blocks_total) * priv->block_size);
}

guint
uca_ring_buffer_get_num_blocks (UcaRingBuffer *buffer)
{
    UcaRingBufferPrivate *priv;

    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), 0);
    priv = buffer->priv;
    return priv->write_index < priv->n_blocks_total ? priv->write_index : priv->n_blocks_total;
}

static void
realloc_mem (UcaRingBufferPrivate *priv)
{
    if (priv->data != NULL)
        g_free (priv->data);

    priv->data = g_malloc0_n (priv->n_blocks_total, priv->block_size);
}

static void
uca_ring_buffer_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UcaRingBufferPrivate *priv;

    g_return_if_fail (UCA_IS_RING_BUFFER (object));
    priv = UCA_RING_BUFFER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_BLOCK_SIZE:
            g_value_set_uint64 (value, (guint64) priv->block_size);
            break;

        case PROP_NUM_BLOCKS:
            g_value_set_uint (value, priv->n_blocks_total);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
uca_ring_buffer_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UcaRingBufferPrivate *priv;

    g_return_if_fail (UCA_IS_RING_BUFFER (object));
    priv = UCA_RING_BUFFER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_BLOCK_SIZE:
            priv->block_size = (gsize) g_value_get_uint64 (value);
            realloc_mem (priv);
            break;

        case PROP_NUM_BLOCKS:
            priv->n_blocks_total = g_value_get_uint (value);
            realloc_mem (priv);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
uca_ring_buffer_dispose (GObject *object)
{
    G_OBJECT_CLASS (uca_ring_buffer_parent_class)->dispose (object);
}

static void
uca_ring_buffer_finalize (GObject *object)
{
    UcaRingBufferPrivate *priv;

    priv = UCA_RING_BUFFER_GET_PRIVATE (object);
    g_free (priv->data);
    priv->data = NULL;
    G_OBJECT_CLASS (uca_ring_buffer_parent_class)->finalize (object);
}

static void
uca_ring_buffer_class_init (UcaRingBufferClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);

    oclass->get_property = uca_ring_buffer_get_property;
    oclass->set_property = uca_ring_buffer_set_property;
    oclass->dispose = uca_ring_buffer_dispose;
    oclass->finalize = uca_ring_buffer_finalize;

    properties[PROP_BLOCK_SIZE] =
        g_param_spec_uint64 ("block-size",
                             "Block size in bytes",
                             "Number of bytes per block",
                             0, G_MAXUINT64, 0,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    properties[PROP_NUM_BLOCKS] =
        g_param_spec_uint ("num-blocks",
                           "Number of pre-allocated blocks",
                           "Number of pre-allocated blocks",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (klass, sizeof (UcaRingBufferPrivate));
}

static void
uca_ring_buffer_init (UcaRingBuffer *buffer)
{
    UcaRingBufferPrivate *priv;
    priv = buffer->priv = UCA_RING_BUFFER_GET_PRIVATE (buffer);

    priv->n_blocks_total = 0;
    priv->block_size = 0;
    priv->data = NULL;
}
