/* gap_mov_exec.c
 * 1997.11.06 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * Move : procedures for copying source layer(s) to multiple frames
 * (varying Koordinates, opacity, size ...)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* revision history:
 * gimp    2.1.0a   2004/04/18  hof: moved procedures fprintf_gdouble and sscan_flt_numbers
 *                                   to gap_lib.c module
 * gimp    1.3.21c; 2003/10/23  hof: bugfix: trace_layer was not initialized (must be cleared at creation)
 * gimp    1.3.20d; 2003/10/14  hof: new: implemented missing parts for trace and tween_layer processing
 * gimp    1.3.20c; 2003/09/29  hof: new features: perspective transformation, step_speed_factor,
 *                                   tween_layer and trace_layer (not finshed yet)
 *                                   changed opacity, rotation and resize from int to gdouble
 * gimp    1.3.14a; 2003/05/24  hof: rename p_fetch_src_frame to gap_mov_render_fetch_src_frame
 * gimp    1.3.12a; 2003/05/01  hof: merge into CVS-gimp-gap project
 * gimp    1.3.11a; 2003/01/18  hof: Conditional framesave
 * gimp    1.3.5a;  2002/04/20  hof: api cleanup (dont use gimp_drawable_set_image)
 * gimp    1.3.4a;  2002/03/12  hof: removed private pdb-wrappers
 * gimp    1.1.29b; 2000/11/20  hof: FRAME based Stepmodes, bugfixes for path calculation
 * gimp    1.1.23a; 2000/06/03  hof: bugfix anim_preview < 100% did not work
 *                                   (the layer tattoos in a duplicated image may differ from the original !!)
 * gimp    1.1.20a; 2000/04/25  hof: support for keyframes, anim_preview
 * version 0.93.04              hof: Window with Info Message if no Source Image was selected in MovePath
 * version 0.90.00;             hof: 1.st (pre) release 14.Dec.1997
 */
#include "config.h"

/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_libgapbase.h"
#include "gap-intl.h"
#include "gap_layer_copy.h"
#include "gap_lib.h"
#include "gap_image.h"
#include "gap_mov_dialog.h"
#include "gap_mov_exec.h"
#include "gap_mov_render.h"
#include "gap_pdb_calls.h"
#include "gap_arr_dialog.h"
#include "gap_accel_char.h"
#include "gap_mov_xml_par.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */

static void p_add_tween_and_trace(gint32 dest_image_id, GapMovData *mov_ptr, GapMovCurrent *cur_ptr);
static gint p_mov_call_render(GapMovData *mov_ptr, GapMovCurrent *cur_ptr, gint apv_layerstack);
static void p_mov_advance_src_layer(GapMovCurrent *cur_ptr, GapMovValues  *pvals);
static void p_mov_advance_src_frame(GapMovCurrent *cur_ptr, GapMovValues  *pvals);

static void     p_log_current_render_params(GapMovData *mov_ptr, GapMovCurrent *cur_ptr);
static void     p_printf_log_parameters(GapMovData *mov_ptr);
static void     gap_mov_exec_set_handle_offsets_singleframe(GapMovValues *val_ptr, GapMovCurrent *cur_ptr);
static void     p_add_2nd_controlpoint(GapMovValues *val_ptr);
static void     p_init_curr_ptr_with_1st_controlpoint(GapMovCurrent *cur_ptr, GapMovValues *val_ptr, GapMovSingleFrame *singleFramePtr);
static gint32   p_duplicate_layer(gint32 layerId);

static long     p_mov_execute_or_query(GapMovData *mov_ptr, GapMovQuery *mov_query);
static gint32   p_mov_execute_singleframe(GapMovData *mov_ptr);


static long     p_mov_execute(GapMovData *mov_ptr);
static gdouble  p_calc_angle(gint p1x, gint p1y, gint p2x, gint p2y);
static gdouble  p_rotatate_less_than_180(gdouble angle, gdouble angle_new, gint *turns);


static gint     p_calculate_relframe_nr_at_index(GapMovValues *val_ptr, gint index, gint frames);
static gint     p_findEndOfSegmentIndex(GapMovValues *val_ptr, gint startOfSegmentIndex, gint points);
static gdouble  p_calculate_LineLength(GapMovValues *val_ptr, gint idx);
static gdouble  p_calculate_path_segment_length(GapMovValues  *val_ptr
                    , gint startOfSegmentIndex, gint endOfSegmentIndex);
static gint     p_pick_controlpoint_at_curr_length(GapMovValues  *val_ptr
                    , gint startOfSegmentIndex, gint endOfSegmentIndex, gdouble pathSegmentLength
                    , gint accelerationCharacteristic, gdouble lengthFactorLinear
                    , gdouble *flt_posfactor
                    );
static gdouble  p_calculate_posFactor_from_FrameTweens(gdouble frameTweensInSegment
                   , gdouble currFrameTweenInSegment
                   , gint accelerationCharacteristic
                   );
static gint     p_calculate_settings_for_current_FrameTween(
                     GapMovValues  *val_ptr
                   , GapMovCurrent *cur_ptr
                   , gdouble framesPerLine
                   , long     currFrameIndex    /* relative frame number where the first handled frame is 0
                                                 * Note that the first fame is already processed outside the loop
                                                 * therefore this procedure is typically called first time with
                                                 * currFrameIndex value 1.
                                                 */
                   , long     currPtidx         /* current controlpoint index for processing without acceleration characteristic */
                   , gdouble  flt_posfactor     /* current position factor within one line between controlpoints
                                                 * (relevant for processing without acceleration characteristic)
                                                 */
                   , gdouble  affectedFrames        /* number of affected frames (in the whole Move patch operation) */
                   , long     availableCtrlPoints   /* number of available controlpoints */
                   , gint     startOfSegmentIndex
                   , gint     endOfSegmentIndex
                   , GapMovQuery *mov_query
                   );






/* ============================================================================
 * p_add_tween_and_trace
 * ============================================================================
 * if there are tween_layers add them to the dest_image
 * and remove them from the tween_image.
 *
 * copy the trace_layer to the dest_image (if there is one)
 */

void
p_add_tween_and_trace(gint32 dest_image_id, GapMovData *mov_ptr, GapMovCurrent *cur_ptr)
{
  GimpMergeType l_mergemode;
  gint32  l_new_layer_id;
  gint    l_src_offset_x, l_src_offset_y;
  gint32  l_layer_id;

  l_mergemode = GIMP_EXPAND_AS_NECESSARY;
  if(mov_ptr->val_ptr->clip_to_img)
  {
    l_mergemode = GIMP_CLIP_TO_IMAGE;
  }

  /* add Trace_layer */
  if((mov_ptr->val_ptr->trace_image_id >= 0)
  && (mov_ptr->val_ptr->trace_layer_id >= 0))
  {
    /* copy the layer from the temp image to the anim preview multilayer image */
    l_new_layer_id = gap_layer_copy_to_dest_image(dest_image_id,
                                   mov_ptr->val_ptr->trace_layer_id,
                                   100.0,       /* opacity full */
                                   0,           /* NORMAL */
                                   &l_src_offset_x,
                                   &l_src_offset_y
                                   );

    /* add the layer to the destination image */
    gimp_image_insert_layer (dest_image_id, l_new_layer_id, 0, mov_ptr->val_ptr->dst_layerstack +1);

    /* keep the trace_layer for all further move path processing steps */
  }

  /* add Tween_Layer(s) */
  if(mov_ptr->val_ptr->tween_image_id >= 0)
  {
    gint32 l_new_tween_image_id;

    /* for DEBUG only: show tween image before flatten */
    if(1==0)
    {
      gint32  tw_image_id;

      tw_image_id = gimp_image_duplicate(mov_ptr->val_ptr->tween_image_id);
      gimp_display_new(tw_image_id);
    }

    l_layer_id = gap_image_merge_visible_layers(mov_ptr->val_ptr->tween_image_id, l_mergemode);

    /* copy the layer from the temp image to the anim preview multilayer image */
    l_new_layer_id = gap_layer_copy_to_dest_image(dest_image_id,
                                   l_layer_id,
                                   100.0,       /* opacity full */
                                   0,           /* NORMAL */
                                   &l_src_offset_x,
                                   &l_src_offset_y
                                   );

    /* add the layer to the destination image */
    gimp_image_insert_layer (dest_image_id, l_new_layer_id, 0, mov_ptr->val_ptr->dst_layerstack +1);

    if((mov_ptr->val_ptr->trace_image_id >= 0)
    && (mov_ptr->val_ptr->trace_layer_id >= 0))
    {
      /* both tween_layer and trace_layer are active
       * in this case the tween_layer is set invisible
       *
       */
       gimp_drawable_set_visible(l_new_layer_id, FALSE);
    }

    /* remove tween layers from the tween_image after usage */
    l_new_tween_image_id = gap_image_new_of_samesize(mov_ptr->val_ptr->tween_image_id);
    gimp_image_undo_disable (l_new_tween_image_id);

    gimp_image_delete(mov_ptr->val_ptr->tween_image_id);
    mov_ptr->val_ptr->tween_image_id = l_new_tween_image_id;
  }

}  /* end p_add_tween_and_trace */


/* ============================================================================
 * p_mov_call_render
 * ============================================================================
 * The render call behaves different, depending on the calling conditions
 * a) when called to render a tween
 *      create an empty temp frame, render,
 *      and add the result as new layer to the tween_image.
 * b) when called to render the animted_preview in a multilayer image
 *      depending on mode
 *          EXACT: load current frame
 *          ONE:   use one prescaled initial frame
 *      render, add tween and tracelayers (if there any)
 *      [ optional save to video paste buffer ], and
 *      add the layer to the anim preview multilayer image
 * c) when called to render a real Frame
 *      load current frame,
 *      render, add tween and tracelayers (if there any)
 *      save back to disk
 */

gint
p_mov_call_render(GapMovData *mov_ptr, GapMovCurrent *cur_ptr, gint apv_layerstack)
{
  GapAnimInfo *ainfo_ptr;
  gint32  l_tmp_image_id;
  gint32  l_layer_id;
  int     l_rc;
  char    *l_fname;
  char    *l_name;
  GapMovSingleFrame *singleFramePtr;

  l_rc = 0;
  ainfo_ptr = mov_ptr->dst_ainfo_ptr;
  singleFramePtr = mov_ptr->singleFramePtr;

  p_log_current_render_params(mov_ptr, cur_ptr);

  if((mov_ptr->val_ptr->twix > 0)
  && (singleFramePtr == NULL))
  {
    /* We are rendering a virtual frame (a tween) */
    /* ------------------------------------------ */

    /* all tween_layers will be added later to the next real Frame,
     * or to the animated Preview Image.
     * (this will happen in one of the next calls when tween index goes down to 0)
     */
    l_tmp_image_id = gap_image_new_of_samesize(mov_ptr->val_ptr->tween_image_id);
    gimp_image_undo_disable (l_tmp_image_id);

    /* call render procedure for the tween image */
    if(0 == gap_mov_render_render(l_tmp_image_id, mov_ptr->val_ptr, cur_ptr))
    {
      GimpMergeType l_mergemode;

      l_mergemode = GIMP_EXPAND_AS_NECESSARY;
      if(mov_ptr->val_ptr->clip_to_img)
      {
        l_mergemode = GIMP_CLIP_TO_IMAGE;
      }

      /* get the_rendered_object Layer */
      l_layer_id = gap_image_merge_visible_layers(l_tmp_image_id, l_mergemode);

      gimp_drawable_set_name(l_layer_id, _("Tweenlayer"));
      {
        gint32  l_new_layer_id;
        gint    l_src_offset_x, l_src_offset_y;
        gdouble l_tween_opacity;
        gdouble l_fadefactor;

        l_tween_opacity = (mov_ptr->val_ptr->tween_opacity_initial * cur_ptr->currOpacity) / 100.0;

        l_fadefactor = mov_ptr->val_ptr->tween_opacity_desc / 100.0;
        if(l_fadefactor < 1.0)
        {
          gint    l_ii;

          for(l_ii=1; l_ii < mov_ptr->val_ptr->twix; l_ii++)
          {
            /* the higher the tween index, the more we fade out
             * we do not fade out for tween index 1 that is the nearest tween to the
             * real frame
             */
            l_tween_opacity *= l_fadefactor;
          }
        }

        l_tween_opacity = CLAMP(l_tween_opacity, 0.0, 100.0);

        /* copy the layer from the temp image to the tween multilayer image */
        l_new_layer_id = gap_layer_copy_to_dest_image(mov_ptr->val_ptr->tween_image_id,
                                         l_layer_id,
                                         l_tween_opacity,
                                         0,           /* NORMAL */
                                         &l_src_offset_x,
                                         &l_src_offset_y
                                         );

        /* add the layer to the anim preview multilayer image */
        gimp_image_insert_layer (mov_ptr->val_ptr->tween_image_id, l_new_layer_id, 0, 0 /* top of layerstack */ );
      }
    }
    else
    {
      l_rc = -1;
    }

  }
  else
  {
    if(mov_ptr->val_ptr->apv_mlayer_image < 0)
    {
      if(singleFramePtr == NULL)
      {
        /* We are generating the Animation on the ORIGINAL FRAMES */
        /* ------------------------------------------------------ */
        if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
        ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                          cur_ptr->dst_frame_nr,
                                          ainfo_ptr->extension);
        if(ainfo_ptr->new_filename == NULL)
           return -1;

        /* load next frame to render */
        l_tmp_image_id = gap_lib_load_image(ainfo_ptr->new_filename);
        if(l_tmp_image_id < 0)
        {
          return -1;
        }

        gimp_image_undo_disable (l_tmp_image_id);
      }
      else
      {
        l_tmp_image_id = mov_ptr->val_ptr->dst_image_id;
	if (l_tmp_image_id < 0)
	{
          l_tmp_image_id = gimp_drawable_get_image(singleFramePtr->drawable_id);
	}
      }


      /* call render procedure for current image */
      if(0 == gap_mov_render_render(l_tmp_image_id, mov_ptr->val_ptr, cur_ptr))
      {
        /* check if we have tween_layer and/or trace_layer
         * and add them to the temp image
         */
        p_add_tween_and_trace(l_tmp_image_id, mov_ptr, cur_ptr);

        if(singleFramePtr == NULL)
        {
          /* if OK: save the rendered frame back to disk (but not in singleframes mode) */
          if(gap_lib_save_named_frame(l_tmp_image_id, ainfo_ptr->new_filename) < 0)
          {
            l_rc = -1;
          }
        }
      }
      else 
      {
        l_rc = -1;
      }
    }
    else
    {
      /* We are generating an ANIMATED PREVIEW multilayer image */
      /* ------------------------------------------------------ */
      if(mov_ptr->val_ptr->apv_src_frame >= 0)
      {
         /* anim preview uses one constant (prescaled) frame */
         l_tmp_image_id = gimp_image_duplicate(mov_ptr->val_ptr->apv_src_frame);
         gimp_image_undo_disable (l_tmp_image_id);
      }
      else
      {
         /* anim preview exact mode uses original frames */
         if(ainfo_ptr->new_filename != NULL) g_free(ainfo_ptr->new_filename);
         ainfo_ptr->new_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                           cur_ptr->dst_frame_nr,
                                           ainfo_ptr->extension);
         l_tmp_image_id = gap_lib_load_image(ainfo_ptr->new_filename);
         if(l_tmp_image_id < 0)
         {
           return -1;
         }
         
         gimp_image_undo_disable (l_tmp_image_id);

         if((mov_ptr->val_ptr->apv_scalex != 100.0) || (mov_ptr->val_ptr->apv_scaley != 100.0))
         {
           gint32      l_size_x, l_size_y;

           l_size_x = (gimp_image_width(l_tmp_image_id) * mov_ptr->val_ptr->apv_scalex) / 100;
           l_size_y = (gimp_image_height(l_tmp_image_id) * mov_ptr->val_ptr->apv_scaley) / 100;

           gimp_image_scale (l_tmp_image_id, l_size_x, l_size_y);
         }
      }

      /* call render procedure for current image */
      if(0 == gap_mov_render_render(l_tmp_image_id, mov_ptr->val_ptr, cur_ptr))
      {
        /* if OK and optional save to gap_paste-buffer */
        if(mov_ptr->val_ptr->apv_gap_paste_buff != NULL)
        {
           l_fname = gap_lib_alloc_fname(mov_ptr->val_ptr->apv_gap_paste_buff,
                                   cur_ptr->dst_frame_nr,
                                   ".xcf");
           gap_lib_save_named_frame(l_tmp_image_id, l_fname);
        }

        /* check if we have tween_layer and/or trace_layer
         * and add them to the temp image
         */
        p_add_tween_and_trace(l_tmp_image_id, mov_ptr, cur_ptr);

        /* flatten the rendered frame */
        l_layer_id = gimp_image_flatten(l_tmp_image_id);
        if(l_layer_id < 0)
        {
          if(gap_debug) printf("p_mov_call_render: flattened layer_id:%d\n", (int)l_layer_id);
          /* hof:
           * if invisible layers are flattened on an empty image
           * we do not get a resulting layer (returned l_layer_id == -1)
           *
           *  I'm not sure if is this a bug, but here is a workaround:
           *
           * In that case I add a dummy layer 1x1 pixel (at offest -1,-1)
           * and flatten again, and it works (tested with gimp-1.1.19)
           */
          l_layer_id = gimp_layer_new(l_tmp_image_id, "dummy",
                                 1,
                                 1,
                                 ((gint)(gimp_image_base_type(l_tmp_image_id)) * 2),
                                 100.0,     /* Opacity full opaque */
                                 0);        /* NORMAL */
          gimp_image_insert_layer(l_tmp_image_id, l_layer_id, 0, 0);
          gimp_layer_set_offsets(l_layer_id, -1, -1);
          l_layer_id = gimp_image_flatten(l_tmp_image_id);
        }
        gimp_layer_add_alpha(l_layer_id);

        if(gap_debug)
        {
          printf("p_mov_call_render: flattened layer_id:%d\n", (int)l_layer_id);
          printf("p_mov_call_render: tmp_image_id:%d  apv_mlayer_image:%d\n",
                  (int)l_tmp_image_id, (int)mov_ptr->val_ptr->apv_mlayer_image);
        }

        /* set layername (including delay for the framerate) */
        l_name = g_strdup_printf("frame_%06d (%dms)"
                              , (int) cur_ptr->dst_frame_nr
                              , (int)(1000/mov_ptr->val_ptr->apv_framerate));
        gimp_drawable_set_name(l_layer_id, l_name);
        g_free(l_name);

        {
          gint32  l_new_layer_id;
          gint    l_src_offset_x, l_src_offset_y;

          /* copy the layer from the temp image to the anim preview multilayer image */
          l_new_layer_id = gap_layer_copy_to_dest_image(mov_ptr->val_ptr->apv_mlayer_image,
                                         l_layer_id,
                                         100.0,       /* opacity full */
                                         0,           /* NORMAL */
                                         &l_src_offset_x,
                                         &l_src_offset_y
                                         );

          /* add the layer to the anim preview multilayer image */
          gimp_image_insert_layer (mov_ptr->val_ptr->apv_mlayer_image, l_new_layer_id, 0, apv_layerstack);
        }
      }
      else l_rc = -1;
    }
  }

  if(singleFramePtr == NULL)
  {
    /* destroy the tmp image 
     * (but not in singleframes mode. Note that in singleframe mode l_tmp_image_id
     * referes to the image from where we were invoked)
     */
    gimp_image_delete(l_tmp_image_id);
  }

  return l_rc;
}       /* end p_mov_call_render */




