/* Copyright (C) 2011, 2012 Matthias Vogelgesang <matthias.vogelgesang@kit.edu>
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


#include <tango.h>

extern "C" {
#include <gmodule.h>
#include <gio/gio.h>
#include <string.h>
#include <math.h>
#include <kiro/kiro-trb.h>
#include <kiro/kiro-client.h>
#include "uca-kiro-camera.h"
} // EXTERN  C

#define UCA_KIRO_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_KIRO_CAMERA, UcaKiroCameraPrivate))
#undef __CREATE_RANDOM_IMAGE_DATA__

static void uca_kiro_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaKiroCamera, uca_kiro_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_kiro_initable_iface_init))

enum {
    PROP_KIRO_ADDRESS = N_BASE_PROPERTIES,
    PROP_KIRO_PORT,
    PROP_KIRO_TANGO_ADDRESS,
    N_PROPERTIES
};

static const gint kiro_overrideables[] = {
    PROP_NAME,
    0,
};

static GParamSpec *kiro_properties[N_PROPERTIES] = { NULL, };
GParamSpec **kiro_dynamics;

struct _UcaKiroCameraPrivate {
    gfloat frame_rate;
    
    guint8 *dummy_data;
    guint current_frame;
    gchar *kiro_address;
    gchar *kiro_port;
    guint kiro_port_uint;
    gchar *kiro_tango_address;
    GList *kiro_dynamic_properties;

    gboolean thread_running;
    gboolean kiroclient_started;
    gboolean construction_error;

    GThread *grab_thread;
    KiroTrb *receive_buffer;
    KiroClient *kiro_receiver;
};

static gpointer
kiro_grab_func(gpointer data)
{
    UcaKiroCamera *kiro_camera = UCA_KIRO_CAMERA(data);
    g_return_val_if_fail(UCA_IS_KIRO_CAMERA(kiro_camera), NULL);

    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE(kiro_camera);
    UcaCamera *camera = UCA_CAMERA(kiro_camera);
    const gulong sleep_time = (gulong) G_USEC_PER_SEC / priv->frame_rate;

    while (priv->thread_running) {
        camera->grab_func(priv->dummy_data, camera->user_data);
        g_usleep(sleep_time);
    }

    return NULL;
}

static void
uca_kiro_camera_start_recording(UcaCamera *camera, GError **error)
{
    gboolean transfer_async = FALSE;
    UcaKiroCameraPrivate *priv;
    g_return_if_fail(UCA_IS_KIRO_CAMERA(camera));

    priv = UCA_KIRO_CAMERA_GET_PRIVATE(camera);
    
    kiro_client_sync(priv->kiro_receiver);
    kiro_trb_adopt(priv->receive_buffer, kiro_client_get_memory(priv->kiro_receiver));
        
    g_object_get(G_OBJECT(camera),
            "transfer-asynchronously", &transfer_async,
            NULL);

    /*
     * In case asynchronous transfer is requested, we start a new thread that
     * invokes the grab callback, otherwise nothing will be done here.
     */
    if (transfer_async) {
        GError *tmp_error = NULL;
        priv->thread_running = TRUE;
        priv->grab_thread = g_thread_create(kiro_grab_func, camera, TRUE, &tmp_error);

        if (tmp_error != NULL) {
            priv->thread_running = FALSE;
            g_propagate_error(error, tmp_error);
        }
    }
}

static void
uca_kiro_camera_stop_recording(UcaCamera *camera, GError **error)
{
    gboolean transfer_async = FALSE;
    UcaKiroCameraPrivate *priv;
    g_return_if_fail(UCA_IS_KIRO_CAMERA(camera));

    priv = UCA_KIRO_CAMERA_GET_PRIVATE(camera);
    g_free(priv->dummy_data);
    kiro_trb_purge(priv->receive_buffer, FALSE);

    g_object_get(G_OBJECT(camera),
            "transfer-asynchronously", &transfer_async,
            NULL);

    if (transfer_async) {
        priv->thread_running = FALSE;
        g_thread_join(priv->grab_thread);
    }
}

static void
uca_kiro_camera_trigger (UcaCamera *camera, GError **error)
{
}

static gboolean
uca_kiro_camera_grab (UcaCamera *camera, gpointer data, GError **error)
{
    g_return_val_if_fail (UCA_IS_KIRO_CAMERA(camera), FALSE);

    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (camera);

    kiro_client_sync(priv->kiro_receiver);
    kiro_trb_refresh(priv->receive_buffer);
    priv->current_frame++;
    //Element 0 is always about to be written, Element -1 is currently being written
    //Therefore, we take Element -2, to be sure this one is finished
    size_t index = kiro_trb_get_max_elements(priv->receive_buffer) - 2;
    g_memmove (data, kiro_trb_get_element(priv->receive_buffer, index), priv->roi_width * priv->roi_height);

    return TRUE;
}

