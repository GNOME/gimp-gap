/* gap_mov_render.c
 * 1997.11.06 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * Render utility Procedures for GAP MovePath
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
 * gimp    1.3.21b; 2003/09/22  hof: bugfix in selection handling
 * gimp    1.3.20d; 2003/09/14  hof: new: added bluebox stuff
 * gimp    1.3.20c; 2003/09/28  hof: new features: perspective transformation, tween_layer and trace_layer
 *                                   changed opacity, rotation and resize from int to gdouble
 * gimp    1.3.20a; 2003/09/14  hof: fixed compiler warnings
 * gimp    1.3.14a; 2003/05/24  hof: created (splitted off from gap_mov_dialog)
 */

#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

/* GAP includes */
#include "gap_layer_copy.h"
#include "gap_lib.h"
#include "gap_image.h"
#include "gap_mov_exec.h"
#include "gap_mov_dialog.h"
#include "gap_mov_render.h"
#include "gap_pdb_calls.h"
#include "gap_vin.h"
#include "gap_arr_dialog.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */

static void p_mov_selection_handling(gint32 orig_layer_id
                  , gint src_offset_x
                  , gint src_offset_y
                  , GapMovValues *val_ptr
                  , GapMovCurrent *cur_ptr
                  );
static void p_mov_apply_bluebox(gint32 layer_id
                  , GapMovValues *val_ptr
                  , GapMovCurrent *cur_ptr
                  );
static void  p_mov_transform_perspective(gint32 layer_id
                  , GapMovValues *val_ptr
                  , GapMovCurrent *cur_ptr
                  , gint         *resized_flag
                  , guint        *new_width
                  , guint        *new_height
                  );

static void  p_mov_calculate_scale_factors(gint32 image_id, GapMovValues *val_ptr, GapMovCurrent *cur_ptr
                  , guint orig_width, guint orig_height
                  , gdouble *retScaleWidthPercent
                  , gdouble *retScaleheightPercent
                  );


#define BOUNDS(a,x,y)  ((a < x) ? x : ((a > y) ? y : a))

/* -------------------------
 * p_get_paintmode
 * -------------------------
 *
 */
static GimpLayerModeEffects
p_get_paintmode(int mode, gint32 src_layer_id)
{
  if(mode == GAP_MOV_KEEP_SRC_PAINTMODE)
  {
    GimpLayerModeEffects l_mode;

    l_mode = gimp_layer_get_mode(src_layer_id);
    return (l_mode);
  }
  return (GimpLayerModeEffects)mode;
}  /* end p_get_paintmode */

/* ============================================================================
 * p_mov_selection_handling
 * Conditions:
 *    a copy of the selection in the initial source_image (or the actual source_frame)
 *    is available in tmpsel_channel_id
 *    this channel is part of the val_ptr->tmpsel_image_id image.
 *    the  val_ptr->tmpsel_image_id image has the same size as the source image.
 * This procedure adds a dummy layer to the val_ptr->tmpsel_image,
 * copies the alpha channel from the passed orig_layer_id to the dummy layer
 * applies the selection to the dummy layer
 * and then replaces the alpha channel of the orig_layer_id by the alpha channel of the dummy
 * ============================================================================
 */
static void
p_mov_selection_handling(gint32 orig_layer_id
                  , gint src_offset_x
                  , gint src_offset_y
                  , GapMovValues *val_ptr
                  , GapMovCurrent *cur_ptr
                  )
{
  gint l_bpp;
  gint l_width;
  gint l_height;
  gint32 l_tmp_layer_id;

  static gint32 display_id = -1;

  if(val_ptr->tmpsel_image_id < 0)
  {
    return;
  }
  if(!gimp_drawable_has_alpha(orig_layer_id))
  {
    gimp_layer_add_alpha(orig_layer_id);
  }

  l_width = gimp_drawable_width(orig_layer_id);
  l_height = gimp_drawable_height(orig_layer_id);
  l_bpp = gimp_drawable_bpp(orig_layer_id);

  l_tmp_layer_id = gimp_layer_new(val_ptr->tmpsel_image_id, "dummy",
                                 l_width, l_height,
                                  GIMP_RGBA_IMAGE,
                                 100.0,     /* full opaque */
                                 GIMP_NORMAL_MODE);
  gimp_image_insert_layer(val_ptr->tmpsel_image_id, l_tmp_layer_id, 0, 0);
  gimp_layer_set_offsets(l_tmp_layer_id, src_offset_x, src_offset_y);
  gimp_selection_none(val_ptr->tmpsel_image_id);

  gap_layer_copy_picked_channel(l_tmp_layer_id, 3  /* dst_pick is the alpha channel */
                               ,orig_layer_id, (l_bpp -1)
                               ,FALSE  /* shadow */
                               );

  gimp_image_select_item(val_ptr->tmpsel_image_id, GIMP_CHANNEL_OP_REPLACE, val_ptr->tmpsel_channel_id);


  if(cur_ptr->currSelFeatherRadius > 0.001)
  {
    gimp_selection_feather(val_ptr->tmpsel_image_id, cur_ptr->currSelFeatherRadius);
  }
  gimp_selection_invert(val_ptr->tmpsel_image_id);

  if(!gimp_selection_is_empty(val_ptr->tmpsel_image_id))
  {
    /* clear invers selected pixels to set unselected pixels transparent
     * (merge selection into alpha channel of l_tmp_layer_id)
     * We do not CLEAR on empty selection (== everything was selected originally),
     * because this would clear the alpha channels for all pixels.
     * But in that case we want all pixels to keep the origial alpha channel
     */
    gimp_edit_clear(l_tmp_layer_id);

    /* copy alpha channel form dummy back to original */
    gap_layer_copy_picked_channel(orig_layer_id, (l_bpp -1)
                               ,l_tmp_layer_id, 3 /* dst_pick is the alpha channel */
                               ,FALSE  /* shadow */
                               );
  }

  /* DEBUG code: show the tmpsel_image */
  if(1==0)
  {
    if(display_id < 0)
    {
      gimp_display_new(val_ptr->tmpsel_image_id);
    }
    return;
  }

  /* delete dummy layer */
  gimp_image_remove_layer(val_ptr->tmpsel_image_id, l_tmp_layer_id);

}  /* end p_mov_selection_handling  */



