#ifndef UCA_RING_BUFFER_H
#define UCA_RING_BUFFER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UCA_TYPE_RING_BUFFER             (uca_ring_buffer_get_type())
G_DECLARE_FINAL_TYPE(UcaRingBuffer, uca_ring_buffer, UCA, RING_BUFFER, GObject)

UcaRingBuffer * uca_ring_buffer_new                 (gsize          block_size,
                                                     guint          n_blocks);
void            uca_ring_buffer_reset               (UcaRingBuffer *buffer);
gsize           uca_ring_buffer_get_block_size      (UcaRingBuffer *buffer);
guint           uca_ring_buffer_get_num_blocks      (UcaRingBuffer *buffer);
gboolean        uca_ring_buffer_available           (UcaRingBuffer *buffer);
void            uca_ring_buffer_proceed             (UcaRingBuffer *buffer);
gpointer        uca_ring_buffer_get_read_pointer    (UcaRingBuffer *buffer);
gpointer        uca_ring_buffer_get_write_pointer   (UcaRingBuffer *buffer);
void            uca_ring_buffer_write_advance       (UcaRingBuffer *buffer);
gpointer        uca_ring_buffer_get_pointer         (UcaRingBuffer *buffer,
                                                     guint          index);
gpointer        uca_ring_buffer_peek_pointer        (UcaRingBuffer *buffer);

G_END_DECLS

#endif
