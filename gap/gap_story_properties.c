/*  gap_story_properties.c
 *
 *  This module handles GAP storyboard dialog properties window
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
 * version 1.3.26a; 2004/02/21  hof: created
 */


#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib/gstdio.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_libgapbase.h"
#include "gap_story_main.h"
#include "gap_story_undo.h"
#include "gap_story_dialog.h"
#include "gap_story_file.h"
#include "gap_story_properties.h"
// #include "gap_player_dialog.h"
// #include "gap_pdb_calls.h"
#include "gap_pview_da.h"
#include "gap_stock.h"
#include "gap_lib.h"
// #include "gap_image.h"
#include "gap_vin.h"
#include "gap_timeconv.h"
#include "gap_thumbnail.h"
#include "gap_fmac_base.h"
#include "gap_fmac_name.h"
#include "gap_story_vthumb.h"
#include "gap_accel_char.h"
#include "gap_accel_da.h"
#include "gap_arr_dialog.h"


#include "gap-intl.h"


#define GAP_STORY_CLIP_PROP_HELP_ID  "plug-in-gap-storyboard-clip-prop"
#define GAP_STORY_MASK_PROP_HELP_ID  "plug-in-gap-storyboard-mask-prop"

#define GAP_STORY_RESPONSE_RESET ((gint)1)
#define GAP_STORY_RESPONSE_SCENE_SPLIT ((gint)2)
#define GAP_STORY_RESPONSE_SCENE_END   ((gint)3)
#define PW_ENTRY_WIDTH        300
#define PW_COMMENT_WIDTH      480
#define PW_SCALE_WIDTH        300
#define PW_SPIN_BUTTON_WIDTH   80

#define PW_ICON_TYPE_WIDTH     48
#define PW_ICON_TYPE_HEIGHT    35


/* GAP_STORY_SCENE_DIFF_THRESHOLD max: 196608 (256 * 256) * 3 */
#define GAP_STORY_SCENE_DIFF_THRESHOLD_MAX  196608
#define GAP_STORY_SCENE_DIFF_THRESHOLD      3000
#define GAP_STORY_SCENE_DIFF_DIFF_THRESHOLD 700

/* GAP_STORY_SCENE_CMP_IGNORE: 0.0 upto 0.5
 * a value of 0.0 does not tolerate "Appering Objects"
 * and force a new scene if objects apper in a scene.
 * A value of 0.5% ignore big color differences for 50% of the checked
 * blocks in a frame. This is tolerant for big Objects appearing in a scene
 * but may fail to detect differences for most other scene changes.
 */
#define GAP_STORY_SCENE_CMP_IGNORE          0.15
#define GAP_STORY_SCENE_CMP_THRESHOLD       8000

#define GAP_STORY_PROPERTIES_PREVIOUS_FILENAME "GAP_STORY_PROPERTIES_PREVIOUS_FILENAME"

#define GAP_STORY_PW_PTR   "gap_story_pw_ptr"


extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

static void     p_pw_prop_reset_all(GapStbPropWidget *pw);
static void     p_pw_prop_response(GtkWidget *widget
                       , gint       response_id
                       , GapStbPropWidget *pw
                       );
static void     p_pw_push_undo_and_set_unsaved_changes(GapStbPropWidget *pw);

static void     p_pw_propagate_mask_attribute_changes(GapStbPropWidget *pw);
static void     p_pw_propagate_mask_name_changes(GapStbPropWidget *pw
                       , const char *mask_name_old
                       );

static const char * p_pw_get_preferred_decoder(GapStbPropWidget *pw);
static gdouble  p_overall_colordiff(guchar *buf1, guchar *buf2
                                   , gint32 th_width
                                   , gint32 th_height
                                   , gint32 th_bpp
                                   , gdouble cmp_ignore);
static void     p_pw_auto_scene_split(GapStbPropWidget *pw, gboolean all_scenes);



static void     p_pw_timer_go_job(GapStbPropWidget *pw);
static void     p_pw_render_layermask(GapStbPropWidget *pw);

static void     p_pv_pview_render_immediate (GapStbPropWidget *pw
                                   , GapStoryElem *stb_elem_refptr
                                   , GapPView     *pv_ptr
                                   );

static void     p_pv_pview_render (GapStbPropWidget *pw);
static gboolean p_pw_preview_events_cb (GtkWidget *widget
                       , GdkEvent  *event
                       , GapStbPropWidget *pw);
static gboolean p_pw_mask_preview_events_cb (GtkWidget *widget
                       , GdkEvent  *event
                       , GapStbPropWidget *pw);
static gboolean p_toggle_clip_type_elem (GapStbPropWidget *pw);
static gboolean p_pw_icontype_preview_events_cb (GtkWidget *widget
                       , GdkEvent  *event
                       , GapStbPropWidget *pw);
static void     p_pw_check_ainfo_range(GapStbPropWidget *pw, char *filename);
static gboolean p_pw_mask_definition_name_update(GapStbPropWidget *pw);
static void     p_pw_mask_name_update_button_pressed_cb(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_mask_name_reference_update(GapStbPropWidget *pw, const char *mask_name);
static void     p_pw_mask_name_entry_update_cb(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_nullable_widget_set_sensitive(GtkWidget *widget, gboolean sensitive, gboolean show);
static void     p_pw_nullable_gimp_scale_entry_set_sensitive(GtkObject *adj, gboolean sensitive, gboolean show);
static void     p_pw_set_colormask_param_sensitivity (GapStbPropWidget *pw);
static void     p_pw_set_mask_name_dependent_widget_sensitivity(GapStbPropWidget *pw, gint active_index);
static void     p_pw_set_strings_for_mask_name_combo(GapStbPropWidget *pw);
static void     p_pw_mask_name_combo_changed_cb( GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_filename_entry_update_cb(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_filename_changed(const char *filename, GapStbPropWidget *pw);
static void     p_filesel_pw_ok_cb (GtkWidget *widget, GapStbPropWidget *pw);
static void     p_filesel_pw_close_cb ( GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_filesel_button_cb ( GtkWidget *w, GapStbPropWidget *pw);
static void     p_pw_colormask_file_filesel_pw_ok_cb (GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_colormask_file_filesel_pw_close_cb ( GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_colormask_file_filesel_button_cb ( GtkWidget *w, GapStbPropWidget *pw);
static void     p_pw_fmac_filesel_pw_ok_cb (GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_fmac_filesel_pw_close_cb ( GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_fmac_filesel_button_cb ( GtkWidget *w, GapStbPropWidget *pw);
static void     p_pw_comment_entry_update_cb(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_colormask_file_entry_update_cb(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_fmac_entry_update_cb(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_update_info_labels_and_cliptype_senstivity(GapStbPropWidget *pw);
static void     p_pw_update_framenr_labels(GapStbPropWidget *pw, gint32 framenr);
static void     p_pw_update_properties(GapStbPropWidget *pw);
static void     p_pw_gint32_adjustment_callback(GtkObject *obj, gint32 *val);
static void     p_pw_mask_enable_toggle_update_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_pw_pingpong_toggle_update_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_radio_mask_anchor_clip_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_radio_mask_anchor_master_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_radio_delace_none_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_radio_delace_odd_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_radio_delace_even_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_radio_delace_odd_first_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_radio_delace_even_first_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_delace_spinbutton_cb(GtkObject *obj, GapStbPropWidget *pw);
static void     p_maskstep_density_spinbutton_cb(GtkObject *obj, GapStbPropWidget *pw);
static void     p_step_density_spinbutton_cb(GtkObject *obj, GapStbPropWidget *pw);
static void     p_fmac_steps_spinbutton_cb(GtkObject *obj, GapStbPropWidget *pw);
static void     p_fmac_accel_spinbutton_cb(GtkObject *obj, GapStbPropWidget *pw);

static void     p_radio_flip_update(GapStbPropWidget *pw, gint32 flip_request);
static void     p_radio_flip_none_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_radio_flip_hor_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_radio_flip_ver_callback(GtkWidget *widget, GapStbPropWidget *pw);
static void     p_radio_flip_both_callback(GtkWidget *widget, GapStbPropWidget *pw);

static void     on_clip_elements_dropped_as_mask_ref (GtkWidget        *widget,
                   GdkDragContext   *context,
                   gint              x,
                   gint              y,
                   GtkSelectionData *selection_data,
                   guint             info,
                   guint             time);
static void     p_pw_dialog_init_dnd(GapStbPropWidget *pw);

static void         p_pw_check_fmac_sensitivity(GapStbPropWidget *pw);
static GtkWidget *  p_pw_create_clip_pv_container(GapStbPropWidget *pw);
static GtkWidget *  p_pw_create_mask_pv_container(GapStbPropWidget *pw);
static GtkWidget *  p_pw_create_icontype_pv_container(GapStbPropWidget *pw);
static void         p_pw_clear_widgets(GapStbPropWidget *pw);


/* ---------------------------------
 * p_pw_prop_reset_all
 * ---------------------------------
 */
static void
p_pw_prop_reset_all(GapStbPropWidget *pw)
{
  gboolean pingpong_state;
  gboolean mask_enable_state;
  gboolean comment_set;
  gint idx;
  char *mask_name_new;
  char *mask_name_old;
  
  mask_name_new = NULL;
  mask_name_old = NULL;
  if(pw->stb_elem_refptr)
  {
    gap_stb_undo_push_clip(pw->tabw
        , GAP_STB_FEATURE_PROPERTIES_CLIP
        , pw->stb_elem_refptr->story_id
        );

    pw->stb_refptr->unsaved_changes = TRUE;
    if(pw->stb_refptr->active_section)
    {
      pw->stb_refptr->active_section->version++;
    }
    if(pw->stb_elem_refptr->mask_name)
    {
      mask_name_old = g_strdup(pw->stb_elem_refptr->mask_name);
    }

    /* get all attributes from the backup element */
    gap_story_elem_copy(pw->stb_elem_refptr, pw->stb_elem_bck);

    if(pw->stb_elem_refptr->mask_name)
    {
      mask_name_new = g_strdup(pw->stb_elem_refptr->mask_name);
    }

    /* must be first to set the record_type */
    p_pw_update_info_labels_and_cliptype_senstivity(pw);

    pingpong_state = (pw->stb_elem_refptr->playmode == GAP_STB_PM_PINGPONG);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pw->pingpong_toggle), pingpong_state);

    mask_enable_state = (pw->stb_elem_refptr->mask_disable != TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pw->pw_mask_enable_toggle), mask_enable_state);

    if(pw->stb_elem_refptr->orig_filename)
    {
      gtk_entry_set_text(GTK_ENTRY(pw->pw_filename_entry), pw->stb_elem_bck->orig_filename);
    }
    comment_set = FALSE;
    if(pw->stb_elem_refptr->comment)
    {
      if(pw->stb_elem_refptr->comment->orig_src_line)
      {
        gtk_entry_set_text(GTK_ENTRY(pw->comment_entry), pw->stb_elem_refptr->comment->orig_src_line);
        comment_set = TRUE;
      }
    }
    if(!comment_set)
    {
        gtk_entry_set_text(GTK_ENTRY(pw->comment_entry), "");
    }
    
    if(mask_name_new)
    {
        gtk_entry_set_text(GTK_ENTRY(pw->pw_mask_name_entry), mask_name_new);
        if(strcmp(mask_name_old, mask_name_new) != 0)
        {
          p_pw_propagate_mask_name_changes(pw, mask_name_old);
        }
    }
    else
    {
        gtk_entry_set_text(GTK_ENTRY(pw->pw_mask_name_entry), "");
    }

    gtk_adjustment_set_value(GTK_ADJUSTMENT(pw->pw_spinbutton_from_adj), pw->stb_elem_refptr->from_frame);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pw->pw_spinbutton_to_adj), pw->stb_elem_refptr->to_frame);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pw->pw_spinbutton_loops_adj), pw->stb_elem_refptr->nloop);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pw->pw_spinbutton_step_density_adj), pw->stb_elem_refptr->step_density);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pw->pw_spinbutton_mask_stepsize_adj), pw->stb_elem_refptr->mask_stepsize);



    idx = CLAMP((gint32)pw->stb_elem_refptr->delace, 0, (GAP_MAX_DELACE_MODES -1));
    if(pw->pw_delace_mode_radio_button_arr[idx])
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pw->pw_delace_mode_radio_button_arr[idx])
                                   , TRUE);
    }

    idx = CLAMP(pw->stb_elem_refptr->flip_request, 0, (GAP_MAX_FLIP_REQUEST -1));
    if(pw->pw_flip_request_radio_button_arr[idx])
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pw->pw_flip_request_radio_button_arr[idx])
                                   , TRUE);
    }
    
    idx = CLAMP((gint32)pw->stb_elem_refptr->mask_anchor, 0, (3 -1));
    if(pw->pw_mask_anchor_radio_button_arr[idx])
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pw->pw_mask_anchor_radio_button_arr[idx])
                                   , TRUE);
    }


    p_pw_update_properties(pw);
  }
  if(mask_name_new)
  {
    g_free(mask_name_new);
  }
  if(mask_name_old)
  {
    g_free(mask_name_old);
  }

}  /* end p_pw_prop_reset_all */


/* ---------------------------------
 * p_pw_prop_response
 * ---------------------------------
 */
static void
p_pw_prop_response(GtkWidget *widget
                  , gint       response_id
                  , GapStbPropWidget *pw
                  )
{
  GtkWidget *dlg;
  GapStbMainGlobalParams  *sgpp;

  sgpp = pw->sgpp;
  switch (response_id)
  {
    case GAP_STORY_RESPONSE_RESET:
      if((pw->stb_elem_bck)
      && (pw->stb_elem_refptr)
      && (!pw->scene_detection_busy))
      {
        p_pw_prop_reset_all(pw);
      }
      break;
    case GAP_STORY_RESPONSE_SCENE_END:
    case GAP_STORY_RESPONSE_SCENE_SPLIT:
      if((!pw->scene_detection_busy)
      && (!sgpp->gva_lock)
      && (pw->pw_filesel == NULL))
      {
        gboolean all_scenes;

        if(pw->stb_refptr->active_section != NULL)
        {
          pw->stb_refptr->active_section->version++;
        }
        pw->stb_refptr->unsaved_changes = TRUE;
        pw->scene_detection_busy = TRUE;
        all_scenes = FALSE;
        if(response_id == GAP_STORY_RESPONSE_SCENE_SPLIT)
        {
          all_scenes = TRUE;
        }

        p_pw_auto_scene_split(pw, all_scenes);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(pw->pw_spinbutton_to_adj)
                            , pw->stb_elem_refptr->to_frame);
        p_pv_pview_render(pw);
        pw->scene_detection_busy = FALSE;
      }

      if(!pw->close_flag)
      {
        break;
      }

      /* the p_pw_auto_scene_split procedure was cancelled (by close_flag)
       * in this case run into the close code
       */
    case GTK_RESPONSE_CLOSE:
    default:
      if((response_id == GTK_RESPONSE_CLOSE)
      && (pw->scene_detection_busy))
      {
        /* we were called from within the p_pw_auto_scene_split procedure
         * that has not finished yet. in this case just set the close_flag
         * The p_pw_auto_scene_split procedure will stop at next loop
         * if close_flag  == TRUE
         */
        pw->close_flag = TRUE;
        return;
      }
      
      if (pw->is_mask_definition)
      {
        if(FALSE == p_pw_mask_definition_name_update(pw))
        {
          /* occurs if a non-unique mask definition name was entered.
           */
          return;
        }
      }
      
      if(pw->pw_filesel)
      {
        p_filesel_pw_close_cb(pw->pw_filesel, pw);
      }
      if(pw->go_timertag >= 0)
      {
        g_source_remove(pw->go_timertag);
      }
      pw->go_timertag = -1;
      
      gap_pview_reset(pw->pv_ptr);
      gap_pview_reset(pw->mask_pv_ptr);
      gap_pview_reset(pw->typ_icon_pv_ptr);

      dlg = pw->pw_prop_dialog;
      if(dlg)
      {
        pw->pw_prop_dialog = NULL;
        pw->stb_elem_refptr = NULL;
        pw->stb_refptr = NULL;
        gtk_widget_destroy(dlg);
      }
      break;
  }
}  /* end p_pw_prop_response */


/* --------------------------------------
 * p_pw_push_undo_and_set_unsaved_changes
 * --------------------------------------
 * this procedure is called in most cases when
 * a clip property has changed.
 * (note that the Undo push implementation filter out
 * multiple clip property changes t the same object in sequence)
 */
static void
p_pw_push_undo_and_set_unsaved_changes(GapStbPropWidget *pw)
{
  if(pw != NULL)
  {
    if((pw->stb_elem_refptr != NULL) && (pw->stb_refptr != NULL))
    {
      gap_stb_undo_push_clip(pw->tabw
          , GAP_STB_FEATURE_PROPERTIES_CLIP
          , pw->stb_elem_refptr->story_id
          );

      pw->stb_refptr->unsaved_changes = TRUE;
      if(pw->stb_refptr->active_section != NULL)
      {
        pw->stb_refptr->active_section->version++;
      }
    
    }
  }
}  /* end p_pw_push_undo_and_set_unsaved_changes */

/* --------------------------------------
 * p_pw_propagate_mask_attribute_changes
 * -------------------------------------
 */
static void
p_pw_propagate_mask_attribute_changes(GapStbPropWidget *pw)
{
  if(pw->stb_elem_refptr)
  {
    if(pw->is_mask_definition)
    {
      gap_story_dlg_pw_update_mask_references(pw->tabw);
    }
  }
}  /* end p_pw_propagate_mask_attribute_changes */

/* ---------------------------------
 * p_pw_propagate_mask_name_changes
 * ---------------------------------
 * this procedure is typically called to rename already existing
 * mask_names of a mask definition property.
 * In this case all references hav to be updated to use the new mask_name.
 */
static void
p_pw_propagate_mask_name_changes(GapStbPropWidget *pw
  , const char *mask_name_old
  )
{
  if((pw->stb_elem_refptr)
  &&(mask_name_old))
  {
    if(pw->is_mask_definition)
    {
      if(pw->stb_elem_refptr->mask_name)
      {
         gboolean updateOK;
         
         /* we have changed an already existing mask_name in a mask_definition
          * element attribute.
          * This requires update of all video track elements that are refering
          * to this mask definition.
          */
         updateOK = gap_story_update_mask_name_references(pw->stb_refptr
           , pw->stb_elem_refptr->mask_name
           , mask_name_old
           );
         if (!updateOK)
         {
           g_message(_("Error: references could not be updated from the\n"
                       "old mask name: \"%s\" to the\n"
                       "new mask name: \"%s\"\n"
                       "because the new mask name is already in use.")
                     ,mask_name_old
                     ,pw->stb_elem_refptr->mask_name
                     );
         }

         /* if there are more property windows open that are
          * displaying one of the references to the changed mask_name
          * those windows have to update the mask_name entry widget.
          */
         gap_story_dlg_pw_update_mask_references(pw->tabw);
      }
    }
  }
}  /* end p_pw_propagate_mask_name_changes */




/* ---------------------------------
 * p_pw_get_preferred_decoder
 * ---------------------------------
 */
static const char *
p_pw_get_preferred_decoder(GapStbPropWidget *pw)
{
  return(gap_story_get_preferred_decoder(pw->stb_refptr, pw->stb_elem_refptr));
}  /* end p_pw_get_preferred_decoder */


/* -------------------------------
 * p_overall_colordiff
 * -------------------------------
 * guess overall color difference
 * for 2 RGB (or RGBA) databuffers of same size.
 *
 * the imagedata is processed in sub blocks of 8x8 pixel
 * The returned difference is 0 if all checked blocks
 * are equal in color.
 * A maximum difference of 196608 (256 * 256) * 3
 * will be returned if buf2 is the negative of buf1
 * (full black versus full white)
 *
 * big differences are ignored until there are
 * more than GAP_STORY_SCENE_CMP_IGNORE percent (0.0 upto 1.0)
 *  of the compared blocks having big differences
 * (if Objects enter a scene there are typical some blocks
 *  with big differences that should not be triggered as new scene start)
 *
 * For performance reasons we check
 * only 1 Block in a square of 2x2 Blocks
 */
