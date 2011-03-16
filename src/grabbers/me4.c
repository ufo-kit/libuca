
#include <stdlib.h>
#include <string.h>
#include <clser.h>
#include <fgrab_struct.h>
#include <fgrab_prototyp.h>

#include "uca.h"
#include "uca-grabber.h"

struct fg_apc_data {
    Fg_Struct   *fg;
    dma_mem     *mem;
    uca_grabber_grab_callback cb;
};

struct uca_sisofg_map_t {
    enum uca_grabber_constants uca_prop;
    int  fg_id;
    bool interpret_data;    /**< is data a constant or a value? */
};

static struct uca_sisofg_map_t uca_to_fg[] = {
    /* properties */
    { UCA_GRABBER_WIDTH,            FG_WIDTH,               false },
    { UCA_GRABBER_HEIGHT,           FG_HEIGHT,              false },
    { UCA_GRABBER_OFFSET_X,         FG_XOFFSET,             false },
    { UCA_GRABBER_OFFSET_Y,         FG_YOFFSET,             false },
    { UCA_GRABBER_EXPOSURE,         FG_EXPOSURE,            false },
    { UCA_GRABBER_TRIGGER_MODE,     FG_TRIGGERMODE,         true},
    { UCA_GRABBER_FORMAT,           FG_FORMAT,              true},
    { UCA_GRABBER_CAMERALINK_TYPE,  FG_CAMERA_LINK_CAMTYP,  true },

    /* values */
    { UCA_FORMAT_GRAY8,             FG_GRAY,                false },
    { UCA_FORMAT_GRAY16,            FG_GRAY16,              false },
    { UCA_CL_8BIT_FULL_8,           FG_CL_8BIT_FULL_8,      false },
    { UCA_CL_8BIT_FULL_10,          FG_CL_8BIT_FULL_10,     false },
    { UCA_TRIGGER_FREERUN,          FREE_RUN,               false },
    { UCA_GRABBER_INVALID,          0,                      false }
};

#define GET_FG(grabber) (((struct fg_apc_data *) grabber->user)->fg)
#define GET_MEM(grabber) (((struct fg_apc_data *) grabber->user)->mem)

uint32_t uca_me4_destroy(struct uca_grabber_t *grabber)
{
    if (grabber != NULL) {
        Fg_FreeMemEx(GET_FG(grabber), GET_MEM(grabber));
        Fg_FreeGrabber(GET_FG(grabber));
    }
    return UCA_NO_ERROR;
}

static struct uca_sisofg_map_t *uca_me4_find_property(enum uca_grabber_constants property)
{
    int i = 0;
    /* Find a valid frame grabber id for the property */
    while (uca_to_fg[i].uca_prop != UCA_GRABBER_INVALID) {
        if (uca_to_fg[i].uca_prop == property)
            return &uca_to_fg[i];
        i++;
    }
    return NULL;
}

uint32_t uca_me4_set_property(struct uca_grabber_t *grabber, enum uca_grabber_constants property, void *data)
{
    struct uca_sisofg_map_t *fg_prop = uca_me4_find_property(property);
    if (fg_prop == NULL)
        return UCA_ERR_PROP_INVALID;

    if (fg_prop->interpret_data) {
        /* Data is not a value but a constant that we need to translate to
         * Silicon Software speak. Therefore, we try to find it in the map also. */
        struct uca_sisofg_map_t *constant = uca_me4_find_property(*((uint32_t *) data));
        if (constant != NULL)
            return Fg_setParameter(GET_FG(grabber), fg_prop->fg_id, &constant->fg_id, PORT_A) == FG_OK ? UCA_NO_ERROR : UCA_ERR_PROP_INVALID;
        return UCA_ERR_PROP_INVALID;
    }
    else
        return Fg_setParameter(GET_FG(grabber), fg_prop->fg_id, data, PORT_A) == FG_OK ? UCA_NO_ERROR : UCA_ERR_PROP_GENERAL;
}

uint32_t uca_me4_get_property(struct uca_grabber_t *grabber, enum uca_grabber_constants property, void *data)
{
    struct uca_sisofg_map_t *fg_prop = uca_me4_find_property(property);
    if (fg_prop == NULL)
        return UCA_ERR_PROP_INVALID;

    /* FIXME: translate data back to UCA_ normalized constants */
    return Fg_getParameter(GET_FG(grabber), fg_prop->fg_id, data, PORT_A) == FG_OK ? UCA_NO_ERROR : UCA_ERR_PROP_GENERAL;
}

