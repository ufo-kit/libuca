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

#define UCA_KIRO_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), UCA_TYPE_KIRO_CAMERA, UcaKiroCameraPrivate))

static void uca_kiro_initable_iface_init (GInitableIface *iface);
GError *initable_iface_error = NULL;

G_DEFINE_TYPE_WITH_CODE (UcaKiroCamera, uca_kiro_camera, UCA_TYPE_CAMERA,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                uca_kiro_initable_iface_init))


/**
 * UcaCameraError:
   @UCA_KIRO_CAMERA_ERROR_MISSING_TANGO_ADDRESS: No TANGO address ('kiro-tango-address') property was supplied during camera creation
   @UCA_KIRO_CAMERA_ERROR_TANGO_CONNECTION_FAILED: Could not connect to the given TANGO address
   @UCA_KIRO_CAMERA_ERROR_KIRO_CONNECTION_FAILED: Failed to establish a KIRO connection to the given TANGO server
   @UCA_KIRO_CAMERA_ERROR_TANGO_EXCEPTION_OCCURED: A TANGO exception was raised during communication with the server
   @UCA_KIRO_CAMERA_ERROR_BAD_CAMERA_INTERFACE: The given TANGO server does not expose the expected UcaCamera base interface
 */
GQuark uca_kiro_camera_error_quark()
{
    return g_quark_from_static_string ("uca-kiro-camera-error-quark");
}


enum {
    PROP_KIRO_ADDRESS = N_BASE_PROPERTIES,
    PROP_KIRO_PORT,
    PROP_KIRO_TANGO_ADDRESS,
    PROP_KIRO_REMOTE_NAME,
    N_PROPERTIES
};

static const gint kiro_overrideables[] = {
    PROP_NAME,
    0,
};

static GParamSpec *kiro_properties[N_PROPERTIES] = { NULL, };

struct _UcaKiroCameraPrivate {
    guint8 *dummy_data;
    guint current_frame;
    gchar *kiro_address;
    gchar *kiro_port;
    guint kiro_port_uint;
    gchar *kiro_tango_address;
    gchar *remote_name;
    Tango::DeviceProxy *tango_device;
    GParamSpec **kiro_dynamic_attributes;

    gboolean thread_running;
    gboolean kiro_connected;
    gboolean construction_error;

    GThread *grab_thread;
    KiroTrb *receive_buffer;
    KiroClient *kiro_receiver;

    guint roi_height;
    guint roi_width;
    guint bytes_per_pixel;
};

static gpointer
kiro_grab_func(gpointer data)
{
    UcaKiroCamera *kiro_camera = UCA_KIRO_CAMERA (data);
    g_return_val_if_fail (UCA_IS_KIRO_CAMERA (kiro_camera), NULL);

    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (kiro_camera);
    UcaCamera *camera = UCA_CAMERA (kiro_camera);
    gdouble fps;
    g_object_get (G_OBJECT (data), "frames-per-second", &fps, NULL);
    const gulong sleep_time = (gulong) G_USEC_PER_SEC / fps;

    while (priv->thread_running) {
        camera->grab_func (NULL, camera->user_data);
        g_usleep (sleep_time);
    }

    return NULL;
}

static void
uca_kiro_camera_start_recording(UcaCamera *camera, GError **error)
{
    gboolean transfer_async = FALSE;
    UcaKiroCameraPrivate *priv;
    g_return_if_fail(UCA_IS_KIRO_CAMERA (camera));

    priv = UCA_KIRO_CAMERA_GET_PRIVATE (camera);
    
    kiro_client_sync (priv->kiro_receiver);
    kiro_trb_adopt (priv->receive_buffer, kiro_client_get_memory (priv->kiro_receiver));
        
    g_object_get (G_OBJECT(camera),
            "transfer-asynchronously", &transfer_async,
            NULL);

    //'Cache' ROI settings from TANGO World
    g_object_get (G_OBJECT(camera),
            "roi-width", &priv->roi_width,
            NULL);
    g_object_get (G_OBJECT(camera),
            "roi-height", &priv->roi_height,
            NULL);

    size_t bits = 0;
    g_object_get (G_OBJECT(camera),
            "sensor-bitdepth", &bits,
            NULL);

    priv->bytes_per_pixel = 1;
    if (bits > 8) priv->bytes_per_pixel++;
    if (bits > 16) priv->bytes_per_pixel++;
    if (bits > 24) priv->bytes_per_pixel++;

    Tango::DevState state;
    g_object_get (G_OBJECT(camera),
            "State", &state,
            NULL);
    try {
        if (Tango::DevState::STANDBY == state)
            priv->tango_device->command_inout ("StartRecording");
    }
    catch (Tango::DevFailed &e) {
        g_warning ("Failed to execute 'StartRecording' on the remote camera due to a TANGO exception.\n");
        g_set_error (error, UCA_KIRO_CAMERA_ERROR, UCA_KIRO_CAMERA_ERROR_TANGO_EXCEPTION_OCCURED,
                     "A TANGO exception was raised: '%s'", (const char *)e.errors[0].desc);
        return;
    }


    /*
     * In case asynchronous transfer is requested, we start a new thread that
     * invokes the grab callback, otherwise nothing will be done here.
     */
    if (transfer_async) {
        GError *tmp_error = NULL;
        priv->thread_running = TRUE;
        priv->grab_thread = g_thread_create (kiro_grab_func, camera, TRUE, &tmp_error);

        if (tmp_error != NULL) {
            priv->thread_running = FALSE;
            g_propagate_error (error, tmp_error);
            try {
                priv->tango_device->command_inout ("StopRecording");
            }
            catch (Tango::DevFailed &e) {
                g_warning ("Failed to execute 'StopRecording' on the remote camera due to a TANGO exception: '%s'\n", (const char *)e.errors[0].desc);
            }
        }
    }
}

