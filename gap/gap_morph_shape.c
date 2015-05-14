/* gap_morph_shape.c
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * version 2.7.0;             hof: created
 */

#include "config.h"


/* SYTEM (UNIX) includes */
#include "string.h"
#include <errno.h>

/* GIMP includes */
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

/* GAP includes */
#include "gap_lib_common_defs.h"
#include "gap_colordiff.h"
#include "gap_locate.h"
#include "gap_edge_detection.h"
#include "gap_colordiff.h"
#include "gap_morph_shape.h"
#include "gap_morph_main.h"
#include "gap_morph_dialog.h"
#include "gap_morph_exec.h"
#include "gap_lib.h"
#include "gap_image.h"
#include "gap_libgapbase.h"

#include "gap-intl.h"

#define MAX_TOLERATED_ANGLE_DIFF_DEGREE 22.0
#define MAX_TOLERATED_LENGTH_DIFF_PIXELS 33.0


extern      int gap_debug; /* ==0  ... dont print debug infos */

typedef struct GapMorphShapeContext { /* nickname: msctx */
     gint32 edgeImageId;
     gint32 edgeLayerId;
     gint32 refLayerId;
     gint32 targetLayerId;
     GimpDrawable *edgeDrawable;
     GimpPixelFetcher *pftEdge;
     
     gdouble locateColordiffThreshold;
     gint32  locateDetailMoveRadius;
     gint32  locateDetailShapeRadius;
     gdouble edgeColordiffThreshold;
     gdouble colorSensitivity;
     gint32  countEdgePixels;

     gint32  numShapePoints;
     gint32  countGeneratedPoints;
     
     /* generate points within the clipping boundary sel1/sel2 coordinate */
     gint32  sel1X;
     gint32  sel1Y;
     gint32  sel2X;
     gint32  sel2Y;
     
     
     gboolean doProgress;
     GtkWidget *progressBar;
     gboolean  *cancelWorkpointGenerationPtr;

  } GapMorphShapeContext;

static gboolean     p_pixel_check_opaque(GimpPixelFetcher *pft
                                     , gint bpp
                                     , gdouble needx
                                     , gdouble needy
                                     );
static void         p_find_outmost_opaque_pixel(GimpPixelFetcher *pft
                           ,gint bpp
                           ,gdouble alpha_rad
                           ,gint32 width
                           ,gint32 height
                           ,gdouble *px
                           ,gdouble *py
                           );


/* ------------------------------------
 * gap_morph_shape_calculateAngleDegree
 * ------------------------------------
 */
gdouble
gap_morph_shape_calculateAngleDegree(GapMorphWorkPoint *wp, gdouble *length)
{
  gdouble angleRad;
  gdouble angleDeg;
  gdouble sqrLen;
  gdouble dx;
  gdouble dy;
  gdouble tanAlpha;
 
  dx = wp->fdst_x - wp->osrc_x;
  dy = wp->fdst_y - wp->osrc_y;

  sqrLen = (dx * dx) + (dy * dy);

  if (dx > 0)
  {
    tanAlpha = dy / dx;
    angleRad = atan(tanAlpha);
    if(angleRad < 0)
    {
      angleRad += (2.0 * G_PI);
    }
  } else if (dx < 0)
  {
    tanAlpha = dy / (0.0 - dx);
    angleRad = atan(tanAlpha);
    angleRad = G_PI - angleRad;
  } else
  {
    /* dx == 0 */
    if(dy > 0)
    {
      /* + 90 degree */
      angleRad = G_PI / 2.0;
    } else if(dy < 0)
    {
      /* 270 degree */
      angleRad = G_PI + (G_PI / 2.0);
    } else
    {
      /* undefined angle */
      angleRad = 0;
    }

  }

  *length = sqrt(sqrLen);
  angleDeg = (angleRad * 180.0) / G_PI;
  
  if(gap_debug)
  {
    printf("WP: dx:%.1f dy:%.1f  len:%.3f   angleDeg:%.5f\n"
       ,(float)dx
       ,(float)dy
       ,(float)*length
       ,(float)angleDeg
       );
  }
  
  return(angleDeg);

}  /* end gap_morph_shape_calculateAngleDegree */




/* -----------------------------
 * gap_morph_shape_new_workpont
 * -----------------------------
 */
GapMorphWorkPoint *
gap_morph_shape_new_workpont(gdouble srcx, gdouble srcy, gdouble dstx, gdouble dsty)
{
  GapMorphWorkPoint *wp;

  wp = g_new(GapMorphWorkPoint, 1);
  wp->next = NULL;
  wp->fdst_x = dstx;
  wp->fdst_y = dsty;
  wp->osrc_x = srcx;
  wp->osrc_y = srcy;

  wp->dst_x = wp->fdst_x;
  wp->dst_y = wp->fdst_y;
  wp->src_x = wp->osrc_x;
  wp->locateColordiff = 0.0;


  wp->src_y = wp->osrc_y;
  
  return(wp);
}  /* end gap_morph_shape_new_workpont */