/* ============================================================================
 * p_mov_advance_src_layer
 * advance layer index according to stepmode
 * ============================================================================
 */
static void
p_mov_advance_src_layer(GapMovCurrent *cur_ptr, GapMovValues  *pvals)
{
  static gdouble l_ping = -1;
  gdouble l_step_speed_factor;
  gdouble l_round;

  /* limit step factor to number of available layers -1 */
  l_step_speed_factor = MIN(pvals->step_speed_factor, (gdouble)cur_ptr->src_last_layer);
  if(pvals->tween_steps > 0)
  {
    /* when we have tweens, the speed_factor must be divided (the +1 is for the real frame) */
    l_step_speed_factor /= (gdouble)(pvals->tween_steps +1);
  }
  l_round = 0.0;

  if(gap_debug) printf("p_mov_advance_src_layer: stepmode=%d last_layer=%d idx=%d (%.4f) speed_factor: %.4f\n",
                       (int)pvals->src_stepmode,
                       (int)cur_ptr->src_last_layer,
                       (int)cur_ptr->src_layer_idx,
                       (float)cur_ptr->src_layer_idx_dbl,
                       (float)l_step_speed_factor
                      );

  /* note: top layer has index 0
   *       therfore reverse loops have to count up
   *       forward loop is defined as sequence from BG to TOP layer
   */
  if((cur_ptr->src_last_layer > 0 ) && (pvals->src_stepmode != GAP_STEP_NONE))
  {
    switch(pvals->src_stepmode)
    {
      case GAP_STEP_ONCE_REV:
        cur_ptr->src_layer_idx_dbl += l_step_speed_factor;
        if(cur_ptr->src_layer_idx_dbl > cur_ptr->src_last_layer)
        {
           cur_ptr->src_layer_idx_dbl = (gdouble)cur_ptr->src_last_layer;
        }
        break;
      case GAP_STEP_ONCE:
        cur_ptr->src_layer_idx_dbl -= l_step_speed_factor;
        if(cur_ptr->src_layer_idx_dbl < 0)
        {
           cur_ptr->src_layer_idx_dbl = 0.0;
        }
        break;
      case GAP_STEP_PING_PONG:
        cur_ptr->src_layer_idx_dbl += (l_ping * l_step_speed_factor);
        if(l_ping < 0)
        {
          if(cur_ptr->src_layer_idx_dbl < -0.5)
          {
             cur_ptr->src_layer_idx_dbl = 1.0;
             l_ping = 1.0;
          }
          l_round = 0.5;
        }
        else
        {
          if(cur_ptr->src_layer_idx_dbl >= (gdouble)(cur_ptr->src_last_layer +1))
          {
             cur_ptr->src_layer_idx_dbl = (gdouble)cur_ptr->src_last_layer - 1.0;
             l_ping = -1;
          }
        }
        break;
      case GAP_STEP_LOOP_REV:
        cur_ptr->src_layer_idx_dbl += l_step_speed_factor;
        if(cur_ptr->src_layer_idx_dbl >= (gdouble)(cur_ptr->src_last_layer +1))
        {
           cur_ptr->src_layer_idx_dbl -= (gdouble)(cur_ptr->src_last_layer + 1);
        }
        break;
      case GAP_STEP_LOOP:
      default:
        cur_ptr->src_layer_idx_dbl -= l_step_speed_factor;
        if(cur_ptr->src_layer_idx_dbl < -0.5)
        {
           cur_ptr->src_layer_idx_dbl += (gdouble)(cur_ptr->src_last_layer + 1);
        }
        l_round = 0.5;
        break;

    }
    cur_ptr->src_layer_idx = MAX((long)(cur_ptr->src_layer_idx_dbl + l_round), 0);
  }
}       /* end  p_advance_src_layer */



/* ============================================================================
 * p_mov_advance_src_frame
 *   advance chached image to next source frame according to FRAME based pvals->stepmode
 * ============================================================================
 */
static void
p_mov_advance_src_frame(GapMovCurrent *cur_ptr, GapMovValues  *pvals)
{
  static gdouble l_ping = 1;
  gdouble l_step_speed_factor;
  gdouble l_round;

  /* limit step factor to number of available frames -1 */
  l_step_speed_factor = MIN(pvals->step_speed_factor, (gdouble)(pvals->cache_ainfo_ptr->frame_cnt -1));
  if(pvals->tween_steps > 0)
  {
    /* when we have tweens, the speed_factor must be divided (the +1 is for the real frame) */
    l_step_speed_factor /= (gdouble)(pvals->tween_steps +1);
  }
  l_round = 0.0;

  if(pvals->src_stepmode != GAP_STEP_FRAME_NONE)
  {
    if(pvals->cache_ainfo_ptr == NULL )
    {
      pvals->cache_ainfo_ptr =  gap_lib_alloc_ainfo(pvals->src_image_id, GIMP_RUN_NONINTERACTIVE);
    }

    if(pvals->cache_ainfo_ptr->first_frame_nr < 0)
    {
       gap_lib_dir_ainfo(pvals->cache_ainfo_ptr);
    }
  }

  if(gap_debug) printf("p_mov_advance_src_frame: stepmode=%d frame_cnt=%d first_frame=%d last_frame=%d idx=%d (%.4f) speed_factor: %.4f\n",
                       (int)pvals->src_stepmode,
                       (int)pvals->cache_ainfo_ptr->frame_cnt,
                       (int)pvals->cache_ainfo_ptr->first_frame_nr,
                       (int)pvals->cache_ainfo_ptr->last_frame_nr,
                       (int)cur_ptr->src_frame_idx,
                       (float)cur_ptr->src_frame_idx_dbl,
                       (float)l_step_speed_factor
                      );

  if((pvals->cache_ainfo_ptr->frame_cnt > 1 ) && (pvals->src_stepmode != GAP_STEP_FRAME_NONE))
  {
    switch(pvals->src_stepmode)
    {
      case GAP_STEP_FRAME_ONCE_REV:
        cur_ptr->src_frame_idx_dbl -= l_step_speed_factor;
        if(cur_ptr->src_frame_idx_dbl < (gdouble)pvals->cache_ainfo_ptr->first_frame_nr)
        {
           cur_ptr->src_frame_idx_dbl = (gdouble)pvals->cache_ainfo_ptr->first_frame_nr;
        }
        break;
      case GAP_STEP_FRAME_ONCE:
        cur_ptr->src_frame_idx_dbl += l_step_speed_factor;
        if(cur_ptr->src_frame_idx_dbl > (gdouble)pvals->cache_ainfo_ptr->last_frame_nr)
        {
           cur_ptr->src_frame_idx_dbl = (gdouble)pvals->cache_ainfo_ptr->last_frame_nr;
        }
        break;
      case GAP_STEP_FRAME_PING_PONG:
        cur_ptr->src_frame_idx_dbl += (l_ping * l_step_speed_factor);
        if(l_ping < 0)
        {
          if(cur_ptr->src_frame_idx_dbl < (gdouble)(pvals->cache_ainfo_ptr->first_frame_nr -0.5))
          {
             cur_ptr->src_frame_idx_dbl = (gdouble)pvals->cache_ainfo_ptr->first_frame_nr + 1;
             l_ping = 1;
          }
          l_round = 0.5;
        }
        else
        {
          if(cur_ptr->src_frame_idx_dbl >= (gdouble)(pvals->cache_ainfo_ptr->last_frame_nr +1))
          {
             cur_ptr->src_frame_idx_dbl = (gdouble)pvals->cache_ainfo_ptr->last_frame_nr - 1;
             l_ping = -1;
          }
        }
        break;
      case GAP_STEP_FRAME_LOOP_REV:
        cur_ptr->src_frame_idx_dbl -= l_step_speed_factor;
        if(cur_ptr->src_frame_idx_dbl < (gdouble)(pvals->cache_ainfo_ptr->first_frame_nr -0.5))
        {
           cur_ptr->src_frame_idx_dbl += (gdouble)pvals->cache_ainfo_ptr->frame_cnt;
        }
        l_round = 0.5;
        break;
      case GAP_STEP_FRAME_LOOP:
      default:
        cur_ptr->src_frame_idx_dbl += l_step_speed_factor;
        if(cur_ptr->src_frame_idx_dbl >= (gdouble)(pvals->cache_ainfo_ptr->last_frame_nr +1))
        {
           cur_ptr->src_frame_idx_dbl -= (gdouble)pvals->cache_ainfo_ptr->frame_cnt;
        }
        break;

    }
    cur_ptr->src_frame_idx = CLAMP((long)(cur_ptr->src_frame_idx_dbl + l_round)
                                  ,pvals->cache_ainfo_ptr->first_frame_nr
                                  ,pvals->cache_ainfo_ptr->last_frame_nr
                                  );
    gap_mov_render_fetch_src_frame(pvals, cur_ptr->src_frame_idx);
  }
}       /* end  p_advance_src_frame */






/* -----------------------------------
 * p_calculate_relframe_nr_at_index
 * -----------------------------------
 * IN points is the number of processing relevant controlpoints
 * returns the index of the last controlpoint in the path segment that starts at specified index
 * Note that path segment includes all controlpoints up to the next keyframe inclusive. If no keyframe
 * present there is only one big segment from first to last controlpoint.
 */
static gint
p_calculate_relframe_nr_at_index(GapMovValues *val_ptr, gint index, gint frames)
{
  if (index <= 0)
  {
    return (1);
  }

  if(index  < val_ptr->point_idx_max )
  {
    if (val_ptr->point[index].keyframe > 0)
    {
      return (1 + val_ptr->point[index].keyframe);
    }
  }

  return (MAX(1, (frames -1)));

}  /* end p_calculate_relframe_nr_at_index */


/* -----------------------------------
 * p_findEndOfSegmentIndex
 * -----------------------------------
 * IN points is the number of processing relevant controlpoints
 * returns the index of the last controlpoint in the segment that starts
 * at specified startOfSegmentIndex.
 * the segment ends at next KEYFRAME or at the last controlpoint (that is an implicite keyframe)
 */
static gint
p_findEndOfSegmentIndex(GapMovValues *val_ptr, gint startOfSegmentIndex, gint points)
{
  gint ii;

  for (ii= startOfSegmentIndex +1; ii < points; ii++)
  {
    if (val_ptr->point[ii].keyframe > 0)
    {
      /* checked controlpoint is a keyframe */
      return (ii);
    }
  }

  return (points -1);
}  /* end p_findEndOfSegmentIndex */


/* -----------------------------------
 * p_calculate_LineLength
 * -----------------------------------
 * returns the length (in pixels) between the controlpont at the specified index idx
 * and the next controlpoint at idx +1
 * returns 0 in case the specified index is out of valid range
 */
static gdouble
p_calculate_LineLength(GapMovValues *val_ptr, gint idx)
{
  gdouble dx;
  gdouble dy;
  gdouble len;

  if ((idx < 0) || (idx >= val_ptr->point_idx_max))
  {
    return (0.0);
  }

  dx = abs (val_ptr->point[idx].p_x - val_ptr->point[idx +1].p_x);
  dy = abs (val_ptr->point[idx].p_y - val_ptr->point[idx +1].p_y);

  len = sqrt((dx * dx) + (dy * dy));

  return (len);

}  /* end p_calculate_LineLength */


/* -----------------------------------
 * p_calculate_path_segment_length
 * -----------------------------------
 *
 */
static gdouble
p_calculate_path_segment_length(GapMovValues  *val_ptr
   , gint startOfSegmentIndex, gint endOfSegmentIndex)
{
  gint idx;
  gdouble lenSum;

  lenSum = 0;
  for(idx=startOfSegmentIndex; idx < endOfSegmentIndex; idx++)
  {
    lenSum += p_calculate_LineLength(val_ptr, idx);
  }
  return (lenSum);
}  /* end p_calculate_path_segment_length */


/* -----------------------------------
 * p_pick_controlpoint_at_curr_length
 * -----------------------------------
 * returns the relevant controlpoint index
 * at specified currentLen in pixels (eg. current position length within the specified
 * path segment that starts at controlpoint startOfSegmentIndex)
 *
 * IN:  startOfSegmentIndex
 * IN:  endOfSegmentIndex
 * IN:  pathSegmentLength length in pixels (0 at stastOfSegment, upto segment length)
 * IN:
 * OUT: flt_posfactor
 */
static gint
p_pick_controlpoint_at_curr_length(GapMovValues  *val_ptr
   , gint startOfSegmentIndex, gint endOfSegmentIndex, gdouble pathSegmentLength
   , gint accelerationCharacteristic, gdouble lengthFactorLinear
   , gdouble *flt_posfactor
   )
{
   gint idx;
   gdouble lenSum;
   gdouble lenSumPrev;
   gdouble currentLen;
   gdouble lengthFactorAcc;         /* 0.0 at begin of segment 1.0 at end of segment position */

   /* calculate length factor (0.0 to 1.0) respecting position specific acceleartion characteristic */
   lengthFactorAcc = gap_accelMixFactor(lengthFactorLinear, accelerationCharacteristic);
   currentLen = pathSegmentLength * lengthFactorAcc;




   lenSum = 0;
   lenSumPrev = 0;

   for(idx = startOfSegmentIndex; idx <= endOfSegmentIndex; idx++)
   {
     gdouble lineLen;

     lineLen = p_calculate_LineLength(val_ptr, idx);
     lenSum += lineLen;

     if(lenSum >= currentLen)
     {
       gdouble l_flt_posfactor;
       l_flt_posfactor = (currentLen - lenSumPrev) / MAX(1.0, lineLen);

       if(gap_debug)
       {
         printf("  p_pick_controlpoint_at_curr_length[%d]  lenSum:%f currentLen:%f lengthFactorAcc:%f\n"
            ,(int)idx
            ,(float) lenSum
            ,(float) currentLen
            ,(float) lengthFactorAcc
            );
       }


       *flt_posfactor =  CLAMP (l_flt_posfactor, 0.0, 1.0);
       return (idx);
     }
    lenSumPrev = lenSum;
  }

  *flt_posfactor = 0;
  return (endOfSegmentIndex);

}  /* end p_pick_controlpoint_at_curr_length */

/* --------------------------------------
 * p_calculate_posFactor_from_FrameTweens
 * --------------------------------------
 * returns the relevant positionFactor for the
 * specified currFrameTweenInSegment and frameTweensInSegment
 * respecting the specified acceleration characteristic
 *
 */
static gdouble
p_calculate_posFactor_from_FrameTweens(gdouble frameTweensInSegment
   , gdouble currFrameTweenInSegment
   , gint accelerationCharacteristic
   )
{
  gdouble factorLinear;         /* 0.0 at begin of segment 1.0 at end of segment */
  gdouble posFactorAcc;         /* 0.0 at begin of segment 1.0 at end of segment */

  factorLinear = currFrameTweenInSegment / (MAX (1.0, frameTweensInSegment));
  factorLinear = CLAMP (factorLinear, 0.0, 1.0);

  /* calculate length factor (0.0 to 1.0) respecting position specific acceleartion characteristic */
  posFactorAcc = gap_accelMixFactor(factorLinear, accelerationCharacteristic);


  return (posFactorAcc);

}  /* end p_calculate_posFactor_from_FrameTweens */


/* -------------------------------------------
 * p_calculate_settings_for_current_FrameTween
 * -------------------------------------------
 * calculate settings for the currently processed
 * Frame (or tween) according to the controlpoints.
 * supports older behavior of GIMP-GAP-2.6 and prior
 * and the extended variant with accerlaration characteristics
 * per path segment.
 * mode without acceleration characteristic (GIMP-GAP-2.6 behvior)
 * ========================================
 *   This mode is selected by acceleration characteristic value 0.
 *   In this mode the flt_posfactor specifies the position within one line
 *   between the controlpoints with index [l_ptidx -1] and [l_ptidx]
 *
 * Mode with acceleration characteristic:
 * ========================================
 *   a Path segment includes all controlpoints between two keyframes
 *   (e.g controlpoints where keyframe > 0) Note that first and last contolpoint
 *   are implicite keyframes.
 *   In case none of the contolpoints has keyframe > 0, all controlpoints
 *   are in just one segment that starts at first and ends at last controlpoint.
 *
 *   The current movement position depends on the length of the current
 *   path segment and on the specified acceleration characteristic for movement.
 *   acc characteristic 1 is constant speed mode, 2 or more is acceleration, -2 or less deceleration
 *   The line within the path segment is calculated by picking
 *   the relevant ctrlPtidx.
 *
 *   in case acceleration characteristic values != 0 are used for any other settings than position,
 *   then all controlpoints that are NON keyframes are ignored for other settings than position (x,y).
 *   Those settings (opacity, size ...) are then calculated from the
 *   controlpoint at start and end of the path segment.
 *   (note that first and last controlpoint are
 *    implicite keyframes and therefore always relevant)
 */