static void
uca_kiro_camera_stop_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_KIRO_CAMERA (camera));
    UcaKiroCameraPrivate *priv;
    priv = UCA_KIRO_CAMERA_GET_PRIVATE (camera);

    Tango::DevState state;
    g_object_get (G_OBJECT(camera),
            "State", &state,
            NULL);

    try {
        if (Tango::DevState::RUNNING == state)
            priv->tango_device->command_inout ("StopRecording");
    }
    catch (Tango::DevFailed &e) {
        g_warning ("Failed to execute 'StopRecording' on the remote camera due to a TANGO exception.\n");
        g_set_error (error, UCA_KIRO_CAMERA_ERROR, UCA_KIRO_CAMERA_ERROR_TANGO_EXCEPTION_OCCURED,
                     "A TANGO exception was raised: '%s'", (const char *)e.errors[0].desc);
    }

    gboolean transfer_async = FALSE;
    g_object_get(G_OBJECT (camera),
            "transfer-asynchronously", &transfer_async,
            NULL);

    if (transfer_async) {
        priv->thread_running = FALSE;
        g_thread_join (priv->grab_thread);
    }

    g_free (priv->dummy_data);
    kiro_trb_purge(priv->receive_buffer, FALSE);
}

static void
uca_kiro_camera_trigger (UcaCamera *camera, GError **error)
{
}

static gboolean
uca_kiro_camera_grab (UcaCamera *camera, gpointer data, GError **error)
{
    g_return_val_if_fail (UCA_IS_KIRO_CAMERA (camera), FALSE);

    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (camera);

    kiro_client_sync (priv->kiro_receiver);
    kiro_trb_refresh (priv->receive_buffer);
    priv->current_frame++;

    //Element 0 is always about to be written, Element -1 is currently being written
    //Therefore, we take Element -2, to be sure this one is finished
    size_t index = kiro_trb_get_max_elements (priv->receive_buffer) - 2;
    if (data)
        g_memmove (data, kiro_trb_get_element(priv->receive_buffer, index), priv->roi_width * priv->roi_height * priv->bytes_per_pixel);

    return TRUE;
}



