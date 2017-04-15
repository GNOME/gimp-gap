/* gap_locate2.c
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

/* SYTEM (UNIX) includes */
#include "string.h"
#include "stdlib.h"

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_pixelrgn.h"
#include "gap_locate2.h"

#define MAX_DIFF_VALUE_PER_PIXEL  (255.0 + 255.0 + 255.0)
#define OPACITY_LEVEL_UCHAR 50

#define GOOD_MATCH_FACTOR 1.05

extern int gap_debug;

typedef struct Context {
  gint32    refShapeRadius;
  gint32    refX;
  gint32    refY;
  gint32    bestX;
  gint32    bestY;
  gint32    px;
  gint32    py;
  gint32    rowsProcessedCount;     /* debug information (for development/debug purpose) */
  gint32    cancelAttemptCount;     /* debug information (for development/debug purpose) */
  gboolean  cancelAttemptFlag;      /* indicator to cancel current evaluation attempt */
  gboolean  isFinishedFlag;         /* indicator to cancel all further evaluations */
  gint32  requiredPixelCount;       /* number of pixels minimum required for plausible area comparison  (30 % of full area size)*/
  gint32  almostFullAreaPixelCount; /* number of pixels  (90 % of full area size)*/
  gint32  involvedPixelCount;       /* number of pixels involved in current comparison attempt */
  gdouble sumDiffValue;             /* summ of the RGB channel differences of the involved pixels in the current attempt */
  gdouble currentDistance;          /* square distance from reference coords to the offset of the current attempt */
  gint32  bestMatchingPixelCount;   /* number of pixels involved in best matching attempt */
  gdouble bestMatchingDistance;     /* square distance from reference coords to the offset of the best matching attempt */
  gdouble bestMatchingSumDiffValue; /* summ of the RGB channel differences of the best matching attempt */
  gdouble bestMatchingAvgColordiff;
  gdouble goodMatchingSumDiffValue; /* summ of the RGB channel differences of the best matching attempt plus 10 percent */
  gdouble veryNearDistance;         /* square of near radius to stop evaluation when exactly matching area is detected */
  GimpDrawable *refDrawable;
  GimpDrawable *targetDrawable;
} Context;

  /* size of the color relation aerea 13 x 13 pixels
   * for tuning located coordinates
   * (effective checked  area size is 9x9 pixels
   * with tuning offsets to the center shifted by upto +-2 pixels in any direction)
   *
   * 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
   *                   6,
   */
//#define GAP_COLOR_REL_ARRAY_SIZE   13
//#define GAP_COLOR_REL_CENTER_INDEX 6
//#define GAP_COLOR_REL_BORDER       2
       
  /* size of the color relation aerea 23 x 23 pixels
   * for tuning located coordinates
   * (effective checked  area size is 15x15 pixels
   * with tuning offsets to the center shifted by upto +-4 pixels in any direction)
   *
   * 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22
   *                .  .  .  .  .  .   11  .   .   .   .   .   .   .
   */
//#define GAP_COLOR_REL_ARRAY_SIZE   23
//#define GAP_COLOR_REL_CENTER_INDEX 11
//#define GAP_COLOR_REL_BORDER        4

  /* size of the color relation aerea 31 x 31 pixels
   * for tuning located coordinates
   * (effective checked  area size is 23x23 pixels
   * with tuning offsets to the center shifted by upto +-4 pixels in any direction)
   *
   * 0, 1, 2, 3,   4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,   27, 28, 29, 30
   *               .  .  .  .  .  .  .   .   .   .   .   15  .   .   .   .   .   .   .   .   .   .   . 
   */
//#define GAP_COLOR_REL_ARRAY_SIZE   31
//#define GAP_COLOR_REL_CENTER_INDEX 15
//#define GAP_COLOR_REL_BORDER        4

  /* size of the color relation aerea 37 x 37 pixels
   * for tuning located coordinates
   * (effective checked  area size is 29x29 pixels
   * with tuning offsets to the center shifted by upto +-4 pixels in any direction)
   *
   * 0, 1, 2, 3,   4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36
   *               .  .  .  .  .  .  .   .   .   .   .   .   .   .   18  .   .   .   .   .   .   .   .   .   .   .   .   .   .
   */
#define GAP_COLOR_REL_ARRAY_SIZE   37
#define GAP_COLOR_REL_CENTER_INDEX 18
#define GAP_COLOR_REL_BORDER        4


#define GAP_COLOR_REL_REQ          (GAP_COLOR_REL_ARRAY_SIZE - (1 + (2* GAP_COLOR_REL_BORDER)))
#define GAP_COLOR_REL_REQUIRED_PIXELS (GAP_COLOR_REL_REQ * GAP_COLOR_REL_REQ)



typedef struct ColorRelation
{
  guchar rgba   [GAP_COLOR_REL_ARRAY_SIZE][GAP_COLOR_REL_ARRAY_SIZE][4];
  gboolean isEmpty [GAP_COLOR_REL_ARRAY_SIZE][GAP_COLOR_REL_ARRAY_SIZE];
  gint32 crRed   [GAP_COLOR_REL_ARRAY_SIZE][GAP_COLOR_REL_ARRAY_SIZE];
  gint32 crGreen [GAP_COLOR_REL_ARRAY_SIZE][GAP_COLOR_REL_ARRAY_SIZE];
  gint32 crBlue  [GAP_COLOR_REL_ARRAY_SIZE][GAP_COLOR_REL_ARRAY_SIZE];
  gint32  pixelCount;
} ColorRelation;

static gdouble     p_calculate_average_colordiff(gdouble sumDiffValue, gint32 pixelCount);
static void        p_compare_regions (const GimpPixelRgn *refPR
                    ,const GimpPixelRgn *targetPR
                    ,Context *context);
static gdouble     p_calculate_distance_to_ref_coord(Context *context, gint32 px, gint32 py);
static void        p_attempt_locate_at_current_offset(Context *context, gint32 px, gint32 py);

static void        p_tuneLocatedCoordinates(Context *context, gint32  *targetX, gint32  *targetY);
static void        p_fetchColorArray(ColorRelation *colRelPtr, GimpPixelFetcher *pft, gint width, gint height, gint centerX, gint centerY, gboolean hasAlpha, const char *info);
static void        p_calculateColorRelationArrays(ColorRelation *colRelPtr, gint cx, gint cy);
static gdouble     p_calculateColorRelationArraysDifference(ColorRelation *refColRelPtr, ColorRelation *targetColRelPtr, gint dx, gint dy, gint32 *pixelCount);
static GapLocateTuneOffsElem * p_findTuneOffsShortList(Context *context, gint32  targetX, gint32  targetY);

/* ----------------------------------------
 * gap_locate_newGapLocateTuneOffsElem
 * ----------------------------------------
 */
GapLocateTuneOffsElem *
gap_locate_newGapLocateTuneOffsElem(gint idx, gint idy, gdouble relDiff)
{
   GapLocateTuneOffsElem *tuneOffsElem;
   
   
   tuneOffsElem = g_new(GapLocateTuneOffsElem, 1);

   tuneOffsElem->tuneOffsetX = idx;
   tuneOffsElem->tuneOffsetY = idy;
   tuneOffsElem->relDiff = relDiff;
   tuneOffsElem->valid = TRUE;
   tuneOffsElem->next = NULL;
   
   return(tuneOffsElem);
}  /* end gap_locate_newGapLocateTuneOffsElem */

