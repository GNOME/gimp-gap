/* gap_cme_callbacks.c
 *
 * This Module contains GUI Procedure Callbacks for the common Video Encoder
 * Master dialog
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
 * version 2.1.0a;  2004.05.07   hof: ported to gtk2+2 and integration into gimp-gap project
 * version 1.2.1a;  2001.04.08   hof: created
 */


#include <config.h>

#include <time.h>

#include <glib/gstdio.h>

#include <gtk/gtk.h>
#include "gap-intl.h"

#include "gap_libgapbase.h"

#include "gap_cme_main.h"
#include "gap_cme_gui.h"
#include "gap_gve_sox.h"
#include "gap_gve_story.h"
#include "gap_cme_callbacks.h"

static void            p_start_encoder_status_poll_timer(GapCmeGlobalParams *gpp);
static void            p_remove_encoder_status_poll_timer(GapCmeGlobalParams *gpp);
static void            p_drop_chache_and_start_video_encoder(GapCmeGlobalParams *gpp);
static void            on_encoder_status_poll_timer(gpointer   user_data);


static void
p_start_encoder_status_poll_timer(GapCmeGlobalParams *gpp)
{
  if(gpp)
  {
    gpp->encoder_status_poll_timertag =
       (gint32) g_timeout_add(250, (GSourceFunc) on_encoder_status_poll_timer, gpp);
  }
}
static void
p_remove_encoder_status_poll_timer(GapCmeGlobalParams *gpp)
{
  if(gpp)
  {
    if(gpp->encoder_status_poll_timertag >= 0)
    {
      g_source_remove(gpp->encoder_status_poll_timertag);
      gpp->productive_encoder_timertag = -1;
    }
  }
}

static void
on_encoder_status_poll_timer(gpointer   user_data)
{
  GapCmeGlobalParams *gpp;

  if(gap_debug)
  {
    gap_file_printf("\n on_encoder_status_poll_timer: START\n");
  }

  gpp = (GapCmeGlobalParams *)user_data;
  
  if(gpp)
  {
    p_remove_encoder_status_poll_timer(gpp);
    gap_cme_gui_update_encoder_status(gpp);
    
    if(gpp->video_encoder_run_state == GAP_CME_ENC_RUN_STATE_RUNNING)
    {
      /* restart timer for next poll cycle */
      p_start_encoder_status_poll_timer(gpp);
    }
  }
  
}  /* end on_encoder_status_poll_timer */

static void
on_productive_encoder_start(gpointer   user_data)
{
  GapCmeGlobalParams *gpp;

  if(gap_debug)
  {
    gap_file_printf("\n on_productive_encoder_start: START\n");
  }

  gpp = (GapCmeGlobalParams *)user_data;
  
  if(gpp)
  {
    if(gpp->productive_encoder_timertag >= 0)
    {
      if(gap_debug)
      {
        gap_file_printf("MAIN on_productive_encoder_start remove productive_encoder_timertag:%d\n", (int)gpp->productive_encoder_timertag);
      }
      g_source_remove(gpp->productive_encoder_timertag);
      gpp->productive_encoder_timertag = -1;
    }

    if(gap_debug)
    {
      gap_file_printf("MAIN on_productive_encoder_start VIDEO ENCODER START ------------------\n");
    }

    gap_cme_gui_start_video_encoder_as_thread(gpp);


    if(gap_debug)
    {
      gap_file_printf("MAIN on_productive_encoder_start VIDEO ENCODER finished ------------------\n");
    }
    p_start_encoder_status_poll_timer(gpp);
  }
  
}  /* end on_productive_encoder_start */

/* ----------------------------------------
 * p_drop_chache_and_start_video_encoder
 * ----------------------------------------
 */
static void
p_drop_chache_and_start_video_encoder(GapCmeGlobalParams *gpp)
{
  /* delete images in the cache
   * (the cache may have been filled while parsing
   * storyboard file in the common dialog
   */
  gap_gve_story_drop_image_cache();
  if(gap_debug)
  {
    gap_file_printf("MAIN after gap_gve_story_drop_image_cache ------------------\n");
  }

  /* start timer (encoder start after 800 millisecs) */
  gpp->productive_encoder_timertag =
    (gint32) g_timeout_add(800, (GSourceFunc) on_productive_encoder_start, gpp);


}  /* end p_drop_chache_and_start_video_encoder */


