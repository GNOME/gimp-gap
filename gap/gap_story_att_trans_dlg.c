/*  gap_story_att_trans_dlg.c
 *
 *  This module handles GAP storyboard dialog transition attribute properties window
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
 * version 2.3.0-238; 2006/06/18  hof: support overlapping frames within a video track
 * version 2.2.1-214; 2006/03/31  hof: created
 */


#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_base.h"
#include "gap_story_main.h"
#include "gap_story_undo.h"
#include "gap_story_dialog.h"
#include "gap_story_file.h"
#include "gap_story_render_processor.h"
#include "gap_story_att_trans_dlg.h"
#include "gap_pview_da.h"
#include "gap_stock.h"
#include "gap_lib.h"
#include "gap_vin.h"
#include "gap_timeconv.h"
#include "gap_layer_copy.h"
#include "gap_accel_da.h"
#include "gap_mov_dialog.h"
#include "gap_mov_exec.h"


#include "gap-intl.h"


#define ATTW_COMMENT_WIDTH      480
#define CONVERT_TO_100PERCENT   100.0

#define GAP_STORY_ATTR_PROP_HELP_ID  "plug-in-gap-storyboard-attr-prop"
#define GAP_STORY_ATT_RESPONSE_RESET 1
#define GAP_STORY_ATT_RESPONSE_PLAYBACK 2

#define PVIEW_SIZE 256
#define LAYERNAME_ORIG "orig_layer"
#define LAYERNAME_OPRE "opre_layer"

#define LAYERNAME_DECO "deco_layer"
#define LAYERNAME_CURR "curr_layer"
#define LAYERNAME_PREF "pref_layer"
#define LAYERNAME_BASE "base_layer"

#define LAYERSTACK_TOP  -1
#define LAYERSTACK_CURR 2
#define LAYERSTACK_PREF 1
#define LAYERSTACK_BASE 0

#define OBJ_DATA_KEY_ATTW     "attw"
#define OBJ_DATA_KEY_IMG_IDX  "img_idx"

#define PVIEW_TO_MASTER_SCALE  0.5

extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

static gdouble  p_getConvertFactor(glong att_type_idx);
static void     p_attw_prop_response(GtkWidget *widget
                  , gint       response_id
                  , GapStbAttrWidget *attw
                  );

static void     p_attw_push_undo_and_set_unsaved_changes(GapStbAttrWidget *attw);

static void     p_attw_prop_reset_all(GapStbAttrWidget *attw);
static void     p_playback_effect_range(GapStbAttrWidget *attw);
static void     p_attw_timer_job(GapStbAttrWidget *attw);
static void     p_attw_update_properties(GapStbAttrWidget *attw);
static void     p_attw_update_sensitivity(GapStbAttrWidget *attw);
static gdouble  p_get_default_attribute(GapStbAttrWidget *attw
                     , GdkEventButton  *bevent
                     , glong att_type_idx
                     , gboolean form_value);

static void     p_attw_start_button_clicked_callback(GtkWidget *widget
                  , GdkEventButton  *bevent
                  , glong att_type_idx);
static void     p_attw_end_button_clicked_callback(GtkWidget *widget
                  , GdkEventButton  *bevent
                  , glong att_type_idx);

static void     p_copy_duration_to_all(gint32 duration, GapStbAttrWidget *attw);
static void     p_attw_overlap_dur_button_clicked_callback(GtkWidget *widget
                  , GdkEventButton  *bevent
                  , GapStbAttrWidget *attw);
static void     p_attw_dur_button_clicked_callback(GtkWidget *widget
                  , GdkEventButton  *bevent
                  , glong att_type_idx);
static void     p_attw_gdouble_adjustment_callback(GtkObject *obj, gdouble *val);
static void     p_duration_dependent_refresh(GapStbAttrWidget *attw);
static void     p_attw_duration_adjustment_callback(GtkObject *obj, gint32 *val);
static void     p_attw_accel_adjustment_callback(GtkObject *obj, gint32 *val);
static void     p_attw_enable_toggle_update_callback(GtkWidget *widget, gboolean *val);

static void     p_attw_auto_update_toggle_update_callback(GtkWidget *widget, gboolean *val);

static gboolean p_attw_preview_events_cb (GtkWidget *widget
                       , GdkEvent  *event
                       , GapStbAttrWidget *attw);
static void     p_calculate_prefetch_render_attributes(GapStbAttrWidget *attw
                       , glong img_idx
                       , GapStoryCalcAttr  *calc_attr_ptr
                       );
static void     p_calculate_render_attributes(GapStbAttrWidget *attw
                       , glong img_idx
                       , GapStoryCalcAttr  *calc_attr
                       );
static void     p_check_and_make_opre_default_layer(GapStbAttrWidget *attw, glong img_idx);
static void     p_check_and_make_orig_default_layer(GapStbAttrWidget *attw, glong img_idx);
static gint32   p_create_color_layer(GapStbAttrWidget *attw, gint32 image_id
                    , const char *name, gint stackposition, gdouble opacity
                    , gdouble red, gdouble green, gdouble blue);
static gint32   p_create_base_layer(GapStbAttrWidget *attw, gint32 image_id);
static gint32   p_create_deco_layer(GapStbAttrWidget *attw, gint32 image_id);
static void     p_create_gfx_image(GapStbAttrWidget *attw, glong img_idx);
static void     p_delete_gfx_images(GapStbAttrWidget *attw);
static void     p_adjust_stackposition(gint32 image_id, gint32 layer_id, gint position);


static gboolean p_create_transformed_layer_movepath(gint32 image_id
                                                  , gint32 origsize_layer_id
                                                  , gint32 *layer_id_ptr
                                                  , GapStoryCalcAttr  *calculated
                                                  , gint32 stackposition, const char *layername
                                                  , GapStbAttrWidget *attw
                                                  , glong img_idx
                                                  );

static void     p_create_transformed_layer(gint32 image_id
                    , gint32 origsize_layer_id
                    , gint32 *layer_id_ptr
                    , GapStoryCalcAttr  *calculated
                    , gint32 stackposition
                    , const char *layername
                    , GapStbAttrWidget *attw
                    , glong img_idx
                    , gboolean enableMovepath
                    );
static gboolean p_calculate_prefetch_visibility(GapStbAttrWidget *attw, glong img_idx);

static void     p_render_gfx(GapStbAttrWidget *attw, glong img_idx);

static void     p_update_framenr_labels(GapStbAttrWidget *attw, glong img_idx, gint32 framenr);
static gint32   p_get_relevant_duration(GapStbAttrWidget *attw, glong img_idx);
static void     p_update_full_preview_gfx(GapStbAttrWidget *attw);

static gboolean p_stb_req_equals_layer_info(GapStbAttrLayerInfo *linfo
                         , GapStoryLocateRet *stb_ret
                         , const char *filename);
static void     p_destroy_orig_layer( gint32 image_id
                    , gint32 *orig_layer_id_ptr
                    , gint32 *derived_layer_id_ptr
                    , GapStbAttrLayerInfo *linfo);
static gboolean p_check_orig_layer_up_to_date(gint32 image_id
                    , gint32 *orig_layer_id_ptr
                    , gint32 *derived_layer_id_ptr
                    , GapStbAttrLayerInfo *linfo
                    , GapStoryLocateRet *stb_ret
                    , const char *filename);
static void     p_orig_layer_frame_fetcher(GapStbAttrWidget *attw
                    , gint32 master_framenr
                    , gint32 image_id
                    , gint32 *orig_layer_id_ptr
                    , gint32 *derived_layer_id_ptr
                    , GapStbAttrLayerInfo *linfo);

static gint32   p_calc_and_set_display_framenr(GapStbAttrWidget *attw
                    , long   img_idx
                    , gint32 duration);

static void     p_create_or_replace_orig_and_opre_layer (GapStbAttrWidget *attw
                         , long   img_idx
                         , gint32 duration);
static gint32   p_fetch_video_frame_as_layer(GapStbMainGlobalParams *sgpp
                   , const char *video_filename
                   , gint32 framenumber
                   , gint32 seltrack
                   , const char *preferred_decoder
                   , gint32 image_id
                   );

static gint32   p_fetch_imagefile_as_layer (const char *img_filename
                   , gint32 image_id
                   );
static gint32   p_fetch_layer_from_animimage (const char *img_filename
                   , gint32 localframe_index, gint32 image_id
                   );

static void     p_attw_movepath_filesel_pw_ok_cb (GtkWidget *widget
                   ,GapStbAttrWidget *attw);
static void     p_attw_movepath_filesel_pw_close_cb ( GtkWidget *widget
                      , GapStbAttrWidget *attw);
static void     p_attw_movepath_filesel_button_cb ( GtkWidget *w
                       , GapStbAttrWidget *attw);

static void     p_create_movepath_edit_resources(GapStbAttrWidget *attw);
static void     p_edit_movepath_closed_callback(gpointer ptr);
static void     p_attw_movepath_edit_button_cb ( GtkWidget *w
                       , GapStbAttrWidget *attw);
static void     p_attw_movepath_file_validity_check(GapStbAttrWidget *attw);
static void     p_attw_movepath_file_entry_update_cb(GtkWidget *widget, GapStbAttrWidget *attw);
static void     p_attw_comment_entry_update_cb(GtkWidget *widget, GapStbAttrWidget *attw);

static void     p_init_layer_info(GapStbAttrLayerInfo *linfo);

static void     p_create_and_attach_pv_widgets(GapStbAttrWidget *attw
                 , GtkWidget *table
                 , gint row
                 , gint col_start
                 , gint col_end
                 , glong img_idx
                 );
static void     p_create_and_attach_att_arr_widgets(const char *row_title
                 , GapStbAttrWidget *attw
                 , GtkWidget *table
                 , gint row
                 , gint column
                 , glong att_type_idx
                 , gdouble lower_constraint
                 , gdouble upper_constraint
                 , gdouble step_increment
                 , gdouble page_increment
                 , gdouble page_size
                 , gint    digits
                 , const char *enable_tooltip
                 , gboolean   *att_arr_enable_ptr
                 , const char *from_tooltip
                 , gdouble    *att_arr_value_from_ptr
                 , const char *to_tooltip
                 , gdouble    *att_arr_value_to_ptr
                 , const char *dur_tooltip
                 , gint32     *att_arr_value_dur_ptr
                 , const char *accel_tooltip
                 , gint32     *att_arr_value_accel_ptr
                 );


static gdouble
p_getConvertFactor(glong att_type_idx)
{
  if ((att_type_idx == GAP_STB_ATT_TYPE_ROTATE)
  ||  (att_type_idx == GAP_STB_ATT_TYPE_MOVEPATH))
  {
    return 1.0;
  }
  return (CONVERT_TO_100PERCENT);
}

/* ----------------------------------------------------
 * p_debug_dup_image
 * ----------------------------------------------------
 * Duplicate image, and open a display for the duplicate
 * (Procedure is used for debug only
 */
static void
p_debug_dup_image(gint32 image_id)
{
  gint32 l_dup_image_id;

  l_dup_image_id = gimp_image_duplicate(image_id);
  gimp_display_new(l_dup_image_id);
}  /* end p_debug_dup_image */


/* ---------------------------------
 * p_attw_prop_response
 * ---------------------------------
 */
static void
p_attw_prop_response(GtkWidget *widget
                  , gint       response_id
                  , GapStbAttrWidget *attw
                  )
{
  GtkWidget *dlg;
  /* GapStbMainGlobalParams  *sgpp; */

  /* sgpp = attw->sgpp; */
  switch (response_id)
  {
    case GAP_STORY_ATT_RESPONSE_RESET:
      if((attw->stb_elem_bck)
      && (attw->stb_elem_refptr))
      {
        p_attw_prop_reset_all(attw);
      }
      break;
    case GAP_STORY_ATT_RESPONSE_PLAYBACK:
      p_playback_effect_range(attw);
      break;
    case GTK_RESPONSE_CLOSE:
    default:
      dlg = attw->attw_prop_dialog;
      if(dlg)
      {
        if(attw->movepath_filesel != NULL)
        {
          /* force close in case file selection dialog is still open */
          p_attw_movepath_filesel_pw_close_cb(attw->movepath_filesel, attw);
        }

        if(attw->movepath_edit_dialog != NULL)
        {
          /* force close of the movepath edit dialog that is still open */
          gtk_widget_destroy(attw->movepath_edit_dialog);
          attw->movepath_edit_dialog = NULL;
        }

        p_delete_gfx_images(attw);
        if(attw->go_timertag >= 0)
        {
          g_source_remove(attw->go_timertag);
        }
        attw->go_timertag = -1;
        attw->attw_prop_dialog = NULL;
        attw->stb_elem_refptr = NULL;
        attw->stb_refptr = NULL;
        gtk_widget_destroy(dlg);
      }
      break;
  }
}  /* end p_attw_prop_response */


/* --------------------------------------
 * p_attw_push_undo_and_set_unsaved_changes
 * --------------------------------------
 * this procedure is called in most cases when
 * a transition attribute property has changed.
 * (note that the Undo push implementation filters out
 * multiple attribute property changes on the same object in sequence)
 */
static void
p_attw_push_undo_and_set_unsaved_changes(GapStbAttrWidget *attw)
{
  if(attw != NULL)
  {
    if((attw->stb_elem_refptr != NULL) && (attw->stb_refptr != NULL))
    {
      gap_stb_undo_push_clip_with_file_snapshot(attw->tabw
          , GAP_STB_FEATURE_PROPERTIES_TRANSITION
          , attw->stb_elem_refptr->story_id
          , &attw->stb_elem_refptr->att_movepath_file_xml
          );

      attw->stb_refptr->unsaved_changes = TRUE;
      if(attw->stb_refptr->active_section != NULL)
      {
        attw->stb_refptr->active_section->version++;
      }

    }
  }
}  /* end p_attw_push_undo_and_set_unsaved_changes */


/* ---------------------------------
 * p_attw_prop_reset_all
 * ---------------------------------
 */
static void
p_attw_prop_reset_all(GapStbAttrWidget *attw)
{
  gboolean comment_set;
  gint ii;

  if(attw)
  {
    if(attw->stb_elem_refptr)
    {
      p_attw_push_undo_and_set_unsaved_changes(attw);

      gap_story_elem_copy(attw->stb_elem_refptr, attw->stb_elem_bck);

      if(attw->stb_elem_refptr->att_movepath_file_xml)
      {
        gtk_entry_set_text(GTK_ENTRY(attw->movepath_file_entry), attw->stb_elem_refptr->att_movepath_file_xml);
      }
      else
      {
        gtk_entry_set_text(GTK_ENTRY(attw->movepath_file_entry), "");
      }


      comment_set = FALSE;
      if(attw->stb_elem_refptr->comment)
      {
        if(attw->stb_elem_refptr->comment->orig_src_line)
        {
          gtk_entry_set_text(GTK_ENTRY(attw->comment_entry), attw->stb_elem_refptr->comment->orig_src_line);
          comment_set = TRUE;
        }
      }
      if(!comment_set)
      {
          gtk_entry_set_text(GTK_ENTRY(attw->comment_entry), "");
      }

      for(ii=0; ii < GAP_STB_ATT_TYPES_ARRAY_MAX; ii++)
      {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (attw->att_rows[ii].enable_toggle)
                                , attw->stb_elem_refptr->att_arr_enable[ii]);

        gtk_adjustment_set_value(GTK_ADJUSTMENT(attw->att_rows[ii].spinbutton_from_adj)
                                , attw->stb_elem_refptr->att_arr_value_from[ii] *
                                p_getConvertFactor(ii));
        gtk_adjustment_set_value(GTK_ADJUSTMENT(attw->att_rows[ii].spinbutton_to_adj)
                                , attw->stb_elem_refptr->att_arr_value_to[ii] *
                                p_getConvertFactor(ii));
        gtk_adjustment_set_value(GTK_ADJUSTMENT(attw->att_rows[ii].spinbutton_dur_adj)
                                , attw->stb_elem_refptr->att_arr_value_dur[ii]);

        gtk_adjustment_set_value(GTK_ADJUSTMENT(attw->att_rows[ii].spinbutton_accel_adj)
                                , attw->stb_elem_refptr->att_arr_value_accel[ii]);
      }
      gtk_adjustment_set_value(GTK_ADJUSTMENT(attw->spinbutton_overlap_dur_adj)
                                , attw->stb_elem_refptr->att_overlap);


      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (attw->fit_width_toggle)
             , attw->stb_elem_refptr->att_fit_width);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (attw->fit_height_toggle)
             , attw->stb_elem_refptr->att_fit_height);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (attw->keep_proportions_toggle)
             , attw->stb_elem_refptr->att_keep_proportions);

      attw->timer_full_update_request = TRUE;
      p_attw_update_properties(attw);
    }
  }

}  /* end p_attw_prop_reset_all */


/* ------------------------------
 * p_playback_effect_range
 * ------------------------------
 */
static void
p_playback_effect_range(GapStbAttrWidget *attw)
{
  gint32 begin_frame;
  gint32 end_frame;

  begin_frame = p_calc_and_set_display_framenr(attw, 0, 0);
  end_frame = begin_frame + p_get_relevant_duration(attw, 1);

  gap_story_attw_range_playback(attw, begin_frame, end_frame);

}  /* end p_playback_effect_range */

