/*  gap_player_dialog.c
 *
 *  video (preview) playback of animframes  by Wolfgang Hofer (hof)
 *     supports both (fast) thumbnail based playback
 *     and full image playback (slow)
 *  the current implementation has NO audio support.
 *  2003/06/11
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Revision history
 *  (2003/07/31)  v1.3.17b   hof: message text fixes for translators (# 118392)
 *  (2003/06/26)  v1.3.16a   hof: bugfix: make preview drawing_area fit into frame (use an aspect_frame)
 *                                query gimprc for "show-tool-tips"
 *  (2003/06/21)  v1.3.15a   hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_player_main.h"
#include "gap_player_dialog.h"
#include "gap_pdb_calls.h"
#include "gap_pview_da.h"
#include "gap_stock.h"
#include "gap_lib.h"
#include "gap_vin.h"
#include "gap_timeconv.h"
#include "gap_thumbnail.h"

#include "gap-intl.h"


extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */

#define GAP_PLAYER_MIN_SIZE 64
#define GAP_PLAYER_MAX_SIZE 1024

#define GAP_PLAYER_CHECK_SIZE 6
#define GAP_PLAY_MAX_GOBUTTONS 51
#define GAP_PLAYER_MIDDLE_GO_NUMBER ((GAP_PLAY_MAX_GOBUTTONS / 2))

typedef struct t_gobutton
{
  t_global_params *gpp;
  gint             go_number;
} t_gobutton;


/* the callbacks */
static void   on_shell_window_destroy                (GtkObject       *object,
                                                      gpointer         user_data);

static void   on_from_spinbutton_changed             (GtkEditable     *editable,
                                                     gpointer         user_data);

static void   on_to_spinbutton_changed               (GtkEditable     *editable,
                                                       gpointer         user_data);



static gboolean on_vid_preview_button_press_event    (GtkWidget       *widget,
                                                      GdkEventButton  *bevent,
                                                      gpointer         user_data);
static gboolean on_vid_preview_expose_event          (GtkWidget       *widget,
                                                      GdkEventExpose  *eevent,
                                                      gpointer         user_data);
static void   on_vid_preview_size_allocate           (GtkWidget       *widget,
                                                      GtkAllocation   *allocation,
                                                      gpointer         user_data);

static void   on_framenr_button_clicked              (GtkButton       *button,
                                                       gpointer         user_data);

static void   on_framenr_spinbutton_changed          (GtkEditable     *editable,
                                                      gpointer         user_data);


static void   on_origspeed_button_clicked            (GtkButton       *button,
                                                      gpointer         user_data);
static void   on_speed_spinbutton_changed            (GtkEditable     *editable,
                                                      gpointer         user_data);


static gboolean   on_size_button_button_press_event  (GtkWidget       *widget,
                                                      GdkEventButton  *event,
                                                      gpointer         user_data);



static void   on_size_spinbutton_changed             (GtkEditable     *editable,
                                                      gpointer         user_data);
static gboolean   on_size_spinbutton_enter           (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      gpointer         user_data);
static gboolean   on_size_spinbutton_leave           (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      gpointer         user_data);


static void   on_exact_timing_checkbutton_toggled    (GtkToggleButton *togglebutton,
                                                      gpointer         user_data);

static void   on_use_thumb_checkbutton_toggled       (GtkToggleButton *togglebutton,
                                                      gpointer         user_data);

static void   on_pinpong_checkbutton_toggled         (GtkToggleButton *togglebutton,
                                                      gpointer         user_data);

static void   on_selonly_checkbutton_toggled         (GtkToggleButton *togglebutton,
                                                      gpointer         user_data);

static void   on_loop_checkbutton_toggled            (GtkToggleButton *togglebutton,
                                                      gpointer         user_data);

static void   on_play_button_clicked                (GtkButton       *button,
                                                     gpointer         user_data);

static gboolean on_pause_button_press_event         (GtkButton       *button,
                                                     GdkEventButton  *bevent,
                                                     gpointer         user_data);

static void   on_back_button_clicked                (GtkButton       *button,
                                                     gpointer         user_data);

static void   on_close_button_clicked               (GtkButton       *button,
                                                     gpointer         user_data);


static void   on_timer_playback                      (gpointer   user_data);

static void   on_timer_go_job                   (gpointer   user_data);

static void   on_go_button_clicked                   (GtkButton       *button,
                                                      gpointer         user_data);

static gboolean   on_go_button_enter                 (GtkButton       *button,
                                                      GdkEvent        *event,
                                                      gpointer         user_data);

static void   on_gobutton_hbox_leave                 (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      gpointer         user_data);



static void    p_update_position_widgets(t_global_params *gpp);
static void    p_stop_playback(t_global_params *gpp);

/* -----------------------------
 * p_check_tooltips
 * -----------------------------
 */
static void
p_check_tooltips(void)
{
  char *value_string;
  
  value_string = gimp_gimprc_query("show-tool-tips");
  
  if(value_string != NULL)
  {
    if (strcmp(value_string, "no") == 0)
    {
       gimp_help_disable_tooltips ();
    }
    else
    {
       gimp_help_enable_tooltips ();
    }
  }
  else
  {
       gimp_help_enable_tooltips ();
  }
  
}  /* end p_check_tooltips */

/* -----------------------------
 * p_reload_ainfo_ptr
 * -----------------------------
 */
static void
p_reload_ainfo_ptr(t_global_params *gpp, gint32 image_id)
{
  gpp->image_id = image_id;
  
  if(gpp->ainfo_ptr)  { p_free_ainfo(&gpp->ainfo_ptr); }

  gpp->ainfo_ptr = p_alloc_ainfo(gpp->image_id, gpp->run_mode);
  if(gpp->ainfo_ptr == NULL)
  {
    return;
  }
  if (0 == p_dir_ainfo(gpp->ainfo_ptr))
  {
    if(0 == p_chk_framerange(gpp->ainfo_ptr))
    {
      t_video_info *vin_ptr;
      vin_ptr = p_get_video_info(gpp->ainfo_ptr->basename);

      gpp->ainfo_ptr->width  = gimp_image_width(gpp->image_id);
      gpp->ainfo_ptr->height = gimp_image_height(gpp->image_id);

      if(vin_ptr)
      {
        gpp->original_speed = vin_ptr->framerate;
      }
      if(vin_ptr)
      {
        g_free(vin_ptr);
      }
    }
  }
  
}  /* end p_reload_ainfo_ptr */

/* -----------------------------
 * p_update_ainfo_dependent_widgets
 * -----------------------------
 */
static void
p_update_ainfo_dependent_widgets(t_global_params *gpp)
{
  gdouble l_lower;
  gdouble l_upper;
  
  if(gpp == NULL) { return; }
  if(gpp->ainfo_ptr == NULL) { return; }
  
  l_lower = (gdouble)gpp->ainfo_ptr->first_frame_nr;
  l_upper = (gdouble)gpp->ainfo_ptr->last_frame_nr;
  
  GTK_ADJUSTMENT(gpp->from_spinbutton_adj)->lower = l_lower;
  GTK_ADJUSTMENT(gpp->from_spinbutton_adj)->upper = l_upper;
  GTK_ADJUSTMENT(gpp->from_spinbutton_adj)->value = CLAMP(GTK_ADJUSTMENT(gpp->from_spinbutton_adj)->value
                                                         ,l_lower, l_upper);
  
  GTK_ADJUSTMENT(gpp->to_spinbutton_adj)->lower = l_lower;
  GTK_ADJUSTMENT(gpp->to_spinbutton_adj)->upper = l_upper;
  GTK_ADJUSTMENT(gpp->to_spinbutton_adj)->value = CLAMP(GTK_ADJUSTMENT(gpp->to_spinbutton_adj)->value
                                                       ,l_lower, l_upper);
  
  GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)->lower = l_lower;
  GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)->upper = l_upper;
  GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)->value = CLAMP(GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)->value
                                                            ,l_lower, l_upper);
  
}  /* end p_update_ainfo_dependent_widgets */

 
/* ------------------------------------
 * p_check_image_is_alive
 * ------------------------------------
 * TODO: gimp 1.3.14 keeps a copy of closed images
 *       therefore this proceedure may tell only half the truth
 *
 * return TRUE  if OK (image is still valid)
 * return FALSE if image is NOT valid
 */
static gboolean
p_check_image_is_alive(gint32 image_id)
{
  gint32 *images;
  gint    nimages;
  gint    l_idi;
  gint    l_found;

  /*if(gap_debug) printf("p_check_image_is_alive: image_id %d START\n", (int)image_id); */

  if(image_id < 0)
  {
     return FALSE;
  }

  images = gimp_image_list(&nimages);
  l_idi = nimages -1;
  l_found = FALSE;
  while((l_idi >= 0) && images)
  {
    if(image_id == images[l_idi])
    {
          l_found = TRUE;
          break;
    }
    l_idi--;
  }
  if(images) g_free(images);
  if(l_found)
  {
    return TRUE;  /* OK */
  }

  if(gap_debug) printf("p_check_image_is_alive: image_id %d is not VALID\n", (int)image_id);
 
  return FALSE ;   /* INVALID image id */
}  /* end p_check_image_is_alive */

/* ------------------------------------
 * p_find_master_image_id
 * ------------------------------------
 * try to find the master image by filename 
 * matching at basename and extension part
 *
 * return positive image_id  on success
 * return -1 if nothing found
 */
