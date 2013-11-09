/* gap_colormask_dialog.h
 *    by hof (Wolfgang Hofer)
 *    colormask filter dialog handling procedures
 *  2010/02/25
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
 * version 2.7.0;             hof: created
 */

#ifndef _GAP_COLORMASK_DIALOG_H
#define _GAP_COLORMASK_DIALOG_H

/* SYTEM (UNIX) includes */ 
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"


GtkWidget*  gap_colormask_create_dialog (GapColormaskValues *cmaskvals);

gboolean    gap_colormask_dialog (GapColormaskValues *cmaskvals, GimpDrawable *layer_drawable);

#endif