/* ---------------------------------
 * p_doProgress
 * ---------------------------------
 */
static void
p_doProgress(GapMorphShapeContext *msctx)
{
  guchar *progressText;
  gdouble percentage;

  if(msctx->doProgress != TRUE)
  {
    return;
  }
  percentage = (gdouble)msctx->countGeneratedPoints / MAX(1.0, (gdouble)msctx->numShapePoints);
  
  progressText = g_strdup_printf(_("generating workpoint:%d (%d)")
                     , (int)msctx->countGeneratedPoints
                     , (int)msctx->numShapePoints
                     );
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(msctx->progressBar), progressText);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(msctx->progressBar), percentage);
  g_free(progressText);

  while (gtk_events_pending ())
  {
    gtk_main_iteration ();
  }
}  /* end p_doProgress */


/* ---------------------------------
 * p_find_nearest_point
 * ---------------------------------
 * saerch the workpoint list for the point that is the nearest
 * to position in_x/in_y in the osrc or fdst koord system.
 * (depending on the swp->src_flag)
 */
static GapMorphWorkPoint *
p_find_workpoint_near_src_coords(GapMorphGlobalParams *mgpp
                    , gdouble in_x
                    , gdouble in_y
                    , gint32  nearRadius
                    )
{
  GapMorphWorkPoint *wp;
  GapMorphWorkPoint *wp_list;
  gdouble            sqr_distance;
  gdouble            near_sqr_dist;
  
  near_sqr_dist = (nearRadius * nearRadius) * 2;
  wp_list = mgpp->master_wp_list;

  for(wp = wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
  {
    gdouble  adx;
    gdouble  ady;

    if((wp->osrc_x == in_x)
    && (wp->osrc_y == in_y))
    {
      return(wp);
    }

    adx = abs(wp->osrc_x - in_x);
    ady = abs(wp->osrc_y - in_y);

    sqr_distance = (adx * adx) + (ady * ady);

    if(sqr_distance <= near_sqr_dist)
    {
      return(wp);
    }
  }

  return(NULL);
}  /* end p_find_nearest_point */



/* -----------------------------
 * p_locate_workpoint
 * -----------------------------
 * locate point coordinate in the other drawable
 * (by searching for best matching shape within move radius)
 */
static gboolean
p_locate_workpoint(GapMorphShapeContext *msctx
  , gdouble rx, gdouble ry
  , gdouble *tx, gdouble *ty
  , gdouble *locateColordiff)
{
  gint32    refX;
  gint32    refY;
  gint32    targetX;
  gint32    targetY;
  gboolean  success;
  
  refX = rx;
  refY = ry;

  *locateColordiff = gap_locateDetailWithinRadius(msctx->refLayerId
                  , refX
                  , refY
                  , msctx->locateDetailShapeRadius
                  , msctx->targetLayerId
                  , msctx->locateDetailMoveRadius
                  , msctx->locateColordiffThreshold
                  , &targetX
                  , &targetY
                  );
  if(gap_debug)
  {
    printf("p_locate_workpoint: x/y: %d / %d MoveRadius:%d ShapeRadius:%d Threshold:%.5f colordiff:%.5f\n"
          , (int)refX
          , (int)refY
          , (int)msctx->locateDetailMoveRadius
          , (int)msctx->locateDetailShapeRadius
          , (float)msctx->locateColordiffThreshold
          , (float)*locateColordiff
          );
          
  }

  if(*locateColordiff < msctx->locateColordiffThreshold)
  {
    /* found a sufficient matching shape in the target drawable
     * at targetX/Y coordinate. Overwrite tx and ty with this matching point coordinate
     */
    *tx = targetX;
    *ty = targetY;
    success = TRUE;

    if(gap_debug)
    {
      printf("p_locate_workpoint: SUCCESS, found matching shape! tx:%d ty:%d colordiff:%.4f colordiffThres:%.4f\n"
         ,(int)targetX
         ,(int)targetY
         ,(float)*locateColordiff
         ,(float)msctx->locateColordiffThreshold
         );
    }
  }
  else
  {
    success = FALSE;
  }

  return(success);
  
}  /* end p_locate_workpoint */












/* -----------------------------------------------
 * p_find_workpoint_nearest_to_src_coords
 * -----------------------------------------------
 * find workpoint that is nearest (but not equal)
 * to specified coords.
 * return the nearest workpoint within check radius
 *        (and set wp->sqr_dist  to the square distance)
 * or NULLin case no workpoint is near.
 */
static GapMorphWorkPoint*
p_find_workpoint_nearest_to_src_coords(GapMorphGlobalParams *mgpp
                    , gdouble in_x
                    , gdouble in_y
                    , gint32  checkRadius
                    )
{
  GapMorphWorkPoint *wp;
  GapMorphWorkPoint *wp_ret;
  GapMorphWorkPoint *wp_list;
  gdouble            sqr_distance;
  gdouble            check_sqr_dist;
  
  check_sqr_dist = (checkRadius * checkRadius) * 2;
  wp_list = mgpp->master_wp_list;
  wp_ret = NULL;

  for(wp = wp_list; wp != NULL; wp = (GapMorphWorkPoint *)wp->next)
  {
    gdouble  adx;
    gdouble  ady;

    if((wp->osrc_x == in_x)
    && (wp->osrc_y == in_y))
    {
      continue;
    }

    adx = abs(wp->osrc_x - in_x);
    ady = abs(wp->osrc_y - in_y);

    sqr_distance = (adx * adx) + (ady * ady);

    if(sqr_distance < check_sqr_dist)
    {
      check_sqr_dist = sqr_distance;
      wp_ret = wp;
    }
  }
  
  if(wp_ret)
  {
    wp_ret->sqr_dist = check_sqr_dist;
  }

  return(wp_ret);
  
}  /* end p_find_nearest_point */



/* -----------------------------------------------
 * p_compare_workpoint_direction
 * -----------------------------------------------
 * return FALSE in case the specified workpoint
 * movement vector differs significant from specified angle/length.
 * in case the vector length is 0 (just a point)
 * TRUE is returned.
 */
static gboolean
p_compare_workpoint_direction(GapMorphWorkPoint *wpRef
  , gdouble angleDeg
  , gdouble length
  )
{
  gdouble lengthRef;
  gdouble angleDegRef;
  gdouble angleDiffDeg;
  gdouble lengthDiffPixels;

  angleDegRef = gap_morph_shape_calculateAngleDegree(wpRef, &lengthRef);
  lengthDiffPixels = fabs(length - lengthRef);

  if((lengthRef > 0) && (length > 0))
  {
    lengthDiffPixels = fabs(length - lengthRef);
    if(lengthDiffPixels > MAX_TOLERATED_LENGTH_DIFF_PIXELS)
    {
      return (FALSE);
    }


    angleDiffDeg = fabs(angleDeg - angleDegRef);
    if(angleDiffDeg > 180)
    {
      angleDiffDeg = 360 - angleDiffDeg;
    }

    if(angleDiffDeg > MAX_TOLERATED_ANGLE_DIFF_DEGREE)
    {
      return (FALSE);
    }
  }
  return (TRUE);
  
}  /* end p_compare_workpoint_direction */


/* -----------------------------------------------
 * p_copy_workpoint_attributes
 * -----------------------------------------------
 * copy relevant attributes
 * (but keep next pointer unchanged)
 */
static void
p_copy_workpoint_attributes(GapMorphWorkPoint *wpSrc, GapMorphWorkPoint *wpDst)
{
   wpDst->fdst_x = wpSrc->fdst_x;   /* final dest koord (as set by user for last dest. frame) */
   wpDst->fdst_y = wpSrc->fdst_y;
   wpDst->osrc_x = wpSrc->osrc_x;   /* start source koord (as set by user for the 1.st frame) */
   wpDst->osrc_y = wpSrc->osrc_y;

   wpDst->dst_x = wpSrc->dst_x;    /* koord trans */
   wpDst->dst_y = wpSrc->dst_y;
   wpDst->src_x = wpSrc->src_x;    /* osrc_x scaled to koords of current (dest) frame */
   wpDst->src_y = wpSrc->src_y;

   wpDst->locateColordiff = wpSrc->locateColordiff;
}

/* -----------------------------------------------
 * p_workpoint_plausiblity_filter
 * -----------------------------------------------
 * return FALSE in case the specified workpoint
 * has untypical movement that differs significant from near workpoints.
 */
static gboolean
p_workpoint_plausiblity_filter(GapMorphShapeContext *msctx
                    , GapMorphGlobalParams *mgpp
                    , GapMorphWorkPoint *wp
                    )
{
#define NEAR_CHECK_RADIUS 20.0
#define OUTER_CHECK_RADIUS (30.0 + NEAR_CHECK_RADIUS)
  GapMorphWorkPoint *wpRef;
  gdouble length;
  gdouble angleDeg;
  gboolean isWorkpointOk;
  
  isWorkpointOk = TRUE;
  angleDeg = gap_morph_shape_calculateAngleDegree(wp, &length);

  wpRef = p_find_workpoint_nearest_to_src_coords(mgpp
                    , wp->osrc_x   /* gdouble in_x */
                    , wp->osrc_y   /* gdouble in_y */
                    , OUTER_CHECK_RADIUS
                    );
  if(wpRef != NULL)
  {
    /* found a near workpoint that is either already checked
     * or was already present before start of workpoint generation
     */ 
    isWorkpointOk = p_compare_workpoint_direction(wpRef, angleDeg, length);

    if(gap_debug)
    {
      printf("NEAREST Ref workpoint found x:%d y:%d xRef:%d yRef:%d OK:%d colordiff:%.4f colordiffRef:%.4f dist:%.4f DIST_LIMIT:%.4f\n"
          , (int)wp->osrc_x
          , (int)wp->osrc_y
          , (int)wpRef->osrc_x
          , (int)wpRef->osrc_y
          , (int)isWorkpointOk
          , (float)wp->locateColordiff
          , (float)wpRef->locateColordiff
          , (float)sqrt(wpRef->sqr_dist)
          , (float)NEAR_CHECK_RADIUS
          );
    }


    if (isWorkpointOk != TRUE)
    {
      if(wp->locateColordiff < wpRef->locateColordiff)
      {
        if(sqrt(wpRef->sqr_dist) <= NEAR_CHECK_RADIUS)
        {
          /* overwrite wpRef attributes because the new workpoint wp
           */
          p_copy_workpoint_attributes(wp, wpRef);
          if(gap_debug)
          {
            printf("REPLACE Ref workpoint x:%d y:%d xRef:%d yRef:%d\n"
                , (int)wp->osrc_x
                , (int)wp->osrc_y
                , (int)wpRef->osrc_x
                , (int)wpRef->osrc_y
                );
          }
        }
      }
    }

  }



  if(gap_debug)
  {
    if(isWorkpointOk != TRUE)
    {
      printf("DISCARD non-plausible workpoint at x:%d y:%d\n"
        , (int)wp->osrc_x
        , (int)wp->osrc_y
        );
    }
  }

  return(isWorkpointOk);
  
}  /* end p_workpoint_plausiblity_filter */





/* --------------------------------
 * p_generateWorkpoints
 * --------------------------------
 * scan the edge drawable with descending grid size (defined stepSizeTab)
 * and descending edge threshold. This shall spread the generated
 * workpoints all over the whole area where bright edge points
 * are picked first.
 */
static void
p_generateWorkpoints(GapMorphShapeContext *msctx, GapMorphGlobalParams *mgpp)
{
#define LOOP_LOOKUP_TAB_SIZE 5
  static gint32  edgeThresTab[LOOP_LOOKUP_TAB_SIZE]   = { 120, 60, 30, 10,  1 };
  static gint32  nearRadiusTab[LOOP_LOOKUP_TAB_SIZE]  = {  20, 14, 10,  6,  2 };
  static gint32  stepSizeTab[LOOP_LOOKUP_TAB_SIZE]    = {  10,  8,  6,  2,  1 };
  gint32  ex;
  gint32  ey;
  gdouble srcX;
  gdouble srcY;
  gint32  nearRadius;
  gint32  edgePixelThreshold255;
  gint32  stepSize;
  gint32  ii;

  for(ii=0; ii < LOOP_LOOKUP_TAB_SIZE; ii++)
  {
    edgePixelThreshold255 = edgeThresTab[ii];
    nearRadius = nearRadiusTab[ii];
    stepSize = stepSizeTab[ii];
    
    for(ey=msctx->sel1Y; ey < msctx->sel2Y; ey += stepSize)
    {
      srcY = ey +1;
      for(ex=msctx->sel1X; ex < msctx->sel2X; ex += stepSize)
      {
        guchar pixel[4];
  
        if(*msctx->cancelWorkpointGenerationPtr == TRUE)
        {
          return;
        }

        gimp_pixel_fetcher_get_pixel (msctx->pftEdge, ex, ey, &pixel[0]);

        if(pixel[0] >= edgePixelThreshold255)
        {
          /* found an edge pixel */
          GapMorphWorkPoint *wp;

          srcX = ex +1;


          /* chek if workpoint with this (or very near) coords does already exist */
          wp = p_find_workpoint_near_src_coords(mgpp
                      , srcX
                      , srcY
                      , nearRadius
                      );
          if(wp == NULL)
          {
            gboolean success;
            gdouble dstX;
            gdouble dstY;
            gdouble locateColordiff;

            success = p_locate_workpoint(msctx
                                        , srcX,  srcY
                                        , &dstX, &dstY
                                        , &locateColordiff
                                        );
            if(success)
            {
              wp = gap_morph_shape_new_workpont(srcX, srcY, dstX, dstY);
              wp->locateColordiff = locateColordiff;
              
              if(TRUE == p_workpoint_plausiblity_filter(msctx
                    , mgpp
                    , wp
                    ))
              {
                msctx->countGeneratedPoints++; 
                /* add new workpoint as 1st listelement */
                wp->next = mgpp->master_wp_list;
                mgpp->master_wp_list = wp;

                p_doProgress(msctx);

                if(msctx->countGeneratedPoints >= msctx->numShapePoints)
                {
                  return;
                }
              }
              else
              {
                g_free(wp);
              }

            }

          }

        }

      }
    }
  }
  
}  /* end p_generateWorkpoints */


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
gap_moprhShapeDetectionEdgeBased(GapMorphGUIParams *mgup, gboolean *cancelFlagPtr)
{
  GapMorphShapeContext  morphShapeContext;
  GapMorphShapeContext *msctx;
  gboolean deleteEdgeImage;
  GapMorphGlobalParams *mgpp;

  mgpp = mgup->mgpp;

  if(mgup->workpointGenerationBusy == TRUE)
  {
    return;
  }
  mgup->workpointGenerationBusy = TRUE;
  deleteEdgeImage = TRUE;
  
  /* init context */  
  msctx = &morphShapeContext;
  msctx->edgeColordiffThreshold = CLAMP(mgpp->edgeColordiffThreshold, 0.0, 1.0);
  msctx->locateColordiffThreshold = CLAMP(mgpp->locateColordiffThreshold, 0.0, 1.0);
  msctx->locateDetailShapeRadius = mgpp->locateDetailShapeRadius;
  msctx->locateDetailMoveRadius = mgpp->locateDetailMoveRadius;
  msctx->colorSensitivity = GAP_COLORDIFF_DEFAULT_SENSITIVITY;
  msctx->refLayerId = mgpp->osrc_layer_id;
  msctx->targetLayerId = mgpp->fdst_layer_id;
  msctx->numShapePoints = mgpp->numWorkpoints;
  msctx->countGeneratedPoints = 0;

  msctx->doProgress = TRUE;
  msctx->progressBar = mgup->progressBar;
  if(msctx->progressBar == NULL)
  {
    msctx->doProgress = FALSE;
  }
  msctx->cancelWorkpointGenerationPtr = cancelFlagPtr;

  if(gap_debug)
  {
    printf("gap_moprhShapeDetectionEdgeBased START edgeThres:%.5f  locateThres:%.5f locateRadius:%d\n"
       , (float)msctx->edgeColordiffThreshold
       , (float)msctx->locateDetailMoveRadius
       , (int)msctx->locateDetailMoveRadius
       );
  }


  msctx->edgeLayerId = gap_edgeDetection(mgup->mgpp->osrc_layer_id   /* refDrawableId */
                                 ,mgup->mgpp->edgeColordiffThreshold
                                 ,&msctx->countEdgePixels
                                 );

  msctx->edgeImageId = gimp_drawable_get_image(msctx->edgeLayerId);

  if(gap_debug)
  {
    /* show the edge image for debug purpose
     * (this image is intended for internal processing usage)
     */
    gimp_display_new(msctx->edgeImageId);
    deleteEdgeImage = FALSE;
  }
  msctx->edgeDrawable = gimp_drawable_get(msctx->edgeLayerId);

 
  if(msctx->edgeDrawable != NULL)
  {
    gint      maxWidth;
    gint      maxHeight;
    
    maxWidth = msctx->edgeDrawable->width-1;
    maxHeight = msctx->edgeDrawable->height-1;
    
    if((mgup->src_win.zoom < 1.0) && (mgup->src_win.zoom > 0.0))
    {
      gdouble   fwidth;
      gdouble   fheight;
      gint      width;
      gint      height;
      
      fwidth  = mgup->src_win.zoom * (gdouble)mgup->src_win.pv_ptr->pv_width;
      fheight = mgup->src_win.zoom * (gdouble)mgup->src_win.pv_ptr->pv_height;
      width  = CLAMP((gint)fwidth,  1, maxWidth);
      height = CLAMP((gint)fheight, 1, maxHeight);
      msctx->sel1X = CLAMP(mgup->src_win.offs_x, 0, maxWidth);
      msctx->sel1Y = CLAMP(mgup->src_win.offs_y, 0, maxHeight);
      msctx->sel2X = CLAMP((msctx->sel1X + width), 0, maxWidth);
      msctx->sel2Y = CLAMP((msctx->sel1Y + height), 0, maxHeight);
    }
    else
    {
      msctx->sel1X = 0;
      msctx->sel1Y = 0;
      msctx->sel2X = maxWidth;
      msctx->sel2Y = maxHeight;
    }
   

    if(gap_debug)
    {
      printf("Boundaries: sel1: %d / %d sel2: %d / %d  zoom:%.5f\n"
        , (int)msctx->sel1X
        , (int)msctx->sel1Y
        , (int)msctx->sel2X
        , (int)msctx->sel2Y
        , (float)mgup->src_win.zoom
        );
    }


    msctx->pftEdge = gimp_pixel_fetcher_new (msctx->edgeDrawable, FALSE /* shadow */);
    gimp_pixel_fetcher_set_edge_mode (msctx->pftEdge, GIMP_PIXEL_FETCHER_EDGE_BLACK);

    p_generateWorkpoints(msctx, mgpp);

    gimp_pixel_fetcher_destroy (msctx->pftEdge);
  }

  if(msctx->edgeDrawable != NULL)
  {
    gimp_drawable_detach(msctx->edgeDrawable);
    msctx->edgeDrawable = NULL;
  }
  
  if(deleteEdgeImage == TRUE)
  {
    gap_image_delete_immediate(msctx->edgeImageId);
  }

  if(gap_debug)
  {
    printf("gap_moprhShapeDetectionEdgeBased END\n");
  }
  mgup->workpointGenerationBusy = FALSE;
  
  return ;

}  /* end gap_moprhShapeDetectionEdgeBased */



/* -----------------------------------------------
 * p_generateWorkpointFileForFramePair
 * -----------------------------------------------
 * generate workpoint file for morphing
 * tweens between currFrameNr and nextFrameNr
 *
 * return TRUE on success, FALSE when Failed
 */
static gboolean
p_generateWorkpointFileForFramePair(const char *workpointFileName, GapMorphGUIParams *mgup
     , gboolean *cancelFlagPtr, gint32 currLayerId, gint32 nextLayerId)
{
  /* startup with empty workpoint list
   */
  if(mgup->mgpp->master_wp_list != NULL)
  {
    gap_morph_exec_free_workpoint_list(&mgup->mgpp->master_wp_list);
  }
  mgup->mgpp->master_wp_list = NULL;
  
  mgup->mgpp->osrc_layer_id = currLayerId;
  mgup->mgpp->fdst_layer_id = nextLayerId;

  /* if workpointfile already exists (and overwrite flag is not specified)
   * load and append newly generated points 
   */
  if(g_file_test(workpointFileName, G_FILE_TEST_EXISTS))
  {
    if(mgup->mgpp->append_flag != TRUE)
    {
      if(gap_debug)
      {
        printf("SKIP processing for already existing workpointfile:%s\n"
            , workpointFileName
            );
      }
      return (TRUE);
    }
    if(mgup->mgpp->overwrite_flag != TRUE)
    {
      if(gap_debug)
      {
        printf("ADD newly generated workpoints to already existing workpointfile:%s\n"
            , workpointFileName
            );
      }
      GapMorphGUIParams morphParamsInFile;
      GapMorphGUIParams *mgupFile;
      GapMorphGlobalParams  dummyParams;
      GapMorphGlobalParams *mgppFile;

      mgppFile = &dummyParams;
      mgppFile->osrc_layer_id = mgup->mgpp->osrc_layer_id;
      mgppFile->fdst_layer_id = mgup->mgpp->fdst_layer_id;
     
      mgupFile = &morphParamsInFile;
      mgupFile->mgpp = mgppFile;
     
      mgup->mgpp->master_wp_list = gap_moprh_exec_load_workpointfile(workpointFileName
                                  ,mgupFile
                                  );
    }
  }

  if(mgup->mgpp->numOutlinePoints > 0)
  {
    gap_morph_shape_generate_outline_workpoints(mgup->mgpp);
  }
  
  
  gap_moprhShapeDetectionEdgeBased(mgup, cancelFlagPtr);
  if(!gap_moprh_exec_save_workpointfile(workpointFileName, mgup))
  {
   gint   l_errno;
   
   l_errno = errno;
   g_message (_("Failed to write morph workpointfile\n"
                "filename: '%s':\n%s")
             , workpointFileName
             , g_strerror (l_errno)
             );
    return (FALSE);
  }

  return (TRUE);

}  /* end p_generateWorkpointFileForFramePair */



/* -----------------------------------------------
 * p_getMergedFrameImageLayer
 * -----------------------------------------------
 *
 * return -1 if FAILED, or layerId (positive integer) on success
 */
gint32
p_getMergedFrameImageLayer(GapAnimInfo *ainfo_ptr, gint32 frameNr)
{
  gint32 imageId;
  gint32 layerId;

  layerId = -1;
  if(ainfo_ptr->new_filename != NULL)
  {
    g_free(ainfo_ptr->new_filename);
  }
  ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      frameNr,
                                      ainfo_ptr->extension);
  if(ainfo_ptr->new_filename == NULL)
  {
     printf("could not create frame filename for frameNr:%d\n", (int)frameNr);
     return -1;
  }

  if(!g_file_test(ainfo_ptr->new_filename, G_FILE_TEST_EXISTS))
  {
     printf("file not found: %s\n", ainfo_ptr->new_filename);
     return -1;
  }

  /* load current frame */
  imageId = gap_lib_load_image(ainfo_ptr->new_filename);
  if(imageId >= 0)
  {
    layerId = gap_image_merge_visible_layers(imageId, GIMP_CLIP_TO_IMAGE);
  }
  
  return(layerId);
}