/* ------------------------------------------
 * p_switch_gui_to_running_encoder_state
 * ------------------------------------------
 * disable video output entry and all existing notebook tabs,
 * except the last one (the encoder status frame)
 * (that is updated while encoder thread is running via polling)
 * then establish the encoding state.
 */
static void
p_switch_gui_to_running_encoder_state(GapCmeGlobalParams *gpp)
{
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *notebook;
  gint idx;
  gint npages;
 
  gtk_widget_set_sensitive(gpp->cme__entry_video, FALSE);
  gtk_widget_set_sensitive(gpp->cme__button_video_filesel, FALSE);

  frame = gpp->cme__encoder_status_frame;
  notebook = gpp->cme__notebook;

  gtk_widget_show(frame);
  npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK (notebook));
  for (idx = 0; idx < npages -1; idx++)
  {
    gtk_widget_set_sensitive(gtk_notebook_get_nth_page(GTK_NOTEBOOK (notebook), idx), FALSE);
  }

  /* store encoder start time */
  gpp->encoder_started_on_utc_seconds = time(NULL);

  /* show last tab (-1) of the notbook */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), -1);

  gpp->video_encoder_run_state =  GAP_CME_ENC_RUN_STATE_RUNNING;

}  /* end p_switch_gui_to_running_encoder_state */


/* ------------------------------------------------------ */
/* BEGIN callbacks                                        */
/* ------------------------------------------------------ */


/* ---------------------------------
 * on_cme__response
 * ---------------------------------
 */
void
on_cme__response (GtkWidget *widget,
                 gint       response_id,
                 GapCmeGlobalParams *gpp)
{
  GtkWidget *dialog;

  if(gpp)
  {
    gpp->val.run = FALSE;
  }

  switch (response_id)
  {
    case GTK_RESPONSE_OK:
      if(gpp)
      {
        switch(gpp->video_encoder_run_state)
        {
          case GAP_CME_ENC_RUN_STATE_READY:
            if(gpp->val.gui_proc_thread)
            {
              if(gap_cme_gui_check_gui_thread_is_active(gpp))
              {
                return;
              }
            }
            if(gpp->ow__dialog_window != NULL)
            {
              /* Overwrite dialog is already open
               * but the User pressed the OK button in the qte main dialog again.
               */
              gtk_window_present(GTK_WINDOW(gpp->ow__dialog_window));
              return;
            }
            if(FALSE == gap_cme_gui_check_encode_OK (gpp))
            {
              return;  /* can not start, illegal parameter value combinations */
            }
            gpp->val.run = TRUE;
            p_switch_gui_to_running_encoder_state(gpp);
            p_drop_chache_and_start_video_encoder(gpp);
            return;
            break;
          case GAP_CME_ENC_RUN_STATE_RUNNING:
            return;  /* ignore further clicks on OK button while encoder is running */
            break;
          case GAP_CME_ENC_RUN_STATE_FINISHED:
            /* close the master encoder window if OK button clicked after while encoder has finished */
            break;
        }
      }  
      

      /* now run into the default case, to close the shell_window (dont break) */
      
    default:
      dialog = NULL;
      if(gpp)
      {
        gap_gve_misc_set_master_encoder_cancel_request(&gpp->encStatus, TRUE);

        gap_cme_gui_remove_poll_timer(gpp);
        p_remove_encoder_status_poll_timer(gpp);
        dialog = gpp->shell_window;
        if(dialog)
        {
          gpp->shell_window = NULL;
          gtk_widget_destroy (dialog);
        }

        gtk_main_quit ();

      }
      else
      {
        gtk_main_quit ();
      }

      break;
  }
}  /* end on_cme__response */




/* ------------------------
 * on_cme__combo_enocder
 * ------------------------
 */
