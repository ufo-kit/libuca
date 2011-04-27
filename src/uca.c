#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"

#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif

#ifdef HAVE_DUMMY_CAMERA
#include "cameras/dummy.h"
#endif

#ifdef HAVE_ME4
#include "grabbers/me4.h"
#endif

#ifdef HAVE_PCO_EDGE
#include "cameras/pco.h"
#endif

#ifdef HAVE_PHOTON_FOCUS
#include "cameras/pf.h"
#endif

#ifdef HAVE_IPE_CAMERA
#include "cameras/ipe.h"
#endif

const char *uca_unit_map[] = {
    "px",
    "bits",
    "ns",
    "µs",
    "ms",
    "s",
    "rows",
    "fps",
    "°C",
    "[0/1]",
    "" 
};

static struct uca_property property_map[UCA_PROP_LAST+1] = {
    { "General.Name",           uca_na,     uca_string,  uca_read }, 
    { "Image.Width",            uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "Image.Width.Min",        uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Width.Max",        uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Height",           uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "Image.Height.Min",       uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Height.Max",       uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Offset.x",         uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "Image.Offset.x.Min",     uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Offset.x.Max",     uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Offset.y",         uca_pixel,  uca_uint32t, uca_readwrite }, 
    { "Image.Offset.y.Min",     uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Offset.y.Max",     uca_pixel,  uca_uint32t, uca_read }, 
    { "Image.Bitdepth",         uca_bits,   uca_uint32t, uca_read}, 
    { "Time.Exposure",          uca_us,     uca_uint32t, uca_readwrite }, 
    { "Time.Exposure.Min",      uca_us,     uca_uint32t, uca_read }, 
    { "Time.Exposure.Max",      uca_us,     uca_uint32t, uca_read }, 
    { "Time.Delay",             uca_us,     uca_uint32t, uca_readwrite }, 
    { "Time.Delay.Min",         uca_us,     uca_uint32t, uca_read }, 
    { "Time.Delay.Max",         uca_us,     uca_uint32t, uca_read }, 
    { "Time.Framerate",         uca_fps,    uca_uint32t, uca_read }, 
    { "Temperature.Sensor",     uca_dc,     uca_uint32t, uca_read },
    { "Temperature.Camera",     uca_dc,     uca_uint32t, uca_read },
    { "Trigger.Mode",           uca_na,     uca_uint32t, uca_readwrite }, 
    { "Trigger.Exposure",       uca_na,     uca_uint32t, uca_readwrite },
    { "Gain.PGA",               uca_na,     uca_uint32t, uca_readwrite },
    { "Gain.PGA.Min",           uca_na,     uca_uint32t, uca_read },
    { "Gain.PGA.Max",           uca_na,     uca_uint32t, uca_read },
    { "Gain.PGA.Step",          uca_na,     uca_uint32t, uca_read },
    { "Gain.ADC",               uca_na,     uca_uint32t, uca_readwrite },
    { "Gain.ADC.Min",           uca_na,     uca_uint32t, uca_read },
    { "Gain.ADC.Max",           uca_na,     uca_uint32t, uca_read },
    { "Gain.ADC.Step",          uca_na,     uca_uint32t, uca_read },
    { "Grabber.Timeout",        uca_s,      uca_uint32t, uca_readwrite },
    { "Grabber.Synchronous",    uca_bool,   uca_uint32t, uca_readwrite },
    { "Mode.Timestamp",         uca_na,     uca_uint32t, uca_readwrite }, 
    { "Mode.Scan",              uca_na,     uca_uint32t, uca_readwrite }, 
    { "Interlace.Samplerate",   uca_na,     uca_uint32t, uca_readwrite }, 
    { "Interlace.Threshold.Pixel", uca_na,  uca_uint32t, uca_readwrite }, 
    { "Interlace.Threshold.Row", uca_na,    uca_uint32t, uca_readwrite },
    { "Mode.correction",        uca_na,     uca_uint32t, uca_readwrite }, 
    { NULL, 0, 0, 0 }
};


#ifdef HAVE_PTHREADS
static pthread_mutex_t g_uca_init_lock = PTHREAD_MUTEX_INITIALIZER;
#define uca_lock() pthread_mutex_lock((&g_uca_init_lock))
#define uca_unlock() pthread_mutex_unlock((&g_uca_init_lock))
#else
#define uca_lock(lock) 
#define uca_unlock(lock) 
#endif

struct uca *g_uca = NULL;

struct uca *uca_init(const char *config_filename)
{
    uca_lock();
    if (g_uca != NULL) {
        uca_unlock();
        return g_uca;
    }
    g_uca = (struct uca *) malloc(sizeof(struct uca));
    g_uca->cameras = NULL;