gdouble
p_overall_colordiff(guchar *buf1, guchar *buf2
                   , gint32 th_width
                   , gint32 th_height
                   , gint32 th_bpp
                   , gdouble cmp_ignore)
{
  gint32 stepsize, bpp_step;
  gint32 checked_blocks;
  gint32 pixels_per_block;
  gint32 ii, xx, yy, jj, x, y;
  gdouble color_diff;
  gdouble color_diff_sum;
  gint32 rdif, gdif, bdif;
  gint32 rpick1, gpick1, bpick1;
  gint32 rpick2, gpick2, bpick2;
  gint32 pick_radius;
  gint32 rowstride;
  gint32 count_big_diff;
  gdouble ignore_big_diff_blocks;

  color_diff_sum = 0;
  checked_blocks = 0;
  count_big_diff = 0;

  pick_radius = MIN(8, th_width);
  pick_radius = MIN(pick_radius, th_height);

  rowstride = th_width * th_bpp;
  stepsize = pick_radius * 2;
  bpp_step = stepsize * th_bpp;
  pixels_per_block = pick_radius * pick_radius;
  ignore_big_diff_blocks = (th_height / stepsize) * (th_width / stepsize);
  ignore_big_diff_blocks *= cmp_ignore;

  for(y = 0; y < th_height - pick_radius; y += stepsize)
  {
    ii = y * rowstride;
    for(x = 0; x < th_width - pick_radius; x += stepsize)
    {
      rpick1 = 0;  rpick2 = 0;
      gpick1 = 0;  gpick2 = 0;
      bpick1 = 0;  bpick2 = 0;

      /* calculate summary color of one block */
      for(yy = 0; yy < pick_radius; yy++)
      {
        jj = ii + (yy * rowstride);
        for(xx = 0; xx < pick_radius; xx++)
        {
          rpick1 += buf1[jj];
          rpick2 += buf2[jj];
          gpick1 += buf1[jj +1];
          gpick2 += buf2[jj +1];
          bpick1 += buf1[jj +2];
          bpick2 += buf2[jj +2];
          jj += th_bpp;
        }
      }

      rdif = abs(rpick1 - rpick2) / pixels_per_block;
      gdif = abs(gpick1 - gpick2) / pixels_per_block;
      bdif = abs(bpick1 - bpick2) / pixels_per_block;

      color_diff =  ( (rdif * rdif)
                    + (gdif * gdif)
                    + (bdif * bdif)
                    );
      if((color_diff < GAP_STORY_SCENE_CMP_THRESHOLD)
      || (count_big_diff > ignore_big_diff_blocks))
      {
        color_diff_sum += color_diff;
        checked_blocks++;
      }
      else
      {
        /* count blocks with big differences */
        count_big_diff++;
      }
      ii += bpp_step;
    }
  }

  return(color_diff_sum / MAX(checked_blocks,1));
}  /* end p_overall_colordiff */


/* -------------------------------
 * p_pw_auto_scene_split
 * -------------------------------
 */
static void
p_pw_auto_scene_split(GapStbPropWidget *pw, gboolean all_scenes)
{
  guchar *prev_th_data;
  guchar *th_data;
  gint32 l_th_width;
  gint32 l_th_height;
  gint32 l_th_bpp;
  gint32 l_1st_th_width;
  gint32 l_1st_th_height;
  gint32 l_1st_th_bpp;
  gint32 framenr;
  gint32 framenr_max;
  gint32 video_id;
  gdouble diff;
  gdouble sum_diff;
  gdouble num_diff;
  gdouble diff_threshold;
  gdouble diff_diff_threshold;
  gdouble l_cmp_ignore;
  gboolean drop_th_data;
  gboolean drop_prev_th_data;
  GapStoryElem *stb_elem;
  GapStbMainGlobalParams  *sgpp;
  GdkPixbuf *pixbuf;
  GdkPixbuf *prev_pixbuf;

  stb_elem = pw->stb_elem_refptr;
  sgpp = pw->sgpp;
  if(stb_elem == NULL)
  {
    return;
  }
  if((stb_elem->record_type != GAP_STBREC_VID_MOVIE)
  && (stb_elem->record_type != GAP_STBREC_VID_FRAMES))
  {
    g_message(_("Automatic scene detection operates only "
                "on cliptypes MOVIE and FRAMES"));
    return;
  }
  if ((stb_elem->record_type == GAP_STBREC_VID_MOVIE)
  &&  (sgpp->auto_vthumb == FALSE))
  {
    g_message(_("Scene detection depends on video thumbnails. "
                "Please enable video thumbnails (in the Windows Menu)"));
    return;
  }

  pixbuf = NULL;
  prev_pixbuf = NULL;
  prev_th_data = NULL;
  framenr = stb_elem->from_frame;
  framenr_max = stb_elem->to_frame;

  if((framenr_max <= framenr)
  || (all_scenes == FALSE))
  {
    /* allow search until end */
    framenr_max = GAP_STB_MAX_FRAMENR;
  }

  drop_th_data = FALSE;
  drop_prev_th_data = FALSE;
  l_cmp_ignore = GAP_STORY_SCENE_CMP_IGNORE;

  diff_threshold = GAP_STORY_SCENE_DIFF_THRESHOLD;
  diff_diff_threshold = GAP_STORY_SCENE_DIFF_DIFF_THRESHOLD;
  sum_diff = 0;
  num_diff = 0;
  if(stb_elem->record_type == GAP_STBREC_VID_MOVIE)
  {
    prev_th_data = gap_story_vthumb_fetch_thdata(sgpp
              ,pw->stb_refptr
              ,stb_elem
              ,stb_elem->from_frame
              ,stb_elem->seltrack
              ,p_pw_get_preferred_decoder(pw)
              ,&l_th_bpp
              ,&l_th_width
              ,&l_th_height
              );
  }
  else
  {
    char *filename;

    filename = gap_lib_alloc_fname(stb_elem->basename
                                   , stb_elem->from_frame
                                   , stb_elem->ext
                                   );
    if(filename)
    {
      prev_pixbuf = gap_thumb_file_load_pixbuf_thumbnail(filename
                                      , &l_th_width
                                      , &l_th_height
                                      , &l_th_bpp
                                      );
      g_free(filename);
      if(prev_pixbuf)
      {
        prev_th_data = gdk_pixbuf_get_pixels(prev_pixbuf);
      }
      else
      {
        g_message(_("Scene detection for cliptype FRAMES "
                    "depends on thumbnails. "
                    "Please create thumbnails for your frames and then try again."));
        return;
      }

    }
  }
  if(prev_th_data == NULL)
  {
    return;
  }
  l_1st_th_width  = l_th_width;
  l_1st_th_height = l_th_height;
  l_1st_th_bpp    = l_th_bpp;

  gap_stb_undo_group_begin((GapStbTabWidgets *)pw->tabw);
  gap_stb_undo_push((GapStbTabWidgets *)pw->tabw, GAP_STB_FEATURE_SCENE_SPLITTING);

  while(TRUE)
  {
    framenr++;
    /* fetch vthumb from vthumb list or read from videofile
     * without adding to the vthumb list
     * (the list would grow too much if all thumbnails of all handled
     *  videos were added.
     *  therefore we add only vthumbs of the startframes of a new scene)
     *
     */
    if(stb_elem->record_type == GAP_STBREC_VID_MOVIE)
    {
      th_data = gap_story_vthumb_fetch_thdata_no_store(sgpp
              ,pw->stb_refptr
              ,stb_elem
              ,framenr
              ,stb_elem->seltrack
              ,p_pw_get_preferred_decoder(pw)
              ,&l_th_bpp
              ,&l_th_width
              ,&l_th_height
              ,&drop_th_data   /* TRUE if th_data was not already in vthumb list */
              ,&video_id
              );
    }
    else
    {
      char *filename;

      th_data = NULL;
      filename = gap_lib_alloc_fname(stb_elem->basename
                                   , framenr
                                   , stb_elem->ext
                                   );
      if(filename)
      {
        pixbuf = gap_thumb_file_load_pixbuf_thumbnail(filename
                                        , &l_th_width
                                        , &l_th_height
                                        , &l_th_bpp
                                        );
        if(pixbuf)
        {
          th_data = gdk_pixbuf_get_pixels(pixbuf);
        }
        else
        {
          if(g_file_test(filename, G_FILE_TEST_EXISTS))
          {
            g_message(_("Scene detection for cliptype FRAMES "
                        "depends on thumbnails. "
                        "Please create thumbnails for your frames and then try again."));
          }
        }
        g_free(filename);
      }
    }


    if((th_data == NULL)
    || (l_1st_th_width  != l_th_width)
    || (l_1st_th_height != l_th_height)
    || (l_1st_th_bpp    != l_th_bpp))
    {
      /* STOP because we cant compare,
       * we have no current frame of same size as prev. frame
       */
      stb_elem->to_frame = framenr -1;
      
      gap_stb_undo_group_end((GapStbTabWidgets *)pw->tabw);
      return;
    }

    diff = p_overall_colordiff(prev_th_data
                              ,th_data
                              ,l_th_width
                              ,l_th_height
                              ,l_th_bpp
                              ,l_cmp_ignore
                              );


    /* visible trace the current frame in the pview widget */
    gap_pview_render_f_from_buf (pw->pv_ptr
                    , th_data
                    , l_th_width
                    , l_th_height
                    , l_th_bpp
                    , FALSE         /* Dont allow_grab_src_data */
                    , stb_elem->flip_request
                    , GAP_STB_FLIP_NONE  /* flip_status */
                    );

    p_pw_update_framenr_labels(pw, framenr);

    sum_diff += diff;
    num_diff++;

    if(gap_debug)
    {
      printf("SCENE: frame_max:%d frame:%d  THRES:%d AVG_DIFF:%d DIFF:%d\n"
      , (int)framenr_max
      , (int)framenr
      , (int)diff_threshold
      , (int)(sum_diff /  num_diff)
      , (int)diff
      );
    }

    /* check if diff is bigger than abs threshold
     * or if we have more than 4 frames:
     *   if current diff is 8 times bigger than the average diff
     * we force a new SCENE
     * we also do new SCENE if Scene detection was stopped by close_flag
     * or if frame_max (limit) is reached
     */
    if((diff > diff_threshold)
    || ((num_diff > 4) && (diff > (8 * sum_diff /  num_diff)) && (diff > diff_diff_threshold))
    || (framenr >= framenr_max)
    || (pw->close_flag))
    {
      diff = 0;
      sum_diff = 0;
      num_diff = 0;

      /* raise the thresholds for the first frame after the cut
       * (avoid ultra short scenes with less than 3 frames)
       */
      diff_threshold = GAP_STORY_SCENE_DIFF_THRESHOLD_MAX * 2;
      diff_diff_threshold = GAP_STORY_SCENE_DIFF_THRESHOLD_MAX * 2;

      stb_elem->to_frame = framenr -1;
      if(!all_scenes)
      {
        gap_stb_undo_group_end((GapStbTabWidgets *)pw->tabw);
        return;
      }
      else
      {
        GapStoryElem *stb_elem_new;

        if((framenr >= framenr_max)
        || (pw->close_flag))
        {
          gap_stb_undo_group_end((GapStbTabWidgets *)pw->tabw);
          return;
        }

        if(gap_debug) printf("AUTO SCENE NEW_ELEM:\n");

        /* add a new Element for the next scene */
        stb_elem_new = gap_story_elem_duplicate(stb_elem);

        stb_elem_new->from_frame = framenr;
        stb_elem_new->to_frame = framenr;
        gap_story_elem_calculate_nframes(stb_elem_new);

        stb_elem_new->next = stb_elem->next;
        stb_elem->next = stb_elem_new;
        stb_elem = stb_elem_new;

        if(gap_debug)
        {
          printf("AUTO SCENE NEW_ELEM linked to list: drop:%d, video_id:%d\n"
               ,(int)drop_th_data
               ,(int)video_id
               );
        }

        if((stb_elem->record_type == GAP_STBREC_VID_MOVIE)
        && (drop_th_data)
        && (video_id >= 0))
        {
           if(gap_debug) printf("AUTO SCENE ADD VTHUMB:\n");
           drop_th_data = FALSE;
           gap_story_vthumb_add_vthumb(sgpp
                                   ,framenr
                                   ,th_data
                                   ,l_th_width
                                   ,l_th_height
                                   ,l_th_bpp
                                   ,video_id
                                   );
           /* refresh storyboard layout and thumbnail list widgets */
           gap_story_dlg_recreate_tab_widgets(pw->tabw
                                  ,sgpp
                                  );
        }
        if(stb_elem->record_type != GAP_STBREC_VID_MOVIE)
        {
           /* refresh storyboard layout and thumbnail list widgets */
           gap_story_dlg_recreate_tab_widgets(pw->tabw
                                  ,sgpp
                                  );
        }
      }

    }

    if((prev_th_data) && (drop_prev_th_data))
    {
      /* throw away video thumbnails for frames that
       * are not start of a scene
       * or were not already in the vthum list before
       */
      if(stb_elem->record_type == GAP_STBREC_VID_MOVIE)
      {
        g_free(prev_th_data);
      }
      else
      {
        g_object_unref(prev_pixbuf);
        prev_pixbuf = NULL;
      }
      prev_th_data = NULL;
    }
    if(stb_elem->record_type == GAP_STBREC_VID_MOVIE)
    {
      prev_th_data = th_data;
    }
    else
    {
      prev_th_data = gdk_pixbuf_get_pixels(pixbuf);
      prev_pixbuf = pixbuf;
    }
    drop_prev_th_data = drop_th_data;
    diff_threshold = MAX(GAP_STORY_SCENE_DIFF_THRESHOLD, (diff_threshold / 4));
    diff_diff_threshold = MAX(GAP_STORY_SCENE_DIFF_DIFF_THRESHOLD, (diff_diff_threshold / 4));

    /* g_main_context_iteration makes sure that
     *  gtk does refresh widgets,  and react on events while the scene detection
     *  is busy.
     */
    while(g_main_context_iteration(NULL, FALSE));

  }
}  /* end p_pw_auto_scene_split */


/* ---------------------------------
 * p_pw_render_layermask
 * ---------------------------------
 */
static void
p_pw_render_layermask(GapStbPropWidget *pw)
{
  if(pw->stb_elem_refptr)
  {
    if(!pw->is_mask_definition)
    {
      if(pw->stb_elem_refptr->mask_name)
      {
        GapStoryElem *stb_elem_maskdef;
        
        stb_elem_maskdef = gap_story_find_mask_definition_by_name(pw->stb_refptr
                               , pw->stb_elem_refptr->mask_name);
        if(stb_elem_maskdef)
        {
          if(gap_debug)
          {
            printf("p_pw_render_layermask: stb_elem_maskdef:");
            gap_story_debug_print_elem(stb_elem_maskdef);
          }
          p_pv_pview_render_immediate(pw, stb_elem_maskdef, pw->mask_pv_ptr);
        }
        else
        {
           if(strlen(pw->stb_elem_refptr->mask_name) == 0)
           {
             /* mask_name is empty dont show this case as invalid refernence */
             gtk_widget_hide (pw->mask_pv_container);
             return;
           }
           /* render default icon for undefined mask reference */
           gap_story_dlg_render_default_icon(NULL, pw->mask_pv_ptr);
        }
        gtk_widget_show (pw->mask_pv_container);
      }
      else
      {
        gtk_widget_hide (pw->mask_pv_container);
      }
    }
  }
}  /* end p_pw_render_layermask */


/* ---------------------------------
 * p_pv_pview_render_immediate
 * ---------------------------------
 * 1.) fetch thumbnal pixbuf data,
 * 2.) if no thumbnail available
 *     try to load full image (and create the thumbnail for next usage)
 * 3.) if neither thumbnail nor image could be fetched
 *        render a default icon
 *
 */
static void
p_pv_pview_render_immediate (GapStbPropWidget *pw
   , GapStoryElem *stb_elem_refptr
   , GapPView     *pv_ptr
   )
{
   gint32  l_th_width;
   gint32  l_th_height;
   gint32  l_th_bpp;
   GdkPixbuf *pixbuf;
   char     *l_frame_filename;

   if(stb_elem_refptr == NULL)
   {
     return;
   }

   /* init preferred width and height
    * (as hint for the thumbnail loader to decide
    *  if thumbnail is to fetch from normal or large thumbnail directory
    *  just for the case when both sizes are available)
    */
   l_th_width = 128;
   l_th_height = 128;

   if((stb_elem_refptr->record_type == GAP_STBREC_VID_MOVIE)
   || (stb_elem_refptr->record_type == GAP_STBREC_VID_SECTION)
   || (stb_elem_refptr->record_type == GAP_STBREC_VID_ANIMIMAGE)
   )
   {
     guchar *l_th_data;
     /* if(gap_debug) printf("RENDER: p_pv_pview_render_immediate MOVIE Thumbnail\n"); */

     l_th_data = gap_story_vthumb_fetch_thdata(pw->sgpp
              ,pw->stb_refptr
              ,stb_elem_refptr
              ,stb_elem_refptr->from_frame
              ,stb_elem_refptr->seltrack
              ,p_pw_get_preferred_decoder(pw)
              ,&l_th_bpp
              ,&l_th_width
              ,&l_th_height
              );
     if(l_th_data)
     {
       gboolean l_th_data_was_grabbed;

       l_th_data_was_grabbed = gap_pview_render_f_from_buf (pv_ptr
                    , l_th_data
                    , l_th_width
                    , l_th_height
                    , l_th_bpp
                    , TRUE         /* allow_grab_src_data */
                    , stb_elem_refptr->flip_request
                    , GAP_STB_FLIP_NONE  /* flip_status */
                    );
       if(!l_th_data_was_grabbed)
       {
         /* the gap_pview_render_f_from_buf procedure can grab the l_th_data
          * instead of making a ptivate copy for later use on repaint demands.
          * if such a grab happened it returns TRUE.
          * (this is done for optimal performance reasons)
          * in such a case the caller must NOT free the src_data (l_th_data) !!!
          */
          g_free(l_th_data);
       }
       l_th_data = NULL;
       
       if(stb_elem_refptr == pw->stb_elem_refptr)
       {
         p_pw_update_framenr_labels(pw, pw->stb_elem_refptr->from_frame);
       }
       return;
     }

   }

   l_frame_filename = gap_story_get_filename_from_elem(stb_elem_refptr);
   if(l_frame_filename == NULL)
   {
     /* no filename available, use default icon */
     gap_story_dlg_render_default_icon(stb_elem_refptr, pv_ptr);
     return;
   }

   pixbuf = gap_thumb_file_load_pixbuf_thumbnail(l_frame_filename
                                    , &l_th_width, &l_th_height
                                    , &l_th_bpp
                                    );
   if(pixbuf)
   {
     gap_pview_render_f_from_pixbuf (pv_ptr
                                    , pixbuf
                                    , stb_elem_refptr->flip_request
                                    , GAP_STB_FLIP_NONE  /* flip_status */
                                    );
     g_object_unref(pixbuf);
   }
   else
   {
      gint32  l_image_id;
      
      l_image_id = -1;
      
      if((stb_elem_refptr->record_type != GAP_STBREC_VID_MOVIE)
      && (stb_elem_refptr->record_type != GAP_STBREC_VID_SECTION))
      {
        l_image_id = gap_lib_load_image(l_frame_filename);
      }


      if (l_image_id < 0)
      {
        /* could not read the image
         */
        if(gap_debug) printf("p_frame_widget_render: fetch failed, using DEFAULT_ICON\n");
        gap_story_dlg_render_default_icon(stb_elem_refptr, pv_ptr);
      }
      else
      {
        /* there is no need for undo on this scratch image
         * so we turn undo off for performance reasons
         */
        gimp_image_undo_disable (l_image_id);
        gap_pview_render_f_from_image (pv_ptr
                                    , l_image_id
                                    , stb_elem_refptr->flip_request
                                    , GAP_STB_FLIP_NONE  /* flip_status */
                                    );

        /* create thumbnail (to speed up acces next time) */
        gap_thumb_cond_gimp_file_save_thumbnail(l_image_id, l_frame_filename);

        gimp_image_delete(l_image_id);
      }
   }
   if(stb_elem_refptr == pw->stb_elem_refptr)
   {
     p_pw_update_framenr_labels(pw, pw->stb_elem_refptr->from_frame);
   }

}       /* end p_pv_pview_render_immediate */


/* ------------------
 * p_pw_timer_go_job
 * ------------------
 */