void
on_cme__combo_enocder  (GtkWidget     *widget,
                        GapCmeGlobalParams *gpp)
{
  gint       value;
  GapGveEncList *l_ecp;

  if(gap_debug) gap_file_printf("CB: on_cme__combo_encoder\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  l_ecp = gpp->ecp;
  while(l_ecp)
  {
    if(l_ecp->menu_nr == value)
    {
      /* found encoder element with wanted menu_nr */
      break;
    }
    l_ecp = (GapGveEncList *)l_ecp->next;
  }

  if(l_ecp)
  {
     if(gap_debug)
     {
        gap_file_printf("CB: on_cme__combo_encoder index: %d, %s, plugin: %s\n"
               , (int)l_ecp->menu_nr
               , l_ecp->menu_name
               , l_ecp->vid_enc_plugin);
     }

    gtk_label_set_text(GTK_LABEL(gpp->cme__label_active_encoder_name), l_ecp->menu_name);

     /* copy the selected ecp record */
     memcpy(&gpp->val.ecp_sel, l_ecp, sizeof(GapGveEncList));
     gap_cme_gui_upd_wgt_sensitivity(gpp);
     gap_cme_gui_upd_vid_extension(gpp);
  }
}  /* end on_cme__combo_enocder */


/* ------------------------
 * on_cme__combo_scale
 * ------------------------
 */
void
on_cme__combo_scale  (GtkWidget     *widget,
                      GapCmeGlobalParams *gpp)
{
  gint       value;
  gint       l_idx;
  static gint  tab_width[GAP_CME_STANDARD_SIZE_MAX_ELEMENTS] =  { 0, 320, 320, 640, 720, 720, 1280, 1920, 1920 };
  static gint  tab_height[GAP_CME_STANDARD_SIZE_MAX_ELEMENTS] = { 0, 240, 288, 480, 480, 576,  720, 1080, 1088 };

  if(gap_debug) gap_file_printf("CB: on_cme__combo_scale\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  l_idx = value;
  if(gap_debug) 
  {
    gap_file_printf("CB: on_cme__combo_scale index: %d\n", (int)l_idx);
  }
  
  if(l_idx == GAP_CME_STANDARD_SIZE_KEEP)
  {
    /* the "unchanged" entry selection does nothing */ 
    return;
  }

  if((l_idx >= GAP_CME_STANDARD_SIZE_MAX_ELEMENTS) || (l_idx < 1))
  {
     l_idx = 0;
     if(!gap_cme_gui_check_gui_thread_is_active(gpp))
     {
       tab_width[0] = gimp_image_width(gpp->val.image_ID);
       tab_height[0] = gimp_image_height(gpp->val.image_ID);
     }
  }

  if(gpp->cme__spinbutton_width_adj)
  {
    gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_width_adj)
                            , (gfloat)tab_width[l_idx]);
  }

  if(gpp->cme__spinbutton_height_adj)
  {
    gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_height_adj)
                          , (gfloat)tab_height[l_idx]);
  }
}  /* end on_cme__combo_scale */


/* ------------------------
 * on_cme__combo_framerate
 * ------------------------
 */
void
on_cme__combo_framerate  (GtkWidget     *widget,
                          GapCmeGlobalParams *gpp)
{
  gint       value;
  gint       l_idx;
  static gdouble  tab_framerate[GAP_CME_STANDARD_FRAMERATE_MAX_ELEMENTS]
                  =  { 0, 23.98, 24, 25, 29.97, 30, 50, 59.94, 60, 1, 5, 10, 12, 15 };

  if(gap_debug) gap_file_printf("CB: on_cme__combo_framerate\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  l_idx = value;
  
  if(l_idx == GAP_CME_STANDARD_FRAMERATE_KEEP)
  {
    return;
  }

  if(gap_debug) gap_file_printf("CB: on_cme__combo_framerate index: %d\n", (int)l_idx);
  if((l_idx >= GAP_CME_STANDARD_FRAMERATE_MAX_ELEMENTS) || (l_idx < 1))
  {
     l_idx = 0;
     tab_framerate[0] = gpp->ainfo.framerate;
  }

  if(gpp->cme__spinbutton_framerate_adj)
  {
    gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_framerate_adj)
                            , (gfloat)tab_framerate[l_idx]);
  }
}  /* end on_cme__combo_framerate */


/* ---------------------------
 * on_cme__combo_outsamplerate
 * ---------------------------
 */
