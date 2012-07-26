
#ifndef _PYLON_CAMERA_H_
#define _PYLON_CAMERA_H_

#include <glib.h>

G_BEGIN_DECLS

void pylon_camera_new(const gchar* lib_path, const gchar* camera_ip, GError** error);

void pylon_camera_set_exposure_time(gdouble exp_time, GError** error);
void pylon_camera_get_exposure_time(gdouble* exp_time, GError** error);

void pylon_camera_get_sensor_size(guint* width, guint* height, GError** error);
void pylon_camera_get_bit_depth(guint* depth, GError** error);

void pylon_camera_get_roi(guint16* roi_x, guint16* roi_y, guint16* roi_width, guint16* roi_height, GError** error);
void pylon_camera_set_roi(guint16 roi_x, guint16 roi_y, guint16 roi_width, guint16 roi_height, GError** error);

void pylon_camera_start_acquision(GError** error);
void pylon_camera_stop_acquision(GError** error);
void pylon_camera_grab(gpointer *data, GError** error);

void pylon_camera_delete();

G_END_DECLS

#endif

