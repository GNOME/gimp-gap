/* gap_edge_detection.h
 *    by hof (Wolfgang Hofer)
 *  2010/08/08
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

#ifndef _GAP_EDGE_DETECTION_H
#define _GAP_EDGE_DETECTION_H

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

typedef struct GapEdgeValues { /* nickname: edval */
     gdouble       blurRadius1X;
     gdouble       blurRadius1Y;
     gdouble       blurRadius2X;
     gdouble       blurRadius2Y;
     gint32        shiftLeft;
     gint32        shiftRight;
     gint32        shiftUp;
     gint32        shiftDown;
     gboolean      autoLevels;
     gboolean      desaturate;
     gboolean      invert;
     gboolean      createEdgeAsNewLayer;

  } GapEdgeValues;

/* ----------------------------------------
 * gap_edgeDetection
 * ----------------------------------------
 *
 * returns the drawable id of a newly created channel
 * representing edges of the specified image.
 *
 * black pixels indicate areas of same or similar colors,
 * white indicates sharp edges.
 *
 */
gint32 gap_edgeDetection(gint32  refDrawableId
  , gdouble edgeColordiffThreshold
  , gint32 *countEdgePixels
  );


/* ---------------------------------
 * gap_edgeDetectionByShiftBlurDiff
 * ---------------------------------
 * replace the content of specified activeDrawableId
 * with results of edgeDetection.
 * The edge detection is done based on differences of copies of the original activeLayer
 * optionally shifted by small amount (1 or 2 pixels) horizontally and/or vertically and optionally blured.
 *
 * the edge detection result is furter optionally auto streched (levels) and inverted.
 * returns the drawable id of the edgeMask
 */
gint32
gap_edgeDetectionByShiftBlurDiff(gint32 activeDrawableId, GapEdgeValues *edvalPtr);



#endif