static gint
p_calculate_settings_for_current_FrameTween(
     GapMovValues  *val_ptr
   , GapMovCurrent *cur_ptr
   , gdouble framesPerLine
   , long     currFrameIndex    /* relative frame number where the first handled frame is 0
                                 * Note that the first fame is already processed outside the loop
                                 * therefore this procedure is typically called first time with
                                 * currFrameIndex value 1.
                                 */
   , long     currPtidx         /* current controlpoint index for processing without acceleration characteristic */
   , gdouble  flt_posfactor     /* current position factor within one line between controlpoints
                                 * (relevant for processing without acceleration characteristic)
                                 */
   , gdouble  affectedFrames        /* number of affected frames (in the whole Move patch operation) */
   , long     availableCtrlPoints   /* number of available controlpoints */
   , gint     startOfSegmentIndex
   , gint     endOfSegmentIndex
   , GapMovQuery *mov_query
   )
{
  gint     frameNrAtEndOfSegment;

  gdouble tweenMultiplicator;
  gdouble lengthFactorLinear;      /* 0.0 at begin of segment 1.0 at end of segment position */
  gdouble frameTweensInSegment;
  gdouble currFrameTweenInSegment; /* frame number relative to 0 at each starting point of a new segment
                                    * tweens can have non integer frame number like 1.5 or 1.3333 or 1.25 etc....
                                    */
  gdouble pathSegmentLength;
  gdouble  posFactor;
  gdouble  posFactorMovement;
  gint     segmPtidx;              /* calculated controlpoint index of relevant line begin with current segment */
  gdouble   prevX;
  gdouble   prevY;

  posFactor = 0.0;
  posFactorMovement = 0.0;
  segmPtidx = -1;                  /* inital value -1 indicates that movement does NOT use acceleration characteristic */


  prevX = cur_ptr->currX;
  prevY = cur_ptr->currY;

  tweenMultiplicator = 1.0;
  if(val_ptr->tween_steps > 1.0)
  {
    tweenMultiplicator = val_ptr->tween_steps +1;
  }

  frameNrAtEndOfSegment = p_calculate_relframe_nr_at_index(val_ptr, endOfSegmentIndex, affectedFrames);

  frameTweensInSegment = abs ( frameNrAtEndOfSegment
                             - p_calculate_relframe_nr_at_index(val_ptr, startOfSegmentIndex, affectedFrames)
                             ) + 1;
  frameTweensInSegment *= tweenMultiplicator;



  currFrameTweenInSegment =
     tweenMultiplicator * (abs (currFrameIndex - val_ptr->point[startOfSegmentIndex].keyframe)); 
  currFrameTweenInSegment += (val_ptr->tween_steps - val_ptr->twix);

  /* calculate length factor respecting position in the path segment where 0 is at begin 1 at end */
  lengthFactorLinear = currFrameTweenInSegment / MAX(frameTweensInSegment, 1);

  pathSegmentLength = p_calculate_path_segment_length(val_ptr, startOfSegmentIndex, endOfSegmentIndex);

  if(gap_debug)
  {
    printf("p_mov_execute:startOfSegmentIndex:%d accPosition:%d pathSegmentLength:%.4f currFrameIndex:%d\n"
          ,(int)startOfSegmentIndex
          ,(int)val_ptr->point[startOfSegmentIndex].accPosition
          ,(float)pathSegmentLength
          ,(int)currFrameIndex
          );
  }

  cur_ptr->accPosition         = val_ptr->point[startOfSegmentIndex].accPosition;
  cur_ptr->accOpacity          = val_ptr->point[startOfSegmentIndex].accOpacity;
  cur_ptr->accSize             = val_ptr->point[startOfSegmentIndex].accSize;
  cur_ptr->accRotation         = val_ptr->point[startOfSegmentIndex].accRotation;
  cur_ptr->accPerspective      = val_ptr->point[startOfSegmentIndex].accPerspective;
  cur_ptr->accSelFeatherRadius = val_ptr->point[startOfSegmentIndex].accSelFeatherRadius;

  /* calculate Movement settings for the currently processed Frame (or tween)
   * position dependent acceleration processing requires a path segment length > 0
   * AND accPosition != 0
   */
  if ((val_ptr->point[startOfSegmentIndex].accPosition != 0)
  && (pathSegmentLength > 0))
  {
    /* acceleration characteristic processing */

    segmPtidx = p_pick_controlpoint_at_curr_length(val_ptr
         , startOfSegmentIndex, endOfSegmentIndex, pathSegmentLength
         , val_ptr->point[startOfSegmentIndex].accPosition
         , lengthFactorLinear
         , &posFactorMovement
         );

    cur_ptr->currX  =       GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].p_x,      (gdouble)val_ptr->point[segmPtidx +1].p_x);
    cur_ptr->currY  =       GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].p_y,      (gdouble)val_ptr->point[segmPtidx +1].p_y);

    if(gap_debug)
    {
       printf("p_mov_execute: currFrameIndex:%d Position, start/endOfSegmentIndex=%d/%d currFrameTweenInSegment=%d  frameTweensInSegment=%d\n"
             , (int)currFrameIndex
             , (int)startOfSegmentIndex
             , (int)endOfSegmentIndex
             , (int)currFrameTweenInSegment
             , (int)frameTweensInSegment
             );
       printf("p_mov_execute: Position, frameNrAtEndOfSegment=%d\n", (int)frameNrAtEndOfSegment);
       printf("p_mov_execute: Position, lengthFactorLinear=%f\n", (float)lengthFactorLinear);
       printf("p_mov_execute: Position, pathSegmentLength=%f\n", (float)pathSegmentLength);
       printf("p_mov_execute: Position, posFactorMovement=%f  segmPtidx=%d\n"
             , (float)posFactorMovement
             , (int)segmPtidx
             );
       printf("p_mov_execute: p_x[%d]:%.4f p_x[%d]:%.4f  currX:%.4f\n"
             , (int)segmPtidx
             , (float)val_ptr->point[segmPtidx].p_x
             , (int)segmPtidx+1
             , (float)val_ptr->point[segmPtidx +1].p_x
             , (float)cur_ptr->currX
             );
       printf("p_mov_execute: p_y[%d]:%.4f p_y[%d]:%.4f  currY:%.4f\n"
             , (int)segmPtidx
             , (float)val_ptr->point[segmPtidx].p_y
             , (int)segmPtidx+1
             , (float)val_ptr->point[segmPtidx +1].p_y
             , (float)cur_ptr->currY
             );
    }
  }
  else
  {
    /* No acceleration characteristic specified for movement (compatible to GAP 2.6.x release behavior) */
    if(gap_debug)
    {
      printf("p_mov_execute: framesPerLine=%f, flt_posfactor=%f\n"
           , (float)framesPerLine
           , (float)flt_posfactor
           );
    }


    cur_ptr->currX  =       GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].p_x,      (gdouble)val_ptr->point[currPtidx].p_x);
    cur_ptr->currY  =       GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].p_y,      (gdouble)val_ptr->point[currPtidx].p_y);
  }


  /* for the query mode calculate max speed in the query segment */
  if(mov_query != NULL)
  {
    gdouble dx;
    gdouble dy;
    gdouble pixelLengthInOneTweenStep;
    gint    refPtidx;                    /* index to controlpoint that marks end of current line */

    dx = fabs(cur_ptr->currX - prevX);
    dy = fabs(cur_ptr->currY - prevY);
    pixelLengthInOneTweenStep = sqrt((dx * dx) + (dy * dy));

    refPtidx = currPtidx;
    if (segmPtidx >= 0)
    {
      refPtidx = segmPtidx +1;
    }

    if(gap_debug)
    {
      printf("p_mov_execute: pixelLengthInOneTweenStep=%f  pointIndexToQuery:%d  currPtidx:%d refPtidx:%d StartQuery:%d EndQuery:%d\n"
           , (float)pixelLengthInOneTweenStep
           , (int)mov_query->pointIndexToQuery
           , (int)currPtidx
           , (int)refPtidx
           , (int)mov_query->startOfSegmentIndexToQuery
           , (int)mov_query->endOfSegmentIndexToQuery
           );
    }

    if((refPtidx >  mov_query->startOfSegmentIndexToQuery)
    && (refPtidx <= mov_query->endOfSegmentIndexToQuery))
    {
      if(mov_query->tweenCount == 0)
      {
        mov_query->tweenCount++;
        if(gap_debug)
        {
          printf("p_mov_execute: SKIP max speed calculation at first frame of segment\n");
        }
      }
      else
      {
        mov_query->maxSpeedInPixelsPerFrame = MAX(mov_query->maxSpeedInPixelsPerFrame, pixelLengthInOneTweenStep);
        if (mov_query->minSpeedInPixelsPerFrame < 0)
        {
           mov_query->minSpeedInPixelsPerFrame = pixelLengthInOneTweenStep;
        }
        else
        {
           mov_query->minSpeedInPixelsPerFrame = MIN(mov_query->minSpeedInPixelsPerFrame, pixelLengthInOneTweenStep);
        }
      }
      mov_query->pathSegmentLengthInPixels = pathSegmentLength;
    }



  }


  /* calculate Opacity settings for the currently processed Frame (or tween) */
  if ((val_ptr->point[startOfSegmentIndex].accOpacity != 0)
  && (frameTweensInSegment > 0))
  {
    /* acceleration characteristic processing for opacity */
    posFactor = p_calculate_posFactor_from_FrameTweens(frameTweensInSegment
                                                      , currFrameTweenInSegment
                                                      , val_ptr->point[startOfSegmentIndex].accOpacity
                                                      );
    cur_ptr->currOpacity  = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].opacity,  (gdouble)val_ptr->point[endOfSegmentIndex].opacity);
  }
  else
  {
    if(segmPtidx >= 0)
    {
      /* No acceleration characteristic specified for opacity, but movement uses accelertion characteristic
       * (in this case force frametiming synchronized to movement)
       */
      cur_ptr->currOpacity  = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].opacity,  (gdouble)val_ptr->point[segmPtidx +1].opacity);
    }
    else
    {
      /* No acceleration characteristic specified for opacity (compatible to GAP 2.6.x release behavior) */
      cur_ptr->currOpacity  = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].opacity,  (gdouble)val_ptr->point[currPtidx].opacity);
    }
  }


  /* calculate Zoom (e.g. Size) settings for the currently processed Frame (or tween) */
  if ((val_ptr->point[startOfSegmentIndex].accSize != 0)
  && (frameTweensInSegment > 0))
  {
    /* acceleration characteristic processing for opacity */
    posFactor = p_calculate_posFactor_from_FrameTweens(frameTweensInSegment
                                                      , currFrameTweenInSegment
                                                      , val_ptr->point[startOfSegmentIndex].accSize
                                                      );
    cur_ptr->currWidth    = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].w_resize, (gdouble)val_ptr->point[endOfSegmentIndex].w_resize);
    cur_ptr->currHeight   = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].h_resize, (gdouble)val_ptr->point[endOfSegmentIndex].h_resize);
  }
  else
  {
    if(segmPtidx >= 0)
    {
      /* No acceleration characteristic specified for zoom, but movement uses accelertion characteristic
       * (in this case force frametiming synchronized to movement)
       */
      cur_ptr->currWidth    = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].w_resize, (gdouble)val_ptr->point[segmPtidx +1].w_resize);
      cur_ptr->currHeight   = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].h_resize, (gdouble)val_ptr->point[segmPtidx +1].h_resize);
    }
    else
    {
      cur_ptr->currWidth    = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].w_resize, (gdouble)val_ptr->point[currPtidx].w_resize);
      cur_ptr->currHeight   = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].h_resize, (gdouble)val_ptr->point[currPtidx].h_resize);
    }
  }

  if(gap_debug)
  {
     printf("currWidth:%.4f\n"
       ,(float)cur_ptr->currWidth
       );
  }


  /* calculate Rotation settings for the currently processed Frame (or tween) */
  if ((val_ptr->point[startOfSegmentIndex].accRotation != 0)
  && (frameTweensInSegment > 0))
  {
    /* acceleration characteristic processing for rotation */
    posFactor = p_calculate_posFactor_from_FrameTweens(frameTweensInSegment
                                                      , currFrameTweenInSegment
                                                      , val_ptr->point[startOfSegmentIndex].accRotation
                                                      );
    cur_ptr->currRotation = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].rotation, (gdouble)val_ptr->point[endOfSegmentIndex].rotation);
  }
  else
  {
    if(segmPtidx >= 0)
    {
      /* No acceleration characteristic specified for rotation, but movement uses accelertion characteristic
       * (in this case force frametiming synchronized to movement)
       */
      cur_ptr->currRotation = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].rotation, (gdouble)val_ptr->point[segmPtidx +1].rotation);
      if(gap_debug)
      {
        printf("ROTATE [%02d] %f    [%02d] %f       MixFctor: %f  MixValue: %f\n"
          , (int)segmPtidx,      (float)val_ptr->point[currPtidx -1].rotation
          , (int)segmPtidx+1,    (float)val_ptr->point[currPtidx].rotation
          , (float)posFactorMovement
          , (float)cur_ptr->currRotation
          );
      }
    }
    else
    {
      cur_ptr->currRotation = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].rotation, (gdouble)val_ptr->point[currPtidx].rotation);

      if(gap_debug)
      {
        printf("p_mov_execute: framesPerLine=%f, flt_posfactor=%f\n"
           , (float)framesPerLine
           , (float)flt_posfactor
           );
        printf("ROTATE [%02d] %f    [%02d] %f       MIX: %f\n"
          , (int)currPtidx-1,  (float)val_ptr->point[currPtidx -1].rotation
          , (int)currPtidx,    (float)val_ptr->point[currPtidx].rotation
          , (float)cur_ptr->currRotation);
      }
    }

  }

  /* calculate Perspective settings for the currently processed Frame (or tween) */
  if ((val_ptr->point[startOfSegmentIndex].accPerspective != 0)
  && (frameTweensInSegment > 0))
  {
    /* acceleration characteristic processing for rotation */
    posFactor = p_calculate_posFactor_from_FrameTweens(frameTweensInSegment
                                                      , currFrameTweenInSegment
                                                      , val_ptr->point[startOfSegmentIndex].accPerspective
                                                      );
    cur_ptr->currTTLX     = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].ttlx,     (gdouble)val_ptr->point[endOfSegmentIndex].ttlx);
    cur_ptr->currTTLY     = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].ttly,     (gdouble)val_ptr->point[endOfSegmentIndex].ttly);
    cur_ptr->currTTRX     = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].ttrx,     (gdouble)val_ptr->point[endOfSegmentIndex].ttrx);
    cur_ptr->currTTRY     = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].ttry,     (gdouble)val_ptr->point[endOfSegmentIndex].ttry);
    cur_ptr->currTBLX     = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].tblx,     (gdouble)val_ptr->point[endOfSegmentIndex].tblx);
    cur_ptr->currTBLY     = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].tbly,     (gdouble)val_ptr->point[endOfSegmentIndex].tbly);
    cur_ptr->currTBRX     = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].tbrx,     (gdouble)val_ptr->point[endOfSegmentIndex].tbrx);
    cur_ptr->currTBRY     = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].tbry,     (gdouble)val_ptr->point[endOfSegmentIndex].tbry);
  }
  else
  {
    if(segmPtidx >= 0)
    {
      /* No acceleration characteristic specified for rotation, but movement uses accelertion characteristic
       * (in this case force frametiming synchronized to movement)
       */
      cur_ptr->currTTLX     = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].ttlx,     (gdouble)val_ptr->point[segmPtidx +1].ttlx);
      cur_ptr->currTTLY     = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].ttly,     (gdouble)val_ptr->point[segmPtidx +1].ttly);
      cur_ptr->currTTRX     = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].ttrx,     (gdouble)val_ptr->point[segmPtidx +1].ttrx);
      cur_ptr->currTTRY     = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].ttry,     (gdouble)val_ptr->point[segmPtidx +1].ttry);
      cur_ptr->currTBLX     = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].tblx,     (gdouble)val_ptr->point[segmPtidx +1].tblx);
      cur_ptr->currTBLY     = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].tbly,     (gdouble)val_ptr->point[segmPtidx +1].tbly);
      cur_ptr->currTBRX     = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].tbrx,     (gdouble)val_ptr->point[segmPtidx +1].tbrx);
      cur_ptr->currTBRY     = GAP_BASE_MIX_VALUE(posFactorMovement, (gdouble)val_ptr->point[segmPtidx].tbry,     (gdouble)val_ptr->point[segmPtidx +1].tbry);
    }
    else
    {
      cur_ptr->currTTLX     = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].ttlx,     (gdouble)val_ptr->point[currPtidx].ttlx);
      cur_ptr->currTTLY     = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].ttly,     (gdouble)val_ptr->point[currPtidx].ttly);
      cur_ptr->currTTRX     = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].ttrx,     (gdouble)val_ptr->point[currPtidx].ttrx);
      cur_ptr->currTTRY     = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].ttry,     (gdouble)val_ptr->point[currPtidx].ttry);
      cur_ptr->currTBLX     = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].tblx,     (gdouble)val_ptr->point[currPtidx].tblx);
      cur_ptr->currTBLY     = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].tbly,     (gdouble)val_ptr->point[currPtidx].tbly);
      cur_ptr->currTBRX     = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].tbrx,     (gdouble)val_ptr->point[currPtidx].tbrx);
      cur_ptr->currTBRY     = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].tbry,     (gdouble)val_ptr->point[currPtidx].tbry);
    }
  }

  /* calculate Selection Feather Radius settings for the currently processed Frame (or tween) */
  if ((val_ptr->point[startOfSegmentIndex].accSelFeatherRadius != 0)
  && (frameTweensInSegment > 0))
  {
    /* acceleration characteristic processing for rotation */
    posFactor = p_calculate_posFactor_from_FrameTweens(frameTweensInSegment
                                                      , currFrameTweenInSegment
                                                      , val_ptr->point[startOfSegmentIndex].accSelFeatherRadius
                                                      );
    cur_ptr->currSelFeatherRadius = GAP_BASE_MIX_VALUE(posFactor, (gdouble)val_ptr->point[startOfSegmentIndex].sel_feather_radius,     (gdouble)val_ptr->point[endOfSegmentIndex].sel_feather_radius);
  }
  else
  {
    cur_ptr->currSelFeatherRadius = GAP_BASE_MIX_VALUE(flt_posfactor, (gdouble)val_ptr->point[currPtidx -1].sel_feather_radius,     (gdouble)val_ptr->point[currPtidx].sel_feather_radius);
  }

  return(frameNrAtEndOfSegment);
}  /* end p_calculate_settings_for_current_FrameTween */


/* --------------------------------
 * p_log_current_render_params
 * --------------------------------
 * log current render parameters to stdout (for test and analyse purpose)
 */
static void
p_log_current_render_params(GapMovData *mov_ptr, GapMovCurrent *cur_ptr)
{
  gboolean defaultFlag;

  defaultFlag = FALSE;
  if(gap_debug)
  {
    defaultFlag = TRUE;
  }

  if(gap_base_get_gimprc_gboolean_value(GAP_MOVEPATH_GIMPRC_LOG_RENDER_PARAMS, defaultFlag))
  {
    GapMovValues *val_ptr;

    val_ptr = mov_ptr->val_ptr;

    printf("\nCurrent Render Params: dst_frame_nr:%ld tweenIndex:%d src_layer_idx:%ld\n"
                "       currX:%f currY:%f\n"
                "       Width:%f Height:%f\n"
                "       Opacity:%f  Rotate:%f  clip_to_img:%d force_visibility:%d\n"
                "       src_stepmode:%d handleX:%d handleY:%d currSelFeatherRadius:%f rotate_threshold:%f\n",
                     cur_ptr->dst_frame_nr, (int)val_ptr->twix, cur_ptr->src_layer_idx,
                     (float)cur_ptr->currX,
                     (float)cur_ptr->currY,
                     (float)cur_ptr->currWidth,
                     (float)cur_ptr->currHeight,
                     (float)cur_ptr->currOpacity,
                     (float)cur_ptr->currRotation,
                     val_ptr->clip_to_img,
                     val_ptr->src_force_visible,
                     val_ptr->src_stepmode,
                     cur_ptr->l_handleX,
                     cur_ptr->l_handleY,
                     (float)cur_ptr->currSelFeatherRadius,
                     (float)val_ptr->rotate_threshold
                     );

    printf("       Perspective Factors: [0] %.3f %.3f  [1] %.3f %.3f  [2] %.3f %.3f  [3] %.3f %.3f\n"
          ,(float)cur_ptr->currTTLX
          ,(float)cur_ptr->currTTLY
          ,(float)cur_ptr->currTTRX
          ,(float)cur_ptr->currTTRY
          ,(float)cur_ptr->currTBLX
          ,(float)cur_ptr->currTBLY
          ,(float)cur_ptr->currTBRX
          ,(float)cur_ptr->currTBRY
          );
    printf("       accPosition:%d accOpacity:%d accSize:%d accRotation:%d accPerspective:%d accSelFeatherRadius:%d\n"
          ,(int)cur_ptr->accPosition
          ,(int)cur_ptr->accOpacity
          ,(int)cur_ptr->accSize
          ,(int)cur_ptr->accRotation
          ,(int)cur_ptr->accPerspective
          ,(int)cur_ptr->accSelFeatherRadius
          );
  }

}  /* end p_log_current_render_params */


