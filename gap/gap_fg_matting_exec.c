/*  gap_fg_matting_exec.c
 *    foreground extraction based on alpha matting algorithm.
 *    Render transparancy for a layer based on a tri-map drawable (provided by the user).
 *    The tri-map defines what pixels are FOREGROUND (white) BACKGROUND (black) or UNDEFINED (gray).
 *    foreground extraction affects the UNDEFINED pixels and calculates transparency for those pixels.
 *
 *    This module provides the top level processing procedures
 *    and does some pre and postprocessing on the tri-map input.
 *  2011/10/05
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

/* Revision history
 *  (2011/10/05)  2.7.0       hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_fg_matting_main.h"
#include "gap_fg_matting_exec.h"
#include "gap_fg_tile_manager.h"
#include "gap_fg_matting_dialog.h"
#include "gap_fg_matting.h"
#include "gap_fg_regions.h"

#include "gap-intl.h"


static gboolean globalDoProgress;


static gint32 gap_drawable_foreground_extract (GimpDrawable              *drawable,
                                 GimpDrawable              *maskDrawable,
                                 GapFgExtractValues        *fgValPtr
                                 );
static GappMattingState * gap_drawable_foreground_extract_matting_init (GimpDrawable *drawable,
                                               gint          mask_x,
                                               gint          mask_y,
                                               gint          mask_width,
                                               gint          mask_height);
static void  gap_drawable_foreground_extract_matting (GimpDrawable       *maskDrawable,
                                         GimpDrawable       *resultDrawable,
                                         GappMattingState   *state,
                                         gfloat              start_percentage
                                         );
static void  gap_drawable_foreground_extract_matting_done (GappMattingState *state);

static gint32  p_tri_map_preprocessing (GimpDrawable *drawable, GapFgExtractValues *fgValPtr, gint32 *dummyLayerId);



/* ---------------------------------
 * p_progressFunc
 * ---------------------------------
 * MattingProgressFunc
 *   progress callback function.
 */
static void
p_progressFunc(gpointer progress_data, gdouble   fraction)
{
  if(globalDoProgress)
  {
    gimp_progress_update(fraction);
  }
}  /* end p_progressFunc */



/* -----------------------------
 * gap_fg_matting_exec_apply_run
 * -----------------------------
 * handle undo, progress init and
 * apply the processing functions
 */
gint32
gap_fg_matting_exec_apply_run (gint32 image_id, gint32 drawable_id
     , gboolean doProgress,  gboolean doFlush
     , GapFgExtractValues *fgValPtr)
{
  gint32           retLayerId;
  gint32           oldLayerMaskId;
  gint32           triMapOrig;
  gint32           triMapRelevant;
  gint32           dummyLayerId;
  GimpDrawable    *maskDrawable;
  GimpDrawable    *activeDrawable;

  /* dont operate on other drawables than layers */
  g_return_val_if_fail (gimp_drawable_is_layer(drawable_id), -1);

  globalDoProgress = doProgress;
  triMapOrig = fgValPtr->tri_map_drawable_id;
  /* try to use layermask as tri-mask
   * in case negative id was specified by the caller
   */
  oldLayerMaskId = gimp_layer_get_mask(drawable_id);
  if (fgValPtr->tri_map_drawable_id < 0)
  {
    fgValPtr->tri_map_drawable_id = oldLayerMaskId;
  }

  gimp_image_undo_group_start (image_id);

  if (doProgress)
  {
    gimp_progress_init (_("Foreground Extract"));
  }

  activeDrawable = gimp_drawable_get (drawable_id);

  triMapRelevant = p_tri_map_preprocessing (activeDrawable, fgValPtr, &dummyLayerId);

  maskDrawable = gimp_drawable_get(triMapRelevant);
  if (maskDrawable == NULL)
  {
    gimp_image_undo_group_end (image_id);
    gimp_drawable_detach (activeDrawable);

    printf("ERROR: no valid tri-mask was specified\n");

    return (-1);
  }


  if(gap_debug)
  {
    printf("gap_fg_matting_exec_apply_run START: drawableID:%d triMapOrig:%d tri_map_drawable_id:%d triMapRelevant:%d\n"
           "    create_layermask:%d lock_color:%d "
           " create_result:%d colordiff_threshold:%.5f"
           "\n"
       , (int)drawable_id
       , (int)triMapOrig
       , (int)fgValPtr->tri_map_drawable_id
       , (int)triMapRelevant
       , (int)fgValPtr->create_layermask
       , (int)fgValPtr->lock_color
       , (int)fgValPtr->create_result
       , (float)fgValPtr->colordiff_threshold
       );
  }

  retLayerId = gap_drawable_foreground_extract (activeDrawable, maskDrawable, fgValPtr);

  gimp_drawable_detach (maskDrawable);


  if(dummyLayerId >= 0)
  {
    if(gap_debug)
    {
      printf("removing dummy layer:%d\n"
        ,(int) dummyLayerId
        );
    }
    gimp_image_remove_layer(image_id, dummyLayerId);
  }

  if (doProgress)
  {
    gimp_progress_update(1.0);
  }

  gimp_image_undo_group_end (image_id);

  if(gap_debug)
  {
    printf("gap_fg_matting_exec_apply_run DONE: retLayerId:%d\n"
       , (int)retLayerId
       );
  }

  if (doFlush != FALSE)
  {
    gimp_drawable_flush (activeDrawable);
  }

  gimp_drawable_detach (activeDrawable);

  return (retLayerId);
}       /* end gap_fg_matting_exec_apply_run */











