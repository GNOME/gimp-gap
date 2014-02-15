/* gap_edge_detection.c
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

/* SYTEM (UNIX) includes */
#include "string.h"
/* GIMP includes */
/* GAP includes */
#include "gap_lib_common_defs.h"
#include "gap_edge_detection.h"
#include "gap_colordiff.h"
#include "gap_lib.h"
#include "gap_image.h"
#include "gap_layer_copy.h"
#include "gap_libgapbase.h"
#include "gap_edge_detection.h"
#include "gap_pdb_calls.h"

#define OPACITY_LEVEL_UCHAR 50


extern      int gap_debug; /* ==0  ... dont print debug infos */

typedef struct GapEdgeContext { /* nickname: ectx */
     GimpDrawable *refDrawable;
     GimpDrawable *edgeDrawable;
     gdouble       edgeColordiffThreshold;
     gint32        edgeOpacityThreshold255;
     gint32        edgeImageId;
     gint32        edgeDrawableId;
     gint32        countEdgePixels;

     GimpPixelFetcher *pftRef;

  } GapEdgeContext;


/* ----------------------------------
 * p_get_debug_coords_from_guides
 * ----------------------------------
 * get debug coordinaztes from 1st horizontal and vertical guide crossing.
 *
 * note that guides are not relevant for the productive processing
 * but the 1st guide crossing is used to specify
 * a coordinate where debug output shall be printed.
 */
static void
p_get_debug_coords_from_guides(gint32 image_id, gint *cx, gint *cy)
{
  gint32  guide_id;
  gint    guideRow;
  gint    guideCol;

  guide_id = 0;

  guideRow = -1;
  guideCol = -1;

  if(image_id < 0)
  {
     return;
  }


  while(TRUE)
  {
    guide_id = gimp_image_find_next_guide(image_id, guide_id);

    if (guide_id < 1)
    {
       break;
    }
    else
    {
       gint32 orientation;

       orientation = gimp_image_get_guide_orientation(image_id, guide_id);
       if(orientation != 0)
       {
         if(guideCol < 0)
         {
           guideCol = gimp_image_get_guide_position(image_id, guide_id);
         }
       }
       else
       {
         if(guideRow < 0)
         {
           guideRow = gimp_image_get_guide_position(image_id, guide_id);
         }
       }
    }

  }

  *cx = guideCol;
  *cy = guideRow;

  //if(gap_debug)
  {
    printf("image_id:%d  guideCol:%d :%d\n"
       ,(int)image_id
       ,(int)guideCol
       ,(int)guideRow
       );
  }

}  /* end p_get_debug_coords_from_guides */





/* ---------------------------------
 * p_edgeProcessingForOneRegion
 * ---------------------------------
 * render edge drawable for the current processed pixel region.
 * (use a pixelfetcher on region boundaries)
 */