/* ------------------
 * p_attw_timer_job
 * ------------------
 * render both graphical previews
 * or do full update if timer_full_update_request flag is set.
 * (also reset this flag)
 */
static void
p_attw_timer_job(GapStbAttrWidget *attw)
{
  GapStbMainGlobalParams  *sgpp;

  if(gap_debug)
  {
    printf("\np_attw_timer_job: START\n");
  }
  sgpp = attw->sgpp;

  if((attw)
  && (sgpp))
  {

    if(attw->go_timertag >= 0)
    {
      g_source_remove(attw->go_timertag);
    }
    attw->go_timertag = -1;

    if(attw->timer_full_update_request)
    {
      attw->timer_full_update_request = FALSE;
      p_update_full_preview_gfx(attw);

      /* it would be sufficient to update the labels
       * of all clips displayed in the storyboard,
       * but we simply render everything
       * (the code is already complex enough)
       */
      gap_story_dlg_attw_render_all(attw);
    }
    else
    {
      p_render_gfx(attw, 0);
      p_render_gfx(attw, 1);
    }

  }
}  /* end p_attw_timer_job */


/* ------------------------------------
 * p_attw_update_properties
 * ------------------------------------
 * render graphical view respecting all enabled attributes
 * rotate, opacity, move X/Y and Zoom X/Y
 */
static void
p_attw_update_properties(GapStbAttrWidget *attw)
{
  gap_story_dlg_tabw_update_frame_label(attw->tabw, attw->sgpp);

  if(attw->go_timertag < 0)
  {
    gint32 delay_millisec;

    delay_millisec = 100;
    attw->go_timertag = (gint32) g_timeout_add(delay_millisec
                                             , (GtkFunction)p_attw_timer_job
                                             , attw
                                             );
  }

}  /* end p_attw_update_properties */

/* ------------------------------------
 * p_attw_update_sensitivity
 * ------------------------------------
 * set sensitivity of all from/to, dur attribute widgets
 * according to their enable flag.
 * the row with widget for the movepath settings depends
 * on the movepath_file_xml_is_valid flag
 */
static void
p_attw_update_sensitivity(GapStbAttrWidget *attw)
{
  gint ii;
  gboolean sensitive;

  if(attw == NULL)
  {
    return;
  }
  if(attw->stb_elem_refptr == NULL)
  {
    return;
  }

  for(ii=0; ii < GAP_STB_ATT_TYPES_ARRAY_MAX; ii++)
  {
    sensitive = attw->stb_elem_refptr->att_arr_enable[ii];
    if (ii == GAP_STB_ATT_TYPE_MOVEPATH)
    {
      gtk_widget_set_sensitive(attw->att_rows[ii].enable_toggle, attw->movepath_file_xml_is_valid);
      if(attw->movepath_file_xml_is_valid != TRUE)
      {
        sensitive = FALSE;
      }
    }

    gtk_widget_set_sensitive(attw->att_rows[ii].spinbutton_from, sensitive);
    gtk_widget_set_sensitive(attw->att_rows[ii].spinbutton_to, sensitive);
    gtk_widget_set_sensitive(attw->att_rows[ii].spinbutton_dur, sensitive);
    gtk_widget_set_sensitive(attw->att_rows[ii].button_from, sensitive);
    gtk_widget_set_sensitive(attw->att_rows[ii].button_to, sensitive);
    gtk_widget_set_sensitive(attw->att_rows[ii].button_dur, sensitive);
    gtk_widget_set_sensitive(attw->att_rows[ii].spinbutton_accel, sensitive);

  }

  sensitive = ((attw->stb_elem_refptr->att_fit_width == TRUE)
            || (attw->stb_elem_refptr->att_fit_height == TRUE));
  gtk_widget_set_sensitive(attw->keep_proportions_toggle, sensitive);

  sensitive = FALSE;
  if(attw->stb_elem_refptr->att_movepath_file_xml != NULL)
  {
    if(attw->stb_elem_refptr->att_movepath_file_xml[0] != '\0')
    {
      sensitive = TRUE;
    }
  }

  gtk_widget_set_sensitive(attw->movepath_edit_button, sensitive);

}  /* end p_attw_update_sensitivity */


/* --------------------------------
 * p_get_default_attribute
 * --------------------------------
 * get the typical default value depending
 * on the attribute type (specified via att_type_idx)
 * and depending on MODIFIER Keys held down when button was clicked
 * (specified via bevent)
 *
 * SHIFT: reset to start value (at dialog creation time)
 * CTRL:  left/top outside,  Half size,   50% opaque
 * ALT:   right/bot outside, Double size, 75% opaque
 */
static gdouble
p_get_default_attribute(GapStbAttrWidget *attw
                     , GdkEventButton  *bevent
                     , glong att_type_idx
                     , gboolean form_value)
{
  if(bevent)
  {
    if (bevent->state & GDK_CONTROL_MASK)
    {
      if ((att_type_idx == GAP_STB_ATT_TYPE_MOVE_X)
      ||  (att_type_idx == GAP_STB_ATT_TYPE_MOVE_Y))
      {
        return (-1.0);  /* -1.0 indicates left/top outside */
      }
      if ((att_type_idx == GAP_STB_ATT_TYPE_ZOOM_X)
      ||  (att_type_idx == GAP_STB_ATT_TYPE_ZOOM_Y))
      {
        return (0.5);  /* 1.0 scale to half size */
      }
      if (att_type_idx == GAP_STB_ATT_TYPE_ROTATE)
      {
        return (180.0);
      }
      return (0.5);    /* indicates 50% opacity */
    }

    if(bevent->state & GDK_MOD1_MASK)  /* ALT */
    {
      if ((att_type_idx == GAP_STB_ATT_TYPE_MOVE_X)
      ||  (att_type_idx == GAP_STB_ATT_TYPE_MOVE_Y))
      {
        return (1.0);  /* 1.0 indicates right/bottom outside */
      }
      if ((att_type_idx == GAP_STB_ATT_TYPE_ZOOM_X)
      ||  (att_type_idx == GAP_STB_ATT_TYPE_ZOOM_Y))
      {
        return (2.0);  /* 1.0 scale to doble size */
      }
      if (att_type_idx == GAP_STB_ATT_TYPE_ROTATE)
      {
        return (360.0);
      }
      return (0.75);  /* indicates 75% opacity */
    }

    if(bevent->state & GDK_SHIFT_MASK)
    {
      if(attw->stb_elem_bck)
      {
        if(form_value)
        {
          return (attw->stb_elem_bck->att_arr_value_from[att_type_idx]);
        }
        return(attw->stb_elem_bck->att_arr_value_to[att_type_idx]);
      }
    }
  }

  return (gap_story_get_default_attribute(att_type_idx));

}  /* end p_get_default_attribute */

/* -----------------------------------------
 * p_attw_start_button_clicked_callback
 * -----------------------------------------
 */
static void
p_attw_start_button_clicked_callback(GtkWidget *widget
               , GdkEventButton  *bevent
               , glong att_type_idx)
{
  GapStbAttrWidget *attw;
  attw = g_object_get_data( G_OBJECT(widget), OBJ_DATA_KEY_ATTW );
  if(attw)
  {
    if(attw->stb_elem_refptr)
    {
      gdouble attr_value;

      p_attw_push_undo_and_set_unsaved_changes(attw);
      attr_value = p_getConvertFactor(att_type_idx) * p_get_default_attribute(attw
                        , bevent
                        , att_type_idx
                        , TRUE   /* use from value for reset */
                        );

      gtk_adjustment_set_value(GTK_ADJUSTMENT(attw->att_rows[att_type_idx].spinbutton_from_adj)
                                , attr_value);

    }
  }
}  /* end p_attw_start_button_clicked_callback */

/* -----------------------------------------
 * p_attw_end_button_clicked_callback
 * -----------------------------------------
 */
static void
p_attw_end_button_clicked_callback(GtkWidget *widget
               , GdkEventButton  *bevent
               , glong att_type_idx)
{
  GapStbAttrWidget *attw;
  attw = g_object_get_data( G_OBJECT(widget), OBJ_DATA_KEY_ATTW );
  if(attw)
  {
    if(attw->stb_elem_refptr)
    {
      gdouble attr_value;

      p_attw_push_undo_and_set_unsaved_changes(attw);
      attr_value = p_getConvertFactor(att_type_idx) * p_get_default_attribute(attw
                        , bevent
                        , att_type_idx
                        , FALSE   /* use to value for reset */
                        );

      gtk_adjustment_set_value(GTK_ADJUSTMENT(attw->att_rows[att_type_idx].spinbutton_to_adj)
                                , attr_value);

    }
  }
}  /* end p_attw_end_button_clicked_callback */


/* -----------------------------------------
 * p_copy_duration_to_all
 * -----------------------------------------
 * copy to all duration attributes (except overlap)
 */
static void
p_copy_duration_to_all(gint32 duration, GapStbAttrWidget *attw)
{
  if(attw)
  {
    gint ii;

    p_attw_push_undo_and_set_unsaved_changes(attw);
    for(ii = 0; ii < GAP_STB_ATT_TYPES_ARRAY_MAX; ii++)
    {
      if(attw->stb_elem_refptr->att_arr_enable[ii] == TRUE)
      {
        gtk_adjustment_set_value(GTK_ADJUSTMENT(attw->att_rows[ii].spinbutton_dur_adj)
                                , duration);
      }
    }
  }
}  /* end p_copy_duration_to_all */


/* -----------------------------------------
 * p_attw_overlap_dur_button_clicked_callback
 * -----------------------------------------
 */
static void
p_attw_overlap_dur_button_clicked_callback(GtkWidget *widget
               , GdkEventButton  *bevent
               , GapStbAttrWidget *attw)
{
  if(attw)
  {
    if(attw->stb_elem_refptr)
    {
      gint32 duration;

      p_attw_push_undo_and_set_unsaved_changes(attw);
      duration = attw->stb_elem_refptr->att_overlap;
      p_copy_duration_to_all(duration, attw);

    }
  }
}  /* end p_attw_overlap_dur_button_clicked_callback */



/* -----------------------------------------
 * p_attw_dur_button_clicked_callback
 * -----------------------------------------
 */
static void
p_attw_dur_button_clicked_callback(GtkWidget *widget
               , GdkEventButton  *bevent
               , glong att_type_idx)
{
  GapStbAttrWidget *attw;
  attw = g_object_get_data( G_OBJECT(widget), OBJ_DATA_KEY_ATTW );
  if(attw)
  {
    if(attw->stb_elem_refptr)
    {
      gint32 duration;

      p_attw_push_undo_and_set_unsaved_changes(attw);
      duration = attw->stb_elem_refptr->att_arr_value_dur[att_type_idx];
      p_copy_duration_to_all(duration, attw);

    }
  }
}  /* end p_attw_dur_button_clicked_callback */


/* ----------------------------------
 * p_attw_gdouble_adjustment_callback
 * ----------------------------------
 *
 * Notes: the value we get from the spinbutton is divided by 100
 *        because the GUI presents 100 percent as 100.0
 *        but internal representation uses the value 1.0
 *        for 100 percent.
 */
static void
p_attw_gdouble_adjustment_callback(GtkObject *obj, gdouble *val)
{
  GapStbAttrWidget *attw;
  gdouble l_val;
  glong   att_type_idx;

  att_type_idx = (glong) g_object_get_data( G_OBJECT(obj), "att_type_idx" );
  attw = g_object_get_data( G_OBJECT(obj), OBJ_DATA_KEY_ATTW );
  if(attw)
  {
    if(attw->stb_elem_refptr)
    {
      l_val = (GTK_ADJUSTMENT(obj)->value) / p_getConvertFactor(att_type_idx);
      if(gap_debug)
      {
        printf("gdouble_adjustment_callback: old_val:%f val:%f\n"
          , (float)*val
          , (float)l_val
          );
      }
      if(l_val != *val)
      {
        p_attw_push_undo_and_set_unsaved_changes(attw);
        *val = l_val;
        p_attw_update_properties(attw);
      }
    }
  }

}  /* end p_attw_gdouble_adjustment_callback */


/* ---------------------------------
 * p_duration_dependent_refresh
 * ---------------------------------
 * support of overlapping requires refresh of both previews in most situations
 * (in some sitations, where att_overlap == 0 it might be sufficient
 * to refresh only the preview that represents the TO value.
 * this simple implementation currently refreshes always both previews)
 */
static void
p_duration_dependent_refresh(GapStbAttrWidget *attw)
{
  attw->timer_full_update_request = TRUE;
  p_attw_update_properties(attw);
}  /* end p_duration_dependent_refresh */

/* -----------------------------------
 * p_attw_duration_adjustment_callback
 * -----------------------------------
 */
static void
p_attw_duration_adjustment_callback(GtkObject *obj, gint32 *val)
{
  GapStbAttrWidget *attw;
  gint32 l_val;


  attw = g_object_get_data( G_OBJECT(obj), OBJ_DATA_KEY_ATTW );
  if(attw)
  {
    if(attw->stb_elem_refptr)
    {
      l_val = RINT (GTK_ADJUSTMENT(obj)->value);
      if(gap_debug)
      {
        printf("gint32_adjustment_callback: obj:%ld old_val:%d val:%d\n"
             ,(long)obj
             ,(int)*val
             ,(int)l_val
             );
      }
      if(l_val != *val)
      {
        p_attw_push_undo_and_set_unsaved_changes(attw);
        *val = l_val;
        p_duration_dependent_refresh(attw);
      }
    }
  }

}  /* end p_attw_duration_adjustment_callback */


/* -----------------------------------
 * p_attw_accel_adjustment_callback
 * -----------------------------------
 */
static void
p_attw_accel_adjustment_callback(GtkObject *obj, gint32 *val)
{
  GapStbAttrWidget *attw;
  gint32 l_val;


  attw = g_object_get_data( G_OBJECT(obj), OBJ_DATA_KEY_ATTW );
  if(attw)
  {
    if(attw->stb_elem_refptr)
    {
      l_val = RINT (GTK_ADJUSTMENT(obj)->value);
      if(gap_debug)
      {
        printf("accel gint32_adjustment_callback: obj:%ld old_val:%d val:%d\n"
             ,(long)obj
             ,(int)*val
             ,(int)l_val
             );
      }
      if(l_val != *val)
      {
        p_attw_push_undo_and_set_unsaved_changes(attw);
        *val = l_val;

      }
    }
  }

}  /* end p_attw_accel_adjustment_callback */

/* ------------------------------------
 * p_attw_enable_toggle_update_callback
 * ------------------------------------
 */
static void
p_attw_enable_toggle_update_callback(GtkWidget *widget, gboolean *val)
{
  GapStbAttrWidget *attw;

  attw = g_object_get_data( G_OBJECT(widget), OBJ_DATA_KEY_ATTW );
  if((attw) && (val))
  {
    if(attw->stb_elem_refptr)
    {
      p_attw_push_undo_and_set_unsaved_changes(attw);

      if (GTK_TOGGLE_BUTTON (widget)->active)
      {
        *val = TRUE;
      }
      else
      {
        *val = FALSE;
      }
      p_attw_update_sensitivity(attw);
      p_duration_dependent_refresh(attw);
      p_attw_update_properties(attw);
    }
  }
}  /* end p_attw_enable_toggle_update_callback */



/* -----------------------------------------
 * p_attw_auto_update_toggle_update_callback
 * -----------------------------------------
 */
static void
p_attw_auto_update_toggle_update_callback(GtkWidget *widget, gboolean *val)
{
  GapStbAttrWidget *attw;

  attw = g_object_get_data( G_OBJECT(widget), OBJ_DATA_KEY_ATTW );
  if((attw) && (val))
  {
    if(attw->stb_elem_refptr)
    {
      p_attw_push_undo_and_set_unsaved_changes(attw);

      if (GTK_TOGGLE_BUTTON (widget)->active)
      {
        *val = TRUE;
        p_update_full_preview_gfx(attw);
      }
      else
      {
        *val = FALSE;
      }
    }
  }
}  /* end p_attw_auto_update_toggle_update_callback */


/* ---------------------------------
 * p_attw_preview_events_cb
 * ---------------------------------
 */
static gboolean
p_attw_preview_events_cb (GtkWidget *widget
                       , GdkEvent  *event
                       , GapStbAttrWidget *attw)
{
  /* GapStbMainGlobalParams  *sgpp; */
  /* GdkEventButton *bevent; */
  /* GdkEventExpose *eevent; */
  glong                    img_idx;
  gboolean                 enableStoryboardDebugFeatures;


  if ((attw->stb_elem_refptr == NULL)
  ||  (attw->stb_refptr == NULL))
  {
    /* the attribute widget is not initialized no action allowed */
    return FALSE;
  }
  /* sgpp = attw->sgpp; */

  img_idx = (glong)g_object_get_data( G_OBJECT(widget), OBJ_DATA_KEY_IMG_IDX );
  if(img_idx != 0)
  {
    img_idx = 1;
  }


  switch (event->type)
  {
    case GDK_BUTTON_PRESS:
      /* bevent = (GdkEventButton *) event; */

      // TODO: define actions when button pressed.

      /* debug code to display a copy of the image */
      enableStoryboardDebugFeatures =
          gap_base_get_gimprc_gboolean_value(GAP_GIMPRC_ENABLE_STORYBOARD_DEBUG_FEATURES, FALSE);
      if(enableStoryboardDebugFeatures)
      {
        p_debug_dup_image(attw->gfx_tab[img_idx].image_id);
      }

      return FALSE;
      break;

    case GDK_EXPOSE:
      if(gap_debug)
      {
        printf("p_attw_preview_events_cb GDK_EXPOSE widget:%ld  img_idx:%ld\n"
                              , (long)widget
                              , (long)img_idx
                              );
      }

      /* eevent = (GdkEventExpose *) event; */

      gap_pview_repaint(attw->gfx_tab[img_idx].pv_ptr);
      gdk_flush ();

      break;

    default:
      break;
  }

  return FALSE;
}       /* end  p_attw_preview_events_cb */