static gint32
p_find_master_image_id(t_global_params *gpp)
{
  gint32 *images;
  gint    nimages;
  gint    l_idi;
  gint    l_baselen;
  gint    l_extlen;
  gint32  l_found_image_id;

  if(gpp->ainfo_ptr == NULL) { return -1; }
  if(gpp->ainfo_ptr->basename == NULL) { return -1; }

  if(gap_debug)
  {
    printf("p_find_master_image_id: image_id %s %s START\n"
           , gpp->ainfo_ptr->basename
           , gpp->ainfo_ptr->extension
           );
  }

  l_baselen = strlen(gpp->ainfo_ptr->basename);
  l_extlen = strlen(gpp->ainfo_ptr->extension);

  images = gimp_image_list(&nimages);
  l_idi = nimages -1;
  l_found_image_id = -1;
  while((l_idi >= 0) && images)
  {
    gchar *l_filename;
    
    l_filename = gimp_image_get_filename(images[l_idi]);
    if(l_filename)
    {
      if(gap_debug) printf("p_find_master_image_id: comare with %s\n", l_filename);
      
      if(strncmp(l_filename, gpp->ainfo_ptr->basename, l_baselen) == 0)
      {
         gint l_len;
         
         l_len = strlen(l_filename);
         if(l_len > l_extlen)
         {
            if(strncmp(&l_filename[l_len - l_extlen], gpp->ainfo_ptr->extension, l_extlen) == 0)
            {
              l_found_image_id = images[l_idi];
              break;
            }
         }
      }
      g_free(l_filename);
    }
    l_idi--;
  }
  if(images) g_free(images);

  return l_found_image_id;

}  /* end p_find_master_image_id */


/* ------------------------------------
 * p_keep_track_of_active_master_image
 * ------------------------------------
 */
static void
p_keep_track_of_active_master_image(t_global_params *gpp)
{
  p_stop_playback(gpp);
  
  gpp->image_id = p_find_master_image_id(gpp);
  if(gpp->image_id >= 0)
  {
    p_reload_ainfo_ptr(gpp, gpp->image_id);
    p_update_ainfo_dependent_widgets(gpp);
  }
  else
  {
    /* cannot find the master image, so quit immediate */
    printf("p_keep_track_of_active_master_image: Master Image not found (may have been closed)\n");
    printf("Exiting Playback\n");
    on_shell_window_destroy(NULL, gpp);
  }
}  /* end p_keep_track_of_active_master_image */



/* -----------------------------
 * p_check_for_active_image
 * -----------------------------
 * check if framenr is the the active image
 * (from where we were called at plug-in invocation time)
 *
 * this procedure also tries to keep track of changes
 * of the active master image_id outside this plug-in.
 *
 * (stepping to another frame, using other GAP plug-ins
 *  changes the master image_id outside ...)
 */
gboolean
p_check_for_active_image(t_global_params *gpp, gint32 framenr)
{
  
  if (p_check_image_is_alive(gpp->image_id))
  {
    if(framenr == gpp->ainfo_ptr->curr_frame_nr)
    {
      return(TRUE);
    }
  }
  else
  {
    p_keep_track_of_active_master_image(gpp); 
    if(framenr == gpp->ainfo_ptr->curr_frame_nr)
    {
       return(TRUE);
    }
    return (FALSE);
  }
  
  return (FALSE);

}  /* end p_check_for_active_image */



/* -----------------------------
 * p_update_pviewsize
 * -----------------------------
 * set preview size nd size spinbutton
 */
static void
p_update_pviewsize(t_global_params *gpp)
{
  /*
   * Resize the greater one of dwidth and dheight to PREVIEW_SIZE
   */
  if ( gpp->ainfo_ptr->width > gpp->ainfo_ptr->height )
  {
    /* landscape */
    gpp->pv_height = gpp->ainfo_ptr->height * gpp->pv_pixelsize / gpp->ainfo_ptr->width;
    gpp->pv_width = gpp->pv_pixelsize;
  }
  else
  {
    /* portrait */
    gpp->pv_width = gpp->ainfo_ptr->width * gpp->pv_pixelsize / gpp->ainfo_ptr->height;
    gpp->pv_height = gpp->pv_pixelsize;
  }

  if(gpp->pv_pixelsize != (gint32)GTK_ADJUSTMENT(gpp->size_spinbutton_adj)->value)
  { 
    gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->size_spinbutton_adj)
                            , (gfloat)gpp->pv_pixelsize
                            );
  }
                                                       
  gpp->pv_pixelsize = (gint32)GTK_ADJUSTMENT(gpp->size_spinbutton_adj)->value;

  p_pview_set_size(gpp->pv_ptr
                  , gpp->pv_width
                  , gpp->pv_height
                  , MAX(GAP_PLAYER_CHECK_SIZE, (gpp->pv_pixelsize / 16))
                  );

} /* end p_update_pviewsize */


/* -----------------------------
 * p_update_position_widgets
 * -----------------------------
 * set position spinbutton and time entry
 */
static void
p_update_position_widgets(t_global_params *gpp)
{
  static gchar time_txt[12];

  if((gint32)GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)->value != gpp->play_current_framenr)
  {
    gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)
                            , (gfloat)gpp->play_current_framenr
                            );                            
  }
  p_conv_framenr_to_timestr( (gpp->play_current_framenr - gpp->ainfo_ptr->first_frame_nr)
                           , gpp->original_speed
                           , time_txt
                           , sizeof(time_txt)
                           );

  gtk_label_set_text ( GTK_LABEL(gpp->timepos_label), time_txt);
}  /* end p_update_position_widgets */


/* -----------------------------
 * p_start_playback_timer
 * -----------------------------
 * this procedure sets up the delay
 * until next timercall.
 */
static void
p_start_playback_timer(t_global_params *gpp)
{
  gint32 delay_until_next_timercall_millisec;
  gdouble cycle_time_secs;

  cycle_time_secs = 1.0 / MAX(gpp->speed, 1.0);
  if(cycle_time_secs != gpp->cycle_time_secs)
  {
     /* playback speed has changed while playing
      * reset timing (restart playing with new speed on the fly)
      * use a half delay, as guess after speed has changed
      */
     gpp->rest_secs = cycle_time_secs / 2.0;
     gpp->delay_secs = 0.0;                  /* reset the absolute delay (since start or speedchange) */
     gpp->framecnt = 0.0;
     g_timer_start(gpp->gtimer);  /* (re)start timer at start of playback (== reset to 0) */
  }
  else
  {
    if(gpp->rest_secs < 0)
    {
       /* use a minimal delay, bacause we are LATE */;
      gpp->rest_secs = 10.0 / 1000.0; 
      gpp->framecnt = 0.0;
      g_timer_start(gpp->gtimer);  /* (re)start timer at start of playback (== reset to 0) */
    }
  }

  gpp->cycle_time_secs = cycle_time_secs;
  delay_until_next_timercall_millisec =  (gpp->rest_secs) * 1000.0;
  
  /*if(gap_debug) printf("p_start_playback_timer: START delay_until_next_timercall_millisec:%d\n", (int)delay_until_next_timercall_millisec);*/

  gpp->play_is_active = TRUE;
  gpp->play_timertag = (gint32) g_timeout_add(delay_until_next_timercall_millisec,
                                        (GtkFunction)on_timer_playback, gpp);
}  /* end p_start_playback_timer */


/* -----------------------------
 * p_initial_start_playback_timer
 * -----------------------------
 */
static void
p_initial_start_playback_timer(t_global_params *gpp)
{
  gpp->cycle_time_secs = 1.0 / MAX(gpp->speed, 1.0);
  gpp->rest_secs = 10.0 / 1000.0;   /* use minimal delay for the 1st call */
  gpp->delay_secs = 0.0;            /* absolute delay (for display) */
  gpp->framecnt = 0.0;
  gpp->go_job_framenr = -1;         /* pending timer_go_job gets useless, since we start playback now  */
  
  gtk_label_set_text ( GTK_LABEL(gpp->status_label), _("Playing"));

  g_timer_start(gpp->gtimer);  /* (re)start timer at start of playback (== reset to 0) */
  p_start_playback_timer(gpp);

}  /* end p_initial_start_playback_timer */


/* -----------------------------
 * p_remove_play_timer
 * -----------------------------
 */
static void
p_remove_play_timer(t_global_params *gpp)
{
  /*if(gap_debug) printf("p_remove_play_timer\n");*/

  if(gpp->play_timertag >= 0)
  {
    g_source_remove(gpp->play_timertag);
  }
  gpp->play_timertag = -1;
}  /* end p_remove_play_timer */


/* -----------------------------
 * p_stop_playback
 * -----------------------------
 */
static void
p_stop_playback(t_global_params *gpp)
{
  /* if(gap_debug) printf("p_stop_playback\n"); */
  p_remove_play_timer(gpp);
  gpp->play_is_active = FALSE;
  gpp->pingpong_count = 0;

  gtk_label_set_text ( GTK_LABEL(gpp->status_label), _("Ready"));

  p_check_tooltips();
}  /* end p_stop_playback */


#define PREVIEW_BPP 3


/* ------------------------------
 * p_display_frame
 * ------------------------------
 * display framnr from thumbnail or from full image
 * the active image (the one from where we were invoked)
 * is not read from discfile to reflect actual changes.
 */