// ---------------------------------------------------------- //
//                      TANGO <-> GLib                        //
// ---------------------------------------------------------- //
gboolean
unpack_gvaluearray_from_tango (GValue *value, Tango::DeviceAttribute &t_attr, GParamSpec *pspec)
{
    GType value_type = ((GParamSpecValueArray*)pspec)->element_spec->value_type;
    if (G_TYPE_UINT != value_type) {
        g_print ("Array type attribue '%s' holds elements of type '%s' which can't be handled.\n", pspec->name, g_type_name (value_type) );
        return FALSE;
    }

    guint array_length = t_attr.get_dim_x ();
    GValueArray *gvalarray = g_value_array_new (array_length);

    //Convenience Macro to prevent either having to rewrite this block
    //of code over and over again, or creating two almost identical
    // switch-cases...
    //UNPACK TANGO GVALUEARRAY
    #define __U_T_GVA(__GTYPE, __GFUNC) { \
        vector<__GTYPE> t_vect; \
        t_attr >> t_vect; \
        for (guint idx = 0; idx < array_length; idx++) { \
            g_value_array_append (gvalarray, NULL); \
            GValue *val = g_value_array_get_nth (gvalarray, idx); \
            g_value_init (val, value_type); \
            __GFUNC (val, t_vect[idx]); \
        } \
    }

    switch (value_type) {
        case G_TYPE_BOOLEAN:
            //We need to to this manualy since there is no implicit conversion from c++ bool to gboolean
            {
                vector<bool> t_vect;
                t_attr >> t_vect;
                for (guint idx = 0; idx < array_length; idx++) {
                    g_value_array_append (gvalarray, NULL);
                    GValue *val = g_value_array_get_nth (gvalarray, idx);
                    g_value_init (val, value_type);
                    g_value_set_boolean (val, (t_vect[idx] ? TRUE : FALSE));
                }
            }
            break;
        case G_TYPE_STRING:
            //We need to to this manualy since there is no implicit conversion from c++ string to gchar*
            {
                vector<string> t_vect;
                t_attr >> t_vect;
                for (guint idx = 0; idx < array_length; idx++) {
                    g_value_array_append (gvalarray, NULL);
                    GValue *val = g_value_array_get_nth (gvalarray, idx);
                    g_value_init (val, value_type);
                    g_value_set_string (val, t_vect[idx].c_str ());
                }
            }
            break;
        case G_TYPE_UCHAR:
            __U_T_GVA (guchar, g_value_set_uchar);
            break;
        case G_TYPE_INT:
            __U_T_GVA (gint, g_value_set_int);
            break;
        case G_TYPE_UINT:
            __U_T_GVA (guint, g_value_set_uint);
            break;
        case G_TYPE_INT64:
            __U_T_GVA (gint64, g_value_set_int64);
            break;
        case G_TYPE_UINT64:
            __U_T_GVA (guint64, g_value_set_uint64);
            break;
        case G_TYPE_LONG:
            __U_T_GVA (glong, g_value_set_long);
            break;
        case G_TYPE_ULONG:
            __U_T_GVA (gulong, g_value_set_ulong);
            break;
        case G_TYPE_FLOAT:
            __U_T_GVA (gfloat, g_value_set_float);
            break;
        case G_TYPE_DOUBLE:
            __U_T_GVA (gdouble, g_value_set_double);
            break;
        default:
            g_print ("Array type attribue '%s' holds elements of type '%s' which can't be handled.\n", pspec->name, g_type_name (value_type) );
            return FALSE;
    }

    g_value_set_boxed_take_ownership (value, gvalarray);
    return TRUE;
}


void
try_handle_read_tango_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    Tango::DeviceAttribute t_attr;
    UcaKiroCamera *camera = UCA_KIRO_CAMERA (object);
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (camera);

    bool property_is_handled = (property_id >= N_PROPERTIES) ? (bool)priv->kiro_dynamic_attributes[property_id] : (bool)kiro_properties[property_id];

    if (property_is_handled) {
        try {
            priv->tango_device->read_attribute (pspec->name, t_attr);
        }
        catch (Tango::DevFailed &e) {
            g_warning ("Property '%s' could not be read due to an unexpected TANGO error...\n", pspec->name);
            Tango::Except::print_exception (e);
            return;
        }

        //Stupid workaround for TANGO::State attribute...
        //Because they just HAD to make a special case
        //for that one specific Enum...
        if (0 == g_strcmp0 (pspec->name, "State")) {
            Tango::DevState state;
            t_attr >> state;
            g_value_set_uint (value, (unsigned int)state);
            return;
        }

        //Convenience Macro to prevent having to write this block
        //of code over and over again
        #define T_TO_G_CONVERT(GTYPE, T_ATTR, FUNCTION, TARGET) { \
            GTYPE t_val; \
            T_ATTR >> t_val; \
            FUNCTION (TARGET, t_val); \
        }

        switch (value->g_type) {
        case G_TYPE_INT:
            T_TO_G_CONVERT (gint, t_attr, g_value_set_int, value);
            break;
        case G_TYPE_FLOAT:
            T_TO_G_CONVERT (gfloat, t_attr, g_value_set_float, value);
            break;
        case G_TYPE_DOUBLE:
            T_TO_G_CONVERT (gdouble, t_attr, g_value_set_double, value);
            break;
        case G_TYPE_UINT:
            T_TO_G_CONVERT (guint, t_attr, g_value_set_uint, value);
            break;
        case G_TYPE_ULONG:
            T_TO_G_CONVERT (gulong, t_attr, g_value_set_ulong, value);
            break;
        case G_TYPE_UCHAR:
            T_TO_G_CONVERT (guchar, t_attr, g_value_set_uchar, value);
            break;
        case G_TYPE_INT64:
            T_TO_G_CONVERT (gint64, t_attr, g_value_set_int64, value);
            break;
        case G_TYPE_UINT64:
            T_TO_G_CONVERT (guint64, t_attr, g_value_set_uint64, value);
            break;
        case G_TYPE_LONG:
            T_TO_G_CONVERT (glong, t_attr, g_value_set_long, value);
            break;
        case G_TYPE_BOOLEAN:
            {
                bool t_val;
                t_attr >> t_val;
                g_value_set_boolean (value, (t_val ? TRUE : FALSE));
            }
            break;
        case G_TYPE_STRING:
            {
                string t_val;
                t_attr >> t_val;
                g_value_set_string (value, t_val.c_str ());
            }
            break;
        default:
            {
                if (g_type_parent (value->g_type) == G_TYPE_ENUM) {
                    T_TO_G_CONVERT (gint, t_attr, g_value_set_enum, value);
                    break;
                }

                if (G_TYPE_VALUE_ARRAY == value->g_type) {
                    if (Tango::AttrDataFormat::SPECTRUM != t_attr.get_data_format ()) {
                        g_warning ("TANGO attribute '%s' is not of type SPECTRUM! (Not a 1-dimensional array, yet libuca was expecting one.)\n", pspec->name);
                        return;
                    }

                    if (0 == unpack_gvaluearray_from_tango (value, t_attr, pspec))
                        g_warning ("Failed to read property '%s'\n", pspec->name);

                    return;
                }

                GType unhandled = pspec->value_type;
                if (G_TYPE_GTYPE == unhandled) {
                    unhandled = ((GParamSpecGType*)pspec)->is_a_type;
                }
                g_print ("GType '%s' can't be handled...\n", g_type_name (unhandled));
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            }
        }
    }
    else {
        g_print ("Unhandled property...\n");
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }

