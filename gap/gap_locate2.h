/* gap_locate2.h
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


/* GIMP-GAP includes */
#include "gap_geo.h"


typedef struct GapLocateTuneOffsElem {
  gint32   tuneOffsetX;
  gint32   tuneOffsetY;
  gdouble  relDiff;
  gboolean valid;
  struct   GapLocateTuneOffsElem *next;
} GapLocateTuneOffsElem;


/* ----------------------------------------
 * gap_locate_newGapLocateTuneOffsElem
 * ----------------------------------------
 * allocates a new list elment and initializes it as valid
 * and with the specified constructor parameters.
 */
GapLocateTuneOffsElem *
gap_locate_newGapLocateTuneOffsElem(gint idx, gint idy, gdouble relDiff);

/* ----------------------------------------
 * gap_locate_freeTuneOffsList
 * ----------------------------------------
 * free all elements in the the specified list.
 */
void
gap_locate_freeTuneOffsList(GapLocateTuneOffsElem *rootElem);

/* ----------------------------------------
 * gap_locate_FindTuneOffsShortList
 * ----------------------------------------
 * This procedure provides a list of tuning offsets in the range of +- 2 pixels around
 * coordinates that were typically located via
 *  gap_locateAreaWithinRadiusWithOffset
 * It computes color relation arrays for the areas around refCoord (in the referenceLayerId)
 * and currCoord (target coordinate in the activeDrawableId) and compares the 
 * color relation arrays reference versus target.
 * This computation and comparison is done with varying tuning offests where only the the best matching
 * offsets are recorded in the short list (max 5 elements) that is returned to the caller
 * (for tuning purpose while aligning video frames for video stabilisation).
 *
 * Note that the list may contain elements marked as invalid in case there are less than 5
 * very good matching variants.
 * The qFactor shall eliminate weak matchers (by setting them invalid)
 * in case there is a clear favorite matching offset available,
 * but keep more elements (== tune attempts) in case there are more very similar matching candidates.
 *
 * returns a short list of potential tuning offsets
 * Note the caller is responsible to free the returned list (by calling p_freeTuneOffsList)
 */
GapLocateTuneOffsElem *
gap_locate_FindTuneOffsShortList(gint32 activeDrawableId, gint32 referenceLayerId, GapPixelCoords *refCoord, GapPixelCoords *currCoord, gdouble qFactor);

/* -------------------------------------------------------
 * gap_locatePickNearestToPredictedCoordinateFromShortlist
 * -------------------------------------------------------
 * IN/OUT: trkCoord  is adjusted (tuned) with the tuning offest that leads to the
 * nearest coordinate compared to the predictedCoord.
 */
void 
gap_locatePickNearestToPredictedCoordinateFromShortlist(GapPixelCoords *trkCoord, GapPixelCoords *predictedCoord, 
   GapLocateTuneOffsElem *shortListP1, gint width, gint height);

/* ------------------------------------
 * gap_locate_check_strong_shortlist
 * ------------------------------------
 * Checks if the short list contains only one good matching element.
 * In case there are more elements with neaerly the same matching quality (relDiff value)
 * it is considered as weak matching.
 * Note that the short list has to be already sorted by ascending relDiff values.
 * therefore the best matching element is always the 1st in the list.
 */
gboolean
gap_locate_check_strong_shortlist(GapLocateTuneOffsElem *shortListP1, gdouble nearlySameFactor, gdouble strongRelDiff);


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


/* --------------------------------------------
 * gap_locateColordiffOpaquePixels
 * --------------------------------------------
 * compares opaque pixels of the specified refDrawableId and targetDrawableId.
 * Both drawables shall have same size 
 * Note that offsets of the layers within the image are relevant for processing.
 * (pixels are compared at corresponding coordinates which means having the same
 * same position in the image)
 * in case there are less pixels involved in the comarison than the specified requiredPixelCount
 * than value 1.0 is returned (to indicate "worst case" 
 * for situations where not enough pixels could be compared because there are too few opaque pixels
 * at corresponding coordinates, or there is no intersection at all)
 * 
 * returns average color difference (0.0 upto 1.0)
 *    where 0.0 indicates exact matching area
 *      and 1.0 indicates all pixel have maximum color diff (when comparing full white agains full black area)
 */
gdouble
gap_locateColordiffOpaquePixels(gint32  refDrawableId
  , gint32  targetDrawableId
  , gint32  requiredPixelCount
  , gint32 *comparedPixelCount
  );



#endif
