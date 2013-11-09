/* gap_colormask_exec.c
 *    by hof (Wolfgang Hofer)
 *    color mask filter worker procedures
 *    to set alpha channel for a layer according to matching colors
 *    of color mask (image)
 *  2010/03/21
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
/* GAP includes */
#include "gap_lib_common_defs.h"
#include "gap_pdb_calls.h"
#include "gap_colordiff.h"
#include "gap_colormask_file.h"
#include "gap_colormask_exec.h"
#include "gap_lib.h"
#include "gap_image.h"
#include "gap_layer_copy.h"
#include "gap_libgapbase.h"


#define WORK_PROTECTED             255     /* marker for protected pixels */
#define WORK_PART_OF_CURRENT_AREA  250     /* marker for pixels that are part of current processed area */
#define WORK_PART_OF_BIG_AREA      245     /* marker for pixels that are part of another already processed area */
#define WORK_PART_OF_SMALL_AREA    240     /* marker for pixels that are part of another already processed area */
#define WORK_VISITED_BUT_NOT_PART_OF_CURRENT_AREA  1
#define WORK_UNCHECKED             0       /* marker for unchecked pixels */

#define IDX_ALPHA  3                       /* in RGBA apha channel has index 3 */

#define COLORMASK_LAYER_NAME "colormask"
#define COLORMASK_DIFF_LAYER_NAME "diffLayer"

#define CLIP_AREA_MAX_RADIUS 3
#define CLIP_AREA_MAX_SIZE   (1 + (2 * CLIP_AREA_MAX_RADIUS))

#define MI_IMAGE 0
#define MI_MASK  1
#define MI_MAX   2

#define ISLE_AREA_MAX_RADIUS 10
#define ISLE_AREA_MAX_SIZE   (1 + (2 * ISLE_AREA_MAX_RADIUS))


#define MATCHTYPE_UNDEFINED     0
#define MATCHTYPE_MATCHING      1
#define MATCHTYPE_NOT_MATCHING  2



extern      int gap_debug; /* ==0  ... dont print debug infos */


typedef struct GapAreaCoordPoint {
     gint            x;
     gint            y;
     void           *next;
} GapAreaCoordPoint;


typedef struct GapColorMaskParams { /* nickname: cmaskParPtr */
     gint32     dst_layer_id;
     gint32     cmask_drawable_id;
     gdouble    colorThreshold;      /* dynamic keycolor dependent color difference lower threshold 0.0 to 1.0 */
     gdouble    loColorThreshold;    /* color difference lower threshold 0.0 to 1.0 */
     gdouble    hiColorThreshold;    /* color difference upper threshold 0.0 to 1.0 */
     gdouble    colorSensitivity;    /* 1.0 to 2.0 sensitivity for colr diff algorithm */
     gdouble    lowerOpacity;        /* opacity for matching colors 0.0 to 1.0 */
     gdouble    upperOpacity;        /* opacity for non matching colors 0.0 to 1.0 */
     gdouble    triggerAlpha;        /* 0.0 to 1.0 mask opacity greater than this alpha value trigger color masking */
     gdouble    isleRadius;          /* radius in pixels for checking isolated pixels */
     gdouble    isleAreaPixelLimit;  /* size of area in pixels for remove isaolated pixel(area)s */
     gdouble    featherRadius;       /* radius in pixels for feathering edges */
     gdouble    thresholdColorArea;
     gboolean   connectByCorner;
     gdouble    pixelDiagonal;
     gdouble    pixelAreaLimit;
     gboolean   keepLayerMask;
     gboolean   keepWorklayer;
     gdouble    edgeColorThreshold;
     gint       algorithm;

     /* stuff for dynamic keycolor dependent threshold handling */
     GimpHSV    keyColorHsv;
     gdouble    loKeyColorThreshold;      /* color difference lower threshold for keycolor 0.0 to 1.0 */
     gdouble    keyColorSensitivity;      /* 0.1 upto 10.0 default 1.0 */
     gboolean   enableKeyColorThreshold;  

     /* internal and precalculated values */
     guchar adjustOpacityLevel;
     gint   adjustLoopCount;

     gdouble    shadeRange;
     guchar     triggerAlpha255;
     guchar     lowerAlpha255;
     guchar     upperAlpha255;
     guchar     transparencyLevel255;
     gdouble    opacityFactor;        /* 1.0 in case opacity lower/upper use the full range */
     gint32     dst_image_id;
     gboolean   debugCoordinate;
     gint       guideRow;
     gint       guideCol;
     gint       x;                  /* current processed coordinate */
     gint       y;

     gint       width;              /* current processed drawable width */
     gint       height;

     GimpPixelFetcher *pftMask;
     GimpPixelFetcher *pftDest;
     GimpPixelFetcher *pftDestLayerMask;
     gint32            dstLayerMaskId;

     gint jaggedRadius;
     gint jeggedLength;

     /* area context stuff */

     GapAreaCoordPoint   *pointList;
     gint32    pointCount;
     gint32    pixelCount;
     gint32    max_ix;
     gint32    max_iy;
     gint32    min_ix;
     gint32    min_iy;
     gint32    seed_ix;
     gint32    seed_iy;
     guchar    marker;
     gint      currentAreaPass;

     gint      sel_x1;
     gint      sel_y1;
     gint      sel_x2;
     gint      sel_y2;

     gint32            worklayerId;
     guchar            seedPixel[4];
     GimpHSV           seedHsv;
     GimpPixelFetcher *pftWork;
     gdouble           sumColorDiff;
     gdouble           avgColorDiff;
     guchar            areaPrecalculatedOpacity;
     gboolean          usePrecalculatedOpacity;


     /* stuff for average algorithm handling */
     gint    checkRadius;
     gint    checkMatchRadius;
     gdouble *colordiffTable;          // big colordiff lookup table in full image size
     gdouble avgFactor;                // == 0.5  // 0.0 to 1.0
     gdouble avgColorThreshold;        // ==  colorThreshold + ((hiColorThreshold - colorThreshold) *  avgFactor)


     gint32 areaMinimumPixels;   // GUI Parameter 5 to     (0.9 * CLIP_AREA_MAX_SIZE * CLIP_AREA_MAX_SIZE)

     gint32 maskAreaSize;  //     ... area of similar color to current image pixel in the colormask
     gint32 imgAreaSize;   //     ... area of similar color to current
     gint32 maskOnlyAreaSize;  //     ... area pixels that are part of the mask area but NOT part of the image area
     gint32 imgOnlyAreaSize;   //     ... area pixels that are part of the image area but NOT part of the mask area
     gint32 commonAreaSize;    //     ... common pixels that are member in both image and mask area

     gdouble sumMatch[MI_MAX];
     gdouble sumRange[MI_MAX];
     gdouble sumNoMatch[MI_MAX];
     gdouble countMatch[MI_MAX];
     gdouble countRange[MI_MAX];
     gdouble countNoMatch[MI_MAX];

     gdouble avgColordiffMaskArea;
     gdouble avgColordiffImgArea;
     gdouble avgColordiffMaskOnlyArea;
     gdouble avgColordiffImgOnlyArea;
     gdouble avgColordiffCommonArea;
     gdouble sumColordiffMaskOnlyArea;
     gdouble sumColordiffImgOnlyArea;
     gdouble sumColordiffCommonArea;
     
     /* stuff for GAP_COLORMASK_ALGO_AVG_CHANGE */
     gint    significantRadius;              /* radius to limit search for significant brightness/colorchanges */
     gdouble significantColordiff;           /* threshold to define when colors are considered as significant different */
     gdouble significantBrightnessDiff;      /* threshold to define significant brightness changes */


     gint32 seedOffsetX;   /* for convert image coords to clipAreaTable coords */
     gint32 seedOffsetY;
     guchar clipAreaTable[CLIP_AREA_MAX_SIZE][CLIP_AREA_MAX_SIZE][MI_MAX];
     guchar isleAreaTable[ISLE_AREA_MAX_SIZE][ISLE_AREA_MAX_SIZE];



     /* stuff for progress handling */
     gboolean   doProgress;
     gdouble    pixelsToProcessInAllPasses;
     gdouble    pixelsDoneAllPasses;

  } GapColorMaskParams;

static  inline void  p_set_dynamic_threshold_by_KeyColor(guchar *pixel
                        , GapColorMaskParams *cmaskParPtr);


static void          p_colormask_avg_rgn_render_region (const GimpPixelRgn *maskPR
                        ,const GimpPixelRgn *destPR
                        ,const GimpPixelRgn *lmskPR
                        ,GapColorMaskParams *cmaskParPtr);

static gboolean      p_check_matching_pixel_isolation(GapColorMaskParams *cmaskParPtr);
static guchar        p_calc_opacity_for_matching_color(GapColorMaskParams *cmaskParPtr);
static guchar        p_calc_opacity_for_range_color(GapColorMaskParams *cmaskParPtr);
static void          p_check_range_neighbour_sum(GapColorMaskParams *cmaskParPtr, gint dx, gint dy
                         , gint radius, gdouble *sumNbColorDiff, gdouble *countNbPixels);


static void          p_countPerColordiff(GapColorMaskParams *cmaskParPtr, gdouble colordiff, gint mi);
static void          p_avg_check_and_mark_nb_pixel(GapColorMaskParams *cmaskParPtr, GimpPixelFetcher *pft, gint nx, gint ny, gint mi);
static void          p_find_pixel_area_of_similar_color(GapColorMaskParams *cmaskParPtr, GimpPixelFetcher *pft, gint mi);
static void          p_calculate_clip_area_average_values(GapColorMaskParams *cmaskParPtr);


static gboolean      p_check_average_colordiff_incl_neighbours(GapColorMaskParams *cmaskParPtr);

static gboolean      p_check_significant_diff(guchar *aPixelPtr
                       , guchar *bPixelPtr
		       , gint xx, gint yy
                       , GapColorMaskParams *cmaskParPtr);
static guchar        p_check_significant_changes_in_one_direction(GapColorMaskParams *cmaskParPtr, gint dx, gint dy);

static inline void   p_check_avg_add_one_pixel(GapColorMaskParams *cmaskParPtr, gint xx, gint yy    // DEPRECATED
                           , gdouble *sumNbColorDiff, gdouble *countNbPixels);
static gdouble       p_check_avg_diff_within_radius(GapColorMaskParams *cmaskParPtr, gint radius    // DEPRECATED
                           , gdouble *sumNbColorDiff, gdouble *countNbPixels);


static void          p_init_colordiffTable (const GimpPixelRgn *maskPR
                        ,const GimpPixelRgn *destPR
                        ,GapColorMaskParams *cmaskParPtr);
static inline gint32 p_getColordiffTableIndex(GapColorMaskParams *cmaskParPtr, gint x, gint y);


static void          p_difflayer_rgn_render (const GimpPixelRgn *diffPR
                         , const GimpPixelRgn *destPR, GapColorMaskParams *cmaskParPtr);


static void          p_create_colordiffTable_Layer(GapColorMaskParams *cmaskParPtr, GimpDrawable *dst_drawable);







/* ---------------------------------
 * p_handle_progress
 * ---------------------------------
 * handle progress based on the specified amount of processed pixels
 */
static void
p_handle_progress(GapColorMaskParams *cmaskParPtr, gint pixels, const char *caller)
{
  gdouble progress;

  cmaskParPtr->pixelsDoneAllPasses += pixels;
  progress = cmaskParPtr->pixelsDoneAllPasses / cmaskParPtr->pixelsToProcessInAllPasses;
  gimp_progress_update(progress);

  return;

  if(gap_debug)
  {
    printf("Progress: %s (%.1f / %.1f) pixels:%d %.3f\n"
      , caller
      , (float)cmaskParPtr->pixelsDoneAllPasses
      , (float)cmaskParPtr->pixelsToProcessInAllPasses
      , (int)pixels
      , (float)progress
      );
  }
}  /* end p_handle_progress */

/* --------------------------------------------
 * p_create_empty_layer
 * --------------------------------------------
 * create an empty layer (the worklayer) and add it to the specified image
 *
 */
static gint32
p_create_empty_layer(gint32 image_id
                       , gint32 width
                       , gint32 height
                       , const char *name
                        )
{
  gint32 layer_id;
  GimpImageBaseType l_basetype;

  l_basetype   = gimp_image_base_type(image_id);
  if(l_basetype != GIMP_RGB)
  {
    printf("** ERROR image != RGB is not supported\n");
    return -1;
  }

  layer_id = gimp_layer_new(image_id
                , name
                , width
                , height
                , GIMP_RGBA_IMAGE
                , 100.0   /* full opacity */
                , 0       /* normal mode */
                );

  gimp_image_insert_layer (image_id, layer_id, 0, 0 /* stackposition */ );

  return (layer_id);

}  /* end p_create_empty_layer */





/* ---------------------------------
 * p_colordiff_guchar_cmask
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 */
gdouble
p_colordiff_guchar_cmask(guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , GapColorMaskParams *cmaskParPtr)
{
  gdouble colordiffRgb;
  gdouble colordiffHsv;
  
  colordiffRgb = 0.0;
  if (cmaskParPtr->connectByCorner == TRUE) ///// #### TODO
  {
    colordiffRgb = gap_colordiff_simple_guchar(aPixelPtr
                   , bPixelPtr
                   , cmaskParPtr->debugCoordinate
                   );
  }

  colordiffHsv = gap_colordiff_guchar(aPixelPtr
                   , bPixelPtr
                   , cmaskParPtr->colorSensitivity
                   , cmaskParPtr->debugCoordinate
                   );

  return (MAX(colordiffRgb, colordiffHsv));
  
}  /* end p_colordiff_guchar_cmask */


/* ---------------------------------
 * p_colordiff_guchar_cmask_RGBorHSV
 * ---------------------------------
 * returns difference of 2 colors as gdouble value
 * in range 0.0 (exact match) to 1.0 (maximal difference)
 */
gdouble
p_colordiff_guchar_cmask_RGBorHSV(GimpHSV *aHsvPtr, guchar *aPixelPtr
                   , guchar *bPixelPtr
                   , GapColorMaskParams *cmaskParPtr)
{
  gdouble colordiffRgb;
  gdouble colordiffHsv;
  
  colordiffRgb = 0.0;
  if (cmaskParPtr->connectByCorner == TRUE) ///// #### TODO
  {
    colordiffRgb = gap_colordiff_simple_guchar(aPixelPtr
                   , bPixelPtr
                   , cmaskParPtr->debugCoordinate
                   );
  }
  colordiffHsv = gap_colordiff_guchar_GimpHSV(aHsvPtr
                  , bPixelPtr
                   , cmaskParPtr->colorSensitivity
                   , cmaskParPtr->debugCoordinate
                   );

  return (MAX(colordiffRgb, colordiffHsv));

}  /* end p_colordiff_guchar_cmask_RGBorHSV */



/* ----------------------------------
 * p_get_guides
 * ----------------------------------
 * get 1st horizontal and vertical guide.
 *
 * note that guides are not relevant for the productive processing
 * but the 1st guide crossing is used to specify
 * a coordinate where debug output shall be printed.
 */
static void
p_get_guides(GapColorMaskParams *cmaskParPtr)
{
  gint32  guide_id;

  guide_id = 0;

  cmaskParPtr->guideRow = -1;
  cmaskParPtr->guideCol = -1;

  if(cmaskParPtr->dst_image_id < 0)
  {
     return;
  }


  while(TRUE)
  {
    guide_id = gimp_image_find_next_guide(cmaskParPtr->dst_image_id, guide_id);

    if (guide_id < 1)
    {
       break;
    }
    else
    {
       gint32 orientation;

       orientation = gimp_image_get_guide_orientation(cmaskParPtr->dst_image_id, guide_id);
       if(orientation != 0)
       {
         if(cmaskParPtr->guideCol < 0)
         {
           cmaskParPtr->guideCol = gimp_image_get_guide_position(cmaskParPtr->dst_image_id, guide_id);
         }
       }
       else
       {
         if(cmaskParPtr->guideRow < 0)
         {
           cmaskParPtr->guideRow = gimp_image_get_guide_position(cmaskParPtr->dst_image_id, guide_id);
         }
       }
    }

  }

  if(gap_debug)
  {
    printf("dst_image_id:%d  guideCol:%d :%d\n"
       ,(int)cmaskParPtr->dst_image_id
       ,(int)cmaskParPtr->guideCol
       ,(int)cmaskParPtr->guideRow
       );
  }

}  /* end p_get_guides */

/* ----------------------------------
 * p_is_debug_active_on_coordinate
 * ----------------------------------
 * check if current processed coords (x/y) are on first guide crossing
 * (for debug purpuse to force logging at koordinates selected via guides)
 */
static gboolean
p_is_debug_active_on_coordinate(GapColorMaskParams *cmaskParPtr)
{
  if(cmaskParPtr->x == cmaskParPtr->guideCol)
  {
    if(cmaskParPtr->y == cmaskParPtr->guideRow)
    {
      return(TRUE);
    }
    if (cmaskParPtr->guideRow == -1)
    {
      /* there is no vertical guide available, in this case match all rows */
      return(TRUE);
    }
  }
  else
  {
    if((cmaskParPtr->y == cmaskParPtr->guideRow) && (cmaskParPtr->guideCol == -1))
    {
      /* there is no horizontal guide available, in this case match all coloumns */
      return(TRUE);
    }
  }
  return (FALSE);

}  /* end p_is_debug_active_on_coordinate */




/* --------------------------------------
 * p_calculate_alpha_from_colormask_pixel  (SIMPLE VARIANT)
 * --------------------------------------
 * calculates transparency according to
 * color difference of individual pixel in the processed
 * layer and its corresponding pixel in the colormask.
 *
 * returns the alpha channel as byte value.
 */
static inline guchar
p_calculate_alpha_from_colormask_pixel(gint maskBpp
     , guchar *maskPtr
     , guchar *origPtr
     , GapColorMaskParams *cmaskParPtr
     )
{
  gdouble colorDiff;

  if (maskBpp > 3)
  {
    /* colormask has an alpha channel */
    if (maskPtr[3] <= cmaskParPtr->triggerAlpha255)
    {
      /* transparent mask (below or equal trigger value)
       * switches off color masking for that pixel
       * return 255 (full opaque) as opacity value
       * for the layermask.
       * Note that an opaque layermask pixel
       * preserves opacity as set via alpha channel unchanged when the layermask is applied.
       * (no need to check color differences in that case)
       */
      return (255);
    }
  }

  colorDiff = p_colordiff_guchar_cmask(maskPtr
                                 , origPtr
                                 , cmaskParPtr
                                 );

  p_set_dynamic_threshold_by_KeyColor(origPtr, cmaskParPtr);
  
  if(colorDiff <= cmaskParPtr->colorThreshold)
  {
    /* color matches good */
    return (cmaskParPtr->lowerAlpha255);
  }

  if ((colorDiff < cmaskParPtr->hiColorThreshold)
  &&  (cmaskParPtr->shadeRange > 0.0))
  {
    /* color matches extended range (calculate corresponding alpha shade value) */
    gdouble opacity255;  /* range 0.0 to 255.0 */
    guchar  alpha;
    gdouble shadeFactor;

    shadeFactor = (colorDiff - cmaskParPtr->colorThreshold) / cmaskParPtr->shadeRange;


     opacity255 = GAP_BASE_MIX_VALUE(shadeFactor
                                   , (gdouble)cmaskParPtr->lowerAlpha255
                                   , (gdouble)cmaskParPtr->upperAlpha255
                                   );
    alpha = CLAMP((guchar)opacity255, 0, 255);

    if (cmaskParPtr->debugCoordinate)
    {
      printf("p_calculate_alpha x:%03d y:%03d shadeFactor:%.4f shadeRange:%.4f colorDiff:%.6f  ALPHA:%d \n"
             ,(int)cmaskParPtr->x
             ,(int)cmaskParPtr->y
             ,(float)shadeFactor
             ,(float)cmaskParPtr->shadeRange
             ,(float)colorDiff
             ,(int)alpha
             );
    }

    return (alpha);
  }

  /* color does not match */
  return (cmaskParPtr->upperAlpha255);

}  /* end p_calculate_alpha_from_colormask_pixel */




