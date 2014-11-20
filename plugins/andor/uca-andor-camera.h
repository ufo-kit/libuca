/* AUTHOR: Sven Werchner
 * DATE: 06.12.2013
 *
 */

#ifndef __UCA_ANDOR_CAMERA_H
#define __UCA_ANDOR_CAMERA_H

#include <glib-object.h>
#include "uca-camera.h"

G_BEGIN_DECLS

#define UCA_TYPE_ANDOR_CAMERA               (uca_andor_camera_get_type())
#define UCA_ANDOR_CAMERA(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj), UCA_TYPE_ANDOR_CAMERA, UcaAndorCamera))
#define UCA_IS_ANDOR_CAMERA(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj), UCA_TYPE_ANDOR_CAMERA))
#define UCA_ANDOR_CAMERA_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST((klass), UCA_TYPE_ANDOR_CAMERA, UcaAndorCameraClass))
#define UCA_IS_ANDOR_CAMERA_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass), UCA_TYPE_ANDOR_CAMERA))
#define UCA_ANDOR_CAMERA_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS((obj), UCA_TYPE_ANDOR_CAMERA, UcaAndorCameraClass))

#define UCA_ANDOR_CAMERA_ERROR              uca_andor_camera_error_quark()

typedef enum {
    ANDOR_NOERROR = 0,
    UCA_ANDOR_CAMERA_ERROR_LIBANDOR_INIT,
    UCA_ANDOR_CAMERA_ERROR_LIBANDOR_GENERAL,
    UCA_ANDOR_CAMERA_ERROR_UNSUPPORTED,
    UCA_ANDOR_CAMERA_ERROR_INVALID_MODE,
    UCA_ANDOR_CAMERA_ERROR_MODE_NOT_AVAILABLE,
    UCA_ANDOR_CAMERA_ERROR_ENUM_OUT_OF_RANGE
} UcaAndorCameraError;

typedef struct _UcaAndorCamera          UcaAndorCamera;
typedef struct _UcaAndorCameraClass     UcaAndorCameraClass;
typedef struct _UcaAndorCameraPrivate   UcaAndorCameraPrivate;


struct _UcaAndorCamera {
    /*< private >*/
    UcaCamera parent;

    UcaAndorCameraPrivate *priv;
};

struct _UcaAndorCameraClass {
    /*< private >*/
    UcaCameraClass parent;
};

GType uca_andor_camera_get_type (void);

G_END_DECLS

#endif