/* ============================================================================
 * p_mov_apply_bluebox
 *    perform perspective transformations on the passed layer
 * ============================================================================
 */
static void
p_mov_apply_bluebox(gint32 layer_id
                  , GapMovValues *val_ptr
                  , GapMovCurrent *cur_ptr
                  )
{
  if(val_ptr->bbp == NULL)
  {
    /* blubox parameters are not provided by the caller.
     * in this case we init with default values and try to fetch
     * values from previous bluebox filter runs
     */
    val_ptr->bbp = gap_bluebox_bbp_new(layer_id);;
  }

  if(val_ptr->bbp)
  {
    val_ptr->bbp->image_id = gimp_drawable_get_image(layer_id);
    val_ptr->bbp->drawable_id = layer_id;
    val_ptr->bbp->layer_id = layer_id;
    val_ptr->bbp->run_mode = GIMP_RUN_NONINTERACTIVE;
    val_ptr->bbp->run_flag = TRUE;

    gap_bluebox_apply(val_ptr->bbp);
  }
}  /* end p_mov_apply_bluebox  */


/* ============================================================================
 * p_mov_transform_perspective
 *    perform perspective transformations on the passed layer
 * ============================================================================
 */
static void
p_mov_transform_perspective(gint32 layer_id
                  , GapMovValues *val_ptr
                  , GapMovCurrent *cur_ptr
                  , gint         *resized_flag
                  , guint        *new_width
                  , guint        *new_height
                  )
{
  gdouble             width;
  gdouble             height;
  gdouble             w2;
  gdouble             h2;
  gdouble             neww;
  gdouble             newh;
  gdouble             x0;
  gdouble             y0;
  gdouble             x1;
  gdouble             y1;
  gdouble             x2;
  gdouble             y2;
  gdouble             x3;
  gdouble             y3;

  if( (cur_ptr->currTTLX == 1.0)
  &&  (cur_ptr->currTTLY == 1.0)
  &&  (cur_ptr->currTTRX == 1.0)
  &&  (cur_ptr->currTTRY == 1.0)
  &&  (cur_ptr->currTBLX == 1.0)
  &&  (cur_ptr->currTBLY == 1.0)
  &&  (cur_ptr->currTBRX == 1.0)
  &&  (cur_ptr->currTBRY == 1.0))
  {
    /* all 4 corner points unchanged, no perspective call needed */
    return;
  }


  width  = gimp_drawable_width(layer_id);
  height = gimp_drawable_height(layer_id);
  w2 = width / 2.0;
  h2 = height / 2.0;

  /* apply transform factors curT### to all 4 corners */
  x0 = w2 - (cur_ptr->currTTLX * w2);
  y0 = h2 - (cur_ptr->currTTLY * h2);
  x1 = w2 + (cur_ptr->currTTRX * w2);
  y1 = h2 - (cur_ptr->currTTRY * h2);
  x2 = w2 - (cur_ptr->currTBLX * w2);
  y2 = h2 + (cur_ptr->currTBLY * h2);
  x3 = w2 + (cur_ptr->currTBRX * w2);
  y3 = h2 + (cur_ptr->currTBRY * h2);

  neww = MAX((x1 - x0), (x3 - x2));
  newh = MAX((y2 - y0), (y3 - y1));

  if(gap_debug)
  {
    printf("** p_mov_transform_perspective:\n");
    printf("  Factors: [0] %.3f %.3f  [1] %.3f %.3f  [2] %.3f %.3f  [3] %.3f %.3f\n"
          ,(float)cur_ptr->currTTLX
          ,(float)cur_ptr->currTTLY
          ,(float)cur_ptr->currTTRX
          ,(float)cur_ptr->currTTRY
          ,(float)cur_ptr->currTBLX
          ,(float)cur_ptr->currTBLY
          ,(float)cur_ptr->currTBRX
          ,(float)cur_ptr->currTBRY
          );
    printf("  width: %.3f height: %.3f\n"
          ,(float)width
          ,(float)height
          );
    printf("  x0: %4.3f y0: %4.3f     x1: %4.3f y1: %4.3f\n"
          ,(float)x0
          ,(float)y0
          ,(float)x1
          ,(float)y1
          );
    printf("  x2: %4.3f y2: %4.3f     x3: %4.3f y3: %4.3f\n\n"
          ,(float)x2
          ,(float)y2
          ,(float)x3
          ,(float)y3
          );
    printf("  neww: %.3f newh: %.3f\n"
          ,(float)neww
          ,(float)newh
          );
  }


  gimp_context_set_defaults();
  gimp_context_set_transform_resize(GIMP_TRANSFORM_RESIZE_ADJUST);   /* do NOT clip */                                 
  gimp_item_transform_perspective(layer_id,
                      x0,
                      y0,
                      x1,
                      y1,
                      x2,
                      y2,
                      x3,
                      y3
                      );

  *resized_flag = 1;
  *new_width = neww;
  *new_height = newh;

}  /* end p_mov_transform_perspective  */