static void
p_display_frame(t_global_params *gpp, gint32 framenr)
{
  char *l_filename;
   gint32  l_th_width;
   gint32  l_th_height;
   gint32  l_th_data_count;
   gint32  l_th_bpp;
   guchar *l_th_data;
   gboolean framenr_is_the_active_image;

  /*if(gap_debug) printf("p_display_frame START: framenr:%d\n", (int)framenr);*/

  l_th_data = NULL;
  l_th_bpp = 3;
  
  l_filename = p_alloc_fname(gpp->ainfo_ptr->basename, framenr, gpp->ainfo_ptr->extension);

  framenr_is_the_active_image = p_check_for_active_image(gpp, framenr);

  if(gpp->use_thumbnails)
  {
    if(framenr_is_the_active_image)
    {
       p_gimp_image_thumbnail(gpp->image_id
                          , gpp->pv_width
                          , gpp->pv_height
                          , &l_th_width
                          , &l_th_height
                          , &l_th_bpp
			  , &l_th_data_count
                          , &l_th_data
                          );
    }
    else
    {
      p_gimp_file_load_thumbnail(l_filename
                                , &l_th_width, &l_th_height
                                , &l_th_data_count, &l_th_data);
    }
  }

  if (l_th_data)
  {
    gboolean l_th_data_was_grabbed;

    l_th_data_was_grabbed = p_pview_render_from_buf (gpp->pv_ptr
                 , l_th_data
                 , l_th_width
                 , l_th_height
                 , l_th_bpp
                 , TRUE         /* allow_grab_src_data */
                 );
    if(l_th_data_was_grabbed)
    {
      /* the p_pview_render_from_buf procedure can grab the l_th_data
       * instead of making a ptivate copy for later use on repaint demands.
       * if such a grab happened it returns TRUE.
       * (this is done for optimal performance reasons)
       * in such a case the caller must NOT free the src_data (l_th_data) !!!
       */
      l_th_data = NULL;
    }

  }
  else
  {
    gint32  l_image_id;
     
    /* got no thumbnail data, must use the full image */
    if(framenr_is_the_active_image)
    {
      l_image_id = gimp_image_duplicate(gpp->image_id);
    }
    else
    {
      l_image_id = p_load_image(l_filename);

      if (l_image_id < 0)
      {
        /* could not read the image
         * one reason could be, that frames were deleted while this plugin is active
         * so we stop playback,
         * and try to reload informations about all frames
         */
        if(gap_debug) printf("LOAD IMAGE_ID: %s failed\n", l_filename);
        p_keep_track_of_active_master_image(gpp);
      }
    }
    
    /* there is no need for undo on this scratch image
     * so we turn undo off for performance reasons
     */
    gimp_image_undo_disable (l_image_id);
    
    p_pview_render_from_image (gpp->pv_ptr, l_image_id);
    if(gpp->use_thumbnails)
    {
      p_cond_gimp_file_save_thumbnail(l_image_id, l_filename);
    }
    gimp_image_delete(l_image_id);
  }
  

  gpp->play_current_framenr = framenr;
  p_update_position_widgets(gpp);

  gdk_flush();

  if(l_th_data)  g_free(l_th_data);

  g_free(l_filename);

}  /* end p_display_frame */


/* ------------------------------
 * p_get_next_framenr_in_sequence
 * ------------------------------
 */
gint32
p_get_next_framenr_in_sequence(t_global_params *gpp)
{
  gint32 l_first;
  gint32 l_last;

  l_first = gpp->ainfo_ptr->first_frame_nr;
  l_last  = gpp->ainfo_ptr->last_frame_nr;
  if(gpp->play_selection_only)
  {
    l_first = gpp->begin_frame;
    l_last  = gpp->end_frame;
  }


  if(gpp->play_backward)
  {
    if(gpp->play_current_framenr <= l_first)
    {
      if(gpp->play_loop)
      {
        if(gpp->play_pingpong)
        {
          gpp->play_current_framenr = l_first + 1;
          gpp->play_backward = FALSE;
          gpp->pingpong_count++;
        }
        else
        {
          gpp->play_current_framenr = l_last;
        }
      }
      else
      {
        if((gpp->play_pingpong) && (gpp->pingpong_count <= 0))
        {
          gpp->play_current_framenr = l_first + 1;
          gpp->play_backward = FALSE;
          gpp->pingpong_count++;
        }
        else
        {
          gpp->pingpong_count = 0;
          return -1;  /* STOP if first frame reached */
        }
      }
    }
    else
    {
      gpp->play_current_framenr--;
    }
  }
  else
  {
    if(gpp->play_current_framenr >= l_last)
    {
      if(gpp->play_loop)
      {
        if(gpp->play_pingpong)
        {
          gpp->play_current_framenr = l_last - 1;
          gpp->play_backward = TRUE;
          gpp->pingpong_count++;
        }
        else
        {
          gpp->play_current_framenr = l_first;
        }
      }
      else
      {
        if((gpp->play_pingpong) && (gpp->pingpong_count <= 0))
        {
          gpp->play_current_framenr = l_last - 1;
          gpp->play_backward = TRUE;
          gpp->pingpong_count++;
        }
        else
        {
          gpp->pingpong_count = 0;
          return -1;  /* STOP if last frame reached */
        }
      }
    }
    else
    {
      gpp->play_current_framenr++;
    }
  }
  
  gpp->play_current_framenr = CLAMP(gpp->play_current_framenr, l_first, l_last);
  return (gpp->play_current_framenr);
}  /* end p_get_next_framenr_in_sequence */


/* ------------------------------
 * p_framenr_from_go_number
 * ------------------------------
 */
gint32
p_framenr_from_go_number(t_global_params *gpp, gint32 go_number)
{
  /*
  if(gap_debug) printf("p_framenr_from_go_number go_base_framenr: %d  go_base:%d go_number:%d curr_frame:%d\n"
     , (int)gpp->go_base_framenr
     , (int)gpp->go_base
     , (int)go_number
     , (int)gpp->play_current_framenr );
  */

  if((gpp->go_base_framenr < 0) || (gpp->go_base < 0))
  {
    gpp->go_base_framenr = gpp->play_current_framenr;
    gpp->go_base = go_number;
  }

  /* printf("p_framenr_from_go_number: result framenr:%d\n", (int)(gpp->go_base_framenr  + (go_number - gpp->go_base)) ); */

  return CLAMP(gpp->go_base_framenr  + (go_number - gpp->go_base)
                     , gpp->ainfo_ptr->first_frame_nr
                     , gpp->ainfo_ptr->last_frame_nr
                     );
}  /* end p_framenr_from_go_number */


/* ------------------
 * on_timer_playback
 * ------------------
 * This timer callback is called periodical in intervall depending of playback speed.
 * (the playback timer is completely removed while not playing)
 */
static void
on_timer_playback(gpointer   user_data)
{
  t_global_params *gpp;
  gulong elapsed_microsecs;
  gdouble elapsed_secs;
  static char  status_txt[30];

  /*if(gap_debug) printf("\non_timer_playback: START\n");*/
  gpp = (t_global_params*)user_data;
  if(gpp)
  {
    p_remove_play_timer(gpp);
    if(gpp->in_timer_playback)
    {
      /* if speed is too fast the timer may fire again, while still processing
       * the previous callback. in that case return without any action
       * NOTE: at test with gtk2.2.1 this line of CODE was never reached,
       *       even if the frame is too late.
       *       the check if in_timer_playback is done just to be at the safe side.
       *       (late frames are detected by checking the elapsed time).
       */
      printf("\n\n\n  on_timer_playback interrupted by next TIMERCALL \n\n");
      return;
    }
    gpp->in_timer_playback = TRUE;

    if(gpp->play_is_active)
    {
       gint ii;
       gint ii_max;
       gint32 l_prev_framenr;
       gint32 l_framenr = -1;
       gboolean l_frame_dropped;
       gdouble  l_delay;
       
       l_framenr = -1;
       l_frame_dropped = FALSE;
       l_delay = gpp->delay_secs;
       
       ii_max = (gpp->exact_timing) ? 20 : 2;
       
       /* - if we have exact timing handle (or drop) upto max 20 frames in one timercallback
        * - if your machine is fast enough to display the frame at gpp->speed
        *   this loop will display only one frame. (the ideal case)
        * - if NOT fast enough, but the frame is just a little late,
        *   it tries to display upto (3) frames without restarting the timer.
        *   (and without giving control back to the gtk main loop)
        * - if we are still late at this point
        *   we begin to drop  upto (20) frames to get back in exact time.
        * - if this does not help either, we give up
        *   (results in NON exact timing, regardless to the exact_timing flag).
        */
       for(ii=1; ii < ii_max; ii++)
       {
         l_prev_framenr = l_framenr;
         l_framenr = p_get_next_framenr_in_sequence(gpp);
         gpp->framecnt += 1.0;  /* count each handled frame (for timing purpose) */

         /*if(gap_debug) printf("on_timer_playback[%d]: l_framenr:%d\n", (int)ii, (int)l_framenr);*/

         if(l_framenr < 0)
         {
           if((l_frame_dropped) && (l_prev_framenr > 0))
           {
             /* the last frame in sequence (l_prev_framenr) would be DROPPED
              * to keep exact timing, but now it is time to STOP
              * in this case we display that frame.
              */
             p_display_frame(gpp, l_prev_framenr);
           }
           p_stop_playback(gpp);  /* STOP at end of sequence */
           break;
         }
         else
         {
           if((ii > 3)
           || ((0 - gpp->rest_secs) > (gpp->cycle_time_secs * 2)) )
           {
             /* if delay is too much 
              * (we are 2 cycletimes back, or already had handled 3 LATE frames in sequence)
              * start dropping frames now
              * until we are in time again
              */
             l_frame_dropped = TRUE;
             printf("DROP (SKIP) frame\n");
             gtk_label_set_text ( GTK_LABEL(gpp->status_label), _("SKIP"));
           }
           else
           {
             p_display_frame(gpp, l_framenr);
           }

           /* get secs elapsed since playbackstart (or last speed change) */
           elapsed_secs = g_timer_elapsed(gpp->gtimer, &elapsed_microsecs);

           gpp->rest_secs = (gpp->cycle_time_secs * gpp->framecnt) - elapsed_secs;
           /*if(gap_debug) printf("on_timer_playback[%d]: rest:%.4f\n", (int)ii, (float)gpp->rest_secs); */

           if(gpp->rest_secs > 0)
           {
              if((!l_frame_dropped) && (ii == 1))
              {
                 gtk_label_set_text ( GTK_LABEL(gpp->status_label), _("Playing"));
              }
              /* we are fast enough, there is rest time to wait until next frame */
             l_delay = gpp->delay_secs;  /* we have no additional delay */
             break;
           }

           l_delay = gpp->delay_secs - gpp->rest_secs;
            
           /* no time left at this point, try to display (or drop) next frame immediate */
           if(!l_frame_dropped)
           {
                 g_snprintf(status_txt, sizeof(status_txt), _("Delay %.2f"), (float)(l_delay));
                 gtk_label_set_text ( GTK_LABEL(gpp->status_label), status_txt);
           }
         }
       }    /* end for */

       /* keep track of absolute delay (since start or speed change) just for display purposes */
       gpp->delay_secs = l_delay;
              
       /* restart timer for next cycle */
       if(gpp->play_is_active)
       {
         p_start_playback_timer(gpp);
       }

    }
    gpp->in_timer_playback = FALSE;
  }
}  /* end on_timer_playback */