void
on_cme__combo_outsamplerate  (GtkWidget     *widget,
                              GapCmeGlobalParams *gpp)
{
  gint       value;

  if(gap_debug) gap_file_printf("CB: on_cme__combo_outsamplerate\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);

  if(gap_debug)
  {
    gap_file_printf("CB: on_cme__combo_outsamplerate value: %d\n", (int)value);
  }



  if(gpp->cme__spinbutton_samplerate_adj)
  {
    gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_samplerate_adj)
                           , (gfloat)value);
  }
}  /* end on_cme__combo_outsamplerate */


/* ------------------------
 * on_cme__combo_vid_norm
 * ------------------------
 */
void
on_cme__combo_vid_norm  (GtkWidget     *widget,
                         GapCmeGlobalParams *gpp)
{
  gint       value;

  if(gap_debug) gap_file_printf("CB: on_cme__combo_vid_norm\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);

  if(gap_debug) gap_file_printf("CB: on_cme__combo_vid_norm : %d\n", (int)value);

  gpp->val.vid_format = value;
  
}  /* end on_cme__combo_vid_norm */


/* ---------------------------------
 * fsv fileselct videofile callbacks
 * ---------------------------------
 */

void
on_fsv__fileselection_destroy          (GtkObject       *object,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_fsv__fileselection_destroy\n");
 if(gpp == NULL) return;

 gpp->fsv__fileselection = NULL;
}


void
on_fsv__button_OK_clicked              (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
  const gchar *filename;
  GtkEntry *entry;

 if(gap_debug) gap_file_printf("CB: on_fsv__button_OK_clicked\n");
 if(gpp == NULL) return;


 if(gpp->fsv__fileselection)
 {
   filename =  gtk_file_selection_get_filename (GTK_FILE_SELECTION (gpp->fsv__fileselection));
   g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s"
             ,filename );
   entry = GTK_ENTRY(gpp->cme__entry_video);
   if(entry)
   {
      gtk_entry_set_text(entry, filename);
   }
   on_fsv__button_cancel_clicked(NULL, (gpointer)gpp);
 }
}


void
on_fsv__button_cancel_clicked          (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_fsv__button_cancel_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsv__fileselection)
 {
   gtk_widget_destroy(gpp->fsv__fileselection);
   gpp->fsv__fileselection = NULL;
 }
}


/* ---------------------------------
 * fsb fileselct filtermacro file callbacks
 * ---------------------------------
 */

void
on_fsb__fileselection_destroy          (GtkObject       *object,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_fsb__fileselection_destroy\n");
 if(gpp == NULL) return;

 gpp->fsb__fileselection = NULL;
}


void
on_fsb__button_OK_clicked              (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
  const gchar *filename;
  GtkEntry *entry;

 if(gap_debug) gap_file_printf("CB: on_fsb__button_OK_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsb__fileselection)
 {
   filename =  gtk_file_selection_get_filename (GTK_FILE_SELECTION (gpp->fsb__fileselection));
   g_snprintf(gpp->val.filtermacro_file, sizeof(gpp->val.filtermacro_file), "%s"
           , filename);
   entry = GTK_ENTRY(gpp->cme__entry_mac);
   if(entry)
   {
      gtk_entry_set_text(entry, filename);
   }
   on_fsb__button_cancel_clicked(NULL, (gpointer)gpp);
 }
}


void
on_fsb__button_cancel_clicked          (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_fsb__button_cancel_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsb__fileselection)
 {
   gtk_widget_destroy(gpp->fsb__fileselection);
   gpp->fsb__fileselection = NULL;
 }
}


/* ---------------------------------
 * fss fileselct storyboard file callbacks
 * ---------------------------------
 */

void
on_fss__fileselection_destroy          (GtkObject       *object,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_fss__fileselection_destroy\n");
 if(gpp == NULL) return;

 gpp->fss__fileselection = NULL;
}