/* -----------------------------------------
 * p_calculate_prefetch_render_attributes
 * -----------------------------------------
 * Calculate render attributes for the prefetch frame
 * The prefetch frame is used for overlapping frames for
 * the "normal" track that is overlapped by the shadow track.
 * Defaults for all transitions transition settings are used here,
 * because the transition settings will apply to the curr_layer in the shadow track,
 * but settings for keep_proportions, fit_width and fit_height are respected.
 * NOTE:
 *   for exact rendering the transition attribute settings at prefetch position
 *   would be required, but defaults should be sufficient to visualizes the settings
 *   of the current transition without interfering with settings of prior transitions.
 */
static void
p_calculate_prefetch_render_attributes(GapStbAttrWidget *attw
    , glong img_idx
    , GapStoryCalcAttr  *calc_attr_ptr
    )
{
  gint ii;
  gint32  pv_master_width;   /* master width scaled to preview size */
  gint32  pv_master_height;  /* master height scaled to preview size */
  gdouble master_scale;
  gdouble att_tab[GAP_STB_ATT_GFX_ARRAY_MAX][GAP_STB_ATT_TYPES_ARRAY_MAX];
  gdouble pv_frame_width;
  gdouble pv_frame_height;

  pv_master_width = gimp_image_width(attw->gfx_tab[img_idx].image_id) * PVIEW_TO_MASTER_SCALE;
  pv_master_height = gimp_image_height(attw->gfx_tab[img_idx].image_id) * PVIEW_TO_MASTER_SCALE;

  master_scale = (gdouble)pv_master_width
               / (gdouble)(MAX( 1, attw->stb_refptr->master_width));

  pv_frame_width = (gdouble)gimp_drawable_width(attw->gfx_tab[img_idx].opre_layer_id) * master_scale;
  pv_frame_height = (gdouble)gimp_drawable_height(attw->gfx_tab[img_idx].opre_layer_id) * master_scale;

  for(ii=0; ii < GAP_STB_ATT_TYPES_ARRAY_MAX; ii++)
  {
    att_tab[0][ii] =  gap_story_get_default_attribute(ii);
    att_tab[1][ii] =  gap_story_get_default_attribute(ii);
  }

  gap_story_file_calculate_render_attributes(calc_attr_ptr
    , gimp_image_width(attw->gfx_tab[img_idx].image_id)
    , gimp_image_height(attw->gfx_tab[img_idx].image_id)
    , pv_master_width
    , pv_master_height
    , (gint32)rint(pv_frame_width)
    , (gint32)rint(pv_frame_height)
    , attw->stb_elem_refptr->att_keep_proportions
    , attw->stb_elem_refptr->att_fit_width
    , attw->stb_elem_refptr->att_fit_height
    , att_tab [img_idx][GAP_STB_ATT_TYPE_ROTATE]
    , att_tab [img_idx][GAP_STB_ATT_TYPE_OPACITY]
    , att_tab [img_idx][GAP_STB_ATT_TYPE_ZOOM_X]
    , att_tab [img_idx][GAP_STB_ATT_TYPE_ZOOM_Y]
    , att_tab [img_idx][GAP_STB_ATT_TYPE_MOVE_X]
    , att_tab [img_idx][GAP_STB_ATT_TYPE_MOVE_Y]
    );


}  /* end p_calculate_prefetch_render_attributes */



/* -----------------------------------------
 * p_calculate_render_attributes
 * -----------------------------------------
 * NOTE: filtermacro processing is ignored,
 *       the preview
 */
static void
p_calculate_render_attributes(GapStbAttrWidget *attw
    , glong img_idx
    , GapStoryCalcAttr  *calc_attr_ptr
    )
{
  gint ii;
  gint32  pv_master_width;   /* master width scaled to preview size */
  gint32  pv_master_height;  /* master height scaled to preview size */
  gdouble master_scale;
  gdouble att_tab[GAP_STB_ATT_GFX_ARRAY_MAX][GAP_STB_ATT_TYPES_ARRAY_MAX];
  gdouble pv_frame_width;
  gdouble pv_frame_height;

  pv_master_width = gimp_image_width(attw->gfx_tab[img_idx].image_id) * PVIEW_TO_MASTER_SCALE;
  pv_master_height = gimp_image_height(attw->gfx_tab[img_idx].image_id) * PVIEW_TO_MASTER_SCALE;

  master_scale = (gdouble)pv_master_width
               / (gdouble)(MAX( 1, attw->stb_refptr->master_width));

  pv_frame_width = (gdouble)gimp_drawable_width(attw->gfx_tab[img_idx].orig_layer_id) * master_scale;
  pv_frame_height = (gdouble)gimp_drawable_height(attw->gfx_tab[img_idx].orig_layer_id) * master_scale;

  for(ii=0; ii < GAP_STB_ATT_TYPES_ARRAY_MAX; ii++)
  {
    if(attw->stb_elem_refptr->att_arr_enable[ii] == TRUE)
    {
      att_tab[0][ii] =  attw->stb_elem_refptr->att_arr_value_from[ii];
      att_tab[1][ii] =  attw->stb_elem_refptr->att_arr_value_to[ii];
    }
    else
    {
      att_tab[0][ii] =  gap_story_get_default_attribute(ii);
      att_tab[1][ii] =  gap_story_get_default_attribute(ii);
    }
  }

  gap_story_file_calculate_render_attributes(calc_attr_ptr
    , gimp_image_width(attw->gfx_tab[img_idx].image_id)
    , gimp_image_height(attw->gfx_tab[img_idx].image_id)
    , pv_master_width
    , pv_master_height
    , (gint32)rint(pv_frame_width)
    , (gint32)rint(pv_frame_height)
    , attw->stb_elem_refptr->att_keep_proportions
    , attw->stb_elem_refptr->att_fit_width
    , attw->stb_elem_refptr->att_fit_height
    , att_tab [img_idx][GAP_STB_ATT_TYPE_ROTATE]
    , att_tab [img_idx][GAP_STB_ATT_TYPE_OPACITY]
    , att_tab [img_idx][GAP_STB_ATT_TYPE_ZOOM_X]
    , att_tab [img_idx][GAP_STB_ATT_TYPE_ZOOM_Y]
    , att_tab [img_idx][GAP_STB_ATT_TYPE_MOVE_X]
    , att_tab [img_idx][GAP_STB_ATT_TYPE_MOVE_Y]
    );


}  /* end p_calculate_render_attributes */


/* -----------------------------------------
 * p_check_and_make_opre_default_layer
 * -----------------------------------------
 * check if there is a valid original prefetch layer opre_layer_id (>= 0)
 * if NOT create a faked opre layer at master size.
 */
static void
p_check_and_make_opre_default_layer(GapStbAttrWidget *attw, glong img_idx)
{
  if(attw->gfx_tab[img_idx].opre_layer_id < 0)
  {
    gint32  image_id;
    gint32  layer_id;
    gdouble red, green, blue, alpha;

    image_id = attw->gfx_tab[img_idx].image_id;
    layer_id = gimp_layer_new(image_id
                  , LAYERNAME_OPRE
                  , attw->stb_refptr->master_width
                  , attw->stb_refptr->master_height
                  , GIMP_RGBA_IMAGE
                  , 100.0   /* full opacity */
                  , 0       /* normal mode */
                  );

    gimp_image_insert_layer (image_id, layer_id, 0, LAYERSTACK_TOP);
    red   = 0.72;
    green = 0.80;
    blue  = 0.25;
    alpha = 1.0;
    gap_layer_clear_to_color(layer_id, red, green, blue, alpha);

    attw->gfx_tab[img_idx].opre_layer_id = layer_id;
    attw->gfx_tab[img_idx].opre_info.layer_is_fake = TRUE;
  }
  gimp_item_set_visible(attw->gfx_tab[img_idx].opre_layer_id, FALSE);

}  /* end p_check_and_make_opre_default_layer */



/* -----------------------------------------
 * p_check_and_make_orig_default_layer
 * -----------------------------------------
 * check if there is a valid orig_layer_id (>= 0)
 * if NOT create a faked orig layers at master size.
 */
static void
p_check_and_make_orig_default_layer(GapStbAttrWidget *attw, glong img_idx)
{
  if(attw->gfx_tab[img_idx].orig_layer_id < 0)
  {
    gint32  image_id;
    gint32  layer_id;
    gdouble red, green, blue, alpha;

    image_id = attw->gfx_tab[img_idx].image_id;
    layer_id = gimp_layer_new(image_id
                  , LAYERNAME_ORIG
                  , attw->stb_refptr->master_width
                  , attw->stb_refptr->master_height
                  , GIMP_RGBA_IMAGE
                  , 100.0   /* full opacity */
                  , 0       /* normal mode */
                  );

    gimp_image_insert_layer (image_id, layer_id, 0, LAYERSTACK_TOP);
    red   = 0.42;
    green = 0.90;
    blue  = 0.35;
    alpha = 1.0;
    gap_layer_clear_to_color(layer_id, red, green, blue, alpha);

    attw->gfx_tab[img_idx].orig_layer_id = layer_id;
    attw->gfx_tab[img_idx].orig_info.layer_is_fake = TRUE;
  }
  gimp_item_set_visible(attw->gfx_tab[img_idx].orig_layer_id, FALSE);

}  /* end p_check_and_make_orig_default_layer */


/* -----------------------------------------
 * p_create_color_layer
 * -----------------------------------------
 * create a layer with specified color at image size (== preview size)
 * with a transparent rectangle at scaled master size in the center.
 */
static gint32
p_create_color_layer(GapStbAttrWidget *attw, gint32 image_id
    , const char *name, gint stackposition, gdouble opacity
    , gdouble red, gdouble green, gdouble blue)
{
  gint32 layer_id;
  gdouble ofs_x;
  gdouble ofs_y;
  gdouble pv_master_width;   /* master width scaled to preview size */
  gdouble pv_master_height;  /* master height scaled to preview size */

  layer_id = gimp_layer_new(image_id
                  , name
                  , gimp_image_width(image_id)
                  , gimp_image_height(image_id)
                  , GIMP_RGBA_IMAGE
                  , opacity
                  , 0       /* normal mode */
                  );
  gimp_image_insert_layer (image_id, layer_id, 0, stackposition);
  gap_layer_clear_to_color(layer_id, red, green, blue, 1.0);
  gimp_item_set_visible(layer_id, TRUE);

  pv_master_width = (gdouble)gimp_image_width(image_id) * PVIEW_TO_MASTER_SCALE;
  pv_master_height = (gdouble)gimp_image_height(image_id) * PVIEW_TO_MASTER_SCALE;
  ofs_x = ((gdouble)gimp_image_width(image_id) - pv_master_width) / 2.0;
  ofs_y = ((gdouble)gimp_image_height(image_id) - pv_master_height) / 2.0;

  gimp_context_push();
  gimp_context_set_feather(FALSE);
  gimp_context_set_feather_radius(0.0, 0.0);

  /* green border */
  {
    GimpRGB  color;
    GimpRGB  bck_color;

    color.r = 0.25;
    color.g = 0.80;
    color.b = 0.17;
    color.a = 1.0;


    /* selection for green rectangle (2 pixels border around master size) */
    gimp_image_select_rectangle(image_id
                  , GIMP_CHANNEL_OP_REPLACE
                  , ofs_x -2
                  , ofs_y -2
                  , pv_master_width +4
                  , pv_master_height +4
                  );

    gimp_context_get_background(&bck_color);
    gimp_context_set_background(&color);
    /* fill the drawable (ignoring selections) */
    gimp_edit_fill(layer_id, GIMP_BACKGROUND_FILL);


    /* restore BG color in the context */
    gimp_context_set_background(&bck_color);
  }

  gimp_image_select_rectangle(image_id
                  , GIMP_CHANNEL_OP_REPLACE
                  , ofs_x
                  , ofs_y
                  , pv_master_width
                  , pv_master_height
                  );

  gimp_edit_clear(layer_id);
  gimp_selection_none(image_id);

  gimp_context_pop();

  return (layer_id);
}  /* end p_create_color_layer */


/* -----------------------------------------
 * p_create_base_layer
 * -----------------------------------------
 */
static gint32
p_create_base_layer(GapStbAttrWidget *attw, gint32 image_id)
{
  gint32 layer_id;
  gdouble  red, green, blue, alpha;

  red   = 0.86;
  green = 0.85;
  blue  = 0.84;
  alpha = 100.0;      /* full opaque */

  layer_id = p_create_color_layer(attw
    , image_id
    , LAYERNAME_BASE
    , LAYERSTACK_BASE
    , alpha
    , red
    , green
    , blue
    );

  return (layer_id);
}  /* end p_create_base_layer */

/* -----------------------------------------
 * p_create_deco_layer
 * -----------------------------------------
 */
static gint32
p_create_deco_layer(GapStbAttrWidget *attw, gint32 image_id)
{
  gint32 layer_id;
  gdouble  red, green, blue, alpha;

  red   = 0.86;
  green = 0.85;
  blue  = 0.84;
  alpha = 60.0;      /* 60% opaque */

  layer_id = p_create_color_layer(attw
    , image_id
    , LAYERNAME_DECO
    , LAYERSTACK_TOP
    , alpha
    , red
    , green
    , blue
    );

  return (layer_id);
}  /* end p_create_deco_layer */

/* -----------------------------------------
 * p_create_gfx_image
 * -----------------------------------------
 */
static void
p_create_gfx_image(GapStbAttrWidget *attw, glong img_idx)
{
  gint32 image_id;

  image_id = gimp_image_new( attw->gfx_tab[img_idx].pv_ptr->pv_width
                           , attw->gfx_tab[img_idx].pv_ptr->pv_height
                           , GIMP_RGB
                           );
  gimp_image_undo_disable (image_id);

  attw->gfx_tab[img_idx].base_layer_id = p_create_base_layer(attw, image_id);
  attw->gfx_tab[img_idx].deco_layer_id = p_create_deco_layer(attw, image_id);
  attw->gfx_tab[img_idx].orig_layer_id = -1;  /* to be created later */
  attw->gfx_tab[img_idx].curr_layer_id = -1;  /* to be created later */
  attw->gfx_tab[img_idx].opre_layer_id = -1;  /* to be created later */
  attw->gfx_tab[img_idx].pref_layer_id = -1;  /* to be created later */
  attw->gfx_tab[img_idx].image_id = image_id;
  attw->gfx_tab[img_idx].opre_info.layer_is_fake = TRUE;
  attw->gfx_tab[img_idx].orig_info.layer_is_fake = TRUE;
}  /* end p_create_gfx_image */

/* -----------------------------------------
 * p_delete_gfx_images
 * -----------------------------------------
 */
static void
p_delete_gfx_images(GapStbAttrWidget *attw)
{
  gimp_image_delete(attw->gfx_tab[0].image_id);
  gimp_image_delete(attw->gfx_tab[1].image_id);

  attw->gfx_tab[0].image_id = -1;
  attw->gfx_tab[1].image_id = -1;

}  /* end p_delete_gfx_images */


/* -----------------------------------------
 * p_adjust_stackposition
 * -----------------------------------------
 */
static void
p_adjust_stackposition(gint32 image_id, gint32 layer_id, gint position)
{
  gint   ii;

  /* adjust stack position */
  gimp_image_lower_item_to_bottom (image_id, layer_id);
  for (ii=0; ii < position; ii++)
  {
    gimp_image_raise_item (image_id, layer_id);
  }
}  /* end p_adjust_stackposition */


/* -----------------------------------------
 * p_is_width_or_height_fixed
 * -----------------------------------------
 * return TRUE in case either width or height is fixed
 * (e.g. is disabled to be scaled to fit target size)
 */
static gboolean
p_is_width_or_height_fixed(gboolean fit_width, gboolean fit_height, gboolean keep_proportions)
{
  if ((fit_width == FALSE) && (fit_height == FALSE))
  {
    return (TRUE);
  }

  if ((fit_width != fit_height) && (keep_proportions == FALSE))
  {
    return (TRUE);
  }

  return (FALSE);

}  /* end p_is_width_or_height_fixed */



/* -----------------------------------------
 * p_create_transformed_layer_movepath
 * -----------------------------------------
 * create transformed copy of the specified origsize_layer_id,
 * according to movepath rendering and current transformation settings.
 */