/* ---------------------------------
 * p_colormask_rgn_render_region
 * ---------------------------------
 * render transparency according to colormask
 * to the layermask of the destination drawable.
 */
static void
p_colormask_rgn_render_region (const GimpPixelRgn *maskPR
                    ,const GimpPixelRgn *destPR
                    ,const GimpPixelRgn *lmskPR
                    ,GapColorMaskParams *cmaskParPtr)
{
  guint    row;
  guchar* mask = maskPR->data;   /* the colormask */
  guchar* dest = destPR->data;   /* the destination drawable */
  guchar* lmsk = lmskPR->data;   /* the layermask of the destination drawable */


  for (row = 0; row < destPR->h; row++)
  {
    guint  col;
    guint  idxMask;
    guint  idxDest;
    guint  idxLmsk;

    idxMask = 0;
    idxDest = 0;
    idxLmsk = 0;
    for(col = 0; col < destPR->w; col++)
    {
      cmaskParPtr->x = destPR->x + col;
      cmaskParPtr->y = destPR->y + row;
      lmsk[idxLmsk] = p_calculate_alpha_from_colormask_pixel(maskPR->bpp
                          , &mask[idxMask]     /* maskPtr */
                          , &dest[idxDest]     /* origPtr */
                          , cmaskParPtr
                          );

      /* ------ start debug code block -----*/
      //if(gap_debug)
      {
        cmaskParPtr->debugCoordinate = p_is_debug_active_on_coordinate(cmaskParPtr);
        if (cmaskParPtr->debugCoordinate)
        {
           gdouble colorDiff;
           colorDiff = p_colordiff_guchar_cmask( &mask[idxMask]
                               , &dest[idxDest]
                               , cmaskParPtr
                               );

           printf("colormask x:%03d y:%03d  ALPHA:%d colorDiff:%.6f (colorThreshold:%.6f)"
             ,(int)cmaskParPtr->x
             ,(int)cmaskParPtr->y
             ,(int)lmsk[idxLmsk]
             ,(float)colorDiff
             ,(float)cmaskParPtr->colorThreshold
             );


           if (maskPR->bpp > 3)
           {
             printf(" TRIGGER_ALPHA:%d MASK_ALPHA:%d"
               ,(int)cmaskParPtr->triggerAlpha255
               ,(int)mask[idxMask +3]
               );
           }

           printf("\n");
        }
      }
      /* ------ end debug code block -----*/


      idxDest += destPR->bpp;
      idxMask += maskPR->bpp;
      idxLmsk += lmskPR->bpp;
    }

    mask += maskPR->rowstride;
    dest += destPR->rowstride;
    lmsk += lmskPR->rowstride;

    if(cmaskParPtr->doProgress)
    {
      p_handle_progress(cmaskParPtr, maskPR->w, "p_colormask_rgn_render_region");
    }
  }

}  /* end p_colormask_rgn_render_region */










/* --------------------------------
 * p_add_point
 * --------------------------------
 * add specified coords as new element to the begin of the pointList
 */
static inline void
p_add_point(GapColorMaskParams *cmaskParPtr, gint32 ix, gint32 iy)
{
  GapAreaCoordPoint *point;
//  static gint32 highWaterMark = 0;

  point = g_new ( GapAreaCoordPoint, 1 );

  point->x = ix;
  point->y = iy;

  point->next = cmaskParPtr->pointList;

  cmaskParPtr->pointList = point;

  cmaskParPtr->pointCount++;

//   //if(gap_debug)
//   {
//     if(cmaskParPtr->pointCount > highWaterMark)
//     {
//        highWaterMark = cmaskParPtr->pointCount;
//        printf("(%d) add pointCount:%d Seed x/y: (%d/%d) x/y: (%d/%d)\n"
//          ,(int)cmaskParPtr->currentAreaPass
//          ,(int)cmaskParPtr->pointCount
//          ,(int)cmaskParPtr->seed_ix
//          ,(int)cmaskParPtr->seed_iy
//          ,(int)ix
//          ,(int)iy
//          );
//     }
//   }
//
//   if(cmaskParPtr->pointCount > (cmaskParPtr->width * cmaskParPtr->height))
//   {
//      printf("(%d) internal error... pointCount:%d seed x/y: (%d/%d)\n"
//          ,(int)cmaskParPtr->currentAreaPass
//          ,(int)cmaskParPtr->pointCount
//          ,(int)cmaskParPtr->seed_ix
//          ,(int)cmaskParPtr->seed_iy
//        );
//      exit(44);
//   }

}  /* end p_add_point */


/* --------------------------------
 * p_remove_first_point
 * --------------------------------
 * remove first element from the pointList
 */
static inline void
p_remove_first_point(GapColorMaskParams *cmaskParPtr)
{
  GapAreaCoordPoint *point;

  point = cmaskParPtr->pointList;

  if (point != NULL)
  {
//     if(gap_debug)
//     {
//        printf("(%d) remove pointCount:%d point x/y: (%d/%d)\n"
//          ,(int)cmaskParPtr->currentAreaPass
//          ,(int)cmaskParPtr->pointCount
//          ,(int)point->x
//          ,(int)point->y
//          );
//     }
    cmaskParPtr->pointCount--;
    cmaskParPtr->pointList = (GapAreaCoordPoint *)point->next;
    g_free(point);
  }
  else
  {
    //if(gap_debug)
    {
       printf("(%d) remove pointCount:%d seed x/y: (%d/%d)  pointList is NULL!\n"
         ,(int)cmaskParPtr->currentAreaPass
         ,(int)cmaskParPtr->pointCount
         ,(int)cmaskParPtr->seed_ix
         ,(int)cmaskParPtr->seed_iy
         );
    }
  }

}  /* end p_remove_first_point */


/* --------------------------------------------
 * p_free_point_list
 * --------------------------------------------
 */
static void
p_free_point_list(GapColorMaskParams *cmaskParPtr)
{
  while(cmaskParPtr->pointList != NULL)
  {
      p_remove_first_point(cmaskParPtr);
  }
}  /* end p_free_point_list */



/* -------------------------------------------------------------------- */
/* ------------------ stuff for EDGE ALGORITHM  ----- START ------------ */
/* -------------------------------------------------------------------- */

/* --------------------------------------
 * p_edge_calculate_pixel_opacity  (EDGE VARIANT)
 * --------------------------------------
 * calculates transparency according to
 * color difference of individual pixel in the processed
 * layer and its corresponding pixel in the colormask.
 *
 * returns the calculated opacity as byte value.
 */
static inline guchar
p_edge_calculate_pixel_opacity(gint maskBpp
     , guchar *maskPtr
     , guchar *origPtr
     , guchar *origUpper
     , guchar *origLeft
     , guchar *lmskUpper
     , guchar *lmskLeft
     , GapColorMaskParams *cmaskParPtr
     )
{
  gdouble colorDiff;
  guchar  nbAlpha;
  guchar          pixel[4];
  guchar          layermaskPixel[4];

  if (maskBpp > 3)
  {
    /* colormask has an alpha channel */
    if (maskPtr[3] <= cmaskParPtr->triggerAlpha255)
    {
      /* transparent mask (below or equal trigger value)
       * switches off color masking for that pixel
       * return 255 (full opaque) as opacity value
       * for the layermask.
       * Note that an opaque layermask pixel
       * preserves opacity as set via alpha channel unchanged when the layermask is applied.
       * (no need to check color differences in that case)
       */
      return (255);
    }
  }

  colorDiff = p_colordiff_guchar_cmask(maskPtr
                                 , origPtr
                                 , cmaskParPtr
                                 );

  p_set_dynamic_threshold_by_KeyColor(origPtr, cmaskParPtr);

  if(colorDiff <= cmaskParPtr->colorThreshold)
  {
    /* color matches good */
    return (cmaskParPtr->lowerAlpha255);
  }

  if (colorDiff >= cmaskParPtr->hiColorThreshold)
  {
    /* color does not match */
    return (cmaskParPtr->upperAlpha255);
  }


///////////////////////////// DEBUG
//   if(TRUE)
//   {
//     gdouble shadeFactor;
//     gdouble green;
//
//     shadeFactor = (colorDiff - cmaskParPtr->colorThreshold) / cmaskParPtr->shadeRange;
//     green = GAP_BASE_MIX_VALUE(shadeFactor
//                               , (gdouble)128.0
//                               , (gdouble)255.0
//                               );
//
//     origPtr[0] = 32;    /* origRed   */
//     origPtr[1] = CLAMP((guchar)green, 0, 255);;   /* origGreen */
//     origPtr[2] = 32;    /* origBlue  */
//
//     return (cmaskParPtr->lowerAlpha255);
//   }
///////////////////////////// DEBUG



  /* color matches range between the thresholds
   * in this case check if the current pixel is an edge pixel
   * for edge pixels use lower alpha,
   * for non-edge pixels use same alpha as the already processed left (or upper) neighbour pixel
   */

  nbAlpha = cmaskParPtr->lowerAlpha255;

  /* check upper neigbour */
//   if(gap_debug)
//   {
//     printf("X/Y: %d/%d origUpper:%d lmskUpper:%d\n"
//       , (int)cmaskParPtr->x
//       , (int)cmaskParPtr->y
//       , (int)origUpper
//       , (int)lmskUpper
//       );
//   }

  if (origUpper != NULL)
  {
    colorDiff = p_colordiff_guchar_cmask(origUpper
                                 , origPtr
                                 , cmaskParPtr
                                 );
    nbAlpha = lmskUpper[0];
  }
  else
  {
    /* current pixel is first in tile column
     * in this special case use pixel fether to access neighbour pixel
     */
    if(cmaskParPtr->y > 0)
    {
      gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDest
                                , cmaskParPtr->x
                                , cmaskParPtr->y-1
                                , &pixel[0]
                                );
      gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                , cmaskParPtr->x
                                , cmaskParPtr->y-1
                                , &layermaskPixel[0]
                                );
      nbAlpha = layermaskPixel[0];
      colorDiff = p_colordiff_guchar_cmask(&pixel[0]
                                   , origPtr
                                   , cmaskParPtr
                                   );
    }
    else
    {
      colorDiff = 1.0;  /* coordinate x == 0 is considered as edge pixel */
    }
  }

  if (colorDiff >= cmaskParPtr->edgeColorThreshold)
  {
    return (cmaskParPtr->lowerAlpha255);
  }



  /* check left neigbour */

//   if(gap_debug)
//   {
//     printf("X/Y: %d/%d origLeft:%d lmskLeft:%d\n"
//       , (int)cmaskParPtr->x
//       , (int)cmaskParPtr->y
//       , (int)origLeft
//       , (int)lmskLeft
//       );
//   }

  if (origLeft != NULL)
  {
    colorDiff = p_colordiff_guchar_cmask(origLeft
                                 , origPtr
                                 , cmaskParPtr
                                 );
    nbAlpha = lmskLeft[0];
  }
  else
  {
    /* current pixel is first in tile row
     * in this special case use pixel fether to access neighbour pixel
     */
    if(cmaskParPtr->x > 0)
    {
      gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDest
                                , cmaskParPtr->x-1
                                , cmaskParPtr->y
                                , &pixel[0]
                                );
      gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                , cmaskParPtr->x-1
                                , cmaskParPtr->y
                                , &layermaskPixel[0]
                                );
      nbAlpha = layermaskPixel[0];
      colorDiff = p_colordiff_guchar_cmask(&pixel[0]
                                   , origPtr
                                   , cmaskParPtr
                                   );
    }
    else
    {
      colorDiff = 1.0;  /* coordinate x == 0 is considered as edge pixel */
    }
  }

  if (colorDiff >= cmaskParPtr->edgeColorThreshold)
  {
    return (cmaskParPtr->lowerAlpha255);
  }


  return (nbAlpha);
}  /* end p_edge_calculate_pixel_opacity */



/* ---------------------------------
 * p_colormask_edge_rgn_render_region
 * ---------------------------------
 * render transparency according to colormask
 * to the layermask of the destination drawable.
 */
static void
p_colormask_edge_rgn_render_region (const GimpPixelRgn *maskPR
                    ,const GimpPixelRgn *origPR
                    ,const GimpPixelRgn *lmskPR
                    ,GapColorMaskParams *cmaskParPtr)
{
  guint    row;
  guchar* mask = maskPR->data;   /* the colormask */
  guchar* orig = origPR->data;   /* the drawable */
  guchar* lmsk = lmskPR->data;   /* the layermask of the drawable */

  guchar* origUpper = NULL;      /* the upper pixelrow of the drawable */
  guchar* lmskUpper = NULL;      /* the upper pixelrow of the layermask of the drawable */

  for (row = 0; row < origPR->h; row++)
  {
    guchar* origLeft;      /* the left neighbor pixel of the drawable */
    guchar* lmskLeft;      /* the upper pixelrow of the layermask of the drawable */
    guint  col;
    guint  idxMask;
    guint  idxOrig;
    guint  idxLmsk;

    idxMask = 0;
    idxOrig = 0;
    idxLmsk = 0;
    origLeft = NULL;
    lmskLeft = NULL;

    cmaskParPtr->y = origPR->y + row;

//     if(gap_debug)
//     {
//       printf("ROW:%d Y: %d origUpper:%d lmskUpper:%d\n"
//       , (int)row
//       , (int)cmaskParPtr->y
//       , (int)origUpper
//       , (int)lmskUpper
//       );
//     }

    for(col = 0; col < origPR->w; col++)
    {
      cmaskParPtr->x = origPR->x + col;
      if(origUpper != NULL)
      {
        lmsk[idxLmsk] = p_edge_calculate_pixel_opacity(maskPR->bpp
                          , &mask[idxMask]     /* maskPtr */
                          , &orig[idxOrig]     /* origPtr */
                          , &origUpper[idxOrig]
                          , origLeft
                          , &lmskUpper[idxLmsk]
                          , lmskLeft
                          , cmaskParPtr
                          );
      }
      else
      {
        lmsk[idxLmsk] = p_edge_calculate_pixel_opacity(maskPR->bpp
                          , &mask[idxMask]     /* maskPtr */
                          , &orig[idxOrig]     /* origPtr */
                          , NULL
                          , origLeft
                          , NULL
                          , lmskLeft
                          , cmaskParPtr
                          );
      }

      origLeft = &orig[idxOrig];
      lmskLeft = &lmsk[idxLmsk];

      idxOrig += origPR->bpp;
      idxMask += maskPR->bpp;
      idxLmsk += lmskPR->bpp;
    }
    lmskUpper = lmsk;
    origUpper = orig;


    mask += maskPR->rowstride;
    orig += origPR->rowstride;
    lmsk += lmskPR->rowstride;

    if(cmaskParPtr->doProgress)
    {
      p_handle_progress(cmaskParPtr, maskPR->w, "p_colormask_edge_rgn_render_region");
    }
  }

}  /* end p_colormask_edge_rgn_render_region */


/* -------------------------------------------------------------------- */
/* ------------------ stuff for EDGE ALGORITHM  ----- END ------------- */
/* -------------------------------------------------------------------- */






/* ---------------------------------------------
 * p_get_distances_to_nearest_transparent_pixels
 * ---------------------------------------------
 * check distances to next transparent neghbour pixels (where opacity value in the layermask
 * is lower or equal to the specified transparencyLevel value)
 * in all 4 directions.
 *   [0]  right neigbour
 *   [1]  left neigbour
 *   [2]  lower neigbour
 *   [3]  upper neigbour
 *
 * Note: in case there is no transparent pixel found within featherRadius
 * the nbDistance value for that direction is set to 100 + featherRadius
 * to indicate that no transparent pixel is near.
 */
static inline void
p_get_distances_to_nearest_transparent_pixels(GapColorMaskParams *cmaskParPtr, gdouble *nbDistance)
{
  guchar  nbOrigPixels[4][4];
  gint    ii;
  gint    xx;
  gint    yy;

  nbDistance[0] = 100 + cmaskParPtr->featherRadius;
  nbDistance[1] = 100 + cmaskParPtr->featherRadius;
  nbDistance[2] = 100 + cmaskParPtr->featherRadius;
  nbDistance[3] = 100 + cmaskParPtr->featherRadius;


  for(ii=1; ii <= cmaskParPtr->featherRadius; ii++)
  {
    xx = cmaskParPtr->x + ii;
    if (xx >= cmaskParPtr->width)
    {
      break;
    }
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , xx
                                 , cmaskParPtr->y
                                 , nbOrigPixels[0]
                                 );
    if(nbOrigPixels[0][0] <= cmaskParPtr->transparencyLevel255)
    {
      nbDistance[0] = ii;
      break;
    }
  }

  for(ii=1; ii <= cmaskParPtr->featherRadius; ii++)
  {
    xx = cmaskParPtr->x - ii;
    if (xx < 0)
    {
      break;
    }
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , xx
                                 , cmaskParPtr->y
                                 , nbOrigPixels[1]
                                 );
    if(nbOrigPixels[1][0] <= cmaskParPtr->transparencyLevel255)
    {
      nbDistance[1] = ii;
      break;
    }
  }

  for(ii=1; ii <= cmaskParPtr->featherRadius; ii++)
  {
    yy = cmaskParPtr->y + ii;
    if (yy >= cmaskParPtr->height)
    {
      break;
    }
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , cmaskParPtr->x
                                 , yy
                                 , nbOrigPixels[2]
                                 );
    if(nbOrigPixels[2][0] <= cmaskParPtr->transparencyLevel255)
    {
      nbDistance[2] = ii;
      break;
    }
  }

  for(ii=1; ii <= cmaskParPtr->featherRadius; ii++)
  {
    yy = cmaskParPtr->y - ii;
    if (yy < 0)
    {
      break;
    }
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , cmaskParPtr->x
                                 , yy
                                 , nbOrigPixels[3]
                                 );
    if(nbOrigPixels[3][0] <= cmaskParPtr->transparencyLevel255)
    {
      nbDistance[3] = ii;
      break;
    }
  }


}  /* end p_get_distances_to_nearest_transparent_pixels */



/* ---------------------------------
 * p_blur_alpha_at_edge_pixels
 * ---------------------------------
 * calculate blurred alpha value depentent to distance to next
 * transparent pixel (whre transparency is below level)
 * to the layermask of the destination drawable.
 */