static void
p_pw_timer_go_job(GapStbPropWidget *pw)
{
  GapStbMainGlobalParams  *sgpp;

  if(gap_debug)
  {
    printf("\np_pw_timer_go_job: START\n");
  }
  sgpp = pw->sgpp;

  if((pw)
  && (sgpp))
  {

    if(pw->go_timertag >= 0)
    {
      g_source_remove(pw->go_timertag);
    }
    pw->go_timertag = -1;

    if((pw->go_job_framenr >= 0)
    ||(pw->go_render_all_request))
    {
      if(sgpp->gva_lock)
      {
        /* the video api is still busy with fetching the previous frame
         * (do not disturb, but setup the go_timer for a next try
         * after 96 milliseconds)
         */
        pw->go_timertag = (gint32) g_timeout_add(96, (GtkFunction)p_pw_timer_go_job, pw);

        /*if(gap_debug) printf("p_pw_timer_go_job: TRY LATER (96msec) %06d\n", (int)pw->go_job_framenr); */
      }
      else
      {
        if(pw->stb_elem_refptr)
        {
          if(pw->go_render_all_request)
          {
            if (pw->go_job_framenr >= 0)
            {
              pw->stb_elem_refptr->from_frame = pw->go_job_framenr;
              p_pv_pview_render_immediate(pw, pw->stb_elem_refptr, pw->pv_ptr);
            }
            gap_story_dlg_pw_render_all(pw, pw->go_recreate_request);
            pw->go_render_all_request = FALSE;

            /* sometimes the displayed thumbnail does not match with the displayed
             * from_frame number at this point. (dont know exactly why)
             * the simple workaround is just render twice
             * and now it seems to work fine.
             * (rendering twice should not be too slow, since the requested video thumbnails
             *  are now cached in memory)
             */
            p_pv_pview_render_immediate(pw, pw->stb_elem_refptr, pw->pv_ptr);
            gap_story_dlg_pw_render_all(pw, FALSE);
          }
          else
          {
            if (pw->go_job_framenr >= 0)
            {
              gap_stb_undo_push_clip(pw->tabw
               , GAP_STB_FEATURE_PROPERTIES_CLIP
               , pw->stb_elem_refptr->story_id
               );

              pw->stb_elem_refptr->from_frame = pw->go_job_framenr;
              p_pv_pview_render_immediate(pw, pw->stb_elem_refptr, pw->pv_ptr);

            }
          }
          p_pw_render_layermask(pw);

          pw->go_job_framenr = -1;
          pw->go_recreate_request = FALSE;

        }

      }
    }

  }
  if(gap_debug)
  {
    printf("p_pw_timer_go_job: END\n");
  }
}  /* end p_pw_timer_go_job */


/* ---------------------------------
 * p_pv_pview_render
 * ---------------------------------
 */
static void
p_pv_pview_render (GapStbPropWidget *pw)
{
   if(pw->stb_elem_refptr->record_type != GAP_STBREC_VID_MOVIE)
   {
     p_pv_pview_render_immediate(pw, pw->stb_elem_refptr, pw->pv_ptr);
     return;
   }
   pw->go_job_framenr = pw->stb_elem_refptr->from_frame;

   /* video thumb is rendered deferred (16 millisec) via timer */
   if(pw->go_timertag < 0)
   {
     pw->go_timertag = (gint32) g_timeout_add(16, (GtkFunction)p_pw_timer_go_job, pw);
   }

}  /* end p_pv_pview_render */

/* ---------------------------------
 * p_pw_preview_events_cb
 * ---------------------------------
 */
static gboolean
p_pw_preview_events_cb (GtkWidget *widget
                       , GdkEvent  *event
                       , GapStbPropWidget *pw)
{
  /*GdkEventExpose *eevent;*/
  /*GdkEventButton *bevent;*/
  GapStbMainGlobalParams  *sgpp;

  if ((pw->stb_elem_refptr == NULL)
  ||  (pw->stb_refptr == NULL))
  {
    /* the frame_widget is not initialized or it is just a dummy, no action needed */
    return FALSE;
  }
  sgpp = pw->sgpp;

  switch (event->type)
  {
    case GDK_BUTTON_PRESS:
      /*bevent = (GdkEventButton *) event;*/

      if((!pw->scene_detection_busy)
      && (!sgpp->gva_lock)
      && (pw->go_timertag < 0))
      {
        if((pw->stb_elem_refptr->record_type == GAP_STBREC_VID_SECTION)
        || (pw->stb_elem_refptr->record_type == GAP_STBREC_VID_BLACKSECTION))
        {
          gap_story_pw_composite_playback(pw);
        }
        else
        {
          gap_story_pw_single_clip_playback(pw);
        }
      }
      return FALSE;
      break;

    case GDK_EXPOSE:
      if(gap_debug) printf("p_pw_preview_events_cb GDK_EXPOSE widget:%ld  da_wgt:%ld\n"
                              , (long)widget
                              , (long)pw->pv_ptr->da_widget
                              );

      /*eevent = (GdkEventExpose *) event;*/

      if(widget == pw->pv_ptr->da_widget)
      {
        gap_pview_repaint(pw->pv_ptr);
        gdk_flush ();
      }

      break;

    default:
      break;
  }

  return FALSE;
}       /* end  p_pw_preview_events_cb */


/* ---------------------------------
 * p_pw_mask_preview_events_cb
 * ---------------------------------
 */
static gboolean
p_pw_mask_preview_events_cb (GtkWidget *widget
                       , GdkEvent  *event
                       , GapStbPropWidget *pw)
{
  /*GdkEventExpose *eevent;*/
  /*GdkEventButton *bevent;*/
  GapStbMainGlobalParams  *sgpp;

  if ((pw->stb_elem_refptr == NULL)
  ||  (pw->stb_refptr == NULL))
  {
    /* the frame_widget is not initialized or it is just a dummy, no action needed */
    return FALSE;
  }
  sgpp = pw->sgpp;

  switch (event->type)
  {
    case GDK_BUTTON_PRESS:
      /*bevent = (GdkEventButton *) event;*/

      if((!pw->scene_detection_busy)
      && (!sgpp->gva_lock)
      && (pw->go_timertag < 0))
      {
        gap_story_pw_composite_playback(pw);
      }
      return FALSE;
      break;

    case GDK_EXPOSE:
      if(gap_debug)
      {
        printf("p_pw_mask_preview_events_cb GDK_EXPOSE widget:%ld  da_wgt:%ld\n"
                              , (long)widget
                              , (long)pw->mask_pv_ptr->da_widget
                              );
      }
      /*eevent = (GdkEventExpose *) event;*/

      if(widget == pw->mask_pv_ptr->da_widget)
      {
        gap_pview_repaint(pw->mask_pv_ptr);
        gdk_flush ();
      }

      break;

    default:
      break;
  }

  return FALSE;
}       /* end  p_pw_mask_preview_events_cb */


/* -----------------------------------
 * p_toggle_clip_type_elem
 * -----------------------------------
 * return TRUE if type was toggled
 *        FALSE if toggle was refused.
 */
static gboolean
p_toggle_clip_type_elem (GapStbPropWidget *pw)
{
  GapStoryBoard *stb;
  GapStoryElem *stb_elem;
  
  stb = pw->stb_refptr;
  stb_elem = pw->stb_elem_refptr;

  gap_stb_undo_push_clip(pw->tabw
        , GAP_STB_FEATURE_PROPERTIES_CLIP
        , pw->stb_elem_refptr->story_id
        );
  
  if(stb_elem->record_type == GAP_STBREC_VID_SECTION)
  {
    stb_elem->record_type = GAP_STBREC_VID_BLACKSECTION;
    return (TRUE);
  }

  if(stb_elem->record_type == GAP_STBREC_VID_BLACKSECTION)
  {
    GapStorySection *main_section;
    main_section = gap_story_find_main_section(stb);
    
    if (stb->active_section == main_section)
    {
      stb_elem->record_type = GAP_STBREC_VID_SECTION;
      return (TRUE);
    }
    return (FALSE);
  }

  if(stb_elem->record_type == GAP_STBREC_VID_IMAGE)
  {
    stb_elem->record_type = GAP_STBREC_VID_ANIMIMAGE;
    p_pw_check_ainfo_range(pw, pw->stb_elem_refptr->orig_filename);
    return (TRUE);
  }

  if(stb_elem->record_type == GAP_STBREC_VID_FRAMES)
  {
    stb_elem->record_type = GAP_STBREC_VID_IMAGE;
    p_pw_check_ainfo_range(pw, pw->stb_elem_refptr->orig_filename);
    return (TRUE);
  }

  if(stb_elem->record_type == GAP_STBREC_VID_ANIMIMAGE)
  {
    stb_elem->record_type = GAP_STBREC_VID_IMAGE;
    if(pw->stb_elem_refptr->orig_filename)
    {
      char *filename;
      filename = g_strdup(pw->stb_elem_refptr->orig_filename);
      gap_story_upd_elem_from_filename(stb_elem, filename);
      g_free(filename);
      if(stb_elem->record_type == GAP_STBREC_VID_UNKNOWN)
      {
        stb_elem->record_type = GAP_STBREC_VID_IMAGE;
      }
    }
    p_pw_check_ainfo_range(pw, pw->stb_elem_refptr->orig_filename);
    return (TRUE);
  }


  return (FALSE);
  
}  /* end p_toggle_clip_type_elem */

/* ---------------------------------
 * p_pw_icontype_preview_events_cb
 * ---------------------------------
 */
static gboolean
p_pw_icontype_preview_events_cb (GtkWidget *widget
                       , GdkEvent  *event
                       , GapStbPropWidget *pw)
{
  /*GdkEventExpose *eevent;*/
  /*GdkEventButton *bevent;*/
  GapStbMainGlobalParams  *sgpp;

  if ((pw->stb_elem_refptr == NULL)
  ||  (pw->stb_refptr == NULL))
  {
    /* the frame_widget is not initialized or it is just a dummy, no action needed */
    return FALSE;
  }
  sgpp = pw->sgpp;

  switch (event->type)
  {
    case GDK_BUTTON_PRESS:
      /*bevent = (GdkEventButton *) event;*/

      if((!pw->scene_detection_busy)
      && (!sgpp->gva_lock)
      && (pw->go_timertag < 0))
      {
        gboolean was_toggled;
        
        was_toggled = p_toggle_clip_type_elem (pw);
        gap_story_dlg_render_default_icon(pw->stb_elem_refptr, pw->typ_icon_pv_ptr);
        if (was_toggled == TRUE)
        {
          pw->stb_refptr->unsaved_changes = TRUE;
          p_pw_update_properties(pw);
        }
      }
      return FALSE;
      break;

    case GDK_EXPOSE:
      if(gap_debug)
      {
        printf("p_pw_icontype_preview_events_cb GDK_EXPOSE widget:%ld  da_wgt:%ld\n"
                              , (long)widget
                              , (long)pw->typ_icon_pv_ptr->da_widget
                              );
      }
      /*eevent = (GdkEventExpose *) event;*/

      if(widget == pw->typ_icon_pv_ptr->da_widget)
      {
        gap_story_dlg_render_default_icon(pw->stb_elem_refptr, pw->typ_icon_pv_ptr);
        gdk_flush ();
      }

      break;

    default:
      break;
  }

  return FALSE;
}       /* end  p_pw_icontype_preview_events_cb */


/* ==================================================== START FILESEL stuff ======  */

/* --------------------------------
 * p_pw_check_ainfo_range
 * --------------------------------
 */
static void
p_pw_check_ainfo_range(GapStbPropWidget *pw, char *filename)
{
  GapAnimInfo *ainfo_ptr;
  gdouble l_lower;
  gdouble l_upper;
  gdouble l_val;

  if(gap_debug)
  {
    printf("PROP AINFO CHECK  START\n");
  }

  /* default: allow maximum range
   * (for movies we dont know the exactnumber of frames
   *  until the movie is read sequential until EOF
   *  that would be much to slow, must live with unconstrained range spinbuttons)
   */
  l_lower = 1;
  l_upper = 99999; /* default for unknown total_frames */

  if(pw->stb_elem_refptr->record_type == GAP_STBREC_VID_FRAMES)
  {
    ainfo_ptr = gap_lib_alloc_ainfo_from_name(filename, GIMP_RUN_NONINTERACTIVE);
    if(ainfo_ptr)
    {
      if(0== gap_lib_dir_ainfo(ainfo_ptr))
      {

         l_lower = MIN(ainfo_ptr->last_frame_nr, ainfo_ptr->first_frame_nr);
         l_upper = MAX(ainfo_ptr->last_frame_nr, ainfo_ptr->first_frame_nr);

      }
      gap_lib_free_ainfo(&ainfo_ptr);
    }
  }
  else
  {
    if(pw->stb_elem_refptr->record_type == GAP_STBREC_VID_MOVIE)
    {
      GapStoryVTResurceElem *velem;

      if(gap_debug)
      {
        printf("PROP AINFO CHECK --> GAP_STBREC_VID_MOVIE\n");
      }
      
      velem = gap_story_vthumb_get_velem_movie(pw->sgpp
                           ,pw->stb_elem_refptr->orig_filename
                           ,pw->stb_elem_refptr->seltrack
                           ,p_pw_get_preferred_decoder(pw)
                           );
      if(velem)
      {
        l_upper = velem->total_frames;
      }
    } 
    else
    {
      if((pw->stb_elem_refptr->record_type == GAP_STBREC_VID_ANIMIMAGE)
      || (pw->stb_elem_refptr->record_type == GAP_STBREC_VID_SECTION))
      {
        GapStoryVTResurceElem *velem;

        if(gap_debug)
        {
          printf("PROP AINFO CHECK --> GAP_STBREC_VID_SECTION, ANIM-IMAGE\n");
        }

        velem = gap_story_vthumb_get_velem_no_movie(pw->sgpp
                             ,pw->stb_refptr
                             ,pw->stb_elem_refptr
                             );
        if(pw->stb_elem_refptr->record_type == GAP_STBREC_VID_ANIMIMAGE)
        {
          l_lower = 0;
        }
        if(velem)
        {
          if(pw->stb_elem_refptr->record_type == GAP_STBREC_VID_SECTION)
          {
            l_upper = velem->total_frames;
          }
        }
      } 
    }
  }

  l_val = gtk_adjustment_get_value(GTK_ADJUSTMENT(pw->pw_spinbutton_from_adj));
  GTK_ADJUSTMENT(pw->pw_spinbutton_from_adj)->lower = l_lower;
  GTK_ADJUSTMENT(pw->pw_spinbutton_from_adj)->upper = l_upper;
  if((l_val < l_lower) || (l_val > l_upper))
  {
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pw->pw_spinbutton_from_adj)
                            , CLAMP(l_val, l_lower, l_upper));
  }

  l_val = gtk_adjustment_get_value(GTK_ADJUSTMENT(pw->pw_spinbutton_to_adj));
  GTK_ADJUSTMENT(pw->pw_spinbutton_to_adj)->lower = l_lower;
  GTK_ADJUSTMENT(pw->pw_spinbutton_to_adj)->upper = l_upper;
  if((l_val < l_lower) || (l_val > l_upper))
  {
    gtk_adjustment_set_value(GTK_ADJUSTMENT(pw->pw_spinbutton_to_adj)
                            , CLAMP(l_val, l_lower, l_upper));
  }
  if(gap_debug)
  {
    printf("PROP AINFO CHECK DONE\n");
  }

}  /* end p_pw_check_ainfo_range */

/* --------------------------------
 * p_pw_filename_entry_update_cb
 * --------------------------------
 */
static void
p_pw_filename_entry_update_cb(GtkWidget *widget, GapStbPropWidget *pw)
{
  if(pw->scene_detection_busy)
  {
     return;
  }
  p_pw_filename_changed(gtk_entry_get_text(GTK_ENTRY(widget)), pw);
}  /* end p_pw_filename_entry_update_cb */

/* ---------------------------------------
 * p_pw_mask_definition_name_update
 * ---------------------------------------
 */
static gboolean
p_pw_mask_definition_name_update(GapStbPropWidget *pw)
{
  char *l_mask_name;
  char *mask_name_old;
  gboolean l_okFlag;

  mask_name_old = NULL;
  l_okFlag = TRUE;
  
  if (pw == NULL)
  {
    return (l_okFlag);
  }
  if (pw->pw_mask_name_entry == NULL)
  {
    return (l_okFlag);
  }
  
  l_mask_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(pw->pw_mask_name_entry)));
  if(l_mask_name)
  {
    if(pw->stb_elem_refptr)
    {
      gboolean is_check_required;
      GapStoryElem      *stb_elem_ref;
      
      is_check_required = TRUE; /* in case a new name or different name was entered */
      stb_elem_ref = NULL;
      
      if(pw->stb_elem_refptr->mask_name)
      {
          if(strcmp(pw->stb_elem_refptr->mask_name, l_mask_name) == 0)
          {
            /* NO check if we have still the same mask:name */
            is_check_required = FALSE;
          }
      }
      
      if (is_check_required)
      {
        /* check if there are already references to mask_name_new */
        stb_elem_ref = gap_story_find_mask_reference_by_name(pw->stb_refptr, l_mask_name);
      }
      
      if(stb_elem_ref)
      {
        l_okFlag = FALSE;
        g_message(_("Error: the mask name:  \"%s\" is already in use\n"
                    "please enter another name.")
                    ,l_mask_name
                  );
      
      }
      else
      {
        if(pw->stb_elem_refptr->mask_name)
        {
          if(strcmp(pw->stb_elem_refptr->mask_name, l_mask_name) != 0)
          {
             p_pw_push_undo_and_set_unsaved_changes(pw);

             mask_name_old = g_strdup(pw->stb_elem_refptr->mask_name);
             g_free(pw->stb_elem_refptr->mask_name);
             pw->stb_elem_refptr->mask_name = g_strdup(l_mask_name);
             p_pw_propagate_mask_name_changes(pw, mask_name_old);
             p_pw_update_properties(pw);
          }
        }
        else
        {
          p_pw_push_undo_and_set_unsaved_changes(pw);
          pw->stb_elem_refptr->mask_name = g_strdup(l_mask_name);
          p_pw_render_layermask(pw);
        }
        
        gtk_label_set_text ( GTK_LABEL(pw->pw_mask_definition_name_label)
                           , pw->stb_elem_refptr->mask_name);
        
      }

    }
    g_free(l_mask_name);
  }
  if(mask_name_old)
  {
    g_free(mask_name_old);
  }
  
  if (l_okFlag == TRUE)
  {
    /* the update of mask references also updates the mask_name combo boxes
     * in all other property windows that are currently open
     * (this call is responsible to adds new mask definition entries)
     */
    gap_story_dlg_pw_update_mask_references(pw->tabw);
  }
  
  return (l_okFlag);
}  /* end p_pw_mask_definition_name_update */

/* ---------------------------------------
 * p_pw_mask_name_update_button_pressed_cb
 * ---------------------------------------
 * in case of mask definition
 * the update of mask_name is handled via button to confirm the change.
 * (because automatic updates while typing the name in the entry may lead to
 * unwanted changes in case the first few characters typed match another
 * already existing mask_name)
 */
static void
p_pw_mask_name_update_button_pressed_cb(GtkWidget *widget, GapStbPropWidget *pw)
{
  p_pw_mask_definition_name_update(pw);
}  /* end p_pw_mask_name_update_button_pressed_cb */


/* --------------------------------
 * p_pw_mask_name_reference_update
 * --------------------------------
 */
static void
p_pw_mask_name_reference_update(GapStbPropWidget *pw, const char *mask_name)
{
  if(pw->stb_elem_refptr)
  {
    if(pw->stb_elem_refptr->mask_name)
    {
      gboolean has_changed_flag;
      
      has_changed_flag = TRUE;
      if(mask_name)
      {
        if(strcmp(pw->stb_elem_refptr->mask_name, mask_name) == 0)
        {
          has_changed_flag = FALSE;
        }
      }
      
      /* replace an already existing mask_name reference */
      if(has_changed_flag == TRUE)
      {
         p_pw_push_undo_and_set_unsaved_changes(pw);
         g_free(pw->stb_elem_refptr->mask_name);
         pw->stb_elem_refptr->mask_name = NULL;
         if (mask_name != NULL)
         {
           pw->stb_elem_refptr->mask_name = g_strdup(mask_name);
         }
         p_pw_update_properties(pw);
      }
    }
    else
    {
      if (mask_name != NULL)
      {
        /* set a new mask_name reference */
        p_pw_push_undo_and_set_unsaved_changes(pw);
        pw->stb_elem_refptr->mask_name = g_strdup(mask_name);
        p_pw_render_layermask(pw);
      }
    }
  }
}  /* end p_pw_mask_name_reference_update */

/* --------------------------------
 * p_pw_mask_name_entry_update_cb
 * --------------------------------
 */
static void
p_pw_mask_name_entry_update_cb(GtkWidget *widget, GapStbPropWidget *pw)
{
  char *l_mask_name;

  if(pw == NULL)
  {
    return;
  }
  if(pw->is_mask_definition)
  {
    /* ignore changes of the mask_name entry for mask definitions.
     * (because automatic updates while typing the name in the entry may lead to
     *  unwanted changes in case the first few characters typed do match another
     *  already existing mask_name,
     *  therfore the update is triggered explicitely via a button
     *  see p_pw_mask_name_update_button_pressed_cb )
     */
    return;
  }

  l_mask_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
  if(l_mask_name)
  {
    p_pw_mask_name_reference_update(pw, l_mask_name);
    g_free(l_mask_name);
  }
}  /* end p_pw_mask_name_entry_update_cb */