static gboolean
p_create_transformed_layer_movepath(gint32 image_id
  , gint32 origsize_layer_id
  , gint32 *layer_id_ptr
  , GapStoryCalcAttr  *calculated
  , gint32 stackposition, const char *layername
  , GapStbAttrWidget *attw
  , glong img_idx
  )
{
  char    *movepath_file_xml;
  gboolean keep_proportions;
  gboolean fit_width;
  gboolean fit_height;
  gint     master_width;
  gint     master_height;
  gint32   new_layer_id;
  gint32   mov_obj_image_id;
  gint32   mov_obj_layer_id;
  gdouble  att_tab[GAP_STB_ATT_GFX_ARRAY_MAX][GAP_STB_ATT_TYPES_ARRAY_MAX];
  gint     ii;

  if(attw == NULL)
  {
    printf("** INTERNAL ERROR p_create_transformed_layer_movepath called with attw is NULL\n");
    return (FALSE);
  }
  if(attw->stb_refptr == NULL)
  {
    printf("** INTERNAL ERROR p_create_transformed_layer_movepath called with stb_refptr is NULL\n");
    return (FALSE);
  }

  if(attw->movepath_file_xml_is_valid != TRUE)
  {
    return (FALSE);
  }

  master_width = attw->stb_refptr->master_width;
  master_height = attw->stb_refptr->master_height;
  keep_proportions = attw->stb_elem_refptr->att_keep_proportions;
  fit_width = attw->stb_elem_refptr->att_fit_width;
  fit_height = attw->stb_elem_refptr->att_fit_height;
  movepath_file_xml = attw->stb_elem_refptr->att_movepath_file_xml;
  for(ii=0; ii < GAP_STB_ATT_TYPES_ARRAY_MAX; ii++)
  {
    if(attw->stb_elem_refptr->att_arr_enable[ii] == TRUE)
    {
      att_tab[0][ii] =  attw->stb_elem_refptr->att_arr_value_from[ii];
      att_tab[1][ii] =  attw->stb_elem_refptr->att_arr_value_to[ii];
    }
    else
    {
      att_tab[0][ii] =  gap_story_get_default_attribute(ii);
      att_tab[1][ii] =  gap_story_get_default_attribute(ii);
    }
  }

  if(movepath_file_xml == NULL)
  {
    return (FALSE);
  }
  if(origsize_layer_id < 0)
  {
    return (FALSE);
  }


  /* recreate the current layer at stackposition 2 */

  /* create image with 1:1 copy of the origsize_layer_id */
  mov_obj_image_id = gimp_image_new( gimp_drawable_width(origsize_layer_id)
                       , gimp_drawable_height(origsize_layer_id)
                       , GIMP_RGB
                       );
  gimp_image_undo_disable (mov_obj_image_id);
  mov_obj_layer_id = gimp_layer_new_from_drawable(origsize_layer_id, mov_obj_image_id);


  if(mov_obj_layer_id >= 0)
  {
    gimp_item_set_visible(mov_obj_layer_id, TRUE);
    if(! gimp_drawable_has_alpha(mov_obj_layer_id))
    {
       /* have to add alpha channel */
       gimp_layer_add_alpha(mov_obj_layer_id);
    }
    gimp_image_insert_layer(mov_obj_image_id, mov_obj_layer_id, 0, 0);
    gimp_layer_set_offsets(mov_obj_layer_id, 0, 0);
  }

  //if(img_idx == 0)
  //{
  //  p_debug_dup_image(mov_obj_image_id);
  //}

  /* prescale to preview size in case fixed witdh or height
   * (we can skip the prescale in other modes, because the non fixed modes
   * allow scaling to render size in the movepath render processing)
   */
  if (p_is_width_or_height_fixed(fit_width, fit_height, keep_proportions) == TRUE)
  {
    gint scaled_width;
    gint scaled_height;

    scaled_width = gimp_image_width(mov_obj_image_id) * gimp_image_width(image_id) / master_width;
    scaled_height = gimp_image_height(mov_obj_image_id) * gimp_image_height(image_id) / master_height;
    gimp_image_scale(mov_obj_image_id, scaled_width, scaled_height);
  }

  /* call storyboard processor that does movepath and other transformations
   * (note that processing is done at size of the preview image that is 2 times larger
   * than the wanted result because
   */
  new_layer_id = gap_story_render_transform_with_movepath_processing(image_id
                              , mov_obj_image_id
                              , mov_obj_layer_id
                              , keep_proportions
                              , fit_width
                              , fit_height
                              , att_tab [img_idx][GAP_STB_ATT_TYPE_ROTATE]   /* rotation in degree */
                              , att_tab [img_idx][GAP_STB_ATT_TYPE_OPACITY]   /* 0.0 upto 1.0 */
                              , att_tab [img_idx][GAP_STB_ATT_TYPE_ZOOM_X]    /* 0.0 upto 10.0 where 1.0 = 1:1 */
                              , att_tab [img_idx][GAP_STB_ATT_TYPE_ZOOM_Y]    /* 0.0 upto 10.0 where 1.0 = 1:1 */
                              , att_tab [img_idx][GAP_STB_ATT_TYPE_MOVE_X]    /* -1.0 upto +1.0 where 0 = no move, -1 is left outside */
                              , att_tab [img_idx][GAP_STB_ATT_TYPE_MOVE_Y]     /* -1.0 upto +1.0 where 0 = no move, -1 is top outside */
                              , movepath_file_xml
                              , att_tab [img_idx][GAP_STB_ATT_TYPE_MOVEPATH]    /* movepath_framePhase */
                           );

  gimp_layer_resize_to_image_size(new_layer_id);

  gimp_layer_scale(new_layer_id
                  , gimp_drawable_width(new_layer_id) * PVIEW_TO_MASTER_SCALE
                  , gimp_drawable_height(new_layer_id) * PVIEW_TO_MASTER_SCALE
                  , TRUE  /*  TRUE: centered local on layer */
                  );


  if(gap_debug)
  {
    printf("p_create_transformed_layer_movepath: "
      "new_layer_id:%d new_layers_image_id:%d  mov_obj_image_id:%d (preview)image_id:%d\n"
      ,(int)new_layer_id
      ,(int)gimp_item_get_image(new_layer_id)
      ,(int)mov_obj_image_id
      ,(int)image_id
      );
  }
  if(mov_obj_image_id >= 0)
  {
    gap_image_delete_immediate(mov_obj_image_id);
  }


  gimp_item_set_visible(new_layer_id, TRUE);

  p_adjust_stackposition(image_id, new_layer_id, stackposition);

  gimp_layer_set_opacity(new_layer_id
                        , calculated->opacity
                        );
  *layer_id_ptr = new_layer_id;

  return (TRUE);

}  /* end p_create_transformed_layer_movepath */


/* -----------------------------------------
 * p_create_transformed_layer
 * -----------------------------------------
 * create transformed copy of the specified origsize_layer_id,
 * according to calculated transformation settings.
 *
 */
static void
p_create_transformed_layer(gint32 image_id
  , gint32 origsize_layer_id
  , gint32 *layer_id_ptr
  , GapStoryCalcAttr  *calculated
  , gint32 stackposition
  , const char *layername
  , GapStbAttrWidget *attw
  , glong img_idx
  , gboolean enableMovepath
  )
{
  char    *movepath_file_xml;

  movepath_file_xml = NULL;
  if((attw != NULL)
  && (enableMovepath))
  {
    if(attw->stb_elem_refptr != NULL)
    {
      if (attw->stb_elem_refptr->att_arr_enable[GAP_STB_ATT_TYPE_MOVEPATH] == TRUE)
      {
        movepath_file_xml = attw->stb_elem_refptr->att_movepath_file_xml;
      }
    }
  }

  /* if size is not equal calculated size
   * or movepath rendering necessary then remove curr layer
   * to force recreation in desired size
   */
  if (*layer_id_ptr >= 0)
  {
     if ((gimp_drawable_width(*layer_id_ptr)  != calculated->width)
     ||  (gimp_drawable_height(*layer_id_ptr) != calculated->height)
     ||  (movepath_file_xml != NULL))
     {
        gimp_image_remove_layer( image_id
                               , *layer_id_ptr);
        *layer_id_ptr = -1;
     }
  }

  /* check and recreate the current layer at stackposition 2 */
  if (*layer_id_ptr < 0)
  {
    gint32 new_layer_id;

    if(movepath_file_xml != NULL)
    {
      gboolean movepathOk;

      movepathOk = p_create_transformed_layer_movepath(image_id
                 , origsize_layer_id
                 , layer_id_ptr
                 , calculated
                 , stackposition
                 , layername
                 , attw
                 , img_idx
                 );
      if(movepathOk == TRUE)
      {
        return;
      }
      /* else fallback to rendering without movepath transformations */
    }

    new_layer_id = gimp_layer_copy(origsize_layer_id);
    gimp_image_insert_layer (image_id, new_layer_id, 0, stackposition);
    gimp_item_set_name(new_layer_id, layername);

    gimp_layer_scale(new_layer_id, calculated->width, calculated->height, 0);



    gimp_item_set_visible(new_layer_id, TRUE);

    *layer_id_ptr = new_layer_id;
  }
  p_adjust_stackposition(image_id, *layer_id_ptr, stackposition);


  /* set offsets and opacity */
  gimp_layer_set_offsets(*layer_id_ptr
                        , calculated->x_offs
                        , calculated->y_offs
                        );

  gap_story_transform_rotate_layer(image_id, *layer_id_ptr, calculated->rotate);

  gimp_layer_set_opacity(*layer_id_ptr
                        , calculated->opacity
                        );

}  /* end p_create_transformed_layer */


/* -----------------------------------------
 * p_calculate_prefetch_visibility
 * -----------------------------------------
 */
static gboolean
p_calculate_prefetch_visibility(GapStbAttrWidget *attw, glong img_idx)
{
  gboolean prefetch_visible;

  prefetch_visible = FALSE;
  if(attw->stb_elem_refptr->att_overlap > 0)
  {
    gint32 duration;

    duration = p_get_relevant_duration(attw, img_idx);
    if(attw->stb_elem_refptr->att_overlap >= duration)
    {
      prefetch_visible = TRUE;
    }
  }

  return (prefetch_visible);
}  /* end p_calculate_prefetch_visibility */


/* -----------------------------------------
 * p_render_gfx
 * -----------------------------------------
 * render graphical preview.
 * This procedure does not include fetching the referred frame
 * It assumes that the corresponding frame(s) have already been fetched
 * into the orig_layer (and opre_layer for overlapping frames within this track)
 * of the internal gimp image that will be used for
 * rendering of the preview.
 * If that image does not contain such a valid orig_layer and opre_layer,
 * it creates empty default representations, where master size is assumed.
 */
static void
p_render_gfx(GapStbAttrWidget *attw, glong img_idx)
{
  GapStoryCalcAttr  calculate_curr_attributes;
  GapStoryCalcAttr  calculate_pref_attributes;
  gint32   image_id;
  gboolean prefetch_visible;

  image_id = attw->gfx_tab[img_idx].image_id;

  p_check_and_make_orig_default_layer(attw, img_idx);
  p_check_and_make_opre_default_layer(attw, img_idx);


  p_calculate_prefetch_render_attributes(attw
     , img_idx
     , &calculate_pref_attributes
     );

  p_create_transformed_layer(image_id
      , attw->gfx_tab[img_idx].opre_layer_id
      , &attw->gfx_tab[img_idx].pref_layer_id
      , &calculate_pref_attributes
      , LAYERSTACK_PREF
      , LAYERNAME_PREF
      , attw
      , img_idx
      , FALSE     /* do not enableMovepath */
      );


  p_calculate_render_attributes (attw
     , img_idx
     , &calculate_curr_attributes
     );

  p_create_transformed_layer(image_id
      , attw->gfx_tab[img_idx].orig_layer_id
      , &attw->gfx_tab[img_idx].curr_layer_id
      , &calculate_curr_attributes
      , LAYERSTACK_CURR
      , LAYERNAME_CURR
      , attw
      , img_idx
      , TRUE     /* enableMovepath */
      );

  prefetch_visible = p_calculate_prefetch_visibility(attw, img_idx);

  gimp_item_set_visible(attw->gfx_tab[img_idx].opre_layer_id, FALSE);
  gimp_item_set_visible(attw->gfx_tab[img_idx].orig_layer_id, FALSE);
  gimp_item_set_visible(attw->gfx_tab[img_idx].deco_layer_id, TRUE);
  gimp_item_set_visible(attw->gfx_tab[img_idx].curr_layer_id, TRUE);
  gimp_item_set_visible(attw->gfx_tab[img_idx].pref_layer_id, prefetch_visible);
  gimp_item_set_visible(attw->gfx_tab[img_idx].base_layer_id, TRUE);

  /* render the preview from image */
  gap_pview_render_from_image_duplicate (attw->gfx_tab[img_idx].pv_ptr
                                , image_id
                                );
  gtk_widget_queue_draw(attw->gfx_tab[img_idx].pv_ptr->da_widget);
  gdk_flush();
}  /* end p_render_gfx */


/* -----------------------------------
 * p_update_framenr_labels
 * -----------------------------------
 * update both frame number and time label 000001 00:00:000
 * for the specified start or end gfx_preview (via img_idx)
 */
static void
p_update_framenr_labels(GapStbAttrWidget *attw, glong img_idx, gint32 framenr)
{
  char    txt_buf[100];
  gdouble l_speed_fps;

  if(attw == NULL) { return; }
  if(attw->stb_elem_refptr == NULL) { return; }


  g_snprintf(txt_buf, sizeof(txt_buf), "%d  "
            ,(int)framenr
            );
  gtk_label_set_text ( GTK_LABEL(attw->gfx_tab[img_idx].framenr_label), txt_buf);

  l_speed_fps = GAP_STORY_DEFAULT_FRAMERATE;
  if(attw->stb_refptr)
  {
    if(attw->stb_refptr->master_framerate > 0)
    {
      l_speed_fps = attw->stb_refptr->master_framerate;
    }
  }

  gap_timeconv_framenr_to_timestr(framenr -1
                         , l_speed_fps
                         , txt_buf
                         , sizeof(txt_buf)
                         );
  gtk_label_set_text ( GTK_LABEL(attw->gfx_tab[img_idx].frametime_label), txt_buf);

}  /* end p_update_framenr_labels */


/* -----------------------------------------
 * p_get_relevant_duration
 * -----------------------------------------
 */
static gint32
p_get_relevant_duration(GapStbAttrWidget *attw, glong img_idx)
{
  gint32 duration;

  /* for the start frame ( img_idx ==0) always use duration 0 */
  duration = 0;
  if (img_idx != 0)
  {
    gint ii;

    /* for the end frame pick the duration from the first enabled attribute */
    for(ii=0; ii < GAP_STB_ATT_TYPES_ARRAY_MAX; ii++)
    {
      if(attw->stb_elem_refptr->att_arr_enable[ii] == TRUE)
      {
        duration = attw->stb_elem_refptr->att_arr_value_dur[ii];
        break;
      }
    }

  }

  return(duration);
}  /* end p_get_relevant_duration */


/* -----------------------------------------
 * p_update_full_preview_gfx
 * -----------------------------------------
 * update the previews (both start and end) with the corresponding auto_update flag set.
 * for rendering of the preview(s)
 * and fetching of the refered frame at orig size into orig_layer.
 */
static void
p_update_full_preview_gfx(GapStbAttrWidget *attw)
{
   glong img_idx;

   for(img_idx = 0; img_idx < GAP_STB_ATT_GFX_ARRAY_MAX; img_idx++)
   {
     if(attw->gfx_tab[img_idx].auto_update)
     {
       gint32 duration;

       duration = p_get_relevant_duration(attw, img_idx);
       p_create_or_replace_orig_and_opre_layer (attw, img_idx, duration);
       p_render_gfx (attw, img_idx);
     }
   }

}  /* end p_update_full_preview_gfx */




/* ---------------------------------
 * p_stb_req_equals_layer_info
 * ---------------------------------
 * check if required position (specified via stb_ret and filename)
 * refers to the same frame as stored in the specified layer info structure.
 * the relevant infornmation of the orig_layer is stored in
 *      linfo->layer_record_type;
 *      linfo->layer_local_framenr;
 *      linfo->layer_seltrack;
 *     *linfo->layer_filename;
 *
 */
static gboolean
p_stb_req_equals_layer_info(GapStbAttrLayerInfo *linfo
                         , GapStoryLocateRet *stb_ret
                         , const char *filename)
{
  if (stb_ret->stb_elem->record_type != linfo->layer_record_type)
  {
    return(FALSE);
  }
  if (stb_ret->stb_elem->seltrack != linfo->layer_seltrack)
  {
    return(FALSE);
  }

  if (stb_ret->ret_framenr != linfo->layer_local_framenr)
  {
    return(FALSE);
  }

  if (linfo->layer_filename == NULL)
  {
     if(filename != NULL)
     {
       return(FALSE);
     }
  }
  else
  {
     if(filename == NULL)
     {
       return(FALSE);
     }
     if(strcmp(filename, linfo->layer_filename) != 0)
     {
       return(FALSE);
     }
  }

  return(TRUE);
}  /* end p_stb_req_equals_layer_info */


/* ---------------------------------
 * p_destroy_orig_layer
 * ---------------------------------
 * destroy orig layer and corresponding derived layer
 * if they are already existing
 */