/* ------------------
 * on_timer_go_job
 * ------------------
 * This timer callback is called once after a go_button enter/clicked
 * callback with a miniamal delay of 8 millisecs.
 *
 * here is an example how it works:
 *
 *  consider the user moves the mouse from left to right
 *  passing go_buttons 1 upto 7
 *      
 *    (1)     (2)     (3)     (4)    (5)    (6)    (7)
 *     +-------+-------+-------+------+------+------+-------> time axis
 *     ..XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 *       < --  RENDER time for (1) ------------>
 *
 *  - at (1) the on_timer_go_job  Timer is initialized
 *  - The timer will fire after 8 millsecs, and starts rendering.
 *    while renderng 2,3,4,5,6 do happen, and get queued as pending events.
 *  - after render has finished
 *    the events are processed in sequence
 *    the first of the queued evnts (2) will init the go_timer again,
 *    and each of the oter events just sets the go_framenr (upto 6).
 *  - after another 8 millisecs delay the timer callback will start again
 *    and now processes the most recent go_framenr (6)
 *   
 * that way it is possible to skip frames for fast mousemoves on slow machines
 * (without that trick, the GUI would block until all visited go_bottons
 * are processed)
 */
static void
on_timer_go_job(gpointer   user_data)
{
  t_global_params *gpp;

  if(gap_debug) printf("\non_timer_go_job: START\n");
  gpp = (t_global_params*)user_data;
  if(gpp)
  {
    if(gpp->go_job_framenr >= 0)
    {
      p_display_frame(gpp, gpp->go_job_framenr);
    }
    
    if(gpp->go_timertag >= 0)
    {
      g_source_remove(gpp->go_timertag);
    }
    gpp->go_timertag = -1;

  }
}  /* end on_timer_go_job */

/* -----------------------------
 * on_play_button_clicked
 * -----------------------------
 */
static void
on_play_button_clicked(GtkButton *button, gpointer user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }

  gpp->play_backward = FALSE;

  if(gpp->play_is_active)
  {
    return;
  }

  if(gpp->play_selection_only)
  {
    if (gpp->play_current_framenr >= gpp->end_frame)
    {
      /* we are at selection end, start from selection begin */
      gpp->play_current_framenr = gpp->begin_frame;
    }
  }
  else
  {
    if (gpp->play_current_framenr >= gpp->ainfo_ptr->last_frame_nr)
    {
      /* we are at end, start from begin */
      gpp->play_current_framenr = gpp->ainfo_ptr->first_frame_nr;
    }
  }

  p_initial_start_playback_timer(gpp);

}  /* end on_play_button_clicked */


/* -----------------------------
 * on_pause_button_press_event
 * -----------------------------
 */
static gboolean
on_pause_button_press_event        (GtkButton       *button,
                                    GdkEventButton  *bevent,
                                    gpointer         user_data)
{
  t_global_params *gpp;
  gboolean         play_was_active;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return FALSE;
  }
  
  play_was_active = gpp->play_is_active;
  p_stop_playback(gpp);

  if(!play_was_active)
  {

    if ((bevent->button > 1)
    &&  (bevent->type == GDK_BUTTON_PRESS)
    &&  (gpp->ainfo_ptr))
    {
      if(bevent->button > 2)
      {
        p_display_frame(gpp, gpp->end_frame);  /* right mousebutton : goto end */
      }
      else
      {
        p_display_frame(gpp, gpp->ainfo_ptr->curr_frame_nr);  /* middle mousebutton : goto active image (invoker) */
      }
    }
    else
    {
      p_display_frame(gpp, gpp->begin_frame); /* left mousebutton : goto begin */
    }
  }

  return FALSE;
}  /* end on_pause_button_press_event */


/* -----------------------------
 * on_back_button_clicked
 * -----------------------------
 */
static void
on_back_button_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }

  gpp->play_backward = TRUE;

  if(gpp->play_is_active)
  {
    return;
  }

  if(gpp->play_selection_only)
  {
    if (gpp->play_current_framenr <= gpp->begin_frame)
    {
      /* we are already at selection begin, start from selection end */
      gpp->play_current_framenr = gpp->end_frame;
    }
  }
  else
  {
    if (gpp->play_current_framenr <= gpp->ainfo_ptr->first_frame_nr)
    {
      /* we are already at begin, start from end */
      gpp->play_current_framenr = gpp->ainfo_ptr->last_frame_nr;
    }
  }

  p_initial_start_playback_timer(gpp);

} /* end on_back_button_clicked */


/* -----------------------------
 * on_from_spinbutton_changed
 * -----------------------------
 */
static void
on_from_spinbutton_changed             (GtkEditable     *editable,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }
  gpp->begin_frame = (gint32)GTK_ADJUSTMENT(gpp->from_spinbutton_adj)->value;
  if(gpp->begin_frame > gpp->end_frame)
  {
    gpp->end_frame = gpp->begin_frame;
    gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->to_spinbutton_adj)
                            , (gfloat)gpp->end_frame
                            );                            
  }
  
}  /* end on_from_spinbutton_changed */



/* -----------------------------
 * on_to_spinbutton_changed
 * -----------------------------
 */
static void
on_to_spinbutton_changed               (GtkEditable     *editable,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }
  gpp->end_frame = (gint32)GTK_ADJUSTMENT(gpp->to_spinbutton_adj)->value;
  if(gpp->end_frame < gpp->begin_frame)
  {
    gpp->begin_frame = gpp->end_frame;
    gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->from_spinbutton_adj)
                            , (gfloat)gpp->begin_frame
                            );                            
  }
  
}  /* end on_to_spinbutton_changed */


/* -----------------------------
 * on_vid_preview_button_press_event
 * -----------------------------
 */
gboolean
on_vid_preview_button_press_event      (GtkWidget       *widget,
                                        GdkEventButton  *bevent,
                                        gpointer         user_data)
{


  if(gap_debug) printf("on_vid_preview_button_press_event: START\n");
  on_framenr_button_clicked(NULL, user_data);
  
  return FALSE;

}  /* end on_vid_preview_button_press_event */


/* -----------------------------
 * on_vid_preview_expose_event
 * -----------------------------
 */
gboolean
on_vid_preview_expose_event      (GtkWidget       *widget,
                                  GdkEventExpose  *eevent,
                                  gpointer         user_data)
{
  t_global_params *gpp;


  /*if(gap_debug) printf(" xxxxxxxx on_vid_preview_expose_event: START\n");*/

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return FALSE;
  }

  if (gpp->play_is_active && (gpp->rest_secs < 0.2))
  {
    /* we are playing and the next frame update is very near (less than 2/10 sec)
     * in that case it is better choice to skip the repaint
     * on expose events.
     * (dont waste time for repaint in that case)
     */
    return FALSE;
  }
  p_pview_repaint(gpp->pv_ptr);
  
  return FALSE;

}  /* end on_vid_preview_expose_event */


/* -----------------------------
 * on_vid_preview_size_allocate
 * -----------------------------
 * this procedure handles automatic
 * resize of the preview when the user resizes the window,
 * but is also called on other size changes
 * This handlerprocedure acts on the table11 widget.
 *   the table11 is of size 3x3 where the center holds
 *   aspect_frame wiget. (aspect_frame is the container for the drawing_area)
 *   (all othe table11 elements are empty dummy labels)
 * Size changes of table11 are propagated to
 * the preview (aspect_frame and drawing_area) by calls to p_update_pviewsize.
 */