static inline guchar
p_blur_alpha_at_edge_pixels(guchar currentAlpha, GapColorMaskParams *cmaskParPtr)
{
  gdouble distance[4];
  gdouble minDistance;
  gdouble opacity255;  /* range 0.0 to 255.0 */
  guchar  alpha;
  gdouble radiusFactor;
  gint    ii;


  p_get_distances_to_nearest_transparent_pixels(cmaskParPtr, &distance[0]);

  minDistance = 100 + cmaskParPtr->featherRadius;
  for(ii=0; ii < 4; ii++)
  {
     if (distance[ii] < minDistance)
     {
       minDistance = distance[ii];
     }
  }

  if (cmaskParPtr->debugCoordinate)
  {
    printf("p_smooth_alpha_at_edges distances: %.1f  %.1f %.1f %.1f  minDistance %.1f\n"
      ,(float)distance[0]
      ,(float)distance[1]
      ,(float)distance[2]
      ,(float)distance[3]
      ,(float)minDistance
      );
  }

  if(minDistance > cmaskParPtr->featherRadius)
  {
    /* no transparent pixel is near, return the unchanged alpha value */
    return (currentAlpha);
  }

  /* one of the neigbours within featherRadius is transparent (below transparencyLevel)
   * now calculate the fading opacity for this current edge pixel
   * (the bigger the distande, the more opaque)
   * note that fading is done within the range of specified upper and lower opacity parameters
   */
  radiusFactor = minDistance / (1 + cmaskParPtr->featherRadius);
  opacity255 = (gdouble)(currentAlpha - cmaskParPtr->transparencyLevel255);
  opacity255 *= radiusFactor;
  opacity255 *= cmaskParPtr->opacityFactor;

  alpha = CLAMP(((guchar)opacity255 + cmaskParPtr->transparencyLevel255), 0, 255);

  if (cmaskParPtr->debugCoordinate)
  {
    printf("p_blur_alpha_at_edge_pixels at xy:(%d / %d) radiusFactor:%.4f opacityFactor:%.4f"
           " transparencyLevel:%d  opacity255: %.1f  blured_ALPHA %d\n"
      ,(int)cmaskParPtr->x
      ,(int)cmaskParPtr->y
      ,(float)radiusFactor
      ,(float)cmaskParPtr->opacityFactor
      ,(int)cmaskParPtr->transparencyLevel255
      ,(float)opacity255
      ,(int)alpha
      );
  }

  return (alpha);

}  /* end p_blur_alpha_at_edge_pixels */



/* ---------------------------------
 * p_smooth_edges_rgn_render_region
 * ---------------------------------
 * render transparency according to colormask
 * to the layermask of the destination drawable.
 */
static void
p_smooth_edges_rgn_render_region (const GimpPixelRgn *maskPR
                    ,const GimpPixelRgn *lmskPR
                    ,GapColorMaskParams *cmaskParPtr)
{
  guint    row;
  guchar* mask = maskPR->data;   /* the colormask */
  guchar* lmsk = lmskPR->data;   /* the layermask of the destination drawable */


  for (row = 0; row < lmskPR->h; row++)
  {
    guint  col;
    guint  idxMask;
    guint  idxLmsk;

    idxMask = 0;
    idxLmsk = 0;
    for(col = 0; col < lmskPR->w; col++)
    {
      /* process only non transparent pixels in the layermask */
      if (lmsk[idxLmsk] > cmaskParPtr->transparencyLevel255)
      {
        gboolean isProtected;

        isProtected = FALSE;
        if (maskPR->bpp > 3)
        {
          if (mask[idxMask +3] <= cmaskParPtr->triggerAlpha255)
          {
            isProtected = TRUE;
          }
        }

        if (!isProtected)
        {
          cmaskParPtr->x = maskPR->x + col;
          cmaskParPtr->y = maskPR->y + row;

          //if(gap_debug)
          {
            cmaskParPtr->debugCoordinate = p_is_debug_active_on_coordinate(cmaskParPtr);
          }

          /* re-calculate opacity respecting featherRadius near edges */
          lmsk[idxLmsk] = p_blur_alpha_at_edge_pixels(lmsk[idxLmsk], cmaskParPtr);
        }
      }

      idxMask += maskPR->bpp;
      idxLmsk += lmskPR->bpp;
    }

    mask += maskPR->rowstride;
    lmsk += lmskPR->rowstride;

    if(cmaskParPtr->doProgress)
    {
      p_handle_progress(cmaskParPtr, maskPR->w, "p_smooth_edges_rgn_render_region");
    }
  }

}  /* end p_smooth_edges_rgn_render_region */



/* -------------------------------------------------------------------- */
/* ------------------ stuff for remove ISOLATED PIXELS ---------------- */
/* -------------------------------------------------------------------- */

/* --------------------------------------
 * p_map_alpha
 * --------------------------------------
 * returns 0 if alpha considerd as transparent
 *           otherwise 255
 */
static inline guchar
p_map_alpha(GapColorMaskParams *cmaskParPtr, guchar value)
{
  if (value <=  cmaskParPtr->transparencyLevel255)
  {
    return (0);
  }
  return (255);
} /* end p_map_alpha */




/* ---------------------------------------------
 * p_is_isolated_pixel
 * ---------------------------------------------
 * returns TRUE in case pixel is isolated
 * the isolation check is done by comparing the currentAlpha value
 * against neighbour pixels in the layermask.
 * In case there are no (or too few) neigbours with matching opacity
 * the pixel is considered as isolated pixel
 *
 */
static inline gboolean
p_is_isolated_pixel(GapColorMaskParams *cmaskParPtr, guchar currentAlpha)
{
  static gboolean nbCountLookup[5][5] = {
  /*  0      1      2     3     4        countCornerNb
   *
   *  ...    #..    #..    #.#    #.#
   *  .o.    .o.    .o.    .o.    .o.
   *  ...    ...    ..#    ..#    #.#
   */
  {   TRUE,  TRUE,  TRUE,  TRUE,  TRUE  }  /* countEdgeNb == 0 */


  /*  0      1      2     3     4        countCornerNb
   *
   *  .#.    ##.    ##.    ###    ###
   *  .o.    .o.    .o.    .o.    .o.
   *  ...    ...    ..#    ..#    #.#
   */
 ,{   TRUE,  TRUE,  TRUE,  FALSE, FALSE  }  /* countEdgeNb == 1 */



  /*  0      1      2     3     4        countCornerNb
   *
   *  .#.    ##.    ##.    ###    ###
   *  .o.    .o.    .o.    .o.    .o.
   *  .#.    .#.    .##    .##    ###
   */
 ,{   TRUE,  FALSE, FALSE, FALSE, FALSE  }  /* countEdgeNb == 2 */


  /*  0      1      2     3     4        countCornerNb
   *
   *  .#.    ##.    ##.    ###    ###
   *  .o#    .o#    .o#    .o#    .o#
   *  .#.    .#.    .##    .##    ###
   */
 ,{   FALSE, FALSE, FALSE,  FALSE, FALSE }  /* countEdgeNb == 3 */



  /*  0      1      2     3     4        countCornerNb
   *
   *  .#.    ##.    ##.    ###    ###
   *  #o#    #o#    #o#    #o#    #o#
   *  .#.    .#.    .##    .##    ###
   */
 ,{   FALSE, FALSE, FALSE, FALSE, FALSE }  /* countEdgeNb == 4 */
  };

  gboolean isIsolated;
  gint     countCornerNb;
  gint     countEdgeNb;

  guchar  nbLayermaskPixel[4];
  guchar  refAlphaValue;
  gint    xx;
  gint    yy;

  refAlphaValue = p_map_alpha(cmaskParPtr, currentAlpha);
  countEdgeNb = 0;

  /* right neighbor */
  xx = cmaskParPtr->x + 1;
  if (xx < cmaskParPtr->width)
  {
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , xx
                                 , cmaskParPtr->y
                                 , &nbLayermaskPixel[0]
                                 );

    if(p_map_alpha(cmaskParPtr, nbLayermaskPixel[0]) == refAlphaValue)
    {
      countEdgeNb++;
    }
  }

  /* left neighbor */
  xx = cmaskParPtr->x - 1;
  if (xx >= 0)
  {
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , xx
                                 , cmaskParPtr->y
                                 , &nbLayermaskPixel[0]
                                 );

    if(p_map_alpha(cmaskParPtr, nbLayermaskPixel[0]) == refAlphaValue)
    {
      countEdgeNb++;
    }
  }


  /* lower neighbor */
  yy = cmaskParPtr->y + 1;
  if (yy < cmaskParPtr->height)
  {
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , cmaskParPtr->x
                                 , yy
                                 , &nbLayermaskPixel[0]
                                 );

    if(p_map_alpha(cmaskParPtr, nbLayermaskPixel[0]) == refAlphaValue)
    {
      countEdgeNb++;
    }
  }

  if(countEdgeNb > 2)
  {
    return (FALSE);
  }

  /* upper neighbor */
  yy = cmaskParPtr->y - 1;
  if (yy >= 0)
  {
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , cmaskParPtr->x
                                 , yy
                                 , &nbLayermaskPixel[0]
                                 );

    if(p_map_alpha(cmaskParPtr, nbLayermaskPixel[0]) == refAlphaValue)
    {
      countEdgeNb++;
    }
  }

  if(countEdgeNb > 2)
  {
    return (FALSE);
  }

  /* there are 2 or less neighbours
   * check corner neighbours in that case
   */

  countCornerNb = 0;

  /* right lower neighbor */
  xx = cmaskParPtr->x + 1;
  yy = cmaskParPtr->y + 1;
  if ((xx < cmaskParPtr->width) && (yy < cmaskParPtr->height))
  {
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , xx
                                 , yy
                                 , &nbLayermaskPixel[0]
                                 );

    if(p_map_alpha(cmaskParPtr, nbLayermaskPixel[0]) == refAlphaValue)
    {
      countCornerNb++;
    }
  }

  /* left upper neighbor */
  xx = cmaskParPtr->x - 1;
  yy = cmaskParPtr->y - 1;
  if ((xx >= 0) && (yy >= 0))
  {
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , xx
                                 , yy
                                 , &nbLayermaskPixel[0]
                                 );

    if(p_map_alpha(cmaskParPtr, nbLayermaskPixel[0]) == refAlphaValue)
    {
      countCornerNb++;
    }
  }


  /* left lower neighbor */
  xx = cmaskParPtr->x - 1;
  yy = cmaskParPtr->y + 1;
  if ((xx >= 0) && (yy < cmaskParPtr->height))
  {
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , xx
                                 , yy
                                 , &nbLayermaskPixel[0]
                                 );

    if(p_map_alpha(cmaskParPtr, nbLayermaskPixel[0]) == refAlphaValue)
    {
      countCornerNb++;
    }
  }


  /* right upper neighbor */
  xx = cmaskParPtr->x + 1;
  yy = cmaskParPtr->y - 1;
  if ((xx < cmaskParPtr->width) && (yy >= 0))
  {
    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                 , xx
                                 , yy
                                 , &nbLayermaskPixel[0]
                                 );

    if(p_map_alpha(cmaskParPtr, nbLayermaskPixel[0]) == refAlphaValue)
    {
      countCornerNb++;
    }
  }

  isIsolated = nbCountLookup[countEdgeNb][countCornerNb];

  if (cmaskParPtr->debugCoordinate)
  {
    printf("countEdgeNb:%d countCornerNb:%d isIsolated:%d\n"
       ,(int)countEdgeNb
       ,(int)countCornerNb
       ,(int)isIsolated
       );
  }

  return (isIsolated);

}  /* end p_is_isolated_pixel */



/* --------------------------------------------
 * p_isle_check_and_mark_nb_pixel
 * --------------------------------------------
 * checks if the specified neighbor pixel at coords nx, ny
 * has same opacity value as refOpacity (e.g the same opacity as the seed pixel).
 * If this is the case increment pixel count and
 * and add nx/ny to the point list (to initiate check of further neighbour pixels)
 *
 * The isleAreaTable is updated to mark already processed pixels
 * (either as WORK_VISITED_BUT_NOT_PART_OF_CURRENT_AREA or as WORK_PART_OF_CURRENT_AREA)
 *
 */
static void
p_isle_check_and_mark_nb_pixel(GapColorMaskParams *cmaskParPtr, gint nx, gint ny, guchar refOpacity)
{
  guchar          pixel[4];
  gint32 ax;
  gint32 ay;


  ax = nx + cmaskParPtr->seedOffsetX;
  ay = ny + cmaskParPtr->seedOffsetY;

  if (cmaskParPtr->isleAreaTable[ax][ay] != WORK_UNCHECKED)
  {
     /* this neighbor pixel is already processed (nothing to be done in that case) */
     return;
  }


  gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDestLayerMask
                                , nx
                                , ny
                                , &pixel[0]
                               );

  if (pixel[0] != refOpacity)
  {
    /* currently processed layermask pixel has other opacity than current seed
     * and is not member of the checked isle area
     */
    cmaskParPtr->isleAreaTable[ax][ay] = WORK_VISITED_BUT_NOT_PART_OF_CURRENT_AREA;
    return;
  }

  /* mark this neighbor pixel */
  cmaskParPtr->isleAreaTable[ax][ay] = WORK_PART_OF_CURRENT_AREA;
  cmaskParPtr->pixelCount++;
  p_add_point(cmaskParPtr, nx, ny);

  if (cmaskParPtr->debugCoordinate)
  {
    printf("ClipArea Check MATCH at nx:%d ny:%d pixelCount:%d refOpacity:%d opacity:%d\n"
      ,(int)nx
      ,(int)ny
      ,(int)cmaskParPtr->pixelCount
      ,(int)refOpacity
      ,(int)pixel[0]
      );
  }

}  /* p_isle_check_and_mark_nb_pixel */



/* --------------------------------------------
 * p_is_pixel_a_small_isolated_area
 * --------------------------------------------
 *
 * find pixels of same opacity as the specified refOpacity of the current processed seed pixel (cmaskParPtr->x / y)
 * and count neighbor pixel (connected at edges) with same opacity value.
 * Note that the search is limited to a small clipping area that fits into isleAreaTable size
 * This is done for performance reasons.
 *
 * return TRUE  in case the seed pixel is isolated 
 * return FALSE in case seed pixel is part of an area with same opacity
 *                      that is larger than the specified isleAreaPixelLimit,
 *                      OR in case the area touches the border of the checked clipping area rectangle.
 *
 *
 */
static gboolean
p_is_pixel_a_small_isolated_area(GapColorMaskParams *cmaskParPtr, guchar refOpacity)
{
  gint32          ix;
  gint32          iy;
  gint32          ax;
  gint32          ay;

  gint            clipSelX1;
  gint            clipSelX2;
  gint            clipSelY1;
  gint            clipSelY2;
  gint            maxClipRadius;
  gint            usedArraySize;
  gboolean        isBorderReached;
  
  
  maxClipRadius = MIN(ISLE_AREA_MAX_RADIUS, cmaskParPtr->isleRadius);
  usedArraySize = (1 + (2 * maxClipRadius));

  cmaskParPtr->pointCount = 0;

  cmaskParPtr->seedOffsetX = maxClipRadius - cmaskParPtr->x;
  cmaskParPtr->seedOffsetY =  maxClipRadius - cmaskParPtr->y;


  /* clipping area boundaries to limit area checks
   * for a small rectangular area +- maxClipRadius around current pixel
   */
  clipSelX1 = MAX(cmaskParPtr->sel_x1, (cmaskParPtr->x - maxClipRadius));
  clipSelY1 = MAX(cmaskParPtr->sel_y1, (cmaskParPtr->y - maxClipRadius));

  clipSelX2 = MIN(cmaskParPtr->sel_x2, (cmaskParPtr->x + maxClipRadius));
  clipSelY2 = MIN(cmaskParPtr->sel_y2, (cmaskParPtr->y + maxClipRadius));


  /* reset area context, clear used part of the isleAreaTable */
  for(ax=0; ax < usedArraySize; ax++)
  {
    for(ay=0; ay < usedArraySize; ay++)
    {
      cmaskParPtr->isleAreaTable[ax][ay] = WORK_UNCHECKED;
    }
  }

  cmaskParPtr->pixelCount = 1;
  cmaskParPtr->pointList = NULL;

  /* mark seed pixel */
  ax = cmaskParPtr->x + cmaskParPtr->seedOffsetX;
  ay = cmaskParPtr->y + cmaskParPtr->seedOffsetY;
  cmaskParPtr->isleAreaTable[ax][ay] = WORK_PART_OF_CURRENT_AREA +1;

  ix = cmaskParPtr->x;
  iy = cmaskParPtr->y;
  
  isBorderReached = FALSE;
  
  while (TRUE)
  {
    /* check of the neighbours */
    if (ix > clipSelX1)
    {
      p_isle_check_and_mark_nb_pixel(cmaskParPtr, ix-1, iy, refOpacity);

      if (ix < clipSelX2)
      {
        p_isle_check_and_mark_nb_pixel(cmaskParPtr, ix+1, iy, refOpacity);

        if (iy > clipSelY1)
        {
          p_isle_check_and_mark_nb_pixel(cmaskParPtr, ix,   iy-1, refOpacity);

          if (iy < clipSelY2)
          {
            p_isle_check_and_mark_nb_pixel(cmaskParPtr, ix,   iy+1, refOpacity);
          }
          else { isBorderReached = TRUE; }

        }
        else { isBorderReached = TRUE; }

      }
      else { isBorderReached = TRUE; }
    } 
    else { isBorderReached = TRUE; }
    




    if((cmaskParPtr->pixelCount > cmaskParPtr->isleAreaPixelLimit)
    || (isBorderReached == TRUE))
    {
      if (cmaskParPtr->debugCoordinate)
      {
        printf("NOT_ISOLATED_AREA  x:%d y:%d ix:%d iy:%d pixelCount:%d isleAreaPixelLimit:%d isBorderReached:%d\n"
          ,(int)cmaskParPtr->x
          ,(int)cmaskParPtr->y
          ,(int)ix
          ,(int)iy
          ,(int)cmaskParPtr->pixelCount
          ,(int)cmaskParPtr->isleAreaPixelLimit
          ,(int)isBorderReached
          );
      }
      /* break because at least one of the non-isolation conditions was detected */
      p_free_point_list(cmaskParPtr);
      return (FALSE);
    }

    if (cmaskParPtr->pointList == NULL)
    {
      if (cmaskParPtr->debugCoordinate)
      {
        printf("ISOLATED_AREA  x:%d y:%d ix:%d iy:%d pixelCount:%d isleAreaPixelLimit:%d isBorderReached:%d\n"
          ,(int)cmaskParPtr->x
          ,(int)cmaskParPtr->y
          ,(int)ix
          ,(int)iy
          ,(int)cmaskParPtr->pixelCount
          ,(int)cmaskParPtr->isleAreaPixelLimit
          ,(int)isBorderReached
          );
      }
      /* area completely checked and pixelCount is bleow isleAreaPixelLimit */
      return (TRUE);
    }

    /* continue checking neigbor pixels of same opacity in the point list */
    ix = cmaskParPtr->pointList->x;
    iy = cmaskParPtr->pointList->y;
    p_remove_first_point(cmaskParPtr);



  }

  return (FALSE);  /* never reached code */


}  /* end p_is_pixel_a_small_isolated_area */




/* ---------------------------------
 * p_remove_isolated_pixels
 * ---------------------------------
 * calculate new alpha value in case the current processed pixel is an
 * isolated pixel
 * return the (possibly changed) alpha value.
 *
 */
