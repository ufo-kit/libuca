
#include <stdlib.h>
#include <string.h>
#include <clser.h>
#include <fgrab_struct.h>
#include <fgrab_prototyp.h>

#include "uca.h"
#include "uca-grabber.h"

struct fg_apc_data {
    Fg_Struct               *fg;
    dma_mem                 *mem;
    uint32_t                timeout;

    /* End-user related callback variables */
    uca_cam_grab_callback   callback;
    void                    *meta_data;
    void                    *user;
};

struct uca_sisofg_map_t {
    enum uca_grabber_constants uca_prop;
    int  fg_id;
    bool interpret_data;    /**< is data a constant or a value? */
};

static struct uca_sisofg_map_t uca_to_fg[] = {
    /* properties */
    { UCA_PROP_WIDTH,            FG_WIDTH,               false },
    { UCA_PROP_HEIGHT,           FG_HEIGHT,              false },
    { UCA_PROP_X_OFFSET,         FG_XOFFSET,             false },
    { UCA_PROP_Y_OFFSET,         FG_YOFFSET,             false },
    { UCA_PROP_EXPOSURE,         FG_EXPOSURE,            false },
    { UCA_PROP_GRAB_TIMEOUT,     FG_TIMEOUT,             false },

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

uint32_t uca_me4_destroy(struct uca_grabber *grabber)
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

uint32_t uca_me4_set_property(struct uca_grabber *grabber, int32_t property, void *data)
{
    uint32_t err = UCA_ERR_GRABBER | UCA_ERR_PROP;
    struct uca_sisofg_map_t *fg_prop = uca_me4_find_property(property);
    if (fg_prop == NULL)
        return err | UCA_ERR_INVALID;

    switch (property) {
        case UCA_PROP_GRAB_TIMEOUT:
            ((struct fg_apc_data *) grabber->user)->timeout = *((uint32_t *) data);
            break;

        default:
            break;
    }

    if (fg_prop->interpret_data) {
        /* Data is not a value but a SiSo specific constant that we need to
         * translate to Silicon Software speak. Therefore, we try to find it in
         * the map. */
        struct uca_sisofg_map_t *constant = uca_me4_find_property(*((uint32_t *) data));
        if (constant != NULL)
            return Fg_setParameter(GET_FG(grabber), fg_prop->fg_id, &constant->fg_id, PORT_A) == FG_OK ? \
                UCA_NO_ERROR : err | UCA_ERR_INVALID;
        return err | UCA_ERR_INVALID;
    }
    else
        return Fg_setParameter(GET_FG(grabber), fg_prop->fg_id, data, PORT_A) == FG_OK ? \
            UCA_NO_ERROR : err | UCA_ERR_INVALID;
}

uint32_t uca_me4_get_property(struct uca_grabber *grabber, enum uca_grabber_constants property, void *data)
{
    uint32_t err = UCA_ERR_GRABBER | UCA_ERR_PROP;
    struct uca_sisofg_map_t *fg_prop = uca_me4_find_property(property);
    if (fg_prop == NULL)
        return err | UCA_ERR_INVALID;

    /* FIXME: translate data back to UCA_ normalized constants */
    return Fg_getParameter(GET_FG(grabber), fg_prop->fg_id, data, PORT_A) == FG_OK ? \
        UCA_NO_ERROR : err | UCA_ERR_INVALID;
}

uint32_t uca_me4_alloc(struct uca_grabber *grabber, uint32_t pixel_size, uint32_t n_buffers)
{
    dma_mem *mem = GET_MEM(grabber);
    /* If buffers are already allocated, we are freeing every buffer and start
     * again. */
    if (mem != NULL) 
        Fg_FreeMemEx(GET_FG(grabber), mem);

    uint32_t width, height;
    uca_me4_get_property(grabber, UCA_PROP_WIDTH, &width);
    uca_me4_get_property(grabber, UCA_PROP_HEIGHT, &height);

    mem = Fg_AllocMemEx(GET_FG(grabber), n_buffers*width*height*pixel_size, n_buffers);
    if (mem != NULL) {
        ((struct fg_apc_data *) grabber->user)->mem = mem;
        return UCA_NO_ERROR;
    }
    return UCA_ERR_GRABBER | UCA_ERR_NO_MEMORY;
}

uint32_t uca_me4_acquire(struct uca_grabber *grabber, int32_t n_frames)
{
    if (GET_MEM(grabber) == NULL)
        return UCA_ERR_GRABBER | UCA_ERR_NO_MEMORY;

    int flag = grabber->asynchronous ? ACQ_STANDARD : ACQ_BLOCK;
    n_frames = n_frames < 0 ? GRAB_INFINITE : n_frames;
    if (Fg_AcquireEx(GET_FG(grabber), 0, n_frames, flag, GET_MEM(grabber)) == FG_OK)
        return UCA_NO_ERROR;

    return UCA_ERR_GRABBER | UCA_ERR_ACQUIRE;
}

uint32_t uca_me4_stop_acquire(struct uca_grabber *grabber)
{
    if (GET_MEM(grabber) != NULL)
        if (Fg_stopAcquireEx(GET_FG(grabber), 0, GET_MEM(grabber), STOP_SYNC) != FG_OK)
            return UCA_ERR_GRABBER | UCA_ERR_ACQUIRE;
    return UCA_NO_ERROR;
}

uint32_t uca_me4_grab(struct uca_grabber *grabber, void **buffer, uint64_t *frame_number)
{
    static frameindex_t last_frame = 0;
    struct fg_apc_data *me4 = (struct fg_apc_data *) grabber->user;

    if (grabber->asynchronous)
        last_frame = Fg_getLastPicNumberEx(me4->fg, PORT_A, me4->mem);
    else 
        last_frame = Fg_getLastPicNumberBlockingEx(me4->fg, last_frame+1, PORT_A, me4->timeout, me4->mem);

    if (last_frame <= 0)
        return UCA_ERR_GRABBER | UCA_ERR_FRAME_TRANSFER;

    *frame_number = (uint64_t) last_frame;
    *buffer = Fg_getImagePtrEx(me4->fg, last_frame, PORT_A, me4->mem);
    return UCA_NO_ERROR;
}

static int uca_me4_callback(frameindex_t frame, struct fg_apc_data *apc)
{
    apc->callback(frame, Fg_getImagePtrEx(apc->fg, frame, PORT_A, apc->mem), apc->meta_data, apc->user);
    return 0;
}

uint32_t uca_me4_register_callback(struct uca_grabber *grabber, uca_cam_grab_callback callback, void *meta_data, void *user)
{
    if (grabber->callback == NULL) {
        grabber->callback = callback;

        struct fg_apc_data *apc_data = (struct fg_apc_data *) grabber->user;
        apc_data->callback = callback;
        apc_data->meta_data = meta_data;
        apc_data->user = user;

        struct FgApcControl ctrl;
        ctrl.version = 0;
        ctrl.data = (struct fg_apc_data *) grabber->user;
        ctrl.func = &uca_me4_callback;
        ctrl.flags = FG_APC_DEFAULTS;
        ctrl.timeout = 1;

        if (Fg_registerApcHandler(GET_FG(grabber), PORT_A, &ctrl, FG_APC_CONTROL_BASIC) != FG_OK)
            return UCA_ERR_GRABBER | UCA_ERR_CALLBACK;
    }
    else
        return UCA_ERR_GRABBER | UCA_ERR_CALLBACK;

    return UCA_NO_ERROR;
}

uint32_t uca_me4_init(struct uca_grabber **grabber)
{
    Fg_Struct *fg = Fg_Init("libFullAreaGray8.so", 0);
    if (fg == NULL)
        return UCA_ERR_GRABBER | UCA_ERR_NOT_FOUND;

    struct uca_grabber *uca = (struct uca_grabber *) malloc(sizeof(struct uca_grabber));
    memset(uca, 0, sizeof(struct uca_grabber));

    struct fg_apc_data *me4 = (struct fg_apc_data *) malloc(sizeof(struct fg_apc_data));
    memset(me4, 0, sizeof(struct fg_apc_data));
    me4->fg  = fg;

    Fg_getParameter(fg, FG_TIMEOUT, &me4->timeout, PORT_A);

    uca->user = me4;
    uca->destroy = &uca_me4_destroy;
    uca->set_property = &uca_me4_set_property;
    uca->get_property = &uca_me4_get_property;
    uca->alloc = &uca_me4_alloc;
    uca->acquire = &uca_me4_acquire;
    uca->stop_acquire = &uca_me4_stop_acquire;
    uca->grab = &uca_me4_grab;
    uca->register_callback = &uca_me4_register_callback;
    
    *grabber = uca;
    return UCA_NO_ERROR;
}