/* ----------------------------------------
 * p_pw_nullable_widget_set_sensitive
 * ----------------------------------------
 * set widget sensitivity and hide/show the widget depending on specified flags.
 * but do nothing if specified widget is a null pointer.
 */
static void 
p_pw_nullable_widget_set_sensitive(GtkWidget *widget, gboolean sensitive, gboolean show)
{
  if(widget != NULL)
  {
    gtk_widget_set_sensitive(widget, sensitive);
    if (show)
    {
      gtk_widget_show(widget);
    }
    else
    {
      gtk_widget_hide(widget);
    }
  }
}


/* --------------------------------------------
 * p_pw_nullable_gimp_scale_entry_set_sensitive
 * --------------------------------------------
 * set widget sensitivity and hide/show the widget depending on specified flags.
 * but do nothing if specified widget is a null pointer.
 */
static void 
p_pw_nullable_gimp_scale_entry_set_sensitive(GtkObject *adj, gboolean sensitive, gboolean show)
{
  if(adj != NULL)
  {
    p_pw_nullable_widget_set_sensitive(
         g_object_get_data(G_OBJECT (adj), "label")
       , sensitive
       , show
       );

    p_pw_nullable_widget_set_sensitive(
         g_object_get_data(G_OBJECT (adj), "scale")
       , sensitive
       , show
       );

    p_pw_nullable_widget_set_sensitive(
         g_object_get_data(G_OBJECT (adj), "spinbutton")
       , sensitive
       , show
       );
  }
}


/* ------------------------------------
 * p_pw_set_colormask_param_sensitivity
 * ------------------------------------
 */
static void
p_pw_set_colormask_param_sensitivity (GapStbPropWidget *pw)
{
  gint active_index;

  if(pw == NULL) { return; }
  if(pw->mask_name_combo == NULL) {return;}
  if(pw->stb_elem_refptr == NULL) {return;}

  active_index = gtk_combo_box_get_active(GTK_COMBO_BOX (pw->mask_name_combo));
  p_pw_set_mask_name_dependent_widget_sensitivity(pw, active_index);


}  /* end p_pw_set_colormask_param_sensitivity */

/* ------------------------------------------------
 * p_pw_set_mask_name_dependent_widget_sensitivity
 * ------------------------------------------------
 */
static void
p_pw_set_mask_name_dependent_widget_sensitivity(GapStbPropWidget *pw, gint active_index)
{
  gboolean l_sensitive;
  gboolean l_show;
  
  l_sensitive = TRUE;
  if(active_index <= 0)
  {
    /* active index 0 always refers to NONE (i.e. clip has no mask) */
    l_sensitive = FALSE;
  }
  l_show = l_sensitive;

  p_pw_nullable_widget_set_sensitive(pw->mask_anchor_label, l_sensitive, l_show);
  p_pw_nullable_widget_set_sensitive(pw->pw_mask_enable_toggle, l_sensitive, l_show);
  p_pw_nullable_widget_set_sensitive(pw->pw_mask_anchor_radio_button_arr[0], l_sensitive, l_show);
  p_pw_nullable_widget_set_sensitive(pw->pw_mask_anchor_radio_button_arr[1], l_sensitive, l_show);
  p_pw_nullable_widget_set_sensitive(pw->pw_mask_anchor_radio_button_arr[2], l_sensitive, l_show);
 
  p_pw_nullable_gimp_scale_entry_set_sensitive(pw->pw_spinbutton_mask_stepsize_adj
      , l_sensitive
      , l_show
      );
 
  if(pw->mask_anchor != GAP_MSK_ANCHOR_XCOLOR)
  {
    l_sensitive = FALSE;
  } 

  p_pw_nullable_widget_set_sensitive(pw->colormask_file_label, l_sensitive, l_show);
  p_pw_nullable_widget_set_sensitive(pw->colormask_file_entry, l_sensitive, l_show);
  p_pw_nullable_widget_set_sensitive(pw->colormask_file_filesel_button, l_sensitive, l_show);
 }  /* end p_pw_set_mask_name_dependent_widget_sensitivity */

/* ------------------------------------
 * p_pw_set_strings_for_mask_name_combo
 * ------------------------------------
 * replace all entries in the combo box for mask  names.
 * Note: at least the "None" string and the current mask_name section are always present
 * in the combo box, even if the current mask_name is already removed from the mask definitions.
 *
 */
static void
p_pw_set_strings_for_mask_name_combo(GapStbPropWidget *pw)
{
  GapStoryBoard *stb;
  GapStoryElem    *stb_elem;
  gint index_of_active_item;
  gint index;
  gint remove_position;
  char *curr_mask_name;
  gchar *l_txt;

  if(pw == NULL) {return;}
  if(pw->mask_name_combo == NULL) {return;}

  curr_mask_name = NULL;
  
  /* remove all already existing entries */
  remove_position = 0;
  while(pw->mask_name_combo_elem_count > 0)
  {
    gtk_combo_box_remove_text(GTK_COMBO_BOX (pw->mask_name_combo), remove_position);
    pw->mask_name_combo_elem_count--;
  }

  index_of_active_item = 0;
  pw->mask_name_combo_elem_count = 0;

  /* always add the entry "None" (special name when clip has no mask reference) */  
  gtk_combo_box_append_text(GTK_COMBO_BOX (pw->mask_name_combo)
                                    ,_("None"));
  pw->mask_name_combo_elem_count++;
  index = 1;

  if(pw->stb_elem_refptr) 
  {
    if(pw->stb_elem_refptr->track != GAP_STB_MASK_TRACK_NUMBER) 
    {
      curr_mask_name = pw->stb_elem_refptr->mask_name;
    }
  }
  
  
  /* add mask_names of all mask definitions (if there are any present in the storyboard) */
  stb = pw->stb_refptr;

  if(stb)
  {
    if(stb->mask_section)
    {
      for(stb_elem = stb->mask_section->stb_elem; stb_elem != NULL; stb_elem = stb_elem->next)
      {
         if((stb_elem->mask_name) && (stb_elem->track == GAP_STB_MASK_TRACK_NUMBER))
         {
           if(curr_mask_name != NULL)
           {
             if(strcmp(stb_elem->mask_name, curr_mask_name) == 0)
             {
               index_of_active_item = index;
             }
           }

           l_txt = gap_base_shorten_filename(NULL  /* prefix */
                       ,stb_elem->mask_name       /* filenamepart */
                       ,NULL                      /* suffix */
                       ,32                        /* max_chars */
                       );


           gtk_combo_box_append_text(GTK_COMBO_BOX (pw->mask_name_combo)
                                   , l_txt);
           g_free(l_txt);
           pw->mask_name_combo_elem_count++;
           index++;
         }
      }
    }
  }

  if((curr_mask_name) && (index_of_active_item == 0))
  {
    /* the current mask name was not found in the mask definitions (has become invalid)
     * indicate invalid name by prefix "# " and add it as last entry to the combo box
     */
    index_of_active_item = index;
    l_txt = gap_base_shorten_filename("# "  /* prefix */
                ,curr_mask_name            /* filenamepart */
                ,NULL                      /* suffix */
                ,32                        /* max_chars */
                );
    gtk_combo_box_append_text(GTK_COMBO_BOX (pw->mask_name_combo)
                                    ,l_txt);
    g_free(l_txt);
    pw->mask_name_combo_elem_count++;
    index++;
  }


  gtk_combo_box_set_active(GTK_COMBO_BOX (pw->mask_name_combo)
      ,index_of_active_item);

  p_pw_set_mask_name_dependent_widget_sensitivity(pw, index_of_active_item);
}  /* end p_pw_set_strings_for_mask_name_combo */

/* -------------------------------
 * p_pw_mask_name_combo_changed_cb
 * -------------------------------
 */
static void
p_pw_mask_name_combo_changed_cb( GtkWidget     *widget
                       , GapStbPropWidget *pw)
{
  gint active_index;
  gint index;
  char *selected_mask_name;
  GapStoryBoard *stb;
  GapStoryElem    *stb_elem;

  if(pw == NULL) { return; }
  if(pw->mask_name_combo == NULL) {return;}
  if(pw->stb_elem_refptr == NULL) {return;}

  stb = pw->stb_refptr;
  if(stb == NULL) {return;}

  selected_mask_name = NULL;
  active_index = gtk_combo_box_get_active(GTK_COMBO_BOX (pw->mask_name_combo));

  p_pw_set_mask_name_dependent_widget_sensitivity(pw, active_index);

  if ((active_index > 0) && (stb->mask_section != NULL))
  {
    index = 1;
    for(stb_elem = stb->mask_section->stb_elem; stb_elem != NULL; stb_elem = stb_elem->next)
    {
      if((stb_elem->mask_name) && (stb_elem->track == GAP_STB_MASK_TRACK_NUMBER))
      {
        if(index == active_index)
        {
          selected_mask_name = stb_elem->mask_name;
          break;
        }
        index++;
      }
    }
  }
  
  
  
  if(gap_debug)
  {
    printf("MASK NAME COMBO index set to: %d selected_mask_name:", (int)active_index);
    if (selected_mask_name)
    {
       printf("%s\n", selected_mask_name);
    }
    else
    {
       printf("NULL\n");
    }
    if(pw->stb_elem_refptr->mask_name)
    {
      printf("OlD mask_name: %s\n", pw->stb_elem_refptr->mask_name);
    }
  }

  /* an active_index -1 indicates that the combo box has no entries
   * (this can occur during the widget is built and shall not
   * remove an existing mask_name)
   */
  if (active_index >= 0)
  {
    p_pw_mask_name_reference_update(pw, selected_mask_name);
  }

}  /* end p_pw_mask_name_combo_changed_cb */



/* --------------------------------
 * p_pw_filename_changed
 * --------------------------------
 */
static void
p_pw_filename_changed(const char *filename, GapStbPropWidget *pw)
{
  if(pw == NULL)  { return; }
  if(pw->stb_elem_refptr == NULL)  { return; }

  p_pw_push_undo_and_set_unsaved_changes(pw);

  if(filename)
  {
    gint len;
    len = strlen(filename);
    if(len > 0)
    {
      char *l_filename;
      
      l_filename = g_strdup(filename);
      gimp_set_data(GAP_STORY_PROPERTIES_PREVIOUS_FILENAME, l_filename, len+1);
      g_free(l_filename);
    }
  }
  
  gap_story_upd_elem_from_filename(pw->stb_elem_refptr, filename);
  p_pw_check_ainfo_range(pw, pw->stb_elem_refptr->orig_filename);

  /* update pw stuff */
  p_pw_update_properties(pw);

}  /* end p_pw_filename_changed */


/* --------------------------------
 * p_filesel_pw_ok_cb
 * --------------------------------
 */
static void
p_filesel_pw_ok_cb (GtkWidget *widget
                   ,GapStbPropWidget *pw)
{
  const gchar *filename;
  gchar *dup_filename;

  if(pw == NULL) return;
  if(pw->pw_filesel == NULL) return;

  dup_filename = NULL;
  filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (pw->pw_filesel));
  if(filename)
  {
    if(g_file_test(filename, G_FILE_TEST_EXISTS))
    {
      /* defere p_pw_filename_changed until filesel widget has closed
       * (new videofiles opened for the 1.st time
       *  may take very long time for index creation)
       */
      dup_filename = g_strdup(filename);
    }
    else
    {
      gtk_entry_set_text(GTK_ENTRY(pw->pw_filename_entry), filename);
    }
  }

  gtk_widget_destroy(GTK_WIDGET(pw->pw_filesel));

  if(dup_filename)
  {
      gtk_entry_set_text(GTK_ENTRY(pw->pw_filename_entry), dup_filename);
      p_pw_filename_changed(dup_filename, pw);
      g_free(dup_filename);
  }

  pw->pw_filesel = NULL;
}  /* end p_filesel_pw_ok_cb */


/* -----------------------------
 * p_filesel_pw_close_cb
 * -----------------------------
 */
static void
p_filesel_pw_close_cb ( GtkWidget *widget
                      , GapStbPropWidget *pw)
{
  if(pw->pw_filesel == NULL) return;

  gtk_widget_destroy(GTK_WIDGET(pw->pw_filesel));
  pw->pw_filesel = NULL;   /* now filesel_story is closed */

}  /* end p_filesel_pw_close_cb */



/* -----------------------------
 * p_pw_filesel_button_cb
 * -----------------------------
 */
static void
p_pw_filesel_button_cb ( GtkWidget *w
                       , GapStbPropWidget *pw)
{
  GtkWidget *filesel = NULL;

  if(pw->scene_detection_busy)
  {
     return;
  }

  if(pw->pw_filesel != NULL)
  {
     gtk_window_present(GTK_WINDOW(pw->pw_filesel));
     return;   /* filesel is already open */
  }
  if(pw->stb_elem_refptr == NULL) { return; }

  filesel = gtk_file_selection_new ( _("Set Image or Frame Filename"));
  pw->pw_filesel = filesel;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->ok_button,
                    "clicked", G_CALLBACK (p_filesel_pw_ok_cb),
                    pw);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->cancel_button,
                    "clicked", G_CALLBACK (p_filesel_pw_close_cb),
                    pw);
  g_signal_connect (filesel, "destroy",
                    G_CALLBACK (p_filesel_pw_close_cb),
                    pw);


  if(pw->stb_elem_refptr->orig_filename)
  {
    gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                     pw->stb_elem_refptr->orig_filename);
  }
  else
  {
    gchar *previous_filename = NULL;
    gint name_length;

    /* for creating new clips we have no orig_filename in the properties
     * in this case we use the previous used name in this session
     * as default
     */

    name_length = gimp_get_data_size(GAP_STORY_PROPERTIES_PREVIOUS_FILENAME);
    if(name_length > 0)
    {
      previous_filename = g_malloc(name_length);
      gimp_get_data(GAP_STORY_PROPERTIES_PREVIOUS_FILENAME, previous_filename);

    }
    if(previous_filename)
    {
      gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                     previous_filename);
      g_free(previous_filename);
    }
  }
  gtk_widget_show (filesel);

}  /* end p_pw_filesel_button_cb */


/* ==================================================== END FILESEL stuff ======  */
/* ==================================================== START COLORMASK FILESEL stuff ======  */
/* ------------------------------------
 * p_pw_colormask_file_filesel_pw_ok_cb
 * -----------------------------------
 */
static void
p_pw_colormask_file_filesel_pw_ok_cb (GtkWidget *widget
                   ,GapStbPropWidget *pw)
{
  const gchar *colormask_file_name;
  gchar *dup_colormask_file_name;

  if(pw == NULL) return;
  if(pw->pw_colormask_file_filesel == NULL) return;

  dup_colormask_file_name = NULL;
  colormask_file_name = gtk_file_selection_get_filename (GTK_FILE_SELECTION (pw->pw_colormask_file_filesel));
  if(colormask_file_name)
  {
    dup_colormask_file_name = g_strdup(colormask_file_name);
  }

  gtk_widget_destroy(GTK_WIDGET(pw->pw_colormask_file_filesel));

  if(dup_colormask_file_name)
  {
    gtk_entry_set_text(GTK_ENTRY(pw->colormask_file_entry), dup_colormask_file_name);
    g_free(dup_colormask_file_name);
  }

  pw->pw_colormask_file_filesel = NULL;
}  /* end p_pw_colormask_file_filesel_pw_ok_cb */


/* ---------------------------------------
 * p_pw_colormask_file_filesel_pw_close_cb
 * ---------------------------------------
 */
static void
p_pw_colormask_file_filesel_pw_close_cb ( GtkWidget *widget
                      , GapStbPropWidget *pw)
{
  if(pw->pw_colormask_file_filesel == NULL) return;

  gtk_widget_destroy(GTK_WIDGET(pw->pw_colormask_file_filesel));
  pw->pw_colormask_file_filesel = NULL;   /* indicate that filesel is closed */

}  /* end p_pw_colormask_file_filesel_pw_close_cb */



/* -------------------------------------
 * p_pw_colormask_file_filesel_button_cb
 * -------------------------------------
 */
static void
p_pw_colormask_file_filesel_button_cb ( GtkWidget *w
                       , GapStbPropWidget *pw)
{
  GtkWidget *filesel = NULL;

  if(pw->scene_detection_busy)
  {
     return;
  }

  if(pw->pw_colormask_file_filesel != NULL)
  {
     gtk_window_present(GTK_WINDOW(pw->pw_colormask_file_filesel));
     return;   /* filesel is already open */
  }
  if(pw->stb_elem_refptr == NULL) { return; }

  filesel = gtk_file_selection_new ( _("Set Colormask Parameter Filename"));
  pw->pw_colormask_file_filesel = filesel;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->ok_button,
                    "clicked", G_CALLBACK (p_pw_colormask_file_filesel_pw_ok_cb),
                    pw);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->cancel_button,
                    "clicked", G_CALLBACK (p_pw_colormask_file_filesel_pw_close_cb),
                    pw);
  g_signal_connect (filesel, "destroy",
                    G_CALLBACK (p_pw_colormask_file_filesel_pw_close_cb),
                    pw);


  if(pw->stb_elem_refptr->colormask_file)
  {
    gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                     pw->stb_elem_refptr->colormask_file);
  }
  gtk_widget_show (filesel);

}  /* end p_pw_colormask_file_filesel_button_cb */


/* ==================================================== END COLORMASK FILESEL stuff ======  */

/* ==================================================== START FMAC FILESEL stuff ======  */
/* --------------------------------
 * p_pw_fmac_filesel_pw_ok_cb
 * --------------------------------
 */
static void
p_pw_fmac_filesel_pw_ok_cb (GtkWidget *widget
                   ,GapStbPropWidget *pw)
{
  const gchar *fmacname;
  gchar *dup_fmacname;

  if(pw == NULL) return;
  if(pw->pw_fmac_filesel == NULL) return;

  dup_fmacname = NULL;
  fmacname = gtk_file_selection_get_filename (GTK_FILE_SELECTION (pw->pw_fmac_filesel));
  if(fmacname)
  {
    dup_fmacname = g_strdup(fmacname);
  }

  gtk_widget_destroy(GTK_WIDGET(pw->pw_fmac_filesel));

  if(dup_fmacname)
  {
    gtk_entry_set_text(GTK_ENTRY(pw->fmac_entry), dup_fmacname);
    g_free(dup_fmacname);
  }

  pw->pw_fmac_filesel = NULL;
}  /* end p_pw_fmac_filesel_pw_ok_cb */


/* -----------------------------
 * p_pw_fmac_filesel_pw_close_cb
 * -----------------------------
 */
static void
p_pw_fmac_filesel_pw_close_cb ( GtkWidget *widget
                      , GapStbPropWidget *pw)
{
  if(pw->pw_fmac_filesel == NULL) return;

  gtk_widget_destroy(GTK_WIDGET(pw->pw_fmac_filesel));
  pw->pw_fmac_filesel = NULL;   /* indicate that filesel is closed */

}  /* end p_pw_fmac_filesel_pw_close_cb */



/* -----------------------------
 * p_pw_fmac_filesel_button_cb
 * -----------------------------
 */
static void
p_pw_fmac_filesel_button_cb ( GtkWidget *w
                       , GapStbPropWidget *pw)
{
  GtkWidget *filesel = NULL;

  if(pw->scene_detection_busy)
  {
     return;
  }

  if(pw->pw_fmac_filesel != NULL)
  {
     gtk_window_present(GTK_WINDOW(pw->pw_fmac_filesel));
     return;   /* filesel is already open */
  }
  if(pw->stb_elem_refptr == NULL) { return; }

  filesel = gtk_file_selection_new ( _("Set Filtermacro Filename"));
  pw->pw_fmac_filesel = filesel;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->ok_button,
                    "clicked", G_CALLBACK (p_pw_fmac_filesel_pw_ok_cb),
                    pw);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->cancel_button,
                    "clicked", G_CALLBACK (p_pw_fmac_filesel_pw_close_cb),
                    pw);
  g_signal_connect (filesel, "destroy",
                    G_CALLBACK (p_pw_fmac_filesel_pw_close_cb),
                    pw);


  if(pw->stb_elem_refptr->filtermacro_file)
  {
    gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                     pw->stb_elem_refptr->filtermacro_file);
  }
  gtk_widget_show (filesel);

}  /* end p_pw_fmac_filesel_button_cb */


/* ==================================================== END FMAC FILESEL stuff ======  */



/* ----------------------------
 * p_pw_comment_entry_update_cb
 * ----------------------------
 */