static void
p_edgeProcessingForOneRegion (const GimpPixelRgn *refPR
                    ,const GimpPixelRgn *edgePR
                    ,GapEdgeContext *ectx)
{
  guint    row;
  guchar* ref = refPR->data;
  guchar* edge = edgePR->data;
  guchar  rightPixel[4];
  guchar  botPixel[4];
  guchar  rbPixel[4];
  guchar* rightPixelPtr;
  guchar* botPixelPtr;
  guchar* rbPixelPtr;
  gint32   rx; 
  gint32   ry; 
  gboolean debugPrint;
        
  
  
  if(gap_debug)
  {
    printf("p_edgeProcessingForOneRegion START\n");
  }
  debugPrint = FALSE;
  
  for (row = 0; row < edgePR->h; row++)
  {
    guint  col;
    guint  idxref;
    guint  idxedge;

    ry = refPR->y + row;

    idxref = 0;
    idxedge = 0;
    for(col = 0; col < edgePR->w; col++)
    {
      gdouble  colordiff1;
      gdouble  colordiff2;
      gdouble  colordiff3;
      gdouble  maxColordiff;
      gboolean isColorCompareRequired;

      rbPixelPtr = &ref[idxref];
      
      rx = refPR->x + col;
      
      isColorCompareRequired = TRUE;
        
/*
 *       if(gap_debug)
 *       {
 *         debugPrint = FALSE;
 *         if((rx == 596) || (rx == 597))
 *         {
 *           if((ry==818) ||(ry==818))
 *           {
 *             debugPrint = TRUE;
 *           }
 *         }
 *       }
 */
      
      if(col < edgePR->w -1)
      {
        rightPixelPtr = &ref[idxref + refPR->bpp];

        if(row < edgePR->h -1)
        {
          rbPixelPtr = &ref[idxref + refPR->bpp + refPR->rowstride];
        }
      }
      else if(rx >= ectx->refDrawable->width -1)
      {
         /* drawable border is not considered as edge */
        rightPixelPtr = &ref[idxref];
      }
      else
      {
        rightPixelPtr = &rightPixel[0];
        gimp_pixel_fetcher_get_pixel (ectx->pftRef, rx +1, ry, rightPixelPtr);

        if(ry >= ectx->refDrawable->height -1)
        {
          rbPixelPtr = &rbPixel[0];
          gimp_pixel_fetcher_get_pixel (ectx->pftRef, rx +1, ry +1, rbPixelPtr);
        }
      }
      
      if(row < edgePR->h -1)
      {
        botPixelPtr = &ref[idxref + refPR->rowstride];
      }
      else if(ry >= ectx->refDrawable->height -1)
      {
         /* drawable border is not considered as edge */
        botPixelPtr = &ref[idxref];
      }
      else
      {
        botPixelPtr = &botPixel[0];
        gimp_pixel_fetcher_get_pixel (ectx->pftRef, rx, ry +1, botPixelPtr);
      }

      if(refPR->bpp > 3)
      {
        /* reference drawable has alpha channel
         * in this case significant changes of opacity shall detect edge
         */
        gint32 maxAlphaDiff;
        
        maxAlphaDiff = MAX(abs(ref[idxref +3] - rightPixelPtr[3])
                          ,abs(ref[idxref +3] - botPixelPtr[3]));
        maxAlphaDiff = MAX(maxAlphaDiff
                          ,abs(ref[idxref +3] - rbPixelPtr[3]));
        if(debugPrint)
        {
          printf("rx:%d ry:%d idxref:%d idxedge:%d (maxAlphaDiff):%d  Thres:%d  Alpha ref:%d right:%d bot:%d rb:%d\n"
               , (int)rx
               , (int)ry
               , (int)idxref
               , (int)idxedge
               , (int)maxAlphaDiff
               , (int)ectx->edgeOpacityThreshold255
               , (int)ref[idxref +3]
               , (int)rightPixelPtr[3]
               , (int)botPixelPtr[3]
               , (int)rbPixelPtr[3]
               );
        }
        
        if(maxAlphaDiff > ectx->edgeOpacityThreshold255)
        {
           ectx->countEdgePixels++;
           edge[idxedge] = maxAlphaDiff;
           isColorCompareRequired = FALSE;
        }
        else if(ref[idxref +3] < OPACITY_LEVEL_UCHAR)
        {
          /* transparent pixel is not considered as edge */
          edge[idxedge] = 0;
          isColorCompareRequired = FALSE;
        }
        
      }
      
      
      if(isColorCompareRequired == TRUE)
      {
        
        colordiff1 = gap_colordiff_simple_guchar(&ref[idxref]
                     , &rightPixelPtr[0]
                     , debugPrint    /* debugPrint */
                     );

        colordiff2 = gap_colordiff_simple_guchar(&ref[idxref]
                     , &botPixelPtr[0]
                     , debugPrint    /* debugPrint */
                     );

        colordiff3 = gap_colordiff_simple_guchar(&ref[idxref]
                     , &rbPixelPtr[0]
                     , debugPrint    /* debugPrint */
                     );
        maxColordiff = MAX(colordiff1, colordiff2);
        maxColordiff = MAX(maxColordiff, colordiff3);

        if(debugPrint)
        {
          printf("rx:%d ry:%d colordiff(1): %.5f (2):%.5f (3):%.5f (max):%.5f Thres:%.5f\n"
            , (int)rx
            , (int)ry
            , (float)colordiff1
            , (float)colordiff2
            , (float)colordiff3
            , (float)maxColordiff
            , (float)ectx->edgeColordiffThreshold
            );
        }

        if (maxColordiff < ectx->edgeColordiffThreshold)
        {
            edge[idxedge] = 0;
        }
        else
        {
            gdouble value;

            value = maxColordiff * 255.0;
            edge[idxedge] = CLAMP(value, 1, 255);
            ectx->countEdgePixels++;
        }
      }
       


      idxref += refPR->bpp;
      idxedge += edgePR->bpp;
    }

    ref += refPR->rowstride;
    edge += edgePR->rowstride;

  }

}  /* end p_edgeProcessingForOneRegion */



