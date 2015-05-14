/* gap_locate.h
 *    by hof (Wolfgang Hofer)
 *  2010/08/06
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

#ifndef _GAP_LOCATE_H
#define _GAP_LOCATE_H

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

#define GAP_LOCATE_MIN_REF_SHAPE_RADIUS 3
#define GAP_LOCATE_MAX_REF_SHAPE_RADIUS 16


/* ----------------------------------------
 * gap_locateDetailWithinRadius
 * ----------------------------------------
 *
 * the locateDetailWithinRadius procedure takes a referenced detail
 * specified via refX/Y coordinate, refShapeRadius within a
 * reference drawable
 * and tries to locate the same (or similar) detail coordinates
 * in the target Drawable.
 *
 * This is done by comparing pixel areas at refShapeRadius
 * in the corresponding target Drawable in a loop while
 * varying offsets within targetMoveRadius.
 * the targetX/Y koords are picked at those offsets where the compared areas
 * are best matching (e.g with minimun color difference)
 * the return value is the minimum colrdifference value
 * (in range 0.0 to 1.0 where 0.0 indicates that the compared area is exactly equal)
 * 
 * requiredColordiffThreshold has valid range 0.0 to 1.0
 *                            values < 1.0 can speed up processing because area compare operations
 *                            are cancelled on offesets where colordiff is above this threshold
 *                            (e.g. when further analyse has no chance to deliver result 
 *                             below the required matching quality)
 *                            use value 1.0 if you are interested in weak matching results too.
 *                           
 */
gdouble gap_locateDetailWithinRadius(gint32  refDrawableId
  , gint32  refX
  , gint32  refY
  , gint32  refShapeRadius
  , gint32  targetDrawableId
  , gint32  targetMoveRadius
  , gdouble requiredColordiffThreshold
  , gint32  *targetX
  , gint32  *targetY
  );




#endif