/* ----------------------------------------
 * gap_locate_freeTuneOffsList
 * ----------------------------------------
 */
void
gap_locate_freeTuneOffsList(GapLocateTuneOffsElem *rootElem)
{
  GapLocateTuneOffsElem *tuneOffsElem;
  
  tuneOffsElem = rootElem;
  while(tuneOffsElem != NULL)
  {
    GapLocateTuneOffsElem *nextTuneOffsElem;
    
    nextTuneOffsElem = tuneOffsElem->next;
    g_free(tuneOffsElem);
    tuneOffsElem = nextTuneOffsElem;
  }
}  /* end gap_locate_freeTuneOffsList */



/* ---------------------------------
 * p_calculate_average_colordiff
 * ---------------------------------
 * calculate the average color difference for given sum of value differnces and pixel count
 */
static gdouble
p_calculate_average_colordiff(gdouble sumDiffValue, gint32 pixelCount)
{
  if (pixelCount > 0)
  {
    gdouble avgValue;
    gdouble averageColorDiff;
    
    avgValue = sumDiffValue  / (gdouble)pixelCount;
    averageColorDiff = avgValue / MAX_DIFF_VALUE_PER_PIXEL;
    return (averageColorDiff);
  }
  return (1.0);
  
}  /* end p_calculate_average_colordiff */


/* ---------------------------------
 * p_compare_regions
 * ---------------------------------
 * calculate summary Colorvalues difference for all opaque pixels
 * in the compared area region.
 */
static void
p_compare_regions (const GimpPixelRgn *refPR
                  ,const GimpPixelRgn *targetPR
                  ,Context *context)
{
  guint    row;
  guchar*  ref = refPR->data;   /* the reference drawable */
  guchar*  target = targetPR->data;   /* the target drawable */
  
  guint    commonWidth;
  guint    commonHeight;

  commonWidth = MIN(targetPR->w, refPR->w);
  commonHeight = MIN(targetPR->h, refPR->h);

//   if(gap_debug)
//   {
//     printf("region REF x:%d y:%d w:%d h:%d  TARGET x:%d y:%d w:%d h:%d  Common w:%d h:%d px:%d py:%d\n"
//        , (int)refPR->x
//        , (int)refPR->y
//        , (int)refPR->w
//        , (int)refPR->h
//        , (int)targetPR->x
//        , (int)targetPR->y
//        , (int)targetPR->w
//        , (int)targetPR->h
//        , (int)commonWidth
//        , (int)commonHeight
//        , (int)context->px
//        , (int)context->py
//        );
//   }


  for (row = 0; row < commonHeight; row++)
  {
    guint  col;
    guint  idxref;
    guint  idxtarget;
    
//    context->rowsProcessedCount += 1;
    idxref = 0;
    idxtarget = 0;
    for(col = 0; col < commonWidth; col++)
    {
      gboolean isCompareable;
      
      isCompareable = TRUE;
      
      if(refPR->bpp > 3)
      {
        if(ref[idxref +3] < OPACITY_LEVEL_UCHAR)
        {
          /* transparent reference pixel is not compared */
          isCompareable = FALSE;
        }
      }

      if(targetPR->bpp > 3)
      {
        if(target[idxtarget +3] < OPACITY_LEVEL_UCHAR)
        {
          /* transparent target pixel is not compared */
          isCompareable = FALSE;
        }
      }

      if (isCompareable == TRUE)
      {
        context->involvedPixelCount += 1;
        context->sumDiffValue += abs(ref[idxref]    - target[idxtarget]);
        context->sumDiffValue += abs(ref[idxref +1] - target[idxtarget +1]);
        context->sumDiffValue += abs(ref[idxref +2] - target[idxtarget +2]);

        
      }

      idxref    += refPR->bpp;
      idxtarget += targetPR->bpp;
    }

    /* check for early escape possibilty when current sumDiffValue gets too large 
     * Preformance note: 
     *  we could do this check already in the inner column based loop to detect escape
     *  possibilities a little earlier, but i guess
     *  overall performance might be better when cecks are done only once per row.
     */
    if (context->sumDiffValue > context->goodMatchingSumDiffValue)
    {
      if (context->bestMatchingPixelCount >= context->almostFullAreaPixelCount)
      {
        /* The (so far best matching) result was based on comparison
         * of almost all area pixels involved.
         * (Less pixels may be involved when:
         *   o) there are transparent pixels in the compared areas OR
         *   o) the compared areas are clipped at layer borders
         * )
         * At this point there is nearly no more chance for a better matching
         * average color diff in the current attempt, and we can
         * stop evaluating area at current offset for performance reasons.
         */
        context->cancelAttemptFlag = TRUE;
        //context->cancelAttemptCount += 1;
        return;
      }
      else if (context->sumDiffValue >= (context->bestMatchingSumDiffValue + context->bestMatchingSumDiffValue) )
      {
        /* the sumDiffValue is now alredy 2 times worse then the best match.
         * and the chances are low to get a better average color diff in the current attempt
         */
        context->cancelAttemptFlag = TRUE;
        //context->cancelAttemptCount += 1;
        return;
      }
    }



    ref += refPR->rowstride;
    target += targetPR->rowstride;

  }

}  /* end p_compare_regions */


/* ---------------------------------
 * p_calculate_distance_to_ref_coord
 * ---------------------------------
 * calculate the (square of) the distance between reference coordinates and px/py
 */
static gdouble
p_calculate_distance_to_ref_coord(Context *context, gint32 px, gint32 py)
{
  gdouble squareDistance;
  
  squareDistance =  (context->refX - px) * (context->refX - px);
  squareDistance += (context->refY - py) * (context->refY - py);
  
  return(squareDistance);
  
}  /* end p_calculate_distance_to_ref_coord */


/* --------------------------------------------
 * p_attempt_locate_at_current_offset
 * --------------------------------------------
 */
