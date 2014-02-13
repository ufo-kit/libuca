#include <glib.h>
#include "uca-ring-buffer.h"


static void
test_new_constructor (void)
{
    UcaRingBuffer *buffer;

    buffer = g_object_new (UCA_TYPE_RING_BUFFER,
                           "block-size", (guint64) 512,
                           "num-blocks", 2,
                           NULL);

    g_assert (uca_ring_buffer_get_block_size (buffer) == 512);
    g_assert (uca_ring_buffer_get_num_blocks (buffer) == 0);
    g_assert (uca_ring_buffer_get_write_pointer (buffer) != NULL);

    g_object_unref (buffer);
}

static void
test_new_func (void)
{
    UcaRingBuffer *buffer;

    buffer = uca_ring_buffer_new (512, 2);

    g_assert (uca_ring_buffer_get_block_size (buffer) == 512);
    g_assert (uca_ring_buffer_get_num_blocks (buffer) == 0);
    g_assert (uca_ring_buffer_get_write_pointer (buffer) != NULL);

    g_object_unref (buffer);
}

static void
test_ring (void)
{
    UcaRingBuffer *buffer;
    guint32 *data;

    buffer = uca_ring_buffer_new (512, 2);

    data = uca_ring_buffer_get_write_pointer (buffer);
    data[0] = 0xBADF00D;
    uca_ring_buffer_write_advance (buffer);

    g_assert (uca_ring_buffer_available (buffer));
    g_assert (uca_ring_buffer_get_num_blocks (buffer) == 1);

    data = uca_ring_buffer_get_write_pointer (buffer);
    data[0] = 0xDEADBEEF;
    uca_ring_buffer_write_advance (buffer);

    g_assert (uca_ring_buffer_get_num_blocks (buffer) == 2);
    
    data = uca_ring_buffer_get_read_pointer (buffer);
    g_assert (data[0] == 0xBADF00D);

    data = uca_ring_buffer_get_read_pointer (buffer);
    g_assert (data[0] == 0xDEADBEEF);

    g_assert (!uca_ring_buffer_available (buffer));

    uca_ring_buffer_reset (buffer);
    g_assert (uca_ring_buffer_get_num_blocks (buffer) == 0);
}

static void
test_overwrite (void)
{
    UcaRingBuffer *buffer;
    guint32 *data;

    buffer = uca_ring_buffer_new (512, 1);

    data = uca_ring_buffer_get_write_pointer (buffer);
    data[0] = 0xBADF00D;
    uca_ring_buffer_write_advance (buffer);

    data = uca_ring_buffer_get_write_pointer (buffer);
    data[0] = 0xDEADBEEF;
    uca_ring_buffer_write_advance (buffer);

    data = uca_ring_buffer_get_read_pointer (buffer);
    g_assert (data[0] == 0xDEADBEEF);
}

int
main (int argc, char *argv[])
{
    g_type_init ();
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/ringbuffer/new/constructor", test_new_constructor);
    g_test_add_func ("/ringbuffer/new/func", test_new_func);
    g_test_add_func ("/ringbuffer/functionality ", test_ring);
    g_test_add_func ("/ringbuffer/overwrite ", test_overwrite);

    return g_test_run ();
}
