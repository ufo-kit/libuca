/* AUTHOR: Sven Werchner
 * DATE: 06.12.2013
 *
 */

#include <glib-object.h>
#include <gio/gio.h>
#include <gmodule.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "atcore.h"
#include "uca-camera.h"
#include "uca-andor-camera.h"

#define UCA_ANDOR_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_ANDOR_CAMERA, UcaAndorCameraPrivate))

static void uca_andor_initable_iface_init(GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UcaAndorCamera, uca_andor_camera, UCA_TYPE_CAMERA, G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE, uca_andor_initable_iface_init))

#define TIMEBASE_INVALID 0xDEAD

#define ANDOR_READ 0
#define ANDOR_WRITE 1
#define PROPERTY_INDEPENDENT -10

struct _UcaAndorCameraPrivate {
    guint camera_number;
    AT_H camera_handle; 
    GError *construct_error;

    //properties
    gchar* name;
    gchar* model;
    guint aoi_left; 
    guint aoi_top; 
    guint aoi_width;
    guint aoi_height;
    guint aoi_stride;
    guint sensor_width;
    guint sensor_height;
    double pixel_width;
    double pixel_height;
    gint trigger_mode;
    gfloat frame_rate;
    gfloat max_frame_rate;
    gdouble exp_time;
    double sensor_temperature;
    double target_sensor_temperature;
    gint fan_speed;
    gchar* cycle_mode;
    gboolean is_sim_cam;
    gboolean is_cam_acquiring;

    AT_U8* image_buffer;
    AT_64 image_size_bytes;

    int rand;
};

/*
 *  Andor uses the phrase "Area of Interest" (AOI) instead of "Region of Interest (ROI)
 */
static gint andor_overrideables [] = {
    PROP_NAME,
    PROP_EXPOSURE_TIME,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH, 
    PROP_ROI_HEIGHT,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_PIXEL_WIDTH,
    PROP_SENSOR_PIXEL_HEIGHT,
    PROP_TRIGGER_MODE,
    PROP_IS_RECORDING,
    PROP_SENSOR_BITDEPTH,
    PROP_SENSOR_MAX_FRAME_RATE,
    PROP_HAS_CAMRAM_RECORDING,
    PROP_HAS_STREAMING,
    0,
};

/*
 *  Additional andor-camera properties
 */
enum {
    PROP_ROI_STRIDE = N_BASE_PROPERTIES,
    PROP_SENSOR_TEMPERATURE,
    PROP_TARGET_SENSOR_TEMPERATURE,
    PROP_FAN_SPEED,
    PROP_CYCLE_MODE,
    PROP_FRAMERATE,
    N_PROPERTIES
};  
static GParamSpec *andor_properties [N_PROPERTIES] = { NULL, };

/*
 * UcaAndorRecordMode:
 * @UCA_ANDOR_CAMERA_RECORD_MODE_???
 *
 */

GQuark uca_andor_camera_error_quark () {
    return g_quark_from_static_string ("uca-andor-camera-error-quark");
}

gboolean andorCommunicationSuccessful (int iErr, char* errorIdentifier, GError **error) {
    if (iErr != AT_SUCCESS) {
	*error = NULL;
	GError *temp_error = g_malloc0 (sizeof (GError));
	temp_error = g_error_new (uca_andor_camera_error_quark (), UCA_ANDOR_CAMERA_ERROR_LIBANDOR_GENERAL, "ERROR: During the communication with the AndorSDK ('%s') an error occurred (Code: %i).\n", errorIdentifier, iErr);
	g_propagate_error (error, temp_error);
	return FALSE;
    }
    return TRUE;
}

static void andor_error_invalid_access_mode (guint property_id, int used_mode, UcaAndorCameraPrivate *priv) {
    GError *temp_error = g_malloc0 (sizeof (GError));
    if (used_mode != ANDOR_WRITE && used_mode != ANDOR_READ) {
	temp_error = g_error_new (uca_andor_camera_error_quark (), UCA_ANDOR_CAMERA_ERROR_INVALID_MODE, "ERROR: Invalid access mode is being used. (used mode: %i - Valid Modes: %i, %i)\n", used_mode, ANDOR_WRITE, ANDOR_READ);
    }
    else {
	temp_error = g_error_new (uca_andor_camera_error_quark (), UCA_ANDOR_CAMERA_ERROR_MODE_NOT_AVAILABLE, "ERROR: %s is not available for %s (mode: %i) access.\n", andor_properties[property_id]->name, (used_mode==ANDOR_WRITE)?"writing":"reading", used_mode);
    }
    priv->construct_error = NULL;
    g_propagate_error(&(priv->construct_error), temp_error);	
    
}

/*
 * Addressing an Integer-Parameter from ANDOR
 */