/* -----------------------------------------------
 * p_do_master_progress
 * -----------------------------------------------
 */
static void
p_do_master_progress(GtkWidget *progressBar
   , const char *workpointFileName,  gdouble framesToProcess, gdouble frameNr)
{
  if(progressBar != NULL)
  {
    char *msg;
    gdouble percentage;
    
    percentage = frameNr / MAX(1.0, framesToProcess);
    msg = gap_base_shorten_filename(NULL       /* prefix */
                        ,workpointFileName     /* filenamepart */
                        ,NULL                  /* suffix */
                        ,130                   /* l_max_chars */
                        );
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressBar), msg);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressBar), percentage);
    g_free(msg);

    while (gtk_events_pending ())
    {
      gtk_main_iteration ();
    }
  }
}



/* -----------------------------
 * p_pixel_check_opaque
 * -----------------------------
 * check average opacity in an area
 * of 2x2 pixels
 * return TRUE if average alpha is 50% or more opaque
 */
static gboolean
p_pixel_check_opaque(GimpPixelFetcher *pft, gint bpp, gdouble needx, gdouble needy)
{
  guchar  pixel[4][4];
  gdouble alpha_val;

  gint    xi, yi;
  gint alpha_idx;


  if (needx >= 0.0)
    xi = (int) needx;
  else
    xi = -((int) -needx + 1);

  if (needy >= 0.0)
    yi = (int) needy;
  else
    yi = -((int) -needy + 1);

  gimp_pixel_fetcher_get_pixel (pft, xi, yi, pixel[0]);
  gimp_pixel_fetcher_get_pixel (pft, xi + 1, yi, pixel[1]);
  gimp_pixel_fetcher_get_pixel (pft, xi, yi + 1, pixel[2]);
  gimp_pixel_fetcher_get_pixel (pft, xi + 1, yi + 1, pixel[3]);

  alpha_idx = bpp -1;
  /* average aplha channel normalized to range 0.0 upto 1.0 */
  alpha_val = (
               (gdouble)pixel[0][alpha_idx] / 255.0
            +  (gdouble)pixel[1][alpha_idx] / 255.0
            +  (gdouble)pixel[2][alpha_idx] / 255.0
            +  (gdouble)pixel[3][alpha_idx] / 255.0
            ) /  4.0;

  if (alpha_val > 0.5)  /* fix THRESHOLD half or more opaque */
  {
    return (TRUE);
  }
  return (FALSE);
}  /* end p_pixel_check_opaque */