static inline guchar
p_remove_isolated_pixels(guchar currentAlpha, GapColorMaskParams *cmaskParPtr)
{
  guchar   invertedAlpha;
  gboolean isIsolated;

  isIsolated = p_is_isolated_pixel(cmaskParPtr, currentAlpha);
  if((!isIsolated) && (cmaskParPtr->isleAreaPixelLimit > 1))
  {
    isIsolated = p_is_pixel_a_small_isolated_area(cmaskParPtr, currentAlpha);
  }

  /* check distance  >= isleRadius
   * return currentAlpha unchanged
   * in case extension of area with same value is large enough in any of the 4 directions
   */
  if(!isIsolated)
  {
    if (cmaskParPtr->debugCoordinate)
    {
      printf("p_remove_isolated_pixels NOT isolated   currentAlpha:%d\n"
        ,(int)currentAlpha
        );
    }
    return (currentAlpha);
  }


  /* the current pixel is an isolated pixel
   * therefore invert its opacity to join with neighbor pixel environment
   */
  if (currentAlpha == cmaskParPtr->lowerAlpha255)
  {
    invertedAlpha = cmaskParPtr->upperAlpha255;
  }
  else
  {
    invertedAlpha = cmaskParPtr->lowerAlpha255;
  }

  if (cmaskParPtr->debugCoordinate)
  {
    printf("p_remove_isolated_pixels ISOLATED  currentAlpha:%d inverted:%d\n"
      ,(int)currentAlpha
      ,(int)invertedAlpha
      );
  }

  return (invertedAlpha);

}  /* end p_remove_isolated_pixels */



/* ------------------------------------------
 * p_remove_isolates_pixels_rgn_render_region
 * ------------------------------------------
 * remove isolated pixels (e.g. areas smaller than isleRadius)
 * from the layermask
 * Note that protected pixels (that are transparent in the colormask)
 * are not affected.
 */
static void
p_remove_isolates_pixels_rgn_render_region (const GimpPixelRgn *maskPR
                    ,const GimpPixelRgn *lmskPR
                    ,GapColorMaskParams *cmaskParPtr)
{
  guint    row;
  guchar* mask = maskPR->data;   /* the colormask (RGB or RGBA) */
  guchar* lmsk = lmskPR->data;   /* the layermask of the destination drawable */


  for (row = 0; row < lmskPR->h; row++)
  {
    guint  col;
    guint  idxMask;
    guint  idxLmsk;

    idxMask = 0;
    idxLmsk = 0;
    for(col = 0; col < lmskPR->w; col++)
    {
      gboolean isProtected;

      isProtected = FALSE;
      if (maskPR->bpp > 3)
      {
        if (mask[idxMask + 3] <= cmaskParPtr->triggerAlpha255)
        {
          isProtected = TRUE;
        }
      }

      if (!isProtected)
      {
        cmaskParPtr->x = maskPR->x + col;
        cmaskParPtr->y = maskPR->y + row;

        //if(gap_debug)
        {
          cmaskParPtr->debugCoordinate = p_is_debug_active_on_coordinate(cmaskParPtr);
        }

        /* re-calculate opacity respecting isolated pixels */
        lmsk[idxLmsk] = p_remove_isolated_pixels(lmsk[idxLmsk], cmaskParPtr);
      }

      idxMask += maskPR->bpp;
      idxLmsk += lmskPR->bpp;
    }

    mask += maskPR->rowstride;
    lmsk += lmskPR->rowstride;

    if(cmaskParPtr->doProgress)
    {
      p_handle_progress(cmaskParPtr, maskPR->w, "p_remove_isolates_pixels_rgn_render_region");
    }
  }

}  /* end p_remove_isolates_pixels_rgn_render_region */










/* ============================ Stuff for average algorithm START ======= */
/* -------------------------------------------------------------------- */
/* ------------------ stuff for average algorithm START    ------------ */
/* -------------------------------------------------------------------- */


/* ------------------------------------------
 * p_set_dynamic_threshold_by_KeyColor
 * ------------------------------------------
 * in case enableKeyColorThreshold is true
 * set colorThreshold and avgColorThreshold
 * dynamic by colrdifference of current color versus KeyColor
 */
static inline void
p_set_dynamic_threshold_by_KeyColor(guchar *pixel
                , GapColorMaskParams *cmaskParPtr)
{
  GimpRGB bRgb;
  GimpHSV bHsv;
  gdouble colordiff;
  gdouble factor;

  if(!cmaskParPtr->enableKeyColorThreshold)
  {
    return; /* dynamic threshold for keycolor feature is disabled */
  }
  
  gimp_rgba_set_uchar (&bRgb, pixel[0], pixel[1], pixel[2], 255);
  gimp_rgb_to_hsv(&bRgb, &bHsv);

  colordiff = gap_colordiff_GimpHSV(&cmaskParPtr->keyColorHsv, &bHsv, cmaskParPtr->colorSensitivity, cmaskParPtr->debugCoordinate);

  factor = CLAMP(colordiff * cmaskParPtr->keyColorSensitivity, 0.0, 1.0);
  cmaskParPtr->colorThreshold = GAP_BASE_MIX_VALUE(factor
                                     , cmaskParPtr->loKeyColorThreshold
                                     , cmaskParPtr->loColorThreshold
                                     );

  cmaskParPtr->avgColorThreshold = cmaskParPtr->colorThreshold
                                 + ((cmaskParPtr->hiColorThreshold - cmaskParPtr->colorThreshold) *  cmaskParPtr->avgFactor);


  if(cmaskParPtr->debugCoordinate)
  {
    printf("colordiff at x:%d y:%d (compared to Keycolor hue:%.3f sat:%.3f val:%.3f):%.4f factor:%.4f colorThreshold:%.4f avgColorThreshold:%.4f\n"
         ,(int)cmaskParPtr->x
         ,(int)cmaskParPtr->y
         ,(float)cmaskParPtr->keyColorHsv.h
         ,(float)cmaskParPtr->keyColorHsv.s
         ,(float)cmaskParPtr->keyColorHsv.v
         ,(float)colordiff
         ,(float)factor
         ,(float)cmaskParPtr->colorThreshold
         ,(float)cmaskParPtr->avgColorThreshold
         );
  }

}  /* end p_set_dynamic_threshold_by_KeyColor */


/* ---------------------------------
 * p_colormask_avg_rgn_render_region
 * ---------------------------------
 * render transparency according to colormask
 * to the layermask of the destination drawable.
 */
static void
p_colormask_avg_rgn_render_region (const GimpPixelRgn *maskPR
                    ,const GimpPixelRgn *destPR
                    ,const GimpPixelRgn *lmskPR
                    ,GapColorMaskParams *cmaskParPtr)
{
  guint    row;
  guchar* mask = maskPR->data;   /* the colormask */
  guchar* dest = destPR->data;   /* the destination drawable */
  guchar* lmsk = lmskPR->data;   /* the layermask of the destination drawable */


  for (row = 0; row < destPR->h; row++)
  {
    guint  col;
    guint  idxMask;
    guint  idxDest;
    guint  idxLmsk;

    idxMask = 0;
    idxDest = 0;
    idxLmsk = 0;
    for(col = 0; col < destPR->w; col++)
    {
      gint32 iTab;
      gboolean isPixelProtected;

      isPixelProtected = FALSE;
      cmaskParPtr->x = destPR->x + col;
      cmaskParPtr->y = destPR->y + row;


      /* ------ start debug code block -----*/
      //if(gap_debug)
      {
        cmaskParPtr->debugCoordinate = p_is_debug_active_on_coordinate(cmaskParPtr);
      }
      /* ------ end debug code block -----*/

      if (maskPR->bpp > 3)
      {
        /* colormask has an alpha channel */
        if (mask[idxMask +3] <= cmaskParPtr->triggerAlpha255)
        {
          /* transparent mask (below or equal trigger value)
           * switches off color masking for that pixel
           * Note that an opaque layermask pixel
           * preserves opacity as set via alpha channel unchanged when the layermask is applied.
           * (no need to check color differences in that case)
           */
          isPixelProtected = TRUE;
        }
      }


      if (isPixelProtected)
      {
        /* protected pixels are set full opaque in the layermask
         * (e.g. they keep their original opacity when layermask is applied)
         */
        lmsk[idxLmsk] = 255;
        if(cmaskParPtr->debugCoordinate)
        {
          printf("ALGO_AVG: x:%d y:%d pixel IS PROTECTED alpha:%d triggerAlpha255:%d\n"
            ,(int)cmaskParPtr->x
            ,(int)cmaskParPtr->y
            ,(int)mask[idxMask +4]
            ,(int)cmaskParPtr->triggerAlpha255
            );
        }
      }
      else
      {
        iTab = p_getColordiffTableIndex(cmaskParPtr, cmaskParPtr->x, cmaskParPtr->y);

        if(cmaskParPtr->debugCoordinate)
        {
          printf("ALGO_AVG: x:%d y:%d iTab:%d  colordiff:%.5f \n"
            ,(int)cmaskParPtr->x
            ,(int)cmaskParPtr->y
            ,(int)iTab
            ,(float)cmaskParPtr->colordiffTable[iTab]
            );
        }


        if (cmaskParPtr->colordiffTable[iTab] >= cmaskParPtr->hiColorThreshold)
        {
          /* NOMATCH pixel color differs significant from colormask pixelcolor
           * set pixel to upper alpha (typically full opaque)
           */
          lmsk[idxLmsk] = cmaskParPtr->upperAlpha255;
        }
        else
        {
          p_set_dynamic_threshold_by_KeyColor(&dest[idxDest], cmaskParPtr);
          if (cmaskParPtr->colordiffTable[iTab] < cmaskParPtr->colorThreshold)
          {
            /* MATCH pixel color equal or nearly equal to colormask pixelcolor */
            lmsk[idxLmsk] = p_calc_opacity_for_matching_color(cmaskParPtr);
          }
          else
          {
            /* RANGE pixel color falls in the range of similar but not best matching colors
             */
            lmsk[idxLmsk] = p_calc_opacity_for_range_color(cmaskParPtr);
          }
        }
      }



      idxDest += destPR->bpp;
      idxMask += maskPR->bpp;
      idxLmsk += lmskPR->bpp;
    }

    mask += maskPR->rowstride;
    dest += destPR->rowstride;
    lmsk += lmskPR->rowstride;

    if(cmaskParPtr->doProgress)
    {
      p_handle_progress(cmaskParPtr, maskPR->w, "p_colormask_avg_rgn_render_region");
    }
  }

}  /* end p_colormask_avg_rgn_render_region */


/* -----------------------------------------
 * p_check_matching_pixel_isolation
 * -----------------------------------------
 * check if matching pixel is embedded into non matching pixels
 * and return TRUE in this case.
 * if there are more matching or range pixels in one horizontal or vertical row,
 * the pixel is considered as not isolated and FALSE is returned.
 */
static gboolean
p_check_matching_pixel_isolation(GapColorMaskParams *cmaskParPtr)
{
  gdouble sumNbColorDiff;
  gdouble countNbPixels;
  gdouble matchWidth;
  gdouble matchHeight;

  /* check matching width */
  p_check_range_neighbour_sum(cmaskParPtr
                             , -1  /* dx */
                             , 0   /* dy */
                             , cmaskParPtr->checkMatchRadius
                             , &sumNbColorDiff
                             , &countNbPixels
                             );
  if (countNbPixels >= cmaskParPtr->checkMatchRadius)
  {
    return (FALSE);
  }

  matchWidth = countNbPixels;

  p_check_range_neighbour_sum(cmaskParPtr
                             , 1   /* dx */
                             , 0   /* dy */
                             , cmaskParPtr->checkMatchRadius
                             , &sumNbColorDiff
                             , &countNbPixels
                             );
  matchWidth += (countNbPixels -1);
  if (matchWidth >= cmaskParPtr->checkMatchRadius)
  {
    return (FALSE);
  }

  /* check matching height */
  p_check_range_neighbour_sum(cmaskParPtr
                             , 0    /* dx */
                             , -1   /* dy */
                             , cmaskParPtr->checkMatchRadius
                             , &sumNbColorDiff
                             , &countNbPixels
                             );
  if (countNbPixels >= cmaskParPtr->checkMatchRadius)
  {
    return (FALSE);
  }

  matchHeight = countNbPixels;

  p_check_range_neighbour_sum(cmaskParPtr
                             , 0   /* dx */
                             , 1   /* dy */
                             , cmaskParPtr->checkMatchRadius
                             , &sumNbColorDiff
                             , &countNbPixels
                             );
  matchHeight += (countNbPixels -1);
  if (matchHeight >= cmaskParPtr->checkMatchRadius)
  {
    return (FALSE);
  }

  if (cmaskParPtr->debugCoordinate)
  {
    printf("matching pixel is isolated matchWidth:%d matchHeight:%d radius:%d\n"
      , (int)matchWidth
      , (int)matchHeight
      , (int)cmaskParPtr->checkMatchRadius
      );
  }

  return (TRUE);
}  /* end p_check_matching_pixel_isolation */


/* ---------------------------------
 * p_calc_opacity_for_matching_color
 * ---------------------------------
 * in case all or nearly all pixels within radius do NOT MATCH set
 * current pixel opaque
 */
static guchar
p_calc_opacity_for_matching_color(GapColorMaskParams *cmaskParPtr)
{
  if (p_check_matching_pixel_isolation(cmaskParPtr) == TRUE)
  {
    return (cmaskParPtr->upperAlpha255);
  }

  return (cmaskParPtr->lowerAlpha255);
}  /* end p_calc_opacity_for_matching_color */


/* ---------------------------------
 * p_calc_opacity_for_range_color
 * ---------------------------------
 * range pixels are set to matching lower alpha (transparent) in case where 
 * the configured average algorithm check returns TRUE (considered as matching)
 * otherwise range pixels are set to upper alpha (opaque)
 */
static guchar
p_calc_opacity_for_range_color(GapColorMaskParams *cmaskParPtr)
{
  if(p_check_average_colordiff_incl_neighbours(cmaskParPtr) == TRUE)
  {
    /* average matches good */
    return (cmaskParPtr->lowerAlpha255);
  }



  return (cmaskParPtr->upperAlpha255);

}  /* end p_calc_opacity_for_range_color */


/* ---------------------------------
 * p_check_range_neighbour_sum
 * ---------------------------------
 */
static void
p_check_range_neighbour_sum(GapColorMaskParams *cmaskParPtr, gint dx, gint dy, gint radius, gdouble *sumNbColorDiff, gdouble *countNbPixels)
{
  gint iTab;
  gint ii;

  iTab = p_getColordiffTableIndex(cmaskParPtr, cmaskParPtr->x, cmaskParPtr->y);
  *sumNbColorDiff = cmaskParPtr->colordiffTable[iTab];
  *countNbPixels = 1.0;

  for(ii=1; ii < radius; ii++)
  {
    gint xx;
    gint yy;

    xx = cmaskParPtr->x + (ii * dx);
    yy = cmaskParPtr->y + (ii * dy);

    if (cmaskParPtr->debugCoordinate)
    {
      printf("CHK ii:%d radius:%d\n"
        , (int)ii
        , (int)radius
        );
    }


    /* clipping check */
    if ((xx < 0) || (yy < 0) || (xx >= cmaskParPtr->width) || (yy >= cmaskParPtr->height))
    {
      return;
    }

    iTab = p_getColordiffTableIndex(cmaskParPtr, xx, yy);

    if (cmaskParPtr->debugCoordinate)
    {
      printf("CHK xx:%d yy:%d countNbPixels:%.1f cmaskParPtr:%d hiColorThreshold:%.4f colordiffTab:%.4f\n"
        , (int)xx
        , (int)yy
        , *countNbPixels
        , (int)cmaskParPtr
        , (float)cmaskParPtr->hiColorThreshold
        , (float)cmaskParPtr->colordiffTable[iTab]
        );
    }

    if (cmaskParPtr->colordiffTable[iTab] >= cmaskParPtr->hiColorThreshold)
    {
      /* stop checking because we have reached a non matching pixel */
      return;
    }

    *sumNbColorDiff += cmaskParPtr->colordiffTable[iTab];
    *countNbPixels  += 1.0;
  }

}  /* end p_check_range_neighbour_sum */




/* --------------------------------------------
 * p_countPerColordiff
 * --------------------------------------------
 * count and update colordiff statistics for matching, range and not matching pixels
 * within the checked area. (typical used to analyze a small area of same or similar
 * color within a clipping rectangle)
 */
static void
p_countPerColordiff(GapColorMaskParams *cmaskParPtr, gdouble colordiff, gint mi)
{
  if (colordiff < cmaskParPtr->avgColorThreshold) ////## ?? loColorThreshold)
  {
    cmaskParPtr->sumMatch[mi] += colordiff;
    cmaskParPtr->countMatch[mi]++;
    return;
  }

  if (colordiff >= cmaskParPtr->hiColorThreshold)
  {
    cmaskParPtr->sumNoMatch[mi] += colordiff;
    cmaskParPtr->countNoMatch[mi]++;
    return;
  }

  cmaskParPtr->sumRange[mi] += colordiff;
  cmaskParPtr->countRange[mi]++;

}  /* end p_countPerColordiff */





/* --------------------------------------------
 * p_avg_check_and_mark_nb_pixel
 * --------------------------------------------
 * checks if the specified neighbor pixel at coords nx, ny
 * has similar color as the seed pixel.
 * If this is the case increment pixel count and summary colordiffrenece
 * and add nx/ny to the point list (to initiate check of further neighbour pixels)
 *
 * The clipAreaTable is updated to mark already processed pixels
 * (either as WORK_VISITED_BUT_NOT_PART_OF_CURRENT_AREA or as WORK_PART_OF_CURRENT_AREA)
 */
static void
p_avg_check_and_mark_nb_pixel(GapColorMaskParams *cmaskParPtr, GimpPixelFetcher *pft, gint nx, gint ny, gint mi)
{
  guchar          pixel[4];
  gdouble         colorDiff;

  gint32 ax;
  gint32 ay;
  gint32 iTab;

  ax = nx + cmaskParPtr->seedOffsetX;
  ay = ny + cmaskParPtr->seedOffsetY;

  if (cmaskParPtr->clipAreaTable[ax][ay][mi] != WORK_UNCHECKED)
  {
     /* this neighbor pixel is already processed (nothing to be done in that case) */
     return;
  }

  gimp_pixel_fetcher_get_pixel (pft, nx, ny, &pixel[0]);

  /* check if color matches with currently processed area */
  colorDiff = p_colordiff_guchar_cmask_RGBorHSV(&cmaskParPtr->seedHsv
                    , &cmaskParPtr->seedPixel[0]
                    , &pixel[0]
                    , cmaskParPtr
                    );

  if (cmaskParPtr->debugCoordinate)
  {
    printf("ClipArea neighbor Check ax:%d ay:%d  nx:%d ny:%d  thresholdColorArea:%.4f colorDiff:%.4f\n"
      ,(int)ax
      ,(int)ay
      ,(int)nx
      ,(int)ny
      ,(float)cmaskParPtr->thresholdColorArea
      ,(float)colorDiff
      );
  }
  if (colorDiff > cmaskParPtr->thresholdColorArea)
  {
    /* currently processed pixel has other color than current seed
     * and is not member of the same color-area
     */
    cmaskParPtr->clipAreaTable[ax][ay][mi] = WORK_VISITED_BUT_NOT_PART_OF_CURRENT_AREA;
    return;
  }

  /* mark this neigbor pixel */
  cmaskParPtr->clipAreaTable[ax][ay][mi] = WORK_PART_OF_CURRENT_AREA;

  iTab = p_getColordiffTableIndex(cmaskParPtr, nx, ny);

  cmaskParPtr->pixelCount++;
  cmaskParPtr->sumColorDiff += cmaskParPtr->colordiffTable[iTab];

  p_countPerColordiff(cmaskParPtr, cmaskParPtr->colordiffTable[iTab], mi); /// #### 

  p_add_point(cmaskParPtr, nx, ny);

  if (cmaskParPtr->debugCoordinate)
  {
    printf("ClipArea Check MATCH at nx:%d ny:%d sumColorDiff:%.4f count:%d avgDiff:%.4f iTab:%d\n"
      ,(int)nx
      ,(int)ny
      ,(float)cmaskParPtr->sumColorDiff
      ,(int)cmaskParPtr->pixelCount
      ,(float)cmaskParPtr->sumColorDiff / cmaskParPtr->pixelCount
      ,(int)iTab
      );
  }

}  /* p_avg_check_and_mark_nb_pixel */