/* -----------------------
 * p_tri_map_preprocessing
 * -----------------------
 * prepare the tri mask for processing
 * - have bpp == 1
 * - have same size and offset as the input drawable (that is a layer)
 * - pixel values >= 240 are set to value 240 (MATTING_USER_FOREGROUND)
 * - in case the input layer already has an alpha channel
 *   all fully transparent (alpha == 0) pixels are also set 0 (MATTING_USER_BACKGROUND)
 *   in the tri map to keep pixels fully transparent. (such pixels typicall do not
 *   have a useful color information in the RGB channels)
 *
 *
 * in case the user provided the tri map as layer or channel that is NOT the layermask of the input layer
 * we create a new dummy layer with same size and offset as the input layer
 * and add a layermask to this dummy layer.
 * The layermask of the dummy layer is then filled with the intersecting grayscale copy
 * of the user-provided triMap drawable and will be used as tri map in the alpha matting processing.
 *
 * returns the dawable Id of the relevant TRI MAP that fulfills the conditons listed above.
 */
static gint32
p_tri_map_preprocessing (GimpDrawable *drawable, GapFgExtractValues *fgValPtr, gint32 *dummyLayerId)
{
  gint32           prepocessedTriMapLayerId;
  gint32           inputLayerMaskId;
  gint32           imageId;

  *dummyLayerId = -1;
  imageId = gimp_item_get_image(drawable->drawable_id);


  inputLayerMaskId = gimp_layer_get_mask(drawable->drawable_id);
  if (fgValPtr->tri_map_drawable_id == inputLayerMaskId)
  {
    prepocessedTriMapLayerId = inputLayerMaskId;
  }
  else
  {
    gint          offset_x;
    gint          offset_y;
    gint32        dummyLayerMaskId;
    gint32        l_fsel_layer_id;

    *dummyLayerId = gimp_layer_new(imageId
            , "DUMMY"
            , drawable->width
            , drawable->height
            , GIMP_RGBA_IMAGE
            , 100.0   /* full opacity */
            , GIMP_NORMAL_MODE       /* normal mode */
            );

    /* get offsets of the input drawable (layer) within the image */
    gimp_drawable_offsets (drawable->drawable_id, &offset_x, &offset_y);

    /* add dummy layer (of same size at same offsets) to the same image */
    gimp_image_insert_layer(imageId, *dummyLayerId, 0, -1 /* stackposition */ );
    gimp_layer_set_offsets(*dummyLayerId, offset_x, offset_y);

    /* create a new layermask (black is full transparent */
    dummyLayerMaskId = gimp_layer_create_mask(*dummyLayerId, GIMP_ADD_BLACK_MASK);
    gimp_layer_add_mask(*dummyLayerId, dummyLayerMaskId);

    gimp_edit_copy(fgValPtr->tri_map_drawable_id);
    l_fsel_layer_id = gimp_edit_paste(dummyLayerMaskId, FALSE);
    gimp_floating_sel_anchor(l_fsel_layer_id);

    prepocessedTriMapLayerId = dummyLayerMaskId;
  }

  gap_fg_rgn_tri_map_normalize(drawable, prepocessedTriMapLayerId);

  return(prepocessedTriMapLayerId);

}       /* end p_tri_map_preprocessing */



