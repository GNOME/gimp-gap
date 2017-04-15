/* gap_edge_detection.c
 *    by hof (Wolfgang Hofer)
 *
 *   This module implements 2 different edge detection methods.
 *   - One of them [gap_edgeDetection] is used for internal purposes in other GAP features
 *   - The alternative method [gap_edgeDetectionByShiftBlurDiff]  based on shifted and or blured copies is reachable via the Video menu 
 *     and callable via PDB
 *
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


static gboolean      p_call_plug_in_gauss_iir2(gint32 imageId, gint32 layerId, gdouble radiusX, gdouble radiusY);
static gint32        p_createEdgeLayer(gint32 imageId, gint32 activeDrawableId, GapEdgeValues *edvalPtr, 
                         gint32 offset1X, gint32 offset1Y, gint32 offset2X, gint32 offset2Y);


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

  if(gap_debug)
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
 * Stuff for the alternative algorithm: (reachable via GUI dialog)
 *
 * ----------------------------------------------
 * Edge detection via Difference to optional Shifted and Blurred Copy 
 * ----------------------------------------------
 * This can be used same as the Difference of Gaussian Blur (DoG) edge detection
 * that is shiped as one of the GIMP's standard edge detection methods,
 * but provides additional features based on shifting before compare.
 * where shift and or blur may be combined or just use only shifting (when blur radius values are 0.0)
 * or use only blur (when all the shift values are 0)
 *
 */

 /* ---------------------------------
  * p_call_plug_in_gauss_iir2
  * ---------------------------------
  */