static void address_integer_value (UcaAndorCameraPrivate *priv, AT_WC* feature, int access_mode, int* value) {
    GError **error = &(priv->construct_error);
    AT_64 temp = 0;
    int err = 0;
    if (access_mode == ANDOR_WRITE) {
	err = AT_SetInt (priv->camera_handle, feature, (AT_64) value);
    }
    else if (access_mode == ANDOR_READ) {
	err = AT_GetInt (priv->camera_handle, feature, &temp);
	if (err != AT_SUCCESS) g_print("%i\n",err);
	*value = (int) temp;
    }
    else {
	andor_error_invalid_access_mode (PROPERTY_INDEPENDENT, access_mode, priv);
	return;
    }

    andorCommunicationSuccessful (err, (char*) feature, error);
    	
    return;
}

/*
 * Addressing a double/float-Parameter from ANDOR
 */
static void address_double_value (UcaAndorCameraPrivate *priv, AT_WC* feature, int access_mode, double* value) {
    GError **error = &(priv->construct_error);
    double temp = 0.0;
    int err = 0;
    if (access_mode == ANDOR_WRITE) {
	err = AT_SetFloat (priv->camera_handle, feature, *value);
    }
    else if (access_mode == ANDOR_READ) {
	err = AT_GetFloat (priv->camera_handle, feature, &temp);
	*value = temp;
    }
    else {
	andor_error_invalid_access_mode (PROPERTY_INDEPENDENT, access_mode, priv);
	return;
    }

    andorCommunicationSuccessful (err, (char*) feature, error);

    return;
}

/*
 * Addressing an Enumerated-Parameter from ANDOR by Index
 */
static void address_enumerated_index_value (UcaAndorCameraPrivate *priv, AT_WC* feature, int access_mode, int* enumerated) {
    GError **error = &(priv->construct_error);
    int count = 0;
    int temp = 0;
    int err = 0;
    if (access_mode == ANDOR_WRITE) {
	err = AT_GetEnumCount (priv->camera_handle, feature, &count);
	if (!andorCommunicationSuccessful (err, "Getting Enum Count", error)) return;
	if (*enumerated >= count || *enumerated<0) {
	    GError *temp_error = g_malloc0 (sizeof (GError));
	    temp_error = g_error_new (uca_andor_camera_error_quark (), UCA_ANDOR_CAMERA_ERROR_ENUM_OUT_OF_RANGE, "ERROR: Requested enumerated value is out of range (Requested: %i ; Range: 0 - %i).\n", *enumerated, count-1);
	    *error = NULL;
	    g_propagate_error(error, temp_error);
	    return;
	}
	err = AT_SetEnumIndex (priv->camera_handle, feature, *enumerated);
    }
    else if (access_mode == ANDOR_READ) {
	err = AT_GetEnumIndex (priv->camera_handle, feature, &temp);
	*enumerated = temp;
    }
    else {
	andor_error_invalid_access_mode (PROPERTY_INDEPENDENT, access_mode, priv);
	return;
    }
    andorCommunicationSuccessful (err, (char*) feature, error);
    return;
}

/*
 * Addressing an Enumerated-Parameter from ANDOR by String
 */
static void address_enumerated_string_value (UcaAndorCameraPrivate *priv, AT_WC* feature, int access_mode, AT_WC** enumerated) {
    GError **error = &(priv->construct_error);
    AT_WC* temp = g_malloc (1023*sizeof (AT_WC));
    int index = 0;
    int err = 0;
    if (access_mode == ANDOR_WRITE) {
	err = AT_SetEnumString (priv->camera_handle, feature, *enumerated);
    }
    else if (access_mode == ANDOR_READ) {
	err = AT_GetEnumIndex (priv->camera_handle, feature, &index);
	if (!andorCommunicationSuccessful (err,"Getting Enum Index to access String value", error)) return;
	err = AT_GetEnumStringByIndex (priv->camera_handle, feature, index, temp, 1023);
	wcscpy (*enumerated, temp);
    }
    else {
	andor_error_invalid_access_mode (PROPERTY_INDEPENDENT, access_mode, priv);
	return;
    }
    andorCommunicationSuccessful (err, (char*) feature, error);
    return;
}