static void
p_attempt_locate_at_current_offset(Context *context, gint32 px, gint32 py)
{
  GimpPixelRgn refPR;
  GimpPixelRgn targetPR;
  gpointer  pr;
  gint      rx1, ry1, rWidth, rHeight;
  gint      tx1, ty1, tWidth, tHeight;
  gint      commonAreaWidth, commonAreaHeight;
  gint      fullShapeWidth,  fullShapeHeight;
  gint      rWidthRequired, rHeightRequired;
  gboolean  isIntersect;

  gint    leftShapeRadius;
  gint    upperShapeRadius;


  if (context->isFinishedFlag)
  {
    return;
  }
  
  fullShapeWidth = (2 * context->refShapeRadius);
  fullShapeHeight = (2 * context->refShapeRadius);

  /* calculate processing relevant intersecting reference / target rectangles */  

  isIntersect =
   gimp_rectangle_intersect((context->refX - context->refShapeRadius)  /* origin1 */
                          , (context->refY - context->refShapeRadius)
                          , fullShapeWidth               /*  width1 */
                          , fullShapeHeight              /* height1 */
                          ,0
                          ,0
                          ,context->refDrawable->width
                          ,context->refDrawable->height
                          ,&rx1
                          ,&ry1
                          ,&rWidth
                          ,&rHeight
                          );
  if (!isIntersect)
  {
    return;
  }
  
  leftShapeRadius = context->refX - rx1;
  upperShapeRadius = context->refY - ry1;

  isIntersect =
   gimp_rectangle_intersect((px - leftShapeRadius)  /* origin1 */
                          , (py - upperShapeRadius)
                          , rWidth               /*  width1 */
                          , rHeight               /* height1 */
                          ,0
                          ,0
                          ,context->targetDrawable->width
                          ,context->targetDrawable->height
                          ,&tx1
                          ,&ty1
                          ,&tWidth
                          ,&tHeight
                          );
  if (!isIntersect)
  {
    return;
  }
  
  commonAreaWidth = tWidth;
  commonAreaHeight = tHeight;

  // TODO test if 2/3 of the fullShapeWidth and fullShapeHeight is sufficient for usable results.
  // alternative1: maybe require  rWidth and rHeight
  // alternative2: maybe require  fullShapeWidth and fullShapeHeight
  rWidthRequired = (fullShapeWidth * 2) / 3;
  rHeightRequired = (fullShapeHeight * 2) / 3;
  
  if ((commonAreaWidth < rWidthRequired) 
  ||  (commonAreaHeight < rHeightRequired))
  {
    /* the common area is significant smaller than the reference shape 
     * skip the compare attempt in this case to avoid unpredictable results (near borders)
     */
    return;
  }

  

//   if(gap_debug)
//   {
//     printf("p_attempt_locate_at: px: %04d py:%04d\n"
//            "                     rx1:%04d ry1:%04d rWidth:%d rHeight:%d\n"
//            "                     tx1:%04d ty1:%04d tWidth:%d tHeight:%d\n"
//            "                     commonAreaWidth:%d commonAreaHeight:%d\n"
//       ,(int)px
//       ,(int)py
//       ,(int)rx1
//       ,(int)ry1
//       ,(int)rWidth
//       ,(int)rHeight
//       ,(int)tx1
//       ,(int)ty1
//       ,(int)tWidth
//       ,(int)tHeight
//       ,(int)commonAreaWidth
//       ,(int)commonAreaHeight
//       );
//   }

  /* rest 'per offset' values in the context */
  context->cancelAttemptFlag = FALSE;
  context->sumDiffValue = 0;
  context->involvedPixelCount = 0;
  context->currentDistance = p_calculate_distance_to_ref_coord(context, px, py);
  context->px = px;
  context->py = py;

  gimp_pixel_rgn_init (&refPR, context->refDrawable, rx1, ry1
                      , commonAreaWidth, commonAreaHeight
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );

  gimp_pixel_rgn_init (&targetPR, context->targetDrawable, tx1, ty1
                      , commonAreaWidth, commonAreaHeight
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );

  /* compare pixel areas in tiled portions via pixel region processing loops.
   */
  for (pr = gimp_pixel_rgns_register (2, &refPR, &targetPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
    if (context->cancelAttemptFlag)
    {
      break;
    }
    else
    {
      p_compare_regions(&refPR, &targetPR, context);
    }
  }

  if (pr != NULL)
  {
     /* NOTE:
      * early escaping from the loop with pr != NULL
      * leads to memory leaks due to unbalanced tile ref/unref calls.
      * the call to gap_gimp_pixel_rgns_unref cals unref on the current tile
      * (in the same way as gimp_pixel_rgns_process does)
      * but does not ref another available tile.
      */
    gap_gimp_pixel_rgns_unref (pr);
  
  }
  

  
  if ((context->involvedPixelCount >= context->requiredPixelCount)
  &&  (context->sumDiffValue <= context->goodMatchingSumDiffValue))
  {
    gdouble avgDiff;
    
    avgDiff = p_calculate_average_colordiff(context->sumDiffValue
                                           , context->involvedPixelCount
                                           );
    //if(gap_debug)
    //{
    //  printf("Match Candidate: pixlCount:%d sumDiffValue: %f AVG(%f) currentDistance: %f sqrt(%f) \n"
    //            "   goodMatchingSumDiffValue:%f bestMatching bMpixlCount:%d bMSumDiffValue: %f AVG(%f) bMDistance: %f sqr(%f)\n"
    //        ,(int)context->involvedPixelCount
    //        ,(float)context->sumDiffValue
    //        ,(float)avgDiff
    //        ,(float)context->currentDistance
    //        ,(float)sqrt(context->currentDistance)
    //        , (float) context->goodMatchingSumDiffValue
    //        ,(int)context->bestMatchingPixelCount
    //        ,(float)context->bestMatchingSumDiffValue
    //        ,(float)context->bestMatchingAvgColordiff
    //        ,(float)context->bestMatchingDistance
    //        ,(float)sqrt(context->bestMatchingDistance)
    //        );
    //        
    //}
       
    if((avgDiff  < context->bestMatchingAvgColordiff)
    || ((avgDiff <= context->bestMatchingAvgColordiff) && ( context->currentDistance < context->bestMatchingDistance)))
    {
      context->bestMatchingSumDiffValue = context->sumDiffValue;
      context->bestMatchingPixelCount = context->involvedPixelCount;
      context->bestX = px;
      context->bestY = py;
      context->goodMatchingSumDiffValue  = context->bestMatchingSumDiffValue * GOOD_MATCH_FACTOR;  /* 5 percent more than best */
      context->bestMatchingAvgColordiff = avgDiff;
 
      if(gap_debug)
      {
        printf("FOUND: bestX:%d bestY:%d squareDist:%d Dist:%d bestSquareDist:%d bestDist:%d\n"
               "             sumDiffValues:%d pixelCount:%d bestMatchingAvgColordiff:%.5f\n"
          , (int)context->bestX
          , (int)context->bestY
          , (int)context->currentDistance
          , (int)sqrt(context->currentDistance)
          , (int)context->bestMatchingDistance
          , (int)sqrt(context->bestMatchingDistance)
          , (int)context->bestMatchingSumDiffValue
          , (int)context->bestMatchingPixelCount
          , (float)context->bestMatchingAvgColordiff
          );
      }
      context->bestMatchingDistance = context->currentDistance;
       
      if ((context->currentDistance <= context->veryNearDistance)
      &&  (context->sumDiffValue == 0))
      {
        /* stop all further attempts on exact matching area when near reference origin */
        context->isFinishedFlag = TRUE;
      }
    }
  }

  
}  /* end p_attempt_locate_at_current_offset */



/* --------------------------------------------
 * p_tuneLocatedCoordinates                     DEPRECATED
 * --------------------------------------------
 * check color relations to nearest neighbour pixels
 * and compare them ref against target layer
 * where traget is shifted by 1 or 2 pixel offsets
 * to find best matching color relations.
 */
static void
p_tuneLocatedCoordinates(Context *context, gint32  *targetX, gint32  *targetY)
{
  GimpPixelFetcher *pftRef;
  GimpPixelFetcher *pftTarget;
  ColorRelation refColorRelation;
  ColorRelation targetColorRelation;
  gint    idx;
  gint    idy;
  gint32  requiredPixelCount;
  gint32  tuneOffsetX;
  gint32  tuneOffsetY;
  gdouble minRelDiff;

  gint32  tarX;
  gint32  tarY;

  tarX = *targetX;
  tarY = *targetY;
  
  /* init pixel fetchers */
  pftRef = gimp_pixel_fetcher_new (context->refDrawable, FALSE /* shadow */);
  pftTarget = gimp_pixel_fetcher_new (context->targetDrawable, FALSE /* shadow */);
  gimp_pixel_fetcher_set_edge_mode (pftRef, GIMP_PIXEL_FETCHER_EDGE_NONE);
  gimp_pixel_fetcher_set_edge_mode (pftTarget, GIMP_PIXEL_FETCHER_EDGE_NONE);


  /* copy small areas near the relevant coordinates
   * into arrays of the ColorRelation structures for further tuning processing
   */
  p_fetchColorArray(&refColorRelation, pftRef
                 , context->refDrawable->width, context->refDrawable->height
                 , context->refX, context->refY
                 , gimp_drawable_has_alpha(context->refDrawable->drawable_id)
                 , "REF"
                 );
  p_fetchColorArray(&targetColorRelation, pftTarget
                 , context->targetDrawable->width, context->targetDrawable->height
                 , tarX, tarY
                 , gimp_drawable_has_alpha(context->targetDrawable->drawable_id)
                 , "TAR"
                 );

  p_calculateColorRelationArrays(&refColorRelation, GAP_COLOR_REL_CENTER_INDEX, GAP_COLOR_REL_CENTER_INDEX);
       
  requiredPixelCount = GAP_COLOR_REL_REQUIRED_PIXELS;
  tuneOffsetX = 0;
  tuneOffsetY = 0;
  minRelDiff = (GAP_COLOR_REL_ARRAY_SIZE * GAP_COLOR_REL_ARRAY_SIZE * 3 * 256);
  for(idx = 0 - GAP_COLOR_REL_BORDER; idx <= GAP_COLOR_REL_BORDER; idx++)
  {
    for(idy = 0 - GAP_COLOR_REL_BORDER; idy <= GAP_COLOR_REL_BORDER; idy++)
    {
      gdouble relDiff;
      gint32     pixelCount;

      p_calculateColorRelationArrays(&targetColorRelation, GAP_COLOR_REL_CENTER_INDEX +idx, GAP_COLOR_REL_CENTER_INDEX +idy);

      relDiff = p_calculateColorRelationArraysDifference(&refColorRelation, &targetColorRelation, idx, idy, &pixelCount);
      

      
      if ((pixelCount >= requiredPixelCount) && (relDiff < minRelDiff))
      {
        minRelDiff = relDiff;
        tuneOffsetX = idx;
        tuneOffsetY = idy;
        
        if(gap_debug)
	{
	  printf("tuneLocatedCoordinates idx:%d, idy:%d, pixelCount:%d, requiredPixelCount:%d, relDiff:%f\n"
	           ,(int)idx
	           ,(int)idy
	           ,(int)pixelCount
	           ,(int)requiredPixelCount
	           ,(float)relDiff
	           );
        }
        
        
      }
    }
  }


  gimp_pixel_fetcher_destroy (pftRef);
  gimp_pixel_fetcher_destroy (pftTarget);

       
   *targetX = tarX + tuneOffsetX;
   *targetY = tarY + tuneOffsetY;
       
}  /* end p_tuneLocatedCoordinates */       
       


/* ---------------------------------------
 * p_fetchColorArray
 * ---------------------------------------
 * copy a small pixel environment area at centerX/Y of rgba pixels via pixelfetcher
 * to the ColorRelation structure where transparent pixels and coordinates 
 * outside the layer width/height are marked as empty.
 */
static void
p_fetchColorArray(ColorRelation *colRelPtr, GimpPixelFetcher *pft, gint width, gint height, gint centerX, gint centerY, gboolean hasAlpha, const char *info)
{
   gint idx;
   gint idy;
      
   for(idy = 0; idy < GAP_COLOR_REL_ARRAY_SIZE; idy ++)
   
   {
     guchar debugLine[GAP_COLOR_REL_ARRAY_SIZE +1];
     for(idx = 0; idx < GAP_COLOR_REL_ARRAY_SIZE; idx ++)
     {
         gint px;
         gint py;
         
         px = (centerX + idx) - GAP_COLOR_REL_CENTER_INDEX; 
         py = (centerY + idy) - GAP_COLOR_REL_CENTER_INDEX;
         
         if(gap_debug)
         {
           if ((idx == 0) && (idy ==0))
           {
             printf("\np_fetchColorArray px:%d, py:%d %s\n", (int)px, (int)py, info);
           }
         }
         
         if ((px >= 0) && (px < width) && (py >= 0) && (py < height))
         {
           guchar *environmentPixelRgbaPtr;
         
           environmentPixelRgbaPtr = &colRelPtr->rgba[idx][idy][0];
           gimp_pixel_fetcher_get_pixel (pft, px, py, environmentPixelRgbaPtr);
           if ((hasAlpha) && (environmentPixelRgbaPtr[3] == 0))  /* full transparent alpha channel */
           {
             colRelPtr->isEmpty[idx][idy] = TRUE;
             debugLine[idx] = '.';
           }
           else
           {
             colRelPtr->isEmpty[idx][idy] = FALSE;
             debugLine[idx] = '#';
           }
         }
         else
         {
           /* set alpha channel fully transparent when outside layer */
           colRelPtr->isEmpty[idx][idy] = TRUE;
           debugLine[idx] = 'o';
         }

     }
     if(gap_debug)
     {
       debugLine[GAP_COLOR_REL_ARRAY_SIZE] = '\0';
       printf ("%s\n", &debugLine[0]);
     }
   }

}  /* end p_fetchColorArray */


/* ---------------------------------------
 * p_calculateColorRelationArrays
 * ---------------------------------------
 * calculate relation of all pixels in the ColorRelation arrays
 * as R,G,B differences to the pixel at position cx/cy.
 */
static void
p_calculateColorRelationArrays(ColorRelation *colRelPtr, gint cx, gint cy)
{
   gint centerRed;
   gint centerGreen;
   gint centerBlue;
   gint idx;
   gint idy;
      
   centerRed   = (gint)colRelPtr->rgba[cx][cy][0];
   centerGreen = (gint)colRelPtr->rgba[cx][cy][1];
   centerBlue  = (gint)colRelPtr->rgba[cx][cy][2];
   
   colRelPtr->pixelCount = 0;
   
   for(idx = 0; idx < GAP_COLOR_REL_ARRAY_SIZE; idx ++)
   {
     for(idy = 0; idy < GAP_COLOR_REL_ARRAY_SIZE; idy ++)
     {
         if (colRelPtr->isEmpty[idx][idy] != TRUE)
         {
           gint red;
           gint green;
           gint blue;
           
           red   = (gint)colRelPtr->rgba[idx][idy][0];
           green = (gint)colRelPtr->rgba[idx][idy][1];
           blue  = (gint)colRelPtr->rgba[idx][idy][2];
           
           colRelPtr->crRed[idx][idy]   = centerRed   - red;
           colRelPtr->crGreen[idx][idy] = centerGreen - green;
           colRelPtr->crBlue[idx][idy]  = centerBlue  - blue;
           
           colRelPtr->pixelCount++;
         }
     }
   }

}  /* end p_calculateColorRelationArrays */


/* ----------------------------------------
 * p_calculateColorRelationArraysDifference
 * ----------------------------------------
 * calculate relative difference by comparing 2 ColorRelation Arrays.
 * as average of all relation differences in R,G,B channels in all "NOT EMPTY" pixels.
 * EMPTY pixels are either fully transparent or outside the layer boundaries.
 * Note that the effective compared area size is smaller than the color relation Arrays
 * because the comparison is done with the center of the target ColorRelation shifted by dx, dy
 * (where dx and dy are in the range from -2 to +2 pixels)
 * For a full size GAP_COLOR_REL_ARRAY_SIZE of 13x13 the effective compared area is 9x9 pixels.
 */
static gdouble
p_calculateColorRelationArraysDifference(ColorRelation *refColRelPtr, ColorRelation *targetColRelPtr, gint dx, gint dy, gint32 *pixelCount)
{
   gint rdx;
   gint rdy;
   gint tdx;
   gint tdy;
   gint32 count;
   
   gdouble absDiff;
   gdouble relDiff;

   relDiff = 0;
   absDiff = 0;
   count = 0;
   for(rdx = GAP_COLOR_REL_BORDER; rdx < GAP_COLOR_REL_ARRAY_SIZE - GAP_COLOR_REL_BORDER; rdx ++)
   {
     tdx = rdx + dx;
     for(rdy = GAP_COLOR_REL_BORDER; rdy < GAP_COLOR_REL_ARRAY_SIZE - GAP_COLOR_REL_BORDER; rdy ++)
     {
       tdy = rdy + dy;
       if ((refColRelPtr->isEmpty[rdx][rdy] != TRUE)
       &&  (targetColRelPtr->isEmpty[tdx][tdy] != TRUE))
       {
         count++;
         absDiff += abs(refColRelPtr->crRed[rdx][rdy]   - targetColRelPtr->crRed[tdx][tdy]);
         absDiff += abs(refColRelPtr->crGreen[rdx][rdy] - targetColRelPtr->crGreen[tdx][tdy]);
         absDiff += abs(refColRelPtr->crBlue[rdx][rdy]  - targetColRelPtr->crBlue[tdx][tdy]);
       }
     }
   }
   
   if (count > 0)
   {
     relDiff = absDiff / (count * 255 * 3);
   }
   *pixelCount = count;
   return (relDiff);
   
}  /* end p_calculateColorRelationArraysDifference */


/* --------------------------------------------
 * p_findTuneOffsShortList
 * --------------------------------------------
 * check color relations to nearest neighbour pixels
 * and compare them ref against target layer
 * where traget is shifted by 1,2,3 or 4 pixel offsets
 * to find a list of the offset variants sorted by best matching color relations at begin of the returned list.
 *
 * (for tuning purpose)
 * Note the caller is responsible to free the returned list
 */
static GapLocateTuneOffsElem *
p_findTuneOffsShortList(Context *context, gint32  targetX, gint32  targetY)
{
  GapLocateTuneOffsElem    *tuneOffsShortList; 
  GimpPixelFetcher *pftRef;
  GimpPixelFetcher *pftTarget;
  ColorRelation refColorRelation;
  ColorRelation targetColorRelation;
  gint    offsets[] = { 0, -1, 1, -2, 2, -3, 3, -4, 4};
  gint    iidx;
  gint    iidy;
  gint    idx;
  gint    idy;
  gint32  requiredPixelCount;

  gint32  tarX;
  gint32  tarY;

  tarX = targetX;
  tarY = targetY;
  
  /* init pixel fetchers */
  pftRef = gimp_pixel_fetcher_new (context->refDrawable, FALSE /* shadow */);
  pftTarget = gimp_pixel_fetcher_new (context->targetDrawable, FALSE /* shadow */);
  gimp_pixel_fetcher_set_edge_mode (pftRef, GIMP_PIXEL_FETCHER_EDGE_NONE);
  gimp_pixel_fetcher_set_edge_mode (pftTarget, GIMP_PIXEL_FETCHER_EDGE_NONE);


  /* copy small areas near the relevant coordinates
   * into arrays of the ColorRelation structures for further tuning processing
   */
  p_fetchColorArray(&refColorRelation, pftRef
                 , context->refDrawable->width, context->refDrawable->height
                 , context->refX, context->refY
                 , gimp_drawable_has_alpha(context->refDrawable->drawable_id)
                 , "REF"
                 );
  p_fetchColorArray(&targetColorRelation, pftTarget
                 , context->targetDrawable->width, context->targetDrawable->height
                 , tarX, tarY
                 , gimp_drawable_has_alpha(context->targetDrawable->drawable_id)
                 , "TAR"
                 );

  p_calculateColorRelationArrays(&refColorRelation, GAP_COLOR_REL_CENTER_INDEX, GAP_COLOR_REL_CENTER_INDEX);
       
  requiredPixelCount = GAP_COLOR_REL_REQUIRED_PIXELS;
  tuneOffsShortList = NULL;
  
  for(iidy = 0; iidy < 9; iidy++)
  {
    idy = offsets[iidy];
    for(iidx = 0; iidx < 9; iidx++)
    {
      gdouble relDiff;
      gint32     pixelCount;
      
      idx = offsets[iidx];

      p_calculateColorRelationArrays(&targetColorRelation, GAP_COLOR_REL_CENTER_INDEX +idx, GAP_COLOR_REL_CENTER_INDEX +idy);

      relDiff = p_calculateColorRelationArraysDifference(&refColorRelation, &targetColorRelation, idx, idy, &pixelCount);
      
      if(gap_debug)
      {
        printf("p_findTuneOffsShortList: idx:%d idy:%d relDiff:%f pixelCount:%d requiredPixelCount:%d\n"
          ,(int)idx
          ,(int)idy
          ,(float)relDiff
          ,(int)pixelCount
          ,(int)requiredPixelCount
          );
      }

      
      if (pixelCount >= requiredPixelCount)
      {
        if (tuneOffsShortList == NULL)
        {
          /* on empty list create the list root element */
          tuneOffsShortList = gap_locate_newGapLocateTuneOffsElem(idx, idy, relDiff);
        }
        else
        {
          GapLocateTuneOffsElem    *tuneOffsElem;

          for(tuneOffsElem = tuneOffsShortList; tuneOffsElem != NULL; tuneOffsElem = tuneOffsElem->next)
          {
            if(relDiff < tuneOffsElem->relDiff)
            {
              GapLocateTuneOffsElem    *newGapLocateTuneOffsElem;
              
              /* copy existing element to a new element */
              newGapLocateTuneOffsElem = gap_locate_newGapLocateTuneOffsElem(tuneOffsElem->tuneOffsetX, tuneOffsElem->tuneOffsetY, tuneOffsElem->relDiff);
              /* and insert the new element after the current element into the list */
              newGapLocateTuneOffsElem->next = tuneOffsElem->next;
              tuneOffsElem->next = newGapLocateTuneOffsElem;
              
              /* replace the current element content with the "better" relDiff value and offsets */
              tuneOffsElem->relDiff = relDiff;
              tuneOffsElem->tuneOffsetX = idx;
              tuneOffsElem->tuneOffsetY = idy;

              break;
            }
            if (tuneOffsElem->next == NULL)
            {
              tuneOffsElem->next = gap_locate_newGapLocateTuneOffsElem(idx, idy, relDiff);
              break;
            }
          }
        }
        
      }
    }
  }


  gimp_pixel_fetcher_destroy (pftRef);
  gimp_pixel_fetcher_destroy (pftTarget);

  return (tuneOffsShortList);
       
}  /* end p_findTuneOffsShortList */    


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
 * offsets are recorded in the short list that is returned to the caller
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
gap_locate_FindTuneOffsShortList(gint32 activeDrawableId, gint32 referenceLayerId, GapPixelCoords *refCoord, GapPixelCoords *currCoord, gdouble qFactor)
{
  GapLocateTuneOffsElem *tuneOffsShortList;
  GapLocateTuneOffsElem *tuneOffsElem;
  gint     idx;
  gboolean firstIvalidElemLogged;

  Context contextData;
  Context *context;

  
  if(gap_debug)
  {
    printf("\ngap_locate_FindTuneOffsShortList START ref.px:%d ref.py:%d cur.px:%d cur.py:%d qFactor:%f\n"
      ,(int)refCoord->px
      ,(int)refCoord->py
      ,(int)currCoord->px
      ,(int)currCoord->py
      ,(float)qFactor
      );
  }

  /* partly init Context (for color relation processing) */
  context = &contextData;
  context->refX = refCoord->px;
  context->refY = refCoord->py;
  context->bestX = refCoord->px;
  context->bestY = refCoord->py;
  context->refDrawable = gimp_drawable_get(referenceLayerId);
  context->targetDrawable = gimp_drawable_get(activeDrawableId);



  /* get the candidates for tunining offsets as sorted list (best matchers first) */
  tuneOffsShortList = p_findTuneOffsShortList(context, currCoord->px, currCoord->py);
  
  idx = 0;
  firstIvalidElemLogged = FALSE;
  for(tuneOffsElem = tuneOffsShortList; tuneOffsElem != NULL; tuneOffsElem = tuneOffsElem->next)
  {
    if (tuneOffsElem->relDiff > tuneOffsShortList->relDiff * qFactor)
    {
      /* mark elements with too big difference as invalid */
      tuneOffsElem->valid = FALSE;
    }
    
    if(gap_debug)
    {
      if ((tuneOffsElem->valid) || (firstIvalidElemLogged == FALSE))
      {
        printf("  %d) ref.px:%d ref.py:%d cur.px:%d cur.py:%d tuneOffsetX:%d tuneOffsetY:%d relDiff:%f requiredRelDiff <= :%f"
          ,(int)idx
          ,(int)refCoord->px
          ,(int)refCoord->py
          ,(int)currCoord->px
          ,(int)currCoord->py
          ,(int)tuneOffsElem->tuneOffsetX
          ,(int)tuneOffsElem->tuneOffsetY
          ,(float)tuneOffsElem->relDiff
          ,(float)(tuneOffsShortList->relDiff * qFactor)
          );
        if(tuneOffsElem->valid)
        {
          printf (" [VALID]\n");
        }
        else
        {
          printf (" [invalid]\n");
          firstIvalidElemLogged = TRUE;  /* do not log further invalid elements in the list */
        }
      }
    }
    
    idx++;

  }

  if(context->refDrawable != NULL)
  {
    gimp_drawable_detach(context->refDrawable);
  }
  if(context->targetDrawable != NULL)
  {
    gimp_drawable_detach(context->targetDrawable);
  }
  
  return(tuneOffsShortList);

}  /* end gap_locate_FindTuneOffsShortList */

/* -------------------------------------------------------
 * gap_locatePickNearestToPredictedCoordinateFromShortlist
 * -------------------------------------------------------
 * IN/OUT: trkCoord  is adjusted (tuned) with the tuning offest that leads to the
 * nearest coordinate compared to the predictedCoord.
 */
void 
gap_locatePickNearestToPredictedCoordinateFromShortlist(GapPixelCoords *trkCoord, GapPixelCoords *predictedCoord, 
   GapLocateTuneOffsElem *shortListP1, gint width, gint height)
{
  GapLocateTuneOffsElem *elemP1;
  
  GapPixelCoords untunedCoord;
  untunedCoord.px = trkCoord->px;
  untunedCoord.py = trkCoord->py;
  gdouble   minSqDist;
  
  minSqDist = (width + height) * (width + height);

  for(elemP1 = shortListP1; elemP1 != NULL; elemP1 = elemP1->next)
  {
    GapPixelCoords tunedCoord;
    gdouble sqrDistance;
    if (elemP1->valid != TRUE) 
    { 
      continue;  /* skip low quality list entries */
    }
    tunedCoord.px =  untunedCoord.px + elemP1->tuneOffsetX;
    tunedCoord.py =  untunedCoord.py + elemP1->tuneOffsetY;
    
    sqrDistance = gap_geo_calculateSqrDist(&tunedCoord, predictedCoord);
    if (sqrDistance < minSqDist)
    {
      minSqDist = sqrDistance;
      trkCoord->px = tunedCoord.px;
      trkCoord->py = tunedCoord.py;
    }
  }
  
  if(gap_debug)
  {
    printf("gap_locatePickNearestToPredicted predictedCoord: %d %d, pickedCoord: %d %d\n"
      , (int)predictedCoord->px
      , (int)predictedCoord->py
      , (int)trkCoord->px
      , (int)trkCoord->py
      );
  }

} /* end gap_locatePickNearestToPredictedCoordinateFromShortlist */


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
gap_locate_check_strong_shortlist(GapLocateTuneOffsElem *shortListP1, gdouble nearlySameFactor, gdouble strongRelDiff)
{
  GapLocateTuneOffsElem *elemP1;
  gint                   idx;
  gboolean               isStrong;

  idx = 0;
  isStrong = FALSE;
  for(elemP1 = shortListP1; elemP1 != NULL; elemP1 = elemP1->next)
  {
    if (idx == 0)
    {
      if (elemP1->relDiff > strongRelDiff)
      {
        /* the 1st element is already poor matching, stop further checks and return FALSE */ 
        isStrong = FALSE;
        break;
      }
      else 
      {
        isStrong = TRUE; /* assume TRUE because the 1st element is matching good enough */
      }
    }
    else
    {
      if (elemP1->relDiff <= shortListP1->relDiff * nearlySameFactor)
      {
        /* the next (2nd) element has nearly same relDiff value
         * Therfore the 1st element is considered as WEAK because it is not the only one
         */
        isStrong = FALSE; 
      }
      break;
    }
    idx++;
  }
  return (isStrong);
  
}  /* end gap_locate_check_strong_shortlist */

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
  )
{
  Context contextData;
  Context *context;
  gdouble averageColorDiff;
  gdouble maxPixelCount;
  gint32  shapeDiameter;
  gint32  fullAreaPixelCount;
  gint32  dx;
  gint32  dy;
  
  
  *targetX = refX;
  *targetY = refY;
  
  shapeDiameter = 1 + (refShapeRadius + refShapeRadius);
  fullAreaPixelCount = (shapeDiameter) * (shapeDiameter);
  
  /* init Context */
  context = &contextData;
  context->refShapeRadius = refShapeRadius;
  context->refX = refX;
  context->refY = refY;
  context->bestX = refX;
  context->bestY = refY;
  context->cancelAttemptCount = 0;
  context->rowsProcessedCount = 0;
  context->cancelAttemptFlag = FALSE;
  context->isFinishedFlag = FALSE;
  context->requiredPixelCount = (fullAreaPixelCount * 30) / 100;
  context->almostFullAreaPixelCount = (fullAreaPixelCount * 90) / 100;
  context->involvedPixelCount = 0;
  context->sumDiffValue = 0;
  context->currentDistance = 0;
  context->bestMatchingPixelCount = 0;
  context->veryNearDistance = (2 * 2);

  context->refDrawable = gimp_drawable_get(refDrawableId);
  context->targetDrawable = gimp_drawable_get(targetDrawableId);
  
  maxPixelCount = MAX(context->refDrawable->width, context->targetDrawable->width)
                * MAX(context->refDrawable->height, context->targetDrawable->height);

  context->bestMatchingAvgColordiff = 1.0; /* worst case */               
  context->bestMatchingSumDiffValue = maxPixelCount * MAX_DIFF_VALUE_PER_PIXEL;
  context->goodMatchingSumDiffValue = context->bestMatchingSumDiffValue;
  context->bestMatchingDistance = maxPixelCount;
  
  averageColorDiff = 1.0;
  
  if(gap_debug)
  {
      printf("gap_locateAreaWithinRadiusWithOffset START: refDrawableId:%d targetDrawableId:%d\n"
             "                           refX:%d refY:%d refShapeRadius:%d\n"
             "                           requiredPixelCount:%d almostFullAreaPixelCount:%d  fullAreaPixelCount:%d\n"
        , (int)refDrawableId
        , (int)targetDrawableId
        , (int)context->refX
        , (int)context->refY
        , (int)refShapeRadius
        , (int)context->requiredPixelCount
        , (int)context->almostFullAreaPixelCount
        , (int)fullAreaPixelCount
        );
  }
  
  
  for(dx = 0; dx <= targetMoveRadius; dx ++)
  {
    if (context->isFinishedFlag) 
    { 
      break; 
    }

    for(dy = 0; dy <= targetMoveRadius; dy++)
    {
 
      p_attempt_locate_at_current_offset(context, (offsetX + refX) + dx, (offsetY + refY) + dy);
      if (context->isFinishedFlag)
      { 
        break;
      }
      
      if (dx > 0)
      {
        p_attempt_locate_at_current_offset(context, (offsetX + refX) - dx, (offsetY + refY) + dy);
        if (context->isFinishedFlag) 
        {
          break; 
        }
      }
      
      if (dy > 0)
      {
        p_attempt_locate_at_current_offset(context, (offsetX + refX) + dx, (offsetY + refY) - dy);
        if (context->isFinishedFlag) 
        {
          break; 
        }
      }
      
      if ((dx > 0) && (dy > 0))
      {
        p_attempt_locate_at_current_offset(context, (offsetX + refX) - dx, (offsetY + refY) - dy);
        if (context->isFinishedFlag) 
        {
          break; 
        }
      }
      
    }
  }
  
  if (context->bestMatchingPixelCount > 0)
  {
    *targetX = context->bestX;
    *targetY = context->bestY;
    averageColorDiff = p_calculate_average_colordiff(context->bestMatchingSumDiffValue
                                    , context->bestMatchingPixelCount
                                    );

    /// tuning did not lead to better results in most cases, 
    /// therfore disabled for now... (and for performance reasons)
    /* try to improve the located target coordinates 
     * by checking color relations to nearest neighbour pixels
     * (this may shift the result by 1 or 2 pixels)
     */
    //// p_tuneLocatedCoordinates(context, targetX, targetY);
    
    if(gap_debug)
    {
      printf("gap_locateAreaWithinRadiusWithOffset Result: targetX:%d targetY:%d averageColorDiff:%.5f\n"
             "                           bestX:%d bestY:%d tuneX:%d tuneY:%d\n"
             "                           sumDiffValues:%d pixelCount:%d\n"
             "                           refX:%d refY:%d  cancelAttemptCount:%d rowsProcessedCount:%d\n"
             "                           requiredPixelCount:%d almostFullAreaPixelCount:%d\n"
        , (int)(*targetX)
        , (int)(*targetY)
        , (float)averageColorDiff
        , (int)context->bestX
        , (int)context->bestY
        , (int)(*targetX) - context->bestX
        , (int)(*targetY) - context->bestY
        , (int)context->bestMatchingSumDiffValue
        , (int)context->bestMatchingPixelCount
        , (int)context->refX
        , (int)context->refY
        , (int)context->cancelAttemptCount
        , (int)context->rowsProcessedCount
        , (int)context->requiredPixelCount
        , (int)context->almostFullAreaPixelCount
        );
    }
  }
  else
  {
    if(gap_debug)
    {
      printf("gap_locateAreaWithinRadiusWithOffset * NOTHING FOUND *\n");
    }
  }


  if(context->refDrawable != NULL)
  {
    gimp_drawable_detach(context->refDrawable);
  }
  if(context->targetDrawable != NULL)
  {
    gimp_drawable_detach(context->targetDrawable);
  }

  return (averageColorDiff);

}  /* end gap_locateAreaWithinRadiusWithOffset */