/* -----------------------------
 * p_find_outmost_opaque_pixel
 * -----------------------------
 * returns koords in paramters px, py
 */
static void
p_find_outmost_opaque_pixel(GimpPixelFetcher *pft
                           ,gint bpp
                           ,gdouble alpha_rad
                           ,gint32 width
                           ,gint32 height
                           ,gdouble *px
                           ,gdouble *py
                           )
{
  gdouble center_x;
  gdouble center_y;
  gdouble cos_alpha;
  gdouble sin_alpha;
  gdouble l_x, l_y, l_r;
  gdouble half_width;
  gdouble half_height;

  l_x = 0;
  l_y = 0;
  cos_alpha = cos(alpha_rad);
  sin_alpha = sin(alpha_rad);

  /* printf("sin: %.5f cos:%.5f\n", sin_alpha, cos_alpha); */

  half_width = (gdouble)(width /2.0);
  half_height = (gdouble)(height /2.0);
  center_x = half_width;
  center_y = half_height;
  l_r = MAX(half_width, half_height);
  l_r *= l_r;

  /* start at the out-most point
   * (may be out of the layer in most cases)
   * and search towards the center along
   * the line with angle alpha
   */
  while(l_r > 0)
  {
    l_y = l_r * sin_alpha;
    l_x = l_r * cos_alpha;
    if((l_x <= half_width)
    && (l_y <= half_height))
    {
      if (((center_x + l_x) >= 0)
      &&  ((center_y + l_y) >= 0))
      {
        /* now we are inside the layer area */
        if (p_pixel_check_opaque(pft
                          ,bpp
                          ,center_x + l_x
                          ,center_y + l_y
                          ))
        {
          break;
        }
      }
    }
    l_r--;
  }

  *px = center_x + l_x;
  *py = center_y + l_y;

}  /* end p_find_outmost_opaque_pixel */