static void uca_andor_camera_start_recording (UcaCamera *camera, GError **error)
{
    UcaAndorCameraPrivate *priv;
    GError *start_rec_error = g_malloc0 (sizeof (GError));
    start_rec_error = NULL;

    g_return_if_fail (UCA_IS_ANDOR_CAMERA(camera));
    priv = UCA_ANDOR_CAMERA_GET_PRIVATE (camera);

    AT_H cam_hndl = priv->camera_handle;

    //Cooling to target-temperature
/*
    int iErr = AT_SetBool (cam_hndl, L"SensorCooling", AT_TRUE);
    andorCommunicationSuccessful (iErr, "Activate Cooling", &start_rec_error);
    if (start_rec_error != NULL) goto propagateStartError;
    
    int temp_count = 0;
    iErr = AT_GetEnumCount (cam_hndl, L"TemperatureControl", &temp_count);
    andorCommunicationSuccessful (iErr, "Getting range of enum 'TemperatureControl'", &start_rec_error);
    if (start_rec_error != NULL) goto propagateStartError;
    iErr = AT_SetEnumIndex (cam_hndl, L"TemperatureControl", 0);
    andorCommunicationSuccessful (iErr, "Setting TemperatureControl to maxValue", &start_rec_error);
    if (start_rec_error != NULL) goto propagateStartError;
    
    int temp_status_index = -3;
    wchar_t* temp_status = g_malloc0 (256*sizeof(wchar_t));
    iErr = AT_GetEnumIndex (cam_hndl, L"TemperatureStatus", &temp_status_index);
    andorCommunicationSuccessful (iErr, "Getting temperature status (index)", &start_rec_error);
    if (start_rec_error != NULL) goto propagateStartError;
    iErr = AT_GetEnumStringByIndex (cam_hndl, L"TemperatureStatus", temp_status_index, temp_status, 256);
    andorCommunicationSuccessful (iErr, "Getting string representation of temp_status", &start_rec_error);
    if (start_rec_error != NULL) goto propagateStartError;
    
    double tmp_temp = 0.0;
    iErr = AT_GetFloat (cam_hndl, L"TargetSensorTemperature", &tmp_temp);
    g_print ("### %f\n", tmp_temp);
    ////Waiting for temperature to stabilise
    while (wcscmp(L"Stabilised",temp_status) != 0) {
	//g_print("%i: %s\n", temp_status_index, (char*) temp_status);
	iErr = AT_GetEnumIndex (cam_hndl, L"TemperatureStatus", &temp_status_index);
	andorCommunicationSuccessful (iErr, "Getting temperature status (index, while)", &start_rec_error);
	if (start_rec_error != NULL) goto propagateStartError;
	iErr = AT_GetEnumStringByIndex (cam_hndl, L"TemperatureStatus", temp_status_index, temp_status, 256);
	andorCommunicationSuccessful (iErr, "Getting string representation of temp_status (while)", &start_rec_error);
	if (start_rec_error != NULL) goto propagateStartError;
    }
*/
    //Getting size of memory to be declared
    AT_64 image_size_bytes = 0;
    AT_GetInt (cam_hndl, L"ImageSizeBytes", &image_size_bytes);
    priv->image_size_bytes = image_size_bytes;

    guint image_size = (int) image_size_bytes;

    //Creating buffer for image data
    gchar* image_buffer = g_malloc0 ((10 * image_size+8) * sizeof (gchar)); //+8 suggested to allow data allignment

    //Buffer Alignment - it is recommended to align the acquisition on an 8 byte boundary
    AT_U8* aligned_image_buffer = (AT_U8*) (((unsigned long) image_buffer + 7) & ~0x7);
    priv->image_buffer = aligned_image_buffer;

    //Queuing image buffer for acquisition
    for (int i = 0; i < 10; i++) {
        AT_QueueBuffer (cam_hndl, aligned_image_buffer + i * image_size_bytes, image_size_bytes);
    }

    AT_SetEnumString(cam_hndl, L"CycleMode", L"Continuous");
    
    int iErr = AT_Command (cam_hndl, L"AcquisitionStart");
    andorCommunicationSuccessful (iErr, "Starting Acquisition", &start_rec_error);
    if (start_rec_error != NULL) goto propagateStartError;    

    priv->rand=0;

    start_rec_error = NULL;
    AT_BOOL cam_acq = FALSE;
    iErr = AT_GetBool (cam_hndl, L"CameraAcquiring", &cam_acq);
    andorCommunicationSuccessful (iErr, "Getting current value: CameraAcquiring", &start_rec_error);
    if (start_rec_error != NULL) goto propagateStartError;  
	
    priv->is_cam_acquiring = (gboolean) cam_acq;	
    
    return;

propagateStartError:
    g_propagate_error (error, start_rec_error);
    return;
}