/* --------------------------------
 * p_printf_log_parameters
 * --------------------------------
 *
 */
static void
p_printf_log_parameters(GapMovData *mov_ptr)
{
  gint l_idx;
  
  printf("apv_mlayer_image: %ld\n", (long)mov_ptr->val_ptr->apv_mlayer_image);
  printf("apv_mode: %ld\n", (long)mov_ptr->val_ptr->apv_mode);
  printf("apv_scale x: %f y:%f\n", (float)mov_ptr->val_ptr->apv_scalex, (float)mov_ptr->val_ptr->apv_scaley);
  if(mov_ptr->val_ptr->apv_gap_paste_buff)
  {
    printf("apv_gap_paste_buf: %s\n", mov_ptr->val_ptr->apv_gap_paste_buff);
  }
  else
  {
    printf("apv_gap_paste_buf: ** IS NULL ** (do not copy to paste buffer)\n");
  }
  printf("src_image_id :%ld\n", (long)mov_ptr->val_ptr->src_image_id);
  printf("src_layer_id :%ld\n", (long)mov_ptr->val_ptr->src_layer_id);
  printf("src_handle :%d\n", mov_ptr->val_ptr->src_handle);
  printf("src_stepmode :%d\n", mov_ptr->val_ptr->src_stepmode);
  printf("src_paintmode :%d\n", mov_ptr->val_ptr->src_paintmode);
  printf("clip_to_img :%d\n", mov_ptr->val_ptr->clip_to_img);
  printf("dst_range_start :%d\n", (int)mov_ptr->val_ptr->dst_range_start);
  printf("dst_range_end :%d\n", (int)mov_ptr->val_ptr->dst_range_end);
  printf("dst_layerstack :%d\n", (int)mov_ptr->val_ptr->dst_layerstack);
  for(l_idx = 0; l_idx <= mov_ptr->val_ptr->point_idx_max; l_idx++)
  {
    printf("p_x[%d] :%d\n", l_idx, mov_ptr->val_ptr->point[l_idx].p_x);
    printf("p_y[%d] :%d\n", l_idx, mov_ptr->val_ptr->point[l_idx].p_y);
    printf("opacity[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].opacity);
    printf("w_resize[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].w_resize);
    printf("h_resize[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].h_resize);
    printf("rotation[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].rotation);
    printf("ttlx[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].ttlx);
    printf("ttly[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].ttly);
    printf("ttrx[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].ttrx);
    printf("ttry[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].ttry);
    printf("tblx[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].tblx);
    printf("tbly[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].tbly);
    printf("tbrx[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].tbrx);
    printf("tbry[%d] :%.3f\n", l_idx, (float)mov_ptr->val_ptr->point[l_idx].tbry);

    printf("accPosition        [%d] :%d\n", l_idx, (int)mov_ptr->val_ptr->point[l_idx].accPosition);
    printf("accOpacity         [%d] :%d\n", l_idx, (int)mov_ptr->val_ptr->point[l_idx].accOpacity);
    printf("accSize            [%d] :%d\n", l_idx, (int)mov_ptr->val_ptr->point[l_idx].accSize);
    printf("accRotation        [%d] :%d\n", l_idx, (int)mov_ptr->val_ptr->point[l_idx].accRotation);
    printf("accPerspective     [%d] :%d\n", l_idx, (int)mov_ptr->val_ptr->point[l_idx].accPerspective);
    printf("accSelFeatherRadius[%d] :%d\n", l_idx, (int)mov_ptr->val_ptr->point[l_idx].accSelFeatherRadius);

    printf("keyframe[%d] :%d\n", l_idx, mov_ptr->val_ptr->point[l_idx].keyframe);
    printf("keyframe_abs[%d] :%d\n", l_idx, mov_ptr->val_ptr->point[l_idx].keyframe_abs);
  }
  printf("\n");

}  /* end p_printf_log_parameters */


/* -------------------------------------------
 * gap_mov_exec_set_handle_offsets_singleframe
 * -------------------------------------------
 *
 */
static void
gap_mov_exec_set_handle_offsets_singleframe(GapMovValues *val_ptr, GapMovCurrent *cur_ptr)
{
  guint    l_src_width, l_src_height;         /* dimensions of the source image */

   /* get dimensions of source image (in single frame mode: same as frame image)  */
   l_src_width  = gimp_image_width(cur_ptr->singleMovObjImageId);
   l_src_height = gimp_image_height(cur_ptr->singleMovObjImageId);

   cur_ptr->l_handleX = 0.0;
   cur_ptr->l_handleY = 0.0;
   switch(val_ptr->src_handle)
   {
      case GAP_HANDLE_LEFT_BOT:
         cur_ptr->l_handleY += l_src_height;
         break;
      case GAP_HANDLE_RIGHT_TOP:
         cur_ptr->l_handleX += l_src_width;
         break;
      case GAP_HANDLE_RIGHT_BOT:
         cur_ptr->l_handleX += l_src_width;
         cur_ptr->l_handleY += l_src_height;
         break;
      case GAP_HANDLE_CENTER:
         cur_ptr->l_handleX += (l_src_width  / 2);
         cur_ptr->l_handleY += (l_src_height / 2);
         break;
      case GAP_HANDLE_LEFT_TOP:
      default:
         break;
   }
}  /* end gap_mov_exec_set_handle_offsets_singleframe */



/* --------------------------------
 * p_add_2nd_controlpoint
 * --------------------------------
 *
 */
static void
p_add_2nd_controlpoint(GapMovValues *val_ptr)
{
  /* copy point[0] to point [1] because we need at least 2
   * points for the algorithms below to work.
   * (simulates a line with length 0, to move along)
   */
  if(gap_debug)
  {
    printf("p_mov_execute: added a 2nd Point\n");
  }
  val_ptr->point[1].p_x = val_ptr->point[0].p_x;
  val_ptr->point[1].p_y = val_ptr->point[0].p_y;
  val_ptr->point[1].opacity = val_ptr->point[0].opacity;
  val_ptr->point[1].w_resize = val_ptr->point[0].w_resize;
  val_ptr->point[1].h_resize = val_ptr->point[0].h_resize;
  val_ptr->point[1].rotation = val_ptr->point[0].rotation;
  val_ptr->point[1].ttlx = val_ptr->point[0].ttlx;
  val_ptr->point[1].ttly = val_ptr->point[0].ttly;
  val_ptr->point[1].ttrx = val_ptr->point[0].ttrx;
  val_ptr->point[1].ttry = val_ptr->point[0].ttry;
  val_ptr->point[1].tblx = val_ptr->point[0].tblx;
  val_ptr->point[1].tbly = val_ptr->point[0].tbly;
  val_ptr->point[1].tbrx = val_ptr->point[0].tbrx;
  val_ptr->point[1].tbry = val_ptr->point[0].tbry;
  val_ptr->point[1].sel_feather_radius = val_ptr->point[0].sel_feather_radius;

  val_ptr->point[1].accPosition         = val_ptr->point[0].accPosition;
  val_ptr->point[1].accOpacity          = val_ptr->point[0].accOpacity;
  val_ptr->point[1].accSize             = val_ptr->point[0].accSize;
  val_ptr->point[1].accRotation         = val_ptr->point[0].accRotation;
  val_ptr->point[1].accPerspective      = val_ptr->point[0].accPerspective;
  val_ptr->point[1].accSelFeatherRadius = val_ptr->point[0].accSelFeatherRadius;

}  /* end p_add_2nd_controlpoint */


/* -------------------------------------
 * p_init_curr_ptr_with_1st_controlpoint
 * -------------------------------------
 * init current values with settings from the 1st controlpoint
 * in case of single frame processing mode (singleFramePtr != NULL)
 * init the id of the relevant layer (singleMovObjLayerId) that may be
 * already part of the processed frame or may be a layer in another image.
 */
static void
p_init_curr_ptr_with_1st_controlpoint(GapMovCurrent *cur_ptr, GapMovValues *val_ptr, GapMovSingleFrame *singleFramePtr)
{
  cur_ptr->currX   = (gdouble)val_ptr->point[0].p_x;
  cur_ptr->currY   = (gdouble)val_ptr->point[0].p_y;
  cur_ptr->currOpacity  = (gdouble)val_ptr->point[0].opacity;
  cur_ptr->currWidth    = (gdouble)val_ptr->point[0].w_resize;
  cur_ptr->currHeight   = (gdouble)val_ptr->point[0].h_resize;
  cur_ptr->currRotation = (gdouble)val_ptr->point[0].rotation;
  cur_ptr->currTTLX = (gdouble)val_ptr->point[0].ttlx;
  cur_ptr->currTTLY = (gdouble)val_ptr->point[0].ttly;
  cur_ptr->currTTRX = (gdouble)val_ptr->point[0].ttrx;
  cur_ptr->currTTRY = (gdouble)val_ptr->point[0].ttry;
  cur_ptr->currTBLX = (gdouble)val_ptr->point[0].tblx;
  cur_ptr->currTBLY = (gdouble)val_ptr->point[0].tbly;
  cur_ptr->currTBRX = (gdouble)val_ptr->point[0].tbrx;
  cur_ptr->currTBRY = (gdouble)val_ptr->point[0].tbry;
  cur_ptr->currSelFeatherRadius = (gdouble)val_ptr->point[0].sel_feather_radius;

  cur_ptr->accPosition         = val_ptr->point[0].accPosition;
  cur_ptr->accOpacity          = val_ptr->point[0].accOpacity;
  cur_ptr->accSize             = val_ptr->point[0].accSize;
  cur_ptr->accRotation         = val_ptr->point[0].accRotation;
  cur_ptr->accPerspective      = val_ptr->point[0].accPerspective;
  cur_ptr->accSelFeatherRadius = val_ptr->point[0].accSelFeatherRadius;


  cur_ptr->isSingleFrame = FALSE;
  cur_ptr->processedLayerId = -1;
  cur_ptr->singleMovObjLayerId = -1;
  cur_ptr->singleMovObjImageId = -1;
  cur_ptr->keep_proportions = FALSE;
  cur_ptr->fit_width = FALSE;
  cur_ptr->fit_height = FALSE;

  if (singleFramePtr != NULL)
  {
    cur_ptr->isSingleFrame = TRUE;
    cur_ptr->singleMovObjLayerId = singleFramePtr->drawable_id;
    cur_ptr->singleMovObjImageId = gimp_drawable_get_image(cur_ptr->singleMovObjLayerId);
    cur_ptr->keep_proportions = singleFramePtr->keep_proportions;
    cur_ptr->fit_width = singleFramePtr->fit_width;
    cur_ptr->fit_height = singleFramePtr->fit_height;
  }

  val_ptr->tween_image_id = -1;
  val_ptr->tween_layer_id = -1;
  val_ptr->trace_image_id = -1;
  val_ptr->trace_layer_id = -1;

}  /* end p_init_curr_ptr_with_1st_controlpoint */


/* --------------------------------
 * p_duplicate_layer
 * --------------------------------
 *
 */
static gint32
p_duplicate_layer(gint32 layerId)
{
  gint32 dupLayerId;
  gint32 imageId;


  imageId = gimp_drawable_get_image(layerId);
  gimp_image_set_active_layer(imageId, layerId);
  dupLayerId = gimp_layer_copy(layerId);
  gimp_image_insert_layer(imageId, dupLayerId, 0, -1 /* -1 place above active layer */);
  
  return(dupLayerId);
  
}  /* end p_duplicate_layer */




/* -----------------------
 * p_mov_execute_or_query
 * -----------------------
 * Copy layer(s) from Sourceimage to given destination frame range,
 * varying koordinates and opacity and perform other transformations
 * according to controlpoint settings on the copied layer.
 * To each affected destination frame one copy of a source layer is added.
 * (more than one layer is added in case of tween processing and tracelayer processing)
 * The source layer is iterated through all layers of the sourceimage
 * according to stemmode parameter.
 * For the placement the layers act as if their size is equal to their
 * Sourceimages size.
 */
long
p_mov_execute_or_query(GapMovData *mov_ptr, GapMovQuery *mov_query)
{
   GapMovCurrent l_current_data;
   GapMovCurrent *cur_ptr;
   GapMovValues  *val_ptr;

   gdouble  l_percentage;
   gdouble  l_fpl;             /* frames_per_line */
   long     l_frame_step;
   gdouble  l_frames;
   long     l_cnt;
   long     l_points;
   long     l_ptidx;
   long     l_prev_keyptidx;
   long     l_fridx;
   gdouble  l_flt_count;
   gint     l_rc;
   gint     l_nlayers;
   gint     l_idk;
   gint     l_prev_keyframe;
   gint     l_apv_layerstack;
   gdouble  l_flt_timing[GAP_MOV_MAX_POINT];   /* timing table in relative frame numbers (0.0 == the first handled frame) */

   gint startOfSegmentIndex;
   gint endOfSegmentIndex;
   gint frameNrAtEndOfSegment;


   if(mov_ptr->val_ptr->src_image_id < 0)
   {
      gap_arr_msg_win(mov_ptr->dst_ainfo_ptr->run_mode,
        _("No source image was selected.\n"
          "Please open a 2nd image of the same type before opening 'Move Path'."));
      return -1;
   }

  frameNrAtEndOfSegment = 0;
  l_apv_layerstack = 0;
  l_percentage = 0.0;

  if((mov_ptr->dst_ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
  && (mov_query == NULL))
  {
    if(mov_ptr->val_ptr->apv_mlayer_image < 0)
    {
      gimp_progress_init( _("Copying layers into frames..."));
    }
    else
    {
      gimp_progress_init( _("Generating animated preview..."));
    }
  }

  if(gap_debug)
  {
    printf("p_mov_execute: values got from dialog:\n");
    p_printf_log_parameters(mov_ptr);
  }

   l_rc    = 0;
   cur_ptr = &l_current_data;
   val_ptr = mov_ptr->val_ptr;

   if(mov_query == NULL)
   {
     if(gap_image_is_alive(val_ptr->tmpsel_image_id))
     {
       gimp_image_delete(val_ptr->tmpsel_image_id);
     }
   }

   val_ptr->tmpsel_image_id = -1;
   val_ptr->tmpsel_channel_id = -1;
   val_ptr->twix = 0;

   /* set offsets (in cur_ptr)  according to handle mode and src_img dimension */
   gap_mov_exec_set_handle_offsets(val_ptr, cur_ptr);


   /* test for invers range */
   if(val_ptr->dst_range_start > val_ptr->dst_range_end)
   {
      /* step down */
      l_frame_step = -1;
      l_cnt = 1 + (val_ptr->dst_range_start - val_ptr->dst_range_end);
   }
   else
   {
      l_frame_step = 1;
      l_cnt = 1 + (val_ptr->dst_range_end - val_ptr->dst_range_start);
   }

   l_frames = (gdouble)l_cnt;              /* nr. of affected frames */
   l_points = val_ptr->point_idx_max +1;   /* nr. of available points */

   if(l_points > l_frames)
   {
      /* cut off some points if we got more than frames */
      l_points = l_cnt;
   }

   if(l_points < 2)
   {
     p_add_2nd_controlpoint(val_ptr);
     l_points = 2;
   }

   startOfSegmentIndex = 0;
   endOfSegmentIndex = l_points -1;  /* initial value in case all points are in only 1 segment */

   cur_ptr->dst_frame_nr = val_ptr->dst_range_start;
   cur_ptr->src_layers = NULL;

   if(mov_query == NULL)
   {
     if(mov_ptr->val_ptr->src_stepmode < GAP_STEP_FRAME)
     {
       gint32        l_sel_channel_id;
       gboolean      l_all_empty;

       if(val_ptr->src_selmode != GAP_MOV_SEL_IGNORE)
       {
         l_all_empty = FALSE;
         if(gimp_selection_is_empty(val_ptr->src_image_id))
         {
           l_all_empty = TRUE;
         }
         l_sel_channel_id = gimp_image_get_selection(val_ptr->src_image_id);
         gap_mov_render_create_or_replace_tempsel_image(l_sel_channel_id, val_ptr, l_all_empty);
       }

       cur_ptr->src_layers = gimp_image_get_layers (val_ptr->src_image_id, &l_nlayers);
       if(cur_ptr->src_layers == NULL)
       {
         printf("ERROR (in p_mov_execute): Got no layers from SrcImage\n");
         return -1;
       }
       if(l_nlayers < 1)
       {
         printf("ERROR (in p_mov_execute): Source Image has no layers\n");
         return -1;
       }
       cur_ptr->src_last_layer = l_nlayers -1;

       /* findout index of src_layer_id */
       for(cur_ptr->src_layer_idx = 0;
           cur_ptr->src_layer_idx  < l_nlayers;
           cur_ptr->src_layer_idx++)
       {
          if(cur_ptr->src_layers[cur_ptr->src_layer_idx] == val_ptr->src_layer_id)
          {
             cur_ptr->src_layer_idx_dbl = (gdouble)cur_ptr->src_layer_idx;
             break;
          }
       }
       cur_ptr->src_last_layer = l_nlayers -1;   /* index of last layer */
     }
     else
     {
       /* for FRAME stepmodes we use flattened Sorce frames
        * (instead of one multilayer source image )
        */
       gap_mov_render_fetch_src_frame (val_ptr, -1);  /* negative value fetches the selected frame number */
       cur_ptr->src_frame_idx = val_ptr->cache_ainfo_ptr->curr_frame_nr;
       cur_ptr->src_frame_idx_dbl = (gdouble)cur_ptr->src_frame_idx;

       if((val_ptr->cache_ainfo_ptr->first_frame_nr < 0)
       && (val_ptr->src_stepmode != GAP_STEP_FRAME_NONE))
       {
          gap_lib_dir_ainfo(val_ptr->cache_ainfo_ptr);
       }

       /* set offsets (in cur_ptr)  according to handle mode and cache_tmp_img dimension */
       gap_mov_exec_set_handle_offsets(val_ptr, cur_ptr);
     }
   }

   p_init_curr_ptr_with_1st_controlpoint(cur_ptr, val_ptr, NULL);


   /* create temp images for tween processing and object tracing */
   if(mov_query == NULL)
   {
     gint32 master_image_id;

     if(mov_ptr->val_ptr->apv_mlayer_image < 0)
     {
       /* use original frame size for tween and trace images */
       master_image_id = mov_ptr->dst_ainfo_ptr->image_id;
     }
     else
     {
       /* use the (eventually downscaled) animation preview image size
        * for tween and trace images
        */
       master_image_id = mov_ptr->val_ptr->apv_mlayer_image;
     }

     /* optional create trace image */
     if (val_ptr->tracelayer_enable)
     {
       /* image of same size with one full transparent layer */
       val_ptr->trace_image_id = gap_image_new_with_layer_of_samesize(master_image_id
                                 , &val_ptr->trace_layer_id
                                 );
       gimp_image_undo_disable (val_ptr->trace_image_id);
       gimp_layer_add_alpha(val_ptr->trace_layer_id);
       gimp_edit_clear(val_ptr->trace_layer_id);
       gimp_drawable_set_name(val_ptr->trace_layer_id, _("Tracelayer"));
     }

     /* RENDER the 1.st frame outside the frameindex loop,
      * ------     ----
      * without care about tweens
      */
     l_rc = p_mov_call_render(mov_ptr, cur_ptr, l_apv_layerstack);

     /* optional create tween image */
     if (val_ptr->tween_steps > 0)
     {
       val_ptr->tween_image_id = gap_image_new_of_samesize(master_image_id);
       gimp_image_undo_disable (val_ptr->tween_image_id);
     }
   }


   /* how many frames are affected from one line of the moving path */
   l_fpl = ((gdouble)l_frames - 1.0) / ((gdouble)(l_points -1));
   if(gap_debug) printf("p_mov_execute: initial l_fpl=%f\n", l_fpl);

   /* calculate l_flt_timing controlpoint timing table considering keyframes */
   l_prev_keyptidx = 0;
   l_prev_keyframe = 0;
   l_flt_timing[0] = 0.0;
   l_flt_timing[l_points -1] = l_frames -1;
   l_flt_count = 0.0;
   for(l_ptidx=1;  l_ptidx < l_points - 1; l_ptidx++)
   {
     /* search for keyframes */
     if(l_ptidx > l_prev_keyptidx)
     {
       for(l_idk = l_ptidx; l_idk < l_points; l_idk++)
       {
          if(l_idk == l_points -1)
          {
            /* last point is always an implicite  keyframe */
            l_fpl = ((gdouble)((l_frames -1) - l_prev_keyframe)) / ((gdouble)((l_idk -  l_ptidx) +1));
            l_prev_keyframe = l_frames -1;

            l_prev_keyptidx = l_idk;
            if(gap_debug) printf("p_mov_execute: last point is implicite keyframe l_fpl=%f\n", l_fpl);
            break;
          }
          else
          {
            if (val_ptr->point[l_idk].keyframe > 0)
            {
              /* found a keyframe, have to recalculate frames_per_line */
              l_fpl = ((gdouble)(val_ptr->point[l_idk].keyframe - l_prev_keyframe)) / ((gdouble)((l_idk -  l_ptidx) +1));
              l_prev_keyframe = val_ptr->point[l_idk].keyframe;

              l_prev_keyptidx = l_idk;
              if(gap_debug) printf("p_mov_execute: keyframe l_fpl=%f\n", l_fpl);
              break;
            }
          }
       }
     }
     l_flt_count += l_fpl;
     l_flt_timing[l_ptidx] = l_flt_count;

     if((l_fpl < 1.0) && (mov_query == NULL))
     {
        printf("p_mov_execute: ** Error frames per line at point[%d] = %f  (is less than 1.0 !!)\n",
          (int)l_ptidx, (float)l_fpl);
     }
   }

   /* in query mode: find start end end of segment that contains the pointIndexToQuery */
   if(mov_query != NULL)
   {
     mov_query->tweenCount = 0;
     mov_query->startOfSegmentIndexToQuery = 0;
     mov_query->endOfSegmentIndexToQuery = l_points - 1;
     for(l_ptidx=1;  l_ptidx < l_points - 1; l_ptidx++)
     {
       if (val_ptr->point[l_ptidx].keyframe > 0)
       {
         if (l_ptidx <= mov_query->pointIndexToQuery)
         {
           mov_query->segmentNumber++;
           mov_query->startOfSegmentIndexToQuery = l_ptidx;
         }

         if (l_ptidx > mov_query->pointIndexToQuery)
         {
           mov_query->endOfSegmentIndexToQuery = l_ptidx;
           break;
         }
       }

     }

   }

   if(gap_debug)
   {
     printf("p_mov_execute: --- CONTROLPOINT relative frametiming TABLE -----\n");
     for(l_ptidx=0;  l_ptidx < l_points; l_ptidx++)
     {
       printf("p_mov_execute: l_flt_timing[%02d] = %f\n", (int)l_ptidx, (float)l_flt_timing[l_ptidx]);
     }
   }


  /* loop for each frame within the range (may step up or down) */
  l_ptidx = 1;
  cur_ptr->dst_frame_nr = val_ptr->dst_range_start;

  /* frameindex loop */
  for(l_fridx = 1; l_fridx < l_cnt; l_fridx++)
  {
     gdouble  l_tw_cnt;   /* number of tweens (including the real frame) 1 if no tweens present */

     if(gap_debug) printf("\np_mov_execute: l_fridx=%ld, l_flt_timing[l_ptidx]=%f, l_rc=%d l_ptidx=%d, l_prev_keyptidx=%d\n",
                           l_fridx, (float)l_flt_timing[l_ptidx], (int)l_rc, (int)l_ptidx, (int)l_prev_keyptidx);

     if(l_rc != 0)
     {
       break;
     }

      /* advance frame_nr, (1st frame was done outside this loop) */
      cur_ptr->dst_frame_nr += l_frame_step;  /* +1  or -1 */

      if((gdouble)l_fridx > l_flt_timing[l_ptidx])
      {
         /*  fix for object jumps forth and back as reported in #607927 */
         if(val_ptr->point[l_ptidx].keyframe > 0)
         {
           if((endOfSegmentIndex != l_points -1) && (endOfSegmentIndex > 0))
           {
             startOfSegmentIndex = endOfSegmentIndex;
           }
         }

         /* change deltas for next line of the move path */
         if(l_ptidx < l_points-1)
         {
           l_ptidx++;

           if(gap_debug)
           {
              printf("p_mov_execute: advance to controlpoint l_ptidx=%d, l_flt_timing[l_ptidx]=%f  startOfSegmentIndex:%d endOfSegmentIndex:%d\n"
                     , (int)l_ptidx
                     , (float)l_flt_timing[l_ptidx]
                     , (int)startOfSegmentIndex
                     , (int)endOfSegmentIndex
                     );
           }
         }
         else
         {
           if(gap_debug)
           {
             printf("p_mov_execute: ** ERROR overflow l_ptidx=%d\n", (int)l_ptidx);
           }
         }
      }

      l_fpl = (l_flt_timing[l_ptidx] - l_flt_timing[l_ptidx -1]); /* float frames per line */

      l_tw_cnt = (gdouble)(val_ptr->tween_steps +1);

      /* loop for the tweens
       * (tweens are virtual frames between the previous and the current frame)
       * when val_ptr->twix is down to 0, the current real frame is reached
       */
      for (val_ptr->twix = val_ptr->tween_steps; val_ptr->twix >= 0; val_ptr->twix--)
      {
        gdouble l_flt_posfactor;

        if(l_fpl != 0.0)
        {
            l_flt_posfactor  = (   (((gdouble)l_fridx * l_tw_cnt) - (gdouble)val_ptr->twix)
                               - (l_flt_timing[l_ptidx -1] * l_tw_cnt)
                             ) / (l_fpl * l_tw_cnt);
        }
        else
        {
            l_flt_posfactor = 1.0;
            if(gap_debug) printf("p_mov_execute: ** ERROR l_fpl is 0.0 frames per line\n");
        }

        l_flt_posfactor = CLAMP (l_flt_posfactor, 0.0, 1.0);

        endOfSegmentIndex = p_findEndOfSegmentIndex(val_ptr, startOfSegmentIndex, l_points);
        frameNrAtEndOfSegment = p_calculate_relframe_nr_at_index(val_ptr, endOfSegmentIndex, l_frames);

        frameNrAtEndOfSegment = p_calculate_settings_for_current_FrameTween(val_ptr, cur_ptr
             , l_fpl
             , l_fridx
             , l_ptidx
             , l_flt_posfactor
             , l_frames
             , l_points
             , startOfSegmentIndex
             , endOfSegmentIndex
             , mov_query
             );



        if(mov_query != NULL)
        {
          /* we run in query mode, just check if controlpoint for query is reached
           * and stop (without rendering)
           */
          if(gap_debug)
          {
            printf("BREAK check: endOfSegmentIndexToQuery:%d  l_ptidx:%d\n"
                 , (int)mov_query->endOfSegmentIndexToQuery
                 , (int)l_ptidx
                 );
          }

          if(l_ptidx > mov_query->endOfSegmentIndexToQuery)
          {
            return (l_rc);
          }
        }
        else
        {
          if(val_ptr->src_stepmode < GAP_STEP_FRAME )
          {
             /* advance settings for next src layer */
             p_mov_advance_src_layer(cur_ptr, val_ptr);
          }
          else
          {
            /* advance settings for next source frame */
            p_mov_advance_src_frame(cur_ptr, val_ptr);
          }

          if(l_frame_step < 0)
          {
            /* if we step down, we have to insert the layer
             * as lowest layer in the existing layerstack
             * of the animated preview multilayer image.
             * (if we step up, we always use 0 as l_apv_layerstack,
             *  that means always insert on top of the layerstack)
             */
            l_apv_layerstack++;
          }
          /* RENDER add current src_layer to current frame */
          l_rc = p_mov_call_render(mov_ptr, cur_ptr, l_apv_layerstack);
        }

      }  /* end tweenindex subloop */

      /* advance to next path segment */
      if(l_fridx >= frameNrAtEndOfSegment)
      {
         startOfSegmentIndex = endOfSegmentIndex;
      }

      /* show progress */
      if((mov_ptr->dst_ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
      && (mov_query == NULL))
      {
        l_percentage = (gdouble)l_fridx / (gdouble)(l_cnt -1);
        gimp_progress_update (l_percentage);
      }

   }  /* end frameindex loop */

   if(mov_query == NULL)
   {
     /* delete the tween image */
     if(val_ptr->tween_image_id >= 0)
     {
       gimp_image_delete(val_ptr->tween_image_id);
       val_ptr->tween_image_id = -1;
     }

     /* delete the trace image */
     if(val_ptr->trace_image_id >= 0)
     {
       gimp_image_delete(val_ptr->trace_image_id);
       val_ptr->trace_image_id = -1;
     }

     /* delete the temp selection image */
     if(gap_image_is_alive(val_ptr->tmpsel_image_id))
     {
       gimp_image_delete(val_ptr->tmpsel_image_id);
     }
   }

   val_ptr->tmpsel_image_id = -1;
   val_ptr->tmpsel_channel_id = -1;

   if(cur_ptr->src_layers != NULL) g_free(cur_ptr->src_layers);

   cur_ptr->src_layers = NULL;


   return l_rc;

}       /* end p_mov_execute_or_query */



/* ---------------------------
 * p_mov_execute_singleframe
 * ---------------------------
 * transform and move layer (specified by mov_ptr->singleFramePtr->drawable_id)
 * according to move path controlpoints in one destination frame
 * at the specified pahse (e.g. frame number within a frame range)
 *
 * in case the  mov_ptr->singleFramePtr->drawable_id is NOT already part
 * of the processed frame it will be copied to the frame and
 * the transformation is done on the copy 
 *  -- typical secnario when called from storyboard processor --
 * Otherwise transformation is done on the original
 *  -- typical scenario when called via PDB (as filter in the modify frames feature)
 *
 * return the layer_id of the processed resulting layer or -1 on error
 * (note that additional layers will be created when tween processing is active)
 */
static gint32
p_mov_execute_singleframe(GapMovData *mov_ptr)
{
   GapMovCurrent l_current_data;
   GapMovCurrent *cur_ptr;
   GapMovValues  *val_ptr;

   gdouble  l_percentage;
   gdouble  l_fpl;             /* frames_per_line */
   long     l_frame_step;
   gdouble  l_frames;
   long     l_cnt;
   long     l_points;
   long     l_ptidx;
   long     l_prev_keyptidx;
   long     l_fridx;
   gdouble  l_flt_count;
   gint32   l_rc;
   gint     l_idk;
   gint     l_prev_keyframe;
   gint     l_apv_layerstack;
   gdouble  l_flt_timing[GAP_MOV_MAX_POINT];   /* timing table in relative frame numbers (0.0 == the first handled frame) */

   gint startOfSegmentIndex;
   gint endOfSegmentIndex;
   gint frameNrAtEndOfSegment;
   GapMovSingleFrame *singleFramePtr;
   gint32            *tweenLayerIdTable;


  tweenLayerIdTable = NULL;
  singleFramePtr = mov_ptr->singleFramePtr;
  if(singleFramePtr == NULL)
  {
    printf("ERROR p_mov_execute_singleframe must not be called wit singleFramePtr NULL!\n");
  }


  frameNrAtEndOfSegment = 0;
  l_apv_layerstack = 0;
  l_percentage = 0.0;

  if(mov_ptr->dst_ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
  {
    gimp_progress_init( _("Transforming layer according to move path frame_phase..."));
  }

  if(gap_debug)
  {
    printf("p_mov_execute_singleframe: values got from dialog:\n");
    p_printf_log_parameters(mov_ptr);
  }

   l_rc    = 0;
   cur_ptr = &l_current_data;
   val_ptr = mov_ptr->val_ptr;

   if(gap_image_is_alive(val_ptr->tmpsel_image_id))
   {
     gimp_image_delete(val_ptr->tmpsel_image_id);
   }

   val_ptr->tmpsel_image_id = -1;
   val_ptr->tmpsel_channel_id = -1;
   val_ptr->twix = 0;


   /* only ascending range processing in single frames mode */
   l_frame_step = 1;
   if(singleFramePtr->total_frames > 0)
   {
     l_cnt = singleFramePtr->total_frames;
   }
   else
   {
     l_cnt = 1 + abs(val_ptr->dst_range_end - val_ptr->dst_range_start);
   }

   l_frames = (gdouble)l_cnt;              /* nr. of affected frames */
   l_points = val_ptr->point_idx_max +1;   /* nr. of available points */

   if(l_points > l_frames)
   {
      /* cut off some points if we got more than frames */
      l_points = l_cnt;
   }

   if(l_points < 2)
   {
     p_add_2nd_controlpoint(val_ptr);
     l_points = 2;
   }

   startOfSegmentIndex = 0;
   endOfSegmentIndex = l_points -1;  /* initial value in case all points are in only 1 segment */

   cur_ptr->dst_frame_nr = 1;
   cur_ptr->src_layers = NULL;
   cur_ptr->src_last_layer = -1;
   p_init_curr_ptr_with_1st_controlpoint(cur_ptr, val_ptr, singleFramePtr);

   /* set offsets (in cur_ptr)  according to handle mode and src_img dimension */
   gap_mov_exec_set_handle_offsets_singleframe(val_ptr, cur_ptr);

   /* mov_ptr->val_ptr->src_stepmode is ignored in singleframes mode
    * (e.g. behaves like GAP_STEP_FRAME_NONE)
    */
   {
     gint32        l_sel_channel_id;
     gboolean      l_all_empty;

     if(val_ptr->src_selmode != GAP_MOV_SEL_IGNORE)
     {
       l_all_empty = FALSE;
       if(gimp_selection_is_empty(cur_ptr->singleMovObjImageId))
       {
         l_all_empty = TRUE;
       }
       l_sel_channel_id = gimp_image_get_selection(cur_ptr->singleMovObjImageId);
       gap_mov_render_create_or_replace_tempsel_image(l_sel_channel_id, val_ptr, l_all_empty);
     }

   }


   /* create duplicate layers for tween processing (or render first frame) */
   {
     gint32 iTween;


     /* optional create tweens (n copies of the singleMovObjLayerId) */
     if (singleFramePtr->frame_phase > 1)
     {
       tweenLayerIdTable = g_new(gint32, val_ptr->tween_steps +1);
       tweenLayerIdTable[0] = cur_ptr->singleMovObjLayerId;
       for(iTween = 1; iTween <= val_ptr->tween_steps; iTween++)
       {
         /* make copies of the cur_ptr->singleMovObjLayerId (one copy foreach tween) */
         tweenLayerIdTable[iTween] = p_duplicate_layer(cur_ptr->singleMovObjLayerId);
       }
     }
     else
     {
       /* RENDER the 1.st frame outside the frameindex loop,
        * ------     ----
        * without care about tweens
        */
       l_rc = p_mov_call_render(mov_ptr, cur_ptr, l_apv_layerstack);
       if(l_rc >= 0)
       {
         l_rc = cur_ptr->processedLayerId;
       }
       return (l_rc);
     }

   }


   /* how many frames are affected from one line of the moving path */
   l_fpl = ((gdouble)l_frames - 1.0) / ((gdouble)(l_points -1));
   if(gap_debug)
   {
     printf("p_mov_execute_singleframe: initial l_fpl=%f\n", l_fpl);
   }

   /* calculate l_flt_timing controlpoint timing table considering keyframes */
   l_prev_keyptidx = 0;
   l_prev_keyframe = 0;
   l_flt_timing[0] = 0.0;
   l_flt_timing[l_points -1] = l_frames -1;
   l_flt_count = 0.0;
   for(l_ptidx=1;  l_ptidx < l_points - 1; l_ptidx++)
   {
     /* search for keyframes */
     if(l_ptidx > l_prev_keyptidx)
     {
       for(l_idk = l_ptidx; l_idk < l_points; l_idk++)
       {
          if(l_idk == l_points -1)
          {
            /* last point is always an implicite  keyframe */
            l_fpl = ((gdouble)((l_frames -1) - l_prev_keyframe)) / ((gdouble)((l_idk -  l_ptidx) +1));
            l_prev_keyframe = l_frames -1;

            l_prev_keyptidx = l_idk;
            if(gap_debug) printf("p_mov_execute_singleframe: last point is implicite keyframe l_fpl=%f\n", l_fpl);
            break;
          }
          else
          {
            if (val_ptr->point[l_idk].keyframe > 0)
            {
              /* found a keyframe, have to recalculate frames_per_line */
              l_fpl = ((gdouble)(val_ptr->point[l_idk].keyframe - l_prev_keyframe)) / ((gdouble)((l_idk -  l_ptidx) +1));
              l_prev_keyframe = val_ptr->point[l_idk].keyframe;

              l_prev_keyptidx = l_idk;
              if(gap_debug) printf("p_mov_execute_singleframe: keyframe l_fpl=%f\n", l_fpl);
              break;
            }
          }
       }
     }
     l_flt_count += l_fpl;
     l_flt_timing[l_ptidx] = l_flt_count;

     if(l_fpl < 1.0)
     {
        printf("p_mov_execute_singleframe: ** Error frames per line at point[%d] = %f  (is less than 1.0 !!)\n",
          (int)l_ptidx, (float)l_fpl);
     }
   }

   if(gap_debug)
   {
     printf("p_mov_execute_singleframe: --- CONTROLPOINT relative frametiming TABLE -----\n");
     for(l_ptidx=0;  l_ptidx < l_points; l_ptidx++)
     {
       printf("p_mov_execute_singleframe: l_flt_timing[%02d] = %f\n", (int)l_ptidx, (float)l_flt_timing[l_ptidx]);
     }
   }


  /* loop for each frame within the range (may step up or down) */
  l_ptidx = 1;
  cur_ptr->dst_frame_nr = 1;

  /* frameindex loop */
  for(l_fridx = 1; l_fridx < l_cnt; l_fridx++)
  {
     gdouble  l_tw_cnt;   /* number of tweens (including the real frame) 1 if no tweens present */
     gboolean isProcessingFramePhase;

     isProcessingFramePhase = FALSE;
     if((singleFramePtr->frame_phase == l_fridx +1)
     || (l_fridx +1 == l_cnt))
     {
       isProcessingFramePhase = TRUE;
     }

     if(gap_debug)
     {
       printf("\np_mov_execute_singleframe: l_fridx=%ld, l_flt_timing[l_ptidx]=%f, l_rc=%d l_ptidx=%d, l_prev_keyptidx=%d\n",
                           l_fridx, (float)l_flt_timing[l_ptidx], (int)l_rc, (int)l_ptidx, (int)l_prev_keyptidx);
       if (isProcessingFramePhase)
       {
         printf("Now l_fridx is the frame index that triggers processing relevant Single Frame Phase\n");
       }
     }
     if(l_rc != 0)
     {
       break;
     }

      /* advance frame_nr, (1st frame was done outside this loop) */
      cur_ptr->dst_frame_nr += l_frame_step;  /* +1  or -1 */

      if((gdouble)l_fridx > l_flt_timing[l_ptidx])
      {
         /*  fix for object jumps forth and back as reported in #607927 */
         if(val_ptr->point[l_ptidx].keyframe > 0)
         {
           if((endOfSegmentIndex != l_points -1) && (endOfSegmentIndex > 0))
           {
             startOfSegmentIndex = endOfSegmentIndex;
           }
         }

         /* change deltas for next line of the move path */
         if(l_ptidx < l_points-1)
         {
           l_ptidx++;

           if(gap_debug)
           {
              printf("p_mov_execute_singleframe: advance to controlpoint l_ptidx=%d, l_flt_timing[l_ptidx]=%f  startOfSegmentIndex:%d endOfSegmentIndex:%d\n"
                     , (int)l_ptidx
                     , (float)l_flt_timing[l_ptidx]
                     , (int)startOfSegmentIndex
                     , (int)endOfSegmentIndex
                     );
           }
         }
         else
         {
           if(gap_debug)
           {
             printf("p_mov_execute_singleframe: ** ERROR overflow l_ptidx=%d\n", (int)l_ptidx);
           }
         }
      }

      l_fpl = (l_flt_timing[l_ptidx] - l_flt_timing[l_ptidx -1]); /* float frames per line */

      l_tw_cnt = (gdouble)(val_ptr->tween_steps +1);

      /* loop for the tweens
       * (tweens are virtual frames between the previous and the current frame)
       * when val_ptr->twix is down to 0, the current real frame is reached
       */
      for (val_ptr->twix = val_ptr->tween_steps; val_ptr->twix >= 0; val_ptr->twix--)
      {
        gdouble l_flt_posfactor;

        if(l_fpl != 0.0)
        {
            l_flt_posfactor  = (   (((gdouble)l_fridx * l_tw_cnt) - (gdouble)val_ptr->twix)
                               - (l_flt_timing[l_ptidx -1] * l_tw_cnt)
                             ) / (l_fpl * l_tw_cnt);
        }
        else
        {
            l_flt_posfactor = 1.0;
            if(gap_debug) printf("p_mov_execute_singleframe: ** ERROR l_fpl is 0.0 frames per line\n");
        }

        l_flt_posfactor = CLAMP (l_flt_posfactor, 0.0, 1.0);

        endOfSegmentIndex = p_findEndOfSegmentIndex(val_ptr, startOfSegmentIndex, l_points);
        frameNrAtEndOfSegment = p_calculate_relframe_nr_at_index(val_ptr, endOfSegmentIndex, l_frames);

        frameNrAtEndOfSegment = p_calculate_settings_for_current_FrameTween(val_ptr, cur_ptr
             , l_fpl
             , l_fridx
             , l_ptidx
             , l_flt_posfactor
             , l_frames
             , l_points
             , startOfSegmentIndex
             , endOfSegmentIndex
             , NULL  /* mov_query */
             );



        if(l_frame_step < 0)
        {
            /* if we step down, we have to insert the layer
             * as lowest layer in the existing layerstack
             * of the animated preview multilayer image.
             * (if we step up, we always use 0 as l_apv_layerstack,
             *  that means always insert on top of the layerstack)
             */
            l_apv_layerstack++;
        }

        if (isProcessingFramePhase)
        {
          cur_ptr->singleMovObjLayerId = tweenLayerIdTable[val_ptr->twix];


          /* RENDER add current src_layer to current frame */
          l_rc = p_mov_call_render(mov_ptr, cur_ptr, l_apv_layerstack);
        }

      }  /* end tweenindex subloop */

      if (isProcessingFramePhase)
      {
        /* returncode handling (id of the resulting processed layer) */
        if(l_rc >= 0)
        {
          l_rc = cur_ptr->processedLayerId;
        }
        /* stop after the relevant single frame (and its tweens) was already processed */
        break;
      }

      /* advance to next path segment */
      if(l_fridx >= frameNrAtEndOfSegment)
      {
         startOfSegmentIndex = endOfSegmentIndex;
      }

      ///* show progress */  // TODO
      //if((mov_ptr->dst_ainfo_ptr->run_mode == GIMP_RUN_INTERACTIVE)
      //&& (mov_query == NULL))
      //{
      //  l_percentage = (gdouble)l_fridx / (gdouble)(l_cnt -1);
      //  gimp_progress_update (l_percentage);
      //}

   }  /* end frameindex loop */

   /* delete the temp selection image */
   if(gap_image_is_alive(val_ptr->tmpsel_image_id))
   {
     gimp_image_delete(val_ptr->tmpsel_image_id);
   }

   val_ptr->tmpsel_image_id = -1;
   val_ptr->tmpsel_channel_id = -1;

   if (tweenLayerIdTable != NULL)
   {
     g_free(tweenLayerIdTable);
   }

   return l_rc;

}       /* end p_mov_execute_singleframe */



/* ============================================================================
 * p_mov_execute
 * Copy layer(s) from Sourceimage to given destination frame range,
 * varying koordinates and opacity of the copied layer.
 * To each affected destination frame exactly one copy of a source layer is added.
 * The source layer is iterated through all layers of the sourceimage
 * according to stemmode parameter.
 * For the placement the layers act as if their size is equal to their
 * Sourceimages size.
 * ============================================================================
 */
static long
p_mov_execute(GapMovData *mov_ptr)
{
  GapMovQuery *mov_query;

  mov_query = NULL;
  return(p_mov_execute_or_query(mov_ptr, mov_query));
}




/* ============================================================================
 * gap_mov_exec_anim_preview
 *   Generate an animated preview for the move path
 * ============================================================================
 * the animate preview is rendered as new multilayer image
 * where each processed frame results in one layer.
 * processing can be done on empty frame, multiple copies of one frame
 * or by reading all affected frames as input (from frame images on disc)
 * Note that animated preview can be rendered at original size
 * or downscaled to a percentage less than 100%
 *
 * In case ainfo_ptr refers to an image that is not part of a sequence
 * of frameimages (stored on disc) a copy of the invoke image (ainfo_ptr->image_id)
 * will be used as input instead of reading frame images from disc.
 */
gint32
gap_mov_exec_anim_preview(GapMovValues *pvals_orig, GapAnimInfo *ainfo_ptr, gint preview_frame_nr)
{
  GapMovData    apv_mov_data;
  GapMovValues  apv_mov_vals;
  GapMovData    *l_mov_ptr;
  GapMovValues  *l_pvals;
  gint        l_idx;
  gint32      l_size_x, l_size_y;
  gint32      l_tmp_image_id;
  gint32      l_tmp_frame_id;
  gint32      l_mlayer_image_id;
  GimpImageBaseType  l_type;
  guint       l_width, l_height;
  gint32      l_stackpos;
  gint        l_nlayers;
  gint32     *l_src_layers;
  gint        l_rc;
  gdouble    l_xresoulution, l_yresoulution;
  gint32     l_unit;

  l_mov_ptr = &apv_mov_data;
  l_pvals = &apv_mov_vals;

  /* copy settings */
  memcpy(l_pvals, pvals_orig, sizeof(GapMovValues));
  l_mov_ptr->val_ptr = l_pvals;
  l_mov_ptr->dst_ainfo_ptr = ainfo_ptr;

  /* init local cached src image for anim preview generation.
   * (never mix cached src image for normal and anim preview
   *  because anim previews are often scaled down)
   */
  l_pvals->cache_src_image_id  = -1;
  l_pvals->cache_tmp_image_id  = -1;
  l_pvals->cache_tmp_layer_id  = -1;
  l_pvals->cache_frame_number  = -1;
  l_pvals->cache_ainfo_ptr = NULL;

  /* -1 assume no tmp_image (use unscaled original source) */
  l_tmp_image_id = -1;
  l_stackpos = 0;

  /* Scale (down) needed ? */
  if((l_pvals->apv_scalex != 100.0) || (l_pvals->apv_scaley != 100.0))
  {
    /* scale the controlpoint koords */
    for(l_idx = 0; l_idx <= l_pvals->point_idx_max; l_idx++)
    {
      l_pvals->point[l_idx].p_x  = (l_pvals->point[l_idx].p_x * l_pvals->apv_scalex) / 100;
      l_pvals->point[l_idx].p_y  = (l_pvals->point[l_idx].p_y * l_pvals->apv_scaley) / 100;
    }

    /* for the FRAME based step modes we cant Scale here,
     * we have to scale later (at fetch time of the frame)
     */
    if(l_pvals->src_stepmode < GAP_STEP_FRAME)
    {
      /* copy and scale the source object image */
      l_tmp_image_id = gimp_image_duplicate(pvals_orig->src_image_id);
      gimp_image_undo_disable (l_tmp_image_id);
      l_pvals->src_image_id = l_tmp_image_id;

      l_size_x = MAX(1, (gimp_image_width(l_tmp_image_id) * l_pvals->apv_scalex) / 100);
      l_size_y = MAX(1, (gimp_image_height(l_tmp_image_id) * l_pvals->apv_scaley) / 100);
      gimp_image_scale(l_tmp_image_id, l_size_x, l_size_y);

       /* findout the src_layer id in the scaled copy by stackpos index */
       l_pvals->src_layer_id = -1;
       l_src_layers = gimp_image_get_layers (pvals_orig->src_image_id, &l_nlayers);
       if(l_src_layers == NULL)
       {
         printf("ERROR: gap_mov_exec_anim_preview GOT no src_layers (original image_id %d)\n",
                 (int)pvals_orig->src_image_id);
       }
       else
       {
         for(l_stackpos = 0;
             l_stackpos  < l_nlayers;
             l_stackpos++)
         {
            if(l_src_layers[l_stackpos] == pvals_orig->src_layer_id)
               break;
         }
         g_free(l_src_layers);

         l_src_layers = gimp_image_get_layers (l_tmp_image_id, &l_nlayers);
         if(l_src_layers == NULL)
         {
           printf("ERROR: gap_mov_exec_anim_preview GOT no src_layers (scaled copy image_id %d)\n",
                  (int)l_tmp_image_id);
         }
         else
         {
            l_pvals->src_layer_id = l_src_layers[l_stackpos];
            g_free(l_src_layers);
         }

       }

      if(gap_debug)
      {
        printf("gap_mov_exec_anim_preview: orig  src_image_id:%d src_layer:%d, stackpos:%d\n"
               ,(int)pvals_orig->src_image_id
               ,(int)pvals_orig->src_layer_id
               ,(int)l_stackpos);
        printf("   Scaled src_image_id:%d scaled_src_layer:%d\n"
               ,(int)l_tmp_image_id
               ,(int)l_pvals->src_layer_id );
      }
    }
  }  /* end if Scaledown needed */


  if(gap_debug)
  {
    printf("gap_mov_exec_anim_preview: src_image_id %d (orig:%d)\n"
           , (int) l_pvals->src_image_id
           , (int) pvals_orig->src_image_id);
  }

  /* create the animated preview multilayer image in (scaled) framesize */
  l_width  = (gimp_image_width(ainfo_ptr->image_id) * l_pvals->apv_scalex) / 100;
  l_height = (gimp_image_height(ainfo_ptr->image_id) * l_pvals->apv_scaley) / 100;
  l_type   = gimp_image_base_type(ainfo_ptr->image_id);
  l_unit   = gimp_image_get_unit(ainfo_ptr->image_id);
  gimp_image_get_resolution(ainfo_ptr->image_id, &l_xresoulution, &l_yresoulution);

  l_mlayer_image_id = gimp_image_new(l_width, l_height,l_type);
  gimp_image_set_resolution(l_mlayer_image_id, l_xresoulution, l_yresoulution);
  gimp_image_set_unit(l_mlayer_image_id, l_unit);
  gimp_image_undo_disable (l_mlayer_image_id);

  l_pvals->apv_mlayer_image = l_mlayer_image_id;

  if(gap_debug)
  {
    printf("gap_mov_exec_anim_preview: apv_mlayer_image %d\n"
           , (int) l_pvals->apv_mlayer_image);
  }

  l_tmp_frame_id = -1;

  {
    gchar *l_filename;
    gboolean useOneFrame;

    useOneFrame = FALSE;
    l_filename = gap_lib_alloc_fname(ainfo_ptr->basename,
                                 preview_frame_nr,
                                 ainfo_ptr->extension);

    /* APV_MODE (Wich frames to use in the preview?)  */
    switch(l_pvals->apv_mode)
    {
      case GAP_APV_QUICK:
        /* use an empty dummy frame for all frames */
        l_tmp_frame_id = gimp_image_new(l_width, l_height,l_type);
        gimp_image_set_resolution(l_tmp_frame_id, l_xresoulution, l_yresoulution);
        gimp_image_undo_disable (l_tmp_frame_id);
        break;
      case GAP_APV_ONE_FRAME:
        /* use only one frame in the preview */
        useOneFrame = TRUE;
        break;
      default:  /* GAP_APV_EXACT */
        /* read the original frames for the preview (slow) */
        l_tmp_frame_id = -1;
        if(l_filename == NULL)
        {
          /* the frame for preview_frame_nr resulted in NULL filename
           * in this case we assume that there will be no valid frames available
           * and use only one frame (as copy of the invoker image)
           * for rendering the animated preview
           */
          useOneFrame = TRUE;
        }
        break;
    }
    
    if(useOneFrame)
    {
      if((l_filename != NULL)
      && (preview_frame_nr  != ainfo_ptr->curr_frame_nr))
      {
        l_tmp_frame_id =  gap_lib_load_image(l_filename);
      }
      if(l_tmp_frame_id < 0)
      {
        l_tmp_frame_id = gimp_image_duplicate(ainfo_ptr->image_id);
      }
      gimp_image_undo_disable (l_tmp_frame_id);
      if((l_pvals->apv_scalex != 100.0) || (l_pvals->apv_scaley != 100.0))
      {
        l_size_x = (gimp_image_width(l_tmp_frame_id) * l_pvals->apv_scalex) / 100;
        l_size_y = (gimp_image_height(l_tmp_frame_id) * l_pvals->apv_scaley) / 100;
        gimp_image_scale(l_tmp_frame_id, l_size_x, l_size_y);
      }
    }
    
    
    if(l_filename != NULL)
    {
      g_free(l_filename);
    }
  }

  l_pvals->apv_src_frame = l_tmp_frame_id;

  if(gap_debug)
  {
    printf("gap_mov_exec_anim_preview: apv_src_frame %d\n"
           , (int) l_pvals->apv_src_frame);
  }


  /* EXECUTE move path in preview Mode */
  /* --------------------------------- */
  l_rc = p_mov_execute(l_mov_ptr);

  if(l_pvals->cache_tmp_image_id >= 0)
  {
     if(gap_debug)
     {
        printf("gap_mov_exec_anim_preview: DELETE cache_tmp_image_id:%d\n",
                 (int)l_pvals->cache_tmp_image_id);
     }
     /* destroy the cached frame image */
     gimp_image_delete(l_pvals->cache_tmp_image_id);
     l_pvals->cache_tmp_image_id = -1;
  }

  if(l_rc < 0)
  {
    return(-1);
  }

  gimp_image_undo_enable (l_mlayer_image_id);

  /* add a display for the animated preview multilayer image */
  gimp_display_new(l_mlayer_image_id);

  /* delete the scaled copy of the src image (if there is one) */
  if(l_tmp_image_id >= 0)
  {
    gimp_image_delete(l_tmp_image_id);
  }
  /* delete the (scaled) dummy frames (if there is one) */
  if(l_tmp_frame_id >= 0)
  {
    gimp_image_delete(l_tmp_frame_id);
  }

  return(l_mlayer_image_id);
}       /* end gap_mov_exec_anim_preview */


/* ============================================================================
 * p_conv_keyframe
 * ============================================================================
 */
gint
gap_mov_exec_conv_keyframe_to_rel(gint abs_keyframe, GapMovValues *pvals)
{
    if(pvals->dst_range_start <= pvals->dst_range_end)
    {
       return (abs_keyframe -  pvals->dst_range_start);
    }
    return (pvals->dst_range_start - abs_keyframe);
}

gint
gap_mov_exec_conv_keyframe_to_abs(gint rel_keyframe, GapMovValues *pvals)
{
    if(pvals->dst_range_start <= pvals->dst_range_end)
    {
      return(rel_keyframe + pvals->dst_range_start);
    }
    return(pvals->dst_range_start - rel_keyframe);
}



/* ----------------------------------
 * gap_mov_exec_gap_save_pointfile
 * ----------------------------------
 */
gint
gap_mov_exec_gap_save_pointfile(char *filename, GapMovValues *pvals)
{
  FILE *l_fp;
  gint l_idx;

  if(filename == NULL) return -1;

  l_fp = g_fopen(filename, "w+");
  if(l_fp != NULL)
  {
    fprintf(l_fp, "# GAP file contains saved Move Path Point Table\n");
    fprintf(l_fp, "%d  %d  # current_point  points\n",
                  (int)pvals->point_idx,
                  (int)pvals->point_idx_max + 1);
    fprintf(l_fp, "# x  y   width height opacity rotation feather_radius number_of_optional_params [8 perspective transform factors] [rel_keyframe]\n");
    for(l_idx = 0; l_idx <= pvals->point_idx_max; l_idx++)
    {
      gint optional_params_indicator;
      gboolean writePerspective;
      gboolean writeAccelerationCharacteristics;
      gboolean writeKeyframe;

      writePerspective = FALSE;
      writeAccelerationCharacteristics = FALSE;
      writeKeyframe = FALSE;

      fprintf(l_fp, "%04d %04d "
                    , (int)pvals->point[l_idx].p_x
                    , (int)pvals->point[l_idx].p_y
                    );
      gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].w_resize, 3, 3, " ");
      gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].h_resize, 3, 3, " ");
      gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].opacity,  3, 3, " ");
      gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].rotation, 3, 3, " ");
      gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].sel_feather_radius, 3, 3, " ");

      optional_params_indicator = 0;

      /* conditional write transformation (only if there is any) */
      if(pvals->point[l_idx].ttlx != 1.0
      || pvals->point[l_idx].ttly != 1.0
      || pvals->point[l_idx].ttrx != 1.0
      || pvals->point[l_idx].ttry != 1.0
      || pvals->point[l_idx].tblx != 1.0
      || pvals->point[l_idx].tbly != 1.0
      || pvals->point[l_idx].tbrx != 1.0
      || pvals->point[l_idx].tbry != 1.0
      )
      {
        optional_params_indicator += 8;
        writePerspective = TRUE;
      }

      /* conditional write acceleration characteristics
       * relevant information can occur at first controlpoint
       * or on keyframes but never for the last controlpoint)
       */
      if (pvals->point[l_idx].accPosition != 0)
      {
        if (l_idx == 0)
        {
          writeAccelerationCharacteristics = TRUE;
        }
        else
        {
          if ((l_idx < pvals->point_idx_max)
          && ((int)pvals->point[l_idx].keyframe > 0))
          {
            writeAccelerationCharacteristics = TRUE;
          }
        }

        if (writeAccelerationCharacteristics == TRUE)
        {
          optional_params_indicator += 6;
        }
      }

      /* check for writing keyframe
       * (the implicite keyframes at first and last controlpoints are not written to file)
       */
      if((l_idx > 0)
      && (l_idx < pvals->point_idx_max)
      && ((int)pvals->point[l_idx].keyframe > 0))
      {
        optional_params_indicator++;
        writeKeyframe = TRUE;
      }

      fprintf(l_fp, "   %02d ", (int)optional_params_indicator);

      if(writePerspective == TRUE)
      {
        fprintf(l_fp, " ");
        gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].ttlx, 2, 3, " ");
        gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].ttly, 2, 3, " ");
        fprintf(l_fp, " ");
        gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].ttrx, 2, 3, " ");
        gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].ttry, 2, 3, " ");
        fprintf(l_fp, " ");
        gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].tblx, 2, 3, " ");
        gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].tbly, 2, 3, " ");
        fprintf(l_fp, " ");
        gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].tbrx, 2, 3, " ");
        gap_base_fprintf_gdouble(l_fp, pvals->point[l_idx].tbry, 2, 3, " ");
      }


      if(writeAccelerationCharacteristics == TRUE)
      {
        fprintf(l_fp, " %02d %02d %02d %02d %02d %02d"
           , (int)pvals->point[l_idx].accPosition
           , (int)pvals->point[l_idx].accOpacity
           , (int)pvals->point[l_idx].accSize
           , (int)pvals->point[l_idx].accRotation
           , (int)pvals->point[l_idx].accPerspective
           , (int)pvals->point[l_idx].accSelFeatherRadius
           );
      }

      /* conditional write keyframe */
      if(writeKeyframe == TRUE)
      {
        fprintf(l_fp, " %d"
                     , (int)gap_mov_exec_conv_keyframe_to_rel(pvals->point[l_idx].keyframe_abs, pvals)
                     );
      }
      fprintf(l_fp, "\n");   /* terminate the output line */
    }

    fclose(l_fp);
    return 0;
  }
  return -1;
}  /* end gap_mov_exec_gap_save_pointfile */