static void
on_vid_preview_size_allocate            (GtkWidget       *widget,
                                         GtkAllocation   *allocation,
                                        gpointer         user_data)
{
   t_global_params *gpp;

   gpp = (t_global_params*)user_data;
   if((gpp == NULL) || (allocation == NULL))
   {
     return;
   }


   if(gap_debug) printf("on_vid_preview_size_allocate: START old: ow:%d oh:%d  new: w:%d h:%d \n"
                           , (int)gpp->old_resize_width
                           , (int)gpp->old_resize_height
                           , (int)allocation->width
                           , (int)allocation->height
                           );

   if(gpp->pv_ptr->da_widget == NULL) { return; }
   if(gpp->pv_ptr->da_widget->window == NULL) { return; }

   if(gap_debug) printf("  on_vid_preview_size_allocate: ORIGINAL pv_w:%d pv_h:%d count:%d\n"
                           , (int)gpp->pv_ptr->pv_width
                           , (int)gpp->pv_ptr->pv_height
                           , (int)gpp->resize_count
                           );

   if(gpp->in_resize)
   {
     if(gap_debug) printf("  on_vid_preview_size_allocate: IN_RESIZE (TERMINATED)\n");
     gpp->old_resize_width = allocation->width;
     gpp->old_resize_height = allocation->height;
     return;
   }

   /* check and perform size update only for new size
    * (allocation of same size will fire a new size_allocate event
    *  after return from this handler and leads to an endless loop)
    */ 


   if(gpp->resize_count > 1)
   {
     gpp->resize_count = 0;
     gpp->old_resize_width = allocation->width;
     gpp->old_resize_height = allocation->height;
     return;
   }
   
   if((gpp->old_resize_width != allocation->width)
   || (gpp->old_resize_height != allocation->height))
   {
     gint32  pv_pixelsize;
     gboolean blocked;
     gdouble  img_ratio;
     gdouble  alloc_ratio;
     
     blocked = FALSE;
     alloc_ratio = (gdouble)allocation->width / (gdouble)allocation->height;
     img_ratio   = (gdouble)gpp->ainfo_ptr->width / (gdouble)gpp->ainfo_ptr->height;
     if(gpp->ainfo_ptr == NULL)
     {
       blocked = FALSE;
       pv_pixelsize = gpp->pv_pixelsize;
     }
     else
     {
        if(img_ratio >= 1.0)
        {
           /* imageorientation is landscape */
           if(alloc_ratio <= img_ratio)
           {
             pv_pixelsize = CLAMP( (allocation->width -10)
                       , GAP_PLAYER_MIN_SIZE
                       , GAP_PLAYER_MAX_SIZE);
           }
           else
           {
             pv_pixelsize = CLAMP( (((allocation->height -10) * gpp->ainfo_ptr->width) / gpp->ainfo_ptr->height)
                       , GAP_PLAYER_MIN_SIZE
                       , GAP_PLAYER_MAX_SIZE);
           }
        }
        else
        {
           /* imageorientation is portrait */
           if(alloc_ratio <= img_ratio)
           {
             pv_pixelsize = CLAMP( (((allocation->width -10) * gpp->ainfo_ptr->height) / gpp->ainfo_ptr->width)
                       , GAP_PLAYER_MIN_SIZE
                       , GAP_PLAYER_MAX_SIZE);
           }
           else
           {
             pv_pixelsize = CLAMP( (allocation->height -10)
                       , GAP_PLAYER_MIN_SIZE
                       , GAP_PLAYER_MAX_SIZE);
           }
        }
     }


     if (gap_debug) printf("pv_pixelsize: %d  img_ratio:%.3f alloc_ratio:%.3f\n"
                               , (int)pv_pixelsize
                               , (float)img_ratio
                               , (float)alloc_ratio
                               );
     
     if(pv_pixelsize > gpp->pv_pixelsize)
     {
       if ((alloc_ratio > 1.0) /* landscape */
       && (allocation->width < gpp->old_resize_width))
       {
         if(gap_debug) printf(" BLOCK preview grow on  width shrink\n");
         blocked = TRUE;
       }
       if ((alloc_ratio <= 1.0) /* portrait */
       && (allocation->height < gpp->old_resize_height))
       {
         if(gap_debug) printf(" BLOCK preview grow on  height shrink\n");
         blocked = TRUE;
       }
     }
     if(!blocked)
     {
         gpp->pv_pixelsize =  pv_pixelsize;

         p_update_pviewsize(gpp);


         if(!gpp->play_is_active)
         {
           /* have to refresh current frame
            *  repaint is not enough because size has changed
            * (if play_is_active we can skip this, because the next update is on the way)
            */
           p_display_frame(gpp, gpp->ainfo_ptr->curr_frame_nr);
           gpp->resize_count++;
         }
     }

     if(gap_debug) printf("  on_vid_preview_size_allocate: AFTER resize pv_w:%d pv_h:%d\n"
                           , (int)gpp->pv_ptr->pv_width
                           , (int)gpp->pv_ptr->pv_height
                           );
   }
   else
   {
     gpp->resize_count = 0;
   }

   gpp->old_resize_width = allocation->width;
   gpp->old_resize_height = allocation->height;

   if(gap_debug) printf("  on_vid_preview_size_allocate: END\n");
   
}  /* end on_vid_preview_size_allocate */


/* -----------------------------
 * on_framenr_button_clicked
 * -----------------------------
 */
static void
on_framenr_button_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
  t_global_params *gpp;
   GimpParam          *return_vals;
   int              nreturn_vals;
 
  /*if(gap_debug) printf("on_framenr_button_clicked: START\n"); */

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }
 
  p_stop_playback(gpp);

   return_vals = gimp_run_procedure ("plug_in_gap_goto",
                                    &nreturn_vals,
	                            GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
				    GIMP_PDB_IMAGE,    gpp->image_id,
				    GIMP_PDB_DRAWABLE, -1,  /* dummy */
	                            GIMP_PDB_INT32,    gpp->play_current_framenr,
                                    GIMP_PDB_END);

  if(return_vals)
  {
    if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    {
       p_reload_ainfo_ptr(gpp, return_vals[1].data.d_image);
       p_update_ainfo_dependent_widgets(gpp); 
       gimp_displays_flush();
    }
     
    g_free(return_vals);
  }
  

}  /* end on_framenr_button_clicked */



/* -----------------------------
 * on_framenr_spinbutton_changed
 * -----------------------------
 */
static void
on_framenr_spinbutton_changed          (GtkEditable     *editable,
                                        gpointer         user_data)
{
  t_global_params *gpp;
  gint32           framenr;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }
  framenr = (gint32)GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)->value;
  if(gpp->play_current_framenr != framenr)
  {
    p_display_frame(gpp, framenr);
  }
  
}  /* end on_framenr_spinbutton_changed */


/* -----------------------------
 * on_origspeed_button_clicked
 * -----------------------------
 */
static void
on_origspeed_button_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }

  if(gpp->speed != gpp->original_speed)
  {
    gpp->prev_speed = gpp->speed;
    gpp->speed = gpp->original_speed;
    gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->speed_spinbutton_adj)
                            , (gfloat)gpp->speed
                            );                            
  }
  else
  {
    if(gpp->original_speed != gpp->prev_speed)
    {
      gpp->speed = gpp->prev_speed;
      gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->speed_spinbutton_adj)
                            , (gfloat)gpp->speed
                            );                            
    }
  }

}  /* end on_origspeed_button_clicked */


/* -----------------------------
 * on_speed_spinbutton_changed
 * -----------------------------
 */
static void
on_speed_spinbutton_changed            (GtkEditable     *editable,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }

  gpp->speed = GTK_ADJUSTMENT(gpp->speed_spinbutton_adj)->value;

}  /* end on_speed_spinbutton_changed */

 
/* ---------------------
 * p_fit_shell_window
 * ---------------------
 */
static void 
p_fit_shell_window(t_global_params *gpp)
{
  gint width;
  gint height;

  /* gtk_window_get_default_size(GTK_WINDOW(gpp->shell_window), &width, &height); */
  /* if(gap_debug) printf("window default size (%dx%d)\n", (int)width, (int)height ); */

  /* FIXME: use preview size plus fix offsets (the offsets are just a guess
   * and may be too small for other languages
   */
  width =  MAX(gpp->pv_ptr->pv_width, 256) + 272;
  height = MAX(gpp->pv_ptr->pv_height, 256) + 128;

  gtk_widget_set_size_request (gpp->shell_window, width, height);  /* shrink shell window */
  gtk_window_resize (GTK_WINDOW(gpp->shell_window), width, height);  /* shrink shell window */
}  /* end p_fit_shell_window */

/* -----------------------------
 * on_size_button_button_press_event
 * -----------------------------
 */
static gboolean
on_size_button_button_press_event  (GtkWidget       *widget,
                                    GdkEventButton  *bevent,
                                    gpointer         user_data)
{
  t_global_params *gpp;


  if(gap_debug) printf("\nON_SIZE_BUTTON_BUTTON_PRESS_EVENT START\n");

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return FALSE;
  }
  gpp->resize_count = 1;
  gpp->in_resize = TRUE;              /* blocks resize chain reaction on initil size allocation event */

  if ((bevent->state & GDK_SHIFT_MASK)
  &&  (bevent->type == GDK_BUTTON_PRESS)
  &&  (gpp->ainfo_ptr))
  {
    if(gap_debug) printf("GDK_SHIFT_MASK !!\n\n");
    gpp->pv_pixelsize = CLAMP(MAX(gpp->ainfo_ptr->width, gpp->ainfo_ptr->height)
                       , GAP_PLAYER_MIN_SIZE
                       , GAP_PLAYER_MAX_SIZE);
  }
  else
  {
    /* toggle between normal and large thumbnail size */
    if(gpp->pv_pixelsize == 256)
    {
      gpp->pv_pixelsize = 128;
    }
    else
    {
      gpp->pv_pixelsize = 256;
    }
  }
  
  if(gpp->pv_pixelsize != (gint32)GTK_ADJUSTMENT(gpp->size_spinbutton_adj)->value)
  {
    p_update_pviewsize(gpp);
    if(!gpp->play_is_active)
    {
      p_display_frame(gpp, gpp->play_current_framenr);
    }
  }
  gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->size_spinbutton_adj)
                            , (gfloat)gpp->pv_pixelsize
                            );                            
  

  p_fit_shell_window(gpp);

  gpp->in_resize = FALSE;
  gpp->resize_count = 2;
  
  return FALSE;

}  /* end on_size_button_button_press_event */


/* -----------------------------
 * on_size_spinbutton_changed
 * -----------------------------
 */
static void
on_size_spinbutton_changed             (GtkEditable     *editable,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }

  gpp->resize_count = 1;

  if(gpp->pv_pixelsize != (gint32)GTK_ADJUSTMENT(gpp->size_spinbutton_adj)->value)
  {
    gpp->pv_pixelsize = (gint32)GTK_ADJUSTMENT(gpp->size_spinbutton_adj)->value;

    if((gpp->in_resize) && (gpp->pv_pixelsize > 256))
    {
       /* disable bigger sizes than 256 when entered by spinbutton
        */
       gpp->pv_pixelsize = 256;
       gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->size_spinbutton_adj)
                            , (gfloat)gpp->pv_pixelsize
                            );                            
      
    }

    p_update_pviewsize(gpp);

    if(!gpp->play_is_active)
    {
      p_display_frame(gpp, gpp->play_current_framenr);
    }
    if(gpp->in_resize)
    {
       /* the resize was caused by click to size_spinbutton
        * or we are in startup, 
        * we should resize shell window to fit.
        */
       p_fit_shell_window(gpp);
    }
  }

}  /* end on_size_spinbutton_changed */


/* -----------------------------
 * on_size_spinbutton_enter
 * -----------------------------
 */
static gboolean   on_size_spinbutton_enter               (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      gpointer         user_data)
{
  t_global_params *gpp;

  /*if(gap_debug) printf("\n on_size_spinbutton_enter START\n");*/

  gpp = (t_global_params *)user_data;
  if(gpp)
  { 
    gpp->in_resize = TRUE;
  }
  return FALSE;
}  /* end on_size_spinbutton_enter */
                                                      

/* -----------------------------
 * on_size_spinbutton_leave
 * -----------------------------
 */
static gboolean   on_size_spinbutton_leave               (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      gpointer         user_data)
{
  t_global_params *gpp;

  /*if(gap_debug) printf("\n on_size_spinbutton_leave START\n");*/

  gpp = (t_global_params *)user_data;
  if(gpp)
  { 
    gpp->in_resize = FALSE;
  }
  return FALSE;
}  /* end on_size_spinbutton_leave */




