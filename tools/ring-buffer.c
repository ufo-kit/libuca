
#include <math.h>
#include "ring-buffer.h"

RingBuffer *
ring_buffer_new (gsize block_size,
                 gsize n_blocks)
{
    RingBuffer *buffer;

    buffer = g_new0 (RingBuffer, 1);
    buffer->block_size = block_size;
    buffer->n_blocks_total = n_blocks;
    buffer->n_blocks_used = 0;
    buffer->current_index = 0;
    buffer->data = g_malloc0_n (n_blocks, block_size);

    return buffer;
}

void
ring_buffer_free (RingBuffer *buffer)
{
    g_free (buffer->data);
    g_free (buffer);
}

void
ring_buffer_reset (RingBuffer *buffer)
{
    buffer->n_blocks_used = 0;
    buffer->current_index = 0;
}

gsize
ring_buffer_get_block_size (RingBuffer *buffer)
{
    return buffer->block_size;
}

gpointer
ring_buffer_get_current_pointer (RingBuffer *buffer)
{
    return buffer->data + (buffer->current_index % buffer->n_blocks_total) * buffer->block_size;
}

gpointer
ring_buffer_get_pointer (RingBuffer *buffer,
                         guint       index)
{
    g_assert (index < buffer->n_blocks_total);
    return buffer->data + ((buffer->current_index - buffer->n_blocks_used + index) % buffer->n_blocks_total) * buffer->block_size;
}

guint
ring_buffer_get_num_blocks (RingBuffer *buffer)
{
    return buffer->n_blocks_used;
}

void
ring_buffer_proceed (RingBuffer *buffer)
{
    buffer->current_index++;

    if (buffer->n_blocks_used < buffer->n_blocks_total)
        buffer->n_blocks_used++;
    else
        buffer->current_index = buffer->current_index % buffer->n_blocks_total;
}