/* ----------------------------------------
 * p_createEmptyEdgeDrawable
 * ----------------------------------------
 * create the (empty) edge drawable as layer of a new image
 *
 */
static void
p_createEmptyEdgeDrawable(GapEdgeContext *ectx)
{
  ectx->edgeImageId = gimp_image_new(ectx->refDrawable->width
                                   , ectx->refDrawable->height
                                   , GIMP_GRAY
                                   );
  ectx->edgeDrawableId = gimp_layer_new(ectx->edgeImageId
                , "edge"
                , ectx->refDrawable->width
                , ectx->refDrawable->height
                , GIMP_GRAY_IMAGE
                , 100.0   /* full opacity */
                , 0       /* normal mode */
                );

  gimp_image_insert_layer (ectx->edgeImageId, ectx->edgeDrawableId, 0, 0 /* stackposition */ );

  ectx->edgeDrawable = gimp_drawable_get(ectx->edgeDrawableId);
  
}  /* end p_createEmptyEdgeDrawable */


/* ----------------------------------------
 * p_edgeDetection
 * ----------------------------------------
 * setup pixel regions and perform edge detection processing per region.
 * as result of this processing  the edgeDrawable is created and rendered.
 */
static void
p_edgeDetection(GapEdgeContext *ectx)
{
  GimpPixelRgn refPR;
  GimpPixelRgn edgePR;
  gpointer  pr;

  p_createEmptyEdgeDrawable(ectx);
  if(ectx->edgeDrawable == NULL)
  {
    return;
  }
  

  gimp_pixel_rgn_init (&refPR, ectx->refDrawable, 0, 0
                      , ectx->refDrawable->width, ectx->refDrawable->height
                      , FALSE     /* dirty */
                      , FALSE     /* shadow */
                       );

  gimp_pixel_rgn_init (&edgePR, ectx->edgeDrawable, 0, 0
                      , ectx->edgeDrawable->width, ectx->edgeDrawable->height
                      , TRUE     /* dirty */
                      , FALSE    /* shadow */
                       );

  /* compare pixel areas in tiled portions via pixel region processing loops.
   */
  for (pr = gimp_pixel_rgns_register (2, &refPR, &edgePR);
       pr != NULL;
       pr = gimp_pixel_rgns_process (pr))
  {
    p_edgeProcessingForOneRegion (&refPR, &edgePR, ectx);
  }
  gimp_drawable_flush (ectx->edgeDrawable);
}



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
  )
{
  GapEdgeContext  edgeContext;
  GapEdgeContext *ectx;
  gdouble         edgeOpacityThreshold;

  /* init context */  
  ectx = &edgeContext;
  ectx->refDrawable = gimp_drawable_get(refDrawableId);
  ectx->edgeDrawable = NULL;
  ectx->edgeColordiffThreshold = CLAMP(edgeColordiffThreshold, 0.0, 1.0);
  edgeOpacityThreshold = CLAMP((edgeColordiffThreshold * 255), 0, 255);
  ectx->edgeOpacityThreshold255 = edgeOpacityThreshold;
  ectx->edgeDrawableId = -1;
  ectx->countEdgePixels = 0;

  if(gap_debug)
  {
    printf("gap_edgeDetection START edgeColordiffThreshold:%.5f refDrawableId:%d\n"
       , (float)ectx->edgeColordiffThreshold
       , (int)refDrawableId
       );
  }
 
  if(ectx->refDrawable != NULL)
  {
    ectx->pftRef = gimp_pixel_fetcher_new (ectx->refDrawable, FALSE /* shadow */);
    gimp_pixel_fetcher_set_edge_mode (ectx->pftRef, GIMP_PIXEL_FETCHER_EDGE_BLACK);

    p_edgeDetection(ectx);

    gimp_pixel_fetcher_destroy (ectx->pftRef);
  }

  if(ectx->refDrawable != NULL)
  {
    gimp_drawable_detach(ectx->refDrawable);
    ectx->refDrawable = NULL;
  }
  if(ectx->edgeDrawable != NULL)
  {
    gimp_drawable_detach(ectx->edgeDrawable);
    ectx->edgeDrawable = NULL;
  }

  *countEdgePixels = ectx->countEdgePixels;

  if(gap_debug)
  {
    printf("gap_edgeDetection END resulting edgeDrawableId:%d countEdgePixels:%d\n"
       , (int)ectx->edgeDrawableId
       , (int)*countEdgePixels
       );
  }
  
  return (ectx->edgeDrawableId);
  
}  /* end gap_edgeDetection */