/* --------------------------------------------
 * p_find_pixel_area_of_similar_color
 * --------------------------------------------
 *
 * find pixels of same or similar color to the current processed seed pixel (cmaskParPtr->x / y)
 * and count pixels that are part of similar color area and calculate summary colordiff (dest layer versus colormask)
 * for all the pixels in the similar color area.
 * Note that the search is limited to a small clipping area that fits into clipAreaTable size
 * This is done both for performance reasons, and to limit results to local area near the current pixel.
 *
 */
static void
p_find_pixel_area_of_similar_color(GapColorMaskParams *cmaskParPtr, GimpPixelFetcher *pft, gint mi)
{
  gint32          ix;
  gint32          iy;
  gint32          ax;
  gint32          ay;
  gint32          iTab;
  GimpRGB         rgb;

  gint            clipSelX1;
  gint            clipSelX2;
  gint            clipSelY1;
  gint            clipSelY2;

  cmaskParPtr->pointCount = 0;
  cmaskParPtr->seed_ix = cmaskParPtr->x;
  cmaskParPtr->seed_iy = cmaskParPtr->y;

  cmaskParPtr->seedOffsetX = CLIP_AREA_MAX_RADIUS - cmaskParPtr->x;
  cmaskParPtr->seedOffsetY =  CLIP_AREA_MAX_RADIUS - cmaskParPtr->y;


  /* clipping area boundaries to limit area checks
   * for a small rectangular area +- CLIP_AREA_MAX_RADIUS around current pixel
   */
  clipSelX1 = MAX(cmaskParPtr->sel_x1, (cmaskParPtr->x - CLIP_AREA_MAX_RADIUS));
  clipSelY1 = MAX(cmaskParPtr->sel_y1, (cmaskParPtr->y - CLIP_AREA_MAX_RADIUS));

  clipSelX2 = MIN(cmaskParPtr->sel_x2, (cmaskParPtr->x + CLIP_AREA_MAX_RADIUS));
  clipSelY2 = MIN(cmaskParPtr->sel_y2, (cmaskParPtr->y + CLIP_AREA_MAX_RADIUS));


  /* reset area context, clear clipAreaTable */
  for(ax=0; ax < CLIP_AREA_MAX_SIZE; ax++)
  {
    for(ay=0; ay < CLIP_AREA_MAX_SIZE; ay++)
    {
      cmaskParPtr->clipAreaTable[ax][ay][mi] = WORK_UNCHECKED;
    }
  }

  /* fetch the seed pixel color */
  gimp_pixel_fetcher_get_pixel (pft, cmaskParPtr->x, cmaskParPtr->y, &cmaskParPtr->seedPixel[0]);
  gimp_rgba_set_uchar (&rgb
                      , cmaskParPtr->seedPixel[0]
                      , cmaskParPtr->seedPixel[1]
                      , cmaskParPtr->seedPixel[2]
                      , 255
                      );
  gimp_rgb_to_hsv(&rgb, &cmaskParPtr->seedHsv);


  iTab = p_getColordiffTableIndex(cmaskParPtr, cmaskParPtr->x, cmaskParPtr->y);
  cmaskParPtr->sumColorDiff = cmaskParPtr->colordiffTable[iTab];
  cmaskParPtr->pixelCount = 1;
  
  p_countPerColordiff(cmaskParPtr, cmaskParPtr->colordiffTable[iTab], mi); /// #### 
  
  
  
  
  
  cmaskParPtr->pointList = NULL;

  /* mark seed pixel */
  ax = cmaskParPtr->x + cmaskParPtr->seedOffsetX;
  ay = cmaskParPtr->y + cmaskParPtr->seedOffsetY;
  cmaskParPtr->clipAreaTable[ax][ay][mi] = WORK_PART_OF_CURRENT_AREA +1;

  if (cmaskParPtr->debugCoordinate)
  {
    printf("Seed at x:%d y:%d (ax:%d ay:%d seedOffsetX:%d Y:%d) sel_y1:%d clipSelX1:%d X2:%d Y1:%d Y2:%d  initial sumColorDiff:%.4f iTab:%d RGB: %03d %03d %03d \n"
      ,(int)cmaskParPtr->x
      ,(int)cmaskParPtr->y
      ,(int)ax
      ,(int)ay
      ,(int)cmaskParPtr->seedOffsetX
      ,(int)cmaskParPtr->seedOffsetY
      ,(int)cmaskParPtr->sel_y1
      ,(int)clipSelX1
      ,(int)clipSelX2
      ,(int)clipSelY1
      ,(int)clipSelY2
      ,(float)cmaskParPtr->sumColorDiff
      ,(int)iTab
      ,(int)cmaskParPtr->seedPixel[0]
      ,(int)cmaskParPtr->seedPixel[1]
      ,(int)cmaskParPtr->seedPixel[2]
      );
  }


  ix = cmaskParPtr->x;
  iy = cmaskParPtr->y;

  while (TRUE)
  {
    /* check of the neighbours */
    if (ix > clipSelX1)
    {
      p_avg_check_and_mark_nb_pixel(cmaskParPtr, pft, ix-1, iy, mi);
    }

    if (ix < clipSelX2)
    {
      p_avg_check_and_mark_nb_pixel(cmaskParPtr, pft, ix+1, iy, mi);
    }

    if (iy > clipSelY1)
    {
      p_avg_check_and_mark_nb_pixel(cmaskParPtr, pft, ix,   iy-1, mi);
    }

    if (iy < clipSelY2)
    {
      p_avg_check_and_mark_nb_pixel(cmaskParPtr, pft, ix,   iy+1, mi);
    }


    if (cmaskParPtr->pointList == NULL)
    {
      /* area completely done (all connected neigbour pixels of the seed are processed */
      break;
    }

    /* continue checking neigbor pixels of similar color in the point list */
    ix = cmaskParPtr->pointList->x;
    iy = cmaskParPtr->pointList->y;
    p_remove_first_point(cmaskParPtr);



  }



}  /* end p_find_pixel_area_of_similar_color */



/* ------------------------------------
 * p_calculate_clip_area_average_values
 * ------------------------------------
 *
 */
static void
p_calculate_clip_area_average_values(GapColorMaskParams *cmaskParPtr)
{
  cmaskParPtr->sumMatch[MI_IMAGE] = 0.0;
  cmaskParPtr->sumRange[MI_IMAGE] = 0.0;
  cmaskParPtr->sumNoMatch[MI_IMAGE] = 0.0;
  cmaskParPtr->countMatch[MI_IMAGE] = 0.0;
  cmaskParPtr->countRange[MI_IMAGE] = 0.0;
  cmaskParPtr->countNoMatch[MI_IMAGE] = 0.0;

  
  p_find_pixel_area_of_similar_color(cmaskParPtr, cmaskParPtr->pftDest, MI_IMAGE);

  cmaskParPtr->imgAreaSize = cmaskParPtr->pixelCount;
  cmaskParPtr->avgColordiffImgArea = cmaskParPtr->sumColorDiff / ((gdouble)cmaskParPtr->pixelCount);


  /* debug code to print the clipAreaTable and results */
  if (cmaskParPtr->debugCoordinate)
  {
    gint mi;
    gint ax;
    gint ay;
    
    for(mi=0; mi < 1; mi++)
    {
      if(mi==0)
      {
        printf("Image clipAreaTable\n");
      }
      if(mi==1)
      {
        printf("Mask clipAreaTable\n");
      }
      for(ay=0; ay < CLIP_AREA_MAX_SIZE; ay++)
      {
        printf(" clipAreaTable_%d[%02d]: ", (int)mi, (int)ay);
        for(ax=0; ax < CLIP_AREA_MAX_SIZE; ax++)
        {
          printf(" %03d", (int)cmaskParPtr->clipAreaTable[ax][ay][mi]);
        }
        printf("\n");
      }
    }

    printf("ClipArea RESULTS x:%d y:%d : avgColordiffImgArea:%.4f imgAreaSize:%d \n"
           "   avgColorThreshold:%.4f \n"
           "   countMatch:%.0f countRange:%.0f countNoMatch:%.0f\n"
      ,(int)cmaskParPtr->x
      ,(int)cmaskParPtr->y
      ,(float)cmaskParPtr->avgColordiffImgArea
      ,(int)cmaskParPtr->imgAreaSize
      ,(float)cmaskParPtr->avgColorThreshold
      ,(float)cmaskParPtr->countMatch[MI_IMAGE]
      ,(float)cmaskParPtr->countRange[MI_IMAGE]
      ,(float)cmaskParPtr->countNoMatch[MI_IMAGE]
      );

    if(cmaskParPtr->countMatch[MI_IMAGE] > 0.0)
    {
      printf("   avgMatch: %.5f\n"
         , (float)(cmaskParPtr->sumMatch[MI_IMAGE] / cmaskParPtr->countMatch[MI_IMAGE])
         );
    }
    if(cmaskParPtr->countRange[MI_IMAGE] > 0.0)
    {
      printf("   avgRange: %.5f\n"
         , (float)(cmaskParPtr->sumRange[MI_IMAGE] / cmaskParPtr->countRange[MI_IMAGE])
         );
    }
    if(cmaskParPtr->countNoMatch[MI_IMAGE] > 0.0)
    {
      printf("   avgNoMatch: %.5f\n"
         , (float)(cmaskParPtr->sumNoMatch[MI_IMAGE] / cmaskParPtr->countNoMatch[MI_IMAGE])
         );
    }

  }

}  /* end p_calculate_clip_area_average_values */




/* -----------------------------------------
 * p_check_average_colordiff_incl_neighbours
 * -----------------------------------------
 * decide if color is matching by checking neighbor pixels
 * using the configured type of average algorithm
 *
 * GAP_COLORMASK_ALGO_AVG_CHANGE_1
 *   check for significant brightness and colorchanges
 *   in 4 directions, starting at current pixel up to a distance of 
 *   the configured significantRadius.
 *   
 *   (MATCHTYPE_MATCHING)
 *     in case the check finds 2 good matching pixels (image layer versus colormask) 
 *     and there was no significant brightness or colorchange
 *     (checked image pixel versus previous image pixel in processed direction)
 *     then consider the starting pixel as good matching.
 *
 *   (MATCHTYPE_NOT_MATCHING)
 *     in case the check finds 2 NOT-matching pixels (image layer versus colormask)
 *     and there was no significant brightness or colorchange 
 *     (checked image pixel versus previous image pixel in processed direction)
 *     then consider the starting pixel as not matching.
 *
 *   (MATCHTYPE_UNDEFINED)
 *     this undefined result occurs in case the check in the current direction 
 *     o) has stopped at the configured significantRadius or image borders
 *     o) has found a significant brightness or colorchange
 *     continue with GAP_COLORMASK_ALGO_AVG_SMART in case
 *     MATCHTYPE_UNDEFINED is the result in all 4 directions.
 *     
 *
 * GAP_COLORMASK_ALGO_AVG_CHANGE_2
 *   similar to GAP_COLORMASK_ALGO_AVG_CHANGE_1
 *   but use results of all 4 directions
 *   (while variant 1 stops check at the first direction
 *   that delivers a result != MATCHTYPE_UNDEFINED)
 *
 * GAP_COLORMASK_ALGO_AVG_AREA:
 *   check if the pixel is part of an area of similar colors
 *   within the the processed image layer.
 *   in case the pixel is part of such an area then use the
 *   most significant type of colordiff (matching, NotMatching)
 *   within the area.
 *   In case colordiff type 'Range' is most significant
 *   (or number of pixels with colordiff type matching and NotMatching are equal)
 *   use the average colordiff of the area(s) or to decide.
 *
 * GAP_COLORMASK_ALGO_AVG_SMART:
 *   check average colordiff of neighbor pixels
 *   in upper, lower, left and right direction.
 *   in case the average colordiff is below the threshold
 *   return TRUE (e.g. the pixel is considered as matching good enough
 *   with the colormask)
 *   Otherwise return FALSE.
 *   This algorithm is also used as fallback strategy
 *   in case the other algortihms could not deliver a valid result.
 */