static void
p_destroy_orig_layer( gint32 image_id
                    , gint32 *orig_layer_id_ptr
                    , gint32 *derived_layer_id_ptr
                    , GapStbAttrLayerInfo *linfo)
{
   if(*orig_layer_id_ptr >= 0)
   {
     /* not up to date: destroy orig layer */
     gimp_image_remove_layer(image_id
                            , *orig_layer_id_ptr);
     *orig_layer_id_ptr = -1;
   }

   linfo->layer_record_type = GAP_STBREC_VID_UNKNOWN;
   linfo->layer_local_framenr = -1;
   linfo->layer_seltrack = -1;
   if (linfo->layer_filename != NULL)
   {
     g_free(linfo->layer_filename);
     linfo->layer_filename = NULL;
   }

   /* remove curr layer if already exists */
   if(*derived_layer_id_ptr >= 0)
   {
     gimp_image_remove_layer(image_id
                            , *derived_layer_id_ptr);
     *derived_layer_id_ptr = -1;
   }

}  /* end p_destroy_orig_layer */


/* ---------------------------------
 * p_check_orig_layer_up_to_date
 * ---------------------------------
 * check if there is an orig layer and if it is still
 * up to date. (return TRUE in that case)
 * else return FALSE.
 *
 * if there is a non-up to date orig layer destroy this outdated
 * orig layer and the correspondig derived layer too.
 */
static gboolean
p_check_orig_layer_up_to_date(gint32 image_id
                    , gint32 *orig_layer_id_ptr
                    , gint32 *derived_layer_id_ptr
                    , GapStbAttrLayerInfo *linfo
                    , GapStoryLocateRet *stb_ret
                    , const char *filename)
{
   if(*orig_layer_id_ptr >= 0)
   {
     if (p_stb_req_equals_layer_info(linfo, stb_ret, filename) == TRUE)
     {
       return (TRUE);  /* OK orig layer already up to date */
     }
   }
   /* remove orig layer and derived layer if already exists */
   p_destroy_orig_layer(image_id
                       ,orig_layer_id_ptr
                       ,derived_layer_id_ptr
                       ,linfo
                       );


   return (FALSE);
}  /* end p_check_orig_layer_up_to_date */



/* --------------------------------------
 * p_orig_layer_frame_fetcher
 * --------------------------------------
 */
static void
p_orig_layer_frame_fetcher(GapStbAttrWidget *attw
                    , gint32 master_framenr
                    , gint32 image_id
                    , gint32 *orig_layer_id_ptr
                    , gint32 *derived_layer_id_ptr
                    , GapStbAttrLayerInfo *linfo)
{
   gint32     l_layer_id;
   char      *l_filename;


   l_layer_id = -1;
   l_filename = NULL;

   {
     GapStoryLocateRet *stb_ret;
     GapStorySection   *section;

     section = gap_story_find_section_by_stb_elem(attw->stb_refptr, attw->stb_elem_refptr);
     stb_ret = gap_story_locate_expanded_framenr(section
                                    , master_framenr
                                    , attw->stb_elem_refptr->track
                                    );
     if(stb_ret)
     {
       if ((stb_ret->locate_ok)
       && (stb_ret->stb_elem))
       {
         gboolean is_up_to_date;

         l_filename = gap_story_get_filename_from_elem_nr_anim(stb_ret->stb_elem
                                                , stb_ret->ret_framenr
                                               );

         is_up_to_date = p_check_orig_layer_up_to_date(image_id
                             ,orig_layer_id_ptr
                             ,derived_layer_id_ptr
                             ,linfo
                             ,stb_ret
                             ,l_filename
                             );

         if(is_up_to_date)
         {
           g_free(stb_ret);
           if(l_filename)
           {
             g_free(l_filename);
           }

           return;  /* orig_layer is still up to date, done */
         }

         if(stb_ret->stb_elem->record_type == GAP_STBREC_VID_MOVIE)
         {
           if(l_filename)
           {
             /* fetch_full_sized_frame */
             l_layer_id = p_fetch_video_frame_as_layer(attw->sgpp
                ,l_filename
                ,stb_ret->ret_framenr
                ,stb_ret->stb_elem->seltrack
                ,gap_story_get_preferred_decoder(attw->stb_refptr
                                                 ,stb_ret->stb_elem
                                                 )
                ,image_id
                );
             if(l_layer_id > 0)
             {
                l_layer_id = gap_layer_flip(l_layer_id, stb_ret->stb_elem->flip_request);

                linfo->layer_record_type = stb_ret->stb_elem->record_type;
                linfo->layer_local_framenr = stb_ret->ret_framenr;
                linfo->layer_seltrack = stb_ret->stb_elem->seltrack;
                if (linfo->layer_filename != NULL)
                {
                  g_free(linfo->layer_filename);
                  linfo->layer_filename = g_strdup(l_filename);
                }
             }
           }
         }
         else if(stb_ret->stb_elem->record_type == GAP_STBREC_VID_COLOR)
         {
           l_layer_id = gimp_layer_new(image_id, "color"
                                 ,gimp_image_width(image_id)
                                 ,gimp_image_height(image_id)
                                 ,GIMP_RGBA_IMAGE
                                 ,100.0     /* Opacity */
                                 ,GIMP_NORMAL_MODE
                                 );
           gimp_image_insert_layer(image_id, l_layer_id, 0, LAYERSTACK_TOP);
           gap_layer_clear_to_color(l_layer_id
                                   , stb_ret->stb_elem->color_red
                                   , stb_ret->stb_elem->color_green
                                   , stb_ret->stb_elem->color_blue
                                   , stb_ret->stb_elem->color_alpha
                                   );

           linfo->layer_record_type = stb_ret->stb_elem->record_type;
           linfo->layer_local_framenr = 0;
           linfo->layer_seltrack = 1;
           if (linfo->layer_filename != NULL)
           {
             g_free(linfo->layer_filename);
             linfo->layer_filename = NULL;
           }

         }
         else if(stb_ret->stb_elem->record_type == GAP_STBREC_VID_ANIMIMAGE)
         {
           if(gap_debug)
           {
             printf("GAP_STBREC_VID_ANIMIMAGE: l_filename:%s ret_framenr:%d\n"
                 , l_filename
                 , stb_ret->ret_framenr
                 );
           }
           if(l_filename)
           {
             /* fetch_full_sized_frame */
             l_layer_id = p_fetch_layer_from_animimage(l_filename
                ,stb_ret->ret_framenr
                ,image_id
                );
             if(l_layer_id > 0)
             {
                l_layer_id = gap_layer_flip(l_layer_id, stb_ret->stb_elem->flip_request);

                linfo->layer_record_type = stb_ret->stb_elem->record_type;
                linfo->layer_local_framenr = stb_ret->ret_framenr;
                linfo->layer_seltrack = 1;
                if (linfo->layer_filename != NULL)
                {
                  g_free(linfo->layer_filename);
                  linfo->layer_filename = g_strdup(l_filename);
                }
             }
           }
         }
         else
         {
           /* record type is IMAGE or FRAME, fetch full size image
            */
           l_layer_id = p_fetch_imagefile_as_layer(l_filename
                            ,image_id
                            );
           if(l_layer_id > 0)
           {
              l_layer_id = gap_layer_flip(l_layer_id, stb_ret->stb_elem->flip_request);

              linfo->layer_record_type = stb_ret->stb_elem->record_type;
              linfo->layer_local_framenr = 0;
              linfo->layer_seltrack = 1;
              if (linfo->layer_filename != NULL)
              {
                g_free(linfo->layer_filename);
                linfo->layer_filename = g_strdup(l_filename);
              }
           }
         }
       }
       g_free(stb_ret);
     }
   }

   *orig_layer_id_ptr = l_layer_id;
   if(l_layer_id >= 0)
   {
     linfo->layer_is_fake = FALSE;
   }
   else
   {
     p_destroy_orig_layer(image_id
                       ,orig_layer_id_ptr
                       ,derived_layer_id_ptr
                       ,linfo
                       );
   }

   if(l_filename)
   {
     g_free(l_filename);
   }
}  /* end p_orig_layer_frame_fetcher */



/* ---------------------------------------
 * p_create_or_replace_orig_and_opre_layer
 * ---------------------------------------
 */
static gint32
p_calc_and_set_display_framenr(GapStbAttrWidget *attw
                         , long   img_idx
                         , gint32 duration)
{
   gint32     l_framenr_start;
   gint32     l_framenr;
   gint32     l_prefetch_framenr;
   gint32     l_dsiplay_framenr;
   GapStorySection   *section;

   section = gap_story_find_section_by_stb_elem(attw->stb_refptr, attw->stb_elem_refptr);


   /* calculate the absolute frame number for the frame to access */
   l_framenr_start = gap_story_get_framenr_by_story_id(section
                                                ,attw->stb_elem_refptr->story_id
                                                ,attw->stb_elem_refptr->track
                                                );
   /* stb_elem_refptr refers to the transition attribute stb_elem
    */
   l_framenr = l_framenr_start + MAX(0, (duration -1));
   l_prefetch_framenr = -1;
   l_dsiplay_framenr = l_framenr;

   if(attw->stb_elem_refptr->att_overlap > 0)
   {
     l_prefetch_framenr = CLAMP((l_framenr - attw->stb_elem_refptr->att_overlap), 0, l_framenr_start);

     if(l_prefetch_framenr > 0)
     {
       l_dsiplay_framenr = l_prefetch_framenr;
     }
   }
   p_update_framenr_labels(attw, img_idx, l_dsiplay_framenr);

   return (l_dsiplay_framenr);
}  /* end p_calc_and_set_display_framenr */


/* ---------------------------------------
 * p_create_or_replace_orig_and_opre_layer
 * ---------------------------------------
 * fetch the frame that is refered by the stb_elem_refptr + duration (frames)
 * If there is no frame found, use a default image.
 * (possible reasons: duration refers behind valid frame range,
 *  video support is turned off at compile time, but reference points
 *  into a movie clip)
 *
 * the fetched (or default) frame is added as invisible layer with alpha channel
 * on top of the layerstack in the specified image as orig_layer_id.
 * (an already existing orig_layer_id will be replaced if there exists one)
 *
 * If the orig_layer is up to date the fetch is not performed.
 *
 * if overlapping is used (att_overap > 0)
 * we prefetch a 2.nd original (the opre_layer) that shall be rendered
 * below the orig_layer. its position is current - att_overap
 *
 */
static void
p_create_or_replace_orig_and_opre_layer (GapStbAttrWidget *attw
                         , long   img_idx
                         , gint32 duration)
{
   gint32     l_framenr_start;
   gint32     l_framenr;
   gint32     l_prefetch_framenr;
   GapStorySection   *section;

   section = gap_story_find_section_by_stb_elem(attw->stb_refptr, attw->stb_elem_refptr);

   /* calculate the absolute expanded frame number for the frame to access
    * (we operate with expanded frame number, where the track overlapping
    * is ignored. this allows access to frames that otherwise were skipped due to
    * overlapping in the shadow track)
    */
   l_framenr_start = gap_story_get_expanded_framenr_by_story_id(section
                                                ,attw->stb_elem_refptr->story_id
                                                ,attw->stb_elem_refptr->track
                                                );
   /* stb_elem_refptr refers to the transition attribute stb_elem
    */
   l_framenr = l_framenr_start + MAX(0, (duration -1));
   l_prefetch_framenr = -1;

   if(attw->stb_elem_refptr->att_overlap > 0)
   {
     l_prefetch_framenr = l_framenr - attw->stb_elem_refptr->att_overlap;
   }

   p_calc_and_set_display_framenr(attw, img_idx, duration );

   /* OPRE_LAYER (re)creation */
   if(l_prefetch_framenr > 0)
   {
     p_orig_layer_frame_fetcher(attw
                      , l_prefetch_framenr
                      , attw->gfx_tab[img_idx].image_id
                      ,&attw->gfx_tab[img_idx].opre_layer_id   /* orig_layer_id_ptr */
                      ,&attw->gfx_tab[img_idx].pref_layer_id   /* derived_layer_id_ptr */
                      ,&attw->gfx_tab[img_idx].opre_info       /* linfo */
                      );
   }

   p_check_and_make_opre_default_layer(attw, img_idx);


   /* ORIG_LAYER (re)creation */
   p_orig_layer_frame_fetcher(attw
                    , l_framenr
                    , attw->gfx_tab[img_idx].image_id
                    ,&attw->gfx_tab[img_idx].orig_layer_id   /* orig_layer_id_ptr */
                    ,&attw->gfx_tab[img_idx].curr_layer_id   /* derived_layer_id_ptr */
                    ,&attw->gfx_tab[img_idx].orig_info       /* linfo */
                    );
   /* check if we have a valid orig layer, create it if have not
    * (includes setting invisible)
    */
   p_check_and_make_orig_default_layer(attw, img_idx);


}       /* end p_create_or_replace_orig_and_opre_layer */


/* ---------------------------------
 * p_fetch_video_frame_as_layer
 * ---------------------------------
 */
static gint32
p_fetch_video_frame_as_layer(GapStbMainGlobalParams *sgpp
                   , const char *video_filename
                   , gint32 framenumber
                   , gint32 seltrack
                   , const char *preferred_decoder
                   , gint32 image_id
                   )
{
  guchar *img_data;
  gint32 img_width;
  gint32 img_height;
  gint32 img_bpp;
  gboolean do_scale;
  gint32 l_new_layer_id;

  l_new_layer_id  = -1;
  do_scale = FALSE;

  /* Fetch the wanted Frame from the Videofile */
  img_data = gap_story_dlg_fetch_videoframe(sgpp
                   , video_filename
                   , framenumber
                   , seltrack
                   , preferred_decoder
                   , 1.5               /* delace */
                   , &img_bpp
                   , &img_width
                   , &img_height
                   , do_scale
                   );

  if(img_data != NULL)
  {
    GimpDrawable *drawable;
    GimpPixelRgn pixel_rgn;
    GimpImageType type;

    /* Fill in the alpha channel (for RGBA type) */
    if(img_bpp == 4)
    {
       gint i;

       for (i=(img_width * img_height)-1; i>=0; i--)
       {
         img_data[3+(i*4)] = 255;
       }
    }


    type = GIMP_RGB_IMAGE;
    if(img_bpp == 4)
    {
      type = GIMP_RGBA_IMAGE;
    }

    l_new_layer_id = gimp_layer_new (image_id
                                , LAYERNAME_ORIG
                                , img_width
                                , img_height
                                , type
                                , 100.0
                                , GIMP_NORMAL_MODE);

    drawable = gimp_drawable_get (l_new_layer_id);
    gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0
                      , drawable->width, drawable->height
                      , TRUE      /* dirty */
                      , FALSE     /* shadow */
                       );
    gimp_pixel_rgn_set_rect (&pixel_rgn, img_data
                          , 0
                          , 0
                          , img_width
                          , img_height);


    gimp_drawable_flush (drawable);
    gimp_drawable_detach(drawable);

    if(! gimp_drawable_has_alpha (l_new_layer_id))
    {
      /* implicitly add an alpha channel before we try to raise */
      gimp_layer_add_alpha(l_new_layer_id);
    }

    gimp_image_insert_layer (image_id,l_new_layer_id, 0, LAYERSTACK_TOP);
  }

  return(l_new_layer_id);

}  /* end p_fetch_video_frame_as_layer */


/* ---------------------------------
 * p_fetch_imagefile_as_layer
 * ---------------------------------
 * add the specified image filename as new layer on top of layerstack in the specified image_id.
 * The newly added layer is a copy of the composite view (e.g. a merge of its visible layers)
 */
static gint32
p_fetch_imagefile_as_layer (const char *img_filename
                   , gint32 image_id
                   )
{
  gint32 l_tmp_image_id;
  gint32 l_new_layer_id;

  l_new_layer_id  = -1;

  {
    char  *l_filename;
    l_filename = g_strdup(img_filename);
    l_tmp_image_id = gap_lib_load_image(l_filename);
    g_free(l_filename);
  }

  if(l_tmp_image_id >= 0)
  {
    gint32  l_layer_id;
    gint    l_src_offset_x, l_src_offset_y;

    gimp_image_undo_disable (l_tmp_image_id);
    l_layer_id = gap_image_merge_visible_layers(l_tmp_image_id, GIMP_CLIP_TO_IMAGE);

    if(!gimp_drawable_has_alpha(l_layer_id))
    {
      gimp_layer_add_alpha(l_layer_id);
    }
    gimp_layer_resize_to_image_size(l_layer_id);

    /* copy the layer from the temp image to the preview multilayer image */
    l_new_layer_id = gap_layer_copy_to_dest_image(image_id,
                                   l_layer_id,
                                   100.0,       /* opacity full */
                                   0,           /* NORMAL */
                                   &l_src_offset_x,
                                   &l_src_offset_y
                                   );

    gimp_image_insert_layer (image_id, l_new_layer_id, 0, LAYERSTACK_TOP);

    /* destroy the tmp image */
    gimp_image_delete(l_tmp_image_id);
  }

  return(l_new_layer_id);

}  /* end p_fetch_imagefile_as_layer */


/* ---------------------------------
 * p_fetch_layer_from_animimage
 * ---------------------------------
 * pick layer at specified localframe_index (e.g. layerstack_position) from specified
 * multilayer animimage on disk (specified via filename)
 * and add a copy of this layer as new layer on top of layerstack in the specified image_id.
 */