/* -------------------------------------------
 * gap_morph_shape_generate_outline_workpoints
 * -------------------------------------------
 */
void
gap_morph_shape_generate_outline_workpoints(GapMorphGlobalParams *mgpp)
{
  GapMorphWorkPoint *wp;
  GimpPixelFetcher *src_pixfet;
  GimpPixelFetcher *dst_pixfet;
  GimpDrawable *dst_drawable;
  GimpDrawable *src_drawable;
  gdouble alpha_rad;
  gdouble step_rad;
  gint  ii;

  src_drawable = gimp_drawable_get (mgpp->osrc_layer_id);
  dst_drawable = gimp_drawable_get (mgpp->fdst_layer_id);

  src_pixfet = gimp_pixel_fetcher_new (src_drawable, FALSE /*shadow*/);
  dst_pixfet = gimp_pixel_fetcher_new (dst_drawable, FALSE /*shadow*/);

  step_rad =  (2.0 * G_PI) / MAX(1, mgpp->numOutlinePoints);
  alpha_rad = 0.0;

  /* loop from 0 to 360 degree */
  for(ii=0; ii < mgpp->numOutlinePoints; ii++)
  {
     gdouble sx, sy, dx, dy;

     p_find_outmost_opaque_pixel(src_pixfet
                                ,src_drawable->bpp
                                ,alpha_rad
                                ,src_drawable->width
                                ,src_drawable->height
                                ,&sx
                                ,&sy
                                );
     p_find_outmost_opaque_pixel(dst_pixfet
                                ,dst_drawable->bpp
                                ,alpha_rad
                                ,dst_drawable->width
                                ,dst_drawable->height
                                ,&dx
                                ,&dy
                                );

     /* create a new workpoint with sx,sy, dx, dy coords */
     wp = gap_morph_shape_new_workpont(sx ,sy ,dx ,dy);
     wp->next = mgpp->master_wp_list;
     mgpp->master_wp_list = wp;

     alpha_rad += step_rad;
  }
  gimp_pixel_fetcher_destroy (src_pixfet);
  gimp_pixel_fetcher_destroy (dst_pixfet);

  gimp_drawable_detach(src_drawable);
  gimp_drawable_detach(dst_drawable);

}  /* end gap_morph_shape_generate_outline_workpoints */