/* ----------------------------------
 * gap_mov_exec_gap_load_pointfile
 * ----------------------------------
 * return 0 if Load was OK,
 * return -2 when load has read inconsistent pointfile
 *           and the pointtable needs to be reset (dialog has to call p_reset_points)
 */
gint
gap_mov_exec_gap_load_pointfile(char *filename, GapMovValues *pvals)
{
#define POINT_REC_MAX 512
#define MAX_NUMVALUES_PER_LINE 23
  FILE   *l_fp;
  gint    l_idx;
  char    l_buff[POINT_REC_MAX +1 ];
  char   *l_ptr;
  gint    l_cnt;
  gint    l_rc;
  gint    l_i1, l_i2;
  gdouble l_farr[MAX_NUMVALUES_PER_LINE];


  l_rc = -1;
  if(filename == NULL) return(l_rc);

  l_fp = g_fopen(filename, "r");
  if(l_fp != NULL)
  {
    l_idx = -1;
    while (NULL != fgets (l_buff, POINT_REC_MAX, l_fp))
    {
       /* skip leading blanks */
       l_ptr = l_buff;
       while(*l_ptr == ' ') { l_ptr++; }

       /* check if line empty or comment only (starts with '#') */
       if((*l_ptr != '#') && (*l_ptr != '\n') && (*l_ptr != '\0'))
       {
         l_cnt = gap_base_sscan_flt_numbers(l_ptr, &l_farr[0], MAX_NUMVALUES_PER_LINE);
         l_i1 = (gint)l_farr[0];
         l_i2 = (gint)l_farr[1];

         if(gap_debug)
         {
            gint ii;

            printf("scanned %d numbers\n", l_cnt);
            for(ii=0; ii < l_cnt; ii++)
            {
              printf("  value[%02d] : %f\n", (int)ii, (float)l_farr[ii]);
            }
         }
         if(l_idx == -1)
         {
           if((l_cnt < 2) || (l_i2 > GAP_MOV_MAX_POINT) || (l_i1 > l_i2))
           {
             break;
           }
           pvals->point_idx     = l_i1;
           pvals->point_idx_max = l_i2 -1;
           l_idx = 0;
         }
         else
         {
           gint    optional_params_indicator;
           gint    key_idx;
           gint    acc_idx;
           gint    perspective_idx;

           pvals->point[l_idx].keyframe_abs = 0;
           pvals->point[l_idx].keyframe = 0;
           pvals->point[l_idx].p_x      = l_i1;
           pvals->point[l_idx].p_y      = l_i2;
           pvals->point[l_idx].ttlx     = 1.0;
           pvals->point[l_idx].ttly     = 1.0;
           pvals->point[l_idx].ttrx     = 1.0;
           pvals->point[l_idx].ttry     = 1.0;
           pvals->point[l_idx].tblx     = 1.0;
           pvals->point[l_idx].tbly     = 1.0;
           pvals->point[l_idx].tbrx     = 1.0;
           pvals->point[l_idx].tbry     = 1.0;
           pvals->point[l_idx].w_resize = l_farr[2];
           pvals->point[l_idx].h_resize = l_farr[3];
           pvals->point[l_idx].opacity  = l_farr[4];
           pvals->point[l_idx].rotation = l_farr[5];
           pvals->point[l_idx].sel_feather_radius = 0.0;

           pvals->point[l_idx].accPosition         = 0;
           pvals->point[l_idx].accOpacity          = 0;
           pvals->point[l_idx].accSize             = 0;
           pvals->point[l_idx].accRotation         = 0;
           pvals->point[l_idx].accPerspective      = 0;
           pvals->point[l_idx].accSelFeatherRadius = 0;

          /* the older format used in GAP.1.2 has 6 or 7 integer numbers per line
            * and is still readable by this code.
            *
            * the new format has 2 integer values (p_x, p_y)
            * and 5 float values (w_resize, h_resize, opacity, rotation, feather_radius)
            * and 1 int value optional_params_indicator (telling how much and what kind of parameter will follow)
            * possible optional parameter(s) are:
            * (if all of them are present, then in the order as listed below)
            *
            *     8  float values for the transformation factors
            *     6  int values for acceleration characteristics (since GIMP_GAP 2.7.x)
            *     1  int value for keyframe information (not present at first and last controlpoint)
            */
           if((l_cnt != 6) && (l_cnt != 7)   /* for compatibility to old format */
           && (l_cnt != 8) && (l_cnt != 9)   && (l_cnt != 16) && (l_cnt != 17)  /* for GAP 2.6.x format */
           && (l_cnt != 14) && (l_cnt != 15) && (l_cnt != 22) && (l_cnt != 23) )
           {
             /* invalid pointline format detected */
             l_rc = -2;  /* have to call p_reset_points() when called from dialog window */

             printf("invalid move path pointfile %d numbers per line are not supported\n", (int)l_cnt);
             break;
           }

           optional_params_indicator = 0;
           key_idx = -1;
           acc_idx = -1;
           perspective_idx = -1;

           if(l_cnt >= 8)
           {
             pvals->point[l_idx].sel_feather_radius = l_farr[6];
             optional_params_indicator = l_farr[7];
           }


           if(gap_debug)
           {
             printf("gap_mov_exec_gap_load_pointfile  optional_params_indicator:%d\n", optional_params_indicator);
           }


           switch (optional_params_indicator)
           {
               case 0:
                   break;
               case 1:
                   /* have one keyframe value */
                   key_idx = 8;
                   break;
               case 6:
                   /* have six accelerate characteristic values */
                   acc_idx = 8;
                   break;
               case 7:
                   /* have six accelerate characteristic values
                    * and one keyframe value
                    */
                   acc_idx = 8;
                   key_idx = 8 + 6;
                   break;
               case 8:
                   /* have eight perspective values */
                   perspective_idx = 8;
                   break;
               case 9:
                   /* have eight perspective values
                    * and one keyframe value
                    */
                   perspective_idx = 8;
                   key_idx         = 8 + 8;
                   break;
               case 14:
                   /* have eight perspective values
                    * and six accelerate characteristic values
                    */
                   perspective_idx = 8;
                   acc_idx         = 8 + 8;
                   break;
               case 15:
                   /* have eight perspective values
                    * and six accelerate characteristic values
                    * and one keyframe value
                    */
                   perspective_idx = 8;
                   acc_idx         = 8 + 8;
                   key_idx         = 8 + 8 + 6;
                   break;
           }

           if(l_idx > 0)
           {
             switch(l_cnt)
             {
               case 7:
                   key_idx = 6; /* for compatibilty with old format */
                   break;
             }
           }

           if(perspective_idx >= 0)
           {
             pvals->point[l_idx].ttlx = l_farr[perspective_idx];
             pvals->point[l_idx].ttly = l_farr[perspective_idx +1];
             pvals->point[l_idx].ttrx = l_farr[perspective_idx +2];
             pvals->point[l_idx].ttry = l_farr[perspective_idx +3];
             pvals->point[l_idx].tblx = l_farr[perspective_idx +4];
             pvals->point[l_idx].tbly = l_farr[perspective_idx +5];
             pvals->point[l_idx].tbrx = l_farr[perspective_idx +6];
             pvals->point[l_idx].tbry = l_farr[perspective_idx +7];
           }
           if(acc_idx >= 0)
           {
             pvals->point[l_idx].accPosition         = l_farr[acc_idx];
             pvals->point[l_idx].accOpacity          = l_farr[acc_idx +1];
             pvals->point[l_idx].accSize             = l_farr[acc_idx +2];
             pvals->point[l_idx].accRotation         = l_farr[acc_idx +3];
             pvals->point[l_idx].accPerspective      = l_farr[acc_idx +4];
             pvals->point[l_idx].accSelFeatherRadius = l_farr[acc_idx +5];
           }

           if(key_idx > 0)
           {
             pvals->point[l_idx].keyframe = l_farr[key_idx];
             pvals->point[l_idx].keyframe_abs = gap_mov_exec_conv_keyframe_to_abs(l_farr[key_idx], pvals);
           }
           l_idx ++;
         }

         if(l_idx > pvals->point_idx_max) break;
       }
    }

    fclose(l_fp);
    if(l_idx >= 0)
    {
       l_rc = 0;  /* OK if we found at least one valid Controlpoint in the file */
    }
  }
  return (l_rc);
}  /* end gap_mov_exec_gap_load_pointfile */