void
on_fss__button_OK_clicked              (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
  const gchar *filename;
  GtkEntry *entry;

 if(gap_debug) gap_file_printf("CB: on_fss__button_OK_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fss__fileselection)
 {
   filename =  gtk_file_selection_get_filename (GTK_FILE_SELECTION (gpp->fss__fileselection));
   g_snprintf(gpp->val.storyboard_file, sizeof(gpp->val.storyboard_file), "%s"
           , filename);
   entry = GTK_ENTRY(gpp->cme__entry_stb);
   on_fss__button_cancel_clicked(NULL, (gpointer)gpp);
   if(entry)
   {
      gtk_entry_set_text(entry, filename);
   }
 }
}


void
on_fss__button_cancel_clicked          (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_fss__button_cancel_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fss__fileselection)
 {
   gtk_widget_destroy(gpp->fss__fileselection);
   gpp->fss__fileselection = NULL;
 }
}



/* ---------------------------------
 * fsa fileselct audiofile callbacks
 * ---------------------------------
 */


void
on_fsa__fileselection_destroy          (GtkObject       *object,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_fsa__fileselection_destroy\n");
 if(gpp == NULL) return;

 gpp->fsa__fileselection = NULL;
}


void
on_fsa__button_OK_clicked              (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
  const gchar *filename;
  GtkEntry *entry;

 if(gap_debug) gap_file_printf("CB: on_fsa__button_OK_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsa__fileselection)
 {
   filename =  gtk_file_selection_get_filename (GTK_FILE_SELECTION (gpp->fsa__fileselection));
   g_snprintf(gpp->val.audioname1, sizeof(gpp->val.audioname1), "%s"
           , filename);
   entry = GTK_ENTRY(gpp->cme__entry_audio1);
   if(entry)
   {
      gtk_entry_set_text(entry, filename);
   }
   on_fsa__button_cancel_clicked(NULL, (gpointer)gpp);
 }
}


void
on_fsa__button_cancel_clicked          (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_fsa__button_cancel_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsa__fileselection)
 {
   gtk_widget_destroy(gpp->fsa__fileselection);
   gpp->fsa__fileselection = NULL;
 }
}


/* ---------------------------------
 * Overwrite Dialog
 * ---------------------------------
 */

void
on_ow__dialog_destroy                  (GtkObject       *object,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_ow__dialog_destroy\n");

 if(gpp)
 {
   gtk_widget_destroy (gpp->ow__dialog_window);
 }

 gtk_main_quit ();
}


void
on_ow__button_one_clicked              (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_ow__button_one_clicked\n");

 if(gpp)
 {
   gpp->val.ow_mode = 0;
   gtk_widget_destroy (gpp->ow__dialog_window);
 }
 gtk_main_quit ();
}



void
on_ow__button_cancel_clicked           (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_ow__button_cancel_clicked\n");

 if(gpp)
 {
   gpp->val.ow_mode = -1;
   gtk_widget_destroy (gpp->ow__dialog_window);
 }
 gtk_main_quit ();
}


/* ---------------------------------
 * Quicktime ENCODER Dialog
 * ---------------------------------
 */

void
on_cme__button_params_clicked           (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_cme__button_params_clicked\n");

 if(gpp)
 {
     /* generic call of GAP video encoder specific GUI_PROC  plugin */
     gap_cme_gui_pdb_call_encoder_gui_plugin(gpp);
 }
}


void
on_cme__spinbutton_width_changed       (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkAdjustment *adj;

 if(gap_debug) gap_file_printf("CB: on_cme__spinbutton_width_changed\n");

 if(gpp == NULL) return;

 adj = GTK_ADJUSTMENT(gpp->cme__spinbutton_width_adj);
 if(adj)
 {
   if(gap_debug) gap_file_printf("vid_width spin value: %f\n", (float)adj->value );

   if((gint)adj->value != gpp->val.vid_width)
   {
     gpp->val.vid_width = (gint)adj->value;
     if(gpp->cme__combo_scale != NULL)
     {
       gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (gpp->cme__combo_scale), GAP_CME_STANDARD_SIZE_KEEP);
     }
    
   }
 }

}