/* -------------------------------
 * gap_drawable_foreground_extract
 * -------------------------------
 * perform foreground extraction.
 * This is done in 3 steps INIT, EXTRACTION, DONE (e.g. cleanup)
 */
static gint32
gap_drawable_foreground_extract (GimpDrawable              *drawable,
                                 GimpDrawable              *maskDrawable,
                                 GapFgExtractValues        *fgValPtr
                                 )
{
  GappMattingState *state;
  gint32            resultLayerId;
  gint              mask_offset_x;
  gint              mask_offset_y;

  resultLayerId = -1;

  /* get mask offsets within the image
   *  - in case mask is a channel offsets are always 0)
   *  - in case mask is a layer retieve its offsets
   */
  gimp_drawable_offsets (drawable->drawable_id, &mask_offset_x, &mask_offset_y);

  state = gap_drawable_foreground_extract_matting_init (drawable,
        mask_offset_x, mask_offset_y,
        maskDrawable->width,
        maskDrawable->height
        );

  if (state)
  {
    gint32 imageId;

    imageId = gimp_item_get_image(drawable->drawable_id);
    resultLayerId = gimp_layer_new(imageId
            , "FG"
            , drawable->width
            , drawable->height
            , GIMP_RGBA_IMAGE
            , 100.0   /* full opacity */
            , GIMP_NORMAL_MODE       /* normal mode */
            );
    if (resultLayerId != -1)
    {
      GimpDrawable *resultDrawable;

      resultDrawable = gimp_drawable_get(resultLayerId);
      if (resultDrawable != NULL)
      {
        gint          offset_x;
        gint          offset_y;
        gboolean      resultUpdateRequired;


        resultUpdateRequired = FALSE;

        /* get offsets of the input drawable (layer) within the image */
        gimp_drawable_offsets (drawable->drawable_id, &offset_x, &offset_y);

        /* add resulting layer (of same size at same offsets) to the same image */
        gimp_image_insert_layer(imageId, resultLayerId, 0, -1 /* stackposition */ );
        gimp_layer_set_offsets(resultLayerId, offset_x, offset_y);

        /* perform the foreground extraction */
        gap_drawable_foreground_extract_matting (maskDrawable, resultDrawable, state,
            0.0 // start_percentage 0 (non-interactive shall always start rendering immediate
            );

        /* clean up after fg extract is done */
        gap_drawable_foreground_extract_matting_done (state);

        /* postprocessing */
        if (fgValPtr->lock_color)
        {
          if(gap_debug)
          {
            printf("postprocessing: restore resultDrawable:%d RGB channels from original drawable:%d\n"
               ,(int)resultDrawable->drawable_id
               ,(int)drawable->drawable_id
               );
          }
          resultUpdateRequired = TRUE;
          gap_fg_rgn_copy_rgb_channels(drawable, resultDrawable);
        }

        if (fgValPtr->create_layermask)
        {
          gint32 resultLayerMaskId;

          /* create a new layermask (from alpha channel) */
          resultLayerMaskId = gimp_layer_create_mask(resultLayerId, GIMP_ADD_ALPHA_MASK);
          gimp_layer_add_mask(resultLayerId, resultLayerMaskId);
          if (gimp_drawable_has_alpha(drawable->drawable_id))
          {
            if(gap_debug)
            {
              printf("postprocessing: restore resultDrawable:%d ALPHA channel from original drawable:%d\n"
               ,(int)resultDrawable->drawable_id
               ,(int)drawable->drawable_id
               );
            }
            /* copy the original alpha channel to the result layer */
            gap_fg_rgn_copy_alpha_channel(drawable, resultDrawable);
            resultUpdateRequired = TRUE;
          }
        }

        if(resultUpdateRequired)
        {
          gimp_drawable_update (resultDrawable->drawable_id
                               , 0, 0
                               , resultDrawable->width, resultDrawable->height
                               );
        }


        gimp_drawable_detach (resultDrawable);
      }
    }
  }

  return (resultLayerId);

}  /* end gap_drawable_foreground_extract */