static void
uca_kiro_camera_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail(UCA_IS_KIRO_CAMERA(object));
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_FRAMERATE:
            priv->frame_rate = g_value_get_float(value);
            break;
        case PROP_KIRO_TANGO_ADDRESS:
            priv->kiro_tango_address = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }
}

static void
uca_kiro_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE(object);

    g_print("KIRO GET PROPERTY\n");
    
    switch (property_id) {
        case PROP_NAME:
            g_value_set_string(value, "KIRO camera");
            break;
        case PROP_FRAMERATE:
            g_value_set_float(value, priv->frame_rate);
            break;
        case PROP_KIRO_ADDRESS:
            g_value_set_string(value, priv->kiro_address);
            break;
        case PROP_KIRO_PORT:
            g_value_set_uint(value, priv->kiro_port_uint);
            break;
        case PROP_KIRO_TANGO_ADDRESS:
            g_value_set_string(value, priv->kiro_tango_address);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
uca_kiro_camera_finalize(GObject *object)
{
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE(object);

    g_rand_free (priv->rand);

    if (priv->thread_running) {
        priv->thread_running = FALSE;
        g_thread_join(priv->grab_thread);
    }

    if (priv->kiro_receiver) {
        g_object_unref(priv->kiro_receiver);
        priv->kiro_receiver = NULL;
    }
    
    if (priv->receive_buffer) {
        g_object_unref(priv->receive_buffer);
        priv->receive_buffer = NULL;
    }

    if (priv->dummy_data) {
        g_free(priv->dummy_data);
        priv->dummy_data = NULL;
    }
    
    if (priv->kiro_dynamic_properties)
    {
        g_list_free(priv->kiro_dynamic_properties);
        priv->kiro_dynamic_properties = NULL;
    }
    
    g_free(priv->kiro_address);
    g_free(priv->kiro_port);
    g_free(priv->kiro_tango_address);

    G_OBJECT_CLASS(uca_kiro_camera_parent_class)->finalize(object);
}

static gboolean
ufo_kiro_camera_initable_init (GInitable *initable,
                               GCancellable *cancellable,
                               GError **error)
{
    g_return_val_if_fail (UCA_IS_KIRO_CAMERA (initable), FALSE);
    
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (UCA_KIRO_CAMERA (initable));
    if(priv->construction_error)
        return FALSE;
    
    return TRUE;
}

static void
uca_kiro_initable_iface_init (GInitableIface *iface)
{
    iface->init = ufo_kiro_camera_initable_init;
}

static void
uca_kiro_camera_constructed (GObject *object)
{
    //Initialization for the KIRO Server and TANGO Interface cloning is moved
    //here and done early!
    //We want to add dynamic properties and it is too late to do so in the
    //real initable part. Therefore, we do it here and 'remember' any errors
    //that occur and check them later in the initable part.
    
    UcaKiroCamera *self = UCA_KIRO_CAMERA (object);
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (self);
    
    GValue address = G_VALUE_INIT;
    g_value_init(&address, G_TYPE_STRING);
    uca_kiro_camera_get_property(object, PROP_KIRO_TANGO_ADDRESS, &address, NULL);
    gint address_not_none = g_strcmp0(g_value_get_string (&address), "NONE");
    if (0 == address_not_none)
    {
        g_print("kiro-tango-address was not set! Can not connect to server...\n");
        priv->construction_error = TRUE;
    }
    else
    {
        uca_kiro_camera_clone_interface(g_value_get_string (&address), self);
    }
    
    /*
    if (0 > kiro_client_connect(priv->kiro_receiver, g_value_get_string(&address), g_value_get_string(&port)))
    {
        g_print("Unable to connect to server at address: %s, port: %s\n", g_value_get_string(&address), g_value_get_string(&port));
        return FALSE;
    }
    */
    
    
    G_OBJECT_CLASS(uca_kiro_camera_parent_class)->constructed(object);    
}



static void
uca_kiro_camera_class_init(UcaKiroCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_kiro_camera_set_property;
    gobject_class->get_property = uca_kiro_camera_get_property;
    gobject_class->finalize = uca_kiro_camera_finalize;
    gobject_class->constructed = uca_kiro_camera_constructed;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_kiro_camera_start_recording;
    camera_class->stop_recording = uca_kiro_camera_stop_recording;
    camera_class->grab = uca_kiro_camera_grab;
    camera_class->trigger = uca_kiro_camera_trigger;

    for (guint i = 0; kiro_overrideables[i] != 0; i++)
        g_object_class_override_property(gobject_class, kiro_overrideables[i], uca_camera_props[kiro_overrideables[i]]);

    kiro_properties[PROP_FRAMERATE] =
        g_param_spec_float("frame-rate",
                "Frame rate",
                "Number of frames per second that are taken",
                1.0f, 100.0f, 100.0f,
                (GParamFlags) G_PARAM_READWRITE);
                
    kiro_properties[PROP_KIRO_ADDRESS] =
        g_param_spec_string("kiro-address",
                "KIRO Server Address",
                "Address of the KIRO Server to grab images from",
                "NONE",
                G_PARAM_READABLE);
                
    kiro_properties[PROP_KIRO_PORT] =
        g_param_spec_uint("kiro-port",
                "KIRO Server Port",
                "Port of the KIRO Server to grab images from",
                1, 65535, 60010,
                G_PARAM_READABLE);
                
    kiro_properties[PROP_KIRO_TANGO_ADDRESS] =
        g_param_spec_string("kiro-tango-address",
                "KIRO TANGO address",
                "Address of the KIRO Server in the TANGO environment",
                "NONE",
                (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property(gobject_class, id, kiro_properties[id]);

    g_type_class_add_private(klass, sizeof(UcaKiroCameraPrivate));
}

static void
uca_kiro_camera_init(UcaKiroCamera *self)
{
    self->priv = UCA_KIRO_CAMERA_GET_PRIVATE(self);
    self->priv->grab_thread = NULL;
    self->priv->current_frame = 0;
    self->priv->frame_rate = 100.0;
    self->priv->kiro_address = g_strdup("NONE");
    self->priv->kiro_port = g_strdup("NONE");
    self->priv->kiro_port_uint = 60010;
    self->priv->kiro_tango_address = g_strdup("NONE");
    self->priv->construction_error = FALSE;
    self->priv->kiro_dynamic_properties = NULL;

    self->priv->receive_buffer = KIRO_TRB (g_object_new(KIRO_TYPE_TRB, NULL));
    self->priv->kiro_receiver = KIRO_CLIENT (g_object_new(KIRO_TYPE_CLIENT, NULL));

    uca_camera_register_unit (UCA_CAMERA (self), "frame-rate", UCA_UNIT_COUNT);        
}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_KIRO_CAMERA;
}


GType
gtype_from_tango_type (Tango::CmdArgType t)
{
    using namespace Tango;
        switch (t) {
            case DEV_VOID:
                return G_TYPE_NONE;
            case DEV_BOOLEAN:
                return G_TYPE_BOOLEAN;
            case DEV_SHORT:
                //Fall-through intentional
            case DEV_INT:
                //Fall-through intentional
            case DEV_LONG:
                return G_TYPE_INT;
            case DEV_FLOAT:
                return G_TYPE_FLOAT;
            case DEV_DOUBLE:
                return G_TYPE_DOUBLE;
            case DEV_USHORT:
                return G_TYPE_UINT;
            case DEV_ULONG:
                return G_TYPE_ULONG;
            case CONST_DEV_STRING:
                //Fall-through intentional
            case DEV_STRING:
                return G_TYPE_STRING;
            case DEV_UCHAR:
                return G_TYPE_UCHAR;
            case DEV_LONG64:
                return G_TYPE_INT64;
            case DEV_ULONG64:
                return G_TYPE_UINT64;
            /*
            DEV_STATE
            DEV_ENCODED
            DEVVAR_CHARARRAY
            DEVVAR_SHORTARRAY
            DEVVAR_LONGARRAY
            DEVVAR_FLOATARRAY
            DEVVAR_DOUBLEARRAY
            DEVVAR_USHORTARRAY
            DEVVAR_ULONGARRAY
            DEVVAR_STRINGARRAY
            DEVVAR_LONGSTRINGARRAY
            DEVVAR_DOUBLESTRINGARRAY
            DEVVAR_BOOLEANARRAY
            DEVVAR_LONG64ARRAY
            DEVVAR_ULONG64ARRAY
            */
            default:
                return G_TYPE_INVALID;
        };
}



void 
uca_kiro_camera_clone_interface (const gchar* address, UcaKiroCamera *kiro_camera)
{
    g_print("%s\n", address);
       
    try
    {
        Tango::DeviceProxy *device = new Tango::DeviceProxy(address);
        vector<string> *list = device->get_attribute_list();
        
        for ( vector<string>::iterator iter = list->begin(); iter != list->end(); ++iter )
        {
            GType type = gtype_from_tango_type((Tango::CmdArgType)device->attribute_query(*iter).data_type);
            if (G_TYPE_INVALID != type)
                g_print("%s TYPE is: %u\n", (*iter).c_str(), (unsigned int)type);
            else
                g_print("%s TYPE is INVALID (TANGO Type: %u)\n", (*iter).c_str(), (unsigned int)device->attribute_query(*iter).data_type);
        }
        
        size_t properties = list->size();
        kiro_dynamics = new GParamSpec* [properties];
        
        
        
        
    }
    catch (Tango::DevFailed &e) 
    { 
        Tango::Except::print_exception(e); 
    }
}

