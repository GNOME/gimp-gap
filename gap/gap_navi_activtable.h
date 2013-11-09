/* gap_navi_activtable.h
 * 2002.04.21 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 */
/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * 1.3.5a; 2002/04/21   hof: created (Handle Event: active_image changed id)
 */

#ifndef _GAP_NAVI_ACTIVTABLE_H
#define _GAP_NAVI_ACTIVTABLE_H

#include "libgimp/gimp.h"

void           gap_navat_update_active_image(gint32 old_image_id, gint32 new_image_id);
void           gap_navat_set_active_image(gint32 image_id, gint32 pid);
gint32         gap_navat_get_active_image(gint32 image_id, gint32 pid);

#endif


