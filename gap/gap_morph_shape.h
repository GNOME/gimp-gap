/* gap_morph_shape.h
 *    image shape dependent morph workpoint creation
 *    by hof (Wolfgang Hofer)
 *  2010/08/10
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

#ifndef _GAP_MORPH_SHAPE_H
#define _GAP_MORPH_SHAPE_H

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_morph_main.h"
#include "gap_morph_dialog.h"
#include "gap_lib_common_defs.h"

/* -----------------------------
 * gap_morph_shape_new_workpont
 * -----------------------------
 */
GapMorphWorkPoint *
gap_morph_shape_new_workpont(gdouble srcx, gdouble srcy, gdouble dstx, gdouble dsty);



/* --------------------------------
 * gap_moprhShapeDetectionEdgeBased
 * --------------------------------
 *
 * generates morph workpoints via edge based shape detection.
 * This is done by edge detection in the source image
 *  (specified via mgup->mgpp->osrc_layer_id)
 *
 * and picking workpoints on the detected edges
 * and locating the corresponding point in the destination image.
 *
 * IN: mgup->num_shapepoints  specifies the number of workpoints to be generated.
 */
void
gap_moprhShapeDetectionEdgeBased(GapMorphGUIParams *mgup, gboolean *cancelFlagPtr);



/* -----------------------------------------------
 * gap_morph_shape_generate_frame_tween_workpoints
 * -----------------------------------------------
 * call with progressBar in non-interactive mode
 * or with progressBar when progress handling is required.
 */
gint32
gap_morph_shape_generate_frame_tween_workpoints(GapAnimInfo *ainfo_ptr
   , GapMorphGlobalParams *mgpp, GtkWidget *masterProgressBar, GtkWidget *progressBar, gboolean *cancelFlagPtr);


/* -------------------------------------------
 * gap_morph_shape_generate_outline_workpoints
 * -------------------------------------------
 */
void
gap_morph_shape_generate_outline_workpoints(GapMorphGlobalParams *mgpp);


/* -------------------------------------------
 * gap_morph_shape_calculateAngleDegree
 * -------------------------------------------
 * return angle of the workpoint movement vektor in degree
 * Out: length of this vector
 */
gdouble
gap_morph_shape_calculateAngleDegree(GapMorphWorkPoint *wp, gdouble *length);



#endif