static void uca_andor_camera_stop_recording (UcaCamera *camera, GError **error)
{
    UcaAndorCameraPrivate *priv;
    GError *stop_rec_error = g_malloc0 (sizeof (GError));
    stop_rec_error = NULL;

    g_return_if_fail (UCA_IS_ANDOR_CAMERA(camera));
    priv = UCA_ANDOR_CAMERA_GET_PRIVATE(camera);
	
    AT_H cam_hndl = priv->camera_handle;

    int iErr = AT_Command (cam_hndl, L"AcquisitionStop");
    andorCommunicationSuccessful (iErr, "Stopping Acquisition", &stop_rec_error);
    if (stop_rec_error != NULL ) {
	g_print("iErr = %i\n", iErr);
	g_propagate_error (error, stop_rec_error);
	return;
    }

    AT_Flush(cam_hndl);
	
    stop_rec_error = NULL;
    AT_BOOL cam_acq = TRUE;
    iErr = AT_GetBool (cam_hndl, L"CameraAcquiring", &cam_acq);
    andorCommunicationSuccessful (iErr, "Getting current value: CameraAcquiring", &stop_rec_error);
    if (stop_rec_error != NULL ) {
	g_propagate_error (error, stop_rec_error);
	return;
    }
	
    priv->is_cam_acquiring = (gboolean) cam_acq;

    return;
}


static gboolean uca_andor_camera_grab (UcaCamera *camera, gpointer data, GError **error)
{
    UcaAndorCameraPrivate *priv;
    GError *grab_error = g_malloc0 (sizeof (GError));
    grab_error = NULL;

    g_return_val_if_fail (UCA_IS_ANDOR_CAMERA(camera), FALSE);
    priv = UCA_ANDOR_CAMERA_GET_PRIVATE(camera);

    AT_H cam_hndl = priv->camera_handle;

    int iErr;
    /*
    AT_BOOL cam_acq = AT_TRUE;
    int iErr = AT_GetBool (cam_hndl, L"CameraAcquiring", &cam_acq);
    if (!cam_acq) { //When camera stops acquiring on its own (maybe due to 'Max-frames-reached' or similiar) it needs to be restarted
	iErr = AT_Command (cam_hndl, L"AcquisitionStart");
	andorCommunicationSuccessful (iErr, "Restarting Acquisition during grab", &grab_error);
	if (grab_error != NULL) {
	    g_print ("%i\n", iErr);
	    g_propagate_error (error, grab_error);
	    return FALSE;
	}
	grab_error = NULL;
    }
    */

    AT_U8* buf = NULL; //g_malloc0 (priv->image_size_bytes*sizeof (AT_U8));
    int bufsize;

    iErr = AT_WaitBuffer (cam_hndl, &buf, &bufsize, 10000);
    andorCommunicationSuccessful (iErr, "Grabbing data - waiting for buffer", &grab_error);
    if (grab_error != NULL) {
	g_print("%i\n", iErr);
	g_propagate_error (error, grab_error);
	return FALSE;
    }
    printf("Got buffer: %p %lu (expected: %lu)\n", buf, bufsize, priv->image_size_bytes);

    g_memmove (data, buf, priv->image_size_bytes);
    /* AT_Flush (cam_hndl); */
    /* AT_QueueBuffer (cam_hndl, priv->image_buffer, priv->image_size_bytes); */
    AT_QueueBuffer (cam_hndl, buf, priv->image_size_bytes);

    return TRUE;
}