/*
 * Stuff for the alternative algorithm:
 *
 * ----------------------------------------------
 * Edge detection via Difference to Blurred Copy 
 * ----------------------------------------------
 * 
 */
 
 
 /* ---------------------------------
  * p_colordiffProcessingForOneRegion
  * ---------------------------------
  * subtract RGB channels of the refPR from edgePR
  * and set edgePR pixels to desaturated result of the subtraction.
  * (desaturation is done by lightness)
  */
 static void
 p_colordiffProcessingForOneRegion (const GimpPixelRgn *edgePR
                     , const GimpPixelRgn *refPR
                     , const GimpPixelRgn *ref2PR
                     , gdouble threshold01f, gboolean invert
                     , gint cx, gint cy
                     )
 {
   guint    row;
   guchar* ref = refPR->data;
   guchar* ref2 = ref2PR->data;
   guchar* edge = edgePR->data;
   gdouble colordiff;
   
   if(gap_debug)
   {
     printf("p_colordiffProcessingForOneRegion START Edge:w:%d h:%d x:%d y:%d  Ref:w:%d h:%d x:%d y:%d \n"
        ,edgePR->w
        ,edgePR->h
        ,edgePR->x
        ,edgePR->y
        ,refPR->w
        ,refPR->h
        ,refPR->x
        ,refPR->y
         );
   }
   
   for (row = 0; row < edgePR->h; row++)
   {
     guint  col;
     guint  idxref;
     guint  idxref2;
     guint  idxedge;
 
     idxref = 0;
     idxref2 = 0;
     idxedge = 0;
     for(col = 0; col < edgePR->w; col++)
     {
       gint value;
       gboolean  debugPrint;
       gdouble colordiff1;
       gdouble colordiff2;
 
       debugPrint = FALSE;
       
       if((cx == edgePR->x + col)
       && (cy == edgePR->y + row))
       {
         debugPrint = TRUE;
         printf("threshold01f:%.4f\n", (float)threshold01f);
       }
       
//        colordiff = gap_colordiff_simple_guchar(&ref[idxref]
//                      , &edge[idxref]
//                      , debugPrint    /* debugPrint */
//                      );
//        colordiff = gap_colordiff_guchar(&ref[idxref]
//                             ,  &edge[idxref]
// 			    , 1.15            /* gdouble color sensitivity 1.0 to 2.0 */
//                             , debugPrint
//                             );
       colordiff1 = gap_colordiff_hvmax_guchar(&ref[idxref]
                   , &edge[idxref]
                   , debugPrint
                   );
       colordiff2 = gap_colordiff_hvmax_guchar(&ref2[idxref]
                   , &edge[idxref]
                   , debugPrint
                   );
       colordiff = MAX(colordiff1, colordiff2);          
       value = 0;
       if(colordiff > threshold01f)
       {
         gdouble valuef;
         
         valuef = colordiff * 255.0;
         value = CLAMP(valuef, 0, 255);
       }

       if (invert)
       {
         value = 255 - value;
       }
       
       if(debugPrint)
       {
         printf("value: %d\n"
               ,(int)value
               );
       }
       
       edge[idxedge]    = value;
       edge[idxedge +1] = value;
       edge[idxedge +2] = value;
 
 
       idxref += refPR->bpp;
       idxref2 += ref2PR->bpp;
       idxedge += edgePR->bpp;
     }
 
     ref += refPR->rowstride;
     ref2 += ref2PR->rowstride;
     edge += edgePR->rowstride;
 
   }
 
 
 }  /* end p_colordiffProcessingForOneRegion */
 
 
 /* ----------------------------------------
  * p_subtract_ref_layer
  * ----------------------------------------
  * setup pixel regions and perform edge detection by subtracting RGB channels 
  * of the orignal (refDrawable) from the blurred copy (edgeDrawable)
  * and convert the rgb differences to lightness.
  *
  * as result of this processing in the edgeDrawable contains a desaturated
  * colordifference of the original versus blured copy.
  */
 static void
 p_subtract_ref_layer(gint32 image_id, GimpDrawable *edgeDrawable, GimpDrawable *refDrawable
   , gdouble threshold, gint32 shift, gboolean invert)
 {
   GimpPixelRgn edgePR;
   GimpPixelRgn refPR;
   GimpPixelRgn ref2PR;
   gpointer  pr;
   gdouble   threshold01f;
   gdouble   threshold255f;
   gint      threshold255;
   gint      cx;
   gint      cy;
   
   threshold01f = CLAMP((threshold / 100.0), 0, 1);
   threshold255f = 255.0 * threshold01f;
   threshold255 = threshold255f;

   p_get_debug_coords_from_guides(image_id, &cx, &cy);
   
   gimp_pixel_rgn_init (&edgePR, edgeDrawable, 0, 0
                       , edgeDrawable->width - shift, edgeDrawable->height - shift
                       , TRUE     /* dirty */
                       , FALSE    /* shadow */
                        );

   /* start at shifted offset 0/+1 */ 
   gimp_pixel_rgn_init (&refPR, refDrawable, 0, shift
                       , refDrawable->width - shift, refDrawable->height - shift
                       , FALSE     /* dirty */
                       , FALSE     /* shadow */
                        );
   /* start at shifted offset +1/0 */ 
   gimp_pixel_rgn_init (&ref2PR, refDrawable, shift, 0
                       , refDrawable->width - shift, refDrawable->height - shift
                       , FALSE     /* dirty */
                       , FALSE     /* shadow */
                        );
 
   /* compare pixel areas in tiled portions via pixel region processing loops.
    */
   for (pr = gimp_pixel_rgns_register (3, &edgePR, &refPR, &ref2PR);
        pr != NULL;
        pr = gimp_pixel_rgns_process (pr))
   {
     p_colordiffProcessingForOneRegion (&edgePR, &refPR, &ref2PR, threshold01f, invert, cx, cy);
   }
 
   gimp_drawable_flush (edgeDrawable);
   gimp_drawable_update (edgeDrawable->drawable_id
                          , 0, 0
                          , edgeDrawable->width, edgeDrawable->height
                          );
 
 }  /* end  p_subtract_ref_layer */
 
 
 
 /* ---------------------------------
  * p_call_plug_in_gauss_iir2
  * ---------------------------------
  */
 gboolean
 p_call_plug_in_gauss_iir2(gint32 imageId, gint32 edgeLayerId, gdouble radiusX, gdouble radiusY)
 {
    static char     *l_called_proc = "plug-in-gauss-iir2";
    GimpParam       *return_vals;
    int              nreturn_vals;
 
    return_vals = gimp_run_procedure (l_called_proc,
                                  &nreturn_vals,
                                  GIMP_PDB_INT32,     GIMP_RUN_NONINTERACTIVE,
                                  GIMP_PDB_IMAGE,     imageId,
                                  GIMP_PDB_DRAWABLE,  edgeLayerId,
                                  GIMP_PDB_FLOAT,     radiusX,
                                  GIMP_PDB_FLOAT,     radiusY,
                                  GIMP_PDB_END);
 
    if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    {
       gimp_destroy_params(return_vals, nreturn_vals);
       return (TRUE);   /* OK */
    }
    gimp_destroy_params(return_vals, nreturn_vals);
    printf("GAP: Error: PDB call of %s failed, d_status:%d %s\n"
       , l_called_proc
       , (int)return_vals[0].data.d_status
       , gap_status_to_string(return_vals[0].data.d_status)
       );
    return(FALSE);
 }       /* end p_call_plug_in_gauss_iir2 */
 
 
 
 /* ---------------------------------
  * gap_edgeDetectionByBlurDiff
  * ---------------------------------
  */
 gint32
 gap_edgeDetectionByBlurDiff(gint32 activeDrawableId, gdouble blurRadius, gdouble blurResultRadius
   , gdouble threshold, gint32 shift, gboolean doLevelsAutostretch
   , gboolean invert)
 {
   gint32 blurLayerId;
   gint32 edgeLayerId;
   gint32 imageId;
   GimpDrawable *edgeDrawable;
   GimpDrawable *refDrawable;
   GimpDrawable *blurDrawable;

 
 
 
   imageId = gimp_item_get_image(activeDrawableId);
 
   edgeLayerId = gimp_layer_copy(activeDrawableId);
   gimp_image_insert_layer (imageId, edgeLayerId, 0, 0 /* stackposition */ );
 
   edgeDrawable = gimp_drawable_get(edgeLayerId);
   refDrawable = gimp_drawable_get(activeDrawableId);
 
   if(blurRadius > 0.0)
   {
     p_call_plug_in_gauss_iir2(imageId, edgeLayerId, blurRadius, blurRadius);
   }
   
   blurDrawable = NULL;
//    blurLayerId = gimp_layer_copy(edgeLayerId);
//    gimp_image_insert_layer (imageId, blurLayerId, 0, 0 /* stackposition */ );
//    blurDrawable = gimp_drawable_get(blurLayerId);

   p_subtract_ref_layer(imageId, edgeDrawable, refDrawable, threshold, shift, invert);
   //p_subtract_ref_layer(imageId, edgeDrawable, blurDrawable, threshold, shift, invert);
   if (doLevelsAutostretch)
   {
     gimp_levels_stretch(edgeLayerId);
   }

   if(blurResultRadius > 0.0)
   {
     p_call_plug_in_gauss_iir2(imageId, edgeLayerId, blurResultRadius, blurResultRadius);
   }
 
   if(refDrawable)
   {
     gimp_drawable_detach(refDrawable);
   }
   
   if(edgeDrawable)
   {
     gimp_drawable_detach(edgeDrawable);
   }
   if(blurDrawable)
   {
     gimp_drawable_detach(blurDrawable);
   }
 
   return (edgeLayerId);
   
 }  /* end gap_edgeDetectionByBlurDiff */
 
 