/* ----------------------------------------
 * p_mov_calculate_scale_factors
 * ----------------------------------------
 * In case of single frame mode rendering is done with
 * settings that were saved (recorded) based on frame and moving object size(s)
 * that may differ from actual sizes at render time.
 * The scaling factors are adjusted for automatically
 * pre-scaling depending on scenarios that are controled by 3 flags.
 *
 *
 * Example:
 * recorded sizes of the frame and the moving object:
 *
 *   +---------------------+    recordedFrameWidth:  640
 *   |                     |    recordedFrameHeight: 400
 *   |  +-------+          |
 *   |  |#######|          |    recordedObjWidth:    320
 *   |  |#######|          |    recordedObjHeight:   240
 *   |  +-------+          |
 *   |                     |
 *   |                     |
 *   +---------------------+
 *
 * actual rendering shall be done on frame size 1280x400
 * on a moving object of size 400 x 400.
 *
 *      +------+
 *      |######|   orig_width:      400
 *      |######|   orig_height:     400
 *      |######|
 *      |######|
 *      +------+
 *
 * Prescaling will depending on 3 flags:
 *
 * ==== scenarios that allow changing proportions of the moving object =====
 *
 * o) fit_width = TRUE, fit_height = TRUE, keep_proportions = FALSE
 *    In this scenario the moving object is pre-scaled to
 *    preScaleWidth and preScaleHeight. This gives same relations
 *    of the moving object / frame as the relations were at recording time,
 *    but deform proportions of the moving object from square to rectangle shape.
 *
 *   +-------------------------------------+    renderImageWidth:  1280
 *   |                                     |    renderImageHeight:  800
 *   |                                     |
 *   |  +--------------+                   |
 *   |  |##############|                   |    preScaleWidth:      640
 *   |  |##############|                   |    preScaleHeight:     480
 *   |  |##############|                   |
 *   |  |##############|                   |
 *   |  +--------------+                   |    renderObjWidth:     640
 *   |                                     |    renderObjHeight:    480
 *   |                                     |
 *   |                                     |
 *   |                                     |
 *   +-------------------------------------+
 *
 * o) fit_width = TRUE, fit_height = FALSE, keep_proportions = FALSE
 *    In this scenario the moving object is scaled to preScaleWidth
 *    and keeps its original height.
 *
 *   +-------------------------------------+    renderImageWidth:  1280
 *   |                                     |    renderImageHeight:  800
 *   |                                     |
 *   |  +--------------+                   |
 *   |  +--------------+                   |    preScaleWidth:      640
 *   |  |##############|                   |    preScaleHeight:     480
 *   |  |##############|                   |
 *   |  +--------------+                   |
 *   |  +--------------+                   |    renderObjWidth:     640
 *   |                                     |    renderObjHeight:    400
 *   |                                     |
 *   |                                     |
 *   |                                     |
 *   +-------------------------------------+
 *
 * o) fit_width = FALSE, fit_height = TRUE, keep_proportions = FALSE
 *    In this scenario the moving object is scaled to preScaleHeight
 *    and keeps its original width.
 *
 *   +-------------------------------------+    renderImageWidth:  1280
 *   |                                     |    renderImageHeight:  800
 *   |                                     |
 *   |  +---+------+---+                   |
 *   |  |   |######|   |                   |    preScaleWidth:      640
 *   |  |   |######|   |                   |    preScaleHeight:     480
 *   |  |   |######|   |                   |
 *   |  |   |######|   |                   |
 *   |  +---+------+---+                   |    renderObjWidth:     400
 *   |                                     |    renderObjHeight:    480
 *   |                                     |
 *   |                                     |
 *   |                                     |
 *   +-------------------------------------+
 *
 *
 *
 * ==== scenarios that keep proportions of the moving object =====
 *
 *
 * o) fit_width = TRUE, fit_height = TRUE, keep_proportions = TRUE
 *    In this scenario the moving object is pre-scaled to fit into a  rectangle
 *    of size preScaleWidth x preScaleHeight, keeping its original proportions.
 *
 *   +-------------------------------------+    renderImageWidth:  1280
 *   |                                     |    renderImageHeight:  800
 *   |                                     |
 *   |  +--+--------+--+                   |
 *   |  |  |########|  |                   |    preScaleWidth:      640
 *   |  |  |########|  |                   |    preScaleHeight:     480
 *   |  |  |########|  |                   |
 *   |  |  |########|  |                   |
 *   |  +--+--------+--+                   |    renderObjWidth:     480
 *   |                                     |    renderObjHeight:    480
 *   |                                     |
 *   |                                     |
 *   |                                     |
 *   +-------------------------------------+
 *
 * o) fit_width = TRUE, fit_height = FALSE, keep_proportions = TRUE
 *    In this scenario the moving object is pre-scaled to preScaleWidth
 *    keeping its original proportions.
 *
 *   +-------------------------------------+    renderImageWidth:  1280
 *   |                                     |    renderImageHeight:  800
 *   |  +--------------+                   |
 *   |  +##############+                   |
 *   |  |##############|                   |    preScaleWidth:      640
 *   |  |##############|                   |    preScaleHeight:     480
 *   |  |##############|                   |
 *   |  |##############|                   |
 *   |  +##############+                   |    renderObjWidth:     640
 *   |  +--------------+                   |    renderObjHeight:    640
 *   |                                     |
 *   |                                     |
 *   |                                     |
 *   +-------------------------------------+
 *
 * o) fit_width = FALSE, fit_height = TRUE, keep_proportions = TRUE
 *    In this scenario the moving object is pre-scaled to preScaleHeight
 *    keeping its original proportions.
 *
 *   +-------------------------------------+    renderImageWidth:  1280
 *   |                                     |    renderImageHeight:  800
 *   |                                     |
 *   |  +--+--------+--+                   |
 *   |  |  |########|  |                   |    preScaleWidth:      640
 *   |  |  |########|  |                   |    preScaleHeight:     480
 *   |  |  |########|  |                   |
 *   |  |  |########|  |                   |
 *   |  +--+--------+--+                   |    renderObjWidth:     480
 *   |                                     |    renderObjHeight:    480
 *   |                                     |
 *   |                                     |
 *   |                                     |
 *   +-------------------------------------+
 *
 * o) fit_width = FALSE, fit_height = FALSE, (keep_proportions NOT relevant)
 *    In this scenario the moving object is not pre-scaled scaled
 *    (therefore propotions are unchanged even in case keep_proportions flag is FALSE)
 *
 *   +-------------------------------------+    renderImageWidth:  1280
 *   |                                     |    renderImageHeight:  800
 *   |                                     |
 *   |  +--------------+                   |
 *   |  |   +######+   |                   |    preScaleWidth:      640
 *   |  |   |######|   |                   |    preScaleHeight:     480
 *   |  |   |######|   |                   |
 *   |  |   +######+   |                   |
 *   |  +--------------+                   |    renderObjWidth:     400
 *   |                                     |    renderObjHeight:    400
 *   |                                     |
 *   |                                     |
 *   |                                     |
 *   +-------------------------------------+
 *
 *
 * Note: the examples above show frame/moving object scenarios with handle object
 * at center settings and with current scale 100%
 *  (cur_ptr->currWidth == 100  cur_ptr->currHeight == 100)
 *
 */