/* ============================================================================
 * procedured for calculating angles
 *   (used in rotate_follow option)
 * ============================================================================
 */

static gdouble
p_calc_angle(gint p1x, gint p1y, gint p2x, gint p2y)
{
  /* calculate angle in degree
   * how to rotate an object that follows the line between p1 and p2
   */
  gdouble l_a;
  gdouble l_b;
  gdouble l_angle_rad;
  gdouble l_angle;

  l_a = p2x - p1x;
  l_b = (p2y - p1y) * (-1.0);

  if(l_a == 0)
  {
    if(l_b < 0)  { l_angle = 90.0; }
    else         { l_angle = 270.0; }
  }
  else
  {
    l_angle_rad = atan(l_b/l_a);
    l_angle = (l_angle_rad * 180.0) / G_PI;

    if(l_a < 0)
    {
      l_angle = 180 - l_angle;
    }
    else
    {
      l_angle = l_angle * (-1.0);
    }
  }

  if(gap_debug)
  {
     printf("p_calc_angle: p1(%d/%d) p2(%d/%d)  a=%f, b=%f, angle=%f\n"
         , (int)p1x, (int)p1y, (int)p2x, (int)p2y
         , (float)l_a, (float)l_b, (float)l_angle);
  }
  return(l_angle);
}