void
on_cme__spinbutton_height_changed      (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkAdjustment *adj;

 if(gap_debug) gap_file_printf("CB: on_cme__spinbutton_height_changed\n");

 if(gpp == NULL) return;

 adj = GTK_ADJUSTMENT(gpp->cme__spinbutton_height_adj);
 if(adj)
 {
   if(gap_debug) gap_file_printf("vid_height spin value: %f\n", (float)adj->value );

   if((gint)adj->value != gpp->val.vid_height)
   {
     gpp->val.vid_height = (gint)adj->value;
     if(gpp->cme__combo_scale != NULL)
     {
       gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (gpp->cme__combo_scale), GAP_CME_STANDARD_SIZE_KEEP);
     }
   }
 }

}


void
on_cme__spinbutton_from_changed        (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkAdjustment *adj;

 if(gap_debug) gap_file_printf("CB: on_cme__spinbutton_from_changed\n");

 if(gpp == NULL) return;

 adj = GTK_ADJUSTMENT(gpp->cme__spinbutton_from_adj);
 if(adj)
 {
   if(gap_debug) gap_file_printf("range_from spin value: %f\n", (float)adj->value );

   if((gint)adj->value != gpp->val.range_from)
   {
     gpp->val.range_from = (gint)adj->value;
     gap_cme_gui_update_vid_labels (gpp);
   }
 }
}


void
on_cme__spinbutton_to_changed          (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkAdjustment *adj;

 if(gap_debug) gap_file_printf("CB: on_cme__spinbutton_to_changed\n");

 if(gpp == NULL) return;

 adj = GTK_ADJUSTMENT(gpp->cme__spinbutton_to_adj);
 if(adj)
 {
   if(gap_debug) gap_file_printf("range_to spin value: %f\n", (float)adj->value );

   if((gint)adj->value != gpp->val.range_to)
   {
     gpp->val.range_to = (gint)adj->value;
     gap_cme_gui_update_vid_labels (gpp);
   }
 }
}


void
on_cme__spinbutton_framerate_changed   (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkAdjustment *adj;

 if(gap_debug) gap_file_printf("CB: on_cme__spinbutton_framerate_changed\n");


 if(gpp == NULL) return;

 adj = GTK_ADJUSTMENT(gpp->cme__spinbutton_framerate_adj);
 if(adj)
 {
   if(gap_debug) gap_file_printf("framerate spin value: %f\n", (float)adj->value );

   if((gint)adj->value != gpp->val.framerate)
   {
     gpp->val.framerate = adj->value;
     if(gpp->cme__combo_framerate != NULL)
     {
       gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (gpp->cme__combo_framerate), GAP_CME_STANDARD_FRAMERATE_KEEP);
     }
     gap_cme_gui_update_vid_labels (gpp);
     
   }
 }

}



void
on_cme__entry_audio1_changed           (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkEntry *entry;

 if(gap_debug) 
 {
   gap_file_printf("CB: on_cme__entry_audio1_changed\n");
 }
 if(gpp == NULL) return;

 entry = GTK_ENTRY(gpp->cme__entry_audio1);
 if(entry)
 {
    g_snprintf(gpp->val.audioname1, sizeof(gpp->val.audioname1), "%s"
              , gtk_entry_get_text(entry));

    /* remove tmp_audfile on each change of the audiofile */
    if(gpp->val.tmp_audfile[0] != '\0')
    {
      if(g_file_test(gpp->val.tmp_audfile, G_FILE_TEST_EXISTS))
      {
        g_remove(gpp->val.tmp_audfile);
      }
      gpp->val.tmp_audfile[0] = '\0';
    }
    gap_cme_gui_update_aud_labels (gpp);
    if(gpp->val.wav_samplerate1 > 0)
    {
      gpp->val.samplerate = gpp->val.wav_samplerate1;
      gtk_adjustment_set_value(GTK_ADJUSTMENT(gpp->cme__spinbutton_samplerate_adj)
                              , (gfloat)gpp->val.samplerate);
    }
 }


}


void
on_cme__button_audio1_clicked          (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_cme__button_audio1_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsa__fileselection == NULL)
 {
   gpp->val.audioname_ptr = &gpp->val.audioname1[0];
   gpp->fsa__fileselection = gap_cme_gui_create_fsa__fileselection(gpp);
   gtk_file_selection_set_filename (GTK_FILE_SELECTION (gpp->fsa__fileselection),
                                    gpp->val.audioname1);

   gtk_widget_show (gpp->fsa__fileselection);
 }
}