/* -----------------------------
 * on_exact_timing_checkbutton_toggled
 * -----------------------------
 */
static void
on_exact_timing_checkbutton_toggled       (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }

  if (togglebutton->active)
  {
       gpp->exact_timing = TRUE;
  }
  else
  {
       gpp->exact_timing = FALSE;
  }

}  /* end on_exact_timing_checkbutton_toggled */


/* -----------------------------
 * on_use_thumb_checkbutton_toggled
 * -----------------------------
 */
static void
on_use_thumb_checkbutton_toggled       (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }

  if (togglebutton->active)
  {
       gpp->use_thumbnails = TRUE;
  }
  else
  {
       gpp->use_thumbnails = FALSE;
  }

}  /* end on_use_thumb_checkbutton_toggled */

/* -----------------------------
 * on_pinpong_checkbutton_toggled
 * -----------------------------
 */
static void
on_pinpong_checkbutton_toggled         (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }

  if (togglebutton->active)
  {
       gpp->play_pingpong = TRUE;
  }
  else
  {
       gpp->play_pingpong = FALSE;
  }

}  /* end on_pinpong_checkbutton_toggled */


/* -----------------------------
 * on_selonly_checkbutton_toggled
 * -----------------------------
 */
static void
on_selonly_checkbutton_toggled         (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }

  if (togglebutton->active)
  {
       gpp->play_selection_only = TRUE;
  }
  else
  {
       gpp->play_selection_only = FALSE;
  }

}  /* end on_selonly_checkbutton_toggled */


/* -----------------------------
 * on_loop_checkbutton_toggled
 * -----------------------------
 */
static void
on_loop_checkbutton_toggled            (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }

  if (togglebutton->active)
  {
       gpp->play_loop = TRUE;
  }
  else
  {
       gpp->play_loop = FALSE;
  }

}  /* end on_loop_checkbutton_toggled */




/* -----------------------------
 * on_close_button_clicked
 * -----------------------------
 */
static void
on_close_button_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }
  p_stop_playback(gpp);

  gtk_widget_destroy (GTK_WIDGET (gpp->shell_window));  /* close & destroy dialog window */
  gtk_main_quit ();

}  /* end on_close_button_clicked */


/* -----------------------------
 * on_shell_window_destroy
 * -----------------------------
 */
static void
on_shell_window_destroy                     (GtkObject       *object,
                                        gpointer         user_data)
{
  t_global_params *gpp;

  gpp = (t_global_params*)user_data;
  if(gpp)
  {
    p_stop_playback(gpp);
  }
  gtk_main_quit ();

}  /* end on_shell_window_destroy */



/* -----------------------------
 * on_go_button_clicked
 * -----------------------------
 */
static void
on_go_button_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
   t_gobutton *gob;
   t_global_params *gpp;
   gint32  framenr;

   gob = (t_gobutton *)user_data;
   if(gob == NULL) { return; }

   gpp = gob->gpp;
   if(gpp == NULL) { return; }
   

   /*if (gap_debug) printf("on_go_button_clicked: go_number:%d\n", (int)gob->go_number );*/

   p_stop_playback(gpp);
   framenr = p_framenr_from_go_number(gpp, gob->go_number);
   if(gpp->play_current_framenr != framenr)
   {
     p_display_frame(gpp, framenr);
   }
   
   if(framenr != gpp->ainfo_ptr->curr_frame_nr)
   {
     on_framenr_button_clicked(NULL, gpp);
   }
}  /* end on_go_button_clicked */


/* -----------------------------
 * on_go_button_enter
 * -----------------------------
 */
static gboolean
on_go_button_enter                   (GtkButton       *button,
                                        GdkEvent      *event,
                                        gpointer       user_data)
{
   t_gobutton *gob;
   t_global_params *gpp;
   gint32  framenr;
   
   gob = (t_gobutton *)user_data;
   if(gob == NULL) { return FALSE; }

   gpp = gob->gpp;
   if(gpp == NULL) { return FALSE; }
   

   /*if (gap_debug) printf("ON_GO_BUTTON_ENTER: go_number:%d\n", (int)gob->go_number ); */
  
   p_stop_playback(gpp);
 
   /* skip display on full sized image mode 
    * (gui cant follow that fast on many machines, and would react slow)
    */ 
   if(gpp->use_thumbnails)
   { 
      framenr = p_framenr_from_go_number(gpp, gob->go_number);
      if(gpp->play_current_framenr != framenr)
      {
          if(gap_debug)
          {
             if(gpp->go_timertag >= 0) 
             {
               printf("on_go_button_enter: DROP GO_FRAMENR: %d\n", (int)gpp->go_job_framenr);
             }
          }
          
          /* we want the go_timer to display that framenr */
          gpp->go_job_framenr = framenr;
          if(gpp->go_timertag < 0)
          {
             /* if the go_timer is not already prepared to fire
              * we start  p_display_frame(gpp, gpp->go_job_framenr); 
              * after minimal delay of 8 millisecods.
              */
             gpp->go_timertag = (gint32) g_timeout_add(8, (GtkFunction)on_timer_go_job, gpp);
          }

      }
   }

   return FALSE;
}  /* end on_go_button_enter */

/* -----------------------------
 * on_gobutton_hbox_leave
 * -----------------------------
 */
static void
on_gobutton_hbox_leave                 (GtkWidget        *widget,
                                        GdkEvent         *event,
                                        gpointer          user_data)
{
  t_global_params *gpp;

  /*if (gap_debug) printf("ON_GOBUTTON_HBOX_LEAVE\n");*/

  gpp = (t_global_params*)user_data;
  if(gpp == NULL)
  {
    return;
  }

  /* reset go_base */
  gpp->go_base_framenr = -1;
  gpp->go_base = -1;
 
  p_check_tooltips();
 
}  /* end on_gobutton_hbox_leave */


/* -----------------------------
 * p_create_player_window
 * -----------------------------
 */