static void
p_pw_comment_entry_update_cb(GtkWidget *widget, GapStbPropWidget *pw)
{
  if(pw == NULL) { return; }
  if(pw->stb_elem_refptr == NULL) { return; }
  if(pw->stb_refptr)
  {
    pw->stb_refptr->unsaved_changes = TRUE;
  }

  gap_stb_undo_push_clip(pw->tabw
          , GAP_STB_FEATURE_PROPERTIES_CLIP
          , pw->stb_elem_refptr->story_id
          );

  if(pw->stb_elem_refptr->comment == NULL)
  {
    pw->stb_elem_refptr->comment = gap_story_new_elem(GAP_STBREC_VID_COMMENT);
  }

  if(pw->stb_elem_refptr->comment)
  {
    if(pw->stb_elem_refptr->comment->orig_src_line)
    {
      g_free(pw->stb_elem_refptr->comment->orig_src_line);
    }
    pw->stb_elem_refptr->comment->orig_src_line = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
  }
}  /* end p_pw_comment_entry_update_cb */

/* -----------------------------------
 * p_pw_colormask_file_entry_update_cb
 * -----------------------------------
 */
static void
p_pw_colormask_file_entry_update_cb(GtkWidget *widget, GapStbPropWidget *pw)
{
  if(pw == NULL) { return; }
  if(pw->stb_elem_refptr == NULL) { return; }
  if(pw->stb_refptr)
  {
    p_pw_push_undo_and_set_unsaved_changes(pw);
  }

  if(pw->stb_elem_refptr->colormask_file)
  {
    g_free(pw->stb_elem_refptr->colormask_file);
  }

  pw->stb_elem_refptr->colormask_file = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

}  /* end p_pw_colormask_file_entry_update_cb */


/* ----------------------------
 * p_pw_fmac_entry_update_cb
 * ----------------------------
 */
static void
p_pw_fmac_entry_update_cb(GtkWidget *widget, GapStbPropWidget *pw)
{
  if(pw == NULL) { return; }
  if(pw->stb_elem_refptr == NULL) { return; }
  if(pw->stb_refptr)
  {
    p_pw_push_undo_and_set_unsaved_changes(pw);
  }

  if(pw->stb_elem_refptr->filtermacro_file)
  {
    g_free(pw->stb_elem_refptr->filtermacro_file);
  }

  pw->stb_elem_refptr->filtermacro_file = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  p_pw_check_fmac_sensitivity(pw);
  
}  /* end p_pw_fmac_entry_update_cb */


/* -----------------------------------------------
 * p_pw_update_info_labels_and_cliptype_senstivity
 * -----------------------------------------------
 */
static void
p_pw_update_info_labels_and_cliptype_senstivity(GapStbPropWidget *pw)
{
  char    txt_buf[100];
  gdouble l_speed_fps;
  gboolean l_sensitive;
  gboolean l_mov_sensitive;
  gboolean l_mov_show;
  gboolean l_show;

  if(pw == NULL) { return; }
  if(pw->stb_elem_refptr == NULL) { return; }

  gap_story_elem_calculate_nframes(pw->stb_elem_refptr);

  l_sensitive = FALSE;
  l_mov_sensitive = FALSE;
  switch(pw->stb_elem_refptr->record_type)
  {
    case GAP_STBREC_VID_SILENCE:
      gtk_label_set_text ( GTK_LABEL(pw->cliptype_label), _("EMPTY"));
      break;
    case GAP_STBREC_VID_COLOR:
      gtk_label_set_text ( GTK_LABEL(pw->cliptype_label), _("COLOR"));
      break;
    case GAP_STBREC_VID_IMAGE:
      gtk_label_set_text ( GTK_LABEL(pw->cliptype_label), _("SINGLE-IMAGE"));
      break;
    case GAP_STBREC_VID_ANIMIMAGE:
      l_sensitive = TRUE;
      gtk_label_set_text ( GTK_LABEL(pw->cliptype_label), _("ANIM-IMAGE"));
      break;
    case GAP_STBREC_VID_FRAMES:
      l_sensitive = TRUE;
      gtk_label_set_text ( GTK_LABEL(pw->cliptype_label), _("FRAME-IMAGES"));
      break;
    case GAP_STBREC_VID_MOVIE:
      l_sensitive = TRUE;
      l_mov_sensitive = TRUE;
      gtk_label_set_text ( GTK_LABEL(pw->cliptype_label), _("MOVIE"));
      break;
    case GAP_STBREC_VID_SECTION:
      l_sensitive = TRUE;
      gtk_label_set_text ( GTK_LABEL(pw->cliptype_label), _("SECTION"));
      break;
    case GAP_STBREC_VID_BLACKSECTION:
      l_sensitive = TRUE;
      gtk_label_set_text ( GTK_LABEL(pw->cliptype_label), _("BLACKSECTION"));
      break;
    case GAP_STBREC_VID_COMMENT:
      gtk_label_set_text ( GTK_LABEL(pw->cliptype_label), _("COMMENT"));
      break;
    default:
      gtk_label_set_text ( GTK_LABEL(pw->cliptype_label), _("** UNKNOWN **"));
  }
  l_mov_show = l_mov_sensitive;
  l_show = l_sensitive;
  
  gap_story_dlg_render_default_icon(pw->stb_elem_refptr, pw->typ_icon_pv_ptr);

  /* record_type dependent sensitivity */
  p_pw_nullable_widget_set_sensitive(pw->pingpong_label, l_sensitive, l_show);
  p_pw_nullable_widget_set_sensitive(pw->pingpong_toggle, l_sensitive, l_show);
  
  p_pw_nullable_gimp_scale_entry_set_sensitive(pw->pw_spinbutton_from_adj
      , l_sensitive
      , l_show
      );
  p_pw_nullable_gimp_scale_entry_set_sensitive(pw->pw_spinbutton_to_adj
      , l_sensitive
      , l_show
      );
  p_pw_nullable_gimp_scale_entry_set_sensitive(pw->pw_spinbutton_step_density_adj
      , l_sensitive
      , l_show
      );

  p_pw_nullable_gimp_scale_entry_set_sensitive(pw->pw_spinbutton_seltrack_adj
      , l_mov_sensitive
      , l_mov_show
      );

  g_snprintf(txt_buf, sizeof(txt_buf), _("%d (frames)")
            ,(int)pw->stb_elem_refptr->nframes
            );
  gtk_label_set_text ( GTK_LABEL(pw->dur_frames_label), txt_buf);

  l_speed_fps = GAP_STORY_DEFAULT_FRAMERATE;
  if(pw->stb_refptr)
  {
    if(pw->stb_refptr->master_framerate > 0)
    {
      l_speed_fps = pw->stb_refptr->master_framerate;
    }
  }
  gap_timeconv_framenr_to_timestr( pw->stb_elem_refptr->nframes
                         , l_speed_fps
                         , txt_buf
                         , sizeof(txt_buf)
                         );
  gtk_label_set_text ( GTK_LABEL(pw->dur_time_label), txt_buf);
}  /* end p_pw_update_info_labels_and_cliptype_senstivity */


/* ---------------------------------
 * p_pw_update_framenr_labels
 * ---------------------------------
 */
static void
p_pw_update_framenr_labels(GapStbPropWidget *pw, gint32 framenr)
{
  char    txt_buf[100];
  gdouble l_speed_fps;
  gint32  l_framenr_zero;
  
  if(pw == NULL) { return; }
  if(pw->stb_elem_refptr == NULL) { return; }


  g_snprintf(txt_buf, sizeof(txt_buf), "%d  "
            ,(int)framenr
            );
  gtk_label_set_text ( GTK_LABEL(pw->pw_framenr_label), txt_buf);

  l_speed_fps = GAP_STORY_DEFAULT_FRAMERATE;
  l_framenr_zero = framenr -1;
  if(pw->stb_elem_refptr->record_type == GAP_STBREC_VID_ANIMIMAGE)
  {
    /* animimage framenr (i.e. layerstack position) starts at 0
     * (other clips typically start with framenr 1)
     */
    l_framenr_zero = framenr;
  }

  if(pw->stb_refptr)
  {
    if(pw->stb_refptr->master_framerate > 0)
    {
      l_speed_fps = pw->stb_refptr->master_framerate;
    }
  }

  // TODO: can not show the exact frametime for
  // clips that do not start at frame 1

  

  gap_timeconv_framenr_to_timestr(l_framenr_zero
                         , l_speed_fps
                         , txt_buf
                         , sizeof(txt_buf)
                         );
  gtk_label_set_text ( GTK_LABEL(pw->pw_frametime_label), txt_buf);
}  /* end p_pw_update_framenr_labels */


/* ---------------------------------
 * p_pw_update_properties
 * ---------------------------------
 * 1) update the info labels in the properties dialog window
 * 2) deferred update the storyboard table tabw in the master dialog window,
 *    where pw is attached to.
 *    (immediate reflect the change of properties in the corresponding table.)
 */
static void
p_pw_update_properties(GapStbPropWidget *pw)
{
  p_pw_update_info_labels_and_cliptype_senstivity(pw);

  pw->go_render_all_request = TRUE;
  pw->go_job_framenr = pw->stb_elem_refptr->from_frame;
  if(pw->go_timertag < 0)
  {
    pw->go_timertag = (gint32) g_timeout_add(16, (GtkFunction)p_pw_timer_go_job, pw);
  }

}  /* end p_pw_update_properties */


/* ---------------------------------------
 * gap_story_pw_trigger_refresh_properties
 * ---------------------------------------
 */
void
gap_story_pw_trigger_refresh_properties(GapStbPropWidget *pw)
{
  p_pw_set_strings_for_mask_name_combo(pw);
  p_pw_update_properties(pw);
}  /* end gap_story_pw_trigger_refresh_properties */

/* ---------------------------------
 * p_pw_gint32_adjustment_callback
 * ---------------------------------
 */
static void
p_pw_gint32_adjustment_callback(GtkObject *obj, gint32 *val)
{
  GapStbPropWidget *pw;
  gint32 l_val;


  pw = g_object_get_data( G_OBJECT(obj), "pw" );
  if(pw)
  {
    if(pw->stb_elem_refptr)
    {
      l_val = RINT (GTK_ADJUSTMENT(obj)->value);
      if(gap_debug) printf("gint32_adjustment_callback: old_val:%d val:%d\n", (int)*val ,(int)l_val );
      if(l_val != *val)
      {
        p_pw_push_undo_and_set_unsaved_changes(pw);

        *val = l_val;
        p_pw_update_properties(pw);
      }
    }
  }

}  /* end p_pw_gint32_adjustment_callback */


/* ------------------------------------
 * p_pw_pingpong_toggle_update_callback
 * ------------------------------------
 */
static void
p_pw_pingpong_toggle_update_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if(pw)
  {
    if(pw->stb_elem_refptr)
    {
      p_pw_push_undo_and_set_unsaved_changes(pw);

      if (GTK_TOGGLE_BUTTON (widget)->active)
      {
        pw->stb_elem_refptr->playmode = GAP_STB_PM_PINGPONG;
      }
      else
      {
        pw->stb_elem_refptr->playmode = GAP_STB_PM_NORMAL;
      }
      p_pw_update_properties(pw);
    }
  }
}  /* end p_pw_pingpong_toggle_update_callback */

/* ---------------------------------------
 * p_pw_mask_enable_toggle_update_callback
 * ---------------------------------------
 */
static void
p_pw_mask_enable_toggle_update_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if(pw)
  {
    if(pw->stb_elem_refptr)
    {
      p_pw_push_undo_and_set_unsaved_changes(pw);


      if (GTK_TOGGLE_BUTTON (widget)->active)
      {
        pw->stb_elem_refptr->mask_disable = FALSE;
      }
      else
      {
        pw->stb_elem_refptr->mask_disable = TRUE;
      }
      p_pw_update_properties(pw);
    }
  }
}  /* end p_pw_mask_enable_toggle_update_callback */


/* ---------------------------------
 * p_radio_mask_anchor_clip_callback
 * ---------------------------------
 */
static void
p_radio_mask_anchor_clip_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if((pw) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    pw->mask_anchor = GAP_MSK_ANCHOR_CLIP;
    if(pw->stb_elem_refptr)
    {
      p_pw_push_undo_and_set_unsaved_changes(pw);
      pw->stb_elem_refptr->mask_anchor = pw->mask_anchor;
      p_pw_set_colormask_param_sensitivity(pw);
    }
  }
}  /* end p_radio_mask_anchor_clip_callback */


/* -----------------------------------
 * p_radio_mask_anchor_master_callback
 * -----------------------------------
 */
static void
p_radio_mask_anchor_master_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if((pw) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    pw->mask_anchor = GAP_MSK_ANCHOR_MASTER;
    if(pw->stb_elem_refptr)
    {
      p_pw_push_undo_and_set_unsaved_changes(pw);
      pw->stb_elem_refptr->mask_anchor = pw->mask_anchor;
      p_pw_set_colormask_param_sensitivity(pw);
    }
  }
}  /* end p_radio_mask_anchor_master_callback */

/* -----------------------------------
 * p_radio_mask_anchor_xcolor_callback
 * -----------------------------------
 */
static void
p_radio_mask_anchor_xcolor_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if((pw) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    pw->mask_anchor = GAP_MSK_ANCHOR_XCOLOR;
    if(pw->stb_elem_refptr)
    {
      p_pw_push_undo_and_set_unsaved_changes(pw);
      pw->stb_elem_refptr->mask_anchor = pw->mask_anchor;
      p_pw_set_colormask_param_sensitivity(pw);
    }
  }
}  /* end p_radio_mask_anchor_xcolor_callback */


/* ---------------------------------
 * p_radio_delace_none_callback
 * ---------------------------------
 */
static void
p_radio_delace_none_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if((pw) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    p_pw_push_undo_and_set_unsaved_changes(pw);
    pw->delace_mode = 0;
    gtk_widget_set_sensitive(pw->pw_spinbutton_delace, FALSE);
    if(pw->stb_elem_refptr)
    {
      pw->stb_elem_refptr->delace = pw->delace_mode + CLAMP(pw->delace_threshold, 0.0, 0.999999);
    }
  }
}  /* end p_radio_delace_none_callback */

/* ---------------------------------
 * p_radio_delace_odd_callback
 * ---------------------------------
 */
static void
p_radio_delace_odd_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if((pw) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    p_pw_push_undo_and_set_unsaved_changes(pw);
    pw->delace_mode = 1;
    gtk_widget_set_sensitive(pw->pw_spinbutton_delace, TRUE);
    if(pw->stb_elem_refptr)
    {
      pw->stb_elem_refptr->delace = pw->delace_mode + CLAMP(pw->delace_threshold, 0.0, 0.999999);
    }
  }
}  /* end p_radio_delace_odd_callback */

/* ---------------------------------
 * p_radio_delace_even_callback
 * ---------------------------------
 */
static void
p_radio_delace_even_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if((pw) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    p_pw_push_undo_and_set_unsaved_changes(pw);
    pw->delace_mode = 2;
    gtk_widget_set_sensitive(pw->pw_spinbutton_delace, TRUE);
    if(pw->stb_elem_refptr)
    {
      pw->stb_elem_refptr->delace = pw->delace_mode + CLAMP(pw->delace_threshold, 0.0, 0.999999);
    }
  }
}  /* end p_radio_delace_even_callback */


/* ---------------------------------
 * p_radio_delace_odd_first_callback
 * ---------------------------------
 */
static void
p_radio_delace_odd_first_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if((pw) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    p_pw_push_undo_and_set_unsaved_changes(pw);
    pw->delace_mode = 3;
    gtk_widget_set_sensitive(pw->pw_spinbutton_delace, TRUE);
    if(pw->stb_elem_refptr)
    {
      pw->stb_elem_refptr->delace = pw->delace_mode + CLAMP(pw->delace_threshold, 0.0, 0.999999);
    }
  }
}  /* end p_radio_delace_odd_first_callback */

/* ---------------------------------
 * p_radio_delace_even_first_callback
 * ---------------------------------
 */
static void
p_radio_delace_even_first_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if((pw) && (GTK_TOGGLE_BUTTON (widget)->active))
  {
    p_pw_push_undo_and_set_unsaved_changes(pw);
    pw->delace_mode = 4;
    gtk_widget_set_sensitive(pw->pw_spinbutton_delace, TRUE);
    if(pw->stb_elem_refptr)
    {
      pw->stb_elem_refptr->delace = pw->delace_mode + CLAMP(pw->delace_threshold, 0.0, 0.999999);
    }
  }
}  /* end p_radio_delace_even_first_callback */

/* ---------------------------------
 * p_delace_spinbutton_cb
 * ---------------------------------
 */
static void
p_delace_spinbutton_cb(GtkObject *obj, GapStbPropWidget *pw)
{
  if(pw)
  {
    pw->delace_threshold = (GTK_ADJUSTMENT(pw->pw_spinbutton_delace_adj)->value);
    if(pw->stb_elem_refptr)
    {
      p_pw_push_undo_and_set_unsaved_changes(pw);
      pw->stb_elem_refptr->delace = pw->delace_mode + CLAMP(pw->delace_threshold, 0.0, 0.999999);
    }
  }
}  /* end p_delace_spinbutton_cb */


/* ---------------------------------
 * p_step_density_spinbutton_cb
 * ---------------------------------
 */
static void
p_step_density_spinbutton_cb(GtkObject *obj, GapStbPropWidget *pw)
{
  gdouble l_val;
  if(pw)
  {
    l_val = (GTK_ADJUSTMENT(pw->pw_spinbutton_step_density_adj)->value);
    if(pw->stb_elem_refptr)
    {
      if(pw->stb_elem_refptr->step_density != l_val)
      {
        p_pw_push_undo_and_set_unsaved_changes(pw);
        pw->stb_elem_refptr->step_density = l_val;
        gap_story_elem_calculate_nframes(pw->stb_elem_refptr);

        p_pw_update_properties(pw);
      }
    }
  }
}  /* end p_step_density_spinbutton_cb */



/* ---------------------------------
 * p_maskstep_density_spinbutton_cb
 * ---------------------------------
 */
static void
p_maskstep_density_spinbutton_cb(GtkObject *obj, GapStbPropWidget *pw)
{
  gdouble l_val;
  if(pw)
  {
    l_val = (GTK_ADJUSTMENT(pw->pw_spinbutton_mask_stepsize_adj)->value);
    if(pw->stb_elem_refptr)
    {
      if(pw->stb_elem_refptr->mask_stepsize != l_val)
      {
        p_pw_push_undo_and_set_unsaved_changes(pw);
        pw->stb_elem_refptr->mask_stepsize = l_val;

        p_pw_update_properties(pw);
      }
    }
  }
}  /* end p_maskstep_density_spinbutton_cb */



/* -------------------------------
 * p_fmac_steps_spinbutton_cb
 * -------------------------------
 */
static void
p_fmac_steps_spinbutton_cb(GtkObject *obj, GapStbPropWidget *pw)
{
  gint32 l_val;
  if(pw)
  {
    l_val = (GTK_ADJUSTMENT(pw->pw_spinbutton_fmac_steps_adj)->value);
    if(pw->stb_elem_refptr)
    {
      if(pw->stb_elem_refptr->fmac_total_steps != l_val)
      {
        p_pw_push_undo_and_set_unsaved_changes(pw);
        pw->stb_elem_refptr->fmac_total_steps = l_val;

        p_pw_check_fmac_sensitivity(pw);

      }
    }
  }
}  /* end p_fmac_steps_spinbutton_cb */

/* -------------------------------
 * p_fmac_accel_spinbutton_cb
 * -------------------------------
 */
static void
p_fmac_accel_spinbutton_cb(GtkObject *obj, GapStbPropWidget *pw)
{
  gint32 l_val;

  if(pw)
  {
    l_val = (GTK_ADJUSTMENT(pw->pw_spinbutton_fmac_accel_adj)->value);
    if(pw->stb_elem_refptr)
    {
      if(pw->stb_elem_refptr->fmac_accel != l_val)
      {
        p_pw_push_undo_and_set_unsaved_changes(pw);
        pw->stb_elem_refptr->fmac_accel = l_val;

      }
    }
  }
}  /* end p_fmac_accel_spinbutton_cb */


/* ---------------------------------
 * p_radio_flip_update
 * ---------------------------------
 */
static void
p_radio_flip_update(GapStbPropWidget *pw, gint32 flip_request)
{
  if(pw)
  {
    pw->flip_request = flip_request;
    if(pw->stb_elem_refptr)
    {
      p_pw_push_undo_and_set_unsaved_changes(pw);
      pw->stb_elem_refptr->flip_request = pw->flip_request;
      p_pw_update_properties(pw);
      p_pw_propagate_mask_attribute_changes(pw);
    }
  }
}  /* end p_radio_flip_update */

/* ---------------------------------
 * p_radio_flip_none_callback
 * ---------------------------------
 */
