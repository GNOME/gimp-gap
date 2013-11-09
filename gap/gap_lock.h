/* gap_lock.h
 * 2002.04.21 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins   LOCKING
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
 * 1.3.5a; 2002/04/21   hof: created (new gap locking procedures with locktable)
 */

#ifndef _GAP_LOCK_H
#define _GAP_LOCK_H

#include "libgimp/gimp.h"

gboolean  gap_lock_check_for_lock(gint32 image_id, GimpRunMode run_mode);
void      gap_lock_set_lock(gint32 image_id);
void      gap_lock_remove_lock(gint32 image_id);


#endif