GtkWidget*
p_create_player_window (t_global_params *gpp)
{
  GtkWidget *shell_window;
  GtkWidget *event_box;
  
  GtkWidget *frame0;
  GtkWidget *aspect_frame;
  GtkWidget *frame2;

  GtkWidget *vbox1;

  GtkWidget *hbox1;
  GtkWidget *gobutton_hbox;

  GtkWidget *table1;
  GtkWidget *table11;
  GtkWidget *table2;

  GtkWidget *vid_preview;

  GtkWidget *status_label;
  GtkWidget *timepos_label;
  GtkWidget *label;

  GtkWidget *hseparator;

  GtkObject *from_spinbutton_adj;
  GtkWidget *from_spinbutton;
  GtkObject *to_spinbutton_adj;
  GtkWidget *to_spinbutton;
  GtkObject *framenr_spinbutton_adj;
  GtkWidget *framenr_spinbutton;
  GtkObject *speed_spinbutton_adj;
  GtkWidget *speed_spinbutton;
  GtkObject *size_spinbutton_adj;
  GtkWidget *size_spinbutton;


  GtkWidget *play_button;
  GtkWidget *pause_button;
  GtkWidget *back_button;
  GtkWidget *close_button;
  GtkWidget *origspeed_button;
  GtkWidget *size_button;
  GtkWidget *framenr_button;

  GtkWidget *use_thumb_checkbutton;
  GtkWidget *exact_timing_checkbutton;
  GtkWidget *pinpong_checkbutton;
  GtkWidget *selonly_checkbutton;
  GtkWidget *loop_checkbutton;


  shell_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (shell_window), _("Videoframe Playback"));
  gtk_window_set_policy (GTK_WINDOW (shell_window), FALSE, TRUE, TRUE);
  g_signal_connect (G_OBJECT (shell_window), "destroy",
                      G_CALLBACK (on_shell_window_destroy),
                      gpp);


  frame0 = gtk_frame_new (gpp->ainfo_ptr->basename);
  gtk_widget_show (frame0);
  gtk_container_add (GTK_CONTAINER (shell_window), frame0);




  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (frame0), vbox1);


  /* the hbox for the go button array */
  gobutton_hbox = gtk_hbox_new (TRUE, 0);
  gtk_widget_show (gobutton_hbox);

  if(gobutton_hbox)
  {
    gint go_number;
    GtkWidget *go_button;
    t_gobutton *gob;

    /* the gobutton array is be filled with [n] gobuttons */
    
    for(go_number=0; go_number < GAP_PLAY_MAX_GOBUTTONS; go_number++)
    {
       /* the go_button[s] */
       gob = g_malloc0(sizeof(t_gobutton));
       gob->gpp = gpp;
       gob->go_number = go_number;
       
       go_button = gtk_button_new ();
       gtk_widget_show (go_button);

       gtk_widget_set_events(go_button, GDK_ENTER_NOTIFY_MASK );
       gtk_box_pack_start (GTK_BOX (gobutton_hbox), go_button, FALSE, TRUE, 0);
       gtk_widget_set_size_request (go_button, -1, 30);
       /* gimp_help_set_help_data (go_button, _("go to frame"), NULL); */
       g_signal_connect (go_button, "enter_notify_event",
                      G_CALLBACK (on_go_button_enter)
                      ,gob);
       g_signal_connect (G_OBJECT (go_button), "clicked",
                      G_CALLBACK (on_go_button_clicked),
                      gob);
    }
  }

  /* Create an EventBox and for the gobutton_hbox leave Event */
  event_box = gtk_event_box_new ();
  gtk_widget_show (event_box);
  gtk_container_add (GTK_CONTAINER (event_box), gobutton_hbox);
  gtk_widget_set_events(event_box
                       ,  GDK_LEAVE_NOTIFY_MASK /* | gtk_widget_get_events (event_box) */
                       );

  gtk_box_pack_start (GTK_BOX (vbox1), event_box, FALSE, FALSE, 0);
  g_signal_connect (event_box, "leave_notify_event",
                      G_CALLBACK (on_gobutton_hbox_leave)
                      ,gpp);



  table1 = gtk_table_new (2, 2, FALSE);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);



  /* the frame2 for range and playback mode control widgets */
  frame2 = gtk_frame_new (NULL);
  gtk_widget_show (frame2);
  gtk_table_attach (GTK_TABLE (table1), frame2, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* table2 for range and playback mode control widgets */
  table2 = gtk_table_new (17, 2, FALSE);
  gtk_widget_show (table2);
  gtk_container_add (GTK_CONTAINER (frame2), table2);

  label = gtk_label_new (_("From Frame:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table2), label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  label = gtk_label_new (_("To Frame:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table2), label, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* the FROM spinbutton (start of rangeselection)  */
  from_spinbutton_adj = gtk_adjustment_new ( gpp->begin_frame
                                           , gpp->ainfo_ptr->first_frame_nr
                                           , gpp->ainfo_ptr->last_frame_nr
                                           , 1, 10, 10);
  from_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (from_spinbutton_adj), 1, 0);
  gtk_widget_show (from_spinbutton);
  gtk_table_attach (GTK_TABLE (table2), from_spinbutton, 1, 2, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (from_spinbutton, 80, -1);
  gimp_help_set_help_data (from_spinbutton, _("Start Framenumber of Selection Range"), NULL);
  g_signal_connect (G_OBJECT (from_spinbutton), "changed",
                      G_CALLBACK (on_from_spinbutton_changed),
                      gpp);

  /* the TO spinbutton (end of rangeselection)  */
  to_spinbutton_adj = gtk_adjustment_new ( gpp->end_frame
                                           , gpp->ainfo_ptr->first_frame_nr
                                           , gpp->ainfo_ptr->last_frame_nr
                                           , 1, 10, 10);
  to_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (to_spinbutton_adj), 1, 0);
  gtk_widget_show (to_spinbutton);
  gtk_table_attach (GTK_TABLE (table2), to_spinbutton, 1, 2, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (to_spinbutton, 80, -1);
  gimp_help_set_help_data (to_spinbutton, _("End Framenumber of Selection Range"), NULL);
  g_signal_connect (G_OBJECT (to_spinbutton), "changed",
                      G_CALLBACK (on_to_spinbutton_changed),
                      gpp);

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_table_attach (GTK_TABLE (table2), hseparator, 0, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);


  /* the framenr button */
  framenr_button = gtk_button_new_with_label (_("FrameNr"));
  gtk_widget_show (framenr_button);
  gtk_table_attach (GTK_TABLE (table2), framenr_button, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (framenr_button, _("Load this Frame into the calling image"), NULL);
  g_signal_connect (G_OBJECT (framenr_button), "clicked",
                      G_CALLBACK (on_framenr_button_clicked),
                      gpp);


  /* the FRAMENR spinbutton (current displayed frame)  */
  framenr_spinbutton_adj = gtk_adjustment_new ( gpp->play_current_framenr
                                           , gpp->ainfo_ptr->first_frame_nr
                                           , gpp->ainfo_ptr->last_frame_nr
                                           , 1, 10, 10);

  framenr_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (framenr_spinbutton_adj), 1, 0);
  gtk_widget_show (framenr_spinbutton);
  gtk_table_attach (GTK_TABLE (table2), framenr_spinbutton, 1, 2, 3, 4,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (framenr_spinbutton, 80, -1);
  gimp_help_set_help_data (framenr_spinbutton, _("Displayed Frame Nr"), NULL);
  g_signal_connect (G_OBJECT (framenr_spinbutton), "changed",
                      G_CALLBACK (on_framenr_spinbutton_changed),
                      gpp);


  /* the time position */
  label = gtk_label_new (_("Timepos:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table2), label, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* the timepos label */
  /* (had used an entry here before but had update performance problems
   *  beginning at playback speed of 17 frames/sec on PII 300 Mhz)
   */
  timepos_label = gtk_label_new ("00:00:000");
  gtk_widget_show (timepos_label);
  gtk_table_attach (GTK_TABLE (table2), timepos_label, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);




  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_table_attach (GTK_TABLE (table2), hseparator, 0, 2, 5, 6,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);



  /* the origspeed_button */
  origspeed_button = gtk_button_new_with_label (_("Speed"));
  gtk_widget_show (origspeed_button);
  gtk_table_attach (GTK_TABLE (table2), origspeed_button, 0, 1, 6, 7,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (origspeed_button, _("Reset Playback Speed to original (or previous) Value"), NULL);
  g_signal_connect (G_OBJECT (origspeed_button), "clicked",
                      G_CALLBACK (on_origspeed_button_clicked),
                      gpp);

  /* the SPEED spinbutton
   * with the given timer resolution of millisecs the theoretical
   * maximum speed is 1000 frames/sec that would result in 1 timertick
   * this implementation allows a max speed of 250 frames/sec (4 timerticks)
   */
  speed_spinbutton_adj = gtk_adjustment_new ( gpp->speed
                                           , 1.0
                                           , 250.0
                                           , 1, 10, 10);
  speed_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (speed_spinbutton_adj), 1, 4);
  gtk_widget_show (speed_spinbutton);
  gtk_table_attach (GTK_TABLE (table2), speed_spinbutton, 1, 2, 6, 7,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (speed_spinbutton, 80, -1);
  gimp_help_set_help_data (speed_spinbutton, _("Current Playbackspeed (frames/sec)"), NULL);

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_table_attach (GTK_TABLE (table2), hseparator, 0, 2, 7, 8,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
  g_signal_connect (G_OBJECT (speed_spinbutton), "changed",
                      G_CALLBACK (on_speed_spinbutton_changed),
                      gpp);


  /* the size button */
  size_button = gtk_button_new_with_label (_("Size"));
  gtk_widget_show (size_button);
  gtk_widget_set_events(size_button, GDK_BUTTON_PRESS_MASK);
  gtk_table_attach (GTK_TABLE (table2), size_button, 0, 1, 8, 9,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (size_button, _("Toggle Size 128/256, SHIFT: Set 1:1 full image Size"), NULL);
  g_signal_connect (G_OBJECT (size_button), "button_press_event",
                      G_CALLBACK (on_size_button_button_press_event),
                      gpp);

  /* the SIZE spinbutton */
  size_spinbutton_adj = gtk_adjustment_new (gpp->pv_pixelsize
                                           , GAP_PLAYER_MIN_SIZE
                                           , GAP_PLAYER_MAX_SIZE
                                           , 1, 10, 10);

  size_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (size_spinbutton_adj), 1, 0);
  gpp->size_spinbutton = size_spinbutton;
  gtk_widget_show (size_spinbutton);
  gtk_table_attach (GTK_TABLE (table2), size_spinbutton, 1, 2, 8, 9,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (size_spinbutton, 80, -1);
  gimp_help_set_help_data (size_spinbutton, _("Video Preview Size (pixels)"), NULL);

  g_signal_connect (G_OBJECT (size_spinbutton), "changed",
                      G_CALLBACK (on_size_spinbutton_changed),
                      gpp);
  gtk_widget_set_events(size_spinbutton
                       ,  GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK
                       );
  g_signal_connect (G_OBJECT (size_spinbutton), "enter_notify_event",
                      G_CALLBACK (on_size_spinbutton_enter),
                      gpp);
  g_signal_connect (G_OBJECT (size_spinbutton), "leave_notify_event",
                      G_CALLBACK (on_size_spinbutton_leave),
                      gpp);
  


  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_table_attach (GTK_TABLE (table2), hseparator, 0, 2, 9, 10,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);

  /* the playback mode checkbuttons */

  loop_checkbutton = gtk_check_button_new_with_label (_("Loop"));
  gtk_widget_show (loop_checkbutton);
  gtk_table_attach (GTK_TABLE (table2), loop_checkbutton, 0, 2, 10, 11,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (loop_checkbutton, _("ON: Play in endless Loop, OFF: Play only once"), NULL);
  if(gpp->play_loop)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (loop_checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (loop_checkbutton), "toggled",
                      G_CALLBACK (on_loop_checkbutton_toggled),
                      gpp);

  selonly_checkbutton = gtk_check_button_new_with_label (_("Play Selection only"));
  gtk_widget_show (selonly_checkbutton);
  gtk_table_attach (GTK_TABLE (table2), selonly_checkbutton, 0, 2, 11, 12,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (selonly_checkbutton, _("ON: Play Selection only, OFF: Play all frames"), NULL);
  if(gpp->play_selection_only)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (selonly_checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (selonly_checkbutton), "toggled",
                      G_CALLBACK (on_selonly_checkbutton_toggled),
                      gpp);

  pinpong_checkbutton = gtk_check_button_new_with_label (_("PingPong"));
  gtk_widget_show (pinpong_checkbutton);
  gtk_table_attach (GTK_TABLE (table2), pinpong_checkbutton, 0, 2, 12, 13,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (pinpong_checkbutton, _("ON: Play alternating forward/backward"), NULL);
  if(gpp->play_pingpong)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pinpong_checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (pinpong_checkbutton), "toggled",
                      G_CALLBACK (on_pinpong_checkbutton_toggled),
                      gpp);

  use_thumb_checkbutton = gtk_check_button_new_with_label (_("Use Thumbnails"));
  gtk_widget_show (use_thumb_checkbutton);
  gtk_table_attach (GTK_TABLE (table2), use_thumb_checkbutton, 0, 2, 13, 14,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (use_thumb_checkbutton, _("ON: use thumbnails when available, OFF: read full sized frames"), NULL);
  if(gpp->use_thumbnails)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_thumb_checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (use_thumb_checkbutton), "toggled",
                      G_CALLBACK (on_use_thumb_checkbutton_toggled),
                      gpp);


  exact_timing_checkbutton = gtk_check_button_new_with_label (_("Exact Timing"));
  gtk_widget_show (exact_timing_checkbutton);
  gtk_table_attach (GTK_TABLE (table2), exact_timing_checkbutton, 0, 2, 14, 15,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (exact_timing_checkbutton, _("ON: skip frames to hold exact timing, OFF: disable frame skipping"), NULL);
  if(gpp->exact_timing)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (exact_timing_checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (exact_timing_checkbutton), "toggled",
                      G_CALLBACK (on_exact_timing_checkbutton_toggled),
                      gpp);


  /* the status label */
  label = gtk_label_new (_("Status:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table2), label, 0, 1, 15, 16,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* the status value label */
  status_label = gtk_label_new (_("Ready"));
  gtk_widget_show (status_label);
  gtk_table_attach (GTK_TABLE (table2), status_label, 1, 2, 15, 16,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (status_label), 0, 0.5);

  /* a dummy label to fill up table1 until bottom */
  label = gtk_label_new (" ");
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 1, 2, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (GTK_FILL | GTK_SHRINK | GTK_EXPAND), 0, 0);



  /* the playback stock button box */
  hbox1 = gtk_hbox_new (TRUE, 0);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

  /* the PLAY button */
  play_button = gtk_button_new_from_stock (GAP_STOCK_PLAY);
  gtk_widget_show (play_button);
  gtk_box_pack_start (GTK_BOX (hbox1), play_button, FALSE, TRUE, 0);
  gimp_help_set_help_data (play_button, _("Start Playback"), NULL);
  g_signal_connect (G_OBJECT (play_button), "clicked",
                      G_CALLBACK (on_play_button_clicked),
                      gpp);

  /* the PAUSE button */
  pause_button = gtk_button_new_from_stock (GAP_STOCK_PAUSE);
  gtk_widget_show (pause_button);
  gtk_widget_set_events(pause_button, GDK_BUTTON_PRESS_MASK);
  gtk_box_pack_start (GTK_BOX (hbox1), pause_button, FALSE, TRUE, 0);
  gimp_help_set_help_data (pause_button, _("Pause (Goto Selection Start/Active/End if not playing)"), NULL);
  g_signal_connect (G_OBJECT (pause_button), "button_press_event",
                      G_CALLBACK (on_pause_button_press_event),
                      gpp);

  /* the PLAY_REVERSE button */
  back_button = gtk_button_new_from_stock (GAP_STOCK_PLAY_REVERSE);
  gtk_widget_show (back_button);
  gtk_box_pack_start (GTK_BOX (hbox1), back_button, FALSE, TRUE, 0);
  gimp_help_set_help_data (back_button, _("Start Reverse Playback"), NULL);
  g_signal_connect (G_OBJECT (back_button), "clicked",
                      G_CALLBACK (on_back_button_clicked),
                      gpp);

  /* the CLOSE button */
  close_button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  gtk_widget_show (close_button);
  gtk_box_pack_start (GTK_BOX (hbox1), close_button, FALSE, TRUE, 0);
  gimp_help_set_help_data (close_button, _("Close Window"), NULL);
  g_signal_connect (G_OBJECT (close_button), "clicked",
                      G_CALLBACK (on_close_button_clicked),
                      gpp);


  /* aspect_frame is the CONTAINER for the video preview */
  aspect_frame = gtk_aspect_frame_new (NULL   /* without label */
                                      , 0.5   /* xalign center */
                                      , 0.5   /* yalign center */
                                      , gpp->ainfo_ptr->width / gpp->ainfo_ptr->height     /* ratio */
                                      , TRUE  /* obey_child */
                                      );
  gtk_widget_show (aspect_frame);

  /* table11 is used to center aspect_frame */
  table11 = gtk_table_new (3, 3, FALSE);
  gtk_widget_show (table11);
  gtk_table_attach (GTK_TABLE (table1), table11, 0, 1, 0, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), 0, 0);
  {
    gint ix;
    gint iy;
    
    for(ix = 0; ix < 3; ix++)
    {
      for(iy = 0; iy < 3; iy++)
      {
        if((ix == 1) && (iy == 1))
        {
           gtk_table_attach (GTK_TABLE (table11), aspect_frame, ix, ix+1, iy, iy+1,
                             (GtkAttachOptions) (0 ),
                             (GtkAttachOptions) (0 ), 0, 0);
        }
        else
        {
          /* dummy labels to fill up table11  */
          label = gtk_label_new (" ");
          gtk_widget_show (label);
          gtk_table_attach (GTK_TABLE (table11), label, ix, ix+1, iy, iy+1,
                            (GtkAttachOptions) (GTK_FILL | GTK_SHRINK | GTK_EXPAND),
                            (GtkAttachOptions) (GTK_FILL | GTK_SHRINK | GTK_EXPAND), 0, 0);
        }
      }
    }    
  
  }

  gtk_widget_realize (shell_window);

  /* the preview drawing_area_widget */
  /* ############################### */
  gpp->pv_ptr = p_pview_new(128, 128, GAP_PLAYER_CHECK_SIZE, aspect_frame);
  vid_preview = gpp->pv_ptr->da_widget;
  gtk_container_add (GTK_CONTAINER (aspect_frame), vid_preview);
  gtk_widget_show (vid_preview);
  gtk_widget_set_events (vid_preview, GDK_BUTTON_PRESS_MASK | GDK_EXPOSURE_MASK);

  /* gpp copies of objects used outside this procedure  */
  gpp->from_spinbutton_adj = from_spinbutton_adj;
  gpp->to_spinbutton_adj = to_spinbutton_adj;
  gpp->framenr_spinbutton_adj = framenr_spinbutton_adj;
  gpp->speed_spinbutton_adj = speed_spinbutton_adj;
  gpp->size_spinbutton_adj = size_spinbutton_adj;


  gtk_widget_realize (gpp->pv_ptr->da_widget);
  p_update_pviewsize(gpp);
  
  g_signal_connect (G_OBJECT (vid_preview), "button_press_event",
                      G_CALLBACK (on_vid_preview_button_press_event),
                      gpp);
  g_signal_connect (G_OBJECT (vid_preview), "expose_event",
                      G_CALLBACK (on_vid_preview_expose_event),
                      gpp);

  gpp->table11 = table11;
  g_signal_connect (G_OBJECT (table11), "size_allocate",
                      G_CALLBACK (on_vid_preview_size_allocate),
                      gpp);




  gpp->status_label = status_label;
  gpp->timepos_label = timepos_label;

  return shell_window;

}  /* end p_create_player_window */


/* -----------------------------
 * p_playback_dialog
 * -----------------------------
 */
void
p_playback_dialog(t_global_params *gpp)
{
  gimp_ui_init ("gap_player_dialog", FALSE);
  gap_stock_init();
  p_check_tooltips();
 
  gpp->ainfo_ptr = NULL;
  gpp->original_speed = 24.0;   /* default if framerate is unknown */
  gpp->prev_speed = 24.0;       /* default if framerate is unknown */

  p_reload_ainfo_ptr(gpp, gpp->image_id);
  if(gpp->ainfo_ptr == NULL)
  {
    return;
  }

  if(0 == p_chk_framerange(gpp->ainfo_ptr))
  {
    gpp->play_is_active = FALSE;
    gpp->exact_timing = TRUE;
    gpp->play_timertag = -1;
    gpp->go_timertag = -1;
    gpp->go_base_framenr = -1;
    gpp->go_base = -1;
    gpp->pingpong_count = 0;
    gpp->gtimer = g_timer_new();
    gpp->cycle_time_secs = 0.3;
    gpp->rest_secs = 0.0;
    gpp->framecnt = 0.0;

    if(gpp->autostart)
    {
      gpp->begin_frame = CLAMP(gpp->begin_frame
                              , gpp->ainfo_ptr->first_frame_nr
                              , gpp->ainfo_ptr->last_frame_nr);
      gpp->end_frame = CLAMP(gpp->end_frame
                              , gpp->ainfo_ptr->first_frame_nr
                              , gpp->ainfo_ptr->last_frame_nr);
      gpp->play_current_framenr = CLAMP(gpp->ainfo_ptr->curr_frame_nr
                              , gpp->begin_frame
                              , gpp->end_frame);
    }
    else
    {
      gpp->begin_frame = gpp->ainfo_ptr->curr_frame_nr;
      gpp->end_frame = gpp->ainfo_ptr->last_frame_nr;
      gpp->play_current_framenr = gpp->ainfo_ptr->curr_frame_nr;
    }

    gpp->pb_stepsize = 1;
    if((gpp->speed < 1.0) || (gpp->speed > 250.0))
    {
       gpp->speed = gpp->original_speed;
    }
    gpp->prev_speed = gpp->original_speed;
    
    if((gpp->pv_pixelsize < GAP_PLAYER_MIN_SIZE) || (gpp->pv_pixelsize > GAP_PLAYER_MAX_SIZE))
    {
      gpp->pv_pixelsize = 256;
    }
    if ((gpp->pv_width < GAP_PLAYER_MIN_SIZE) || (gpp->pv_width > GAP_PLAYER_MAX_SIZE))
    {
      gpp->pv_width = 256;
    }
    if ((gpp->pv_height < GAP_PLAYER_MIN_SIZE) || (gpp->pv_height > GAP_PLAYER_MAX_SIZE))
    {
      gpp->pv_height = 256;
    }

    gpp->in_feedback = FALSE;
    gpp->in_timer_playback = FALSE;
    gpp->in_resize = TRUE;              /* blocks resize chain reaction on initil size allocation event */
    gpp->resize_count = 0;              /* enable one-shot of size allocation event */
    gpp->old_resize_width = 0;
    gpp->old_resize_height = 0;
    gpp->shell_window = p_create_player_window(gpp);

    p_display_frame(gpp, gpp->play_current_framenr);
    p_pview_repaint(gpp->pv_ptr);
    p_check_tooltips();
    if(gpp->autostart)
    {
       on_play_button_clicked(NULL, gpp);
    }
    gtk_widget_show (gpp->shell_window);
    gpp->in_resize = FALSE;
    gtk_main ();
    
    if(gpp->gtimer)
    {
      g_timer_destroy(gpp->gtimer);
      gpp->gtimer = NULL;
    }

  }


  p_free_ainfo(&gpp->ainfo_ptr);

}  /* end p_playback_dialog */