static void
uca_andor_camera_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (UCA_IS_ANDOR_CAMERA(object));
    UcaAndorCameraPrivate *priv = UCA_ANDOR_CAMERA_GET_PRIVATE(object);
    guint val_uint = 0;
    double val_double = 0.0;
    gint val_enum = 0;
    AT_WC* val_atwc;
    gchar* val_char;

    switch (property_id) {
        case PROP_EXPOSURE_TIME:
	    val_double = g_value_get_double (value);
	    address_double_value (priv, L"ExposureTime", ANDOR_WRITE, &val_double);
	    priv->exp_time = (gdouble) val_double;
	    break;
	case PROP_ROI_WIDTH:
	    val_uint = g_value_get_uint (value);
	    address_integer_value (priv, L"AOIWidth", ANDOR_WRITE, (int*) &val_uint);
	    priv->aoi_width = (guint) val_uint;
	    break;
	case PROP_ROI_HEIGHT:
	    val_uint = g_value_get_uint (value);
	    address_integer_value (priv, L"AOIHeight", ANDOR_WRITE, (int*) &val_uint);
	    priv->aoi_height = (guint) val_uint;
	    break;
	case PROP_ROI_X:
	    val_uint = g_value_get_uint (value);
	    address_integer_value (priv, L"AOILeft", ANDOR_WRITE, (int*) &val_uint);
	    priv->aoi_left = val_uint;
	    break;
	case PROP_ROI_Y:
	    val_uint = g_value_get_uint (value);
	    address_integer_value (priv, L"AOITop", ANDOR_WRITE, (int*) &val_uint);
	    priv->aoi_top = val_uint;
	    break;
	case PROP_SENSOR_WIDTH:
	    val_uint = g_value_get_uint (value);
	    address_integer_value (priv, L"SensorWidth", ANDOR_WRITE, (int*) &val_uint);
	    priv->sensor_width = val_uint;
	    break;
	case PROP_SENSOR_HEIGHT:
	    val_uint = g_value_get_uint (value);
	    address_integer_value (priv, L"SensorHeight", ANDOR_WRITE, (int*) &val_uint);
	    priv->sensor_height = val_uint;
	    break;
	case PROP_SENSOR_PIXEL_WIDTH:
	    val_double = g_value_get_double (value);
	    address_double_value (priv, L"PixelWidth", ANDOR_WRITE, &val_double);
	    priv->pixel_width = val_double;
	    break;
	case PROP_SENSOR_PIXEL_HEIGHT:
	    val_double = g_value_get_double (value);
	    address_double_value (priv, L"PixelHeight", ANDOR_WRITE, &val_double);
	    priv->pixel_height = val_double;
	    break;
	case PROP_SENSOR_BITDEPTH:
	    //placeholder
	    break;
	case PROP_TRIGGER_MODE:
	    val_enum = g_value_get_enum (value);
	    address_enumerated_index_value (priv, L"TriggerMode", ANDOR_WRITE, &val_enum);
	    priv->trigger_mode = val_enum;
	    break;
	case PROP_FRAMERATE:
	    val_double = g_value_get_double (value);
	    address_double_value (priv, L"FrameRate", ANDOR_WRITE, &val_double);
	    priv->frame_rate = g_value_get_float (value);
	    break;
	case PROP_SENSOR_MAX_FRAME_RATE:
	    //placeholder
	    break;
	case PROP_TARGET_SENSOR_TEMPERATURE:
	    val_double = g_value_get_double (value);
	    address_double_value (priv, L"TargetSensorTemperature", ANDOR_WRITE, &val_double);
	    priv->target_sensor_temperature = val_double;
	    break;
	case PROP_FAN_SPEED:
	    val_enum = g_value_get_int (value);
	    address_enumerated_index_value (priv, L"FanSpeed", ANDOR_WRITE, &val_enum);
	    priv->fan_speed = val_enum;
	    break;
	case PROP_CYCLE_MODE:
	    g_free (priv->cycle_mode);
	    val_char = g_value_dup_string (value);
	    val_atwc = g_malloc0 ((strlen(val_char) + 1) * sizeof (AT_WC));
	    mbstowcs (val_atwc, val_char, strlen(val_char));
	    address_enumerated_string_value (priv, L"CycleMode", ANDOR_WRITE, &val_atwc);
	    priv->cycle_mode = g_strdup (val_char);
	    g_free (val_char);
	    g_free (val_atwc);
	    break;
	case PROP_HAS_CAMRAM_RECORDING:
	    //placeholder
	    break;
	case PROP_HAS_STREAMING:
	    //placeholder
	    break;
	default:
	    andor_error_invalid_access_mode (property_id, ANDOR_WRITE, priv);
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	    break;
    }
    
    return;
}