static void
p_mov_calculate_scale_factors(gint32 image_id, GapMovValues *val_ptr, GapMovCurrent *cur_ptr
   , guint orig_width, guint orig_height
   , gdouble *retScaleWidthPercent
   , gdouble *retScaleheightPercent
   )
{
  gdouble              scaleWidthPercent;
  gdouble              scaleHeightPercent;


  scaleWidthPercent = cur_ptr->currWidth;
  scaleHeightPercent = cur_ptr->currHeight;


  if((cur_ptr->isSingleFrame)
  && (orig_width != 0)
  && (orig_height != 0))
  {
    gint32  renderImageWidth;
    gint32  renderImageHeight;
    gdouble preScaleWidth;
    gdouble preScaleHeight;
    gdouble preScaleWidthFactor;
    gdouble preScaleHeightFactor;

    gdouble result_width;         /* resulting width at unscaled size (e.g. 100%) */
    gdouble result_height;        /* resulting height at unscaled size (e.g. 100%) */
    gdouble origWidth;
    gdouble origHeight;

    origWidth = orig_width;
    origHeight = orig_height;


    renderImageWidth = gimp_image_width(image_id);
    renderImageHeight = gimp_image_height(image_id);
    preScaleWidth = origWidth;
    preScaleHeight = origHeight;
    preScaleWidthFactor = 1.0;
    preScaleHeightFactor = 1.0;

    /* calculate preScaleWidth */
    if((val_ptr->recordedFrameWidth != renderImageWidth)
    || (val_ptr->recordedObjWidth != orig_width))
    {
      if(val_ptr->recordedFrameWidth != 0)
      {
        preScaleWidth  = (gdouble)val_ptr->recordedObjWidth * (gdouble)renderImageWidth / (gdouble)val_ptr->recordedFrameWidth;
      }
    }

    /* calculate preScaleHeight */
    if((val_ptr->recordedFrameHeight != renderImageHeight)
    || (val_ptr->recordedObjHeight != orig_height))
    {
      if(val_ptr->recordedFrameHeight != 0)
      {
        preScaleHeight = (gdouble)val_ptr->recordedObjHeight * (gdouble)renderImageHeight / (gdouble)val_ptr->recordedFrameHeight;
      }
    }



    /* calculate (unscaled) result sizes according to flags */
    result_width = orig_width;
    result_height = orig_height;

    if(cur_ptr->keep_proportions)
    {
      gdouble actualObjProportion;

      actualObjProportion = origWidth / origHeight;


      if((cur_ptr->fit_width) && (cur_ptr->fit_height))
      {
        result_width = preScaleHeight * actualObjProportion;
        result_height = preScaleHeight;

        if(result_width > preScaleWidth)
        {
          result_width = preScaleWidth;
          result_height = preScaleWidth / actualObjProportion;
        }
      }
      else
      {
        if(cur_ptr->fit_height)
        {
           result_height = preScaleHeight;
           result_width = (gdouble)preScaleHeight * actualObjProportion;
        }
        if(cur_ptr->fit_width)
        {
           result_width = preScaleWidth;
           result_height = (gdouble)preScaleHeight / actualObjProportion;
        }
      }

    }
    else
    {
      if(cur_ptr->fit_height)
      {
         result_height = preScaleHeight;
      }
      if(cur_ptr->fit_width)
      {
         result_width = preScaleWidth;
      }
    }

    preScaleWidthFactor = result_width / origWidth;
    preScaleHeightFactor = result_height / origHeight;

    scaleWidthPercent = cur_ptr->currWidth * preScaleWidthFactor;
    scaleHeightPercent = cur_ptr->currHeight * preScaleHeightFactor;



    if(gap_debug)
    {
      printf("p_mov_calculate_scale_factors RESULTS:\n");
      printf("  FrameSize Recorded:(%d x %d) Actual:(%d x %d) MovObjSize Recorded:(%d x %d) Actual:(%d x %d)\n"
        ,(int)val_ptr->recordedFrameWidth
        ,(int)val_ptr->recordedFrameHeight
        ,(int)renderImageWidth
        ,(int)renderImageHeight
        ,(int)val_ptr->recordedObjWidth
        ,(int)val_ptr->recordedObjHeight
        ,(int)orig_width
        ,(int)orig_height
        );
      printf("  Prescale flags fit_width:%d fit_height:%d keep_proportions:%d  result_width:%.4f result_height:%.4f\n"
        ,cur_ptr->fit_width
        ,cur_ptr->fit_height
        ,cur_ptr->keep_proportions
        ,(float)result_width
        ,(float)result_height
        );
      printf("  Prescale WxH: (%.3f x %.3f) factors:(%.3f %.3f) scaleWidthPercent:%.3f scaleHeightPercent:%.3f\n"
        ,(float)preScaleWidth
        ,(float)preScaleHeight
        ,(float)preScaleWidthFactor
        ,(float)preScaleHeightFactor
        ,(float)scaleWidthPercent
        ,(float)scaleHeightPercent
        );
    }
  }


  /* deliver result values */
  *retScaleWidthPercent = scaleWidthPercent;
  *retScaleheightPercent = scaleHeightPercent;

}  /* end p_mov_calculate_scale_factors */



/* -----------------------------------
 * gap_mov_render_render
 * -----------------------------------
 * process transformations (current settings position, size opacity ...)
 * for the current source layer.
 *
 * In case current source layer is not already part of the processed frame (image_id)
 * insert a copy of the current source layer into the processed frame.
 * and do perform the transformations on the inserted copy.
 *
 * Note: in singleframe mode the current source layer may be already
 * part of the processed frame. In this special case perform the
 * transformations directly on the source layer.
 *
 */