/* --------------------------------------------
 * gap_locateAreaWithinRadius
 * --------------------------------------------
 * processing starts at reference coords and continues
 * outwards upto targetMoveRadius for 4 quadrants.
 * 
 * returns average color difference (0.0 upto 1.0)
 *    where 0.0 indicates exact matching area
 *      and 1.0 indicates all pixel have maximum color diff (when comaring full white agains full black area)
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
  )
{
  gdouble avgColordiff;
  
  avgColordiff =
    gap_locateAreaWithinRadiusWithOffset(refDrawableId
       , refX
       , refY
       , refShapeRadius
       , targetDrawableId
       , targetMoveRadius
       , targetX
       , targetY
       , 0
       , 0
       );
  return (avgColordiff);
  
}  /* end gap_locateAreaWithinRadius */


/* --------------------------------------------
 * gap_locateColordiffOpaquePixels
 * --------------------------------------------
 * compares opaque pixels of the specified refDrawableId and targetDrawableId.
 * Both drawables shall have same size 
 * Note that offsets of the layers within the image are relevant for processing.
 * (pixels are compared at corresponding coordinates which means having the same
 * same position in the image)
 * in case there are less pixels involved in the comparison than the specified requiredPixelCount
 * than value 1.0 is returned (to indicate "worst case" 
 * for situations where not enough pixels could be compared because there are too few opaque pixels
 * at corresponding coordinates, or there is no intersection at all)
 * 
 * returns average color difference (0.0 upto 1.0)
 *    where 0.0 indicates exact matching area
 *      and 1.0 indicates all pixel have maximum color diff (when comparing full white against full black area)
 */
