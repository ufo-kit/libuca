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

#define UCA_RING_BUFFER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_RING_BUFFER, UcaRingBuffer))

struct _UcaRingBuffer {
    GObject parent_instance;
    guchar  *data;
    gsize    block_size;
    guint    n_blocks_total;
    guint    write_index;
    guint    read_index;
    guint    read;
    guint    written;
};

G_DEFINE_FINAL_TYPE (UcaRingBuffer, uca_ring_buffer, G_TYPE_OBJECT)

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
    return g_object_new (UCA_TYPE_RING_BUFFER,
                           "block-size", (guint64) block_size,
                           "num-blocks", n_blocks,
                           NULL);
}

void
uca_ring_buffer_reset (UcaRingBuffer *buffer)
{
    g_return_if_fail (UCA_IS_RING_BUFFER (buffer));

    buffer->write_index = 0;
    buffer->read_index = 0;
}

gsize
uca_ring_buffer_get_block_size (UcaRingBuffer *buffer)
{
    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), 0);
    return buffer->block_size;
}

gboolean
uca_ring_buffer_available (UcaRingBuffer *buffer)
{
    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), FALSE);
    return buffer->read_index < buffer->write_index;
}

/**
 * uca_ring_buffer_get_read_pointer:
 * @buffer: A #UcaRingBuffer object
 *
 * Get pointer to current read location. If no data is available, %NULL is
 * returned.
 *
 * Return value: (transfer none): Pointer to current read location
 */
gpointer
uca_ring_buffer_get_read_pointer (UcaRingBuffer *buffer)
{
    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), NULL);
    g_return_val_if_fail (buffer->read_index != buffer->write_index, NULL);

    gpointer data = buffer->data + (buffer->read_index % buffer->n_blocks_total) * buffer->block_size;
    buffer->read_index++;
    return data;
}

/**
 * uca_ring_buffer_get_write_pointer:
 * @buffer: A #UcaRingBuffer object
 *
 * Get pointer to current write location.
 *
 * Return value: (transfer none): Pointer to current write location
 */
gpointer
uca_ring_buffer_get_write_pointer (UcaRingBuffer *buffer)
{
    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), NULL);
    return buffer->data + (buffer->write_index % buffer->n_blocks_total) * buffer->block_size;
}

void
uca_ring_buffer_write_advance (UcaRingBuffer *buffer)
{
    g_return_if_fail (UCA_IS_RING_BUFFER (buffer));
    buffer->write_index++;
}

/**
 * uca_ring_buffer_peek_pointer:
 * @buffer: A #UcaRingBuffer object
 *
 * Get pointer to current write location without advancing.
 *
 * Return value: (transfer none): Pointer to current write location
 */
gpointer
uca_ring_buffer_peek_pointer (UcaRingBuffer *buffer)
{
    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), NULL);
    return ((guint8 *) buffer->data) + ((buffer->write_index % buffer->n_blocks_total) * buffer->block_size);
}

/**
 * uca_ring_buffer_get_pointer:
 * @buffer: A #UcaRingBuffer object
 * @index: Block index of queried pointer
 *
 * Get pointer to read location identified by @index.
 *
 * Return value: (transfer none): Pointer to indexed read location
 */
gpointer
uca_ring_buffer_get_pointer (UcaRingBuffer *buffer,
                             guint          index)
{
    g_return_val_if_fail (UCA_IS_RING_BUFFER (buffer), NULL);
    return ((guint8 *) buffer->data) + (((buffer->read_index + index) % buffer->n_blocks_total) * buffer->block_size);
}

guint
uca_ring_buffer_get_num_blocks (UcaRingBuffer *buffer)
{
    return buffer->write_index < buffer->n_blocks_total ? buffer->write_index : buffer->n_blocks_total;
}

static void
realloc_mem (UcaRingBuffer *buffer)
{
    if (buffer->data != NULL)
        g_free (buffer->data);

    buffer->data = g_malloc0_n (buffer->n_blocks_total, buffer->block_size);
}

static void
uca_ring_buffer_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UcaRingBuffer *buffer = UCA_RING_BUFFER (object);

    g_return_if_fail (UCA_IS_RING_BUFFER (object));
    switch (property_id) {
        case PROP_BLOCK_SIZE:
            g_value_set_uint64 (value, (guint64) buffer->block_size);
            break;

        case PROP_NUM_BLOCKS:
            g_value_set_uint (value, buffer->n_blocks_total);
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
    g_return_if_fail (UCA_IS_RING_BUFFER (object));

    UcaRingBuffer *buffer = UCA_RING_BUFFER (object);

    switch (property_id) {
        case PROP_BLOCK_SIZE:
            buffer->block_size = (gsize) g_value_get_uint64 (value);
            realloc_mem (buffer);
            break;

        case PROP_NUM_BLOCKS:
            buffer->n_blocks_total = g_value_get_uint (value);
            realloc_mem (buffer);
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
    UcaRingBuffer *buffer = UCA_RING_BUFFER (object);
    g_free (buffer->data);
    buffer->data = NULL;
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
}

static void
uca_ring_buffer_init (UcaRingBuffer *buffer)
{
    buffer->n_blocks_total = 0;
    buffer->block_size = 0;
    buffer->data = NULL;
}
