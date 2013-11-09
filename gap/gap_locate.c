/* gap_locate.c
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
 * along with this program; if not, write to the Free Software
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * version 2.7.0;             hof: created
 */

/* SYTEM (UNIX) includes */
#include "string.h"
/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_lib_common_defs.h"
#include "gap_locate.h"
#include "gap_locate2.h"
#include "gap_colordiff.h"
#include "gap_lib.h"
#include "gap_image.h"
#include "gap_layer_copy.h"
#include "gap_libgapbase.h"

#define MAX_SHAPE_TABLE_SIZE (1 + (GAP_LOCATE_MAX_REF_SHAPE_RADIUS * 2))
#define OPACITY_LEVEL_UCHAR 50
#define COLORDIFF_BREAK_THRESHOLD_FACTOR 1.1

extern      int gap_debug; /* ==0  ... dont print debug infos */

typedef struct GapLocateColor {
  guchar rgba[4];   /* the color channels */
  } GapLocateColor;


typedef struct GapLocateContext { /* nickname: lctx */
     GimpDrawable *refDrawable;
     GimpDrawable *targetDrawable;
     gint32        refX;
     gint32        refY;
     gint32        refShapeRadius;
     gint32        targetMoveRadius;
     gint32        targetX;
     gint32        targetY;
     gdouble       minColordiff;
     gdouble       breakAtColordiff;
     gdouble       refPixelColordiffThreshold;
     gdouble       edgeColordiffThreshold;
     gdouble       tmpColordiffBreakThreshold;  /* cancel area compare if average colordiff goes above this threshold */
     

     /* common clip rectangle offset in the tareget drawable */
     gint32        targetOffX;
     gint32        targetOffY;

     /* transformation offsets to convert target coordinates (tx/ty) to indexes for refColorTab */
     gint32        transformOffX;
     gint32        transformOffY;
   
     /* common clip rectangle sizes */
     gint32        commonAreaWidth;  
     gint32        commonAreaHeight;

     gdouble       commonAreaSumColordiff;
     gint32        commonAreaComparedPixelCount;

     GimpPixelFetcher *pftRef;
     GimpPixelFetcher *pftTarget;
     GapLocateColor   *refColorPtr;
     GapLocateColor    refColorTab[MAX_SHAPE_TABLE_SIZE][MAX_SHAPE_TABLE_SIZE];

     gint32            pickedTryNumber;
     gint32            pickedPixelCount;
     gint32            countTries;
     gint32            countAreaCompares;
     
     gboolean          cancelAttemptFlag;

  } GapLocateContext;




/* ---------------------------------
 * p_debugPrintReferenceTable
 * ---------------------------------
 */
static void
p_debugPrintReferenceTable(GapLocateContext *lctx)
{
  gint32   rx;
  gint32   ry;
  gint32   tabSizeUsed;
  
  tabSizeUsed = 1 +(2 * lctx->refShapeRadius);
  printf("\nopacity of refColorTab (usedSize)\n+");
  for(rx = 0; rx < tabSizeUsed; rx++)
  {
    if(rx == lctx->refShapeRadius)
    {
      printf("+");
    }
    else
    {
      printf("-");
    }
  }
  printf("\n");
  
  
  for(ry = 0; ry < tabSizeUsed; ry++)
  {
    if(ry == lctx->refShapeRadius)
    {
      printf("+");
    }
    else
    {
      printf("|");
    }
    for(rx = 0; rx < tabSizeUsed; rx++)
    {
      GapLocateColor *currColorPtr;
      currColorPtr = &lctx->refColorTab[rx][ry];
    
      if(currColorPtr->rgba[3] < OPACITY_LEVEL_UCHAR)
      {
        printf(".");
      }
      else
      {
        if((ry == lctx->refShapeRadius) && (rx == lctx->refShapeRadius))
        {
          printf("o");
        }
        else
        {
          printf("x");
        }
      }
      
    }
    printf("\n");
  }
}  /* end p_debugPrintReferenceTable */

/* ---------------------------------
 * p_GapLocateColor_to_GimpHsv
 * ---------------------------------
 * check if the reference shape has more than one single color
 * and has sufficient number of non transparent pixels
 * (otherwise area compare loops have no chance to deliver
 *  acceptable locating results)
 */