void
on_cme__spinbutton_samplerate_changed  (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkAdjustment *adj;

 if(gap_debug) gap_file_printf("CB: on_cme__spinbutton_samplerate_changed\n");

 if(gpp == NULL) return;

 adj = GTK_ADJUSTMENT(gpp->cme__spinbutton_samplerate_adj);
 if(adj)
 {
   gint gintValue;
   
   if(gap_debug)
   {
     gap_file_printf("samplerate spin value: %f\n", (float)adj->value );
   }
   gintValue = (gint)adj->value;
   
   if(gintValue != gpp->val.samplerate)
   {
     gpp->val.samplerate = gintValue;
     gap_cme_gui_update_aud_labels (gpp);
   }

   switch (gintValue)
   {
     case 8000:
     case 11025:
     case 12000:
     case 16000:
     case 22050:
     case 24000:
     case 32000:
     case 44100:
     case 48000:
       if (gpp->cme__combo_outsamplerate != NULL)
       {
         if(gap_debug)
         {
           gap_file_printf("detected a commonly used samplerate value: %d\n", (int)gintValue );
         }
         gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (gpp->cme__combo_outsamplerate)
             , gintValue);
       }
       break;
     default:
       break;
   }


 }

}


void
on_cme__entry_video_changed             (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkEntry *entry;

 if(gap_debug) gap_file_printf("CB: on_cme__entry_video_changed\n");
 if(gpp == NULL) return;

 entry = GTK_ENTRY(gpp->cme__entry_video);
 if(entry)
 {
    g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s"
              , gtk_entry_get_text(entry));
 }
}


void
on_cme__button_video_clicked           (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_cme__button_video_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsv__fileselection == NULL)
 {
   gpp->fsv__fileselection = gap_cme_gui_create_fsv__fileselection(gpp);
   gtk_file_selection_set_filename (GTK_FILE_SELECTION (gpp->fsv__fileselection),
                                    gpp->val.videoname);

   gtk_widget_show (gpp->fsv__fileselection);
 }
}


void
on_cme__button_sox_save_clicked        (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: \n");

  if(gpp)
  {
    if(gap_cme_gui_check_gui_thread_is_active(gpp)) return;
    gap_gve_sox_save_config(&gpp->val);
  }
}


void
on_cme__button_sox_load_clicked        (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: \n");

  if(gpp)
  {
    if(gap_cme_gui_check_gui_thread_is_active(gpp)) return;
    gap_gve_sox_init_config(&gpp->val);
    gap_cme_gui_util_sox_widgets(gpp);
  }
}


void
on_cme__button_sox_default_clicked     (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_cme__button_sox_default_clicked\n");

  if(gpp)
  {
    gap_gve_sox_init_default(&gpp->val);
    gap_cme_gui_util_sox_widgets(gpp);
  }
}


void
on_cme__entry_sox_changed              (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_cme__entry_sox_changed\n");

 if(gpp)
 {
    g_snprintf(gpp->val.util_sox, sizeof(gpp->val.util_sox), "%s"
              , gtk_entry_get_text(GTK_ENTRY(editable)));
 }
}


void
on_cme__entry_sox_options_changed      (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_cme__entry_sox_options_changed\n");

 if(gpp)
 {
    g_snprintf(gpp->val.util_sox_options, sizeof(gpp->val.util_sox_options), "%s"
              , gtk_entry_get_text(GTK_ENTRY(editable)));
 }
}



void
on_cme__button_gen_tmp_audfile_clicked (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_cme__button_gen_tmp_audfile_clicked\n");

 if(gpp)
 {
    gap_gve_sox_chk_and_resample(&gpp->val);
    gap_cme_gui_update_aud_labels(gpp);
 }
}


void
on_cme__entry_mac_changed              (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkEntry *entry;

 if(gap_debug) gap_file_printf("CB: on_cme__entry_mac_changed\n");
 if(gpp == NULL) return;

 entry = GTK_ENTRY(gpp->cme__entry_mac);
 if(entry)
 {
    g_snprintf(gpp->val.filtermacro_file, sizeof(gpp->val.filtermacro_file), "%s"
              , gtk_entry_get_text(entry));
 }
}