static void
uca_andor_camera_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (UCA_IS_ANDOR_CAMERA(object));
    UcaAndorCameraPrivate *priv = UCA_ANDOR_CAMERA_GET_PRIVATE(object);
    guint val_uint = 0;
    double val_double = 0.0;
    gint val_enum = 0;
    AT_WC* val_atwc;
    gchar* val_char;

    switch (property_id) {
	case PROP_NAME:
	    g_value_set_string (value, priv->name);
	    break;
        case PROP_EXPOSURE_TIME:
	    address_double_value (priv, L"ExposureTime", ANDOR_READ, &val_double);
	    g_value_set_double (value, val_double);
	    break;
	case PROP_ROI_WIDTH:
	    address_integer_value (priv, L"AOIWidth", ANDOR_READ, (int*) &val_uint);
	    g_value_set_uint (value, val_uint);
	    break;
	case PROP_ROI_HEIGHT:
	    address_integer_value (priv, L"AOIHeight", ANDOR_READ, (int*) &val_uint);
	    g_value_set_uint (value, val_uint);
	    break;
	case PROP_ROI_X:
	    address_integer_value (priv, L"AOILeft", ANDOR_READ, (int*) &val_uint);
	    g_value_set_uint (value, val_uint);
	    break;
	case PROP_ROI_Y:
	    address_integer_value (priv, L"AOITop", ANDOR_READ, (int*) &val_uint);
	    g_value_set_uint (value, val_uint);
	    break;
	case PROP_SENSOR_WIDTH:
	    address_integer_value (priv, L"SensorWidth", ANDOR_READ, (int*) &val_uint);
	    g_value_set_uint (value, val_uint);
	    break;
	case PROP_SENSOR_HEIGHT:
	    address_integer_value (priv, L"SensorHeight", ANDOR_READ, (int*) &val_uint);
	    g_value_set_uint (value, val_uint);
	    break;
	case PROP_SENSOR_PIXEL_WIDTH:
	    address_double_value (priv, L"PixelWidth", ANDOR_READ, &val_double);
	    g_value_set_double (value, val_double);
	    break;
	case PROP_SENSOR_PIXEL_HEIGHT:
	    address_double_value (priv, L"PixelHeight", ANDOR_READ, &val_double);
	    g_value_set_double (value, val_double);
	    break;
	case PROP_SENSOR_BITDEPTH:
	    //placeholder
	    break;
	case PROP_TRIGGER_MODE:
	    address_enumerated_index_value (priv, L"TriggerMode", ANDOR_READ, &val_enum);
	    g_value_set_enum (value, val_enum); 
	    break;
	case PROP_ROI_STRIDE:
	    address_integer_value (priv, L"AOIStride", ANDOR_READ, (int*) &val_uint);
	    g_value_set_uint (value, val_uint);
	    break;
	case PROP_FRAMERATE:
	    address_double_value (priv, L"FrameRate", ANDOR_READ, &val_double);
	    g_value_set_float (value, (float) val_double);
	    break;
	case PROP_SENSOR_MAX_FRAME_RATE:
	    //placeholder
	    break;
	case PROP_SENSOR_TEMPERATURE:
	    address_double_value (priv, L"SensorTemperature", ANDOR_READ, &val_double);
	    g_value_set_double (value, val_double);
	    break;
	case PROP_TARGET_SENSOR_TEMPERATURE:
	    address_double_value (priv, L"TargetSensorTemperature", ANDOR_READ, &val_double);
	    g_value_set_double (value, val_double);
	    break;
	case PROP_FAN_SPEED:
	    address_enumerated_index_value (priv, L"FanSpeed", ANDOR_READ, &val_enum);
	    g_value_set_int (value, val_enum);
	    break;
	case PROP_CYCLE_MODE:
	    val_atwc = g_malloc0 (1023*sizeof (AT_WC));
	    address_enumerated_string_value (priv, L"CycleMode", ANDOR_READ, &val_atwc);
	    val_char = g_malloc0 ((wcslen(val_atwc) + 1)*sizeof (gchar));
	    wcstombs (val_char, val_atwc, wcslen(val_atwc));
	    g_value_set_string (value, val_char);
	    g_free (val_char);
	    g_free (val_atwc);
	    break;
	case PROP_IS_RECORDING:
	    g_value_set_boolean (value, priv->is_cam_acquiring);
	    break;
	case PROP_HAS_CAMRAM_RECORDING:
	    //placeholder
	    break;
	case PROP_HAS_STREAMING:
	    //placeholder 
	    break;
	default:
	    andor_error_invalid_access_mode (property_id, ANDOR_READ, priv);
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	    break;
    }

    return;
}

static gboolean
ufo_andor_camera_initable_init (GInitable *initable,
                               GCancellable *cancellable,
                               GError **error)
{
    UcaAndorCamera *cam;
    UcaAndorCameraPrivate *priv;

    cam = UCA_ANDOR_CAMERA (initable);
    priv = cam->priv;

    g_return_val_if_fail (UCA_IS_ANDOR_CAMERA(initable), FALSE);
    if (priv->construct_error != NULL) {
	if (error) {
	    *error = g_error_copy (priv->construct_error); 
	}
    }
    return TRUE;
}

static void
uca_andor_initable_iface_init (GInitableIface *iface)
{
    iface->init = ufo_andor_camera_initable_init;
}

static void
uca_andor_camera_finalize (GObject *object)
{
    g_return_if_fail (UCA_IS_ANDOR_CAMERA(object));
    UcaAndorCameraPrivate *priv = UCA_ANDOR_CAMERA_GET_PRIVATE(object);

    GError *error = g_malloc0 (sizeof (GError*));

    int iErr = AT_Close (priv->camera_handle);
    if (!andorCommunicationSuccessful (iErr, "Closing Handle", &error)) return;

    G_OBJECT_CLASS (uca_andor_camera_parent_class)->finalize (object);
	
    iErr = AT_FinaliseLibrary ();
    if (!andorCommunicationSuccessful (iErr, "Closing Library", &error)) return;

    g_clear_error (&(priv->construct_error));
    return;
}

static void uca_andor_camera_trigger (UcaCamera *camera, GError **error)
{
    return;
}