#undef T_TO_G_CONVERT
}


gboolean
pack_gvaluearray_to_tango (const GValue *value, Tango::DeviceAttribute &t_attr, GParamSpec *pspec)
{

    GType value_type = ((GParamSpecValueArray*)pspec)->element_spec->value_type;
    GValueArray *gvalarray = (GValueArray *) g_value_get_boxed (value);
    guint array_length = gvalarray->n_values;

    //Convenience Macro to prevent either having to rewrite this block
    //of code over and over again, or creating two almost identical
    // switch-cases...
    #define __P_GVA_T(__GTYPE, __GFUNC) { \
        vector<__GTYPE> t_vect (array_length); \
        for (guint idx = 0; idx < array_length; idx++) { \
            GValue *val = g_value_array_get_nth (gvalarray, idx); \
            t_vect[idx] = __GFUNC (val); \
        } \
        t_attr << t_vect; \
    }

    switch (value_type) {
        case G_TYPE_BOOLEAN:
            __P_GVA_T (bool, g_value_get_boolean); //This relys on the implicit conversion from int to c++ bool
            break;
        case G_TYPE_UCHAR:
            __P_GVA_T (guchar, g_value_get_uchar);
            break;
        case G_TYPE_STRING:
            __P_GVA_T (string, g_value_get_string); //This relys on the implicit conversion from char* to c++ string
            break;
        case G_TYPE_INT:
            __P_GVA_T (gint, g_value_get_int);
            break;
        case G_TYPE_UINT:
            __P_GVA_T (guint, g_value_get_uint);
            break;
        case G_TYPE_INT64:
            __P_GVA_T (gint64, g_value_get_int64);
            break;
        case G_TYPE_UINT64:
            __P_GVA_T (guint64, g_value_get_uint64);
            break;
        case G_TYPE_LONG:
            __P_GVA_T (glong, g_value_get_long);
            break;
        case G_TYPE_ULONG:
            __P_GVA_T (gulong, g_value_get_ulong);
            break;
        case G_TYPE_FLOAT:
            __P_GVA_T (gfloat, g_value_get_float);
            break;
        case G_TYPE_DOUBLE:
            __P_GVA_T (gdouble, g_value_get_double);
            break;
        default:
            g_print ("Array type attribue '%s' holds elements of type '%s' which can't be handled.\n", pspec->name, g_type_name (value_type) );
            return FALSE;
    }

    t_attr.data_format = Tango::AttrDataFormat::SPECTRUM;
    t_attr.dim_x = array_length;
    return TRUE;

#undef __P_GVA_T
}


