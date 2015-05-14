/* gap_locate.h
 *    alternative implementation for locating corresponding pattern in another layer.
 *    by hof (Wolfgang Hofer)
 *  2011/12/03
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

#ifndef _GAP_LOCATE2_H
#define _GAP_LOCATE2_H

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"


/* ----------------------------------------
 * gap_locateAreaWithinRadius
 * ----------------------------------------
 *
 * the locateAreaWithinRadius procedure takes a referenced detail
 * specified via refX/Y coordinate, refShapeRadius within a
 * reference drawable
 * and tries to locate the same (or similar) detail coordinates
 * in the target Drawable.
 *
 * This is done by comparing rgb pixel values of the areas at refShapeRadius
 * in the corresponding target Drawable in a loop while
 * varying offsets within targetMoveRadius.
 * the targetX/Y koords are picked at those offsets where the compared areas
 * are best matching (e.g with minimun color difference)
 * the return value is the minimum colrdifference value
 * (in range 0.0 to 1.0 where 0.0 indicates that the compared area is exactly equal)
 * 
 */
gdouble
gap_locateAreaWithinRadius(gint32  refDrawableId
  , gint32  refX
  , gint32  refY
  , gint32  refShapeRadius
  , gint32  targetDrawableId
  , gint32  targetMoveRadius
  , gint32  *targetX
  , gint32  *targetY
  );


/* --------------------------------------------
 * gap_locateAreaWithinRadiusWithOffset
 * --------------------------------------------
 * processing starts at reference coords + offest
 * and continues outwards upto targetMoveRadius for 4 quadrants.
 * 
 * returns average color difference (0.0 upto 1.0)
 *    where 0.0 indicates exact matching area
 *      and 1.0 indicates all pixel have maximum color diff (when comaring full white agains full black area)
 */
gdouble
gap_locateAreaWithinRadiusWithOffset(gint32  refDrawableId
  , gint32  refX
  , gint32  refY
  , gint32  refShapeRadius
  , gint32  targetDrawableId
  , gint32  targetMoveRadius
  , gint32  *targetX
  , gint32  *targetY
  , gint32  offsetX
  , gint32  offsetY
  );


#endif