static gint32
p_fetch_layer_from_animimage (const char *img_filename
                   , gint32 localframe_index, gint32 image_id
                   )
{
  gint32 l_tmp_image_id;
  gint32 l_new_layer_id;

  l_new_layer_id  = -1;

  if(gap_debug)
  {
    printf("p_fetch_layer_from_animimage START localframe_index:%d img_filename:%s\n"
         ,(int)localframe_index
         ,img_filename
       );
  }



  {
    char  *l_filename;
    l_filename = g_strdup(img_filename);
    l_tmp_image_id = gap_lib_load_image(l_filename);
    g_free(l_filename);
  }

  if(l_tmp_image_id >= 0)
  {
    gint    l_src_offset_x, l_src_offset_y;
    gint          l_nlayers;
    gint32       *l_layers_list;

    gimp_image_undo_disable (l_tmp_image_id);

    l_layers_list = gimp_image_get_layers(l_tmp_image_id, &l_nlayers);

    if(gap_debug)
    {
      printf("p_fetch_layer_from_animimage l_nlayers:%d localframe_index:%d img_filename:%s\n"
         ,(int)l_nlayers
         ,(int)localframe_index
         ,img_filename
         );
    }


    if(l_layers_list != NULL)
    {
       if((localframe_index < l_nlayers)
       && (localframe_index >= 0))
       {
          gimp_item_set_visible(l_layers_list[localframe_index], TRUE);
          if (0 != gimp_layer_get_apply_mask(l_layers_list[localframe_index]))
          {
            /* the layer has an active mask, apply the mask now
             * because copying from the layer ignores the mask
             */
            gimp_layer_remove_mask(l_layers_list[localframe_index], GIMP_MASK_APPLY);
          }
          gimp_layer_resize_to_image_size(l_layers_list[localframe_index]);

          /* copy the picked layer from the temp image to the preview multilayer image */
          l_new_layer_id = gap_layer_copy_to_dest_image(image_id,
                                   l_layers_list[localframe_index],
                                   100.0,       /* opacity full */
                                   0,           /* NORMAL */
                                   &l_src_offset_x,
                                   &l_src_offset_y
                                   );

          gimp_image_insert_layer (image_id, l_new_layer_id, 0, LAYERSTACK_TOP);

       }
       g_free (l_layers_list);
    }

    /* destroy the tmp image */
    gimp_image_delete(l_tmp_image_id);
  }


  return(l_new_layer_id);
}  /* end p_fetch_layer_from_animimage */


/* ==================================================== START MOVEPATH FILESEL stuff ======  */
/* --------------------------------
 * p_attw_movepath_filesel_pw_ok_cb
 * --------------------------------
 */
static void
p_attw_movepath_filesel_pw_ok_cb (GtkWidget *widget
                   ,GapStbAttrWidget *attw)
{
  const gchar *movepathFilename;
  gchar *dup_movepathFilename;

  if(attw == NULL) return;
  if(attw->movepath_filesel == NULL) return;

  dup_movepathFilename = NULL;
  movepathFilename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (attw->movepath_filesel));
  if(movepathFilename)
  {
    dup_movepathFilename = g_strdup(movepathFilename);
  }

  gtk_widget_destroy(GTK_WIDGET(attw->movepath_filesel));

  if(dup_movepathFilename)
  {
    gtk_entry_set_text(GTK_ENTRY(attw->movepath_file_entry), dup_movepathFilename);
    g_free(dup_movepathFilename);
  }

  attw->movepath_filesel = NULL;
}  /* end p_attw_movepath_filesel_pw_ok_cb */


/* -----------------------------------
 * p_attw_movepath_filesel_pw_close_cb
 * -----------------------------------
 */
static void
p_attw_movepath_filesel_pw_close_cb ( GtkWidget *widget
                      , GapStbAttrWidget *attw)
{
  if(attw->movepath_filesel == NULL) return;

  gtk_widget_destroy(GTK_WIDGET(attw->movepath_filesel));
  attw->movepath_filesel = NULL;   /* indicate that filesel is closed */

}  /* end p_attw_movepath_filesel_pw_close_cb */



/* ---------------------------------
 * p_attw_movepath_filesel_button_cb
 * ---------------------------------
 */
static void
p_attw_movepath_filesel_button_cb ( GtkWidget *w
                       , GapStbAttrWidget *attw)
{
  GtkWidget *filesel = NULL;

  if(attw->attw_prop_dialog == NULL)
  {
     return;
  }

  if(attw->movepath_filesel != NULL)
  {
     gtk_window_present(GTK_WINDOW(attw->movepath_filesel));
     return;   /* filesel is already open */
  }
  if(attw->movepath_edit_dialog != NULL)
  {
     gtk_window_present(GTK_WINDOW(attw->movepath_edit_dialog));
     return;   /* edit dialog is already open */
  }
  if(attw->stb_elem_refptr == NULL)
  {
    return;
  }

  filesel = gtk_file_selection_new ( _("Set Movepath Parameterfile (XML)"));
  attw->movepath_filesel = filesel;

  gtk_window_set_position (GTK_WINDOW (filesel), GTK_WIN_POS_MOUSE);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->ok_button,
                    "clicked", G_CALLBACK (p_attw_movepath_filesel_pw_ok_cb),
                    attw);
  g_signal_connect (GTK_FILE_SELECTION (filesel)->cancel_button,
                    "clicked", G_CALLBACK (p_attw_movepath_filesel_pw_close_cb),
                    attw);
  g_signal_connect (filesel, "destroy",
                    G_CALLBACK (p_attw_movepath_filesel_pw_close_cb),
                    attw);


  if(attw->stb_elem_refptr->att_movepath_file_xml)
  {
    gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel),
                                     attw->stb_elem_refptr->att_movepath_file_xml);
  }
  gtk_widget_show (filesel);

}  /* end p_attw_movepath_filesel_button_cb */


/* ==================================================== END MOVEPATH FILESEL stuff ======  */

/* -----------------------------------------
 * p_create_movepath_edit_resources
 * -----------------------------------------
 * create frame_image, moving object image
 * ainfo and pvals resources that are used
 * to call the movepath edit dialog.
 */
static void
p_create_movepath_edit_resources(GapStbAttrWidget *attw)
{
  gint32   image_id;
  gint32   bg_layer_id;
  gint32   origsize_layer_id;

  /* create the frame image */
  image_id = gimp_image_new(attw->stb_refptr->master_width
                           ,attw->stb_refptr->master_height
                           ,GIMP_RGB
                           );
  attw->movepath_frame_image_id = image_id;
  gimp_image_undo_disable (image_id);

  /* add a transparent layer */
  bg_layer_id = gimp_layer_new(image_id
                  , "background"
                  , gimp_image_width(image_id)
                  , gimp_image_height(image_id)
                  , GIMP_RGBA_IMAGE
                  , 100.0   /* opacity */
                  , 0       /* normal mode */
                  );
  gimp_image_insert_layer (image_id, bg_layer_id, 0, 0);
  gap_layer_clear_to_color(bg_layer_id, 0.0, 0.0, 0.0, 0.0);
  gimp_item_set_visible(bg_layer_id, TRUE);

  // TODO: in case the storyboard has more tracks
  // the frame image should be rendered by the storyboard processor at master size
  // based on a modified storyboard that contains only tracks that render behind the current track


  /* create an image that holds the moving object layer */
  origsize_layer_id = attw->gfx_tab[0].orig_layer_id;
  attw->movepath_obj_image_id = gimp_image_new( gimp_drawable_width(origsize_layer_id)
                       , gimp_drawable_height(origsize_layer_id)
                       , GIMP_RGB
                       );
  gimp_image_undo_disable (attw->movepath_obj_image_id);
  attw->movepath_obj_layer_id = gimp_layer_new_from_drawable(origsize_layer_id, attw->movepath_obj_image_id);
  gimp_image_insert_layer (attw->movepath_obj_image_id, attw->movepath_obj_layer_id, 0, 0);
  gimp_item_set_visible(attw->movepath_obj_layer_id, TRUE);


  /* create default values for movepath
   * (will be overwritten in case xml_paramfile contains already valid settings)
   */
  attw->pvals = gap_mov_exec_new_GapMovValues();
  attw->pvals->dst_image_id = attw->movepath_frame_image_id;

  attw->ainfo_ptr = gap_lib_alloc_ainfo_unsaved_image(attw->movepath_frame_image_id);

}  /* end p_create_movepath_edit_resources */


/* -----------------------------------------
 * p_edit_movepath_closed_callback
 * -----------------------------------------
 * is called on close of the movepath edit dialog
 */
static void
p_edit_movepath_closed_callback(gpointer ptr)
{
  GapStbAttrWidget *attw;

  attw = (GapStbAttrWidget *)ptr;

  if(attw != NULL)
  {
    if(gap_debug)
    {
      printf("p_edit_movepath_closed_callback frame_image_id:%d obj_image_id:%d\n"
        ,(int)attw->movepath_frame_image_id
        ,(int)attw->movepath_obj_image_id
        );
    }
    attw->movepath_edit_dialog = NULL;

    if(attw->pvals != NULL)
    {
      gap_mov_exec_free_GapMovValues(attw->pvals);
      attw->pvals = NULL;
    }

    if(attw->ainfo_ptr != NULL)
    {
      gap_lib_free_ainfo(&attw->ainfo_ptr);
      attw->ainfo_ptr = NULL;
    }

    if(attw->movepath_frame_image_id >= 0)
    {
      gimp_image_delete(attw->movepath_frame_image_id);
      attw->movepath_frame_image_id = -1;
    }

    if(attw->movepath_obj_image_id >= 0)
    {
      gimp_image_delete(attw->movepath_obj_image_id);
      attw->movepath_obj_image_id = -1;
    }
    p_attw_movepath_file_validity_check(attw);
    p_update_full_preview_gfx(attw);

    /* make attributes dialog sensitive again (after movepath edit dialog was closed) */
    gtk_widget_set_sensitive(attw->attw_prop_dialog, TRUE);

  }

}  /* end p_edit_movepath_closed_callback */



/* ---------------------------------
 * p_attw_movepath_edit_button_cb
 * ---------------------------------
 * invoke the movepath editor dialog window
 * (in case it is not yet open
 * and the file selection dialog is not open)
 */
static void
p_attw_movepath_edit_button_cb ( GtkWidget *w
                       , GapStbAttrWidget *attw)
{
  gint32 nframes;

  if(attw->attw_prop_dialog == NULL)
  {
     return;
  }

  if(attw->movepath_filesel != NULL)
  {
     gtk_window_present(GTK_WINDOW(attw->movepath_filesel));
     return;   /* filesel is already open */
  }
  if(attw->stb_elem_refptr == NULL)
  {
    return;
  }

  if(attw->movepath_edit_dialog != NULL)
  {
     gtk_window_present(GTK_WINDOW(attw->movepath_edit_dialog));
     return;   /* edit dialog is already open */
  }

  if(attw->stb_elem_refptr->att_movepath_file_xml == NULL)
  {
    return;
  }
  if(attw->stb_elem_refptr->att_movepath_file_xml[0] == '\0')
  {
    return;
  }

  p_attw_push_undo_and_set_unsaved_changes(attw);
  p_create_movepath_edit_resources(attw);

  if(gap_debug)
  {
      printf("p_attw_movepath_edit_button_cb frame_image_id:%d obj_image_id:%d obj_layer_id:%d attw:%ld\n"
        ,(int)attw->movepath_frame_image_id
        ,(int)attw->movepath_obj_image_id
        ,(int)attw->movepath_obj_layer_id
        ,(long)attw
        );
  }


  /* make attributes dialog insensitive (while movepath edit dialog is open) */
  gtk_widget_set_sensitive(attw->attw_prop_dialog, FALSE);

  nframes = MAX(attw->stb_elem_refptr->att_arr_value_to[GAP_STB_ATT_TYPE_MOVEPATH]
               ,attw->stb_elem_refptr->att_arr_value_from[GAP_STB_ATT_TYPE_MOVEPATH]);

  attw->movepath_edit_dialog = gap_mov_dlg_edit_movepath_dialog(
                                   attw->movepath_frame_image_id
                                 , attw->movepath_obj_layer_id
                                 , attw->stb_elem_refptr->att_movepath_file_xml
                                 , attw->ainfo_ptr
                                 , attw->pvals
                                 , p_edit_movepath_closed_callback
                                 ,(gpointer)attw
                                 , nframes
                                 );

}  /* end p_attw_movepath_edit_button_cb */


/* ------------------------------------
 * p_attw_movepath_file_validity_check
 * ------------------------------------
 */
static void
p_attw_movepath_file_validity_check(GapStbAttrWidget *attw)
{
  if(attw == NULL) { return; }
  attw->movepath_file_xml_is_valid = FALSE;

  if(attw->stb_elem_refptr == NULL) { return; }


  attw->movepath_file_xml_is_valid = gap_mov_exec_check_valid_xml_paramfile(attw->stb_elem_refptr->att_movepath_file_xml);

  p_attw_update_sensitivity(attw);
  p_attw_update_properties(attw);

}  /* end p_attw_movepath_file_validity_check */


/* ------------------------------------
 * p_attw_movepath_file_entry_update_cb
 * ------------------------------------
 */
static void
p_attw_movepath_file_entry_update_cb(GtkWidget *widget, GapStbAttrWidget *attw)
{
  if(attw == NULL) { return; }
  if(attw->stb_elem_refptr == NULL) { return; }

  p_attw_push_undo_and_set_unsaved_changes(attw);

  if(attw->stb_elem_refptr->att_movepath_file_xml)
  {
    g_free(attw->stb_elem_refptr->att_movepath_file_xml);
  }
  attw->stb_elem_refptr->att_movepath_file_xml = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

  p_attw_movepath_file_validity_check(attw);

}  /* end p_attw_movepath_file_entry_update_cb */


/* ------------------------------
 * p_attw_comment_entry_update_cb
 * ------------------------------
 */
static void
p_attw_comment_entry_update_cb(GtkWidget *widget, GapStbAttrWidget *attw)
{
  if(attw == NULL) { return; }
  if(attw->stb_elem_refptr == NULL) { return; }

  p_attw_push_undo_and_set_unsaved_changes(attw);

  if(attw->stb_elem_refptr->comment == NULL)
  {
    attw->stb_elem_refptr->comment = gap_story_new_elem(GAP_STBREC_VID_COMMENT);
  }

  if(attw->stb_elem_refptr->comment)
  {
    if(attw->stb_elem_refptr->comment->orig_src_line)
    {
      g_free(attw->stb_elem_refptr->comment->orig_src_line);
    }
    attw->stb_elem_refptr->comment->orig_src_line = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
  }
}  /* end p_attw_comment_entry_update_cb */


/* ---------------------------------------
 * p_init_layer_info
 * ---------------------------------------
 */
static void
p_init_layer_info(GapStbAttrLayerInfo *linfo)
{
  linfo->layer_record_type = GAP_STBREC_VID_UNKNOWN;
  linfo->layer_local_framenr = -1;
  linfo->layer_seltrack = -1;
  linfo->layer_filename = NULL;
}  /* end p_init_layer_info */


/* -----------------------------------
 * p_create_and_attach_pv_widgets
 * -----------------------------------
 * creates preview widgets
 */
