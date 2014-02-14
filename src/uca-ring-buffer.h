#ifndef UCA_RING_BUFFER_H
#define UCA_RING_BUFFER_H

#include <glib-object.h>

#define UCA_TYPE_RING_BUFFER             (uca_ring_buffer_get_type())
#define UCA_RING_BUFFER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_RING_BUFFER, UcaRingBuffer))
#define UCA_IS_RING_BUFFER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_RING_BUFFER))
#define UCA_RING_BUFFER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UCA_TYPE_RING_BUFFER, UcaRingBufferClass))
#define UCA_IS_RING_BUFFER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_RING_BUFFER))
#define UCA_RING_BUFFER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_RING_BUFFER, UcaRingBufferClass))

G_BEGIN_DECLS

typedef struct _UcaRingBuffer           UcaRingBuffer;
typedef struct _UcaRingBufferClass      UcaRingBufferClass;
typedef struct _UcaRingBufferPrivate    UcaRingBufferPrivate;

struct _UcaRingBuffer {
    /*< private >*/
    GObject parent;

    UcaRingBufferPrivate *priv;
};

struct _UcaRingBufferClass {
    /*< private >*/
    GObjectClass parent;
};

UcaRingBuffer * uca_ring_buffer_new                 (gsize          block_size,
                                                     guint          n_blocks);
void            uca_ring_buffer_reset               (UcaRingBuffer *buffer);
gsize           uca_ring_buffer_get_block_size      (UcaRingBuffer *buffer);
guint           uca_ring_buffer_get_num_blocks      (UcaRingBuffer *buffer);
gboolean        uca_ring_buffer_available           (UcaRingBuffer *buffer);
void            uca_ring_buffer_proceed             (UcaRingBuffer *buffer);
gpointer        uca_ring_buffer_get_read_pointer    (UcaRingBuffer *buffer);
gpointer        uca_ring_buffer_get_write_pointer   (UcaRingBuffer *buffer);
gpointer        uca_ring_buffer_get_pointer         (UcaRingBuffer *buffer,
                                                     guint          index);
gpointer        uca_ring_buffer_peek_pointer        (UcaRingBuffer *buffer);

GType uca_ring_buffer_get_type (void);

G_END_DECLS

#endif