static void
p_GapLocateColor_to_GimpHsv(GapLocateColor *colorPtr, GimpHSV *hsvPtr)
{
  GimpRGB rgb;
  
  gimp_rgba_set_uchar (&rgb, colorPtr->rgba[0], colorPtr->rgba[1], colorPtr->rgba[2], 255);
  gimp_rgb_to_hsv(&rgb, hsvPtr);

}


/* ---------------------------------
 * p_trimReferenceShape
 * ---------------------------------
 * check if the reference shape has more than one single color
 * and has sufficient number of non transparent pixels
 * (otherwise area compare loops have no chance to deliver
 *  acceptable locating results)
 */
static void
p_trimReferenceShape(GapLocateContext *lctx)
{
  GimpHSV refHsv;
  gint32 tabSizeUsed;
  gint32 rx;
  gint32 ry;
  gboolean isRefPixelColor;
  
  p_GapLocateColor_to_GimpHsv(lctx->refColorPtr, &refHsv);
  isRefPixelColor = ((refHsv.s >= 0.25) && (refHsv.v >= 0.1));
  
  tabSizeUsed = 1 +(2 * lctx->refShapeRadius);
  for(ry = 0; ry < tabSizeUsed; ry++)
  {
    for(rx = 0; rx < tabSizeUsed; rx++)
    {
      GimpHSV currentHsv;
      GapLocateColor *currColorPtr;
      gdouble vDif;
      
      currColorPtr = &lctx->refColorTab[rx][ry];
      p_GapLocateColor_to_GimpHsv(currColorPtr, &currentHsv);

      vDif = fabs(refHsv.v - currentHsv.v);
      
      if(isRefPixelColor)
      {
        gdouble hDif;

        hDif = fabs(refHsv.h - currentHsv.h);
        /* normalize hue difference.
         * hue values represents an angle
         * where value 0.5 equals 180 degree
         * and value 1.0 stands for 360 degree that is
         * equal to 0.0
         * Hue is maximal different at 180 degree.
         *
         * after normalizing, the difference
         * hDiff value 1.0 represents angle difference of 180 degree
         */
        if(hDif > 0.5)
        {
          hDif = (1.0 - hDif) * 2.0;
        }
        else
        {
          hDif = hDif * 2.0;
        }

        if((hDif > 0.5) || (vDif > 0.6))
        {
          currColorPtr->rgba[3] = 0;
        }
        
      }
      else
      {
        if(vDif > 0.5)
        {
          currColorPtr->rgba[3] = 0;
        }
      }


    }
  }
}  /* end p_trimReferenceShape */


/* ---------------------------------
 * p_initReferenceTable
 * ---------------------------------
 * init the reference table refColorTab that represents
 * the reference shape as array of colors with alpha channel.
 *
 * the used size of the reference table depends on the refShapeRadius,
 * but not on the reference coordinates lctx->refX/Y
 * if the reference coordinate is near the edges some pixels that refere
 * to coordinates outside the reference image boundaries will be initialized as transparent
 */
static void
p_initReferenceTable(GapLocateContext *lctx)
{
  gint32 tabSizeUsed;
  gint32 rx;
  gint32 ry;
  gint32 refOffX;
  gint32 refOffY;
  
  tabSizeUsed = 1 +(2 * lctx->refShapeRadius);
  refOffX = lctx->refX - lctx->refShapeRadius;
  refOffY = lctx->refY - lctx->refShapeRadius;
 
  for(ry = 0; ry < tabSizeUsed; ry++)
  {
    for(rx = 0; rx < tabSizeUsed; rx++)
    {
      GapLocateColor *currColorPtr;
      gint32          xx;
      gint32          yy;
      
      currColorPtr = &lctx->refColorTab[rx][ry];
      xx = refOffX + rx;
      yy = refOffY + ry;

      if ((xx < 0) || (xx >= lctx->refDrawable->width)
      ||  (yy < 0) || (yy >= lctx->refDrawable->height))
      {
        /* use full transparent black pixel for coordinates outside boundaries */
        currColorPtr->rgba[0] = 0;
        currColorPtr->rgba[1] = 0;
        currColorPtr->rgba[2] = 0;
        currColorPtr->rgba[3] = 0;
      }
      else
      {
        gimp_pixel_fetcher_get_pixel (lctx->pftRef
                        , refOffX + rx
                        , refOffY + ry
                        , &currColorPtr->rgba[0]
                        );
      }
      
    }
  }

  p_trimReferenceShape(lctx);
  
}  /* end p_initReferenceTable */
   