static void
p_radio_flip_none_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if(GTK_TOGGLE_BUTTON (widget)->active)
  {
    p_radio_flip_update(pw, GAP_STB_FLIP_NONE);
  }
}  /* end p_radio_flip_none_callback */


/* ---------------------------------
 * p_radio_flip_hor_callback
 * ---------------------------------
 */
static void
p_radio_flip_hor_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if(GTK_TOGGLE_BUTTON (widget)->active)
  {
    p_radio_flip_update(pw, GAP_STB_FLIP_HOR);
  }
}  /* end p_radio_flip_hor_callback */

/* ---------------------------------
 * p_radio_flip_ver_callback
 * ---------------------------------
 */
static void
p_radio_flip_ver_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if(GTK_TOGGLE_BUTTON (widget)->active)
  {
    p_radio_flip_update(pw, GAP_STB_FLIP_VER);
  }
}  /* end p_radio_flip_ver_callback */

/* ---------------------------------
 * p_radio_flip_both_callback
 * ---------------------------------
 */
static void
p_radio_flip_both_callback(GtkWidget *widget, GapStbPropWidget *pw)
{
  if(GTK_TOGGLE_BUTTON (widget)->active)
  {
    p_radio_flip_update(pw, GAP_STB_FLIP_BOTH);
  }
}  /* end p_radio_flip_both_callback */



/* ------------------------------------
 * on_clip_elements_dropped_as_mask_ref
 * ------------------------------------
 * signal handler called when a frame widget (containing a mask definition or videoclip)
 * was dropped onto a property widget.
 *
 * The triggered action is to create (or replace) the mask_name reference
 * of the current property widget.
 * If the dropped element was not a mask definition, then implicitly
 * add it to the mask definitions.
 *
 * NOTE: mask definition property windows can NOT act as drop destination.
 */
static void
on_clip_elements_dropped_as_mask_ref (GtkWidget        *widget,
                   GdkDragContext   *context,
                   gint              x,
                   gint              y,
                   GtkSelectionData *selection_data,
                   guint             info,
                   guint             time)
{
  GapStbTabWidgets *tabw;
  GapStbMainGlobalParams  *sgpp;
  GapStbPropWidget *pw;
  GapStbFrameWidget *fw_drop;
  GapStbTabWidgets *tabw_src;

  if(gap_debug)
  {
    printf("  ** >>>> DROP handler(COPY:%d MOVE:%d )  context->action:%d\n"
      , (int)GDK_ACTION_COPY
      , (int)GDK_ACTION_MOVE
      , (int)context->action
      );
  }
  fw_drop = NULL;
  tabw_src = NULL;

  pw = g_object_get_data (G_OBJECT (widget)
                        , GAP_STORY_PW_PTR);

  /* check if we were invoked from a frame widget */
  if(pw == NULL)     { return; }


  tabw = (GapStbTabWidgets  *)pw->tabw;
  if(tabw == NULL)   { return; }

  sgpp = (GapStbMainGlobalParams *)tabw->sgpp;
  if(sgpp == NULL)   { return; }

  if(pw->stb_elem_refptr == NULL)    { return; }

  if(pw->stb_elem_refptr->track == GAP_STB_MASK_TRACK_NUMBER) { return; }

  if(pw->stb_refptr == NULL)
  {
    return;
  }

  switch (info)
  {
    case GAP_STB_TARGET_STORYBOARD_ELEM:
      {
        GapStbFrameWidget **fw_drop_ptr;

        fw_drop_ptr = (GapStbFrameWidget **)selection_data->data;
        fw_drop = *fw_drop_ptr;
        if(gap_debug)
        {
          printf("on_clip_elements_dropped_as_mask_ref FW_DROP:%ld\n", (long)fw_drop);
        }
        if (fw_drop == NULL)
        {
          return;
        }
        tabw_src = (GapStbTabWidgets *)fw_drop->tabw;
        if ((tabw_src == NULL)
        || ((tabw_src != sgpp->cll_widgets) && (tabw_src != sgpp->stb_widgets)))
        {
          /* if tabw of the droped frame widget
           * is not equal to one of stb_widgets or cll_widgets
           * assume that the sender was another application
           * which is not supported for drop type GAP_STB_TARGET_STORYBOARD_ELEM.
           */
          return;
        }

        if(fw_drop->stb_elem_refptr)
        {
          gchar *mask_name_new;
          
          GapStoryElem *known_maskdef_elem;
          
          
          p_pw_push_undo_and_set_unsaved_changes(pw);
          
          mask_name_new = NULL;
          known_maskdef_elem = gap_story_find_maskdef_equal_to_ref_elem(pw->stb_refptr
                                  , fw_drop->stb_elem_refptr);
          if(known_maskdef_elem)
          {
            mask_name_new = g_strdup(known_maskdef_elem->mask_name);
          }
          else
          {
            GapStoryElem *stb_elem_dup;
            
            mask_name_new = gap_story_generate_unique_maskname(pw->stb_refptr);
            /* implicitly create a new mask definition from the dropped
             * clip and change properties of current pw to refer
             * to the newly created mask definition
             */
            stb_elem_dup = gap_story_elem_duplicate(fw_drop->stb_elem_refptr);
            if(stb_elem_dup->mask_name)
            {
              g_free(stb_elem_dup->mask_name);
            }
            stb_elem_dup->mask_name = g_strdup(mask_name_new);
            stb_elem_dup->track = GAP_STB_MASK_TRACK_NUMBER;
            gap_story_list_append_elem(pw->stb_refptr, stb_elem_dup);
            pw->go_recreate_request = TRUE;
          }
          
          /* set mask reference */
          if(mask_name_new)
          {
            if(pw->stb_elem_refptr->mask_name)
            {
              g_free(pw->stb_elem_refptr->mask_name);
            }
            pw->stb_elem_refptr->mask_name = g_strdup(mask_name_new);
            gtk_entry_set_text(GTK_ENTRY(pw->pw_mask_name_entry), mask_name_new);
            g_free(mask_name_new);
            p_pw_render_layermask(pw);
          }
          p_pw_update_properties(pw);
        }

      }
      break;
    default:
      return;
  }


  if (gap_debug)
  {
    printf("  ** << DROP handler end\n\n");
  }
}  /* end on_clip_elements_dropped_as_mask_ref */



/* ---------------------------------
 * p_pw_dialog_init_dnd
 * ---------------------------------
 * accept dropping of application internal storyboard elements
 * to add (or replace) the current mask_name (only for mask references)
 *
 * if the dropped element is NOT a mask definition, then implicitly
 * add it to the mask definitions.
 */
static void
p_pw_dialog_init_dnd(GapStbPropWidget *pw)
{
  GtkWidget *widget;
  static const GtkTargetEntry drop_types[] = {
    { "application/gimp-gap-story-elem", GTK_TARGET_SAME_APP, GAP_STB_TARGET_STORYBOARD_ELEM}
  };
  static const gint n_drop_types = sizeof(drop_types)/sizeof(drop_types[0]);

  widget = pw->pw_prop_dialog;
  
  g_object_set_data (G_OBJECT (widget), GAP_STORY_PW_PTR, (gpointer)pw);

  if(pw->stb_elem_refptr == NULL)
  {
    return;
  }

  /* Dont accept dropping frame widgets on mask definition elements */
  if(pw->stb_elem_refptr->track == GAP_STB_MASK_TRACK_NUMBER)
  {
    return;
  }

  gtk_drag_dest_set (GTK_WIDGET (widget),
                     GTK_DEST_DEFAULT_ALL,
                     drop_types, n_drop_types,
                     GDK_ACTION_COPY);
  g_signal_connect (widget, "drag_data_received",
                    G_CALLBACK (on_clip_elements_dropped_as_mask_ref), NULL);

}  /* end p_pw_dialog_init_dnd */


/* -------------------------------
 * p_pw_check_fmac_sensitivity
 * -------------------------------
 * - set sensitivity for the filtermacro steps spinbutton.
 *   It is sensitive if both filtermacro_file and an 
 *   implicit alternative filtermacro_file (MACRO2) are existent and valid.
 *   In this case filter apply with varying values can be used
 *   by entering a value > 1 into the filtermacro steps spinbutton.
 *
 * - The presence of the alternative filtermacro_file is also
 *   displayed in the label pw_label_alternate_fmac_file
 *   via shortened version of the alternate filtermacro filename.
 * - This label is set to space if no valid alternate filtermacro_file
 *   is available. This label also indicates if MACRO2 is active
 *   or not active by adding Suffix "(ON)" or "(OFF)"
 */
static void
p_pw_check_fmac_sensitivity(GapStbPropWidget *pw)
{
  gboolean sensitive;
  gboolean accelSensitive;
  char *alt_fmac_name;
  char *lbl_text;
  
  accelSensitive = FALSE;
  sensitive = FALSE;
  lbl_text = NULL;
  if(pw->stb_elem_refptr)
  {
    if(pw->stb_elem_refptr->filtermacro_file)
    {
       if(gap_fmac_chk_filtermacro_file(pw->stb_elem_refptr->filtermacro_file))
       {
         alt_fmac_name = gap_fmac_get_alternate_name(pw->stb_elem_refptr->filtermacro_file);
         if (alt_fmac_name)
         {
           if(gap_fmac_chk_filtermacro_file(alt_fmac_name))
           {
             sensitive = TRUE;
             if(pw->stb_elem_refptr->fmac_total_steps > 1)
             {
               lbl_text = gap_base_shorten_filename(_("ON:") /* prefix */
                                     ,alt_fmac_name            /* filenamepart */
                                     ,""                       /* suffix */
                                     ,52                       /* max_chars */
                                     );
                accelSensitive = TRUE;
             }
             else
             {
               lbl_text = gap_base_shorten_filename(_("OFF:") /* prefix */
                                     ,alt_fmac_name             /* filenamepart */
                                     ,""                        /* suffix */
                                     ,52                        /* max_chars */
                                     );
             }
                        
           }
         }
       }
    }
  }

  if (lbl_text == NULL)
  {
    lbl_text = g_strdup(" ");
    gtk_label_set_text ( GTK_LABEL(pw->pw_label_alternate_fmac2), "" );
    gtk_widget_hide (pw->pw_hbox_fmac2);
    gtk_widget_hide (pw->pw_spinbutton_fmac_accel);
  }
  else
  {
    gtk_widget_show (pw->pw_hbox_fmac2);
    gtk_widget_show (pw->pw_spinbutton_fmac_accel);
    gtk_label_set_text ( GTK_LABEL(pw->pw_label_alternate_fmac2), _("Filtermacro2: ") );
  }
  
  gtk_label_set_text ( GTK_LABEL(pw->pw_label_alternate_fmac_file), lbl_text );
  gtk_widget_set_sensitive(pw->pw_spinbutton_fmac_steps, sensitive);
  gtk_widget_set_sensitive(pw->pw_spinbutton_fmac_accel, accelSensitive);
  p_pw_update_properties(pw);

  g_free(lbl_text);

}  /* end p_pw_check_fmac_sensitivity */


/* -------------------------------
 * p_pw_create_clip_pv_container
 * -------------------------------
 */
static GtkWidget *
p_pw_create_clip_pv_container(GapStbPropWidget *pw)
{
  GapStbTabWidgets *tabw;
  GtkWidget *alignment;
  GtkWidget *event_box;
  GtkWidget *vbox_clip;
  GtkWidget *label;
  GtkWidget *hbox;
  gdouble    thumb_scale;
  gint32     l_check_size;
  
  tabw = (GapStbTabWidgets *)pw->tabw;
  
  /* the vbox for displaying the clip  */
  vbox_clip = gtk_vbox_new (FALSE, 1);

  /* Create an EventBox (container for the preview drawing_area)
   */
  event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (event_box), vbox_clip);
  gtk_widget_set_events (event_box, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (G_OBJECT (event_box), "button_press_event",
                    G_CALLBACK (p_pw_preview_events_cb),
                    pw);


  /*  The thumbnail preview  */
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox_clip), alignment, FALSE, FALSE, 1);
  gtk_widget_show (alignment);

  /* thumbnail 3 times bigger */
  thumb_scale = 1.0;
  if(tabw->thumbsize < 256)
  {
    thumb_scale = 256 / MAX(tabw->thumbsize, 1);
  }
  l_check_size = (thumb_scale * tabw->thumbsize) / 16;
  pw->pv_ptr = gap_pview_new( (thumb_scale * (gdouble)tabw->thumb_width) + 4.0
                            , (thumb_scale * (gdouble)tabw->thumb_height) + 4.0
                            , l_check_size
                            , NULL   /* no aspect_frame is used */
                            );
  if(pw->is_mask_definition)
  {
    pw->pv_ptr->desaturate_request = TRUE;
  }
  
  gtk_widget_set_events (pw->pv_ptr->da_widget, GDK_EXPOSURE_MASK);
  gtk_container_add (GTK_CONTAINER (alignment), pw->pv_ptr->da_widget);
  gtk_widget_show (pw->pv_ptr->da_widget);
  g_signal_connect (G_OBJECT (pw->pv_ptr->da_widget), "event",
                    G_CALLBACK (p_pw_preview_events_cb),
                    pw);

  hbox = gtk_hbox_new (FALSE, 1);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox_clip), hbox, FALSE, FALSE, 1);

  /* the framenr label  */
  label = gtk_label_new ("000000");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 1);
  gtk_widget_show (label);
  pw->pw_framenr_label = label;

  /* the frametime label  */
  label = gtk_label_new ("mm:ss:msec");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 1);
  gtk_widget_show (label);
  pw->pw_frametime_label = label;



  gtk_widget_show (vbox_clip);
  gtk_widget_show (event_box);

  
  return(event_box);
}  /* end p_pw_create_clip_pv_container */

/* -------------------------------
 * p_pw_create_mask_pv_container
 * -------------------------------
 */
static GtkWidget *
p_pw_create_mask_pv_container(GapStbPropWidget *pw)
{
  GapStbTabWidgets *tabw;
  GtkWidget *alignment;
  GtkWidget *event_box;
  GtkWidget *vbox_mask;
  gdouble    thumb_scale;
  gint32     l_check_size;

  tabw = (GapStbTabWidgets *)pw->tabw;

  /* the vbox for displaying the layermask
   * Is not relevant (hidden) for mask definitions.
   */
  vbox_mask = gtk_vbox_new (FALSE, 1);

  /* Create an EventBox (container for the preview drawing_area)
   */
  event_box = gtk_event_box_new ();
  pw->mask_pv_container = event_box;
  gtk_container_add (GTK_CONTAINER (event_box), vbox_mask);
  gtk_widget_set_events (event_box, GDK_BUTTON_PRESS_MASK);

  if(!pw->is_mask_definition)
  {
    g_signal_connect (G_OBJECT (event_box), "button_press_event",
                      G_CALLBACK (p_pw_mask_preview_events_cb),
                      pw);
  }
  /*  The thumbnail preview  */
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox_mask), alignment, FALSE, FALSE, 1);
  gtk_widget_show (alignment);

  /* thumbnail 3 times bigger */
  thumb_scale = 1.0;
  if(tabw->thumbsize < 256)
  {
    thumb_scale = 256 / MAX(tabw->thumbsize, 1);
  }
  l_check_size = (thumb_scale * tabw->thumbsize) / 16;
  pw->mask_pv_ptr = gap_pview_new( (thumb_scale * (gdouble)tabw->thumb_width) + 4.0
                            , (thumb_scale * (gdouble)tabw->thumb_height) + 4.0
                            , l_check_size
                            , NULL   /* no aspect_frame is used */
                            );
  pw->mask_pv_ptr->desaturate_request = TRUE;

  if(!pw->is_mask_definition)
  {
  
    gtk_widget_set_events (pw->mask_pv_ptr->da_widget, GDK_EXPOSURE_MASK);
    gtk_container_add (GTK_CONTAINER (alignment), pw->mask_pv_ptr->da_widget);
    g_signal_connect (G_OBJECT (pw->mask_pv_ptr->da_widget), "event",
                      G_CALLBACK (p_pw_mask_preview_events_cb),
                      pw);

    gtk_widget_show (pw->mask_pv_ptr->da_widget);

    gtk_widget_show (vbox_mask);
    //gtk_widget_show (event_box);
    gtk_widget_hide (pw->mask_pv_container);  /* the event_box */
    if(pw->stb_elem_refptr)
    {
      /* show the preview widget even if it should be hidden
       * this triggers that gtk realizes the widget
       * and solves the BUG that renders uninitalized data
       * when a desaturated render request occurs before
       * the widget has no window pointer ( da_widget->window is NULL)
       *
       * Hiding of the pw->mask_pv_container is done later on,
       * when rendering the layermask (see p_pw_render_layermask)
       */
      gtk_widget_show (pw->mask_pv_container);
    }

  }

  return(event_box);
}  /* end p_pw_create_mask_pv_container */


/* ----------------------------------
 * p_pw_create_icontype_pv_container
 * ----------------------------------
 */
static GtkWidget *
p_pw_create_icontype_pv_container(GapStbPropWidget *pw)
{
  GtkWidget *alignment;
  GtkWidget *event_box;
  GtkWidget *vbox_icontype;
  gint32     l_check_size;
  
  /* the vbox for displaying the clip  */
  vbox_icontype = gtk_vbox_new (FALSE, 1);

  /* Create an EventBox (container for the preview drawing_area)
   */
  event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (event_box), vbox_icontype);
  gtk_widget_set_events (event_box, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (G_OBJECT (event_box), "button_press_event",
                    G_CALLBACK (p_pw_icontype_preview_events_cb),
                    pw);


  /*  The thumbnail preview  */
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox_icontype), alignment, FALSE, FALSE, 1);
  gtk_widget_show (alignment);

  l_check_size = PW_ICON_TYPE_WIDTH / 16;
  pw->typ_icon_pv_ptr = gap_pview_new( PW_ICON_TYPE_WIDTH
                            , PW_ICON_TYPE_HEIGHT
                            , l_check_size
                            , NULL   /* no aspect_frame is used */
                            );

  gtk_widget_set_events (pw->typ_icon_pv_ptr->da_widget, GDK_EXPOSURE_MASK);
  gtk_container_add (GTK_CONTAINER (alignment), pw->typ_icon_pv_ptr->da_widget);
  gtk_widget_show (pw->typ_icon_pv_ptr->da_widget);
  g_signal_connect (G_OBJECT (pw->typ_icon_pv_ptr->da_widget), "event",
                    G_CALLBACK (p_pw_icontype_preview_events_cb),
                    pw);

  gtk_widget_show (vbox_icontype);
  gtk_widget_show (event_box);

  gap_story_dlg_render_default_icon(pw->stb_elem_refptr, pw->typ_icon_pv_ptr);

  
  return(event_box);
}  /* end p_pw_create_icontype_pv_container */

/* -------------------------------
 * p_pw_clear_widgets
 * -------------------------------
 * clear all widget pointers to NULL
 */
static void
p_pw_clear_widgets(GapStbPropWidget *pw)
{
  pw->mask_name_combo_elem_count = 0;

  pw->pv_ptr = NULL;
  pw->mask_pv_ptr = NULL;
  pw->mask_pv_container = NULL;
  pw->typ_icon_pv_ptr = NULL;

  pw->pw_prop_dialog = NULL;
  pw->pw_filesel = NULL;
  pw->pw_filename_entry = NULL;
  pw->master_table = NULL;
  pw->cliptype_label = NULL;
  pw->dur_frames_label = NULL;
  pw->dur_time_label = NULL;
  pw->pingpong_toggle = NULL;
  pw->comment_entry = NULL;
  pw->pw_fmac_filesel = NULL;
  pw->fmac_entry = NULL;
  pw->pw_colormask_file_filesel = NULL;
  pw->colormask_file_filesel_button = NULL;
  pw->colormask_file_entry = NULL;
  pw->colormask_file_label = NULL;
  pw->pw_spinbutton_from_adj = NULL;
  pw->pw_spinbutton_to_adj = NULL;
  pw->pw_spinbutton_loops_adj = NULL;
  pw->pw_spinbutton_seltrack_adj = NULL;
  pw->pw_spinbutton_delace_adj = NULL;
  pw->pw_spinbutton_delace = NULL;
  pw->pw_spinbutton_step_density_adj = NULL;
  pw->pw_framenr_label = NULL;
  pw->pw_frametime_label = NULL;


  pw->pw_mask_definition_name_label = NULL;
  pw->pw_mask_name_entry = NULL;      /* relevant to enter mask definition */
  pw->mask_name_combo = NULL;         /* selected mask_name reference */ 
  pw->mask_anchor_label = NULL;
  pw->pingpong_label = NULL;
  pw->pw_mask_enable_toggle = NULL;
  pw->pw_mask_anchor_radio_button_arr[0] = NULL;
  pw->pw_mask_anchor_radio_button_arr[1] = NULL;
  pw->pw_mask_anchor_radio_button_arr[2] = NULL;
  
  pw->pw_spinbutton_mask_stepsize_adj = NULL;
  
  pw->pw_spinbutton_fmac_steps = NULL;
  pw->pw_spinbutton_fmac_steps_adj = NULL;
  pw->pw_label_alternate_fmac_file = NULL;

}  /* end p_pw_clear_widgets */