static gboolean
p_call_plug_in_gauss_iir2(gint32 imageId, gint32 layerId, gdouble radiusX, gdouble radiusY)
{
  static char     *l_called_proc = "plug-in-gauss-iir2";
  GimpParam       *return_vals;
  int              nreturn_vals;
  
  if(gap_debug)
  {
    printf("calling %s  imageId:%d layerId:%d rX:%f rY:%f\n"
       , l_called_proc
       , (int)imageId
       , (int)layerId
       , (float)radiusX
       , (float)radiusY
       );
  }
 
  return_vals = gimp_run_procedure (l_called_proc,
                                &nreturn_vals,
                                GIMP_PDB_INT32,     GIMP_RUN_NONINTERACTIVE,
                                GIMP_PDB_IMAGE,     imageId,
                                GIMP_PDB_DRAWABLE,  layerId,
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
  * p_createEdgeLayer
  * ---------------------------------
  * returns the layerId of the created edge layer.
  *
  * inserts 3 temporarty layers above the activeDrawableId:
  *
  *   shiftLayer2Id        (top, SUBTRACT mode, optionally shifted & blured)
  *   shiftLayer1Id        (NORMAL mode, optionally shifted & blured)
  *   blackLayerId         (NORMAL mode, filled black)
  *   activeDrawableId     (base layer for the 3 duplicates)
  *
  * and then merges down the shifted layers to the resulting edgeLayer.
  *
  * Note that the black_layer provides "area without edges" information on the borders
  * and provides black background in desired size in the 2nd mergeDown operation.
  */
static gint32
p_createEdgeLayer(gint32 imageId, gint32 activeDrawableId, GapEdgeValues *edvalPtr, 
   gint32 offset1X, gint32 offset1Y, gint32 offset2X, gint32 offset2Y)
{
  gint32 shiftLayer1Id;
  gint32 shiftLayer2Id;
  gint32 blackLayerId;
  gint32 mergedLayerId;
  gint32 edgeLayerId;
  
  if(gap_debug)
  {
    printf("p_createEdgeLayer: START imageId:%d  activeDrawableId:%d offset1(X,Y): (%d %d) offset2(X,Y): (%d %d)\n"
      , (int)imageId
      , (int)activeDrawableId
      , (int)offset1X
      , (int)offset1Y
      , (int)offset2X
      , (int)offset2Y
      );
  }

  gimp_image_set_active_layer(imageId, activeDrawableId); 

  blackLayerId = gimp_layer_copy(activeDrawableId);
  gimp_image_insert_layer (imageId, blackLayerId, 0, -1 /* stackposition -1 above th active drawable */ );
  gimp_item_set_name(blackLayerId, "blackLayer");
    
  /* initial fill the edge base layer with black opaque color */
  gap_layer_clear_to_color(blackLayerId
                             ,0.0 /* red */
                             ,0.0 /* green */
                             ,0.0 /* blue */
                             ,1.0 /* alpha */
                           );
                           
  gimp_image_set_active_layer(imageId, blackLayerId); 
  
  shiftLayer1Id = gimp_layer_copy(activeDrawableId);
  gimp_image_insert_layer (imageId, shiftLayer1Id, 0, -1 /* stackposition -1 above th active blackLayerId */ );
  gimp_layer_set_offsets(shiftLayer1Id, offset1X, offset1Y);
  gimp_item_set_name(shiftLayer1Id, "shiftLayer1");

  if ((edvalPtr->blurRadius1X > 0.0) || (edvalPtr->blurRadius1Y > 0.0))
  {
    p_call_plug_in_gauss_iir2(imageId, shiftLayer1Id, edvalPtr->blurRadius1X, edvalPtr->blurRadius1Y);
  }

  gimp_image_set_active_layer(imageId, shiftLayer1Id); 

  
  shiftLayer2Id = gimp_layer_copy(activeDrawableId);
  gimp_image_insert_layer (imageId, shiftLayer2Id, 0, -1 /* stackposition -1 above th active shiftLayer1Id */ );
  gimp_layer_set_offsets(shiftLayer2Id, offset2X, offset2Y);
  gimp_layer_set_mode(shiftLayer2Id, GIMP_SUBTRACT_MODE);
  gimp_item_set_name(shiftLayer2Id, "shiftLayer2");

  if ((edvalPtr->blurRadius2X > 0.0) || (edvalPtr->blurRadius2Y > 0.0))
  {
    p_call_plug_in_gauss_iir2(imageId, shiftLayer2Id, edvalPtr->blurRadius2X, edvalPtr->blurRadius2Y);
  }




  if(gap_debug)
  {
    printf("p_createEdgeLayer: activeDrawableId:%d blackLayerId:%d shiftLayer1Id:%d shiftLayer2Id:%d\n"
      , (int)activeDrawableId
      , (int)blackLayerId
      , (int)shiftLayer1Id
      , (int)shiftLayer2Id
      );
  }
  
  /* merge down topmost shiftLayer2Id into shiftLayer1Id */
  mergedLayerId = gimp_image_merge_down(imageId, shiftLayer2Id, GIMP_EXPAND_AS_NECESSARY);
  gimp_item_set_name(mergedLayerId, "mergedLayer");

  if(gap_debug)
  {
    printf("p_createEdgeLayer: mergedLayerId=%d\n", mergedLayerId);
  }

  /* further merge down  into blackLayerId */
  edgeLayerId = gimp_image_merge_down(imageId, mergedLayerId, GIMP_CLIP_TO_BOTTOM_LAYER);
  gimp_item_set_name(edgeLayerId, "edgeLayer");

  gimp_image_set_active_layer(imageId, activeDrawableId); 

  return (edgeLayerId);

  
}  /* end p_createEdgeLayer */

 
/* ---------------------------------
 * gap_edgeDetectionByShiftBlurDiff
 * ---------------------------------
 */
gint32
gap_edgeDetectionByShiftBlurDiff(gint32 activeDrawableId, GapEdgeValues *edvalPtr)
{
  gint32 edgeLayerId;
  gint32 edgeHorizontalLayerId;
  gint32 edgeVerticalLayerId;
  gint32 imageId;
  gint32 retLayerId;
  gint   src_offset_x;
  gint   src_offset_y;


  imageId = gimp_item_get_image(activeDrawableId);
  gimp_drawable_offsets(activeDrawableId, &src_offset_x, &src_offset_y);
  
  
  
  edgeHorizontalLayerId = p_createEdgeLayer(imageId, activeDrawableId, edvalPtr 
     , src_offset_x - edvalPtr->shiftLeft   /* offset1X */
     , src_offset_y                         /* offset1Y */
     , src_offset_x + edvalPtr->shiftRight  /* offset2X */
     , src_offset_y                         /* offset2Y */
     );


  edgeVerticalLayerId = -1;
  if ((edvalPtr->shiftUp != 0)
  ||  (edvalPtr->shiftDown != 0)
  ||  (edvalPtr->blurRadius1X > 0.0)
  ||  (edvalPtr->blurRadius1Y > 0.0)
  ||  (edvalPtr->blurRadius2X > 0.0)
  ||  (edvalPtr->blurRadius2Y > 0.0))
  {
  
    edgeVerticalLayerId = p_createEdgeLayer(imageId, activeDrawableId, edvalPtr 
     , src_offset_x                        /* offset1X */
     , src_offset_y - edvalPtr->shiftUp    /* offset1Y */
     , src_offset_x                        /* offset2X */
     , src_offset_y + edvalPtr->shiftDown  /* offset2Y */
     );

    gimp_layer_set_mode(edgeHorizontalLayerId, GIMP_LIGHTEN_ONLY_MODE);

    /* for operation in both directions
     * the layerstack at this point shall look like this:
     *
     *  edgeHorizontalLayerId (topmost)
     *  edgeVerticalLayerId
     *  activeDrawableId      
     *
     */

    
    /* merge edgeHorizontalLayerId down into edgeVerticalLayerId 
     * note that the upper layer edgeHorizontalLayerId has GIMP_LIGHTEN_ONLY_MODE mode
     * so the result has a mix of both horizontal and vertiacal detected edge lines that are brighter
     * than the other areas in both edgeHorizontalLayerId and edgeVerticalLayerId)
     */
    edgeLayerId = gimp_image_merge_down(imageId, edgeHorizontalLayerId, GIMP_CLIP_TO_BOTTOM_LAYER);

  }
  else
  {
    edgeLayerId = edgeHorizontalLayerId;
  }
  
  
  /* postprocessing options */
  if (edvalPtr->autoLevels)
  {
    /* for gimp-2.8.xx use the old name gimp_levels_stretch (that is deprected since gimp-2.9) */
    gimp_levels_stretch(edgeLayerId);
    
    // GIMP-2.9 gimp_drawable_levels_stretch(edgeLayerId);
  }

  if (edvalPtr->desaturate)
  {
    /* for gimp-2.8.xx use the old name gimp_desaturate_full (that is deprected since gimp-2.9) */
    gimp_desaturate_full(edgeLayerId, GIMP_DESATURATE_LIGHTNESS);

    // GIMP-2.9 calls should look like this:
    //  desaturatedDrawableId = gimp_drawable_desaturate(drawable_id
    //                       , cuvals->desaturate_mode
    //                       );
  }

  if (edvalPtr->invert)
  {
    /* for gimp-2.8.xx use the old name gimp_levels_stretch (that is deprected since gimp-2.9) */
    gimp_invert(edgeLayerId);
    
    // GIMP-2.9  gimp_drawable_invert(edgeLayerId);
  }

  if (edvalPtr->createEdgeAsNewLayer)
  {
    retLayerId = edgeLayerId;
  }
  else
  {
    /* in case no new layer is requested replace the active layer */
    gap_layer_copy_content (activeDrawableId,  edgeLayerId /* src_drawable_id */ );
  
    /* and delete the edgeLayerId after copying */
    gimp_image_remove_layer(imageId, edgeLayerId);
    
    retLayerId = activeDrawableId;
  }
  
  
  return (retLayerId);
  
}  /* end gap_edgeDetectionByShiftBlurDiff */
 