static void
uca_andor_camera_class_init (UcaAndorCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_andor_camera_set_property;
    gobject_class->get_property = uca_andor_camera_get_property;
    gobject_class->finalize = uca_andor_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_andor_camera_start_recording;
    camera_class->stop_recording = uca_andor_camera_stop_recording;
    camera_class->grab = uca_andor_camera_grab;
    camera_class->trigger = uca_andor_camera_trigger;

    for (guint i = 0; andor_overrideables [i] != 0; i++)
        g_object_class_override_property (gobject_class, andor_overrideables [i], uca_camera_props [andor_overrideables [i]]);

    andor_properties [PROP_ROI_STRIDE] =
	g_param_spec_uint ("roi-stride",
		    "ROI Stride",
		    "The stride of the region (or area) of interest",
		    0,G_MAXINT, 1, 
		    G_PARAM_READABLE);
    andor_properties [PROP_SENSOR_TEMPERATURE] =
	g_param_spec_double ("sensor-temperature",
		    "sensor-temp",
		    "The current temperature of the sensor",
		    -100.0, 100.0, 20.0,
		    G_PARAM_READABLE);
    andor_properties [PROP_TARGET_SENSOR_TEMPERATURE] = 
	g_param_spec_double ("target-sensor-temperature",
		    "target-sensor-temp",
		    "The temperature that is to be reached before acquisition may start",
		    -100.0, 100.0, 20.0,
		    G_PARAM_READWRITE);
    andor_properties [PROP_FAN_SPEED] =
        g_param_spec_int ("fan-speed",
		    "fan-speed",
		    "The speed by which the fan is rotating",
		    -100, 100, 0,
		    G_PARAM_READWRITE);
    andor_properties [PROP_CYCLE_MODE] =
	g_param_spec_string ("cycle-mode",
		    "cylce mode",
		    "The currently used cycle mode for the acquisition",
		    "F",
		    G_PARAM_READWRITE);
    andor_properties [PROP_FRAMERATE] = 
	g_param_spec_float ("frame-rate",
		    "frame rate",
		    "The current frame rate of the camera",
		    1.0f, 100.0f, 100.0f,
		    G_PARAM_READWRITE);

    for (guint id = N_BASE_PROPERTIES; id < N_PROPERTIES; id++)
        g_object_class_install_property (gobject_class, id, andor_properties [id]);

    g_type_class_add_private (klass, sizeof (UcaAndorCameraPrivate));
}

/*
 * This function will initiate the andor camera		
 */