void
on_cme__button_mac_clicked             (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_cme__button_mac_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsb__fileselection == NULL)
 {
   gpp->fsb__fileselection = gap_cme_gui_create_fsb__fileselection(gpp);
   gtk_file_selection_set_filename (GTK_FILE_SELECTION (gpp->fsb__fileselection),
                                    gpp->val.filtermacro_file);

   gtk_widget_show (gpp->fsb__fileselection);
 }

}


void
on_cme__entry_stb_changed              (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkEntry *entry;

 if(gap_debug) gap_file_printf("CB: on_cme__entry_stb_changed\n");
 if(gpp == NULL) return;

 entry = GTK_ENTRY(gpp->cme__entry_stb);
 if(entry)
 {
    g_snprintf(gpp->val.storyboard_file, sizeof(gpp->val.storyboard_file), "%s"
              , gtk_entry_get_text(entry));

    gpp->storyboard_create_composite_audio = FALSE;
    gap_cme_gui_check_storyboard_file(gpp);
 }

}


void
on_cme__button_stb_clicked             (GtkButton       *button,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_cme__button_stb_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fss__fileselection == NULL)
 {
   gpp->fss__fileselection = gap_cme_gui_create_fss__fileselection(gpp);
   gtk_file_selection_set_filename (GTK_FILE_SELECTION (gpp->fss__fileselection),
                                    gpp->val.storyboard_file);

   gtk_widget_show (gpp->fss__fileselection);
 }

}



void
on_cme__button_stb_audio_clicked     (GtkButton       *button,
                                      GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_cme__button_stb_audio_clicked\n");

  if(gpp)
  {
    if(gpp->val.storyboard_file[0] != '\0')
    {
      if(g_file_test(gpp->val.storyboard_file, G_FILE_TEST_EXISTS))
      {
        gpp->storyboard_create_composite_audio = TRUE;
        gap_cme_gui_check_storyboard_file(gpp);
      }
    }
  }
}



void
on_cme__entry_debug_multi_changed      (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkEntry *entry;

 if(gap_debug) gap_file_printf("CB: on_cme__entry_debug_multi_changed\n");
 if(gpp == NULL) return;

 entry = GTK_ENTRY(gpp->cme__entry_debug_multi);
 if(entry)
 {
    char *l_write_to;

    l_write_to = g_strdup(gtk_entry_get_text(entry));

    if(gap_debug) gap_file_printf("on_cme__entry_debug_multi_changed:l_write_to:%s:\n", l_write_to);
    gimp_set_data( GAP_VID_ENC_SAVE_MULTILAYER, l_write_to, strlen(l_write_to) +1);
    g_free(l_write_to);
 }

}


void
on_cme__entry_debug_flat_changed       (GtkEditable     *editable,
                                        GapCmeGlobalParams *gpp)
{
  GtkEntry *entry;

 if(gap_debug) gap_file_printf("CB: on_cme__entry_debug_flat_changed\n");
 if(gpp == NULL) return;

 entry = GTK_ENTRY(gpp->cme__entry_debug_flat);
 if(entry)
 {
    char *l_write_to;

    l_write_to = g_strdup(gtk_entry_get_text(entry));

    if(gap_debug) gap_file_printf("on_cme__entry_debug_multi_changed:l_write_to:%s:\n", l_write_to);
    gimp_set_data( GAP_VID_ENC_SAVE_FLAT, l_write_to, strlen(l_write_to) +1);
    g_free(l_write_to);
 }

}


void
on_cme__checkbutton_enc_monitor_toggled
                                        (GtkCheckButton *checkbutton,
                                        GapCmeGlobalParams *gpp)
{
 if(gap_debug) gap_file_printf("CB: on_cme__checkbutton_enc_monitor_toggled\n");

 if(gpp)
 {
    if (GTK_TOGGLE_BUTTON(checkbutton)->active)
    {
       gimp_set_data( GAP_VID_ENC_MONITOR, "TRUE", strlen("TRUE") +1);
    }
    else
    {
       gimp_set_data( GAP_VID_ENC_MONITOR, "FALSE", strlen("FALSE") +1);
    }
 }
}