gint
gap_mov_render_render(gint32 image_id, GapMovValues *val_ptr, GapMovCurrent *cur_ptr)
{
  gint32       l_cp_layer_id;
  gint32       l_cp_layer_mask_id;
  gint         l_offset_x, l_offset_y;            /* new offsets within dest. image */
  gint         l_src_offset_x, l_src_offset_y;    /* layeroffsets as they were in src_image */
  guint        l_new_width;
  guint        l_new_height;
  guint        l_potential_new_width;
  guint        l_potential_new_height;
  guint        l_orig_width;
  guint        l_orig_height;
  gint         l_resized_flag;
  gint         lx1, ly1, lx2, ly2;
  guint        l_image_width;
  guint        l_image_height;
  GimpLayerModeEffects l_mode;
  gdouble              scaleWidthPercent;
  gdouble              scaleHeightPercent;

  if(gap_debug)
  {
    printf("gap_mov_render_render: frame/layer: %ld/%ld  X=%f, Y=%f\n"
                "       Width=%f Height=%f\n"
                "       Opacity=%f  Rotate=%f  clip_to_img = %d force_visibility = %d\n"
                "       src_stepmode = %d rotate_threshold=%.7f\n"
		"       singleMovObjLayerId=%d singleMovObjIMAGEId=%d (frame)image_id=%d\n",
                     cur_ptr->dst_frame_nr, cur_ptr->src_layer_idx,
                     cur_ptr->currX, cur_ptr->currY,
                     cur_ptr->currWidth,
                     cur_ptr->currHeight,
                     cur_ptr->currOpacity,
                     cur_ptr->currRotation,
                     val_ptr->clip_to_img,
                     val_ptr->src_force_visible,
                     val_ptr->src_stepmode,
                     val_ptr->rotate_threshold,
		     cur_ptr->singleMovObjLayerId,
		     gimp_drawable_get_image(cur_ptr->singleMovObjLayerId),
		     image_id
		     );
  }

  if(cur_ptr->isSingleFrame)
  {
    l_mode = p_get_paintmode(val_ptr->src_paintmode
                            ,cur_ptr->singleMovObjLayerId
                            );
    if(gimp_drawable_get_image(cur_ptr->singleMovObjLayerId) == image_id)
    {
      /* the moving object layer id is already part of the processed frame image */
      l_cp_layer_id = cur_ptr->singleMovObjLayerId;
      /* findout the offsets of the original layer within the source Image */
      gimp_drawable_offsets(l_cp_layer_id, &l_src_offset_x, &l_src_offset_y );
    }
    else
    {
      /* the moving object layer id must be coped to the processed frame image
       * (this is typically used when called from the storyboard processor
       * where the frame image is just an empty temporary image without any layers)
       */
      if(val_ptr->src_stepmode >= GAP_STEP_FRAME)
      {
        gimp_layer_resize_to_image_size(cur_ptr->singleMovObjLayerId);
      }

      /* make a copy of the moving object layer
       * (using current opacity  & paintmode values at initial unscaled size)
       * and add the copied layer at dst_layerstack
       * (layerstack position is not important on an empty image)
       */
      l_cp_layer_id = gap_layer_copy_to_dest_image(image_id,
                                     cur_ptr->singleMovObjLayerId,
                                     cur_ptr->currOpacity,
                                     l_mode,
                                     &l_src_offset_x,
                                     &l_src_offset_y);
      if(l_cp_layer_id < 0)
      {
         cur_ptr->processedLayerId = -1;
         return -1;
      }
      gimp_image_insert_layer(image_id, l_cp_layer_id, 0, val_ptr->dst_layerstack);

    }


  }
  else
  {
    if(val_ptr->src_stepmode < GAP_STEP_FRAME)
    {
      if(gap_debug)
      {
        printf("gap_mov_render_render: Before gap_layer_copy_to_dest_image image_id:%d src_layer_id:%d\n"
                ,(int)image_id, (int)cur_ptr->src_layers[cur_ptr->src_layer_idx]);
      }
      l_mode = p_get_paintmode(val_ptr->src_paintmode
                              ,cur_ptr->src_layers[cur_ptr->src_layer_idx]
                              );
      /* make a copy of the current source layer
       * (using current opacity  & paintmode values)
       */
       l_cp_layer_id = gap_layer_copy_to_dest_image(image_id,
                                     cur_ptr->src_layers[cur_ptr->src_layer_idx],
                                     cur_ptr->currOpacity,
                                     l_mode,
                                     &l_src_offset_x,
                                     &l_src_offset_y);
    }
    else
    {
      if(gap_debug)
      {
        printf("gap_mov_render_render: Before gap_layer_copy_to_dest_image image_id:%d cache_tmp_layer_id:%d\n"
                ,(int)image_id, (int)val_ptr->cache_tmp_layer_id);
      }
      l_mode = p_get_paintmode(val_ptr->src_paintmode
                              ,val_ptr->cache_tmp_layer_id
                              );
       /* for FRAME based stepmodes use the flattened layer in the cached frame image */
       l_cp_layer_id = gap_layer_copy_to_dest_image(image_id,
                                     val_ptr->cache_tmp_layer_id,
                                     cur_ptr->currOpacity,
                                     l_mode,
                                     &l_src_offset_x,
                                     &l_src_offset_y);
    }
    /* add the copied layer to current destination image */
    if(gap_debug)
    {
      printf("gap_mov_render_render: after layer copy layer_id=%d\n", (int)l_cp_layer_id);
    }
    if(l_cp_layer_id < 0)
    {
       cur_ptr->processedLayerId = -1;
       return -1;
    }

    gimp_image_insert_layer(image_id, l_cp_layer_id, 0,
                         val_ptr->dst_layerstack);
    if(gap_debug)
    {
      printf("gap_mov_render_render: after add layer\n");
    }
  }


  /* set processedLayerId (for return handling in singleframe mode) */
  cur_ptr->processedLayerId = l_cp_layer_id;


  if(val_ptr->src_force_visible)
  {
     gimp_drawable_set_visible(l_cp_layer_id, TRUE);
  }

  /* check for layermask */
  l_cp_layer_mask_id = gimp_layer_get_mask(l_cp_layer_id);
  if(l_cp_layer_mask_id >= 0)
  {
     /* apply the layermask
      *   some transitions (especially rotate) can't operate proper on
      *   layers with masks !
      *   (tests with gimp_rotate resulted in trashed images,
      *    even if the mask was rotated too)
      */
      gimp_layer_remove_mask (l_cp_layer_id, GIMP_MASK_APPLY);
  }

  /* remove selection (if there is one)
   *   if there is a selection transitions (gimp_rotate)
   *   will create new layer and do not operate on l_cp_layer_id
   */
  gimp_selection_none(image_id);

  l_resized_flag = 0;
  l_orig_width  = gimp_drawable_width(l_cp_layer_id);
  l_orig_height = gimp_drawable_height(l_cp_layer_id);
  l_new_width  = l_orig_width;
  l_new_height = l_orig_height;

  if(val_ptr->src_apply_bluebox)
  {
    p_mov_apply_bluebox (l_cp_layer_id, val_ptr, cur_ptr);
  }

  if(val_ptr->src_selmode != GAP_MOV_SEL_IGNORE)
  {
    p_mov_selection_handling (l_cp_layer_id
                             , l_src_offset_x
                             , l_src_offset_y
                             , val_ptr
                             , cur_ptr
                             );
  }


  /* scale percentage values (where 100.0 is original size without scaling) */
  scaleWidthPercent = cur_ptr->currWidth;
  scaleHeightPercent = cur_ptr->currHeight;

  /* re-calculate scale percentage values
   * (for automatically pre-scaling in case of singleframes processing)  */
  p_mov_calculate_scale_factors(image_id, val_ptr, cur_ptr
                          , l_orig_width, l_orig_height
                          , &scaleWidthPercent
                          , &scaleHeightPercent
                          );





  if((scaleWidthPercent * scaleHeightPercent) > (100.0 * 100.0))
  {
    l_potential_new_width  = (l_orig_width  * scaleWidthPercent) / 100;
    l_potential_new_height = (l_orig_height * scaleHeightPercent) / 100;

    if((l_potential_new_width != l_new_width)
    || (l_potential_new_height != l_new_height))
    {
       /* have to scale layer (enlarge)  */
       l_resized_flag = 1;

       l_new_width  = l_potential_new_width;
       l_new_height = l_potential_new_height;

       if(gap_debug)
       {
         printf("SCALING-1 to size (%d x %d)\n"
           ,(int)l_new_width
           ,(int)l_new_height
           );
       }

       gimp_layer_scale(l_cp_layer_id, l_new_width, l_new_height, 0);
    }

    /* do 4-point perspective stuff (after enlarge) */
    p_mov_transform_perspective(l_cp_layer_id
                       , val_ptr
                       , cur_ptr
                       , &l_resized_flag
                       , &l_new_width
                       , &l_new_height
                       );
  }
  else
  {
    /* do 4-point perspective stuff (before shrink layer) */
    p_mov_transform_perspective(l_cp_layer_id
                       , val_ptr
                       , cur_ptr
                       , &l_resized_flag
                       , &l_new_width
                       , &l_new_height
                       );

    l_potential_new_width  = (l_new_width  * scaleWidthPercent) / 100;
    l_potential_new_height = (l_new_height * scaleHeightPercent) / 100;

    if((l_potential_new_width != l_new_width)
    || (l_potential_new_height != l_new_height))
    {
       /* have to scale layer */
       l_resized_flag = 1;

       l_new_width  = l_potential_new_width;
       l_new_height = l_potential_new_height;
       if(gap_debug)
       {
         printf("SCALING-2 to size (%d x %d)\n"
           ,(int)l_new_width
           ,(int)l_new_height
           );
       }
       gimp_layer_scale(l_cp_layer_id, l_new_width, l_new_height, 0);
    }
  }


  if((cur_ptr->currRotation  > val_ptr->rotate_threshold)
  || (cur_ptr->currRotation <  (0.0 - val_ptr->rotate_threshold)))
  {
    gboolean     l_interpolation;

    l_resized_flag = 1;

    l_interpolation = TRUE;  /* use the default interpolation (as configured in prefs) */

    /* have to rotate the layer (rotation also changes size as needed) */
    gap_pdb_gimp_rotate_degree(l_cp_layer_id, l_interpolation, cur_ptr->currRotation);


    l_new_width  = gimp_drawable_width(l_cp_layer_id);
    l_new_height = gimp_drawable_height(l_cp_layer_id);
  }

  if(l_resized_flag == 1)
  {
     /* adjust offsets according to handle and change of size */
     switch(val_ptr->src_handle)
     {
        case GAP_HANDLE_LEFT_BOT:
           l_src_offset_y += ((gint)l_orig_height - (gint)l_new_height);
           break;
        case GAP_HANDLE_RIGHT_TOP:
           l_src_offset_x += ((gint)l_orig_width - (gint)l_new_width);
           break;
        case GAP_HANDLE_RIGHT_BOT:
           l_src_offset_x += ((gint)l_orig_width - (gint)l_new_width);
           l_src_offset_y += ((gint)l_orig_height - (gint)l_new_height);
           break;
        case GAP_HANDLE_CENTER:
           l_src_offset_x += (((gint)l_orig_width - (gint)l_new_width) / 2);
           l_src_offset_y += (((gint)l_orig_height - (gint)l_new_height) / 2);
           break;
        case GAP_HANDLE_LEFT_TOP:
        default:
           break;
     }
  }

  /* calculate offsets in destination image */
  l_offset_x = (cur_ptr->currX - cur_ptr->l_handleX) + l_src_offset_x;
  l_offset_y = (cur_ptr->currY - cur_ptr->l_handleY) + l_src_offset_y;

  /* modify coordinate offsets of the copied layer within dest. image */
  gimp_layer_set_offsets(l_cp_layer_id, l_offset_x, l_offset_y);

  /* clip the handled layer to image size if desired */
  if(val_ptr->clip_to_img != 0)
  {
     l_image_width = gimp_image_width(image_id);
     l_image_height = gimp_image_height(image_id);

     lx1 = BOUNDS (l_offset_x, 0, (gint32)l_image_width);
     ly1 = BOUNDS (l_offset_y, 0, (gint32)l_image_height);
     lx2 = BOUNDS ((gint32)(l_new_width + l_offset_x), 0, (gint32)l_image_width);
     ly2 = BOUNDS ((gint32)(l_new_height + l_offset_y), 0, (gint32)l_image_height);

     l_new_width = lx2 - lx1;
     l_new_height = ly2 - ly1;

     if (l_new_width && l_new_height)
     {
       gimp_layer_resize(l_cp_layer_id, l_new_width, l_new_height,
                        -(lx1 - l_offset_x),
                        -(ly1 - l_offset_y));
     }
     else
     {
       /* no part of the layer is inside of the current frame (this image)
        * instead of removing we make the layer small and move him outside
        * the image.
        * (that helps to keep the layerstack position of the inserted layer(s)
        * constant in all handled video frames)
        */
       gimp_layer_resize(l_cp_layer_id, 2, 2, -3, -3);
     }
  }

  /* if tracing is ON, copy the actually rendered object to the trace_image */
  if(val_ptr->trace_image_id >= 0)
  {
     gint32  l_trc_layer_id;
     GimpMergeType l_mergemode;
     gdouble       l_opacity_initial;

     l_mergemode = GIMP_EXPAND_AS_NECESSARY;
     if(val_ptr->clip_to_img)
     {
       l_mergemode = GIMP_CLIP_TO_IMAGE;
     }

     /* apply descending opacity for all previous added objects
      * (that are all collected in the val_ptr->trace_layer_id, that is the only layer in
      * the trace_image)
      */
     gimp_layer_set_opacity(val_ptr->trace_layer_id, val_ptr->trace_opacity_desc);

     l_opacity_initial = (val_ptr->trace_opacity_initial * cur_ptr->currOpacity) / 100.0;
     l_opacity_initial = CLAMP(l_opacity_initial, 0.0, 100.0);

     l_trc_layer_id = gap_layer_copy_to_dest_image(val_ptr->trace_image_id,     /* the dest image */
                                   l_cp_layer_id,                  /* the layer to copy */
                                   l_opacity_initial,
                                   0,                              /* NORMAL paintmode */
                                   &l_src_offset_x,
                                   &l_src_offset_y);

     gimp_image_insert_layer(val_ptr->trace_image_id, l_trc_layer_id, 0, 0);

     /* merge the newly added l_trc_layer_id down to one tracelayer again */
     val_ptr->trace_layer_id = gap_image_merge_visible_layers(val_ptr->trace_image_id, l_mergemode);
  }

  if(gap_debug)
  {
    printf("GAP gap_mov_render_render: exit OK\n");
  }

  return 0;
}       /* end gap_mov_render_render */