static gdouble
p_rotatate_less_than_180(gdouble angle, gdouble angle_new, gint *turns)
{
  /* if an object  follows a circular path and does more than one turn
   * there comes a point where it flips from say 265 degree to -85 degree.
   *
   * if there are more (say 3) frames between the controlpoints,
   * the object performs an unexpected rotation effect because the iteration
   * from 265 to -85  is done  in a sequence like this: 265.0, 148.6, 32.3, -85.0
   *
   * we can avoid this by preventing angle changes of more than 180 degree.
   * in such a case this procedure adjusts the new_angle from -85 to 275
   * that results in iterations like this: 265.0, 268.3, 271.6, 275.0
   */
  gint l_diff;
  gint l_turns;

  l_diff = angle - (angle_new + (*turns * 360));
  if((l_diff >= -180) && (l_diff < 180))
  {
      return(angle_new + (*turns * 360));
  }

  l_diff = (angle - angle_new);
  if(l_diff < 0)
  {
     l_turns = (l_diff / 360) -1;
  }
  else
  {
     l_turns = (l_diff / 360) +1;
  }

  *turns = l_turns;

  if(gap_debug)
  {
     printf("p_rotatate_less_than_180: turns %d angle_new:%f\n"
             , (int)l_turns, (float)angle_new);
  }

  return( angle_new + (l_turns * 360));
}


/* ============================================================================
 * gap_mov_exec_calculate_rotate_follow
 * ============================================================================
 */
void
gap_mov_exec_calculate_rotate_follow(GapMovValues *pvals, gdouble startangle)
{
  gint l_idx;
  gdouble l_startangle;
  gdouble l_angle_1;
  gdouble l_angle_2;
  gdouble l_angle_new;
  gdouble l_angle;
  gint    l_turns;

  l_startangle = startangle;

  if(pvals->point_idx_max > 1)
  {
    l_angle = 0.0;
    l_turns = 0;

    for(l_idx = 0; l_idx <= pvals->point_idx_max; l_idx++)
    {
      if(l_idx == 0)
      {
        l_angle = p_calc_angle(pvals->point[l_idx].p_x,
                               pvals->point[l_idx].p_y,
                               pvals->point[l_idx +1].p_x,
                               pvals->point[l_idx +1].p_y);
      }
      else
      {
        if(l_idx == pvals->point_idx_max)
        {
          l_angle_new = p_calc_angle(pvals->point[l_idx -1].p_x,
                                 pvals->point[l_idx -1].p_y,
                                 pvals->point[l_idx].p_x,
                                 pvals->point[l_idx].p_y);
        }
        else
        {
           l_angle_1 = p_calc_angle(pvals->point[l_idx -1].p_x,
                                  pvals->point[l_idx -1].p_y,
                                  pvals->point[l_idx].p_x,
                                  pvals->point[l_idx].p_y);

           l_angle_2 = p_calc_angle(pvals->point[l_idx].p_x,
                                  pvals->point[l_idx].p_y,
                                  pvals->point[l_idx +1].p_x,
                                  pvals->point[l_idx +1].p_y);

           if((l_angle_1 == 0) && (l_angle_2 == 180))
           {
               l_angle_new = 270;
           }
           else
           {
             if((l_angle_1 == 90) && (l_angle_2 == 270))
             {
               l_angle_new = 0;
             }
             else
             {
               l_angle_new = (l_angle_1 + l_angle_2) / 2;
             }
           }
           if(((l_angle_1 < 0) && (l_angle_2 >= 180))
           || ((l_angle_2 < 0) && (l_angle_1 >= 180)))
           {
              l_angle_new += 180;
           }
        }
        l_angle = p_rotatate_less_than_180(l_angle, l_angle_new, &l_turns);
      }

      if(gap_debug)
      {
        printf("ROT Follow [%03d] angle = %f\n", (int)l_idx, (float)l_angle);
      }

      pvals->point[l_idx].rotation = l_startangle + l_angle;
    }
  }
}       /* end gap_mov_exec_calculate_rotate_follow */


/* ============================================================================
 * gap_mov_exec_chk_keyframes
 *   check if controlpoints and keyframe settings are OK
 *   return pointer to errormessage on Errors
 *      contains "\0" if no errors are found
 * ============================================================================
 */