void
try_handle_write_tango_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    Tango::DeviceAttribute t_attr;
    UcaKiroCamera *camera = UCA_KIRO_CAMERA (object);
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (camera);

    bool property_is_handled = (property_id > N_PROPERTIES) ? (bool)priv->kiro_dynamic_attributes[property_id] : (bool)kiro_properties[property_id];

    if (property_is_handled) {
        switch (value->g_type) {
            case G_TYPE_BOOLEAN:
                {
                    bool t_val = (g_value_get_boolean (value) == TRUE);
                    t_attr << t_val;
                }
                break;
            case G_TYPE_INT:
                t_attr << g_value_get_int (value);
                break;
            case G_TYPE_FLOAT:
                t_attr << g_value_get_float (value);
                break;
            case G_TYPE_DOUBLE:
                t_attr << g_value_get_double (value);
                break;
            case G_TYPE_UINT:
                t_attr << g_value_get_uint (value);
                break;
            case G_TYPE_ULONG:
                t_attr << g_value_get_ulong (value);
                break;
            case G_TYPE_STRING:
                t_attr << g_value_get_string (value);
                break;
            case G_TYPE_UCHAR:
                t_attr << g_value_get_uchar (value);
                break;
            case G_TYPE_INT64:
                t_attr << g_value_get_int64 (value);
                break;
            case G_TYPE_UINT64:
                t_attr << g_value_get_uint64 (value);
                break;
            case G_TYPE_LONG:
                t_attr << g_value_get_long (value);
                break;
            case G_TYPE_ENUM:
                t_attr << g_value_get_enum (value);
                break;
            default:
                {
                    if (g_type_parent (value->g_type) == G_TYPE_ENUM) {
                        t_attr << g_value_get_enum (value);
                        break;
                    }

                    if (value->g_type == G_TYPE_VALUE_ARRAY) {
                        if (0 == pack_gvaluearray_to_tango (value, t_attr, pspec)) {
                            g_warning ("Failed to write property '%s'.\n", pspec->name);
                            return;
                        }
                        break;
                    }

                    GType unhandled = value->g_type;
                    if (G_TYPE_GTYPE == unhandled) {
                        unhandled = ((GParamSpecGType*)pspec)->is_a_type;
                    }
                    g_print ("GType '%s' can't be handled...\n", g_type_name (unhandled));
                    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                }
                break;
        }

        t_attr.set_name (pspec->name);

        try {
            priv->tango_device->write_attribute (t_attr);
        }
        catch (Tango::DevFailed &e) {
            g_warning ("Property '%s' could not be written due to a TANGO exception: '%s'\n", pspec->name, (const char *)e.errors[0].desc);
            Tango::Except::print_exception (e);
            return;
        }
    }
    else
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
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
            case DEV_ULONG:
                //return G_TYPE_ULONG;
                //Fall-through intentional
                //NOTE: There seems to be a bug somewhere either in TANGO or in GLib or in Pyhton that
                //Breaks the functionality of the G_TYPE_ULONG properties. Using a G_TYPE_UINT instead
                //works around this problem but might provoke potential overflows...
            case DEV_USHORT:
                return G_TYPE_UINT;
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
            case DEV_STATE:
                return G_TYPE_UINT;
            /*
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


gint
get_property_id_from_name(const gchar* name)
{
    guint idx = 0;
    gboolean found = FALSE;
    for (;idx < N_PROPERTIES; ++idx) {
        if (0 == g_strcmp0(name, uca_camera_props[idx])) {
            found = TRUE;
            break;
        }
    }
    return (TRUE == found) ? idx : -1;
}


void
build_param_spec(GParamSpec **pspec, const Tango::AttributeInfoEx *attrInfo)
{
    GType type = gtype_from_tango_type ((Tango::CmdArgType)attrInfo->data_type);
    const gchar *name = attrInfo->name.c_str ();
    GParamFlags flags = G_PARAM_READABLE;
    if (attrInfo->writable == Tango::AttrWriteType::WRITE)
        flags = (GParamFlags) G_PARAM_READWRITE;


    //Convenience Macro to prevent having to rewrite this block
    //of code over and over again..
    #define __M_PSCPEC(__SPEC_TYPE, __LIMITS_1, __LIMITS_2, __LIMITS_3) { \
        *pspec = \
        __SPEC_TYPE (name, \
            attrInfo->description.c_str (), \
            g_strconcat ("KIRO TANGO <-> GLib interface of ", name, NULL), \
            __LIMITS_1, __LIMITS_2, __LIMITS_3,\
            flags); \
    }

    switch (type) {
        case G_TYPE_INT:
            __M_PSCPEC (g_param_spec_int, G_MININT32, G_MAXINT32, 0);
            break;
        case G_TYPE_FLOAT:
            __M_PSCPEC (g_param_spec_float, G_MINFLOAT, G_MAXFLOAT, 0.);
            break;
        case G_TYPE_DOUBLE:
            __M_PSCPEC (g_param_spec_double, G_MINDOUBLE, G_MAXDOUBLE, 0.)
            break;
        case G_TYPE_UINT:
            __M_PSCPEC (g_param_spec_uint, 0, G_MAXUINT, 0)
            break;
        case G_TYPE_ULONG:
            __M_PSCPEC (g_param_spec_ulong, 0, G_MAXULONG, 0)
            break;
        case G_TYPE_UCHAR:
            __M_PSCPEC (g_param_spec_uchar, 0x00, 0xff, 0x42)
            break;
        case G_TYPE_INT64:
            __M_PSCPEC (g_param_spec_int64, G_MININT64, G_MAXINT64, 0)
            break;
        case G_TYPE_UINT64:
            __M_PSCPEC (g_param_spec_uint64, 0, G_MAXUINT64, 0)
            break;
        case G_TYPE_LONG:
            __M_PSCPEC (g_param_spec_long, G_MININT64, G_MAXINT64, 1)
            break;
        case G_TYPE_ENUM:
            __M_PSCPEC (g_param_spec_int, 0, G_MAXUINT, 0)
            break;
        case G_TYPE_STRING:
            *pspec =
            g_param_spec_string (name,
                attrInfo->description.c_str (),
                g_strconcat ("KIRO TANGO <-> GLib interface of ", name, NULL),
                "DEFAULT",
                flags);
            break;
        case G_TYPE_BOOLEAN:
            *pspec =
            g_param_spec_boolean (name,
                attrInfo->description.c_str (),
                g_strconcat ("KIRO TANGO <-> GLib interface of ", name, NULL),
                FALSE,
                flags);
            break;
        default:
            *pspec =
            g_param_spec_gtype (name,
                attrInfo->description.c_str (),
                g_strconcat ("KIRO TANGO <-> GLib interface of ", name, NULL),
                type,
                flags);
    }

#undef __M_PSCPEC
}


void
uca_kiro_camera_clone_interface(const gchar* address, UcaKiroCamera *kiro_camera)
{
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (kiro_camera);
    UcaKiroCameraClass *klass = UCA_KIRO_CAMERA_GET_CLASS (kiro_camera);
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gboolean start_found, stop_found, unit_found = FALSE;

    try {
        Tango::CommandInfoList *cmd_list = priv->tango_device->command_list_query ();
        for (vector<Tango::CommandInfo>::iterator iter = cmd_list->begin (); iter != cmd_list->end (); ++iter) {
            gint start_cmp = g_strcmp0((*iter).cmd_name.c_str (), "StartRecording");
            if (0 == start_cmp) {
                start_found = TRUE;
            }
            gint stop_cmp = g_strcmp0 ((*iter).cmd_name.c_str (), "StopRecording");
            if (0 == stop_cmp) {
                stop_found = TRUE;
            }
            gint unit_cmp = g_strcmp0((*iter).cmd_name.c_str (), "GetAttributeUnit");
            if (0 == unit_cmp) {
                unit_found = TRUE;
            }
        }

        if ( !start_found || !stop_found ) {
            g_warning ("The Server at '%s' does not provide the necessary 'StartRecording' and 'StopRecording' interface\n", priv->kiro_tango_address);
            g_set_error (&initable_iface_error, UCA_KIRO_CAMERA_ERROR, UCA_KIRO_CAMERA_ERROR_BAD_CAMERA_INTERFACE,
                         "The Server at '%s' does not provide the necessary 'StartRecording' and 'StopRecording' interface\n", priv->kiro_tango_address);
            priv->construction_error = TRUE;
            return;
        }

        vector<string> *attr_list = priv->tango_device->get_attribute_list ();
        GList *non_base_attributes = NULL;
        guint non_base_attributes_count = 0;

        for (vector<string>::iterator iter = attr_list->begin (); iter != attr_list->end (); ++iter) {
            Tango::AttributeInfoEx attrInfo =  priv->tango_device->attribute_query (*iter);
            gint uca_base_prop_id = get_property_id_from_name ((*iter).c_str ());
            if (-1 < uca_base_prop_id) {
                guint is_name_attr = g_strcmp0 ((*iter).c_str (), "name");
                if (0 == is_name_attr) {
                    Tango::DeviceAttribute t_attr;
                    priv->tango_device->read_attribute ("name", t_attr);
                    string reply_name;
                    t_attr >> reply_name;
                    g_free (priv->remote_name);
                    priv->remote_name = g_strdup (reply_name.c_str ());
                }
                kiro_properties[uca_base_prop_id] = g_object_class_find_property (gobject_class, uca_camera_props[uca_base_prop_id]);
                g_object_class_override_property(G_OBJECT_CLASS (UCA_KIRO_CAMERA_GET_CLASS (kiro_camera)), uca_base_prop_id, uca_camera_props[uca_base_prop_id]);
            }
            else {
                non_base_attributes = g_list_append (non_base_attributes, (gpointer)(*iter).c_str ());
                non_base_attributes_count++;
            }
        }

        if (non_base_attributes_count > 0) {
            priv->kiro_dynamic_attributes = new GParamSpec* [N_PROPERTIES + non_base_attributes_count];
            UcaKiroCameraClass *klass = UCA_KIRO_CAMERA_GET_CLASS (kiro_camera);
            GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

            for (guint idx = 0; idx < non_base_attributes_count; idx++) {
                const gchar *attr_name = (const gchar*)g_list_nth_data (non_base_attributes, idx);
                Tango::AttributeInfoEx attrInfo = priv->tango_device->attribute_query (string(attr_name));
                
                if (Tango::AttrDataFormat::IMAGE == attrInfo.data_format || Tango::AttrDataFormat::FMT_UNKNOWN == attrInfo.data_format) {
                    g_print ("Attribute '%s' has unknown DataFormat. Skipping.\n", attr_name);
                    continue;
                }
                
                build_param_spec (&(priv->kiro_dynamic_attributes[N_PROPERTIES + idx]), &attrInfo);
                g_object_class_install_property (gobject_class, N_PROPERTIES + idx, priv->kiro_dynamic_attributes[N_PROPERTIES + idx]);

                if (unit_found) {
                    Tango::DeviceData arg_name;
                    arg_name << attr_name;
                    Tango::DeviceData cmd_reply = priv->tango_device->command_inout("GetAttributeUnit", arg_name);
                    gint unit;
                    cmd_reply >> unit;
                    uca_camera_register_unit (UCA_CAMERA (kiro_camera), attr_name, (UcaUnit)unit);
                }
            }
        }
    }
    catch (Tango::DevFailed &e) {
        Tango::Except::print_exception (e);
        g_set_error (&initable_iface_error, UCA_KIRO_CAMERA_ERROR, UCA_KIRO_CAMERA_ERROR_TANGO_EXCEPTION_OCCURED,
                     "A TANGO exception was raised: '%s'", (const char *)e.errors[0].desc);
        priv->construction_error = TRUE;
    }
}
// ---------------------------------------------------------- //
//                  END: TANGO <-> GLib                       //
// ---------------------------------------------------------- //



static void
uca_kiro_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail(UCA_IS_KIRO_CAMERA (object));
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_KIRO_TANGO_ADDRESS:
            priv->kiro_tango_address = g_value_dup_string (value);
            break;
        default:
            try_handle_write_tango_property (object, property_id, value, pspec);
            return;
    }
}


static void
uca_kiro_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NAME:
            g_value_set_string (value, "KIRO camera");
            break;
        case PROP_KIRO_ADDRESS:
            g_value_set_string (value, priv->kiro_address);
            break;
        case PROP_KIRO_PORT:
            g_value_set_uint (value, priv->kiro_port_uint);
            break;
        case PROP_KIRO_TANGO_ADDRESS:
            g_value_set_string (value, priv->kiro_tango_address);
            break;
        case PROP_KIRO_REMOTE_NAME:
            g_value_set_string (value, priv->remote_name);
            break;
        default:
            try_handle_read_tango_property (object, property_id, value, pspec);
            break;
    }
}

static void
uca_kiro_camera_finalize(GObject *object)
{
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE(object);

    if (priv->thread_running) {
        priv->thread_running = FALSE;
        g_thread_join (priv->grab_thread);
    }

    if (priv->kiro_receiver) {
        g_object_unref (priv->kiro_receiver);
        priv->kiro_receiver = NULL;
    }
    priv->kiro_connected = FALSE;

    if (priv->receive_buffer) {
        g_object_unref (priv->receive_buffer);
        priv->receive_buffer = NULL;
    }

    if (priv->dummy_data) {
        g_free (priv->dummy_data);
        priv->dummy_data = NULL;
    }

    if (priv->tango_device) {
        delete (priv->tango_device);
        priv->tango_device = NULL;
    }

    g_free (priv->kiro_address);
    g_free (priv->kiro_port);
    g_free (priv->kiro_tango_address);

    G_OBJECT_CLASS (uca_kiro_camera_parent_class)->finalize(object);
}

static gboolean
ufo_kiro_camera_initable_init (GInitable *initable,
                               GCancellable *cancellable,
                               GError **error)
{
    g_return_val_if_fail (UCA_IS_KIRO_CAMERA (initable), FALSE);
    
    UcaKiroCameraPrivate *priv = UCA_KIRO_CAMERA_GET_PRIVATE (UCA_KIRO_CAMERA (initable));
    if(priv->construction_error) {
        g_propagate_error (error, initable_iface_error);
        return FALSE;
    }
    
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
    uca_kiro_camera_get_property (object, PROP_KIRO_TANGO_ADDRESS, &address, NULL);
    gint address_not_none = g_strcmp0(g_value_get_string (&address), "NONE");
    if (0 == address_not_none) {
        g_warning ("kiro-tango-address was not set! Can not connect to server...\n");
        priv->construction_error = TRUE;
        g_set_error (&initable_iface_error, UCA_KIRO_CAMERA_ERROR, UCA_KIRO_CAMERA_ERROR_MISSING_TANGO_ADDRESS,
             "'kiro-tango-address' property was not set during construction.");
    }
    else {
        try {
            priv->tango_device = new Tango::DeviceProxy(g_value_get_string (&address));
            Tango::DbData kiro_credentials;
            kiro_credentials.push_back (Tango::DbDatum("LocalAddress"));
            kiro_credentials.push_back (Tango::DbDatum("LocalPort"));
            priv->tango_device->get_property(kiro_credentials);
            string kiro_address, kiro_port;
            kiro_credentials[0] >> kiro_address;
            kiro_credentials[1] >> kiro_port;

            if (0 > kiro_client_connect(priv->kiro_receiver, kiro_address.c_str (), kiro_port.c_str ())) {
                g_warning ("Unable to connect to server at address: %s, port: %s\n", kiro_address.c_str (), kiro_port.c_str ());
                priv->construction_error = TRUE;
                g_set_error (&initable_iface_error, UCA_KIRO_CAMERA_ERROR, UCA_KIRO_CAMERA_ERROR_KIRO_CONNECTION_FAILED,
                             "Failed to establish a KIRO InfiniBand connection.");
            }
            else {
                priv->kiro_connected = TRUE;
                uca_kiro_camera_clone_interface (g_value_get_string (&address), self);
            }
        }
        catch (Tango::DevFailed &e) {
            Tango::Except::print_exception (e);
            g_set_error (&initable_iface_error, UCA_KIRO_CAMERA_ERROR, UCA_KIRO_CAMERA_ERROR_TANGO_EXCEPTION_OCCURED,
                             "A TANGO exception was raised: '%s'", (const char *)e.errors[0].desc);
            priv->construction_error = TRUE;
        }
    }

    G_OBJECT_CLASS (uca_kiro_camera_parent_class)->constructed(object);
}



static void
uca_kiro_camera_class_init(UcaKiroCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS( klass);
    gobject_class->set_property = uca_kiro_camera_set_property;
    gobject_class->get_property = uca_kiro_camera_get_property;
    gobject_class->finalize = uca_kiro_camera_finalize;
    gobject_class->constructed = uca_kiro_camera_constructed;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS (klass);
    camera_class->start_recording = uca_kiro_camera_start_recording;
    camera_class->stop_recording = uca_kiro_camera_stop_recording;
    camera_class->grab = uca_kiro_camera_grab;
    camera_class->trigger = uca_kiro_camera_trigger;

    for (guint i = 0; kiro_overrideables[i] != 0; i++)
        g_object_class_override_property (gobject_class, kiro_overrideables[i], uca_camera_props[kiro_overrideables[i]]);
                
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

    kiro_properties[PROP_KIRO_REMOTE_NAME] =
        g_param_spec_string("remote-name",
                "Name of the remot camera",
                "Name of the camera plugin that is loaded on the KIRO remote site",
                "NONE",
                G_PARAM_READABLE);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property (gobject_class, id, kiro_properties[id]);

    g_type_class_add_private (klass, sizeof(UcaKiroCameraPrivate));
}

static void
uca_kiro_camera_init(UcaKiroCamera *self)
{
    self->priv = UCA_KIRO_CAMERA_GET_PRIVATE(self);
    self->priv->grab_thread = NULL;
    self->priv->current_frame = 0;
    self->priv->kiro_address = g_strdup ("NONE");
    self->priv->kiro_port = g_strdup ("NONE");
    self->priv->remote_name = g_strdup ("NONE");
    self->priv->kiro_port_uint = 60010;
    self->priv->kiro_tango_address = g_strdup ("NONE");
    self->priv->construction_error = FALSE;
    self->priv->kiro_dynamic_attributes = NULL;

    self->priv->receive_buffer = KIRO_TRB (g_object_new (KIRO_TYPE_TRB, NULL));
    self->priv->kiro_receiver = KIRO_CLIENT (g_object_new (KIRO_TYPE_CLIENT, NULL));
}


G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_KIRO_CAMERA;
}