/* ============================================================================
 * gap_mov_render_fetch_src_frame_autoskip
 *   fetch the requested video frame SourceImage into cache_tmp_image_id
 *   and
 *    - reduce all visible layers to one layer (cache_tmp_layer_id)
 *    - (scale to animated preview size if called for AnimPreview )
 *    - reuse cached image (for subsequent calls for the same framenumber
 *      of the same source image -- for  GAP_STEP_FRAME_NONE
 *    - never load current frame number from diskfile (use duplicate of the src_image)
 *  returns 0 (OK) or -1 (on Errors)
 *
 *  For the frame based modes this procedure supports the autoskip feature
 *  that will automatically advance to the next/previous availble frame
 *  (depending on stepsize) when the specified wanted_frame_nr is not availble.
 *
 *  IN/OUTPUT: but_got_frame_nr will be overwritten when the frame with the wanted_frame_nr
 *             is not availble but autoskip to the next/prev frame was successful
 *             in this case *but_got_frame_nr contains the number of an available frame image
 *             in all other cases *but_got_frame_nr is NOT changed by this procedure.
 * ============================================================================
 */
gint
gap_mov_render_fetch_src_frame_autoskip(GapMovValues *pvals,  gint32 wanted_frame_nr
   , gint32 *but_got_frame_nr, long stepsize)
{
  GapAnimInfo  *l_ainfo_ptr;
  GapAnimInfo  *l_old_ainfo_ptr;

  if(gap_debug)
  {
     printf("gap_mov_render_fetch_src_frame_autoskip: START src_image_id: %d wanted_frame_nr:%d"
            " cache_src_image_id:%d cache_frame_number:%d\n"
            , (int)pvals->src_image_id
            , (int)wanted_frame_nr
            , (int)pvals->cache_src_image_id
            , (int)pvals->cache_frame_number
            );
  }

  if(pvals->src_image_id < 0)
  {
     return -1;
  }

  if((pvals->src_image_id != pvals->cache_src_image_id)
  || (wanted_frame_nr != pvals->cache_frame_number))
  {
     if(pvals->cache_tmp_image_id >= 0)
     {
        if(gap_debug)
        {
           printf("gap_mov_render_fetch_src_frame_autoskip: DELETE cache_tmp_image_id:%d\n",
                    (int)pvals->cache_tmp_image_id);
        }
        /* destroy the cached frame image */
        gimp_image_delete(pvals->cache_tmp_image_id);
        pvals->cache_tmp_image_id = -1;
     }

     l_ainfo_ptr =  gap_lib_alloc_ainfo(pvals->src_image_id, GIMP_RUN_NONINTERACTIVE);

     if(pvals->cache_ainfo_ptr == NULL)
     {
       pvals->cache_ainfo_ptr =  l_ainfo_ptr;
     }
     else
     {
        if ((pvals->src_image_id == pvals->cache_src_image_id)
        &&  (strcmp(pvals->cache_ainfo_ptr->basename, l_ainfo_ptr->basename) == 0))
        {
           pvals->cache_ainfo_ptr->curr_frame_nr =  l_ainfo_ptr->curr_frame_nr;
           gap_lib_free_ainfo(&l_ainfo_ptr);
        }
        else
        {
           /* update cached ainfo  if source image has changed
            * (either by id or by its basename)
            */
           l_old_ainfo_ptr = pvals->cache_ainfo_ptr;
           pvals->cache_ainfo_ptr = l_ainfo_ptr;
           gap_lib_free_ainfo(&l_old_ainfo_ptr);
        }
     }

render_fetch_wanted_src_frame:
     if ((wanted_frame_nr == pvals->cache_ainfo_ptr->curr_frame_nr)
     ||  (wanted_frame_nr < 0))
     {
        /* always take the current source frame from the already opened image
         * not only for speedup reasons. (the diskfile may contain non actual imagedata)
         */
        pvals->cache_tmp_image_id = gimp_image_duplicate(pvals->src_image_id);
        wanted_frame_nr = pvals->cache_ainfo_ptr->curr_frame_nr;
     }
     else
     {
       /* build the source framename */
       if(pvals->cache_ainfo_ptr->new_filename != NULL)
       {
         g_free(pvals->cache_ainfo_ptr->new_filename);
       }
       pvals->cache_ainfo_ptr->new_filename = gap_lib_alloc_fname(pvals->cache_ainfo_ptr->basename,
                                        wanted_frame_nr,
                                        pvals->cache_ainfo_ptr->extension);
       if(pvals->cache_ainfo_ptr->new_filename == NULL)
       {
          printf("gap: error got no source frame filename\n");
          return -1;
       }

       if (g_file_test(pvals->cache_ainfo_ptr->new_filename, G_FILE_TEST_EXISTS))
       {
         /* load the wanted source frame */
         pvals->cache_tmp_image_id =  gap_lib_load_image(pvals->cache_ainfo_ptr->new_filename);
         if(pvals->cache_tmp_image_id < 0)
         {
            printf("gap: load error on src image %s\n", pvals->cache_ainfo_ptr->new_filename);
            return -1;
         }
       }
       else
       {
         long    l_available_frame_nr;
         gboolean l_frame_found;

         l_available_frame_nr = gap_lib_get_next_available_frame_number(
               wanted_frame_nr, stepsize
             , pvals->cache_ainfo_ptr->basename, pvals->cache_ainfo_ptr->extension
             , &l_frame_found);
         if (l_frame_found)
         {
           /* autoskip to next or previous available frame number */
           *but_got_frame_nr = l_available_frame_nr;
           wanted_frame_nr = l_available_frame_nr;
           goto render_fetch_wanted_src_frame;
         }
         else
         {
            printf("gap: missing frame number error on src image %s\n", pvals->cache_ainfo_ptr->new_filename);
            return -1;
         }

       }
     }


     /* frame fetch done at this point */

     gimp_image_undo_disable (pvals->cache_tmp_image_id);

     pvals->cache_tmp_layer_id = gap_image_merge_visible_layers(pvals->cache_tmp_image_id, GIMP_EXPAND_AS_NECESSARY);


     /* check if we are generating an anim preview
      * where we must Scale (down) the src image to preview size
      */
     if ((pvals->apv_mlayer_image >= 0)
     &&  ((pvals->apv_scalex != 100.0) || (pvals->apv_scaley != 100.0)))
     {
       gint32      l_size_x, l_size_y;

       if(gap_debug)
       {
          printf("gap_mov_render_fetch_src_frame_autoskip: Scale for Animpreview apv_scalex %f apv_scaley %f\n"
                 , (float)pvals->apv_scalex, (float)pvals->apv_scaley );
       }

       l_size_x = (gimp_image_width(pvals->cache_tmp_image_id) * pvals->apv_scalex) / 100;
       l_size_y = (gimp_image_height(pvals->cache_tmp_image_id) * pvals->apv_scaley) / 100;
       gimp_image_scale(pvals->cache_tmp_image_id, l_size_x, l_size_y);
     }

     if(pvals->src_selmode != GAP_MOV_SEL_IGNORE)
     {
       if((pvals->tmpsel_channel_id < 0)
       || (pvals->src_selmode == GAP_MOV_SEL_FRAME_SPECIFIC))
       {
         gint32        l_sel_channel_id;
         gboolean      l_all_empty;

         l_all_empty = FALSE;
         /* pick the selection for the 1.st handled frame
          * or foreach handled frame when mode is GAP_MOV_SEL_FRAME_SPECIFIC
          */
         if(gimp_selection_is_empty(pvals->cache_tmp_image_id))
         {
           l_all_empty = TRUE;
         }
         l_sel_channel_id = gimp_image_get_selection(pvals->cache_tmp_image_id);
         gap_mov_render_create_or_replace_tempsel_image(l_sel_channel_id, pvals, l_all_empty);
       }
     }

     pvals->cache_src_image_id = pvals->src_image_id;
     pvals->cache_frame_number = wanted_frame_nr;

  }

  return 0; /* OK */

}       /* end gap_mov_render_fetch_src_frame_autoskip */