gchar *gap_mov_exec_chk_keyframes(GapMovValues *pvals)
{
  gint   l_affected_frames;
  gint   l_idx;
  gint   l_errcount;
  gint   l_prev_keyframe;
  gint   l_prev_frame;
  gchar *l_err;
  gchar *l_err_lbltext;

  l_affected_frames = 1 + MAX(pvals->dst_range_start, pvals->dst_range_end)
                    - MIN(pvals->dst_range_start, pvals->dst_range_end);

  l_errcount = 0;
  l_prev_keyframe = 0;
  l_prev_frame = 0;
  l_err_lbltext = g_strdup("\0");

  for(l_idx = 0; l_idx < pvals->point_idx_max; l_idx++ )
  {
     if(pvals->point[l_idx].keyframe_abs != 0)
     {
         pvals->point[l_idx].keyframe = gap_mov_exec_conv_keyframe_to_rel(pvals->point[l_idx].keyframe_abs, pvals);

         if(pvals->point[l_idx].keyframe > l_affected_frames - 2)
         {
            l_err = g_strdup_printf(_("\nError: Keyframe %d at point [%d] higher or equal than last handled frame")
                                      , pvals->point[l_idx].keyframe_abs,  l_idx+1);
            l_err_lbltext = g_strdup_printf("%s%s", l_err_lbltext, l_err);
            g_free(l_err);
            l_errcount++;
         }
         if(pvals->point[l_idx].keyframe < l_prev_frame)
         {
            l_err = g_strdup_printf(_("\nError: Keyframe %d at point [%d] leaves not enough space (frames)"
                                      "\nfor the previous controlpoints")
                                      , pvals->point[l_idx].keyframe_abs, l_idx+1);
            l_err_lbltext = g_strdup_printf("%s%s", l_err_lbltext, l_err);
            g_free(l_err);
            l_errcount++;
         }

         if(pvals->point[l_idx].keyframe <= l_prev_keyframe)
         {
            l_err = g_strdup_printf(_("\nError: Keyframe %d is not in sequence at point [%d]")
                                     , pvals->point[l_idx].keyframe_abs, l_idx+1);
            l_err_lbltext = g_strdup_printf("%s%s", l_err_lbltext, l_err);
            g_free(l_err);
            l_errcount++;
         }

         l_prev_keyframe = pvals->point[l_idx].keyframe;
         if(l_prev_keyframe > l_prev_frame)
         {
           l_prev_frame = l_prev_keyframe +1;
         }
     }
     else
     {
        l_prev_frame++;
        if(l_prev_frame +1 > l_affected_frames)
        {
            l_err = g_strdup_printf(_("\nError: controlpoint [%d] is out of handled framerange"), l_idx+1);
            l_err_lbltext = g_strdup_printf("%s%s", l_err_lbltext, l_err);
            g_free(l_err);
            l_errcount++;
        }
     }
     if(l_errcount > 10)
     {
       break;
     }
  }

  if(pvals->point_idx_max + 1 > l_affected_frames)
  {
        l_err = g_strdup_printf(_("\nError: More controlpoints (%d) than handled frames (%d)."
                                  "\nPlease reduce controlpoints or select more frames"),
                                  (int)pvals->point_idx_max+1, (int)l_affected_frames);
        l_err_lbltext = g_strdup_printf("%s%s", l_err_lbltext, l_err);
        g_free(l_err);
  }

  return(l_err_lbltext);
}       /* end gap_mov_exec_chk_keyframes */


/* ============================================================================
 * p_check_move_path_params
 *   check the parameters for noninteractive call of MovePath
 *   return 0 (OK)  or -1 (Error)
 * ============================================================================
 */
static gint
p_check_move_path_params(GapMovData *mov_data)
{
  gchar *l_err_lbltext;
  gint   l_rc;

  l_rc = 0;  /* assume OK */

  /* range params valid ? */
  if(MIN(mov_data->val_ptr->dst_range_start, mov_data->val_ptr->dst_range_end)
      < mov_data->dst_ainfo_ptr->first_frame_nr)
  {
     printf("Error: Range starts before first frame number %d\n",
             (int)mov_data->dst_ainfo_ptr->first_frame_nr);
     l_rc = -1;
  }

  if(MAX(mov_data->val_ptr->dst_range_start, mov_data->val_ptr->dst_range_end)
      > mov_data->dst_ainfo_ptr->last_frame_nr)
  {
     printf("Error: Range ends after last frame number %d\n",
             (int)mov_data->dst_ainfo_ptr->last_frame_nr);
     l_rc = -1;
  }

  /* is there a valid source object ? */
  if(mov_data->val_ptr->src_layer_id < 0)
  {
     printf("Error: the passed src_layer_id %d  is invalid\n",
             (int)mov_data->val_ptr->src_layer_id);
     l_rc = -1;
  }
  else if(gimp_drawable_is_layer(mov_data->val_ptr->src_layer_id))
  {
     mov_data->val_ptr->src_image_id = gimp_drawable_get_image(mov_data->val_ptr->src_layer_id);
  }
  else
  {
     printf("Error: the passed src_layer_id %d  is no Layer\n",
             (int)mov_data->val_ptr->src_layer_id);
     l_rc = -1;
  }

  /* keyframes OK ? */
  l_err_lbltext = gap_mov_exec_chk_keyframes(mov_data->val_ptr);
  if (*l_err_lbltext != '\0')
  {
     printf("Error in Keyframe settings: %s\n", l_err_lbltext);
     l_rc = -1;
  }
  g_free(l_err_lbltext);

  return (l_rc);
}       /* end p_check_move_path_params */


/* ============================================================================
 * gap_mov_exec_move_path
 *
 * return image_id (of the new loaded current frame) on success
 *        or -1 on errors
 * ============================================================================
 */
gint32
gap_mov_exec_move_path(GimpRunMode run_mode, gint32 image_id, GapMovValues *pvals, gchar *pointfile
             , gint rotation_follow , gdouble startangle)
{
  int l_rc;
  GapAnimInfo *ainfo_ptr;
  GapMovData  l_mov_data;

  l_rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    l_mov_data.val_ptr = pvals;
    l_mov_data.singleFramePtr = NULL;
    if(NULL != l_mov_data.val_ptr)
    {
      if (0 == gap_lib_dir_ainfo(ainfo_ptr))
      {
        if(0 != gap_lib_chk_framerange(ainfo_ptr))   return -1;

        l_mov_data.val_ptr->cache_src_image_id = -1;
        l_mov_data.dst_ainfo_ptr = ainfo_ptr;
        if(run_mode == GIMP_RUN_NONINTERACTIVE)
        {
           l_rc = 0;

           /* get controlpoints from pointfile */
           if (pointfile != NULL)
           {
             l_rc = gap_mov_exec_gap_load_pointfile(pointfile, pvals);
             if (l_rc < 0)
             {
               printf("Execution Error: could not load MovePath controlpoints from file: %s\n",
                       pointfile);
             }
           }

           if(l_rc >= 0)
           {
              l_rc = p_check_move_path_params(&l_mov_data);
           }

           /* Automatic calculation of rotation values */
           if((rotation_follow > 0) && (l_rc == 0))
           {
              gap_mov_exec_calculate_rotate_follow(pvals, startangle);
           }
        }
        else
        {
           /* Dialog for GIMP_RUN_INTERACTIVE
            * (and for GIMP_RUN_WITH_LAST_VALS that is not really supported here)
            */
           l_rc = gap_mov_dlg_move_dialog (&l_mov_data);
           if(0 != gap_lib_chk_framechange(ainfo_ptr))
           {
              l_rc = -1;
           }
        }

        if(l_rc >= 0)
        {
           if(gap_lib_gap_check_save_needed(ainfo_ptr->image_id))
           {
             l_rc = gap_lib_save_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename);
           }
           if(l_rc >= 0)
           {
              l_rc = p_mov_execute(&l_mov_data);
              if (l_rc >= 0)
              {
                /* go back to the frame_nr where move operation was started from */
                l_rc = gap_lib_load_named_frame(ainfo_ptr->image_id, ainfo_ptr->old_filename);
              }
           }
        }

        if(l_mov_data.val_ptr->cache_tmp_image_id >= 0)
        {
           if(gap_debug)
           {
              printf("gap_move: DELETE cache_tmp_image_id:%d\n",
                       (int)l_mov_data.val_ptr->cache_tmp_image_id);
           }
           /* destroy the cached frame image */
           gimp_image_delete(l_mov_data.val_ptr->cache_tmp_image_id);
        }
      }

    }

    gap_lib_free_ainfo(&ainfo_ptr);
  }

  if (l_rc < 0)
  {
    return -1;
  }

  return(l_rc);
}       /* end gap_mov_exec_move_path */




/* ============================================================================
 * gap_mov_exec_query
 *
 * fill information  of the mov_query struct.
 * ============================================================================
 */
void
gap_mov_exec_query(GapMovValues *val_ptr, GapAnimInfo *ainfo_ptr, GapMovQuery *mov_query)
{
  GapMovData    l_mov_data;
  GapMovData   *l_mov_ptr;
  GapMovValues  l_mov_vals;
  GapMovValues *l_pvals;

  l_mov_ptr = &l_mov_data;
  l_pvals = &l_mov_vals;

  if((mov_query != NULL) && (val_ptr != NULL))
  {
    /* init query results */
    mov_query->pathSegmentLengthInPixels = 0.0;
    mov_query->maxSpeedInPixelsPerFrame = 0.0;
    mov_query->minSpeedInPixelsPerFrame = -1.0;
    mov_query->segmentNumber = 0;
    /* copy settings */
    memcpy(l_pvals, val_ptr, sizeof(GapMovValues));
    l_mov_ptr->val_ptr = l_pvals;
    l_mov_ptr->dst_ainfo_ptr = ainfo_ptr;

    /* init local cached src image for anim preview generation.
     * (never mix cached src image for normal and anim preview
     *  because anim previews are often scaled down)
     */
    l_pvals->cache_src_image_id  = -1;
    l_pvals->cache_tmp_image_id  = -1;
    l_pvals->cache_tmp_layer_id  = -1;
    l_pvals->cache_frame_number  = -1;
    l_pvals->cache_ainfo_ptr = NULL;

    p_mov_execute_or_query(l_mov_ptr, mov_query);
    if(mov_query->minSpeedInPixelsPerFrame < 0)
    {
      mov_query->minSpeedInPixelsPerFrame = -1.0;
    }
  }
}





/* ============================================================================
 * gap_mov_exec_set_handle_offsets
 *  set handle offsets according to handle mode and src image dimensions
 * ============================================================================
 */
void gap_mov_exec_set_handle_offsets(GapMovValues *val_ptr, GapMovCurrent *cur_ptr)
{
  guint    l_src_width, l_src_height;         /* dimensions of the source image */

   /* get dimensions of source image */
   if((val_ptr->src_stepmode < GAP_STEP_FRAME)
   || (val_ptr->cache_tmp_image_id < 0))
   {
     l_src_width  = gimp_image_width(val_ptr->src_image_id);
     l_src_height = gimp_image_height(val_ptr->src_image_id);
   }
   else
   {
     /* for Frame Based Modes use the cached tmp image */
     l_src_width  = gimp_image_width(val_ptr->cache_tmp_image_id);
     l_src_height = gimp_image_height(val_ptr->cache_tmp_image_id);
   }

   cur_ptr->l_handleX = 0.0;
   cur_ptr->l_handleY = 0.0;
   switch(val_ptr->src_handle)
   {
      case GAP_HANDLE_LEFT_BOT:
         cur_ptr->l_handleY += l_src_height;
         break;
      case GAP_HANDLE_RIGHT_TOP:
         cur_ptr->l_handleX += l_src_width;
         break;
      case GAP_HANDLE_RIGHT_BOT:
         cur_ptr->l_handleX += l_src_width;
         cur_ptr->l_handleY += l_src_height;
         break;
      case GAP_HANDLE_CENTER:
         cur_ptr->l_handleX += (l_src_width  / 2);
         cur_ptr->l_handleY += (l_src_height / 2);
         break;
      case GAP_HANDLE_LEFT_TOP:
      default:
         break;
   }
}       /* end gap_mov_exec_set_handle_offsets */


/* ------------------------------------
 * gap_mov_exec_new_GapMovValues
 * ------------------------------------
 */
gdouble
gap_mov_exec_get_default_rotate_threshold()
{
  gdouble rotate_threshold;
  
  
  rotate_threshold = 
    gap_base_get_gimprc_gdouble_value (GAP_MOVEPATH_GIMPRC_ROTATE_THRESHOLD
                                       , GAP_MOVEPATH_DEFAULT_ROTATE_THRESHOLD
                                       , 0.0  /* gdouble min_value */
                                       , 1.0  /* gdouble max_value */
                                       );

  return (rotate_threshold);
}  /* end gap_mov_exec_get_default_rotate_threshold */


/* ------------------------------------
 * gap_mov_exec_new_GapMovValues
 * ------------------------------------
 */
GapMovValues *gap_mov_exec_new_GapMovValues()
{
  GapMovValues *pvals;

  pvals = g_new (GapMovValues, 1);

  pvals->version = GAP_MOV_INT_VERSION;
  pvals->rotate_threshold = gap_mov_exec_get_default_rotate_threshold();
  pvals->recordedFrameWidth = 0;     /* witdh of the frame (at recording time of the move path settings) */
  pvals->recordedFrameHeight = 0;    /* height of the frame (at recording time of the move path settings) */
  pvals->recordedObjWidth = 0;
  pvals->recordedObjHeight = 0;
  pvals->total_frames = 0;
  pvals->src_layerstack = 0;
  pvals->src_filename = NULL;



  pvals->dst_image_id = -1;
  pvals->tmp_image_id = -1;
  pvals->tmpsel_image_id = -1;
  pvals->tmpsel_channel_id = -1;
  pvals->apv_mode = 0;
  pvals->apv_src_frame = -1;
  pvals->apv_mlayer_image =  -1;
  pvals->apv_gap_paste_buff = NULL;
  pvals->apv_framerate = 24;
  pvals->apv_scalex = 100.0;
  pvals->apv_scaley = 100.0;
  pvals->cache_src_image_id = -1;
  pvals->cache_tmp_image_id = -1;
  pvals->cache_tmp_layer_id = -1;
  pvals->cache_frame_number = -1;
  pvals->cache_ainfo_ptr = NULL;
  pvals->point_idx = 0;
  pvals->point_idx_max = 0;
  pvals->src_apply_bluebox  = 0;
  pvals->bbp  = NULL;

  pvals->step_speed_factor = 1.0;
  pvals->tracelayer_enable = FALSE;
  pvals->trace_opacity_initial = 100.0;
  pvals->trace_opacity_desc = 80.0;
  pvals->tween_steps = 0;
  pvals->tween_opacity_initial = 80.0;
  pvals->tween_opacity_desc = 80.0;

  return(pvals);

}  /* end gap_mov_exec_new_GapMovValues */


/* ----------------------------------
 * gap_mov_exec_move_path_singleframe
 * ----------------------------------
 * return image_id (of the new loaded current frame) on success
 *        or -1 on errors
 */
gint32
gap_mov_exec_move_path_singleframe(GimpRunMode run_mode, gint32 image_id
   , GapMovValues *pvals, GapMovSingleFrame *singleFramePtr)
{
  gint32 l_rc;
  GapAnimInfo *ainfo_ptr;
  GapMovData  l_mov_data;

  l_rc = -1;
  ainfo_ptr = gap_lib_alloc_ainfo(image_id, run_mode);
  if(ainfo_ptr != NULL)
  {
    l_mov_data.val_ptr = pvals;
    l_mov_data.singleFramePtr = singleFramePtr;

    if(NULL != l_mov_data.val_ptr)
    {
      l_mov_data.val_ptr->cache_src_image_id = -1;
      l_mov_data.dst_ainfo_ptr = ainfo_ptr;

      if (run_mode == GIMP_RUN_INTERACTIVE)
      {
         /* Dialog for singleframes mode
          */
         l_rc = gap_mov_dlg_move_dialog_singleframe (singleFramePtr);
      }
      else
      {
         l_rc = 0;
      }


      if(l_rc >= 0)
      {
         /* get parameters and controlpoints from xml_paramfile */
         if (singleFramePtr->xml_paramfile != NULL)
         {
           gboolean  isXmlLoadOk;

           isXmlLoadOk =  gap_mov_xml_par_load(singleFramePtr->xml_paramfile
                                   , pvals
                                     , gimp_image_width(image_id)
                                     , gimp_image_height(image_id)
                                     );


           if (!isXmlLoadOk)
           {
             l_rc = -1;
             if (run_mode != GIMP_RUN_NONINTERACTIVE)
             {
               g_message(("could not load MovePath settings from file: %s")
                        , singleFramePtr->xml_paramfile);
             }
             printf("Execution Error: could not load MovePath settings from file: %s\n",
                     singleFramePtr->xml_paramfile);
           }
         }

      }


      if(l_rc >= 0)
      {
          /* START of the singleframe PROCESSING */
          l_rc = p_mov_execute_singleframe(&l_mov_data);
      }

      if(l_mov_data.val_ptr->cache_tmp_image_id >= 0)
      {
         if(gap_debug)
         {
            printf("gap_move: DELETE cache_tmp_image_id:%d\n",
                     (int)l_mov_data.val_ptr->cache_tmp_image_id);
         }
         /* destroy the cached frame image */
         gimp_image_delete(l_mov_data.val_ptr->cache_tmp_image_id);
      }

    }

    gap_lib_free_ainfo(&ainfo_ptr);
  }

  if (l_rc < 0)
  {
    return -1;
  }

  return(l_rc);
}       /* end gap_mov_exec_move_path_singleframe */


/* --------------------------------------
 * gap_mov_exec_check_valid_xml_paramfile
 * --------------------------------------
 * return TRUE on valid movepat xml parameterfile
 */
gboolean 
gap_mov_exec_check_valid_xml_paramfile(const char *filename)
{
  gboolean  isXmlLoadOk;
  GapMovValues *pvals;

  isXmlLoadOk = FALSE;
  pvals = gap_mov_exec_new_GapMovValues();
  
  if(filename != NULL)
  {
    if(*filename != '\0')
    {
      isXmlLoadOk =  gap_mov_xml_par_load(filename
                                     , pvals
                                     , 640 /* fake actual width */
                                     , 480 /* fake actual height */
                                     );
    }
  }
  g_free(pvals);

  return (isXmlLoadOk);

}  /* end gap_mov_exec_check_valid_xml_paramfile */


/* ---------------------------------------------
 * gap_mov_exec_move_path_singleframe_directcall
 * ---------------------------------------------
 * this procedure renders one frame of a movepath sequence.
 * it is typically called by the storyboard processor.
 * return the processed layer id
 */
gint32
gap_mov_exec_move_path_singleframe_directcall(gint32 frame_image_id
       , gint32 drawable_id
       , gboolean keep_proportions
       , gboolean fit_width
       , gboolean fit_height
       , gint32 frame_phase
       , const char *xml_paramfile)
{
  GapMovValues *pvals;
  gint32        result_layer_id;
  GapMovSingleFrame singleframevals;


  result_layer_id = -1;

  /* init pvals with default values (to provide defined settings
   * for optional data that may not be present in the xml parameter file)
   */
  pvals = gap_mov_exec_new_GapMovValues();
  pvals->dst_image_id = frame_image_id;
  
  
  singleframevals.drawable_id = drawable_id;
  singleframevals.frame_phase = frame_phase;
  singleframevals.total_frames = -1;          /* get path length (total frames) from xml paramfile */
  singleframevals.keep_proportions = keep_proportions;
  singleframevals.fit_width = fit_width;
  singleframevals.fit_height = fit_height;
  g_snprintf(&singleframevals.xml_paramfile[0], sizeof(singleframevals.xml_paramfile), "%s", xml_paramfile);

  if(gap_debug)
  {
    printf("\ngap_mov_exec_move_path_singleframe_directcall:"
           " drawable_id:%d frame_image_id:%d frame_phase:%d\n"
       ,(int)singleframevals.drawable_id
       ,(int)frame_image_id
       ,(int)singleframevals.frame_phase
       );
  }


  result_layer_id = gap_mov_exec_move_path_singleframe(GIMP_RUN_NONINTERACTIVE
                            , frame_image_id, pvals, &singleframevals);
  if(gap_debug)
  {
    printf("gap_mov_exec_move_path_singleframe_directcall:"
           " result_layer_id:%d orig_drawable_id:%d\n"
       ,(int)result_layer_id
       ,(int)drawable_id
       );
  }
  
  g_free(pvals);
  return (result_layer_id);

}  /* end gap_mov_exec_move_path_singleframe_directcall  */