    uca_grabber_init grabber_inits[] = {
#ifdef HAVE_ME4
        uca_me4_init,
#endif
        NULL
    };

    uca_cam_init cam_inits[] = {
#ifdef HAVE_PCO_EDGE
        uca_pco_init,
#endif
#ifdef HAVE_PHOTON_FOCUS
        uca_pf_init,
#endif
#ifdef HAVE_IPE_CAMERA
        uca_ipe_init,
#endif
#ifdef HAVE_DUMMY_CAMERA
        uca_dummy_init,
#endif
        NULL
    };

    /* Probe each frame grabber that is configured */
    int i = 0;
    struct uca_grabber_priv *grabber = NULL;
    while (grabber_inits[i] != NULL) {
        uca_grabber_init init = grabber_inits[i];
        /* FIXME: we don't want to take the only first one */
        if (init(&grabber) == UCA_NO_ERROR)
            break;
        i++;
    }

    /* XXX: We could have no grabber (aka NULL) which is good anyway, since
     * some cameras don't need a grabber device (such as the IPE camera),
     * therefore we also probe each camera against the NULL grabber. However,
     * each camera must make sure to check for such a situation. */

    if (grabber != NULL) {
        g_uca->grabbers = (struct uca_grabber *) malloc(sizeof(struct uca_grabber));
        g_uca->grabbers->priv = grabber;
        g_uca->grabbers->next = NULL;
    }

    i = 0;
    struct uca_camera *current = NULL;
    /* Probe each camera that is configured and append a found camera to the
     * linked list. */
    while (cam_inits[i] != NULL) {
        struct uca_camera_priv *cam = NULL;
        uca_cam_init init = cam_inits[i];
        if (init(&cam, grabber) == UCA_NO_ERROR) {
            if (current == NULL) {
                g_uca->cameras = (struct uca_camera *) malloc(sizeof(struct uca_camera));
                g_uca->cameras->priv = cam;
                g_uca->cameras->next = NULL;
                current = g_uca->cameras;
            }
            else {
                current->next = (struct uca_camera *) malloc(sizeof(struct uca_camera));
                current->next->priv = cam;
                current = current->next;
            }
            current->next = NULL;
        }
        i++;
    }