/* -------------------------------
 * gap_story_pw_properties_dialog
 * -------------------------------
 */
GtkWidget *
gap_story_pw_properties_dialog (GapStbPropWidget *pw)
{
  GtkWidget *dlg;
  GtkWidget *main_vbox;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *dummy_table;
  GtkWidget *table_ptr;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *entry;
  GtkWidget *check_button;
  GtkWidget *vbox_pviews;
  gint       row;
  GtkObject *adj;
  GapStbTabWidgets *tabw;
  gdouble           l_lower_limit;

  if(gap_debug)
  {
    printf("gap_story_pw_properties_dialog START\n");
  }


  if(pw == NULL) { return (NULL); }
  if(pw->pw_prop_dialog != NULL) { return(NULL); }   /* is already open */

  tabw = (GapStbTabWidgets *)pw->tabw;
  if(tabw == NULL) { return (NULL); }

  if(gap_debug)
  {
    printf("gap_story_pw_properties_dialog before p_pw_clear_widgets\n");
  }
  p_pw_clear_widgets(pw);


  pw->is_mask_definition = FALSE;
  if(pw->stb_elem_refptr)
  {
    if(pw->stb_elem_refptr->track == GAP_STB_MASK_TRACK_NUMBER)
    {
      pw->is_mask_definition = TRUE;
    }
  }

  if(gap_debug)
  {
    printf("gap_story_pw_properties_dialog before dialog_new\n");
  }


  if(pw->is_mask_definition)
  {
    if(pw->stb_elem_bck)
    {
      dlg = gap_dialog_new (_("Mask Properties"), "gap_story_clip_properties"
                         ,NULL, 0
                         ,gimp_standard_help_func, GAP_STORY_MASK_PROP_HELP_ID

                         ,GIMP_STOCK_RESET, GAP_STORY_RESPONSE_RESET
                         ,GTK_STOCK_CLOSE,  GTK_RESPONSE_CLOSE
                         ,NULL);
    }
    else
    {
      dlg = gap_dialog_new (_("Mask Properties"), "gap_story_clip_properties"
                         ,NULL, 0
                         ,gimp_standard_help_func, GAP_STORY_MASK_PROP_HELP_ID

                         ,GTK_STOCK_CLOSE,  GTK_RESPONSE_CLOSE
                         ,NULL);
    }
  }
  else
  {
    if(pw->stb_elem_bck)
    {
      dlg = gap_dialog_new (_("Clip Properties"), "gap_story_clip_properties"
                         ,NULL, 0
                         ,gimp_standard_help_func, GAP_STORY_CLIP_PROP_HELP_ID

                         ,GIMP_STOCK_RESET, GAP_STORY_RESPONSE_RESET
                         ,_("Find Scene End"), GAP_STORY_RESPONSE_SCENE_END
                         ,_("Auto Scene Split"), GAP_STORY_RESPONSE_SCENE_SPLIT
                         ,GTK_STOCK_CLOSE,  GTK_RESPONSE_CLOSE
                         ,NULL);
    }
    else
    {
      dlg = gap_dialog_new (_("Clip Properties"), "gap_story_clip_properties"
                         ,NULL, 0
                         ,gimp_standard_help_func, GAP_STORY_CLIP_PROP_HELP_ID

                         ,_("Find Scene End"), GAP_STORY_RESPONSE_SCENE_END
                         ,_("Auto Scene Split"), GAP_STORY_RESPONSE_SCENE_SPLIT
                         ,GTK_STOCK_CLOSE,  GTK_RESPONSE_CLOSE
                         ,NULL);
   }
  }

  pw->pw_prop_dialog = dlg;

  if(gap_debug)
  {
    printf("gap_story_pw_properties_dialog before g_signal_connect dlg:%ld\n", (long)dlg);
  }

  g_signal_connect (G_OBJECT (dlg), "response",
                    G_CALLBACK (p_pw_prop_response),
                    pw);


  main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);

  /*  the frame  */
  frame = gimp_frame_new (_("Properties"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /* the dummy table is invisible and contains widgets
   * that are created but never displayed.
   * (the creation is done to avoid null pointer checks in all the callbacks)
   */
  dummy_table = gtk_table_new (17, 4, FALSE);
  
  table = gtk_table_new (17, 4, FALSE);
  pw->master_table = table;
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  row = 0;

  if(pw->is_mask_definition)
  {
    /* the masktype label */
    label = gtk_label_new (_("Mask Type:"));
  }
  else
  {
    /* the cliptype label */
    label = gtk_label_new (_("Clip Type:"));
  }
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);


  {
    GtkWidget *hbox_type;
    hbox_type = gtk_hbox_new (FALSE, 1);
    gtk_widget_show (hbox_type);
    gtk_table_attach_defaults (GTK_TABLE(table), hbox_type, 1, 2, row, row+1);

    /* the cliptype icon (pview widget) */
    gtk_box_pack_start (GTK_BOX (hbox_type)
         , p_pw_create_icontype_pv_container(pw)
         , FALSE, FALSE, 1);

    /* the cliptype label content */
    label = gtk_label_new ("");
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (hbox_type), label, FALSE, FALSE, 1);
    gtk_widget_show (label);
    pw->cliptype_label = label;

    /* dummy spacing label */
    label = gtk_label_new ("  ");
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (hbox_type), label, TRUE, TRUE, 1);
    gtk_widget_show (label);

    /* the duration label */
    label = gtk_label_new (_("Duration:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (hbox_type), label, FALSE, FALSE, 1);
    gtk_widget_show (label);

    /* the frame duration label */
    label = gtk_label_new ("");
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (hbox_type), label, FALSE, FALSE, 1);
    gtk_widget_show (label);
    pw->dur_frames_label = label;

  }
  /* the time duration label  */
  label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 2, 3, row, row+1);
  gtk_widget_show (label);
  pw->dur_time_label = label;


  row++;

  /* the filename label */
  label = gtk_label_new (_("File:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  /* the filename entry */
  entry = gtk_entry_new ();
  gtk_widget_set_size_request(entry, PW_ENTRY_WIDTH, -1);
  if(pw->stb_elem_refptr)
  {
    if(pw->stb_elem_refptr->orig_filename)
    {
      gtk_entry_set_text(GTK_ENTRY(entry), pw->stb_elem_refptr->orig_filename);
    }
  }
  gtk_table_attach_defaults (GTK_TABLE(table), entry, 1, 2, row, row+1);
  g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK(p_pw_filename_entry_update_cb),
                     pw);
  gtk_widget_show (entry);
  pw->pw_filename_entry = entry;

  /* the filesel invoker button */
  button = gtk_button_new_with_label ("...");
  gtk_table_attach_defaults (GTK_TABLE(table), button, 2, 3, row, row+1);
  g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(p_pw_filesel_button_cb),
                     pw);
  gtk_widget_show (button);


  row++;

  l_lower_limit = 0.0;
  if(pw->stb_elem_refptr)
  {
    switch(pw->stb_elem_refptr->record_type)
    {
      case GAP_STBREC_VID_MOVIE:
      case GAP_STBREC_VID_SECTION:
      case GAP_STBREC_VID_BLACKSECTION:
        /* movies and section always start at frame 1 */
        l_lower_limit = 1.0;
        break;
      case GAP_STBREC_VID_SILENCE:
      case GAP_STBREC_VID_COLOR:
      case GAP_STBREC_VID_IMAGE:
      case GAP_STBREC_VID_ANIMIMAGE:
      case GAP_STBREC_VID_FRAMES:
      case GAP_STBREC_VID_COMMENT:
      case GAP_STBREC_VID_UNKNOWN:
      case GAP_STBREC_AUD_SILENCE:
      case GAP_STBREC_AUD_SOUND:
      case GAP_STBREC_AUD_MOVIE:
      case GAP_STBREC_ATT_TRANSITION:
        break;
    }
  }
    
  /* the from spinbutton */
  adj = gimp_scale_entry_new (GTK_TABLE (pw->master_table), 0, row++,
                              _("From:"), PW_SCALE_WIDTH, PW_SPIN_BUTTON_WIDTH,
                              pw->stb_elem_refptr->from_frame,
                              l_lower_limit, GAP_STB_MAX_FRAMENR,  /* lower/upper */
                              1.0, 10.0,      /* step, page */
                              0,              /* digits */
                              TRUE,           /* constrain */
                              1.0, GAP_STB_MAX_FRAMENR,  /* lower/upper unconstrained */
                              _("framenumber of the first frame "
                                "in the clip range"), NULL);
  pw->pw_spinbutton_from_adj = adj;
  g_object_set_data(G_OBJECT(adj), "pw", pw);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_pw_gint32_adjustment_callback),
                    &pw->stb_elem_refptr->from_frame);
  {
    GtkWidget *scale;

    scale = GTK_WIDGET(g_object_get_data (G_OBJECT (pw->pw_spinbutton_from_adj), "scale"));
    if(scale)
    {
      gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
    }
  }


  row++;

  /* the to spinbutton */
  adj = gimp_scale_entry_new (GTK_TABLE (pw->master_table), 0, row++,
                              _("To:"), PW_SCALE_WIDTH, PW_SPIN_BUTTON_WIDTH,
                              pw->stb_elem_refptr->to_frame,
                              l_lower_limit, GAP_STB_MAX_FRAMENR,  /* lower/upper */
                              1.0, 10.0,      /* step, page */
                              0,              /* digits */
                              TRUE,           /* constrain */
                              1.0, GAP_STB_MAX_FRAMENR,  /* lower/upper unconstrained */
                              _("framenumber of the last frame "
                                "in the clip range"), NULL);
  pw->pw_spinbutton_to_adj = adj;
  g_object_set_data(G_OBJECT(adj), "pw", pw);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_pw_gint32_adjustment_callback),
                    &pw->stb_elem_refptr->to_frame);


  pw->pw_spinbutton_loops_adj = NULL;
  if(!pw->is_mask_definition)
  {
    row++;
    table_ptr = table;
  }
  else
  {
    table_ptr = dummy_table;
  }
  /* the loops spinbutton */
  adj = gimp_scale_entry_new (GTK_TABLE (table_ptr), 0, row++,
                            _("Loops:"), PW_SCALE_WIDTH, PW_SPIN_BUTTON_WIDTH,
                            pw->stb_elem_refptr->nloop,
                            1.0, 100000.0,  /* lower/upper */
                            1.0, 10.0,      /* step, page */
                            0,              /* digits */
                            TRUE,           /* constrain */
                            1.0, 1000.0,  /* lower/upper unconstrained */
                            _("number of loops "
                              "(how often to play the framerange)"), NULL);
  pw->pw_spinbutton_loops_adj = adj;
  g_object_set_data(G_OBJECT(adj), "pw", pw);
  g_signal_connect (adj, "value_changed",
                  G_CALLBACK (p_pw_gint32_adjustment_callback),
                  &pw->stb_elem_refptr->nloop);



  if(!pw->is_mask_definition)
  {
    row++;
    table_ptr = table;
  }
  else
  {
    table_ptr = dummy_table;
  }

  /* pingpong */
  label = gtk_label_new(_("Pingpong:"));
  pw->pingpong_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE (table_ptr), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);


  /* the pingpong check button */
  check_button = gtk_check_button_new_with_label (" ");
  gtk_table_attach ( GTK_TABLE (table_ptr), check_button, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);
  {
    gboolean pingpong_state;

    pingpong_state = (pw->stb_elem_refptr->playmode == GAP_STB_PM_PINGPONG);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), pingpong_state);

  }
  gimp_help_set_help_data(check_button, _("ON: Play clip in pingpong mode"), NULL);
  gtk_widget_show(check_button);
  pw->pingpong_toggle = check_button;
  g_signal_connect (G_OBJECT (check_button), "toggled",
                  G_CALLBACK (p_pw_pingpong_toggle_update_callback),
                  pw);



  if(!pw->is_mask_definition)
  {
    row++;
    table_ptr = table;
  }
  else
  {
    table_ptr = dummy_table;
  }
  /* the step_density spinbutton */
  adj = gimp_scale_entry_new (GTK_TABLE (table_ptr), 0, row++,
                            _("Stepsize:"), PW_SCALE_WIDTH, PW_SPIN_BUTTON_WIDTH,
                            pw->stb_elem_refptr->step_density,
                            0.125, 8.0,     /* lower/upper */
                            0.125, 0.5,     /* step, page */
                            6,              /* digits */
                            TRUE,           /* constrain */
                            0.125, 8.0,     /* lower/upper unconstrained */
                            _("Stepsize density. Use 1.0 for normal 1:1 frame by frame steps. "
                              "a value of 0.5 shows each input frame 2 times. "
                              "a value of 2.0 shows only every 2nd input frame"), NULL);
  pw->pw_spinbutton_step_density_adj = adj;
  g_object_set_data(G_OBJECT(adj), "pw", pw);
  g_signal_connect (adj, "value_changed",
                  G_CALLBACK (p_step_density_spinbutton_cb),
                  pw);


  row++;
  /* the seltrack spinbutton */
  adj = gimp_scale_entry_new (GTK_TABLE (pw->master_table), 0, row++,
                              _("Videotrack:"), PW_SCALE_WIDTH, PW_SPIN_BUTTON_WIDTH,
                              pw->stb_elem_refptr->seltrack,
                              1.0, 100.0,  /* lower/upper */
                              1.0, 10.0,      /* step, page */
                              0,              /* digits */
                              TRUE,           /* constrain */
                              1.0, 100.0,  /* lower/upper unconstrained */
                              _("select input videotrack "
                                "(most videofiles have just 1 track)"), NULL);
  pw->pw_spinbutton_seltrack_adj = adj;
  g_object_set_data(G_OBJECT(adj), "pw", pw);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (p_pw_gint32_adjustment_callback),
                    &pw->stb_elem_refptr->seltrack);

  row++;

  /* the Deinterlace Mode label */
  label = gtk_label_new (_("Deinterlace:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  /* delace mode radio button group */
  {
    GSList    *radio_group = NULL;
    GtkWidget *radio_table;
    GtkWidget *radio_button;
    gint      l_idx;
    gboolean  l_radio_pressed;

    /* radio_table */
    radio_table = gtk_table_new (1, 3, FALSE);

    l_idx = 0;
    /* radio button delace_mode None */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("None") );
    radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_delace_mode_radio_button_arr[0] = radio_button;
    l_radio_pressed = (pw->delace_mode == 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Read videoframes 1:1 without de-interlace filter"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_delace_none_callback),
                       pw);

    l_idx++;
    /* radio button delace_mode odd */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("Odd") );
    radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_delace_mode_radio_button_arr[1] = radio_button;
    l_radio_pressed = (pw->delace_mode == 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Apply odd-lines filter when reading videoframes"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_delace_odd_callback),
                       pw);

    l_idx++;
    /* radio button delace_mode even */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("Even") );
    radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_delace_mode_radio_button_arr[2] = radio_button;
    l_radio_pressed = (pw->delace_mode == 2);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Apply even-lines filter when reading videoframes"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_delace_even_callback),
                       pw);





    l_idx++;
    /* radio button delace_mode odd */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("Odd First") );
    radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_delace_mode_radio_button_arr[3] = radio_button;
    l_radio_pressed = (pw->delace_mode == 3);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Apply odd-lines, switch to even lines on tween position >= 0.5"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_delace_odd_first_callback),
                       pw);

    l_idx++;
    /* radio button delace_mode even */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("Even First") );
    radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_delace_mode_radio_button_arr[4] = radio_button;
    l_radio_pressed = (pw->delace_mode == 4);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Apply even-lines, switch to even lines on tween position >= 0.5"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_delace_even_first_callback),
                       pw);

    gtk_widget_show (radio_table);
    gtk_table_attach_defaults (GTK_TABLE(table), radio_table, 1, 2, row, row+1);
  }

  /* delace threshold spinbutton */
  {
    GtkWidget *spinbutton;

    pw->pw_spinbutton_delace_adj = gtk_adjustment_new (pw->delace_threshold
                                           , 0.0
                                           , 0.999
                                           , 0.1, 0.1, 0.0);
    spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (pw->pw_spinbutton_delace_adj), 0.1, 3);
    pw->pw_spinbutton_delace = spinbutton;
    gtk_widget_set_size_request (spinbutton, PW_SPIN_BUTTON_WIDTH, -1);
    gtk_widget_show (spinbutton);
    gtk_table_attach_defaults (GTK_TABLE(table), spinbutton, 2, 3, row, row+1);

    gimp_help_set_help_data (spinbutton, _("deinterlacing threshold: "
                                           "0.0 no interpolation "
                                           "0.999 smooth interpolation")
                                           , NULL);
    g_signal_connect (G_OBJECT (pw->pw_spinbutton_delace_adj), "value_changed",
                      G_CALLBACK (p_delace_spinbutton_cb),
                      pw);
    if(pw->delace_mode == 0)
    {
      gtk_widget_set_sensitive(pw->pw_spinbutton_delace, FALSE);
    }
  }


  row++;

  /* the Transform (flip_request) label */
  label = gtk_label_new (_("Transform:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);


  /* flip_request radio button group */
  {
    GSList    *radio_group = NULL;
    GtkWidget *radio_table;
    GtkWidget *radio_button;
    gint      l_idx;
    gboolean  l_radio_pressed;

    /* radio_table */
    radio_table = gtk_table_new (1, 4, FALSE);

    l_idx = 0;
    /* radio button flip_request None */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("None") );
    
    radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_flip_request_radio_button_arr[GAP_STB_FLIP_NONE] = radio_button;
    l_radio_pressed = (pw->flip_request == GAP_STB_FLIP_NONE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Do not apply internal transformations"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_flip_none_callback),
                       pw);

    l_idx++;
    /* radio button flip_request rotate 180 degree */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("Rotate 180") );
     radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_flip_request_radio_button_arr[GAP_STB_FLIP_BOTH] = radio_button;
    l_radio_pressed = (pw->flip_request == GAP_STB_FLIP_BOTH);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Rotate all frames of this clip by 180 degree"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_flip_both_callback),
                       pw);

    l_idx++;
    /* radio button flip_request hor */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("Flip Horizontally") );
    radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_flip_request_radio_button_arr[GAP_STB_FLIP_HOR] = radio_button;
    l_radio_pressed = (pw->flip_request == GAP_STB_FLIP_HOR);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Flip all frames of this clip horizontally"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_flip_hor_callback),
                       pw);

    l_idx++;
    /* radio button flip_request ver */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("Flip Vertically") );
    radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_flip_request_radio_button_arr[GAP_STB_FLIP_VER] = radio_button;
    l_radio_pressed = (pw->flip_request == GAP_STB_FLIP_VER);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Flip all frames of this clip vertically"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_flip_ver_callback),
                       pw);


    gtk_widget_show (radio_table);
    gtk_table_attach_defaults (GTK_TABLE(table), radio_table, 1, 3, row, row+1);
  }


  row++;

  if(pw->is_mask_definition)
  {
    GtkWidget *button;
    /* the mask_name label */
    button = gtk_button_new_with_label(_("Mask Name:"));
    gimp_help_set_help_data(button, _("Set the mask name"), NULL);
    gtk_table_attach_defaults (GTK_TABLE(table), button, 0, 1, row, row+1);
    gtk_widget_show (button);
    g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(p_pw_mask_name_update_button_pressed_cb),
                   pw);
  }
  else
  {
    /* the mask_name label */
    label = gtk_label_new (_("Mask Name:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
    gtk_widget_show (label);
  }
  
  /* the mask_name entry */
  entry = gtk_entry_new ();
  pw->pw_mask_name_entry = entry;
  gtk_widget_set_size_request(entry, PW_ENTRY_WIDTH, -1);
  if(pw->stb_elem_refptr)
  {
    if(pw->stb_elem_refptr->mask_name)
    {
      gtk_entry_set_text(GTK_ENTRY(entry), pw->stb_elem_refptr->mask_name);
    }
  }
  if(pw->is_mask_definition)
  {
    gimp_help_set_help_data(entry, _("Name of the layermask definition clip"), NULL);

    gtk_table_attach_defaults (GTK_TABLE(table), entry, 1, 2, row, row+1);
    g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK(p_pw_mask_name_entry_update_cb),
                     pw);
    gtk_widget_show (entry);

  }
  else
  {
    GtkWidget *combo;
    gimp_help_set_help_data(entry, _("Reference to a layermask definition clip in the Mask section.\n"
                                     "Layermasks are used to control opacity.")
                                     , NULL);
    gtk_widget_hide (entry);

    combo = gtk_combo_box_new_text();
    pw->mask_name_combo = combo;
    pw->mask_name_combo_elem_count = 0;

    p_pw_set_strings_for_mask_name_combo(pw);

    gtk_table_attach_defaults (GTK_TABLE(table), combo, 1, 2, row, row+1);
    gtk_widget_show (combo);
  
    g_signal_connect (G_OBJECT (combo), "changed",
                    G_CALLBACK (p_pw_mask_name_combo_changed_cb),
                    pw);
  }

  pw->pw_mask_enable_toggle = NULL;

  if(!pw->is_mask_definition)
  {
    table_ptr = table;
  }
  else
  {
    table_ptr = dummy_table;
  }
  /* the mask enable check button */
  check_button = gtk_check_button_new_with_label (_("enable"));
  gtk_table_attach ( GTK_TABLE (table_ptr), check_button, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);
  {
    gboolean enable_state;

    enable_state = (pw->stb_elem_refptr->mask_disable != TRUE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), enable_state);

  }
  gimp_help_set_help_data(check_button, _("ON: Enable layer mask"), NULL);
  gtk_widget_show(check_button);
  pw->pw_mask_enable_toggle = check_button;
  g_signal_connect (G_OBJECT (check_button), "toggled",
                  G_CALLBACK (p_pw_mask_enable_toggle_update_callback),
                  pw);

  /* the mask_definition_name label (only used for mask definition properties) */
  label = gtk_label_new(" ");
  pw->pw_mask_definition_name_label = label;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  if(pw->is_mask_definition)
  {
    if(pw->stb_elem_refptr)
    {
      if(pw->stb_elem_refptr->mask_name)
      {
        gtk_label_set_text ( GTK_LABEL(pw->pw_mask_definition_name_label)
                           , pw->stb_elem_refptr->mask_name);
      }
    }
    gtk_table_attach_defaults (GTK_TABLE(table), label, 2, 4, row, row+1);
    gtk_widget_show (label);
  }


  if(!pw->is_mask_definition)
  {
    row++;
    table_ptr = table;
  }
  else
  {
    table_ptr = dummy_table;
  }

  /* the Mask Anchor Mode label */
  label = gtk_label_new (_("Mask Anchor:"));
  pw->mask_anchor_label = label;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table_ptr), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  /* mask anchor mode radio button group */
  {
    GSList    *radio_group = NULL;
    GtkWidget *radio_table;
    GtkWidget *radio_button;
    gint      l_idx;
    gboolean  l_radio_pressed;

    /* radio_table */
    radio_table = gtk_table_new (1, 2, FALSE);

    l_idx = 0;
    /* radio button mask_anchor Clip */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("Clip") );
    radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_mask_anchor_radio_button_arr[0] = radio_button;
    l_radio_pressed = (pw->mask_anchor == GAP_MSK_ANCHOR_CLIP);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Attach mask to clip at clip position in clip size"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_mask_anchor_clip_callback),
                       pw);

    l_idx++;
    /* radio button mask_anchor Master */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("Master") );
    radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_mask_anchor_radio_button_arr[1] = radio_button;
    l_radio_pressed = (pw->mask_anchor == GAP_MSK_ANCHOR_MASTER);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Attach mask in resulting master video size at fixed position"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_mask_anchor_master_callback),
                       pw);

    l_idx++;
    /* radio button mask_anchor ColormaskClip */
    radio_button = gtk_radio_button_new_with_label ( radio_group, _("ClipColormask") );
    radio_group = gtk_radio_button_get_group ( GTK_RADIO_BUTTON (radio_button) );
    gtk_table_attach ( GTK_TABLE (radio_table), radio_button, l_idx, l_idx+1
                     , 0, 1, 0, 0, 0, 0);

    pw->pw_mask_anchor_radio_button_arr[2] = radio_button;
    l_radio_pressed = (pw->mask_anchor == GAP_MSK_ANCHOR_XCOLOR);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button),
                                     l_radio_pressed);
    gimp_help_set_help_data(radio_button, _("Apply as colormask to clip at clip position in clip size"), NULL);

    gtk_widget_show (radio_button);
    g_signal_connect ( G_OBJECT (radio_button), "toggled",
                       G_CALLBACK (p_radio_mask_anchor_xcolor_callback),
                       pw);



    gtk_widget_show (radio_table);
    gtk_table_attach_defaults (GTK_TABLE(table_ptr), radio_table, 1, 2, row, row+1);
  }


  row++;

  /* the colormask parameter file label */
  label = gtk_label_new (_("Mask Params:"));
  pw->colormask_file_label = label;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);



  /* colormask parameter file entry and filesel invoker button */
  {
    GtkWidget *hbox_colormask_file;
  
    hbox_colormask_file = gtk_hbox_new (FALSE, 1);
    gtk_widget_show (hbox_colormask_file);
    gtk_table_attach_defaults (GTK_TABLE(table), hbox_colormask_file, 1, 2, row, row+1);

    /* the filtermacro entry */
    entry = gtk_entry_new ();
    gtk_widget_set_size_request(entry, PW_ENTRY_WIDTH, -1);
    if(pw->stb_elem_refptr)
    {
      if(pw->stb_elem_refptr->colormask_file)
      {
        gtk_entry_set_text(GTK_ENTRY(entry), pw->stb_elem_refptr->colormask_file);
      }
    }

    gtk_box_pack_start (GTK_BOX (hbox_colormask_file), entry, TRUE, TRUE, 1);

    g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK(p_pw_colormask_file_entry_update_cb),
                     pw);
    gtk_widget_show (entry);
    gimp_help_set_help_data (entry, _("parameter file for the colormask filter")
                            , NULL);
    pw->colormask_file_entry = entry;


    /* the filesel invoker button */
    button = gtk_button_new_with_label ("...");
    pw->colormask_file_filesel_button = button;
    gtk_box_pack_start (GTK_BOX (hbox_colormask_file), button, FALSE, FALSE, 1);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(p_pw_colormask_file_filesel_button_cb),
                     pw);
    gtk_widget_show (button);
  }

  if(!pw->is_mask_definition)
  {
    row++;
    table_ptr = table;
  }
  else
  {
    table_ptr = dummy_table;
  }
  /* the maskstep_density spinbutton */
  adj = gimp_scale_entry_new (GTK_TABLE (table_ptr), 0, row++,
                            _("Maskstepsize:"), PW_SCALE_WIDTH, PW_SPIN_BUTTON_WIDTH,
                            pw->stb_elem_refptr->mask_stepsize,
                            0.125, 8.0,     /* lower/upper */
                            0.125, 0.5,     /* step, page */
                            6,              /* digits */
                            TRUE,           /* constrain */
                            0.125, 8.0,     /* lower/upper unconstrained */
                            _("Stepsize density for the layer mask. Use 1.0 for normal 1:1 frame by frame steps. "
                              "a value of 0.5 shows each input mask frame 2 times. "
                              "a value of 2.0 shows only every 2nd input mask frame"), NULL);
  pw->pw_spinbutton_mask_stepsize_adj = adj;
  g_object_set_data(G_OBJECT(adj), "pw", pw);
  g_signal_connect (adj, "value_changed",
                  G_CALLBACK (p_maskstep_density_spinbutton_cb),
                  pw);




  /* the vbox for displaying the previews
   * of clips and layermask references.
   */
  vbox_pviews = gtk_vbox_new (FALSE, 1);
  gtk_widget_show(vbox_pviews);

  gtk_box_pack_start (GTK_BOX (vbox_pviews)
     , p_pw_create_clip_pv_container(pw)
     , FALSE, FALSE, 1);
  gtk_box_pack_start (GTK_BOX (vbox_pviews)
     , p_pw_create_mask_pv_container(pw)
     , FALSE, FALSE, 1);

  gtk_table_attach_defaults (GTK_TABLE(table), vbox_pviews, 3, 4, 0, row+1);

  row++;

  /* the filtermacro label */
  label = gtk_label_new (_("Filtermacro:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);



  /* filtermacro entry and filesel invoker button */
  {
    GtkWidget *hbox_fmac;
  
    hbox_fmac = gtk_hbox_new (FALSE, 1);
    gtk_widget_show (hbox_fmac);
    gtk_table_attach_defaults (GTK_TABLE(table), hbox_fmac, 1, 2, row, row+1);

    /* the filtermacro entry */
    entry = gtk_entry_new ();
    gtk_widget_set_size_request(entry, PW_ENTRY_WIDTH, -1);
    if(pw->stb_elem_refptr)
    {
      if(pw->stb_elem_refptr->filtermacro_file)
      {
        gtk_entry_set_text(GTK_ENTRY(entry), pw->stb_elem_refptr->filtermacro_file);
      }
    }

    gtk_box_pack_start (GTK_BOX (hbox_fmac), entry, TRUE, TRUE, 1);

    g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK(p_pw_fmac_entry_update_cb),
                     pw);
    gtk_widget_show (entry);
    gimp_help_set_help_data (entry, _("filter macro to be performed when frames "
                                    "of this clips are rendered. "
                                    "A 2nd macrofile is implicitly referenced by naming convention "
                                    "via the keyword .VARYING (as suffix or before the extension)")
                                           , NULL);
    pw->fmac_entry = entry;


    /* the filesel invoker button */
    button = gtk_button_new_with_label ("...");
    gtk_box_pack_start (GTK_BOX (hbox_fmac), button, FALSE, FALSE, 1);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(p_pw_fmac_filesel_button_cb),
                     pw);
    gtk_widget_show (button);
  }


  /* fmac_total_steps threshold spinbutton */
  {
    GtkWidget *spinbutton;
    gint32   initial_value;
    
    initial_value = 1;
    if(pw->stb_elem_refptr)
    {
      initial_value = pw->stb_elem_refptr->fmac_total_steps;
    }

    pw->pw_spinbutton_fmac_steps_adj = gtk_adjustment_new (initial_value
                                           , 1.0
                                           , 999999.0
                                           , 1.0, 10.0, 0.0);
    spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (pw->pw_spinbutton_fmac_steps_adj), 1.0, 0);
    pw->pw_spinbutton_fmac_steps = spinbutton;
    gtk_widget_set_size_request (spinbutton, PW_SPIN_BUTTON_WIDTH, -1);
    gtk_widget_show (spinbutton);
    gtk_table_attach_defaults (GTK_TABLE(table), spinbutton, 2, 3, row, row+1);

    gimp_help_set_help_data (spinbutton, _("Steps for macro applying with varying values: "
                                           "(1 for apply with const values)")
                                           , NULL);
    g_signal_connect (G_OBJECT (pw->pw_spinbutton_fmac_steps_adj), "value_changed",
                      G_CALLBACK (p_fmac_steps_spinbutton_cb),
                      pw);
  }


  row++;




  /* 2nd row alternative filtermacro (VARYING) */
  {
    GtkWidget       *hbox_fmac;
    GtkWidget       *spinbutton;
    GapAccelWidget  *accel_wgt;
    gint32           accelerationCharacteristic;

#define ACC_WGT_WIDTH 32
#define ACC_WGT_HEIGHT 22

    hbox_fmac = gtk_hbox_new (FALSE, 1);
    gtk_widget_show (hbox_fmac);
    gtk_table_attach_defaults (GTK_TABLE(table), hbox_fmac, 1, 2, row, row+1);

    pw->pw_hbox_fmac2 = hbox_fmac;

    /* the alternate filtermacro 2 label (prefix) */
    label = gtk_label_new ("");
    pw->pw_label_alternate_fmac2 = label;
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
    gtk_widget_show (label);



    /* the alternate filtermacro_file label */
    label = gtk_label_new ("");
    pw->pw_label_alternate_fmac_file = label;
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_widget_set_size_request(label, PW_ENTRY_WIDTH, -1);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox_fmac), label, TRUE, TRUE, 1);

    
    /* the Acceleration characteristic value spinbutton */
    accelerationCharacteristic = 0;
    if(pw->stb_elem_refptr)
    {
      accelerationCharacteristic = pw->stb_elem_refptr->fmac_accel;
    }

    /* the acceleration characteristic graph display widget */
    accel_wgt = gap_accel_new(ACC_WGT_WIDTH, ACC_WGT_HEIGHT, accelerationCharacteristic);
    
    adj = accel_wgt->adj;
    spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
    pw->pw_spinbutton_fmac_accel_adj = adj;
    pw->pw_spinbutton_fmac_accel = spinbutton;

    gtk_widget_show (spinbutton);
    gtk_table_attach (GTK_TABLE (table), spinbutton, 2, 3, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
    gtk_widget_set_size_request (spinbutton, PW_SPIN_BUTTON_WIDTH, -1);
    gimp_help_set_help_data (spinbutton, _("acceleration characteristic for filtermacro 0=off positive accelerate, negative decelerate"), NULL);

    g_object_set_data(G_OBJECT(adj), "pw", pw);
    
    
    gtk_box_pack_start (GTK_BOX (hbox_fmac), accel_wgt->da_widget, FALSE, FALSE, 1);
    gtk_widget_show (accel_wgt->da_widget);
  
    g_signal_connect (G_OBJECT (pw->pw_spinbutton_fmac_accel_adj), "value_changed",
                      G_CALLBACK (p_fmac_accel_spinbutton_cb),
                      pw);

  }

  
  p_pw_check_fmac_sensitivity(pw);
  

  row++;


  /* the comment label */
  label = gtk_label_new (_("Comment:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);


  /* the comment entry */
  entry = gtk_entry_new ();
  gtk_widget_set_size_request(entry, PW_COMMENT_WIDTH, -1);
  if(pw->stb_elem_refptr)
  {
    if(pw->stb_elem_refptr->comment)
    {
      if(pw->stb_elem_refptr->comment->orig_src_line)
      {
        gtk_entry_set_text(GTK_ENTRY(entry), pw->stb_elem_refptr->comment->orig_src_line);
      }
    }
  }
  gtk_table_attach_defaults (GTK_TABLE(table), entry, 1, 4, row, row+1);
  g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK(p_pw_comment_entry_update_cb),
                     pw);
  gtk_widget_show (entry);
  pw->comment_entry = entry;


  if(pw->stb_elem_refptr)
  {
    if((pw->stb_elem_refptr->record_type == GAP_STBREC_VID_FRAMES)
    || (pw->stb_elem_refptr->record_type == GAP_STBREC_VID_ANIMIMAGE)
    || (pw->stb_elem_refptr->record_type == GAP_STBREC_VID_SECTION)
    || (pw->stb_elem_refptr->record_type == GAP_STBREC_VID_MOVIE))
    {
      /* for framerange clips constraint from / to spinbuttons */
      p_pw_check_ainfo_range(pw, pw->stb_elem_refptr->orig_filename);
    }
  }

  if(!pw->is_mask_definition)
  {
    if(gap_debug)
    {
      printf("gap_story_pw_properties_dialog before p_pw_set_strings_for_mask_name_combo\n");
    }
    p_pw_set_strings_for_mask_name_combo(pw);
  }


  /*  Show the main containers  */
  if(gap_debug)
  {
    printf("gap_story_pw_properties_dialog before gtk_widget_show(main_vbox)\n");
  }

  gtk_widget_show(main_vbox);

  if(gap_debug)
  {
    printf("gap_story_pw_properties_dialog DONE return dlg:%ld\n", (long)dlg);
  }

  return(dlg);
}  /* end gap_story_pw_properties_dialog */