/* --------------------------------------------
 * gap_drawable_foreground_extract_matting_init
 * --------------------------------------------
 *
 */
static GappMattingState *
gap_drawable_foreground_extract_matting_init (GimpDrawable *drawable,
                                               gint          mask_x,
                                               gint          mask_y,
                                               gint          mask_width,
                                               gint          mask_height)
{
  GappMattingState *state;
  GappTileManager *tm;
  const guchar *colormap = NULL;
  gboolean      intersect;
  gint          offset_x;
  gint          offset_y;
  gint          is_x, is_y, is_width, is_height;

  if(gap_debug)
  {
    printf("init: START\n");
  }

  /* get offsets within the image */
  gimp_drawable_offsets (drawable->drawable_id, &offset_x, &offset_y);

  intersect = gimp_rectangle_intersect (offset_x, offset_y,
                                        drawable->width,
                                        drawable->height,
                                        mask_x, mask_y, mask_width, mask_height,
                                        &is_x, &is_y, &is_width, &is_height);


  /* FIXME:
   * Clear the mask outside the rectangle that we are working on?
   */

  if (! intersect)
  {
    return NULL;
  }

  tm = gapp_tile_manager_new(drawable);

  state = matting_init (tm, colormap,
                       offset_x, offset_y,
                       is_x, is_y, is_width, is_height);
  if(gap_debug)
  {
    printf("init: END state:%d\n"
      ,(int) state
      );
  }

  return (state);


}  /* end gap_drawable_foreground_extract_matting_init */


/* ---------------------------------------
 * gap_drawable_foreground_extract_matting
 * ---------------------------------------
 *
 */
static void
gap_drawable_foreground_extract_matting (GimpDrawable       *maskDrawable,
                                         GimpDrawable       *resultDrawable,
                                         GappMattingState   *state,
                                         gfloat              start_percentage
                                         )
{
  GappTileManager *tmMask;
  GappTileManager *tmResult;
  gint x1, y1;
  gint x2, y2;
  gpointer progress_data;
  gdouble  progressDataUnused;

  if(gap_debug)
  {
    printf("extract_matting: START\n");
  }

  g_return_if_fail (state != NULL);

  progress_data = &progressDataUnused;

  x1 = 0;
  y1 = 0;
  x2 = maskDrawable->width;
  y2 = maskDrawable->height;

  tmMask = gapp_tile_manager_new(maskDrawable);
  tmResult = gapp_tile_manager_new(resultDrawable);

  matting_foreground_extract (state,
                              tmMask, x1, y1, x2, y2,
                              start_percentage,
                              (MattingProgressFunc) p_progressFunc,
                              progress_data,
                              tmResult);


  gimp_drawable_update (resultDrawable->drawable_id, x1, y1, x2, y2);
  gimp_drawable_update (maskDrawable->drawable_id, x1, y1, x2, y2);

  if(gap_debug)
  {
    printf("extract_matting: END\n");
  }

}  /* end gap_drawable_foreground_extract_matting */


/* --------------------------------------------
 * gap_drawable_foreground_extract_matting_done
 * --------------------------------------------
 *
 */
static void
gap_drawable_foreground_extract_matting_done (GappMattingState *state)
{
  if(gap_debug)
  {
    printf("matting_cleanup: START\n");
  }

  g_return_if_fail (state != NULL);


  matting_done (state);

  if(gap_debug)
  {
    printf("matting_cleanup: END\n");
  }

}  /* end gap_drawable_foreground_extract_matting_done */



/* --------------------------------------------
 * gap_fg_from_selection_exec_apply_run
 * --------------------------------------------
 * generate a tri map from the current selection by filling the shrinked
 * shape with white, the expanded shape with black and the borderline
 * between shrinked and expanded selection with medium gray.
 * the trimap is attached as layermask to the input drawable,
 * and used as input for the foreground selection via alpha matting algorithm,
 * that creates a resulting layer with trimmed selection.
 *
 */