static gboolean
p_check_average_colordiff_incl_neighbours(GapColorMaskParams *cmaskParPtr)
{
  gdouble sumNbColorDiff;
  gdouble countNbPixels;
  gdouble avgColordiff[4];

  gint32 iTab;

  if ((cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_AVG_CHANGE_1)
  || (cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_AVG_CHANGE_2))
  {
    static gint dxTab[4] = { 1, -1,  0,  0 };
    static gint dyTab[4] = { 0,  0,  1, -1 };
    gint   direction;
    gint   matchCount;
    gint   notMatchCount;


    if (cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_AVG_CHANGE_1) 
    {
      /* simple varaiant based on the first defined result */
      for (direction=0; direction < 4; direction++)
      {
        guchar rc;
     
        rc = p_check_significant_changes_in_one_direction(cmaskParPtr
                   , dxTab[direction], dyTab[direction]);

        if(rc == MATCHTYPE_MATCHING)
        {
          return (TRUE);  /* good matching */
        }
      
        if(rc == MATCHTYPE_NOT_MATCHING)
        {
          return (FALSE);  /* NOT matching */
        }
      }
    }

    /* full varaiant based on all defined results */

    matchCount = 0;
    notMatchCount = 0;


    for (direction=0; direction < 4; direction++)
    {
      guchar rc;
     
      rc = p_check_significant_changes_in_one_direction(cmaskParPtr
             , dxTab[direction], dyTab[direction]);

      if(rc == MATCHTYPE_MATCHING)
      {
        matchCount++;
      }
      else if(rc == MATCHTYPE_NOT_MATCHING)
      {
        notMatchCount++;
      }
    }

    if (cmaskParPtr->debugCoordinate)
    {
          printf("SIG2 result: x:%d y:%d matchCount:%d  notMatchCount:%d\n"
            , (int)cmaskParPtr->x
            , (int)cmaskParPtr->y
            , matchCount
            , notMatchCount
            );
    }

    if(matchCount > notMatchCount)
    {
      return (TRUE);  /* good matching */
    }
    else if (notMatchCount > 0)
    {
      return (FALSE);  /* NOT matching */
    }

    /* pixel is not part of an area of similar colors (or area too small)
     * continue with GAP_COLORMASK_ALGO_AVG_SMART algorithm
     */
    if (cmaskParPtr->debugCoordinate)
    {
      printf("continue with standard AVG_SMART algorithm: ");
    }
  }



  //iTab = p_getColordiffTableIndex(cmaskParPtr, cmaskParPtr->x, cmaskParPtr->y);

  //if ((cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_AVG_AREA)
  //&& (cmaskParPtr->colordiffTable[iTab] >= cmaskParPtr->avgColorThreshold))

  if (cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_AVG_AREA)
  {
    /* handle GAP_COLORMASK_ALGO_AVG_AREA algorithm */

    p_calculate_clip_area_average_values(cmaskParPtr);


    if (cmaskParPtr->imgAreaSize >= cmaskParPtr->areaMinimumPixels)
    {
      if((cmaskParPtr->countMatch[MI_IMAGE] > cmaskParPtr->countRange[MI_IMAGE])
      && (cmaskParPtr->countMatch[MI_IMAGE] > cmaskParPtr->countNoMatch[MI_IMAGE]))
      {
        /* most pixels of the similar colored area are good matching */
        if (cmaskParPtr->debugCoordinate)
        {
          printf("MATCHING is most significant\n");
        }
        return (TRUE); /* good matching */
      }

      if((cmaskParPtr->countNoMatch[MI_IMAGE] > cmaskParPtr->countRange[MI_IMAGE])
      && (cmaskParPtr->countNoMatch[MI_IMAGE] > cmaskParPtr->countMatch[MI_IMAGE]))
      {
        /* most pixels of the similar colored area are NOT matching */
        if (cmaskParPtr->debugCoordinate)
        {
          printf("NOT_MATCHING is most significant\n");
        }
        return (FALSE); /* not matching */
      }
      
      /* most pixels of the similar colored area within range or 
       * there are exactly the same number of good and non matching pixels within this area.
       * therfore next check the overall average colordiff of the area.
       */
      if (cmaskParPtr->avgColordiffImgArea < cmaskParPtr->avgColorThreshold)
      {
        if (cmaskParPtr->debugCoordinate)
        {
          printf("MATCHING due to overall avgColordiffImgArea:%.5f avgColorThreshold:%.5f\n"
              , (float)cmaskParPtr->avgColordiffImgArea
              , (float)cmaskParPtr->avgColorThreshold
              );
        }
        return (TRUE); /* good matching */
      }
      if (cmaskParPtr->avgColordiffImgArea >= cmaskParPtr->hiColorThreshold)
      {
        if (cmaskParPtr->debugCoordinate)
        {
          printf("NOT_MATCHING due to overall avgColordiffImgArea:%.5f avgColorThreshold:%.5f\n"
              , (float)cmaskParPtr->avgColordiffImgArea
              , (float)cmaskParPtr->avgColorThreshold
              );
        }
        return (FALSE); /* not matching */
      }
      
      if (cmaskParPtr->countRange[MI_IMAGE] > 1)
      {
        gdouble avgRangeColordiff;
        gdouble avgMixColordiff;

        /* check average colordiff limited to just those similar color area pixels
         * that match within range
         */
        avgRangeColordiff = cmaskParPtr->sumRange[MI_IMAGE] / cmaskParPtr->countRange[MI_IMAGE];
        
        if (avgRangeColordiff < cmaskParPtr->avgColorThreshold)
        {
          if (cmaskParPtr->debugCoordinate)
          {
            printf("MATCHING due to range avgRangeColordiff:%.5f avgColorThreshold:%.5f\n"
                , (float)avgRangeColordiff
                , (float)cmaskParPtr->avgColorThreshold
                );
          }
          return (TRUE); /* good matching */
        }
        if (avgRangeColordiff >= cmaskParPtr->hiColorThreshold)
        {
          if (cmaskParPtr->debugCoordinate)
          {
            printf("NOT_MATCHING due to range avgRangeColordiff:%.5f avgColorThreshold:%.5f\n"
                , (float)avgRangeColordiff
                , (float)cmaskParPtr->avgColorThreshold
                );
          }
          return (FALSE); /* not matching */
        }
        
        if(cmaskParPtr->countMatch[MI_IMAGE] >= cmaskParPtr->countNoMatch[MI_IMAGE])
        {
          avgMixColordiff = (cmaskParPtr->sumRange[MI_IMAGE] + cmaskParPtr->sumMatch[MI_IMAGE])
                          / (cmaskParPtr->countRange[MI_IMAGE] + cmaskParPtr->countMatch[MI_IMAGE]);
          if (avgMixColordiff < cmaskParPtr->avgColorThreshold)
          {
            if (cmaskParPtr->debugCoordinate)
            {
              printf("MATCHING due to mix range+match avgMixColordiff:%.5f avgColorThreshold:%.5f\n"
                  , (float)avgMixColordiff
                  , (float)cmaskParPtr->avgColorThreshold
                  );
            }
            return (TRUE); /* good matching */
          }
        }
        
        if(cmaskParPtr->countNoMatch[MI_IMAGE] >= cmaskParPtr->countMatch[MI_IMAGE])
        {
          avgMixColordiff = (cmaskParPtr->sumRange[MI_IMAGE] + cmaskParPtr->sumNoMatch[MI_IMAGE])
                          / (cmaskParPtr->countRange[MI_IMAGE] + cmaskParPtr->countNoMatch[MI_IMAGE]);
          if (avgMixColordiff >= cmaskParPtr->hiColorThreshold)
          {
            if (cmaskParPtr->debugCoordinate)
            {
              printf("NOT MATCHING due to mix range+NoMatch avgMixColordiff:%.5f avgColorThreshold:%.5f\n"
                  , (float)avgMixColordiff
                  , (float)cmaskParPtr->avgColorThreshold
                  );
            }
            return (FALSE); /* not matching */
          }
        }
        
        
        
        
        
      }
     
      if(cmaskParPtr->countNoMatch[MI_IMAGE] > cmaskParPtr->countMatch[MI_IMAGE])
      {
        if (cmaskParPtr->debugCoordinate)
        {
          printf("NOT MATCHING more NoMatch pixels than Match pixels\n");
        }
        return (FALSE); /* not matching */
      }
      if(cmaskParPtr->countMatch[MI_IMAGE] > cmaskParPtr->countNoMatch[MI_IMAGE])
      {
        if (cmaskParPtr->debugCoordinate)
        {
          printf("MATCHING more Match pixels than NoMatch pixels\n");
        }
        return (TRUE); /* matching */
      }

    }


    /* pixel is not part of an area of similar colors (or area too small)
     * continue with GAP_COLORMASK_ALGO_AVG_SMART algorithm
     */
    if (cmaskParPtr->debugCoordinate)
    {
      printf("AREA algorithm could not decide, continue with AVG_SMART algorithm: ");
    }

  }

  /* following code handles the algorithm
   * GAP_COLORMASK_ALGO_AVG_SMART and
   * GAP_COLORMASK_ALGO_AVG_AREA (part2 that is ident to the smart algorithm )
   */

  p_check_range_neighbour_sum(cmaskParPtr
                             , 0   /* dx */
                             , 1   /* dy */
                             , cmaskParPtr->checkRadius
                             , &sumNbColorDiff, &countNbPixels);
  avgColordiff[0] = sumNbColorDiff / countNbPixels;
  if ((avgColordiff[0] < cmaskParPtr->avgColorThreshold) && (countNbPixels > 2))
  {
    if (cmaskParPtr->debugCoordinate)
    {
      printf("x:%d y:%d countNbPixels:%.1f avgColordiff:[%.4f] is below Thres:%.4f\n"
          ,(int)cmaskParPtr->x
          ,(int)cmaskParPtr->y
          ,(float)countNbPixels
          ,(float)avgColordiff[0]
          ,(float)cmaskParPtr->avgColorThreshold
          );
    }
    /* average matches good */
    return (TRUE);
  }

  p_check_range_neighbour_sum(cmaskParPtr
                             , 0   /* dx */
                             ,-1   /* dy */
                             , cmaskParPtr->checkRadius
                             , &sumNbColorDiff, &countNbPixels);
  avgColordiff[1] = sumNbColorDiff / countNbPixels;
  if ((avgColordiff[1] < cmaskParPtr->avgColorThreshold) && (countNbPixels > 2))
  {
    if (cmaskParPtr->debugCoordinate)
    {
      printf("x:%d y:%d countNbPixels:%.1f avgColordiff:[%.4f] is below Thres:%.4f\n"
          ,(int)cmaskParPtr->x
          ,(int)cmaskParPtr->y
          ,(float)countNbPixels
          ,(float)avgColordiff[1]
          ,(float)cmaskParPtr->avgColorThreshold
          );
    }
    /* average matches good */
    return (TRUE);
  }



  /* horizontal */

  p_check_range_neighbour_sum(cmaskParPtr
                             , 1   /* dx */
                             , 0   /* dy */
                             , cmaskParPtr->checkRadius
                             , &sumNbColorDiff, &countNbPixels);
  avgColordiff[2] = sumNbColorDiff / countNbPixels;
  if ((avgColordiff[2] < cmaskParPtr->avgColorThreshold) && (countNbPixels > 2))
  {
    if (cmaskParPtr->debugCoordinate)
    {
      printf("x:%d y:%d countNbPixels:%.1f avgColordiff:[%.4f] is below Thres:%.4f\n"
          ,(int)cmaskParPtr->x
          ,(int)cmaskParPtr->y
          ,(float)countNbPixels
          ,(float)avgColordiff[2]
          ,(float)cmaskParPtr->avgColorThreshold
          );
    }
    /* average matches good */
    return (TRUE);
  }


  p_check_range_neighbour_sum(cmaskParPtr
                             ,-1   /* dx */
                             , 0   /* dy */
                             , cmaskParPtr->checkRadius
                             , &sumNbColorDiff, &countNbPixels);
  avgColordiff[3] = sumNbColorDiff / countNbPixels;
  if ((avgColordiff[3] < cmaskParPtr->avgColorThreshold) && (countNbPixels > 2))
  {
    if (cmaskParPtr->debugCoordinate)
    {
      printf("x:%d y:%d countNbPixels:%.1f avgColordiff:[%.4f] is below Thres:%.4f\n"
          ,(int)cmaskParPtr->x
          ,(int)cmaskParPtr->y
          ,(float)countNbPixels
          ,(float)avgColordiff[3]
          ,(float)cmaskParPtr->avgColorThreshold
          );
    }
    /* average matches good */
    return (TRUE);
  }



  if (cmaskParPtr->debugCoordinate)
  {
    printf("x:%d y:%d avgColordiff:[%.4f %.4f %.4f %.4f] at least one is above Thres:%.4f\n"
        ,(int)cmaskParPtr->x
        ,(int)cmaskParPtr->y
        ,(float)avgColordiff[0]
        ,(float)avgColordiff[1]
        ,(float)avgColordiff[2]
        ,(float)avgColordiff[3]
        ,(float)cmaskParPtr->avgColorThreshold
        );
  }

  return (FALSE);
}  /* end p_check_average_colordiff_incl_neighbours */






   
/* ---------------------------------
 * p_check_significant_diff
 * ---------------------------------
 * returns TRUE in case the specified colors differ significant
 *              (according to curent parameter settings)
 */
static gboolean
p_check_significant_diff(guchar *aPixelPtr
                   , guchar *bPixelPtr
		   , gint xx, gint yy
                   , GapColorMaskParams *cmaskParPtr)
{
  gdouble nbColorDiff;
  gdouble nbBrightnessDiff;
  gdouble lum1;
  gdouble lum2;
                    
  lum1 = (gdouble)(aPixelPtr[0] + aPixelPtr[1] + aPixelPtr[2]) / 765.0;  // (255 * 3);
  lum2 = (gdouble)(bPixelPtr[0] + bPixelPtr[1] + bPixelPtr[2]) / 765.0;  // (255 * 3);
  
  nbBrightnessDiff = fabs(lum1 - lum2);
  if(nbBrightnessDiff > cmaskParPtr->significantBrightnessDiff)
  {
    if (cmaskParPtr->debugCoordinate)
    {
        printf("SIG Brightnessdiff x:%d y:%d  xx:%d yy:%d lum1:%.3f lum2:%.3f nbBrightnessDiff:%.4f  significantBrightnessDiff:%.4f\n"
          , (int)cmaskParPtr->x
          , (int)cmaskParPtr->y
	  , (int)xx
	  , (int)yy
	  , (float)lum1
	  , (float)lum2
          , (float)nbBrightnessDiff
          , (float)cmaskParPtr->significantBrightnessDiff
          );
    }
  
    return (TRUE);
  }
  
  nbColorDiff = p_colordiff_guchar_cmask(aPixelPtr
                             , bPixelPtr
                             , cmaskParPtr
                             );

  if(nbColorDiff > cmaskParPtr->significantColordiff)
  {
    if (cmaskParPtr->debugCoordinate)
    {
        printf("SIG Colordiff x:%d y:%d nbColorDiff:%.4f  significantColordiff:%.4f\n"
          , (int)cmaskParPtr->x
          , (int)cmaskParPtr->y
          , (float)nbColorDiff
          , (float)cmaskParPtr->significantColordiff
          );
    }

    return (TRUE);
  }

  if (cmaskParPtr->debugCoordinate)
  {
    printf("SIG similar color x:%d y:%d  xx:%d yy:%d lum1:%.3f lum2:%.3f nbBrightnessDiff:%.4f  significantBrightnessDiff:%.4f"
           "  nbColorDiff:%.4f  significantColordiff:%.4f\n"
          , (int)cmaskParPtr->x
          , (int)cmaskParPtr->y
	  , (int)xx
	  , (int)yy
	  , (float)lum1
	  , (float)lum2
          , (float)nbBrightnessDiff
          , (float)cmaskParPtr->significantBrightnessDiff
          , (float)nbColorDiff
          , (float)cmaskParPtr->significantColordiff
          );
  }
  

  return (FALSE);
}  /* end p_check_significant_diff */



/* --------------------------------------------
 * p_check_significant_changes_in_one_direction
 * --------------------------------------------
 * returns TRUE in case the specified colors differ significant
 *              (according to curent parameter settings)
 */
static guchar
p_check_significant_changes_in_one_direction(GapColorMaskParams *cmaskParPtr, gint dx, gint dy)
{
  guchar  currPixel[4];
  guchar  prevPixel[4];
  gint    distance;
  gdouble prevColordiff;
  
  
  gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDest, cmaskParPtr->x, cmaskParPtr->y, &prevPixel[0]);
  prevColordiff = 0.0;

  for(distance = 1; distance < cmaskParPtr->significantRadius; distance++)
  {
    gboolean significantDiff;
    gdouble  currColordiff;
    gint     xx;
    gint     yy;
    gint32   iTab;

    xx = cmaskParPtr->x + (distance * dx);
    yy = cmaskParPtr->y + (distance * dy);

    
    /* clipping check test position outside image boudaries */
    if ((xx < 0) || (yy < 0) || (xx >= cmaskParPtr->width) || (yy >= cmaskParPtr->height))
    {
      return (MATCHTYPE_UNDEFINED);
    }

    gimp_pixel_fetcher_get_pixel (cmaskParPtr->pftDest, xx, yy, &currPixel[0]);
    
    significantDiff = p_check_significant_diff(&prevPixel[0]
                             , &currPixel[0]
			     , xx
			     , yy
                             , cmaskParPtr
                             );
    if(significantDiff)
    {
      return (MATCHTYPE_UNDEFINED);
    }


    iTab = p_getColordiffTableIndex(cmaskParPtr, xx, yy);
    currColordiff = cmaskParPtr->colordiffTable[iTab];
    
    if (distance > 1)
    {
      if ((currColordiff > cmaskParPtr->hiColorThreshold)
      &&  (prevColordiff > cmaskParPtr->hiColorThreshold))
      {
        /* found "NN" pattern (2 Non matching pixels in series)  */
        return (MATCHTYPE_NOT_MATCHING);
      }

   
   
//       if ((currColordiff < cmaskParPtr->colorThreshold)
//       &&  (prevColordiff < cmaskParPtr->colorThreshold))
//       {
//         /* found "mm" pattern (2 matching pixels in series)  */
//         return (MATCHTYPE_MATCHING);
//       }
    }
    
    prevPixel[0] = currPixel[0];
    prevPixel[1] = currPixel[1];
    prevPixel[2] = currPixel[2];
    
    prevColordiff = currColordiff;
  }

  return (MATCHTYPE_UNDEFINED);
  
}  /* end p_check_significant_changes_in_one_direction */










/* ---------------------------------
 * p_check_avg_add_one_pixel             DEPRECATED
 * ---------------------------------
 *
 */
static inline void
p_check_avg_add_one_pixel(GapColorMaskParams *cmaskParPtr, gint xx, gint yy
   , gdouble *sumNbColorDiff, gdouble *countNbPixels)
{
  gint iTab;

  /* clipping check */
  if ((xx < 0) || (yy < 0) || (xx >= cmaskParPtr->width) || (yy >= cmaskParPtr->height))
  {
    return;
  }

  iTab = p_getColordiffTableIndex(cmaskParPtr, xx, yy);
  *sumNbColorDiff += cmaskParPtr->colordiffTable[iTab];
  *countNbPixels  += 1.0;

  if (cmaskParPtr->debugCoordinate)
  {
    printf("AVCHK xx:%d yy:%d countNbPixels:%.1f colordiffTab:%.4f\n"
      , (int)xx
      , (int)yy
      , *countNbPixels
      , (float)cmaskParPtr->colordiffTable[iTab]
      );
  }

}  /* end p_check_avg_add_one_pixel */




/* ---------------------------------
 * p_init_colordiffTable
 * ---------------------------------
 * initialize colordiffTable at full image size
 * with calculate color differences for all pixels.
 * (this table will not work for huge images on machines
 * with limited memory resouces but allows fast
 * processing by avoiding multiple colordiff processing
 * and pixel fetch steps and shall do it for
 * typical video or HD video image sizes.
 */
static void
p_init_colordiffTable (const GimpPixelRgn *maskPR
                    ,const GimpPixelRgn *destPR
                    ,GapColorMaskParams *cmaskParPtr)
{
  guint    row;
  guchar* mask = maskPR->data;   /* the colormask */
  guchar* dest = destPR->data;   /* the destination drawable */


  for (row = 0; row < destPR->h; row++)
  {
    guint  col;
    guint  idxMask;
    guint  idxDest;

    idxMask = 0;
    idxDest = 0;
    for(col = 0; col < destPR->w; col++)
    {
      gint32 iTab;

      cmaskParPtr->x = destPR->x + col;
      cmaskParPtr->y = destPR->y + row;
      iTab = p_getColordiffTableIndex(cmaskParPtr, cmaskParPtr->x, cmaskParPtr->y);


      {
        cmaskParPtr->colordiffTable[iTab] =
            p_colordiff_guchar_cmask( &mask[idxMask]
                             , &dest[idxDest]
                             , cmaskParPtr
                             );

      }



      idxDest += destPR->bpp;
      idxMask += maskPR->bpp;
    }

    mask += maskPR->rowstride;
    dest += destPR->rowstride;

    if(cmaskParPtr->doProgress)
    {
      p_handle_progress(cmaskParPtr, maskPR->w, "p_init_colordiffTable");
    }
  }

}  /* end p_init_colordiffTable */


/* ---------------------------------
 * p_getColordiffTableIndex
 * ---------------------------------
 */
static inline gint32
p_getColordiffTableIndex(GapColorMaskParams *cmaskParPtr, gint x, gint y)
{
  gint32 iTab;

  iTab = (y * cmaskParPtr->width) + x;
  return (iTab);
}


/* --------------------------------------------
 * p_difflayer_rgn_render
 * --------------------------------------------
 * render difflayer to visualize
 * the colordiffTable in shades of green (MATCHING pixels) yellow (tolrated RANGE) and red. (bad matching RANGE pixels)
 * or transparent (NON-MATCHING pixels)
 * The difflayer is not required for productive processing,
 * this layer is for debug and tuning purpose while development.
 */
static void
p_difflayer_rgn_render (const GimpPixelRgn *diffPR
                    , const GimpPixelRgn *destPR, GapColorMaskParams *cmaskParPtr)
{
  guint    row;
  guchar* diff = diffPR->data;
  guchar* dest = destPR->data;


  for (row = 0; row < diffPR->h; row++)
  {
    guint  col;
    guint  idxDiff;
    guint  idxDest;

    idxDiff = 0;
    idxDest = 0;
    for(col = 0; col < diffPR->w; col++)
    {
      gint32 iTab;
      guchar red, green, blue, alpha;

      cmaskParPtr->x = diffPR->x + col;
      cmaskParPtr->y = diffPR->y + row;
      iTab = p_getColordiffTableIndex(cmaskParPtr, cmaskParPtr->x, cmaskParPtr->y);

      red   = 0;
      green = 0;
      blue  = 0;
      alpha = 255;

      if (cmaskParPtr->colordiffTable[iTab] >= cmaskParPtr->hiColorThreshold)
      {
        /* NOMATCH pixel color differs significant from colormask pixelcolor
         * render those pixels fully transparent.
         */
        alpha = 0;
      }
      else
      {
        gdouble intensity;

        intensity = 1.0;
        
        if(cmaskParPtr->connectByCorner)   //// DEBUG
        {
          p_set_dynamic_threshold_by_KeyColor(&dest[idxDest], cmaskParPtr);
        }

        if (cmaskParPtr->colordiffTable[iTab] < cmaskParPtr->colorThreshold)
        {
          /* MATCH pixel color equal or nearly equal to colormask pixelcolor
           * render those pixels in shades of green
           */
          if (cmaskParPtr->colorThreshold != 0)
          {
            intensity = cmaskParPtr->colordiffTable[iTab] / cmaskParPtr->colorThreshold;
          }
          intensity *= 127.0;
          green = 127 + intensity;
        }
        else if (cmaskParPtr->colordiffTable[iTab] <= cmaskParPtr->avgColorThreshold)
        {
          gdouble loThres, hiThres;

          loThres = MIN(cmaskParPtr->colorThreshold , cmaskParPtr->avgColorThreshold);
          hiThres = MAX(cmaskParPtr->colorThreshold , cmaskParPtr->avgColorThreshold);

          /* RANGE pixel color falls in the range of matching colors (below the tolerated avgColorThreshold level)
           * render those pixels in shades of blue
           */
          if (hiThres - loThres)
          {
            intensity = (cmaskParPtr->colordiffTable[iTab] - loThres)
                      / (hiThres - loThres);
          }
          intensity *= 127.0;
          blue = 127 + intensity;
        }
        else
        {
          gdouble loThres, hiThres;

          loThres = MIN(cmaskParPtr->avgColorThreshold , cmaskParPtr->hiColorThreshold);
          hiThres = MAX(cmaskParPtr->avgColorThreshold , cmaskParPtr->hiColorThreshold);

          /* RANGE pixel color falls in the range of similar but not good matching colors
           * render those pixels in shades of red
           */
          if (hiThres - loThres)
          {
            intensity = (cmaskParPtr->colordiffTable[iTab] - loThres)
                      / (hiThres - loThres);
          }
          intensity *= 127.0;
          red = 127 + intensity;
        }
      }


      diff[idxDiff]    = red;
      diff[idxDiff +1] = green;
      diff[idxDiff +2] = blue;
      diff[idxDiff +3] = alpha;


      idxDiff += diffPR->bpp;
      idxDest += destPR->bpp;
    }

    diff += diffPR->rowstride;
    dest += destPR->rowstride;

    if(cmaskParPtr->doProgress)
    {
      p_handle_progress(cmaskParPtr, diffPR->w, "p_difflayer_rgn_render");
    }
  }

}  /* end p_difflayer_rgn_render */