/* -------------------------------------
 * gap_story_stb_elem_properties_dialog
 * -------------------------------------
 */
void
gap_story_stb_elem_properties_dialog ( GapStbTabWidgets *tabw
                                     , GapStoryElem *stb_elem
                                     , GapStoryBoard *stb_dst)
{
  GapStbPropWidget *pw;
  gint idx;

  if(gap_debug)
  {
    printf("gap_story_stb_elem_properties_dialog START\n");
  }

  /* check if already open */
  for(pw=tabw->pw; pw!=NULL; pw=(GapStbPropWidget *)pw->next)
  {
    if ((pw->pw_prop_dialog == NULL) || (pw->stb_elem_refptr == NULL))
    {
      /* we found a dead element (that is already closed)
       * reuse that element to open a new clip properties dialog window
       */
      break;
    }
    else
    {
      if(pw->stb_elem_refptr->story_id == stb_elem->story_id)
      {
        if(gap_debug)
        {
          printf("gap_story_stb_elem_properties_dialog calling gtk_window_present (window to front)\n");
        }
        /* Properties for the selected element already open
         * bring the window to front
         */
        gtk_window_present(GTK_WINDOW(pw->pw_prop_dialog));
        return ;
      }
    }
  }

  if(pw == NULL)
  {
    if(gap_debug)
    {
      printf("gap_story_stb_elem_properties_dialog pw is NULL calling g_new0(GapStbPropWidget ,1)\n");
    }
    pw = g_new0(GapStbPropWidget ,1);
    pw->stb_elem_bck = NULL;
    pw->next = tabw->pw;
    tabw->pw = pw;
  }
  if(pw->stb_elem_bck)
  {
    if(gap_debug)
    {
      printf("gap_story_stb_elem_properties_dialog pw->stb_elem_bck is NOT NULL, calling gap_story_elem_free\n");
    }
    gap_story_elem_free(&pw->stb_elem_bck);
  }
  pw->stb_elem_bck = NULL;
  pw->pw_prop_dialog = NULL;
  pw->pw_filesel = NULL;


  pw->stb_elem_refptr = stb_elem;
  pw->stb_refptr = stb_dst;
  pw->sgpp = tabw->sgpp;
  pw->tabw = tabw;
  pw->go_render_all_request = FALSE;
  pw->go_recreate_request = FALSE;
  pw->go_job_framenr = -1;
  pw->go_timertag = -1;
  pw->scene_detection_busy = FALSE;
  pw->close_flag = FALSE;
  pw->delace_mode = stb_elem->delace;
  pw->flip_request = stb_elem->flip_request;
  pw->delace_threshold = stb_elem->delace - (gdouble)pw->delace_mode;
  pw->mask_anchor = stb_elem->mask_anchor;
  
  for(idx=0; idx < GAP_MAX_DELACE_MODES; idx++)
  {
    pw->pw_delace_mode_radio_button_arr[idx] = NULL;
  }
  for(idx=0; idx < GAP_MAX_FLIP_REQUEST; idx++)
  {
    pw->pw_flip_request_radio_button_arr[idx] = NULL;
  }
  for(idx=0; idx < GAP_STB_MAX_MASK_ANCHOR_MODES; idx++)
  {
    pw->pw_mask_anchor_radio_button_arr[idx] = NULL;
  }

  if(stb_elem)
  {
    if((stb_elem->orig_filename)
    || (stb_elem->basename))
    {
      pw->stb_elem_bck = gap_story_elem_duplicate(stb_elem);
      if(stb_elem->comment)
      {
        if(stb_elem->comment->orig_src_line)
        {
          pw->stb_elem_bck->comment = gap_story_elem_duplicate(stb_elem->comment);
        }
      }
    }
  }


  pw->pw_prop_dialog = gap_story_pw_properties_dialog(pw);
  if(gap_debug)
  {
    printf("gap_story_stb_elem_properties_dialog pw->pw_prop_dialog:%ld\n"
          ,(long)pw->pw_prop_dialog
          );
  }
  if(pw->pw_prop_dialog)
  {
    gtk_widget_show(pw->pw_prop_dialog);
    p_pw_update_info_labels_and_cliptype_senstivity(pw);
    p_pv_pview_render(pw);
    p_pw_dialog_init_dnd(pw);
  }
  if(gap_debug)
  {
    printf("gap_story_stb_elem_properties_dialog DONE\n");
  }
}  /* end gap_story_stb_elem_properties_dialog */

/* -------------------------------
 * gap_story_fw_properties_dialog
 * -------------------------------
 */
void
gap_story_fw_properties_dialog (GapStbFrameWidget *fw)
{
  GapStbTabWidgets *tabw;

  if(fw == NULL) { return; }
  if(fw->stb_elem_refptr == NULL)  { return; }

  tabw = (GapStbTabWidgets *)fw->tabw;
  if(tabw == NULL)  { return; }


  /* type check, this dialog handles only stb_elements that are video clips
   * (both standard video clips, and video clips that act as mask definitions)
   */
  if(gap_story_elem_is_video(fw->stb_elem_refptr))
  {
    gap_story_stb_elem_properties_dialog(tabw, fw->stb_elem_refptr, fw->stb_refptr);
  }
}  /* end gap_story_fw_properties_dialog */