/* ---------------------------------
 * p_compareAreaRegion
 * ---------------------------------
 * calculate summaryColordiff for all opaque pixels
 * in the compared area region.
 */
static void
p_compareAreaRegion (const GimpPixelRgn *targetPR
                    ,GapLocateContext *lctx)
{
  guint    row;
  guchar* target = targetPR->data;   /* the target drawable */

  if(lctx->cancelAttemptFlag == TRUE)
  {
    return;
  }
  for (row = 0; row < targetPR->h; row++)
  {
    guint  col;
    guint  idxtarget;

    idxtarget = 0;
    for(col = 0; col < targetPR->w; col++)
    {
      GapLocateColor *referenceColorPtr;
      gint32   rx; 
      gint32   ry; 
      gint32   tx; 
      gint32   ty; 
      
      if(targetPR->bpp > 3)
      {
        if(target[idxtarget +3] < OPACITY_LEVEL_UCHAR)
        {
          /* transparent target pixel is not compared */
          idxtarget += targetPR->bpp;
          continue;
        }
      }
      
      tx = targetPR->x + col;
      ty = targetPR->y + row;

      rx = CLAMP(tx - lctx->transformOffX, 0, MAX_SHAPE_TABLE_SIZE -1);
      ry = CLAMP(ty - lctx->transformOffY, 0, MAX_SHAPE_TABLE_SIZE -1);

      referenceColorPtr = &lctx->refColorTab[rx][ry];
      
      if(referenceColorPtr->rgba[3] >= OPACITY_LEVEL_UCHAR)
      {
        gdouble colordiff;
        
        colordiff = gap_colordiff_simple_guchar(&referenceColorPtr->rgba[0]
                  , &target[idxtarget +0]
                  , FALSE                    /*gboolean debugPrint*/
                  );
        lctx->commonAreaComparedPixelCount += 1;
        lctx->commonAreaSumColordiff += colordiff;
        
        

        if(gap_debug)
        {
          printf("try:(%d)  pixel at ref: %d/%d target:%d/%d   colordiff:%.5f\n"
              , (int)lctx->countTries
              , (int)rx
              , (int)ry
              , (int)tx
              , (int)ty
              , (float)colordiff
              );
        }
        
        if(((lctx->commonAreaComparedPixelCount >> 3) & 7) == 7)
        {
          gdouble tmpAvgColordiff;
          
          tmpAvgColordiff = lctx->commonAreaSumColordiff / (gdouble)lctx->commonAreaComparedPixelCount;
          if(tmpAvgColordiff > lctx->tmpColordiffBreakThreshold)
          {
            lctx->cancelAttemptFlag = TRUE;
            if(gap_debug)
            {
              printf("try:(%d)  cancelAttemptFlag set TRUE\n"
                , (int)lctx->countTries
                );
            }
            return;
          }
        }


      }

      idxtarget += targetPR->bpp;
    }

    target += targetPR->rowstride;

  }

}  /* end p_compareAreaRegion */




/* ----------------------------------------
 * p_calculateCommonAreaClipping
 * ----------------------------------------
 * check clipping and calculate commonAreaWidth and Height
 * and offsets in target drawable 
 *
 *  <--- leftRadius --->o<------- rightRadius ------->
 *
 */