/* ============================================================================
 * gap_mov_render_fetch_src_frame
 *   fetch the requested video frame SourceImage into cache_tmp_image_id
 *   and
 *    - reduce all visible layers to one layer (cache_tmp_layer_id)
 *    - (scale to animated preview size if called for AnimPreview )
 *    - reuse cached image (for subsequent calls for the same framenumber
 *      of the same source image -- for  GAP_STEP_FRAME_NONE
 *    - never load current frame number from diskfile (use duplicate of the src_image)
 *  returns 0 (OK) or -1 (on Errors)
 * ============================================================================
 */
gint
gap_mov_render_fetch_src_frame(GapMovValues *pvals,  gint32 wanted_frame_nr)
{
  gint32 l_but_got_frame_nr;
  gint   l_rc;

  l_rc = gap_mov_render_fetch_src_frame_autoskip(pvals
                    ,  wanted_frame_nr, &l_but_got_frame_nr, 1);
  return (l_rc);

}       /* end gap_mov_render_fetch_src_frame */


/* ============================================================================
 * gap_mov_render_create_or_replace_tempsel_image
 *    create or replace a temp image to store the selection
 *    of the initial source image (or frame)
 * the all_empty parameter == TRUE indicates, that the channel_id
 * represents an empty selection.
 * in this case tmpsel_channel_id is set to everything selected
 * ============================================================================
 */
void
gap_mov_render_create_or_replace_tempsel_image(gint32 channel_id
                  , GapMovValues *val_ptr
                  , gboolean all_empty
                  )
{
  if(val_ptr->tmpsel_image_id >= 0)
  {
    gimp_image_delete(val_ptr->tmpsel_image_id);
  }

  val_ptr->tmpsel_image_id = gimp_image_new(gimp_drawable_width(channel_id)
                                           ,gimp_drawable_height(channel_id)
                                           ,GIMP_RGB
                                           );

  gimp_selection_all(val_ptr->tmpsel_image_id);
  val_ptr->tmpsel_channel_id = gimp_selection_save(val_ptr->tmpsel_image_id);

  if(!all_empty)
  {
    gap_layer_copy_content(val_ptr->tmpsel_channel_id   /* dst */
                          ,channel_id                   /* src */
                          );
  }
}  /* end gap_mov_render_create_or_replace_tempsel_image */
