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

#ifndef __UCA_FILE_CAMERA_H
#define __UCA_FILE_CAMERA_H

#include <glib-object.h>
#include "uca-camera.h"

G_BEGIN_DECLS

#define UCA_TYPE_FILE_CAMERA             (uca_file_camera_get_type())
G_DECLARE_FINAL_TYPE(UcaFileCamera, uca_file_camera, UCA, FILE_CAMERA, UcaCamera)

G_END_DECLS

#endif
