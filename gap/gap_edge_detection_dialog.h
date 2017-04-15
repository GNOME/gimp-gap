/* gap_edge_detection_dialog.h
 *    by hof (Wolfgang Hofer)
 *  2017/03/10
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * version 2.7.0;             hof: created
 */

#ifndef _GAP_EDGE_DETECTION_DIALOG_H
#define _GAP_EDGE_DETECTION_DIALOG_H

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"


#include "gap_edge_detection.h"



#define GAP_EDGE_PLUGIN_NAME "plug-in-edge-dosog"
#define GAP_EDGE_HELP_ID     "plug-in-edge-dosog"

/* ---------------------------------
 * deliver new values (initialized with the defaault settings)
 * ---------------------------------
 */
GapEdgeValues * 
gap_edge_edval_new(gint32 acgtivDrawableId);

/* ---------------------------------
 * the Edge Detect MAIN dialog
 * ---------------------------------
 * return -1 on Error
 *         0 .. OK
 */
int
gap_edge_dialog(gint32 activeDrawableId, GapEdgeValues *edvalPtr);


#endif