static void
p_create_and_attach_pv_widgets(GapStbAttrWidget *attw
   , GtkWidget *table
   , gint row
   , gint col_start
   , gint col_end
   , glong img_idx
  )
{
  GtkWidget *vbox2;
  GtkWidget *hbox;
  GtkWidget *alignment;
  GtkWidget *check_button;
  GtkWidget *label;
  GtkWidget *event_box;
  GtkWidget *aspect_frame;
  gint32     l_check_size;
  gdouble    thumb_scale;


  /* the vox2  */
  vbox2 = gtk_vbox_new (FALSE, 1);

  /* aspect_frame is the CONTAINER for the video preview */
  aspect_frame = gtk_aspect_frame_new (NULL   /* without label */
                                      , 0.5   /* xalign center */
                                      , 0.5   /* yalign center */
                                      , attw->stb_refptr->master_width / attw->stb_refptr->master_height     /* ratio */
                                      , TRUE  /* obey_child */
                                      );
  gtk_widget_show (aspect_frame);

  /* Create an EventBox (container for the preview drawing_area)
   */
  event_box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (event_box), vbox2);
  gtk_widget_set_events (event_box, GDK_BUTTON_PRESS_MASK);


  /*  The preview  */
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox2), alignment, FALSE, FALSE, 1);
  gtk_widget_show (alignment);

  /* calculate scale factor (the preview has same proportions as the master
   * and fits into a sqare of size PVIEW_SIZE x PVIEW_SIZE)
   */
  {
    gdouble master_size;

    master_size = (gdouble)MAX(attw->stb_refptr->master_width, attw->stb_refptr->master_height);
    thumb_scale = (gdouble)PVIEW_SIZE / master_size;
  }
  l_check_size = PVIEW_SIZE / 32;
  attw->gfx_tab[img_idx].pv_ptr = gap_pview_new( (thumb_scale * (gdouble)attw->stb_refptr->master_width) + 4.0
                            , (thumb_scale * (gdouble)attw->stb_refptr->master_height) + 4.0
                            , l_check_size
                            , aspect_frame
                            );

  gtk_widget_set_events (attw->gfx_tab[img_idx].pv_ptr->da_widget, GDK_EXPOSURE_MASK);
  gtk_container_add (GTK_CONTAINER (aspect_frame), attw->gfx_tab[img_idx].pv_ptr->da_widget);
  gtk_container_add (GTK_CONTAINER (alignment), aspect_frame);
  gtk_widget_show (attw->gfx_tab[img_idx].pv_ptr->da_widget);

  /* create a gimp image that is internally used for rendering the preview
   * (create no view for this image, to keep it invisible for the user)
   */
  p_create_gfx_image(attw, img_idx);

  g_object_set_data(G_OBJECT(attw->gfx_tab[img_idx].pv_ptr->da_widget)
                   , OBJ_DATA_KEY_IMG_IDX, (gpointer)img_idx);
  g_object_set_data(G_OBJECT(event_box)
                   , OBJ_DATA_KEY_IMG_IDX, (gpointer)img_idx);

  g_signal_connect (G_OBJECT (attw->gfx_tab[img_idx].pv_ptr->da_widget), "event",
                    G_CALLBACK (p_attw_preview_events_cb),
                    attw);
  g_signal_connect (G_OBJECT (event_box), "button_press_event",
                    G_CALLBACK (p_attw_preview_events_cb),
                    attw);




  hbox = gtk_hbox_new (FALSE, 1);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 1);

  /* auto update toggle  check button */
  check_button = gtk_check_button_new_with_label (_("Update"));
  attw->gfx_tab[img_idx].auto_update_toggle = check_button;
  gtk_box_pack_start (GTK_BOX (hbox), check_button, FALSE, FALSE, 1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button)
      , attw->gfx_tab[img_idx].auto_update);
  gimp_help_set_help_data(check_button, _("automatic update using the referred frame"), NULL);
  gtk_widget_show(check_button);

  g_object_set_data(G_OBJECT(check_button), OBJ_DATA_KEY_ATTW, attw);

  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_attw_auto_update_toggle_update_callback),
                    &attw->gfx_tab[img_idx].auto_update);

  /* the framenr label  */
  label = gtk_label_new ("000000");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 1);
  gtk_widget_show (label);
  attw->gfx_tab[img_idx].framenr_label = label;

  /* the framenr label  */
  label = gtk_label_new ("mm:ss:msec");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 1);
  gtk_widget_show (label);
  attw->gfx_tab[img_idx].frametime_label = label;


  gtk_widget_show(vbox2);
  gtk_widget_show (event_box);
  gtk_table_attach ( GTK_TABLE (table), event_box, col_start, col_end, row, row+1, GTK_FILL, 0, 0, 0);

}  /* end p_create_and_attach_pv_widgets */


/* -----------------------------------
 * p_create_and_attach_att_arr_widgets
 * -----------------------------------
 * creates widgets for attribute array values (enable, from, to duration)
 * and attaches them to the specified table in the specified row.
 *
 * Notes: attribute values for the from and to spinbuttons are multiplied by 100
 *        because the internal representation uses the value 1.0
 *        for 100 percent.
 */