/* --------------------------------------------
 * p_create_colordiffTable_Layer
 * --------------------------------------------
 * create difflayer to visualize the colordiffTable
 * The difflayer is not required for productive processing,
 * this layer is for debug and tuning purpose while development.
 */
static void
p_create_colordiffTable_Layer(GapColorMaskParams *cmaskParPtr, GimpDrawable *dst_drawable)
{
  GimpDrawable *diffLayerDrawable;
  gpointer  pr;
  GimpPixelRgn diffPR;
  GimpPixelRgn destPR;
  gint32 diffLayerId;

  diffLayerId = gap_layer_find_by_name(cmaskParPtr->dst_image_id, COLORMASK_DIFF_LAYER_NAME);
  if (diffLayerId < 0)
  {
    diffLayerId = p_create_empty_layer(cmaskParPtr->dst_image_id
                       , cmaskParPtr->width
                       , cmaskParPtr->height
                       , COLORMASK_DIFF_LAYER_NAME);
  }

  if (diffLayerId < 0)
  {
    return;
  }


  diffLayerDrawable = gimp_drawable_get(diffLayerId);

  gimp_pixel_rgn_init (&diffPR, diffLayerDrawable, 0, 0
                      , diffLayerDrawable->width, diffLayerDrawable->height
                      , TRUE     /* dirty */
                      , FALSE     /* shadow */
                       );

  gimp_pixel_rgn_init (&destPR, dst_drawable, 0, 0
                      , dst_drawable->width, dst_drawable->height
                      , TRUE      /* dirty */
                      , FALSE     /* shadow */
                       );



  /* init the the worklayer (copy content and setup alpha channel) */
  for (pr = gimp_pixel_rgns_register (2, &diffPR, &destPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
    p_difflayer_rgn_render (&diffPR, &destPR, cmaskParPtr);
  }

  gimp_drawable_detach(diffLayerDrawable);


}  /* end p_create_colordiffTable_Layer */




/* -------------------------------------------------------------------- */
/* ------------------ stuff for average algorithm END      ------------ */
/* -------------------------------------------------------------------- */

/* ============================ Stuff for average algorithm END ======= */




/* ============================ Stuff for create or replace individual per frame colormask layer ======= */

/* --------------------------------------------
 * p_mix_newmask_rgn_render
 * --------------------------------------------
 * create individual (per frame) colormask in the work drawable.
 * as mix of the previous colormask the initial reference colormask and the original layer
 */
static void
p_mix_newmask_rgn_render(const GimpPixelRgn *pmskPR
                 ,const GimpPixelRgn *maskPR
                 ,const GimpPixelRgn *origPR
                 ,const GimpPixelRgn *workPR
                 ,GapColorMaskParams *cmaskParPtr)
{
  guint    row;
  guchar*  pmsk = pmskPR->data;   /* the colormask from previous frame  */
  guchar*  mask = maskPR->data;   /* the initial reference colormask */
  guchar*  orig = origPR->data;   /* the current frame original */
  guchar*  work = workPR->data;   /* the destination drawable (e.g. new colormask) */


  for (row = 0; row < workPR->h; row++)
  {
    guint  col;
    guint  idxPmsk;
    guint  idxMask;
    guint  idxOrig;
    guint  idxWork;

    idxPmsk = 0;
    idxMask = 0;
    idxOrig = 0;
    idxWork = 0;
    for(col = 0; col < workPR->w; col++)
    {
      gboolean useNewIndividualPixel;
      gboolean usePrevMaskPixel;
      gboolean isProtected;

      //if(gap_debug)
      {
        cmaskParPtr->x = workPR->x + col;
        cmaskParPtr->y = workPR->y + row;
        cmaskParPtr->debugCoordinate = p_is_debug_active_on_coordinate(cmaskParPtr);
      }


      useNewIndividualPixel = FALSE;
      usePrevMaskPixel = FALSE;
      isProtected = FALSE;
      if (maskPR->bpp > 3)
      {
        if (mask[idxMask + 3] <= cmaskParPtr->triggerAlpha255)
        {
          isProtected = TRUE;
        }
      }


      if (!isProtected)
      {
        gdouble colorDiff;

        colorDiff = p_colordiff_guchar_cmask(&mask[idxMask]
                 , &orig[idxOrig]
                 , cmaskParPtr);

        if(colorDiff < cmaskParPtr->hiColorThreshold)
        {
          colorDiff = p_colordiff_guchar_cmask(&pmsk[idxPmsk]
                 , &orig[idxOrig]
                 , cmaskParPtr);
          if(colorDiff < cmaskParPtr->colorThreshold)
          {
            useNewIndividualPixel = TRUE;
          }
          else if(colorDiff < cmaskParPtr->edgeColorThreshold)
          {
            usePrevMaskPixel = TRUE;
          }
        }
        else if(colorDiff < cmaskParPtr->colorThreshold)
        {
            useNewIndividualPixel = TRUE;
        }
      }

      if(useNewIndividualPixel)
      {
        work[idxWork]    = orig[idxOrig];
        work[idxWork +1] = orig[idxOrig +1];
        work[idxWork +2] = orig[idxOrig +2];
      }
      else
      {
        if (usePrevMaskPixel)
        {
          /* for this pixel copy from previous colormask rgb values */
          work[idxWork]    = pmsk[idxPmsk];
          work[idxWork +1] = pmsk[idxPmsk +1];
          work[idxWork +2] = pmsk[idxPmsk +2];
        }
        else
        {
          /* for this pixel copy from initial colormask rgb values */
          work[idxWork]    = mask[idxMask];
          work[idxWork +1] = mask[idxMask +1];
          work[idxWork +2] = mask[idxMask +2];
        }
      }

      if (maskPR->bpp > 3)
      {
        work[idxWork +3] = mask[idxMask + 3];  /* copy alpha channel from original colormask */
      }
      else
      {
        work[idxWork +3] = 255;
      }


      idxPmsk += pmskPR->bpp;
      idxMask += maskPR->bpp;
      idxOrig += origPR->bpp;
      idxWork += workPR->bpp;
    }

    pmsk += pmskPR->rowstride;
    mask += maskPR->rowstride;
    orig += origPR->rowstride;
    work += workPR->rowstride;

    if(cmaskParPtr->doProgress)
    {
      p_handle_progress(cmaskParPtr, maskPR->w, "p_mix_newmask_rgn_render");
    }
  }

}  /* end p_mix_newmask_rgn_render */


/* --------------------------------------------
 * p_fetch_colormask_in_previous_frame
 * --------------------------------------------
 */
static gint32
p_fetch_colormask_in_previous_frame(gint32 orig_layer_id)
{
  gint32 image_id;
  gint32 prevCmskId;
  GapAnimInfo *ainfo_ptr;

  prevCmskId = -1;
  image_id = gimp_drawable_get_image(orig_layer_id);
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, GIMP_RUN_NONINTERACTIVE);
  if(ainfo_ptr != NULL)
  {
    char *prevFrameName;

    ainfo_ptr->frame_nr = ainfo_ptr->curr_frame_nr - 1;

    prevFrameName = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      ainfo_ptr->frame_nr,
                                      ainfo_ptr->extension);
    if(0 == gap_lib_file_exists(prevFrameName))
    {
      //if(gap_debug)
      {
        printf("previous frame %s does not exist (or is empty)\n"
              , prevFrameName
              );
      }
      prevCmskId = -1;
    }
    else
    {
       gint32 image_id;

       image_id = gap_lib_load_image(prevFrameName);
       if (image_id >= 0)
       {
         prevCmskId = gap_layer_find_by_name(image_id, COLORMASK_LAYER_NAME);
       }

    }

    gap_lib_free_ainfo(&ainfo_ptr);
  }

  return(prevCmskId);

}  /* end p_fetch_colormask_in_previous_frame */


/* -----------------------------------------
 * p_remove_layer_by_name
 * -----------------------------------------
 * remove layer from the specified image_id that has
 * the specified name, AND has a layer_id that is different from orig_layer_id
 */
static void
p_remove_layer_by_name(gint32 image_id, gint32 orig_layer_id, const char *name)
{
  gint32 layer_id;

  layer_id = gap_layer_find_by_name(image_id, name);
  if(layer_id < 0)
  {
    return;
  }
  if (layer_id != orig_layer_id)
  {
    gimp_image_remove_layer(image_id, layer_id);
  }
}  /* end p_remove_layer_by_name */

/* ============================ Stuff for create or replace individual perf frame colormask layer ======= */


/* ------------------------------------
 * p_gdouble_to_uchar255
 * ------------------------------------
 * convert from range 0.0 .. 1.0   to 0 .. 255
 */
static guchar
p_gdouble_to_uchar255(gdouble value)
{
  gdouble   value255;
  guchar    ret;

  value255 = value * 255.0;
  ret = CLAMP((guchar)value255, 0, 255);

  if(gap_debug)
  {
    printf("value:%f val255:%f  ret:%d\n"
      , (float)value
      , (float)value255
      , (int)ret
      );
  }
  return (ret);
}

/* -----------------------------------------
 * p_copy_cmaskvals_to_colorMaskParams
 * -----------------------------------------
 */
static void
p_copy_cmaskvals_to_colorMaskParams(GapColormaskValues *cmaskvals, GapColorMaskParams *cmaskParPtr, gint32 dst_layer_id, gboolean doProgress)
{
  //if(gap_debug)
  {
    printf("algorithm: %d\n", (int)cmaskvals->algorithm);
    printf("colormask_id: %d\n", (int)cmaskvals->colormask_id);
    printf("loColorThreshold: %f\n", (float)cmaskvals->loColorThreshold);
    printf("hiColorThreshold: %f\n", (float)cmaskvals->hiColorThreshold);
    printf("colorSensitivity: %f\n", (float)cmaskvals->colorSensitivity);
    printf("lowerOpacity: %f\n", (float)cmaskvals->lowerOpacity);
    printf("upperOpacity: %f\n", (float)cmaskvals->upperOpacity);
    printf("triggerAlpha: %f\n", (float)cmaskvals->triggerAlpha);
    printf("isleRadius: %f\n", (float)cmaskvals->isleRadius);
    printf("isleAreaPixelLimit: %f\n", (float)cmaskvals->isleAreaPixelLimit);
    printf("featherRadius: %f\n", (float)cmaskvals->featherRadius);

    printf("edgeColorThreshold: %f\n", (float)cmaskvals->edgeColorThreshold);
    printf("thresholdColorArea: %f\n", (float)cmaskvals->thresholdColorArea);
    printf("pixelDiagonal: %f\n", (float)cmaskvals->pixelDiagonal);
    printf("pixelAreaLimit: %f\n", (float)cmaskvals->pixelAreaLimit);
    printf("connectByCorner: %d\n", (int)cmaskvals->connectByCorner);
    printf("keepLayerMask: %d\n", (int)cmaskvals->keepLayerMask);
    printf("keepWorklayer: %d\n", (int)cmaskvals->keepWorklayer);
    printf("enableKeyColorThreshold: %d\n", (int)cmaskvals->enableKeyColorThreshold);
    printf("loKeyColorThreshold: %f\n", (float)cmaskvals->loKeyColorThreshold);
    printf("keyColorSensitivity: %f\n", (float)cmaskvals->keyColorSensitivity);

    printf("jaggedRadius: %f\n", (float)cmaskvals->pixelDiagonal);   /// TODO
    printf("jeggedLength: %f\n", (float)cmaskvals->pixelAreaLimit);   /// TODO

    printf("avgFactor: %f\n", (float)cmaskvals->edgeColorThreshold);      /// TODO
    printf("checkRadius: %f\n", (float)cmaskvals->pixelDiagonal);         /// TODO
    printf("checkMatchRadius: %f\n", (float)cmaskvals->pixelDiagonal);    /// TODO
    printf("areaMinimumPixels: %f\n", (float)cmaskvals->pixelAreaLimit);  /// TODO
  }


  cmaskParPtr->algorithm = cmaskvals->algorithm;
  cmaskParPtr->dst_layer_id = dst_layer_id;
  cmaskParPtr->dst_image_id = gimp_drawable_get_image(dst_layer_id);
  cmaskParPtr->cmask_drawable_id = cmaskvals->colormask_id;
  cmaskParPtr->colorThreshold = cmaskvals->loColorThreshold;    /* use loColorThreshold as default for  colorThreshold */
  cmaskParPtr->loColorThreshold = cmaskvals->loColorThreshold;
  cmaskParPtr->hiColorThreshold = cmaskvals->hiColorThreshold;
  cmaskParPtr->colorSensitivity = CLAMP(cmaskvals->colorSensitivity, 1.0, 2.0);
  
  cmaskParPtr->enableKeyColorThreshold = cmaskvals->enableKeyColorThreshold;
  cmaskParPtr->loKeyColorThreshold = cmaskvals->loKeyColorThreshold;
  cmaskParPtr->keyColorSensitivity = cmaskvals->keyColorSensitivity;
  gimp_rgb_to_hsv(&cmaskvals->keycolor, &cmaskParPtr->keyColorHsv);
  

  cmaskParPtr->lowerOpacity = cmaskvals->lowerOpacity;
  cmaskParPtr->upperOpacity = cmaskvals->upperOpacity;
  cmaskParPtr->triggerAlpha = cmaskvals->triggerAlpha;
  cmaskParPtr->isleRadius = cmaskvals->isleRadius;
  cmaskParPtr->isleAreaPixelLimit = cmaskvals->isleAreaPixelLimit;
  cmaskParPtr->featherRadius = cmaskvals->featherRadius;

  cmaskParPtr->edgeColorThreshold = cmaskParPtr->edgeColorThreshold;
  cmaskParPtr->thresholdColorArea = cmaskvals->thresholdColorArea;
  cmaskParPtr->pixelDiagonal = cmaskvals->pixelDiagonal;
  cmaskParPtr->pixelAreaLimit = cmaskvals->pixelAreaLimit;
  cmaskParPtr->connectByCorner = cmaskvals->connectByCorner;
  cmaskParPtr->keepLayerMask = cmaskvals->keepLayerMask;
  cmaskParPtr->keepWorklayer = cmaskvals->keepWorklayer;
  cmaskParPtr->doProgress = doProgress;


  /* precalculate values for per-pixel calculations (performance) */
  cmaskParPtr->dstLayerMaskId = -1;
  cmaskParPtr->triggerAlpha255 = p_gdouble_to_uchar255(cmaskParPtr->triggerAlpha);
  cmaskParPtr->lowerAlpha255 = p_gdouble_to_uchar255(cmaskParPtr->lowerOpacity);
  cmaskParPtr->upperAlpha255 = p_gdouble_to_uchar255(cmaskParPtr->upperOpacity);
  cmaskParPtr->transparencyLevel255 = MIN(cmaskParPtr->lowerAlpha255, cmaskParPtr->upperAlpha255);
  cmaskParPtr->opacityFactor = MAX(cmaskParPtr->lowerOpacity, cmaskParPtr->upperOpacity)
                             - MIN(cmaskParPtr->lowerOpacity, cmaskParPtr->upperOpacity);
  cmaskParPtr->shadeRange = fabs(cmaskParPtr->hiColorThreshold - cmaskParPtr->loColorThreshold);

  cmaskParPtr->jaggedRadius = (gint)cmaskvals->pixelDiagonal;   /// TODO
  cmaskParPtr->jeggedLength = (gint)cmaskvals->pixelAreaLimit;   /// TODO



  cmaskParPtr->checkRadius = (gint)cmaskvals->pixelDiagonal;         /// TODO
  cmaskParPtr->checkMatchRadius = (gint)cmaskvals->pixelDiagonal;    /// TODO
  cmaskParPtr->areaMinimumPixels = (gint)cmaskvals->pixelAreaLimit;  /// TODO
  cmaskParPtr->avgFactor = cmaskvals->edgeColorThreshold;            // TODO
  cmaskParPtr->avgColorThreshold = cmaskParPtr->colorThreshold
                                 + ((cmaskParPtr->hiColorThreshold - cmaskParPtr->colorThreshold) *  cmaskParPtr->avgFactor);


  cmaskParPtr->significantRadius = (gint)cmaskvals->significantRadius;         /// TODO
  cmaskParPtr->significantColordiff = cmaskvals->significantColordiff;         /// TODO
  cmaskParPtr->significantBrightnessDiff = cmaskvals->significantBrightnessDiff;         /// TODO


  //if(gap_debug)
  {

    printf("significantRadius: %d\n", (int)cmaskParPtr->significantRadius);
    printf("significantColordiff: %f\n", (float)cmaskParPtr->significantColordiff);
    printf("significantBrightnessDiff: %f\n", (float)cmaskParPtr->significantBrightnessDiff);
    printf("checkRadius: %f\n", (float)cmaskParPtr->checkRadius);
    printf("checkMatchRadius: %f\n", (float)cmaskParPtr->checkMatchRadius);
    printf("areaMinimumPixels: %f\n", (float)cmaskParPtr->areaMinimumPixels);
    printf("avgFactor: %f\n", (float)cmaskParPtr->avgFactor);
    printf("avgColorThreshold: %f\n", (float)cmaskParPtr->avgColorThreshold);
  }

  cmaskParPtr->debugCoordinate = FALSE;
  p_get_guides(cmaskParPtr);

}  /* end p_copy_cmaskvals_to_colorMaskParams */



/* -----------------------------------------
 * gap_colormask_apply_to_layer_of_same_size
 * -----------------------------------------
 *
 * handles transparency by applying a color image (RGB) or (RGBA) as mask,
 * The pixels that are transparent (below or equal triggerAlpha) in
 * the alpha channel of the colormask are considered as PROTECTED
 * e.g are not affected by this filter.
 * (those pixels keep their original alpha channel)
 *
 * The resulting transparency is generated as new layermask
 * that can be optionally applied at end of processing (keepLayerMask FALSE)
 * Note that an already existing layermask will be applied before processing
 * and is replaced by the new generated one, even if keepLayerMask FALSE is used.
 *
 * current implementation provides 2 algorithms
 * a) by checking indiviual pixels
 * b) by checking areas of similar colors (triggered by thresholdColorArea > 0.0
 *
 * preconditions:
 *  destination layer (dst_layer_id) and colormask (cmask_drawable_id)
 *  must be of same size.
 *  destination layer must be of type RGBA.
 *  colormask drawable must be of type RGB or RGBA.
 *
 * returns -1 on failure or dst_layer_id on success
 *
 */