static void
uca_andor_camera_init (UcaAndorCamera *self)
{
    self->priv = UCA_ANDOR_CAMERA_GET_PRIVATE (self);
    self->priv->construct_error = NULL;
    GError **error = &(self->priv->construct_error);
    int iErr = AT_InitialiseLibrary ();
    self->priv->camera_number = 0; //TODO: Expanding so multiple cameras can be handled
    self->priv->is_sim_cam = FALSE;

    //Initial settings:
    AT_H cam_hndl;
    iErr = AT_Open (self->priv->camera_number, &cam_hndl);
    if (!andorCommunicationSuccessful (iErr, "Opening Handle", error)) return;
    self->priv->camera_handle = cam_hndl;

    self->priv->model = g_malloc0 (1023*sizeof (gchar));
    AT_WC* model = g_malloc0 (1023*sizeof (AT_WC));
    iErr = AT_GetString (cam_hndl, L"CameraModel", model, 1023); //CameraName not available for SimCam
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: CameraModel", error)) return;
    gchar* modelchar = g_malloc0 ((wcslen(model) + 1)*sizeof (gchar));
    wcstombs (modelchar, model, wcslen(model));
    strcpy(self->priv->model,modelchar);

    self->priv->is_sim_cam = strcmp (modelchar, "SIMCAM CMOS") == 0;

    self->priv->name = g_malloc0 (1023*sizeof (gchar));
    if (self->priv->is_sim_cam == FALSE) {
    	AT_WC* name = g_malloc0 (1023*sizeof (AT_WC));
    	iErr = AT_GetString (cam_hndl, L"CameraName", name, 1023);
        /* if (!andorCommunicationSuccessful (iErr, "Getting initial value: CameraName", error)) */
            self->priv->name = self->priv->model;
        /* else { */
        /*     gchar* namechar = g_malloc0 ((wcslen(name) + 1)*sizeof (gchar)); */
        /*     wcstombs (namechar, name, wcslen(name)); */
        /*     strcpy(self->priv->name, namechar); */
        /*     g_free (name); */
        /*     g_free (namechar); */
        /* } */
    }
    else {
	self->priv->name = "SIMCAM CMOS (model)";
    }

    double exptime = 0.0;
    iErr = AT_GetFloat (cam_hndl, L"ExposureTime", &exptime);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: ExposureTime", error)) return;
    self->priv->exp_time = exptime;

    AT_64 aoi_width = 0;
    iErr = AT_GetInt (cam_hndl, L"AOIWidth", &aoi_width);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: AOIWidth", error)) return;
    self->priv->aoi_width = (guint) aoi_width;

    AT_64 aoi_height = 0;
    iErr = AT_GetInt (cam_hndl, L"AOIHeight", &aoi_height);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: AOIHeight", error)) return;
    self->priv->aoi_height = (guint) aoi_height;

    AT_64 aoi_left = 0;
    iErr = AT_GetInt (cam_hndl, L"AOILeft", &aoi_left);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: AOILeft", error)) return;
    self->priv->aoi_left = (guint) aoi_left;

    AT_64 aoi_top = 0;
    iErr = AT_GetInt (cam_hndl, L"AOITop", &aoi_top);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: AOITop", error)) return;
    self->priv->aoi_top = (guint) aoi_top;

    AT_64 sensor_width = 0;
    iErr = AT_GetInt (cam_hndl, L"SensorWidth", &sensor_width);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: SensorWidth", error)) return;
    self->priv->sensor_width = (guint) sensor_width;

    AT_64 sensor_height = 0;
    iErr = AT_GetInt (cam_hndl, L"SensorHeight", &sensor_height);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: SensorHeight", error)) return;
    self->priv->sensor_height = (guint) sensor_height;
	
    double pixel_width = 0;
    iErr = AT_GetFloat (cam_hndl, L"PixelWidth", &pixel_width);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: PixelWidth", error)) return;
    self->priv->pixel_width = pixel_width;

    double pixel_height = 0;
    iErr = AT_GetFloat (cam_hndl, L"PixelHeight", &pixel_height);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: PixelHeight", error)) return;
    self->priv->pixel_height = pixel_height;

    int trigger_mode = 0;
    iErr = AT_GetEnumIndex (cam_hndl, L"TriggerMode", &trigger_mode);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: TriggerMode", error)) return;
    self->priv->trigger_mode = (gint) trigger_mode;

    AT_64 aoi_stride = 0;
    iErr = AT_GetInt (cam_hndl, L"AOIStride", &aoi_stride);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: AOIStride", error)) return;
    self->priv->aoi_stride = (guint) aoi_stride;

    double frame_rate = 0.0;
    iErr = AT_GetFloat (cam_hndl, L"FrameRate", &frame_rate);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: FrameRate", error)) return;
    self->priv->frame_rate = (gfloat) frame_rate;

    self->priv->max_frame_rate = 100000.0f;
    
    double sensor_temperature = 0.0;
    iErr = AT_GetFloat (cam_hndl, L"SensorTemperature", &sensor_temperature);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: SensorTemperature", error)) return;
    self->priv->sensor_temperature = sensor_temperature;

    double target_sensor_temperature = 0.0;
    iErr = AT_GetFloat (cam_hndl, L"TargetSensorTemperature", &target_sensor_temperature);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: TargetSensorTemperature", error)) return;
    self->priv->target_sensor_temperature = target_sensor_temperature;

    int fan_speed = 0;
    iErr = AT_GetEnumIndex (cam_hndl, L"FanSpeed", &fan_speed);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: FanSpeed", error)) return;
    self->priv->fan_speed = fan_speed;

/*    self->priv->cycle_mode = g_malloc0 (1023*sizeof (gchar));
    AT_WC* cyclemode = g_malloc0 (1023*sizeof (AT_WC));
    int cycle_mode_index = 0;
    iErr = AT_GetEnumIndex (cam_hndl, L"CycleMode", &cycle_mode_index);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: CycleMode - Index", error)) return;
    iErr = AT_GetEnumStringByIndex (cam_hndl, L"CycleMode", cycle_mode_index, cyclemode, 1023);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: CycleMode", error)) return;
    gchar* cyclechar = g_malloc0 ((wcslen(cyclemode) + 1)*sizeof(gchar));
    wcstombs (cyclechar, cyclemode, wcslen(cyclemode));
    strcpy (self->priv->cycle_mode, cyclechar);
*/
    self->priv->cycle_mode = NULL;

    AT_BOOL cam_acq = FALSE;
    iErr = AT_GetBool (cam_hndl, L"CameraAcquiring", &cam_acq);
    if (!andorCommunicationSuccessful (iErr, "Getting initial value: CameraAcquiring", error)) return;
    self->priv->is_cam_acquiring = (gboolean) cam_acq;

    uca_camera_register_unit (UCA_CAMERA (self), "frame-rate", UCA_UNIT_COUNT);

    /* Freeing allocated memory */
    g_free (model);
    g_free (modelchar);

    return;

}

G_MODULE_EXPORT GType
uca_camera_get_type (void)
{
    return UCA_TYPE_ANDOR_CAMERA;
}