    if (g_uca->cameras == NULL) {
        free(g_uca);
        g_uca = NULL;
        uca_unlock();
        return NULL;
    }
    uca_unlock();
    return g_uca;
}

void uca_destroy(struct uca *u)
{
    uca_lock();
    if (u != NULL) {
        struct uca_camera *cam = u->cameras, *tmp;
        struct uca_camera_priv *cam_priv;
        while (cam != NULL) {
            tmp = cam;
            cam_priv = cam->priv;
            cam_priv->destroy(cam_priv);
            cam = cam->next;
            free(tmp);
        }

        struct uca_grabber *grabber = u->grabbers, *tmpg;
        struct uca_grabber_priv *grabber_priv;
        while (grabber != NULL) {
            tmpg = grabber;
            grabber_priv = grabber->priv;
            grabber_priv->destroy(grabber_priv);
            grabber = grabber->next;
            free(tmpg);
        }

        free(u);
    }
    uca_unlock();
}

uint32_t uca_get_property_id(const char *property_name, enum uca_property_ids *prop_id)
{
    int i = 0;
    while (property_map[i].name != NULL) {
        if (!strcmp(property_map[i].name, property_name)) {
            *prop_id = (enum uca_property_ids) i;
            return UCA_NO_ERROR;
        }
        i++;
    }
    return UCA_ERR_CAMERA | UCA_ERR_PROP | UCA_ERR_INVALID;
}

struct uca_property *uca_get_full_property(enum uca_property_ids property_id)
{
    if ((property_id >= 0) && (property_id < UCA_PROP_LAST))
        return &property_map[property_id];
    return NULL;
}

const char* uca_get_property_name(enum uca_property_ids property_id)
{
    if ((property_id >= 0) && (property_id < UCA_PROP_LAST))
        return property_map[property_id].name;
    return UCA_NO_ERROR;
}

uint32_t uca_cam_alloc(struct uca_camera *cam, uint32_t n_buffers)
{
    uint32_t bitdepth;
    struct uca_camera_priv *priv = cam->priv;
    priv->get_property(priv, UCA_PROP_BITDEPTH, &bitdepth, 0);
    const int pixel_size = bitdepth == 8 ? 1 : 2;
    if (priv->grabber != NULL)
        return priv->grabber->alloc(priv->grabber, pixel_size, n_buffers);
    return UCA_NO_ERROR;
}

enum uca_cam_state uca_cam_get_state(struct uca_camera *cam)
{
    struct uca_camera_priv *priv = cam->priv;
    return priv->state;
}

uint32_t uca_cam_set_property(struct uca_camera *cam, enum uca_property_ids property, void *data)
{
    struct uca_camera_priv *priv = cam->priv;
    return priv->set_property(priv, property, data);
}

uint32_t uca_cam_get_property(struct uca_camera *cam, enum uca_property_ids property, void *data, size_t num)
{
    struct uca_camera_priv *priv = cam->priv;
    return priv->get_property(priv, property, data, num);
}

uint32_t uca_cam_start_recording(struct uca_camera *cam)
{
    struct uca_camera_priv *priv = cam->priv;
    if (priv->state == UCA_CAM_RECORDING)
        return UCA_ERR_CAMERA | UCA_ERR_CONFIGURATION | UCA_ERR_IS_RECORDING;

    uint32_t err = priv->start_recording(priv);
    if (err == UCA_NO_ERROR)
        priv->state = UCA_CAM_RECORDING;
    return err;
}

uint32_t uca_cam_stop_recording(struct uca_camera *cam)
{
    struct uca_camera_priv *priv = cam->priv;
    if (priv->state != UCA_CAM_RECORDING)
        return UCA_ERR_CAMERA | UCA_ERR_CONFIGURATION | UCA_ERR_NOT_RECORDING;

    uint32_t err = priv->stop_recording(priv);
    if (err == UCA_NO_ERROR)
        priv->state = UCA_CAM_CONFIGURABLE;
    return err;
}

uint32_t uca_cam_trigger(struct uca_camera *cam)
{
    struct uca_camera_priv *priv = cam->priv;
    if (priv->state != UCA_CAM_RECORDING)
        return UCA_ERR_CAMERA | UCA_ERR_TRIGGER | UCA_ERR_NOT_RECORDING;
    return priv->trigger(priv);
}

uint32_t uca_cam_register_callback(struct uca_camera *cam, uca_cam_grab_callback callback, void *user)
{
    struct uca_camera_priv *priv = cam->priv;
    return priv->register_callback(priv, callback, user);
}

uint32_t uca_cam_grab(struct uca_camera *cam, char *buffer, void *meta_data)
{
    struct uca_camera_priv *priv = cam->priv;
    if (priv->state != UCA_CAM_RECORDING)
        return UCA_ERR_CAMERA | UCA_ERR_NOT_RECORDING;
    return priv->grab(priv, buffer, meta_data);
}

