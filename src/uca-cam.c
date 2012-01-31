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

#include <stdlib.h>
#include <string.h>
#include "uca.h"
#include "uca-cam.h"
#include "uca-grabber.h"


struct uca_camera_priv *uca_cam_new(void)
{
    struct uca_camera_priv *cam = (struct uca_camera_priv *) malloc(sizeof(struct uca_camera_priv));

    /* Set all function pointers to NULL so we know early on, if something has
     * not been implemented. */
    memset(cam, 0, sizeof(struct uca_camera_priv));

    cam->state = UCA_CAM_CONFIGURABLE;
    cam->current_frame = 0;
    return cam;
}