static void 
p_calculateCommonAreaClipping(GapLocateContext *lctx, gint32 offX, gint32 offY
    , gint32  targetX, gint32  targetY
    )
{
  gint32  commonWidth;
  gint32  commonHeight;
  gint32  leftRefRadius;
  gint32  topRefRadius;
  gint32  rightRefRadius;
  gint32  botRefRadius;
  gint32  leftTargetX;
  gint32  topTargetY;
  gint32  rightTargetX;
  gint32  botTargetY;
  gint32  leftTargetRadius;
  gint32  topTargetRadius;
  gint32  rightTargetRadius;
  gint32  botTargetRadius;

  commonWidth = MIN(lctx->refDrawable->width, lctx->targetDrawable->width);
  commonHeight = MIN(lctx->refDrawable->height, lctx->targetDrawable->height);

  leftRefRadius = lctx->refShapeRadius;
  topRefRadius = lctx->refShapeRadius;

  rightRefRadius = lctx->refShapeRadius;
  botRefRadius = lctx->refShapeRadius;


 
  leftTargetX = CLAMP((targetX - leftRefRadius), 0, commonWidth -1);
  topTargetY = CLAMP((targetY - topRefRadius), 0, commonHeight -1);

  rightTargetX = CLAMP((targetX + rightRefRadius), 0, commonWidth -1);
  botTargetY = CLAMP((targetY + botRefRadius), 0, commonHeight -1);

  leftTargetRadius = targetX - leftTargetX;
  topTargetRadius = targetY - topTargetY;

  rightTargetRadius = rightTargetX - targetX;
  botTargetRadius = botTargetY - targetY;

  
  lctx->targetOffX = leftTargetX;
  lctx->targetOffY = topTargetY;
  lctx->commonAreaWidth = 1 + leftTargetRadius + rightTargetRadius;
  lctx->commonAreaHeight = 1 + topTargetRadius + botTargetRadius;

  lctx->transformOffX = targetX - lctx->refShapeRadius;
  lctx->transformOffY = targetY - lctx->refShapeRadius;

  if(gap_debug)
  {
    printf("refX/Y: %d / %d  offX/Y: %d / %d  targetX/Y %d / %d  leftTargetX:%d topTargetY:%d rightTargetX:%d botTargetY:%d"
           " leftTargetRadius:%d topTargetRadius:%d rightTargetRadius:%d botTargetRadius %d\n"
      , (int)  lctx->refX
      , (int)  lctx->refY
      , (int)  offX
      , (int)  offY
      , (int)  targetX
      , (int)  targetY
      , (int)  leftTargetX
      , (int)  topTargetY
      , (int)  rightTargetX
      , (int)  botTargetY
      , (int)  leftTargetRadius
      , (int)  topTargetRadius
      , (int)  rightTargetRadius
      , (int)  botTargetRadius
      );
  
    printf("transformOffX:%d transformOffY:%d targetOffX/Y: %d / %d commonArea W/H: %d x %d\n"
      , (int)lctx->transformOffX
      , (int)lctx->transformOffY
      , (int)lctx->targetOffX
      , (int)lctx->targetOffY
      , (int)lctx->commonAreaWidth
      , (int)lctx->commonAreaHeight
      );
  }

}  /* end p_calculateCommonAreaClipping */


/* ----------------------------------------
 * p_compareArea
 * ----------------------------------------
 *
 *  +---+---+
 *  |   |   |
 *  | ..|...|
 *  | ..|...|
 *  +---o---+  ----------
 *  | ..|...|    ^
 *  | ..|...|    |  refShapeRadius
 *  | ..|...|    v
 *  +---+---+  -----------
 *
 *
 */