static void
p_create_and_attach_att_arr_widgets(const char *row_title
   , GapStbAttrWidget *attw
   , GtkWidget *table
   , gint row
   , gint column
   , glong att_type_idx
   , gdouble lower_constraint
   , gdouble upper_constraint
   , gdouble step_increment
   , gdouble page_increment
   , gdouble page_size
   , gint    digits
   , const char *enable_tooltip
   , gboolean   *att_arr_enable_ptr
   , const char *from_tooltip
   , gdouble    *att_arr_value_from_ptr
   , const char *to_tooltip
   , gdouble    *att_arr_value_to_ptr
   , const char *dur_tooltip
   , gint32     *att_arr_value_dur_ptr
   , const char *accel_tooltip
   , gint32     *att_arr_value_accel_ptr
  )
{
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *check_button;
  GtkObject *adj;
  GtkWidget *spinbutton;
  gint      col;
  gdouble   convertFactor;

  col = column;
  convertFactor = p_getConvertFactor(att_type_idx);
  /* enable label */
  label = gtk_label_new(row_title);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE (table), label, col, col+1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  col++;

  /* enable check button */
  check_button = gtk_check_button_new_with_label (" ");
  attw->att_rows[att_type_idx].enable_toggle = check_button;
  gtk_table_attach ( GTK_TABLE (table), check_button, col, col+1, row, row+1, GTK_FILL, 0, 0, 0);
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), *att_arr_enable_ptr);

  }
  gimp_help_set_help_data(check_button, enable_tooltip, NULL);
  gtk_widget_show(check_button);

  g_object_set_data(G_OBJECT(check_button), OBJ_DATA_KEY_ATTW, attw);

  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_attw_enable_toggle_update_callback),
                    att_arr_enable_ptr);


  col++;

  /* from start label */
  button = gtk_button_new_with_label(_("Start:"));
  attw->att_rows[att_type_idx].button_from = button;
  gtk_table_attach(GTK_TABLE (table), button, col, col+1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gimp_help_set_help_data(button, _("Reset to: defaults, "
                                    "use modifier keys CTRL, ALT for alternative defaults. "
                                    "SHIFT resets to initial value"), NULL);
  gtk_widget_show(button);

  g_object_set_data(G_OBJECT(button), OBJ_DATA_KEY_ATTW, attw);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (p_attw_start_button_clicked_callback),
                    (gpointer)att_type_idx);

  col++;

  /* From (Start value of transition) button */
  adj = gtk_adjustment_new ( *att_arr_value_from_ptr * convertFactor
                           , lower_constraint
                           , upper_constraint
                           , step_increment
                           , page_increment
                           , page_size
                           );
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, digits);
  attw->att_rows[att_type_idx].spinbutton_from_adj = adj;
  attw->att_rows[att_type_idx].spinbutton_from = spinbutton;

  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table), spinbutton, col, col+1, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (spinbutton, 80, -1);
  gimp_help_set_help_data (spinbutton, from_tooltip, NULL);

  g_object_set_data(G_OBJECT(adj), "att_type_idx", (gpointer)att_type_idx);
  g_object_set_data(G_OBJECT(adj), OBJ_DATA_KEY_ATTW, attw);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                      G_CALLBACK (p_attw_gdouble_adjustment_callback),
                      att_arr_value_from_ptr);

  col++;

  /* to (end value of transition) button */
  button = gtk_button_new_with_label(_("End:"));
  attw->att_rows[att_type_idx].button_to = button;
  gtk_table_attach(GTK_TABLE (table), button, col, col+1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gimp_help_set_help_data(button, _("Reset to: defaults, "
                                    "use modifier keys CTRL, ALT for alternative defaults. "
                                    "SHIFT resets to initial value"), NULL);
  gtk_widget_show(button);

  g_object_set_data(G_OBJECT(button), OBJ_DATA_KEY_ATTW, attw);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (p_attw_end_button_clicked_callback),
                    (gpointer)att_type_idx);

  col++;

  /* the To value spinbutton */
  adj = gtk_adjustment_new ( *att_arr_value_to_ptr * convertFactor
                           , lower_constraint
                           , upper_constraint
                           , step_increment
                           , page_increment
                           , page_size
                           );
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, digits);
  attw->att_rows[att_type_idx].spinbutton_to_adj = adj;
  attw->att_rows[att_type_idx].spinbutton_to = spinbutton;

  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table), spinbutton, col, col+1, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (spinbutton, 80, -1);
  gimp_help_set_help_data (spinbutton, to_tooltip, NULL);

  g_object_set_data(G_OBJECT(adj), "att_type_idx", (gpointer)att_type_idx);
  g_object_set_data(G_OBJECT(adj), OBJ_DATA_KEY_ATTW, attw);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                      G_CALLBACK (p_attw_gdouble_adjustment_callback),
                      att_arr_value_to_ptr);


  col++;

  /* Frames Duration button */
  button = gtk_button_new_with_label(_("Frames:"));
  attw->att_rows[att_type_idx].button_dur = button;
  gtk_table_attach(GTK_TABLE (table), button, col, col+1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gimp_help_set_help_data(button, _("Copy this number of frames to all enabled rows"), NULL);
  gtk_widget_show(button);

  g_object_set_data(G_OBJECT(button), OBJ_DATA_KEY_ATTW, attw);
  g_signal_connect (G_OBJECT (button), "button_press_event",
                    G_CALLBACK (p_attw_dur_button_clicked_callback),
                    (gpointer)att_type_idx);

  col++;

  /* the Duration value spinbutton */
  adj = gtk_adjustment_new ( *att_arr_value_dur_ptr
                           , 0
                           , GAP_STB_ATT_MAX_DUR
                           , 1
                           , 10
                           , 0
                           );
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  attw->att_rows[att_type_idx].spinbutton_dur_adj = adj;
  attw->att_rows[att_type_idx].spinbutton_dur = spinbutton;

  gtk_widget_show (spinbutton);
  gtk_table_attach (GTK_TABLE (table), spinbutton, col, col+1, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (spinbutton, 60, -1);
  gimp_help_set_help_data (spinbutton, _("Number of frames (duration of transition from start to end value)"), NULL);

  g_object_set_data(G_OBJECT(adj), OBJ_DATA_KEY_ATTW, attw);
  g_signal_connect (G_OBJECT (adj), "value_changed",
                      G_CALLBACK (p_attw_duration_adjustment_callback),
                      att_arr_value_dur_ptr);

  col++;

  /* the time duration label  */
  label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, col, col+1, row, row+1);
  gtk_widget_show (label);
  attw->att_rows[att_type_idx].dur_time_label = label;

  col++;


  /* the acceleration characteristic graph display widget */
  {
#define ACC_WGT_WIDTH 32
#define ACC_WGT_HEIGHT 22

    gint32           accelerationCharacteristic;
    GapAccelWidget  *accel_wgt;

    accelerationCharacteristic = *att_arr_value_accel_ptr;
    accel_wgt = gap_accel_new(ACC_WGT_WIDTH, ACC_WGT_HEIGHT, accelerationCharacteristic);

    /* the Acceleration characteristic value spinbutton */
    adj = accel_wgt->adj;

    spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
    attw->att_rows[att_type_idx].spinbutton_accel_adj = adj;
    attw->att_rows[att_type_idx].spinbutton_accel = spinbutton;

    gtk_widget_show (spinbutton);
    gtk_table_attach (GTK_TABLE (table), spinbutton, col+1, col+2, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
    gtk_widget_set_size_request (spinbutton, 50, -1);
    gimp_help_set_help_data (spinbutton, accel_tooltip, NULL);

    g_object_set_data(G_OBJECT(adj), OBJ_DATA_KEY_ATTW, attw);

    gtk_table_attach( GTK_TABLE(table), accel_wgt->da_widget, col, col+1, row, row+1,
                        GTK_FILL, 0, 4, 0 );
    gtk_widget_show (accel_wgt->da_widget);



    g_signal_connect (G_OBJECT (adj), "value_changed",
                      G_CALLBACK (p_attw_accel_adjustment_callback),
                      att_arr_value_accel_ptr);

  }




}  /* end p_create_and_attach_att_arr_widgets */


/* --------------------------------
 * gap_story_attw_properties_dialog
 * --------------------------------
 */
GtkWidget *
gap_story_attw_properties_dialog (GapStbAttrWidget *attw)
{
  GtkWidget *dlg;
  GtkWidget *main_vbox;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *check_button;
  gint       row;
  GapStbTabWidgets *tabw;

  if(attw == NULL) { return (NULL); }
  if(attw->attw_prop_dialog != NULL) { return(NULL); }   /* is already open */

  tabw = (GapStbTabWidgets *)attw->tabw;
  if(tabw == NULL) { return (NULL); }

  attw->movepath_filesel = NULL;
  attw->movepath_edit_dialog = NULL;
  attw->ainfo_ptr = NULL;
  attw->pvals = NULL;
  attw->movepath_frame_image_id = -1;
  attw->movepath_obj_image_id = -1;
  attw->movepath_obj_layer_id = -1;

  if(attw->stb_elem_bck)
  {
    dlg = gimp_dialog_new (_("Transition Attributes"), "gap_story_attr_properties"
                         ,NULL, 0
                         ,gimp_standard_help_func, GAP_STORY_ATTR_PROP_HELP_ID

                         ,GAP_STOCK_PLAY,   GAP_STORY_ATT_RESPONSE_PLAYBACK
                         ,GIMP_STOCK_RESET, GAP_STORY_ATT_RESPONSE_RESET
                         ,GTK_STOCK_CLOSE,  GTK_RESPONSE_CLOSE
                         ,NULL);
  }
  else
  {
    dlg = gimp_dialog_new (_("Transition Attributes"), "gap_story_attr_properties"
                         ,NULL, 0
                         ,gimp_standard_help_func, GAP_STORY_ATTR_PROP_HELP_ID

                         ,GAP_STOCK_PLAY,   GAP_STORY_ATT_RESPONSE_PLAYBACK
                         ,GTK_STOCK_CLOSE,  GTK_RESPONSE_CLOSE
                         ,NULL);
  }
  gtk_window_set_type_hint (GTK_WINDOW(dlg), GDK_WINDOW_TYPE_HINT_NORMAL);

  attw->attw_prop_dialog = dlg;

  g_signal_connect (G_OBJECT (dlg), "response",
                    G_CALLBACK (p_attw_prop_response),
                    attw);


  main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);

  /*  the frame  */
  frame = gimp_frame_new (_("Properties"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  table = gtk_table_new (16, 10, FALSE);
  attw->master_table = table;
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  row = 0;


  /* the fit size label  */
  label = gtk_label_new (_("FitSize:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);

  /* the fit width check button */
  check_button = gtk_check_button_new_with_label (_("Width"));
  attw->fit_width_toggle = check_button;
  gtk_table_attach ( GTK_TABLE (table), check_button, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button)
      , attw->stb_elem_refptr->att_fit_width);

  }
  gimp_help_set_help_data(check_button, _("scale width of frame to fit master width"), NULL);
  gtk_widget_show(check_button);

  g_object_set_data(G_OBJECT(check_button), OBJ_DATA_KEY_ATTW, attw);

  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_attw_enable_toggle_update_callback),
                    &attw->stb_elem_refptr->att_fit_width);


  /* the fit height check button */
  check_button = gtk_check_button_new_with_label (_("Height"));
  attw->fit_height_toggle = check_button;
  gtk_table_attach ( GTK_TABLE (table), check_button, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button)
      , attw->stb_elem_refptr->att_fit_height);

  }
  gimp_help_set_help_data(check_button, _("scale height of frame to fit master height"), NULL);
  gtk_widget_show(check_button);

  g_object_set_data(G_OBJECT(check_button), OBJ_DATA_KEY_ATTW, attw);

  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_attw_enable_toggle_update_callback),
                    &attw->stb_elem_refptr->att_fit_height);


  /* the keep proportions check button */
  check_button = gtk_check_button_new_with_label (_("Keep Proportion"));
  attw->keep_proportions_toggle = check_button;
  gtk_table_attach ( GTK_TABLE (table), check_button, 3, 5, row, row+1, GTK_FILL, 0, 0, 0);
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button)
      , attw->stb_elem_refptr->att_keep_proportions);

  }
  gimp_help_set_help_data(check_button, _("ON: keep proportions at scaling. "
                                          " (this may result in black borders)"
                                          "OFF: allow changes of image proportions at scaling"), NULL);
  gtk_widget_show(check_button);

  g_object_set_data(G_OBJECT(check_button), OBJ_DATA_KEY_ATTW, attw);

  g_signal_connect (G_OBJECT (check_button), "toggled",
                    G_CALLBACK (p_attw_enable_toggle_update_callback),
                    &attw->stb_elem_refptr->att_keep_proportions);


  /* overlap attributes */
  {
    GtkObject *adj;
    GtkWidget *spinbutton;
    GtkWidget *button;

    /* the overlap label (same row as FitSize) */
    label = gtk_label_new (_("Overlap:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.9, 0.5);
    gtk_table_attach_defaults (GTK_TABLE(table), label, 5, 6, row, row+1);
    gtk_widget_show (label);


    /* Frames Overlap duration button */
    button = gtk_button_new_with_label(_("Frames:"));
    attw->button_overlap_dur = button;
    gtk_table_attach(GTK_TABLE (table), button, 6, 7, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
    gimp_help_set_help_data(button, _("Copy this number of frames to all enabled rows"), NULL);
    gtk_widget_show(button);

    g_signal_connect (G_OBJECT (button), "button_press_event",
                      G_CALLBACK (p_attw_overlap_dur_button_clicked_callback),
                      (gpointer)attw);


    /* the frames overlap duration value spinbutton */
    adj = gtk_adjustment_new ( attw->stb_elem_refptr->att_overlap
                             , 0
                             , 999999
                             , 1
                             , 10
                             , 0
                             );
    spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
    attw->spinbutton_overlap_dur_adj = adj;
    attw->spinbutton_overlap_dur = spinbutton;

    gtk_widget_show (spinbutton);
    gtk_table_attach (GTK_TABLE (table), spinbutton, 7, 8, row, row+1,
                      (GtkAttachOptions) (0),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_widget_set_size_request (spinbutton, 60, -1);
    gimp_help_set_help_data (spinbutton, _("Number of overlapping frames within this track"), NULL);

    g_object_set_data(G_OBJECT(adj), OBJ_DATA_KEY_ATTW, attw);
    g_signal_connect (G_OBJECT (adj), "value_changed",
                        G_CALLBACK (p_attw_duration_adjustment_callback),
                        &attw->stb_elem_refptr->att_overlap);
  }  /* end overlap attributes */

  row++;

  {
    glong att_type_idx;
    gint col = 0;


    att_type_idx = GAP_STB_ATT_TYPE_ROTATE;
    p_create_and_attach_att_arr_widgets(_("Rotate:")
      , attw
      , table
      , row
      , col
      , att_type_idx
      , -36000.0    /* lower constraint for the from/to values */
      ,  36000.0    /* upper constraint for the from/to values */
      , 1.0        /* step increment   for the from/to values  */
      , 45.0       /* page increment   for the from/to values */
      , 0.0        /* page size        for the from/to values */
      , 1          /* digits for the from/to values */
      , _("ON: Enable rotation settings")
      , &attw->stb_elem_refptr->att_arr_enable[att_type_idx]
      , _("rotation value in degree for the first handled frame ")
      , &attw->stb_elem_refptr->att_arr_value_from[att_type_idx]
      , _("rotation value in degree for the last handled frame ")
      , &attw->stb_elem_refptr->att_arr_value_to[att_type_idx]
      , _("number of frames")
      , &attw->stb_elem_refptr->att_arr_value_dur[att_type_idx]
      , _("acceleration characteristic for rotation (1 for constant speed, positive: acceleration, negative: deceleration)")
      , &attw->stb_elem_refptr->att_arr_value_accel[att_type_idx]
      );

    row++;
    att_type_idx = GAP_STB_ATT_TYPE_OPACITY;
    p_create_and_attach_att_arr_widgets(_("Opacity:")
      , attw
      , table
      , row
      , col
      , att_type_idx
      , 0.0      /* lower constraint for the from/to values */
      , 100.0    /* upper constraint for the from/to values */
      , 1.0      /* step increment   for the from/to values  */
      , 10.0     /* page increment   for the from/to values */
      , 0.0      /* page size        for the from/to values */
      , 0        /* digits for the from/to values */
      , _("ON: Enable opacity settings")
      , &attw->stb_elem_refptr->att_arr_enable[att_type_idx]
      , _("opacity value for the first handled frame "
          "where 100 is fully opaque, 0 is fully transparent")
      , &attw->stb_elem_refptr->att_arr_value_from[att_type_idx]
      , _("opacity value for the last handled frame "
          "where 100 is fully opaque, 0 is fully transparent")
      , &attw->stb_elem_refptr->att_arr_value_to[att_type_idx]
      , _("number of frames")
      , &attw->stb_elem_refptr->att_arr_value_dur[att_type_idx]
      , _("acceleration characteristic for opacity (1 for constant speed, positive: acceleration, negative: deceleration)")
      , &attw->stb_elem_refptr->att_arr_value_accel[att_type_idx]
      );


    row++;
    att_type_idx = GAP_STB_ATT_TYPE_MOVE_X;
    p_create_and_attach_att_arr_widgets(_("Move X:")
      , attw
      , table
      , row
      , col
      , att_type_idx
      , -1000.0    /* lower constraint for the from/to values */
      ,  1000.0    /* upper constraint for the from/to values */
      , 1.0        /* step increment   for the from/to values  */
      , 10.0       /* page increment   for the from/to values */
      , 0.0        /* page size        for the from/to values */
      , 2          /* digits for the from/to values */
      , _("ON: Enable move horizontal settings")
      , &attw->stb_elem_refptr->att_arr_enable[att_type_idx]
      , _("move horizontal value for the first handled frame "
          "where 0.0 is centered, 100.0 is outside right, -100.0 is outside left")
      , &attw->stb_elem_refptr->att_arr_value_from[att_type_idx]
      , _("move horizontal value for the last handled frame "
          "where 0.0 is centered, 100.0 is outside right, -100.0 is outside left")
      , &attw->stb_elem_refptr->att_arr_value_to[att_type_idx]
      , _("number of frames")
      , &attw->stb_elem_refptr->att_arr_value_dur[att_type_idx]
      , _("acceleration characteristic for horizontal move (1 for constant speed, positive: acceleration, negative: deceleration)")
      , &attw->stb_elem_refptr->att_arr_value_accel[att_type_idx]
      );


    row++;
    att_type_idx = GAP_STB_ATT_TYPE_MOVE_Y;
    p_create_and_attach_att_arr_widgets(_("Move Y:")
      , attw
      , table
      , row
      , col
      , att_type_idx
      , -1000.0    /* lower constraint for the from/to values */
      ,  1000.0    /* upper constraint for the from/to values */
      , 1.0        /* step increment   for the from/to values  */
      , 10.0       /* page increment   for the from/to values */
      , 0.0        /* page size        for the from/to values */
      , 2          /* digits for the from/to values */
      , _("ON: Enable move vertical settings")
      , &attw->stb_elem_refptr->att_arr_enable[att_type_idx]
      , _("move vertical value for the first handled frame "
          "where 0.0 is centered, 100.0 is outside at bottom, -100.0 is outside at top")
      , &attw->stb_elem_refptr->att_arr_value_from[att_type_idx]
      , _("move vertical value for the last handled frame "
          "where 0.0 is centered, 100.0 is outside at bottom, -100.0 is outside at top")
      , &attw->stb_elem_refptr->att_arr_value_to[att_type_idx]
      , _("number of frames")
      , &attw->stb_elem_refptr->att_arr_value_dur[att_type_idx]
      , _("acceleration characteristic for vertical move (1 for constant speed, positive: acceleration, negative: deceleration)")
      , &attw->stb_elem_refptr->att_arr_value_accel[att_type_idx]
      );

    row++;
    att_type_idx = GAP_STB_ATT_TYPE_ZOOM_X;
    p_create_and_attach_att_arr_widgets(_("Scale Width:")
      , attw
      , table
      , row
      , col
      , att_type_idx
      , 0.0    /* lower constraint for the from/to values */
      , 1000.0 /* upper constraint for the from/to values */
      , 1.0    /* step increment   for the from/to values  */
      , 10.0   /* page increment   for the from/to values */
      , 0.0    /* page size        for the from/to values */
      , 2      /* digits for the from/to values */
      , _("ON: Enable scale width settings")
      , &attw->stb_elem_refptr->att_arr_enable[att_type_idx]
      , _("scale width value for the first handled frame"
          " where 100 is 1:1, 50 is half, 200 is double width")
      , &attw->stb_elem_refptr->att_arr_value_from[att_type_idx]
      , _("scale width value for the last handled frame"
          " where 100 is 1:1, 50 is half, 200 is double width")
      , &attw->stb_elem_refptr->att_arr_value_to[att_type_idx]
      , _("number of frames")
      , &attw->stb_elem_refptr->att_arr_value_dur[att_type_idx]
      , _("acceleration characteristic for scale width (1 for constant speed, positive: acceleration, negative: deceleration)")
      , &attw->stb_elem_refptr->att_arr_value_accel[att_type_idx]
      );

    row++;
    att_type_idx = GAP_STB_ATT_TYPE_ZOOM_Y;
    p_create_and_attach_att_arr_widgets(_("Scale Height:")
      , attw
      , table
      , row
      , col
      , att_type_idx
      , -1000.0    /* lower constraint for the from/to values */
      ,  1000.0    /* upper constraint for the from/to values */
      , 1.0        /* step increment   for the from/to values  */
      , 10.0       /* page increment   for the from/to values */
      , 0.0        /* page size        for the from/to values */
      , 2          /* digits for the from/to values */
      , _("ON: Enable scale height settings")
      , &attw->stb_elem_refptr->att_arr_enable[att_type_idx]
      , _("scale height value for the first handled frame"
          " where 100 is 1:1, 50 is half, 200 is double height")
      , &attw->stb_elem_refptr->att_arr_value_from[att_type_idx]
      , _("scale height value for the last handled frame"
          " where 100 is 1:1, 50 is half, 200 is double height")
      , &attw->stb_elem_refptr->att_arr_value_to[att_type_idx]
      , _("number of frames")
      , &attw->stb_elem_refptr->att_arr_value_dur[att_type_idx]
      , _("acceleration characteristic for scale height (1 for constant speed, positive: acceleration, negative: deceleration)")
      , &attw->stb_elem_refptr->att_arr_value_accel[att_type_idx]
      );

    row++;
    att_type_idx = GAP_STB_ATT_TYPE_MOVEPATH;
    p_create_and_attach_att_arr_widgets(_("Move Path:")
      , attw
      , table
      , row
      , col
      , att_type_idx
      ,  1.0       /* lower constraint for the from/to values */
      ,  GAP_STB_ATT_MAX_MOVPATH_DBL   /* upper constraint for the from/to values */
      , 1.0        /* step increment   for the from/to values  */
      , 10.0       /* page increment   for the from/to values */
      , 0.0        /* page size        for the from/to values */
      , 0          /* digits for the from/to values */
      , _("ON: Enable move path transitions using settings provided via a movepath parameter file")
      , &attw->stb_elem_refptr->att_arr_enable[att_type_idx]
      , _("frame number (phase) of the movement/transition along path for the first handled frame"
          " where 1 is the begin of the path using settings of the 1st controlpoint"
          " in the movepath parameter file")
      , &attw->stb_elem_refptr->att_arr_value_from[att_type_idx]
      , _("frame number (phase) of the movement/transition along path for the last handled frame."
          " note that frame numbers higher than (or equal to) total frames in the movepath parameter file"
          " uses settings of the last controlpoint in this file.")
      , &attw->stb_elem_refptr->att_arr_value_to[att_type_idx]
      , _("number of frames")
      , &attw->stb_elem_refptr->att_arr_value_dur[att_type_idx]
      , _("acceleration characteristic (currently ignored)")
      , &attw->stb_elem_refptr->att_arr_value_accel[att_type_idx]
      );

  }


  row++;

  {
    GtkWidget *gfx_table;

    gfx_table = gtk_table_new (1, 3, FALSE);
    gtk_widget_show (gfx_table);

    gtk_table_attach ( GTK_TABLE (table), gfx_table, 1, 8, row, row+1, GTK_FILL, 0, 0, 0);

    /* the graphical preview(s) of the attributes at start and end */
    p_create_and_attach_pv_widgets(attw
       , gfx_table
       , 1   /* row */
       , 0   /* col_start */
       , 1   /* col_end */
       , 0   /* img_idx */
      );
    label = gtk_label_new (" ");
    gtk_widget_show (label);
    gtk_table_attach ( GTK_TABLE (gfx_table), label, 1, 2, 1, 1+1, GTK_FILL, 0, 0, 0);

    p_create_and_attach_pv_widgets(attw
       , gfx_table
       , 1   /* row */
       , 2   /* col_start */
       , 3   /* col_end */
       , 1   /* img_idx */
      );
  }

  row++;


  {
    GtkWidget *button;


    /* the movepath label */
    label = gtk_label_new (_("Movepath File:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
    gtk_widget_show (label);


    /* the movepath entry */
    entry = gtk_entry_new ();
    gtk_widget_set_size_request(entry, ATTW_COMMENT_WIDTH, -1);
    if(attw->stb_elem_refptr)
    {
      if(attw->stb_elem_refptr->att_movepath_file_xml)
      {
        gtk_entry_set_text(GTK_ENTRY(entry), attw->stb_elem_refptr->att_movepath_file_xml);
      }
    }
    gtk_table_attach_defaults (GTK_TABLE(table), entry, 1, 8, row, row+1);
    g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK(p_attw_movepath_file_entry_update_cb),
                     attw);
    gtk_widget_show (entry);
    attw->movepath_file_entry = entry;

    /* the filesel invoker button */
    button = gtk_button_new_with_label ("...");
    gtk_table_attach_defaults (GTK_TABLE(table), button, 8, 10, row, row+1);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(p_attw_movepath_filesel_button_cb),
                     attw);
    gtk_widget_show (button);


    /* the movepath record/edit dialog invoker button */
    button = gtk_button_new_with_label ("edit");
    attw->movepath_edit_button = button;
    gtk_table_attach_defaults (GTK_TABLE(table), button, 10, 11, row, row+1);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(p_attw_movepath_edit_button_cb),
                     attw);
    gtk_widget_show (button);

  }

  row++;


  /* the comment label */
  label = gtk_label_new (_("Comment:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE(table), label, 0, 1, row, row+1);
  gtk_widget_show (label);


  /* the comment entry */
  entry = gtk_entry_new ();
  gtk_widget_set_size_request(entry, ATTW_COMMENT_WIDTH, -1);
  if(attw->stb_elem_refptr)
  {
    if(attw->stb_elem_refptr->comment)
    {
      if(attw->stb_elem_refptr->comment->orig_src_line)
      {
        gtk_entry_set_text(GTK_ENTRY(entry), attw->stb_elem_refptr->comment->orig_src_line);
      }
    }
  }
  gtk_table_attach_defaults (GTK_TABLE(table), entry, 1, 8, row, row+1);
  g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK(p_attw_comment_entry_update_cb),
                     attw);
  gtk_widget_show (entry);
  attw->comment_entry = entry;

  p_attw_movepath_file_validity_check(attw);
  p_attw_update_sensitivity(attw);

  /*  Show the main containers  */
  gtk_widget_show (main_vbox);

  return(dlg);
}  /* end gap_story_attw_properties_dialog */


/* ----------------------------------------
 * gap_story_att_stb_elem_properties_dialog
 * ----------------------------------------
 */
void
gap_story_att_stb_elem_properties_dialog ( GapStbTabWidgets *tabw
                                     , GapStoryElem *stb_elem
                                     , GapStoryBoard *stb_dst)
{
  GapStbAttrWidget *attw;
  gint ii;

  /* check if already open */
  for(attw=tabw->attw; attw!=NULL; attw=(GapStbAttrWidget *)attw->next)
  {
    if ((attw->attw_prop_dialog == NULL) || (attw->stb_elem_refptr == NULL))
    {
      /* we found a dead element (that is already closed)
       * reuse that element to open a new clip properties dialog window
       */
      break;
    }
    else
    {
      if(attw->stb_elem_refptr->story_id == stb_elem->story_id)
      {
        /* Properties for the selected element already open
         * bring the window to front
         */
        gtk_window_present(GTK_WINDOW(attw->attw_prop_dialog));
        return ;
      }
    }
  }

  if(attw==NULL)
  {
    attw = g_new(GapStbAttrWidget ,1);
    attw->next = tabw->attw;
    tabw->attw = attw;
    attw->stb_elem_bck = NULL;
  }
  if(attw->stb_elem_bck)
  {
    gap_story_elem_free(&attw->stb_elem_bck);
  }
  attw->stb_elem_bck = NULL;
  attw->attw_prop_dialog = NULL;
  attw->stb_elem_refptr = stb_elem;
  attw->stb_refptr = stb_dst;
  attw->sgpp = tabw->sgpp;
  attw->tabw = tabw;
  attw->go_timertag = -1;
  attw->timer_full_update_request = FALSE;
  for (ii=0; ii < GAP_STB_ATT_GFX_ARRAY_MAX; ii++)
  {
    attw->gfx_tab[ii].auto_update = FALSE;
    p_init_layer_info(&attw->gfx_tab[ii].opre_info);
    p_init_layer_info(&attw->gfx_tab[ii].orig_info);
  }

  attw->gfx_tab[1].auto_update = FALSE;
  attw->close_flag = FALSE;

  if(stb_elem)
  {
    attw->stb_elem_bck = gap_story_elem_duplicate(stb_elem);
    if(stb_elem->comment)
    {
      if(stb_elem->comment->orig_src_line)
      {
        attw->stb_elem_bck->comment = gap_story_elem_duplicate(stb_elem->comment);
      }
    }
  }


  attw->attw_prop_dialog = gap_story_attw_properties_dialog(attw);
  if(attw->attw_prop_dialog)
  {
    gtk_widget_show(attw->attw_prop_dialog);

    /* initial rendering (must be done after pview widgets are realised) */
    p_render_gfx(attw, 0);
    p_render_gfx(attw, 1);

  }
}  /* end gap_story_att_stb_elem_properties_dialog */




/* ----------------------------------
 * gap_story_att_fw_properties_dialog
 * ----------------------------------
 */
void
gap_story_att_fw_properties_dialog (GapStbFrameWidget *fw)
{
  GapStbTabWidgets *tabw;

  if(fw == NULL) { return; }
  if(fw->stb_elem_refptr == NULL)  { return; }

  /* type check, this dialog handles only transition and fit size attributes */
  if (fw->stb_elem_refptr->record_type == GAP_STBREC_ATT_TRANSITION)
  {
    tabw = (GapStbTabWidgets *)fw->tabw;
    if(tabw == NULL)  { return; }

    gap_story_att_stb_elem_properties_dialog(tabw, fw->stb_elem_refptr, fw->stb_refptr);
  }
}  /* end gap_story_att_fw_properties_dialog */