gint
gap_fg_from_selection_exec_apply_run (gint32 image_id, gint32 drawable_id
                             , gboolean doProgress, gboolean doFlush
                             , GapFgSelectValues *fsValPtr)
{
  GimpRGB   color;
  gint32    activeSelection;
  gint32    shrinkedSelection;
  gint32    trimap;
  gboolean  hadSelection;
  gint      rc;

  rc = 0;
  trimap = -1;
  activeSelection = -1;
  shrinkedSelection = -1;
  hadSelection = FALSE;
  
  gimp_context_push();
  gimp_image_undo_group_start(image_id);
  

  if (gimp_selection_is_empty(image_id) == TRUE)
  {
     if (gimp_drawable_has_alpha(drawable_id) == FALSE)
     {
       /* if the layer has no alpha select all */
       gimp_selection_all(image_id);
     }
     else
     {
       gimp_selection_layer_alpha(drawable_id);
     }
     activeSelection = gimp_selection_save(image_id);
  }
  else
  {
    activeSelection = gimp_selection_save(image_id);
    hadSelection = TRUE;
  }


  trimap = gimp_layer_get_mask(drawable_id);
  if (trimap < 0)
  {
    /* create trimap as new layermask */
    trimap = gimp_layer_create_mask(drawable_id, GIMP_ADD_BLACK_MASK);
    gimp_layer_add_mask(drawable_id, trimap);
  }
  else
  {
    /* use BLACK color to fill the already existing layermask
     * (note that gimp_drawable_fill is used to fill the entire mask
     * regardless to the current selection)
     */
    color.r = 0.0;
    color.g = 0.0;
    color.b = 0.0;
    color.a = 1.0;
    gimp_context_set_background (&color);
    gimp_drawable_fill(trimap, GIMP_BACKGROUND_FILL);
  }
  
  gimp_selection_sharpen(image_id);
  if (fsValPtr->innerRadius > 0)
  {
    gimp_selection_shrink(image_id, fsValPtr->innerRadius);
  }
  shrinkedSelection = gimp_selection_save(image_id);
  
  
  /* use WHITE color to mark foreground regions
   */
  color.r = 1.0;
  color.g = 1.0;
  color.b = 1.0;
  color.a = 1.0;
  gimp_context_set_background (&color);
  gimp_edit_fill(trimap, GIMP_BACKGROUND_FILL);
  
  gimp_image_select_item(image_id, GIMP_CHANNEL_OP_REPLACE, activeSelection);
  gimp_selection_sharpen(image_id);
  if (fsValPtr->outerRadius > 0)
  {
    gimp_selection_grow(image_id, fsValPtr->outerRadius);
  }
  gimp_selection_combine(shrinkedSelection, GIMP_CHANNEL_OP_SUBTRACT);

  /* use medium GRAY to mark undefined regions
   */
  color.r = 0.5;
  color.g = 0.5;
  color.b = 0.5;
  color.a = 1.0;
  gimp_context_set_background (&color);
  gimp_edit_fill(trimap, GIMP_BACKGROUND_FILL);

  gimp_selection_none(image_id);

  /* perform the foreground selection (that creates the resulting layer) */
  {
    GapFgExtractValues fgExtractValues;
    GapFgExtractValues *fgValPtr;
    
    fgValPtr = &fgExtractValues;
    fgValPtr->input_drawable_id = drawable_id;
    fgValPtr->tri_map_drawable_id = trimap;
    fgValPtr->create_result = TRUE;
    fgValPtr->create_layermask = fsValPtr->create_layermask;
    fgValPtr->lock_color = fsValPtr->lock_color;
    fgValPtr->colordiff_threshold = fsValPtr->colordiff_threshold;

    rc = gap_fg_matting_exec_apply_run (image_id, drawable_id
                                 , doProgress, doFlush
                                 , fgValPtr
                                 );
  }
  
  
  /* restore original selection */
  if (hadSelection == TRUE)
  {
    gimp_image_select_item(image_id, GIMP_CHANNEL_OP_REPLACE, activeSelection);
  }

  gimp_image_undo_group_end(image_id);
  gimp_context_pop();
  
  return (rc);

}  /* end gap_fg_from_selection_exec_apply_run */