gdouble
gap_locateColordiffOpaquePixels(gint32  refDrawableId
  , gint32  targetDrawableId
  , gint32  requiredPixelCount
  , gint32 *comparedPixelCount
  )
{
  Context contextData;
  Context *context;
  gdouble averageColorDiff;
  gdouble maxPixelCount;

  GimpPixelRgn refPR;
  GimpPixelRgn targetPR;
  gpointer  pr;
  gint      offsetRef_x;
  gint      offsetRef_y;
  gint      offsetTarget_x;
  gint      offsetTarget_y;
  gint      deltaOrigin_x, deltaOrigin_y;
  gint      tmpOrigin_x, tmpOrigin_y;
  gint      commonAreaWidth, commonAreaHeight;
  gint      rx1, ry1;    /* pixel region origin in the reference drawable */
  gint      tx1, ty1;    /* pixel region origin in the target drawable */
  gboolean  isIntersect;
  
  *comparedPixelCount = 0;
  
  /* init Context for full drawable area compare processing
   * note that context is designed for the more complex locating procedures
   * therefore most context elements are not used this time...
   */
  context = &contextData;
  context->refShapeRadius = 1;         /* not relevant for full area compare */
  context->refX = 0;                   /* not relevant for full area compare */
  context->refY = 0;                   /* not relevant for full area compare */
  context->bestX = 0;                  /* not relevant for full area compare */
  context->bestY = 0;                  /* not relevant for full area compare */
  context->cancelAttemptCount = 0;     /* not relevant for full area compare */
  context->cancelAttemptFlag = FALSE;  /* IGNORED for full area compare */
  context->isFinishedFlag = FALSE;     /* IGNORED for full area compare */
  context->requiredPixelCount = requiredPixelCount;
  context->involvedPixelCount = 0;
  context->sumDiffValue = 0;
  context->currentDistance = 0;            /* not relevant for full area compare */
  context->bestMatchingPixelCount = 0;     /* not relevant for full area compare */
  context->veryNearDistance = (2 * 2);     /* not relevant for full area compare */

  context->refDrawable = gimp_drawable_get(refDrawableId);
  context->targetDrawable = gimp_drawable_get(targetDrawableId);
  
  maxPixelCount = MAX(context->refDrawable->width, context->targetDrawable->width)
                * MAX(context->refDrawable->height, context->targetDrawable->height);
                
  context->bestMatchingAvgColordiff = 1.0; /* worst case */               
  context->bestMatchingSumDiffValue = maxPixelCount * MAX_DIFF_VALUE_PER_PIXEL;
  context->goodMatchingSumDiffValue = context->bestMatchingSumDiffValue;
  context->bestMatchingDistance = maxPixelCount;
  
  averageColorDiff = 1.0;


  /* get offsets within the image */
  gimp_drawable_offsets (refDrawableId, &offsetRef_x, &offsetRef_y);
  gimp_drawable_offsets (targetDrawableId, &offsetTarget_x, &offsetTarget_y);

  
  /* delta origin (is target origin relative to reference origin) */
  deltaOrigin_x = offsetTarget_x - offsetRef_x;
  deltaOrigin_y = offsetTarget_y - offsetRef_y;

  if(gap_debug)
  {
    printf("gap_locateColordiffOpaquePixels sizeTarget: (%d x %d) offsetTarget x:%d y:%d  sizeRef: (%d x %d)offsetRef x:%d y:%d\n"
      , (int)context->targetDrawable->width
      , (int)context->refDrawable->height
      , (int)offsetTarget_x
      , (int)offsetTarget_y
      , (int)context->refDrawable->width
      , (int)context->refDrawable->height
      , (int)offsetRef_x
      , (int)offsetRef_y
      );
  }

  isIntersect =
   gimp_rectangle_intersect(0, 0                            /* origin1 x, y */
                          , context->refDrawable->width     /*  width1 */
                          , context->refDrawable->height    /* height1 */
                          , deltaOrigin_x, deltaOrigin_y
                          , context->targetDrawable->width
                          , context->targetDrawable->height
                          ,&tmpOrigin_x
                          ,&tmpOrigin_y
                          ,&commonAreaWidth
                          ,&commonAreaHeight
                          );
  if (!isIntersect)
  {
    /* rectangeles do not intersect, deliver "worst case" value 1.0 */
    return (1.0);
  }  

  rx1 = deltaOrigin_x;
  tx1 = 0;
  if (deltaOrigin_x < 0)
  {
    rx1 = 0;
    tx1 = abs(deltaOrigin_x);
  }

  ry1 = deltaOrigin_y;
  ty1 = 0;
  if (deltaOrigin_y < 0)
  {
    ry1 = 0;
    ty1 = abs(deltaOrigin_y);
  }

  
  gimp_pixel_rgn_init (&refPR, context->refDrawable
                      , rx1, ry1      /* origin x, y top left corner*/
                      , commonAreaWidth, commonAreaHeight
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );

  gimp_pixel_rgn_init (&targetPR, context->targetDrawable
                      , tx1, ty1      /* origin x, y top left corner*/
                      , commonAreaWidth, commonAreaHeight
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );

  /* compare pixel areas in tiled portions via pixel region processing loops.
   */
  for (pr = gimp_pixel_rgns_register (2, &refPR, &targetPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
    p_compare_regions(&refPR, &targetPR, context);
  }
  
  /* deliver the number of opaque pixels that actually were involved
   * in the comparison (having opaque alpha channel at corresponding coordinates
   * in both layers
   */
  *comparedPixelCount = context->involvedPixelCount;
  
  if (context->involvedPixelCount >= context->requiredPixelCount)
  {
    averageColorDiff = p_calculate_average_colordiff( context->sumDiffValue
                                                    , context->involvedPixelCount
                                                    );
  }
  
  if(gap_debug)
  {
    printf("gap_locateColordiffOpaquePixels Result: averageColorDiff:%.15f\n"
           "                           involvedPixelCount:%d (requiredPixelCount:%d)\n"
      , (float)averageColorDiff
      , (int)context->involvedPixelCount
      , (int)context->requiredPixelCount
      );
  }



  if(context->refDrawable != NULL)
  {
    gimp_drawable_detach(context->refDrawable);
  }
  if(context->targetDrawable != NULL)
  {
    gimp_drawable_detach(context->targetDrawable);
  }

  return (averageColorDiff);

}  /* end gap_locateColordiffOpaquePixels */