uint32_t uca_me4_alloc(struct uca_grabber_t *grabber, uint32_t pixel_size, uint32_t n_buffers)
{
    if (GET_MEM(grabber) != NULL)
        /* FIXME: invent better error code */
        return UCA_ERR_PROP_GENERAL;

    uint32_t width, height;
    uca_me4_get_property(grabber, FG_WIDTH, &width);
    uca_me4_get_property(grabber, FG_HEIGHT, &height);

    dma_mem *mem = Fg_AllocMemEx(GET_FG(grabber), n_buffers*width*height*pixel_size, n_buffers);
    if (mem != NULL) {
        ((struct fg_apc_data *) grabber->user)->mem = mem;
        return UCA_NO_ERROR;
    }
    return UCA_ERR_PROP_GENERAL;
}

uint32_t uca_me4_acquire(struct uca_grabber_t *grabber, int32_t n_frames)
{
    if (GET_MEM(grabber) == NULL)
        return UCA_ERR_GRABBER_NOMEM;

    int flag = grabber->asynchronous ? ACQ_STANDARD : ACQ_BLOCK;
    n_frames = n_frames < 0 ? GRAB_INFINITE : n_frames;
    if (Fg_AcquireEx(GET_FG(grabber), 0, n_frames, flag, GET_MEM(grabber)) == FG_OK)
        return UCA_NO_ERROR;

    return UCA_ERR_GRABBER_ACQUIRE;
}

uint32_t uca_me4_stop_acquire(struct uca_grabber_t *grabber)
{
    if (GET_MEM(grabber) != NULL)
        if (Fg_stopAcquireEx(GET_FG(grabber), 0, GET_MEM(grabber), STOP_SYNC) != FG_OK)
            return UCA_ERR_PROP_GENERAL;
    return UCA_NO_ERROR;
}

uint32_t uca_me4_grab(struct uca_grabber_t *grabber, void **buffer)
{
    int32_t last_frame;
    if (grabber->asynchronous)
        last_frame = Fg_getLastPicNumberEx(GET_FG(grabber), PORT_A, GET_MEM(grabber));
    else 
        last_frame = Fg_getLastPicNumberBlockingEx(GET_FG(grabber), 1, PORT_A, 10, GET_MEM(grabber));

    if (last_frame <= 0)
        return UCA_ERR_PROP_GENERAL;

    *buffer = Fg_getImagePtrEx(GET_FG(grabber), last_frame, PORT_A, GET_MEM(grabber));
    return UCA_NO_ERROR;
}

static int uca_me4_callback(frameindex_t frame, struct fg_apc_data *apc)
{
    apc->cb(frame, Fg_getImagePtr(apc->fg, frame, PORT_A));
    return 0;
}

uint32_t uca_me4_register_callback(struct uca_grabber_t *grabber, uca_grabber_grab_callback cb)
{
    if (grabber->callback == NULL) {
        grabber->callback = cb;
        ((struct fg_apc_data *) grabber->user)->cb = cb;

        struct FgApcControl ctrl;
        ctrl.version = 0;
        ctrl.data = (struct fg_apc_data *) grabber->user;
        ctrl.func = &uca_me4_callback;
        ctrl.flags = FG_APC_DEFAULTS;
        ctrl.timeout = 1;

        if (Fg_registerApcHandler(GET_FG(grabber), PORT_A, &ctrl, FG_APC_CONTROL_BASIC) != FG_OK)
            return UCA_ERR_GRABBER_CALLBACK_REGISTRATION_FAILED;
    }
    else
        return UCA_ERR_GRABBER_CALLBACK_ALREADY_REGISTERED;

    return UCA_NO_ERROR;
}

uint32_t uca_me4_init(struct uca_grabber_t **grabber)
{
    Fg_Struct *fg = Fg_Init("libFullAreaGray8.so", 0);
    if (fg == NULL)
        return UCA_ERR_GRABBER_NOT_FOUND;

    struct uca_grabber_t *uca = (struct uca_grabber_t *) malloc(sizeof(struct uca_grabber_t));
    struct fg_apc_data *me4 = (struct fg_apc_data *) malloc(sizeof(struct fg_apc_data));

    me4->fg  = fg;
    me4->mem = NULL;
    me4->cb  = NULL;

    uca->user = me4;
    uca->destroy = &uca_me4_destroy;
    uca->set_property = &uca_me4_set_property;
    uca->get_property = &uca_me4_get_property;
    uca->alloc = &uca_me4_alloc;
    uca->acquire = &uca_me4_acquire;
    uca->stop_acquire = &uca_me4_stop_acquire;
    uca->grab = &uca_me4_grab;
    uca->callback = NULL;
    
    *grabber = uca;
    return UCA_NO_ERROR;
}
