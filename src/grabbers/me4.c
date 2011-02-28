
#include <stdlib.h>
#include <string.h>
#include <clser.h>
#include <fgrab_struct.h>
#include <fgrab_prototyp.h>

#include "uca.h"
#include "uca-grabber.h"

struct uca_me4_grabber_t {
    Fg_Struct   *fg;
    dma_mem     *mem;
};

#define GET_FG(grabber) (((struct uca_me4_grabber_t *) grabber->user)->fg)
#define GET_MEM(grabber) (((struct uca_me4_grabber_t *) grabber->user)->mem)

uint32_t uca_me4_destroy(struct uca_grabber_t *grabber)
{
    Fg_FreeGrabber(GET_FG(grabber));
}

uint32_t uca_me4_set_property(struct uca_grabber_t *grabber, enum uca_property_ids property, void *data)
{
    return Fg_setParameter(GET_FG(grabber), property, data, PORT_A) == FG_OK ? UCA_NO_ERROR : UCA_ERR_PROP_GENERAL;
}

uint32_t uca_me4_get_property(struct uca_grabber_t *grabber, enum uca_property_ids property, void *data)
{
    return Fg_getParameter(GET_FG(grabber), property, data, PORT_A) == FG_OK ? UCA_NO_ERROR : UCA_ERR_PROP_GENERAL;
}

uint32_t uca_me4_alloc(struct uca_grabber_t *grabber, uint32_t n_buffers)
{
    if (GET_MEM(grabber) != NULL)
        /* FIXME: invent better error code */
        return UCA_ERR_PROP_GENERAL;

    uint32_t width, height;
    uca_me4_get_property(grabber, FG_WIDTH, &width);
    uca_me4_get_property(grabber, FG_HEIGHT, &height);

    /* FIXME: get size of pixel */
    dma_mem *mem = Fg_AllocMemEx(GET_FG(grabber), n_buffers*width*height*sizeof(uint16_t), n_buffers);
    if (mem != NULL) {
        ((struct uca_me4_grabber_t *) grabber->user)->mem = mem;
        return UCA_NO_ERROR;
    }
    return UCA_ERR_PROP_GENERAL;
}

uint32_t uca_me4_acquire(struct uca_grabber_t *grabber, int32_t n_frames, bool async)
{
    if (GET_MEM(grabber) != NULL) {
        if (Fg_AcquireEx(GET_FG(grabber), 0, n_frames, ACQ_STANDARD, GET_MEM(grabber)) != FG_OK)
            return UCA_NO_ERROR;
    }
    return UCA_ERR_PROP_GENERAL;
}

uint32_t uca_me4_grab(struct uca_grabber_t *grabber, char *buffer, size_t n_bytes)
{
    uint32_t last_frame = Fg_getLastPicNumber(GET_FG(grabber), PORT_A);
    memcpy(buffer, Fg_getImagePtrEx(GET_FG(grabber), last_frame, PORT_A, GET_MEM(grabber)), n_bytes);
}

uint32_t uca_me4_init(struct uca_grabber_t **grabber)
{
    /* FIXME: find out if this board/grabber is running */
    Fg_Struct *fg = Fg_Init("libFullAreaGray8.so", 0);
    if (fg == NULL)
        return UCA_ERR_INIT_NOT_FOUND;

    struct uca_grabber_t *uca = (struct uca_grabber_t *) malloc(sizeof(struct uca_grabber_t));
    struct uca_me4_grabber_t *me4 = (struct uca_me4_grabber_t *) malloc(sizeof(struct uca_me4_grabber_t));

    me4->fg = fg;
    me4->mem = NULL;
    uca->user = me4;
    uca->destroy = &uca_me4_destroy;
    uca->set_property = &uca_me4_set_property;
    uca->get_property = &uca_me4_get_property;
    uca->alloc = &uca_me4_alloc;
    uca->acquire = &uca_me4_acquire;
    uca->grab = &uca_me4_grab;
    
    *grabber = uca;
    return UCA_NO_ERROR;
}