static void p_compareArea(GapLocateContext *lctx, gint32 offX, gint32 offY)
{
#define COMMON_AREA_MIN_EDGE_LENGTH 4
#define COMMON_AREA_MIN_PIXELS_COMPARED 12

  GimpPixelRgn targetPR;
  gpointer  pr;
  gint32 targetX;
  gint32 targetY;
  guchar          pixel[4];
  gdouble         colordiff;

  lctx->countTries++;

  targetX = lctx->refX + offX;
  if(targetX < 0)
  {
    return;
  }
  if(targetX >  lctx->targetDrawable->width -1)
  {
    return;
  }
  targetY = lctx->refY + offY;
  if(targetY < 0)
  {
    return;
  }
  if(targetY > lctx->targetDrawable->height -1)
  {
    return;
  }
  gimp_pixel_fetcher_get_pixel (lctx->pftTarget, targetX, targetY, &pixel[0]);
  colordiff = gap_colordiff_simple_guchar(&lctx->refColorPtr->rgba[0]
                  , &pixel[0]
                  , FALSE                    /*gboolean debugPrint*/
                  );
  if(colordiff > lctx->refPixelColordiffThreshold)
  {
    /* the coresponding pixel in the target drawable at 
     * current offsets differs more than tolerated threshold.
     * in this case skip full area comare for performance reasons
     */
    if(gap_debug)
    {
      printf("p_compareArea ref pixel too much Different, SKIP AREA compare at offsets offX:%d offY:%d refPixelColordiffThreshold:%.5f colordiff:%.5f\n"
         ,(int)offX
         ,(int)offY
         ,(float)lctx->refPixelColordiffThreshold
         ,(float)colordiff
         );
    }
    return;
  }
  
  
  p_calculateCommonAreaClipping(lctx, offX, offY, targetX, targetY);
  
  if((lctx->commonAreaWidth < COMMON_AREA_MIN_EDGE_LENGTH)
  || (lctx->commonAreaHeight < COMMON_AREA_MIN_EDGE_LENGTH))
  {
    /* common area is too small to locate image details
     * (comparing very few or single pixels does not make sense
     *  it would indicate matching area at unexpected coordinates)
     * 
     */
    if(gap_debug)
    {
      printf("p_compareArea is too small at offsets offX:%d offY:%d\n"
         ,(int)offX
         ,(int)offY
         );
    }
    return;
  }


  lctx->commonAreaSumColordiff = 0.0;
  lctx->commonAreaComparedPixelCount = 0;
  lctx->cancelAttemptFlag = FALSE;

  gimp_pixel_rgn_init (&targetPR, lctx->targetDrawable, lctx->targetOffX, lctx->targetOffY
                      , lctx->commonAreaWidth, lctx->commonAreaHeight
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );

  /* compare pixel areas in tiled portions via pixel region processing loops.
   */
  for (pr = gimp_pixel_rgns_register (1, &targetPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
    p_compareAreaRegion (&targetPR, lctx);
  }

  lctx->countAreaCompares++;

  if ((lctx->commonAreaComparedPixelCount >= COMMON_AREA_MIN_PIXELS_COMPARED)
  &&  (lctx->cancelAttemptFlag != TRUE))
  {
    gdouble commonAreaAvgColordiff;
    
    commonAreaAvgColordiff = lctx->commonAreaSumColordiff / (gdouble)lctx->commonAreaComparedPixelCount;

    if(gap_debug)
    {
      printf("p_compareArea try:(%d) done at offsets offX:%d offY:%d  sum:%.5f, count:%.0f commonAreaAvgColordiff:%.5f\n"
         ,(int)lctx->countTries
         ,(int)offX
         ,(int)offY
         ,(float)lctx->commonAreaSumColordiff
         ,(float)lctx->commonAreaComparedPixelCount
         ,(float)commonAreaAvgColordiff
         );
    }

    
    if ((commonAreaAvgColordiff < lctx->minColordiff)
    ||  ((commonAreaAvgColordiff == lctx->minColordiff) && (lctx->commonAreaComparedPixelCount > lctx->pickedPixelCount)))
    {
      /* found best matching common area (so far) at current offests
       * set minColordiff and result targertX/Y coordinates.
       * (processing may continue to find better matching result
       *  at other offsets later on..)
       */
      lctx->minColordiff = commonAreaAvgColordiff;
      lctx->tmpColordiffBreakThreshold = lctx->minColordiff * COLORDIFF_BREAK_THRESHOLD_FACTOR;
      lctx->targetX = targetX;
      lctx->targetY = targetY;
      lctx->pickedTryNumber = lctx->countTries;
      lctx->pickedPixelCount = lctx->commonAreaComparedPixelCount;
      
    }

  }

}  /* end p_compareArea */



/* ----------------------------------------
 * p_locateDetailLoop
 * ----------------------------------------
 * calls area comarison by varying offsets within targetMoveRadius
 * starting at offsets 0/0 to find the offsets where reference matches best with target.
 * break the loop at first perfect match (where minColordiff <= breakAtColordiff level)
 */
static void p_locateDetailLoop(GapLocateContext *lctx)
{
  gint32 offX;
  gint32 offY;
  
  for(offX = 0; offX < lctx->targetMoveRadius; offX++)
  {
    for(offY = 0; offY < lctx->targetMoveRadius; offY++)
    {
      p_compareArea(lctx, offX, offY);
      if(lctx->minColordiff <= lctx->breakAtColordiff)
      {
        return;
      }

      if(offX != 0)
      {
        p_compareArea(lctx, 0 - offX, offY);
        if(lctx->minColordiff <= lctx->breakAtColordiff)
        {
          return;
        }
        if(offY != 0)
        {
          p_compareArea(lctx, 0 - offX, 0 - offY);
          if(lctx->minColordiff <= lctx->breakAtColordiff)
          {
            return;
          }
        }
      }

      if(offY != 0)
      {
        p_compareArea(lctx, offX, 0 - offY);
        if(lctx->minColordiff <= lctx->breakAtColordiff)
        {
          return;
        }
      }

    }
  }
}  /* end p_locateDetailLoop */



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
 * the return value is the minimum colordifference value
 * (in range 0.0 to 1.0 where 0.0 indicates that the compared area is exactly equal)
 *
 * NOTE: this procedure is the old implementation and shall not be used in new code.
 *       it calls the more efficient alternative implementation
 *       unless the user explicte sets the gimprc parameter
 *         gap-locate-details-use-old-algorithm yes
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
  )
{
  GapLocateContext  locateContext;
  GapLocateContext *lctx;
 
  gboolean useGapLocateOldAlgo;
  
  
  useGapLocateOldAlgo =
    gap_base_get_gimprc_gboolean_value("gap-locate-details-use-old-algorithm", FALSE);

  if(useGapLocateOldAlgo == FALSE)
  {
    gdouble avgColordiff;
    
    avgColordiff =
      gap_locateAreaWithinRadius(refDrawableId
                                ,refX
                                ,refY
                                ,refShapeRadius
                                ,targetDrawableId
                                ,targetMoveRadius
                                ,targetX
                                ,targetY
                                );
    
    return(avgColordiff);
  }


  /* init context */  
  lctx = &locateContext;
  lctx->refDrawable = NULL;
  lctx->targetDrawable = NULL;
  lctx->refX = refX;
  lctx->refY = refY;
  lctx->refShapeRadius = CLAMP(refShapeRadius, 1, GAP_LOCATE_MAX_REF_SHAPE_RADIUS);
  lctx->targetMoveRadius = targetMoveRadius;
  lctx->targetX = 0;
  lctx->targetY = 0;
  lctx->minColordiff = 1.0;
  lctx->tmpColordiffBreakThreshold = CLAMP(requiredColordiffThreshold, 0.0, 1.0) * COLORDIFF_BREAK_THRESHOLD_FACTOR;
  lctx->breakAtColordiff = 0.0;
  lctx->refPixelColordiffThreshold = 0.11;
  lctx->edgeColordiffThreshold = 0.14;
  lctx->countTries = 0;
  lctx->countAreaCompares = 0;
  lctx->pickedPixelCount = 0;

  lctx->refDrawable = gimp_drawable_get(refDrawableId);
  lctx->targetDrawable = gimp_drawable_get(targetDrawableId);

  if((lctx->refDrawable != NULL) 
  && (lctx->targetDrawable != NULL))
  {
    /* create and init pixelfetchers (deliver black transparent pixel outside boundaries) */
    lctx->pftRef = gimp_pixel_fetcher_new (lctx->refDrawable, FALSE /* shadow */);
    lctx->pftTarget = gimp_pixel_fetcher_new (lctx->targetDrawable, FALSE /* shadow */);
    gimp_pixel_fetcher_set_edge_mode (lctx->pftRef, GIMP_PIXEL_FETCHER_EDGE_NONE);
    gimp_pixel_fetcher_set_edge_mode (lctx->pftTarget, GIMP_PIXEL_FETCHER_EDGE_BLACK);

    /* init reference pixel color (in center of used area in refColorTab) */
    lctx->refColorPtr = &lctx->refColorTab[lctx->refShapeRadius][lctx->refShapeRadius];
    gimp_pixel_fetcher_get_pixel (lctx->pftRef, refX, refY, &lctx->refColorPtr->rgba[0]);
    
    p_initReferenceTable(lctx);
    p_locateDetailLoop(lctx);

    gimp_pixel_fetcher_destroy (lctx->pftRef);
    gimp_pixel_fetcher_destroy (lctx->pftTarget);
  }

  
  if(lctx->refDrawable != NULL)
  {
    gimp_drawable_detach(lctx->refDrawable);
  }
  
  if(lctx->targetDrawable != NULL)
  {
    gimp_drawable_detach(lctx->targetDrawable);
  }
  
  
  *targetX = lctx->targetX;
  *targetY = lctx->targetY;
  
  if(gap_debug)
  {
    p_debugPrintReferenceTable(lctx);
    printf("gap_locateDetailWithinRadius: tries:%d areaCompares:%d pickedTryNumber:%d pickedPixelCount:%d resulting colordiff:%.5f\n"
       ,(int)lctx->countTries
       ,(int)lctx->countAreaCompares
       ,(int)lctx->pickedTryNumber
       ,(int)lctx->pickedPixelCount
       ,(float)lctx->minColordiff
       );
  }
  
  return (lctx->minColordiff);
  
}  /* end gap_locateDetailWithinRadius */