/* -----------------------------------------------
 * gap_morph_shape_generate_frame_tween_workpoints
 * -----------------------------------------------
 * generate workpoint files (one per frame)
 * for the specified frame range.
 * 
 */
gint32
gap_morph_shape_generate_frame_tween_workpoints(GapAnimInfo *ainfo_ptr
   , GapMorphGlobalParams *mgpp, GtkWidget *masterProgressBar, GtkWidget *progressBar, gboolean *cancelFlagPtr)
{
  GapMorphGUIParams morph_gui_params;
  GapMorphGUIParams *mgup;
  gint32 frameCount;
  gint32 currFrameNr;
  gint32 nextFrameNr;
  gint32 currLayerId;
  gint32 nextLayerId;
  gint32 firstFrameNr;
  gint32 lastFrameNr;
  gdouble framesToProcess;
  

  mgup = &morph_gui_params;
  mgup->mgpp = mgpp;
  mgup->src_win.zoom = 1.0;  /* no zoom available here (use full drawable size for workpoint generation */
  mgup->cancelWorkpointGeneration = FALSE;
  mgup->progressBar = progressBar;  
  mgpp->master_wp_list = NULL;

  gimp_rgb_set(&mgup->pointcolor, 0.1, 1.0, 0.1); /* startup with GREEN pointcolor */
  gimp_rgb_set_alpha(&mgup->pointcolor, 1.0);
  gimp_rgb_set(&mgup->curr_pointcolor, 1.0, 1.0, 0.1); /* startup with YELLOW color */
  gimp_rgb_set_alpha(&mgup->curr_pointcolor, 1.0);

  frameCount = 0;
  nextLayerId = -1;
 
  if(gap_lib_chk_framerange(ainfo_ptr) != 0)
  {
    return(0);
  }

  
  firstFrameNr = CLAMP(mgpp->range_from, ainfo_ptr->first_frame_nr, ainfo_ptr->last_frame_nr);
  currFrameNr = firstFrameNr;
  currLayerId = p_getMergedFrameImageLayer(ainfo_ptr, currFrameNr);
  nextFrameNr = currFrameNr + 1;
  while(TRUE)
  {
    lastFrameNr = MIN(mgpp->range_to, ainfo_ptr->last_frame_nr);
    framesToProcess = MAX(1, (lastFrameNr - currFrameNr) -1);
    
    if (nextFrameNr > lastFrameNr)
    {
      break;
    }
    nextLayerId = p_getMergedFrameImageLayer(ainfo_ptr, nextFrameNr);
    
    if((nextLayerId >= 0) && (currLayerId >= 0))
    {
      char *workpointFileName;
      gboolean success;

      workpointFileName = gap_lib_alloc_fname(ainfo_ptr->basename,
                                      currFrameNr,
                                      "." GAP_MORPH_WORKPOINT_EXTENSION);      

      p_do_master_progress(masterProgressBar
                          , workpointFileName
                          , framesToProcess
                          , (currFrameNr - firstFrameNr)
                          );
   
      success = p_generateWorkpointFileForFramePair(workpointFileName
                              , mgup
                              , cancelFlagPtr
                              , currLayerId
                              , nextLayerId
                              );

      g_free(workpointFileName);
      gap_image_delete_immediate(gimp_drawable_get_image(currLayerId));
      if(!success)
      {
        break;
      }

      currFrameNr = nextFrameNr;
      currLayerId = nextLayerId;
    }
    nextFrameNr++;

  }
  
  if(nextLayerId >= 0)
  {
    gap_image_delete_immediate(gimp_drawable_get_image(nextLayerId));
  }

  return (frameCount);
}  /* end gap_morph_shape_generate_frame_tween_workpoints */