gint32
gap_colormask_apply_to_layer_of_same_size (gint32 dst_layer_id
                        , GapColormaskValues     *cmaskvals
                        , gboolean                doProgress
                        )
{
  GapColorMaskParams colorMaskParams;
  GapColorMaskParams *cmaskParPtr;
  GimpPixelRgn colormaskPR, dstPR, lmskPR;
  GimpDrawable *colormask_drawable;
  GimpDrawable *dst_drawable;
  GimpDrawable *dstLayerMask_drawable;
  gint32        oldLayerMaskId;
  gpointer  pr;
  GimpRGB   background;
  gint32    retLayerId;


  retLayerId = dst_layer_id;

  cmaskParPtr = &colorMaskParams;
  p_copy_cmaskvals_to_colorMaskParams(cmaskvals, cmaskParPtr, dst_layer_id, doProgress);

  dst_drawable = gimp_drawable_get (dst_layer_id);
  colormask_drawable = gimp_drawable_get (cmaskParPtr->cmask_drawable_id);
  //if(gap_debug)
  {
    printf("gap_colormask_apply_to_layer_of_same_size checking preconditions \n"
           "  DST: width:%d height: %d  BPP:%d \n"
           "  MSK: width:%d height: %d  BPP:%d \n"
           "  triggerAlpha:%d lowerAlpha:%d upperAlpha:%d algorithm:%d loColorThreshold:%f\n"
           "  cmaskParPtr:%d  hiColorThreshold: %f\n"
          , (int)dst_drawable->width
          , (int)dst_drawable->height
          , (int)dst_drawable->bpp
          , (int)colormask_drawable->width
          , (int)colormask_drawable->height
          , (int)colormask_drawable->bpp
          , (int)cmaskParPtr->triggerAlpha255
          , (int)cmaskParPtr->lowerAlpha255
          , (int)cmaskParPtr->upperAlpha255
          , (int)cmaskParPtr->algorithm
          , (float)cmaskParPtr->loColorThreshold
          , (int)cmaskParPtr
          , (float)cmaskParPtr->hiColorThreshold
          );
  }

  if((colormask_drawable->width  != dst_drawable->width)
  || (colormask_drawable->height != dst_drawable->height)
  || (dst_drawable->bpp    != 4)
  || (colormask_drawable->bpp    < 3))
  {
    if(gap_debug)
    {
      printf("gap_colormask_apply_to_layer_of_same_size preconditions not ok, operation NOT performed\n");
    }
    gimp_drawable_detach(colormask_drawable);
    gimp_drawable_detach(dst_drawable);
    return (-1);
  }

  gimp_image_undo_group_start (cmaskParPtr->dst_image_id);



  cmaskParPtr->width = dst_drawable->width;
  cmaskParPtr->height = dst_drawable->height;
  /* setup selected area
   * current implementation operates always on full layer size
   */
  cmaskParPtr->sel_x1 = 0;
  cmaskParPtr->sel_y1 = 0;
  cmaskParPtr->sel_x2 = cmaskParPtr->width -1;
  cmaskParPtr->sel_y2 = cmaskParPtr->height -1;

  /* progress init */
  {
    gint pixelSize;

    pixelSize = cmaskParPtr->height * cmaskParPtr->width;

    cmaskParPtr->pixelsDoneAllPasses = 0.0;
    cmaskParPtr->pixelsToProcessInAllPasses = pixelSize;

    if((cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_AVG_SMART)
    || (cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_AVG_AREA))
    {
      /* those algorithms have an extra pass to create the colordiffTable */
      cmaskParPtr->pixelsToProcessInAllPasses += pixelSize;
    }



    if(cmaskParPtr->featherRadius >= 1.0)
    {
      cmaskParPtr->pixelsToProcessInAllPasses += pixelSize;
    }

    if((cmaskParPtr->isleRadius >= 1) && (cmaskParPtr->isleAreaPixelLimit >= 1))
    {
      cmaskParPtr->pixelsToProcessInAllPasses += pixelSize;
    }
  }

  /* remove old layermask if there is one */
  oldLayerMaskId = gimp_layer_get_mask(cmaskParPtr->dst_layer_id);
  if (oldLayerMaskId >= 0)
  {
     gimp_layer_remove_mask (cmaskParPtr->dst_layer_id, GIMP_MASK_DISCARD);
  }


  /* create a new layermask (white is full opaque */
  cmaskParPtr->dstLayerMaskId = gimp_layer_create_mask(cmaskParPtr->dst_layer_id, GIMP_ADD_WHITE_MASK);
  gimp_layer_add_mask(cmaskParPtr->dst_layer_id, cmaskParPtr->dstLayerMaskId);
  dstLayerMask_drawable = gimp_drawable_get(cmaskParPtr->dstLayerMaskId);


  /* create and init pixelfetchers */
  cmaskParPtr->pftDest = gimp_pixel_fetcher_new (dst_drawable, FALSE /* shadow */);
  cmaskParPtr->pftMask = gimp_pixel_fetcher_new (colormask_drawable, FALSE /* shadow */);
  cmaskParPtr->pftDestLayerMask = gimp_pixel_fetcher_new (dstLayerMask_drawable, FALSE /* shadow */);

  gimp_context_get_background (&background);
  gimp_pixel_fetcher_set_bg_color (cmaskParPtr->pftDest, &background);
  gimp_pixel_fetcher_set_bg_color (cmaskParPtr->pftMask, &background);
  gimp_pixel_fetcher_set_bg_color (cmaskParPtr->pftDestLayerMask, &background);
  gimp_pixel_fetcher_set_edge_mode (cmaskParPtr->pftDest, GIMP_PIXEL_FETCHER_EDGE_BLACK);
  gimp_pixel_fetcher_set_edge_mode (cmaskParPtr->pftMask, GIMP_PIXEL_FETCHER_EDGE_BLACK);
  gimp_pixel_fetcher_set_edge_mode (cmaskParPtr->pftDestLayerMask, GIMP_PIXEL_FETCHER_EDGE_BLACK);


  gimp_pixel_rgn_init (&colormaskPR, colormask_drawable, 0, 0
                      , colormask_drawable->width, colormask_drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
  gimp_pixel_rgn_init (&dstPR, dst_drawable, 0, 0
                      , dst_drawable->width, dst_drawable->height
                      , TRUE      /* dirty */
                      , FALSE     /* shadow */
                       );
  gimp_pixel_rgn_init (&lmskPR, dstLayerMask_drawable, 0, 0
                      , dstLayerMask_drawable->width, dstLayerMask_drawable->height
                      , TRUE     /* dirty */
                      , FALSE     /* shadow */
                       );



  if((cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_AVG_SMART)
  || (cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_AVG_CHANGE_1)
  || (cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_AVG_CHANGE_2)
  || (cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_AVG_AREA))
  {
    /* code for the average colordiff based algorithms
     * (all of them are using a colordiffTable) */

    //if(gap_debug)
    {
      printf("BEFORE alloc colordiffTable\n");
    }

    cmaskParPtr->colordiffTable = g_malloc(cmaskParPtr->width * cmaskParPtr->height * sizeof(gdouble));


    /* 1.st pass to create a ColordiffTable and initialize with color differences foreach pixel
     */
    for (pr = gimp_pixel_rgns_register (2, &colormaskPR, &dstPR);
         pr != NULL;
         pr = gimp_pixel_rgns_process (pr))
    {
      p_init_colordiffTable (&colormaskPR, &dstPR, cmaskParPtr);
    }


    gimp_pixel_rgn_init (&colormaskPR, colormask_drawable, 0, 0
                      , colormask_drawable->width, colormask_drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
    gimp_pixel_rgn_init (&dstPR, dst_drawable, 0, 0
                    , dst_drawable->width, dst_drawable->height
                    , TRUE      /* dirty */
                    , FALSE     /* shadow */
                     );


    /* 2.nd pass to render layermask (by average colordiff within radius)
     */
    for (pr = gimp_pixel_rgns_register (3, &colormaskPR, &dstPR, &lmskPR);
         pr != NULL;
         pr = gimp_pixel_rgns_process (pr))
    {
      p_colormask_avg_rgn_render_region (&colormaskPR, &dstPR, &lmskPR, cmaskParPtr);
    }


    if(cmaskParPtr->keepWorklayer)
    {
      p_create_colordiffTable_Layer(cmaskParPtr, dst_drawable);
    }

    g_free(cmaskParPtr->colordiffTable);


  }
  else if(cmaskParPtr->algorithm == GAP_COLORMASK_ALGO_EDGE)
  {
    /* edge algorithm basic pass to render layermask (by compare colormask and destination layer colors)
     */
    for (pr = gimp_pixel_rgns_register (3, &colormaskPR, &dstPR, &lmskPR);
         pr != NULL;
         pr = gimp_pixel_rgns_process (pr))
    {
      p_colormask_edge_rgn_render_region (&colormaskPR, &dstPR, &lmskPR, cmaskParPtr);
    }
  }
  else  /* handles GAP_COLORMASK_ALGO_SIMPLE */
  {
    /* simple basic pass to render layermask (by compare colormask and destination layer colors)
     */
    for (pr = gimp_pixel_rgns_register (3, &colormaskPR, &dstPR, &lmskPR);
         pr != NULL;
         pr = gimp_pixel_rgns_process (pr))
    {
      p_colormask_rgn_render_region (&colormaskPR, &dstPR, &lmskPR, cmaskParPtr);
    }
  }

  /* optional pass to remove isolated pixels in the layermask */
  if((cmaskParPtr->isleRadius >= 1) && (cmaskParPtr->isleAreaPixelLimit >= 1))
  {
    gimp_pixel_rgn_init (&colormaskPR, colormask_drawable, 0, 0
                      , colormask_drawable->width, colormask_drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
    gimp_pixel_rgn_init (&lmskPR, dstLayerMask_drawable, 0, 0
                      , dstLayerMask_drawable->width, dstLayerMask_drawable->height
                      , TRUE      /* dirty */
                      , FALSE     /* shadow */
                       );


    for (pr = gimp_pixel_rgns_register (2, &colormaskPR, &lmskPR);
         pr != NULL;
         pr = gimp_pixel_rgns_process (pr))
    {
        p_remove_isolates_pixels_rgn_render_region (&colormaskPR, &lmskPR, cmaskParPtr);
    }
  }



  /* final optional pass to render smooth edges in the layermask */
  if(cmaskParPtr->featherRadius >= 1.0)
  {
    gimp_pixel_rgn_init (&colormaskPR, colormask_drawable, 0, 0
                      , colormask_drawable->width, colormask_drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
    gimp_pixel_rgn_init (&lmskPR, dstLayerMask_drawable, 0, 0
                      , dstLayerMask_drawable->width, dstLayerMask_drawable->height
                      , TRUE      /* dirty */
                      , FALSE     /* shadow */
                       );


    for (pr = gimp_pixel_rgns_register (2, &colormaskPR, &lmskPR);
         pr != NULL;
         pr = gimp_pixel_rgns_process (pr))
    {
        p_smooth_edges_rgn_render_region (&colormaskPR, &lmskPR, cmaskParPtr);
    }
  }


  gimp_pixel_fetcher_destroy (cmaskParPtr->pftDestLayerMask);
  gimp_pixel_fetcher_destroy (cmaskParPtr->pftDest);
  gimp_pixel_fetcher_destroy (cmaskParPtr->pftMask);

  gimp_drawable_detach(dst_drawable);
  gimp_drawable_detach(colormask_drawable);
  gimp_drawable_detach(dstLayerMask_drawable);


  if(cmaskParPtr->dstLayerMaskId >= 0)
  {
    if(!cmaskParPtr->keepLayerMask)
    {
      gimp_layer_remove_mask (cmaskParPtr->dst_layer_id, GIMP_MASK_APPLY);
    }
  }

  gimp_image_undo_group_end (cmaskParPtr->dst_image_id);

  return (retLayerId);

}  /* end gap_colormask_apply_to_layer_of_same_size */


/* ---------------------------------------------------
 * gap_colormask_apply_to_layer_of_same_size_from_file
 * ---------------------------------------------------
 * call gap_colormask_apply_to_layer_of_same_size
 * with parameters loaded from specified filename
 */
gint32
gap_colormask_apply_to_layer_of_same_size_from_file (gint32 dst_layer_id
                        , gint32                  colormask_id
                        , const char              *filename
                        , gboolean                keepLayerMask
                        , gboolean                doProgress
                        )
{
  GapColormaskValues      cmaskvals;
  gboolean                isLoadOk;

  if(filename != NULL)
  {
    isLoadOk = gap_colormask_file_load (filename, &cmaskvals);
  }
  else
  {
    /* set default values */
    cmaskvals.loColorThreshold = 0.11;
    cmaskvals.hiColorThreshold = 0.11;
    cmaskvals.colorSensitivity = 1.35;
    cmaskvals.lowerOpacity = 0.0;
    cmaskvals.upperOpacity = 1.0;
    cmaskvals.triggerAlpha = 0.8;
    cmaskvals.isleRadius = 1.0;
    cmaskvals.isleAreaPixelLimit = 7.0;
    cmaskvals.featherRadius = 3.0;
    cmaskvals.thresholdColorArea = 0.0;
    cmaskvals.pixelDiagonal = 10.0;
    cmaskvals.pixelAreaLimit = 25.0;

    isLoadOk = TRUE;
  }

  cmaskvals.colormask_id = colormask_id;
  cmaskvals.keepLayerMask = keepLayerMask;
  cmaskvals.keepWorklayer = FALSE;         /* dont keep debug worklayer when using the param file interface */
  if (isLoadOk)
  {
    return (gap_colormask_apply_to_layer_of_same_size (dst_layer_id
                        , &cmaskvals
                        , doProgress
                        ));
  }

  return -1;

}  /* end gap_colormask_apply_to_layer_of_same_size_from_file */




/* ======= */


/* -----------------------------------------
 * gap_create_or_replace_colormask_layer
 * -----------------------------------------
 * this procedure creates (or replaces) a layer named "colormask" and adds
 * it on top of the layerstack.
 * the new colormask layer is made as mix of the specified dst_layer_id
 * and the colormask layer of the previous frame.
 * (therefore the frame image with the previous frame number
 * is loladed and searched for a layer with the name "colormask"
 *
 */
gint32
gap_create_or_replace_colormask_layer (gint32 orig_layer_id
                        , GapColormaskValues     *cmaskvals
                        , gboolean                doProgress
                        )
{
  GapColorMaskParams colorMaskParams;
  GapColorMaskParams *cmaskParPtr;
  GimpPixelRgn prevCmaskPR, colormaskPR, origPR, workPR;
  GimpDrawable *prevCmask_drawable;
  GimpDrawable *colormask_drawable;
  GimpDrawable *orig_drawable;
  GimpDrawable *work_drawable;
  gpointer  pr;
  gint32    workLayerId;
  gint32    prevCmaskId;



  prevCmaskId = p_fetch_colormask_in_previous_frame(orig_layer_id);
  //if(gap_debug)
  {
    printf("gap_create_or_replace_colormask_layer prevCmaskId:%d\n"
      , (int)prevCmaskId
      );
  }

  if (prevCmaskId < 0)
  {
    prevCmaskId = cmaskvals->colormask_id;
  }
  cmaskParPtr = &colorMaskParams;
  p_copy_cmaskvals_to_colorMaskParams(cmaskvals, cmaskParPtr, orig_layer_id, doProgress);

  orig_drawable = gimp_drawable_get (orig_layer_id);
  colormask_drawable = gimp_drawable_get (cmaskParPtr->cmask_drawable_id);
  if(gap_debug)
  {
    printf("gap_create_or_replace_colormask_layer checking preconditions \n"
           "  DST: width:%d height: %d  BPP:%d \n"
           "  MSK: width:%d height: %d  BPP:%d \n"
           "  triggerAlpha:%d lowerAlpha:%d upperAlpha:%d colorThreshold:%f\n"
          , (int)orig_drawable->width
          , (int)orig_drawable->height
          , (int)orig_drawable->bpp
          , (int)colormask_drawable->width
          , (int)colormask_drawable->height
          , (int)colormask_drawable->bpp
          , (int)cmaskParPtr->triggerAlpha255
          , (int)cmaskParPtr->lowerAlpha255
          , (int)cmaskParPtr->upperAlpha255
          , (float)cmaskParPtr->colorThreshold
          );
  }

  if((colormask_drawable->width  != orig_drawable->width)
  || (colormask_drawable->height != orig_drawable->height)
  || (orig_drawable->bpp    != 4)
  || (colormask_drawable->bpp    < 3))
  {
    //if(gap_debug)
    {
      printf("gap_create_or_replace_colormask_layer preconditions not ok, operation NOT performed\n");
    }
    gimp_drawable_detach(colormask_drawable);
    gimp_drawable_detach(orig_drawable);
    return (-1);
  }

  gimp_image_undo_group_start (cmaskParPtr->dst_image_id);



  cmaskParPtr->width = orig_drawable->width;
  cmaskParPtr->height = orig_drawable->height;
  /* progress init */
  {
    gint pixelSize;

    pixelSize = cmaskParPtr->height * cmaskParPtr->width;

    cmaskParPtr->pixelsDoneAllPasses = 0.0;
    cmaskParPtr->pixelsToProcessInAllPasses = pixelSize;
  }


  p_remove_layer_by_name(cmaskParPtr->dst_image_id, orig_layer_id, COLORMASK_LAYER_NAME);
  workLayerId = p_create_empty_layer(cmaskParPtr->dst_image_id
                       , cmaskParPtr->width
                       , cmaskParPtr->height
                       , COLORMASK_LAYER_NAME
                       );

  work_drawable = gimp_drawable_get (workLayerId);
  prevCmask_drawable = gimp_drawable_get (prevCmaskId);


  gimp_pixel_rgn_init (&prevCmaskPR, prevCmask_drawable, 0, 0
                      , prevCmask_drawable->width, prevCmask_drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
  gimp_pixel_rgn_init (&colormaskPR, colormask_drawable, 0, 0
                      , colormask_drawable->width, colormask_drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
  gimp_pixel_rgn_init (&origPR, orig_drawable, 0, 0
                      , orig_drawable->width, orig_drawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );
  gimp_pixel_rgn_init (&workPR, work_drawable, 0, 0
                      , work_drawable->width, work_drawable->height
                      , TRUE      /* dirty */
                      , FALSE     /* shadow */
                       );


  for (pr = gimp_pixel_rgns_register (4, &prevCmaskPR, &colormaskPR, &origPR, &workPR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
      p_mix_newmask_rgn_render (&prevCmaskPR, &colormaskPR, &origPR, &workPR, cmaskParPtr);
  }


  gimp_drawable_detach(prevCmask_drawable);
  gimp_drawable_detach(colormask_drawable);
  gimp_drawable_detach(orig_drawable);
  gimp_drawable_detach(work_drawable);

  gimp_image_undo_group_end (cmaskParPtr->dst_image_id);

  if(prevCmaskId != cmaskParPtr->cmask_drawable_id)
  {
    /* delete (e.g. close) the previous frame image (form memory not from disk)
     */
    gap_image_delete_immediate(gimp_drawable_get_image(prevCmaskId));
  }
  return (workLayerId);

}  /* end gap_create_or_replace_colormask_layer */


/* -----------------------------------------
 * gap_colormask_apply_by_name
 * -----------------------------------------
 * find colormask in the layerstack by name
 * and apply to dst_layer_id if found.
 * return -1 if not found
 *
 */
gint32
gap_colormask_apply_by_name (gint32 dst_layer_id
                        , GapColormaskValues     *cmaskvals
                        , gboolean                doProgress
                        )
{
  gint32    colormaskId;


  colormaskId = gap_layer_find_by_name(gimp_drawable_get_image(dst_layer_id), COLORMASK_LAYER_NAME);
  if (colormaskId < 0)
  {
    return (-1);
  }

  cmaskvals->colormask_id = colormaskId;
  return (gap_colormask_apply_to_layer_of_same_size (dst_layer_id
                          , cmaskvals
                          , doProgress
                          ));

}  /* end gap_colormask_apply_by_name */


