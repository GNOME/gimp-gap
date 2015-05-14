/* gap_resi_dialog.h
 * 1998.07.01 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains the resize and scale Dialog for video frames.
 * (It just is a shell to call Gimp's resize / scale Dialog )
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/* revision history
 * gimp    1.3.14a; 2003/05/18  hof: reincarnation of gap_resi_dialog.h
 * 0.96.00; 1998/07/01   hof: first release
 */

#ifndef _RESI_DIALOG_H
#define _RESI_DIALOG_H
 
/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_range_ops.h"

gint gap_resi_dialog (gint32 image_id, 
                    GapRangeOpsAsiz asiz_mode,
                    char *title_text,
                    long *size_x, long *size_y, 
                    long *offs_x, long *offs_y);
#endif
