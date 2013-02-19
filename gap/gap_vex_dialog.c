/*
 * gap_vex_dialog.c
 *   A GIMP / GAP Plugin to extract frames and/or audio from Videofiles
 *
 *   This is a special kind of File Load Plugin,
 *     based on gap_vid_api (GVA)  (an API to read various Videoformats/CODECS)
 */

/*
 * Changelog:
 * 2004/04/10 v2.1.0:   integrated sourcecode into gimp-gap project
 */

/*
 * Copyright
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

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gap_vex_main.h"
#include "gap_vex_dialog.h"
#include "gap_lib_common_defs.h"

#include "gap-intl.h"

/* GAP_ENABLE_VIDEOAPI_SUPPORT (1) */
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

#define ENC_MENU_ITEM_INDEX_KEY "gap_enc_menu_item_index"

#define GAP_VEX_PLAYER_HELP_ID    "plug-in-gap-extract-player"

#define SPIN_WIDTH_SMALL 40
#define SPIN_WIDTH_LARGE 80
#define ENTRY_WIDTH_LARGE 320

#define TIME_UNDEF_STRING "mm:ss:msec"


/* -------- GUI TOOL PROCEDURES -----------*/
static const char *p_timeconv_framenr_to_timestr_fps( gint32 framenr, gdouble framerate);
static void        p_update_time_labels   (GapVexMainGlobalParams *gpp);
static void        p_update_range_widgets(GapVexMainGlobalParams *gpp);
static void        p_update_wgt_sensitivity(GapVexMainGlobalParams *gpp);
static void        p_init_mw__main_window_widgets (GapVexMainGlobalParams *gpp);
static void        p_call_player_widget(GapVexMainGlobalParams *gpp
                                       , char *imagename
                                       , gint32 imagewidth
                                       , gint32 imageheight
                                       , gint32 begin_frame
                                       , gint32 end_frame
                                       , gint32 seltrack
                                       , gdouble delace
                                       , gboolean docked_mode
                                       );
static void        p_vex_set_range_cb(GapPlayerAddClip *plac_ptr);
static void        p_check_videofile(GapVexMainGlobalParams *gpp);

/* -------- GUI CALLBACK PROCEDURES -----------*/
static void        on_mw_response (GtkWidget *widget,
                                   gint       response_id,
                                   GapVexMainGlobalParams *gpp);

static void        on_mw__combo_preferred_decoder  (GtkWidget     *widget,
                                                   GapVexMainGlobalParams *gpp);
static void        on_mw__combo_deinterlace  (GtkWidget     *widget,
                                              GapVexMainGlobalParams *gpp);
static void        on_mw__combo_mode_multilayer  (GtkWidget     *widget,
                                              GapVexMainGlobalParams *gpp);

static void        on_mw__checkbutton_bluebox_toggled (GtkToggleButton *togglebutton,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__checkbutton_disable_mmx_toggled (GtkToggleButton *togglebutton,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__spinbutton_begin_frame_changed  (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__spinbutton_end_frame_changed    (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__spinbutton_videotrack_changed   (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__spinbutton_audiotrack_changed   (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__entry_video_changed             (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__button_video_clicked            (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__entry_basename_changed          (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__button_basename_clicked         (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__entry_extension_changed         (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__spinbutton_basenum_changed      (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__entry_audiofile_changed         (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__button_audiofile_clicked        (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__button_vrange_dialog_clicked    (GtkButton       *button,
                                        GdkEventButton  *bevent,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__button_vrange_docked_clicked    (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__entry_preferred_decoder_changed (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__checkbutton_exact_seek_toggled  (GtkToggleButton *togglebutton,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__spinbutton_delace_threshold_changed
                                        (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);
static void        on_mw__spinbutton_fn_digits_changed    (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp);



static void        on_fsv__fileselection_destroy          (GtkObject       *object,
                                        GapVexMainGlobalParams *gpp);
static void        on_fsv__button_OK_clicked              (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp);
static void        on_fsv__button_cancel_clicked          (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp);
static void        on_fsb__fileselection_destroy          (GtkObject       *object,
                                        GapVexMainGlobalParams *gpp);
static void        on_fsb__button_OK_clicked              (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp);
static void        on_fsb__button_cancel_clicked          (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp);
static void        on_fsa__fileselection_destroy          (GtkObject       *object,
                                        GapVexMainGlobalParams *gpp);
static void        on_fsa__button_OK_clicked              (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp);
static void        on_fsa__button_cancel_clicked          (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp);


/* -------- GUI DIALOG Creationg PROCEDURES -----------*/
static GtkWidget* create_fsv__fileselection (GapVexMainGlobalParams *gpp);
static GtkWidget* create_fsb__fileselection (GapVexMainGlobalParams *gpp);
static GtkWidget* create_fsa__fileselection (GapVexMainGlobalParams *gpp);

/* PUBLIC: gap_vex_dlg_create_mw__main_window */



#define DEFAULT_BASENAME "frame_"
#define DEFAULT_STORYBASENAME "STORYBOARD.txt"

/* ------------------------
 * gap_vex_dlg_init_gpp
 * ------------------------
 */
void
gap_vex_dlg_init_gpp (GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("INIT: init_global params\n");

 /* set initial values in val */

 g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "TEST.MPG");
 g_snprintf(gpp->val.basename, sizeof(gpp->val.basename), DEFAULT_BASENAME);
 g_snprintf(gpp->val.extension, sizeof(gpp->val.extension), ".xcf");
 g_snprintf(gpp->val.audiofile, sizeof(gpp->val.audiofile), "frame.wav");
 gpp->val.basenum = 0;
 gpp->val.fn_digits = GAP_LIB_DEFAULT_DIGITS;
 gpp->val.preferred_decoder[0] = '\0';
 gpp->val.deinterlace = 0;
 gpp->val.delace_threshold = 1.0;
 gpp->val.exact_seek = 0;

 gpp->val.begin_frame = 1.0;
 gpp->val.end_frame = 1.0;
 gpp->val.videotrack = 1;
 gpp->val.audiotrack = 1;

 gpp->val.multilayer = 0;
 gpp->val.disable_mmx = 0;
 gpp->val.pos_unit = 0;   /* unit FRAMES */

 gpp->plp = NULL;
 gpp->in_player_call = FALSE;
 gpp->video_width = 320;
 gpp->video_height = 200;

 gpp->val.image_ID = -1;
}  /* end gap_vex_dlg_init_gpp */

/* ----------------------------
 * gap_vex_dlg_overwrite_dialog
 * ----------------------------
 * check if filename already exists.
 * if file exists and we are running interactive
 *     Ask the user what to do in an overwrite dialog window.
 *   in non interactive runmode the parameter overwrite_mode
 *   is used for overwrite permission.
 *
 * return -1 overwrite is NOT allowed, calling program should skip write (and exit)
 *         0 OVERWRITE permission for one file
 *         1 OVERWRITE permission for all files
 */
gint
gap_vex_dlg_overwrite_dialog(GapVexMainGlobalParams *gpp, gchar *filename, gint overwrite_mode)
{
  static  GapArrButtonArg  l_argv[3];
  static  GapArrArg  argv[1];

  if(gap_lib_file_exists(filename))

  if(g_file_test(filename, G_FILE_TEST_EXISTS))
  {
    if (overwrite_mode < 1)
    {
       gchar *msg;
       gint l_rc;

       l_argv[0].but_txt  = _("Overwrite File");
       l_argv[0].but_val  = 0;
       l_argv[1].but_txt  = _("Overwrite All");
       l_argv[1].but_val  = 1;
       l_argv[2].but_txt  = GTK_STOCK_CANCEL;
       l_argv[2].but_val  = -1;

       gap_arr_arg_init(&argv[0], GAP_ARR_WGT_LABEL);
       argv[0].label_txt = filename;

       msg = g_strdup_printf(_("File: %s already exists"), filename);
       l_rc = gap_arr_std_dialog ( _("Overwrite")
                                 , msg
                                 ,  1, argv
                                 , 3, l_argv
                                 , -1
                                 );
       g_free(msg);
       return(l_rc);
    }
  }
  return (overwrite_mode);
}  /* end gap_vex_dlg_overwrite_dialog */

/* ---------------------------------
 * p_timeconv_framenr_to_timestr_fps
 * ---------------------------------
 * return constant string with converted time and framerate information
 * Do not attemt to free the returned string. (it refers to a static constant buffer)
 */
static const char *
p_timeconv_framenr_to_timestr_fps( gint32 framenr, gdouble framerate)
{
  static char  txt_buf[100];
  gint len;

  gap_timeconv_framenr_to_timestr( framenr
                           , framerate
                           , txt_buf
                           , sizeof(txt_buf)
                           );
  len = strlen(txt_buf);
  g_snprintf(&txt_buf[len], sizeof(txt_buf) - len, " @ %2.3f fps", (float)framerate);

  return (txt_buf);
}  /* end p_timeconv_framenr_to_timestr_fps */


/* --------------------------------
 * p_update_time_labels
 * --------------------------------
 */
static void
p_update_time_labels   (GapVexMainGlobalParams *gpp)
{
  if(gpp == NULL) return;

  if(!gpp->val.chk_is_compatible_videofile)
  {
    gtk_label_set_text ( GTK_LABEL(gpp->mw__begin_time_label), TIME_UNDEF_STRING);
    gtk_label_set_text ( GTK_LABEL(gpp->mw__end_time_label), TIME_UNDEF_STRING);
    return;
  }

  gtk_label_set_text ( GTK_LABEL(gpp->mw__begin_time_label)
                      , p_timeconv_framenr_to_timestr_fps((gint32)(gpp->val.begin_frame -1)
                                                         ,(gdouble)gpp->video_speed
                                                         )
                     );
  gtk_label_set_text ( GTK_LABEL(gpp->mw__end_time_label)
                      , p_timeconv_framenr_to_timestr_fps((gint32)(gpp->val.end_frame -1)
                                                         ,(gdouble)gpp->video_speed
                                                         )
                     );
}  /* end p_update_time_labels */


/* ------------------------
 * p_update_range_widgets
 * ------------------------
 */
static void
p_update_range_widgets(GapVexMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

  adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_begin_frame_adj);
  gtk_adjustment_set_value(adj, (gfloat)gpp->val.begin_frame);

  adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_end_frame_adj);
  gtk_adjustment_set_value(adj, (gfloat)gpp->val.end_frame);

  p_update_time_labels(gpp);
}  /* end p_update_range_widgets */


/* ------------------------
 * p_update_wgt_sensitivity
 * ------------------------
 */
static void
p_update_wgt_sensitivity(GapVexMainGlobalParams *gpp)
{
  gboolean   sensitive;
  gboolean   sensitive_vid;


  if(gap_debug) printf("p_update_wgt_sensitivity : START\n");
  if(gap_debug) printf("  chk_is_compatible_videofile :%d\n", (int)gpp->val.chk_is_compatible_videofile);
  if(gap_debug) printf("  chk_vtracks :%d\n", (int)gpp->val.chk_vtracks);
  if(gap_debug) printf("  chk_atracks :%d\n", (int)gpp->val.chk_atracks);
  if(gap_debug) printf("  videotrack :%d\n", (int)gpp->val.videotrack);

  if((gpp->val.chk_is_compatible_videofile)
  && ((gpp->val.videotrack > 0) || (gpp->val.audiotrack > 0))  )
  {
      sensitive = TRUE;
  }
  else
  {
      sensitive = FALSE;
  }
  if(gpp->mw__button_OK)
  {
    gtk_widget_set_sensitive(gpp->mw__button_OK, sensitive);
  }


  if(gpp->val.chk_is_compatible_videofile)
  {
      sensitive = TRUE;
  }
  else
  {
      sensitive = FALSE;
  }

  gtk_widget_set_sensitive(gpp->mw__spinbutton_begin_frame, sensitive);
  gtk_widget_set_sensitive(gpp->mw__spinbutton_end_frame, sensitive);
  gtk_widget_set_sensitive(gpp->mw__button_vrange_dialog, sensitive);
  gtk_widget_set_sensitive(gpp->mw__button_vrange_docked, sensitive);
  gtk_widget_set_sensitive(gpp->mw__spinbutton_videotrack, sensitive);
  gtk_widget_set_sensitive(gpp->mw__spinbutton_audiotrack, sensitive);


  if((gpp->val.videotrack > 0)
  && (gpp->val.chk_vtracks > 0)
  && (gpp->val.chk_is_compatible_videofile))
  {
      sensitive = TRUE;
  }
  else
  {
      sensitive = FALSE;
  }
  sensitive_vid = sensitive;

  gtk_widget_set_sensitive(gpp->mw__spinbutton_basenum, sensitive);
  gtk_widget_set_sensitive(gpp->mw__combo_deinterlace, sensitive);
  gtk_widget_set_sensitive(gpp->mw__combo_mode_multilayer, sensitive);

  gtk_widget_set_sensitive(gpp->mw__checkbutton_generate_alpha_via_bluebox, sensitive);
  if(gpp->val.generate_alpha_via_bluebox != TRUE)
  {
      sensitive = FALSE;
  }
  gtk_widget_set_sensitive(gpp->mw__checkbutton_extract_alpha_as_gray_frames, sensitive);
  gtk_widget_set_sensitive(gpp->mw__checkbutton_extract_with_layermask, sensitive);

  if((gpp->val.multilayer == 0) && (sensitive_vid))
  {
      sensitive = TRUE;  /* we want to extract to frame files on disc */

      if((gpp->mw__entry_basename)
      && (strcmp(gpp->val.basename, DEFAULT_STORYBASENAME) == 0))
      {
        gtk_entry_set_text(GTK_ENTRY(gpp->mw__entry_basename), DEFAULT_BASENAME);
      }
  }
  else
  {
      sensitive = FALSE;  /*  we want to extract to one multilayer image  */
  }
  gtk_widget_set_sensitive(gpp->mw__entry_basename, sensitive);
  gtk_widget_set_sensitive(gpp->mw__button_basename, sensitive);
  gtk_widget_set_sensitive(gpp->mw__spinbutton_fn_digits, sensitive);
  gtk_widget_set_sensitive(gpp->mw__entry_extension, sensitive);

  if((gpp->val.multilayer == 2) && (sensitive_vid))
  {
    sensitive = TRUE;  /* we want to extract to frame files on disc */
    gtk_widget_set_sensitive(gpp->mw__entry_basename, sensitive);
    gtk_widget_set_sensitive(gpp->mw__button_basename, sensitive);
    
    if((gpp->mw__entry_basename)
    && (strcmp(gpp->val.basename, DEFAULT_BASENAME) == 0))
    {
      gtk_entry_set_text(GTK_ENTRY(gpp->mw__entry_basename), DEFAULT_STORYBASENAME);
    }
  }



  if((gpp->val.audiotrack > 0)
  && (gpp->val.chk_atracks > 0)
  && (gpp->val.chk_is_compatible_videofile))
  {
      sensitive = TRUE;
  }
  else
  {
      sensitive = FALSE;
  }
  gtk_widget_set_sensitive(gpp->mw__entry_audiofile, sensitive);
  gtk_widget_set_sensitive(gpp->mw__button_audiofile, sensitive);

  if((gpp->val.deinterlace != 0) && (sensitive_vid))
  {
      sensitive = TRUE;
  }
  else
  {
      sensitive = FALSE;
  }
  gtk_widget_set_sensitive(gpp->mw__spinbutton_delace_threshold, sensitive);


  p_update_range_widgets(gpp);

}  /* end p_update_wgt_sensitivity */



/* ------------------------------
 * p_init_mw__main_window_widgets
 * ------------------------------
 */
static void
p_init_mw__main_window_widgets (GapVexMainGlobalParams *gpp)
{
  GtkWidget *wgt;
  GtkAdjustment *adj;
  GtkEntry *entry;

 if(gap_debug) printf("INIT: init_mw__main_window_widgets\n");

 /* put initial values to the widgets */
 adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_videotrack_adj);
 gtk_adjustment_set_value(adj, (gfloat)gpp->val.videotrack);

 adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_audiotrack_adj);
 gtk_adjustment_set_value(adj, (gfloat)gpp->val.audiotrack);

 adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_basenum_adj);
 gtk_adjustment_set_value(adj, (gfloat)gpp->val.basenum);

 adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_fn_digits_adj);
 gtk_adjustment_set_value(adj, (gfloat)gpp->val.fn_digits);

 adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_delace_threshold_adj);
 gtk_adjustment_set_value(adj, (gfloat)gpp->val.delace_threshold);


 entry = GTK_ENTRY(gpp->mw__entry_video);
 gtk_entry_set_text(entry, gpp->val.videoname);

 entry = GTK_ENTRY(gpp->mw__entry_basename);
 gtk_entry_set_text(entry, gpp->val.basename);

 entry = GTK_ENTRY(gpp->mw__entry_extension);
 gtk_entry_set_text(entry, gpp->val.extension);

 entry = GTK_ENTRY(gpp->mw__entry_audiofile);
 gtk_entry_set_text(entry, gpp->val.audiofile);


 entry = GTK_ENTRY(gpp->mw__entry_preferred_decoder);
 gtk_entry_set_text(entry, gpp->val.preferred_decoder);

 wgt = gpp->mw__checkbutton_disable_mmx;
 gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wgt),
                               gpp->val.disable_mmx );

 wgt = gpp->mw__checkbutton_exact_seek;
 gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wgt),
                               gpp->val.exact_seek );

 p_update_range_widgets(gpp);
 p_update_wgt_sensitivity(gpp);
}  /* end p_init_mw__main_window_widgets */



/* -----------------------------
 * p_vex_set_range_cb
 * -----------------------------
 * Player Callback procedure
 * to set range widgets
 */
static void
p_vex_set_range_cb(GapPlayerAddClip *plac_ptr)
{
  GapVexMainGlobalParams *gpp;
  gpp = (GapVexMainGlobalParams *)plac_ptr->user_data_ptr;

  if(gap_debug)
  {
    printf("p_vex_set_range_cb:\n");
    printf("  FROM : %d\n", (int)plac_ptr->range_from);
    printf("  TO : %d\n", (int)plac_ptr->range_to);
  }

  if(gpp == NULL)
  {
    return;
  }

  gpp->val.begin_frame = MIN(plac_ptr->range_from, plac_ptr->range_to);
  gpp->val.end_frame = MAX(plac_ptr->range_from, plac_ptr->range_to);
  p_update_range_widgets(gpp);

}  /* end p_vex_set_range_cb */


/* -----------------------------
 * p_call_player_widget
 * -----------------------------
 * IN: imagename of one frame to playback in normal mode (or the name of a videofile)
 *
 * IN: begin_frame  use -1 to start play from  1.st frame
 * IN: end_frame    use -1 to start play until last frame
 *
 * Call the Player
 * If it is the 1.st call or the player window has closed since last call
 *    create the player widget
 * else
 *    reset the player widget
 */
static void
p_call_player_widget(GapVexMainGlobalParams *gpp
                   , char *imagename
                   , gint32 imagewidth
                   , gint32 imageheight
                   , gint32 begin_frame
                   , gint32 end_frame
                   , gint32 seltrack
                   , gdouble delace
                   , gboolean docked_mode
                   )
{
  if(gpp->in_player_call)
  {
    /* this procedure is already active, and locked against
     * calls while busy
     */
    return;
  }

  gpp->in_player_call = TRUE;


  if(gpp->plp)
  {
    if(gpp->plp->shell_window != NULL)
    {
       gtk_window_present(GTK_WINDOW(gpp->plp->shell_window));
    }

    if((gpp->plp->shell_window == NULL)
    && (gpp->plp->docking_container == NULL))
    {
      if(gap_debug) printf("Player shell has gone, force Reopen now\n");

      gap_player_dlg_cleanup(gpp->plp);
      g_free(gpp->plp);
      gpp->plp = NULL;
    }
  }

  if(gpp->plp == NULL)
  {
    /* 1. START mode */
    gpp->plp = (GapPlayerMainGlobalParams *)g_malloc0(sizeof(GapPlayerMainGlobalParams));

    if(gpp->plp)
    {
      gpp->plp->standalone_mode = FALSE;  /* player acts as widget and does not call gtk_main_quit */

      gpp->plp->help_id = NULL;
      if(docked_mode)
      {
        gtk_widget_show(gpp->mw__player_frame);
        gpp->plp->docking_container = gpp->mw__player_frame;  /* player is docked */
      }
      else
      {
        gpp->plp->docking_container = NULL;  /* player has own window (not docked) */
        gpp->plp->help_id = GAP_VEX_PLAYER_HELP_ID;
      }

      gpp->plp->autostart = FALSE;
      gpp->plp->caller_range_linked = TRUE;
      gpp->plp->use_thumbnails = FALSE;
      gpp->plp->exact_timing = FALSE;
      gpp->plp->play_selection_only = FALSE;
      gpp->plp->play_loop = FALSE;
      gpp->plp->play_pingpong = FALSE;
      gpp->plp->play_backward = FALSE;

      gpp->plp->stb_ptr = NULL;
      gpp->plp->image_id = -1;     /* have no image_id, operate on videofile */
      gpp->plp->imagename = NULL;
      if(imagename)
      {
        gpp->plp->imagename = g_strdup(imagename);
      }
      gpp->plp->imagewidth = imagewidth;
      gpp->plp->imageheight = imageheight;
      gpp->plp->aspect_ratio = GAP_PLAYER_DONT_FORCE_ASPECT;

      gpp->plp->play_current_framenr = 0;
      gpp->plp->begin_frame = begin_frame;
      gpp->plp->end_frame = end_frame;

      gpp->plp->fptr_set_range = p_vex_set_range_cb;
      gpp->plp->user_data_ptr = gpp;
      gpp->plp->seltrack = seltrack;
      gpp->plp->delace = delace;
      gpp->plp->preferred_decoder = g_strdup(gpp->val.preferred_decoder);
      gpp->plp->force_open_as_video = TRUE;   /* TRUE: try video open even for unknown videofile extensions */
      gpp->plp->have_progress_bar = TRUE;


      gap_player_dlg_create(gpp->plp);

    }
  }
  else
  {
    /* RESTART mode */
    gap_player_dlg_restart(gpp->plp
                      , FALSE              /* gboolean autostart */
                      , -1                 /* have no image_id, operate on videofile */
                      , imagename
                      , imagewidth
                      , imageheight
                      , NULL               /* have no storyboard */
                      , begin_frame
                      , end_frame
                      , FALSE              /* gboolean play_selection_only */
                      , seltrack
                      , delace
                      , gpp->val.preferred_decoder
                      , TRUE               /* force_open_as_video */
                      , GAP_STB_FLIP_NONE  /* flip_request */
                      , GAP_STB_FLIP_NONE  /* flip_status */
                      , 1                  /* stb_in_track */
                      );
  }

  if(gpp->plp)
  {
    if((gpp->plp->from_button)
    && (gpp->plp->to_button))
    {
      gimp_help_set_help_data (gpp->plp->from_button
                              , _("Set range to extract")
                              , NULL);
      gimp_help_set_help_data (gpp->plp->to_button
                              , _("Set range to extract")
                              , NULL);
    }
  }
  gpp->in_player_call = FALSE;
}  /* end p_call_player_widget */


/* -------------------
 * p_check_videofile
 * -------------------
 * check videofile compatibility
 * and get some information about the file
 */
static void
p_check_videofile(GapVexMainGlobalParams *gpp)
{
  t_GVA_Handle   *gvahand;
  GtkLabel       *lbl;
  char           *active_decoder;
  gdouble         aspect_ratio;

  gpp->val.chk_is_compatible_videofile = FALSE;
  gpp->val.chk_vtracks = 0;
  gpp->val.chk_atracks = 0;
  gpp->val.chk_total_frames = 0;
  aspect_ratio = 0;

  active_decoder = NULL;

  if(g_file_test(gpp->val.videoname, G_FILE_TEST_EXISTS))
  {
    if(gap_debug) printf("p_check_videofile: %s vid_track:%d aud_track:%d\n", gpp->val.videoname, (int)gpp->val.videotrack, (int)gpp->val.audiotrack );

    gvahand = GVA_open_read_pref(gpp->val.videoname
                           ,gpp->val.videotrack
                           ,gpp->val.audiotrack
                           ,gpp->val.preferred_decoder
                           , FALSE  /* use MMX if available (disable_mmx == FALSE) */
                           );
    if(gvahand)
    {
       gpp->val.chk_is_compatible_videofile = TRUE;
       gpp->val.chk_vtracks = gvahand->vtracks;
       gpp->val.chk_atracks = gvahand->atracks;
       gpp->val.chk_total_frames = gvahand->total_frames;
       gpp->video_width = gvahand->width;
       gpp->video_height = gvahand->height;
       gpp->video_speed = gvahand->framerate;
       aspect_ratio = gvahand->aspect_ratio;


       if(gvahand->dec_elem)
       {
         t_GVA_DecoderElem *dec_elem;

         dec_elem = (t_GVA_DecoderElem *)gvahand->dec_elem;

         if(dec_elem->decoder_name)
         {
           active_decoder = g_strdup(dec_elem->decoder_name);
         }

         /* if(dec_elem->decoder_description) */
       }

       GVA_close(gvahand);
    }
  }


  /* update label aspect ratio */
  lbl = GTK_LABEL(gpp->mw__label_aspect_ratio);
  if(aspect_ratio != 0.0)
  {
     char ratio_txt[20];
     char ratio2_txt[20];

     ratio2_txt[0] = '\0';
     if(p_check_aspect(aspect_ratio, 3, 2))
     {
       g_snprintf(ratio2_txt, sizeof(ratio2_txt), " (3:2)");
     }
     if(p_check_aspect(aspect_ratio, 4, 3))
     {
       g_snprintf(ratio2_txt, sizeof(ratio2_txt), " (4:3)");
     }
     if(p_check_aspect(aspect_ratio, 16, 9))
     {
       g_snprintf(ratio2_txt, sizeof(ratio2_txt), " (16:9)");
     }
     g_snprintf(ratio_txt, sizeof(ratio_txt)
                           , "%0.5f%s"
                           , (float)aspect_ratio
                           , ratio2_txt
                           );

     gtk_label_set_text(lbl, ratio_txt);
  }
  else
  {
     gtk_label_set_text(lbl, _("unknown") );
  }


  /* update label active decoder */
  lbl = GTK_LABEL(gpp->mw__label_active_decoder);
  if(active_decoder)
  {
     gtk_label_set_text(lbl, active_decoder);
     g_free(active_decoder);
     active_decoder = NULL;
  }
  else
  {
     gtk_label_set_text(lbl, "*******");
  }

  p_update_wgt_sensitivity(gpp);

}  /* end p_check_videofile */


/* ---------------------------------
 * on_mw_response
 * ---------------------------------
 */
static void
on_mw_response (GtkWidget *widget,
                 gint       response_id,
                 GapVexMainGlobalParams *gpp)
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
        gpp->val.run = TRUE;
      }

    default:
      dialog = NULL;
      if(gpp)
      {
        dialog = gpp->mw__main_window;
        if(dialog)
        {
          gpp->mw__main_window = NULL;
          gtk_widget_destroy (dialog);
        }
      }
      gtk_main_quit ();
      break;
  }
}  /* end on_mw_response */

/* -----------------------------------
 * on_mw__combo_preferred_decoder
 * -----------------------------------
 */
static void
on_mw__combo_preferred_decoder  (GtkWidget     *widget,
                                 GapVexMainGlobalParams *gpp)
{
  GtkEntry *entry;
  gint       l_idx;
  gint       value;
  const char *preferred_decoder;

 if(gap_debug) printf("CB: on_mw__combo_preferred_decoder\n");

 if(gpp == NULL) return;

 gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
 l_idx = value;

 preferred_decoder = "\0";
 if(gap_debug) printf("CB: on_mw__combo_preferred_decoder index: %d\n", (int)l_idx);
 switch(l_idx)
 {
   case GAP_VEX_DECODER_NONE:
     preferred_decoder = "\0";
     break;
   case GAP_VEX_DECODER_LIBMPEG3:
     preferred_decoder = "libmpeg3";
     break;
   case GAP_VEX_DECODER_QUICKTIME:
     preferred_decoder = "quicktime4linux";
     break;
   case GAP_VEX_DECODER_LIBAVFORMAT:
     preferred_decoder = "libavformat";
     break;
 }
 g_snprintf(gpp->val.preferred_decoder, sizeof(gpp->val.preferred_decoder)
            , "%s", preferred_decoder
               );
 entry = GTK_ENTRY(gpp->mw__entry_preferred_decoder);
 if(entry)
 {
   gtk_entry_set_text(entry, preferred_decoder);
 }

}  /* end on_mw__combo_preferred_decoder */


/* ------------------------------
 * on_mw__combo_deinterlace
 * ------------------------------
 */
static void
on_mw__combo_deinterlace  (GtkWidget     *widget,
                           GapVexMainGlobalParams *gpp)
{
  gint       l_idx;
  gint       value;
  gboolean   sensitive;

  if(gap_debug) printf("CB: on_mw__combo_deinterlace\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  l_idx = value;

  if(gap_debug) printf("CB: on_mw__combo_deinterlace index: %d\n", (int)l_idx);

  gpp->val.deinterlace = l_idx;

  if(gpp->val.deinterlace != 0)
  {
       sensitive = TRUE;
  }
  else
  {
       sensitive = FALSE;
  }

  if(gpp->mw__spinbutton_delace_threshold)
  {
    gtk_widget_set_sensitive(gpp->mw__spinbutton_delace_threshold, sensitive);
  }

}  /* end on_mw__combo_deinterlace */


/* ------------------------------
 * on_mw__combo_mode_multilayer
 * ------------------------------
 */
static void
on_mw__combo_mode_multilayer  (GtkWidget     *widget,
                           GapVexMainGlobalParams *gpp)
{
  gint       l_idx;
  gint       value;
  gboolean   sensitive;

  if(gap_debug) printf("CB: on_mw__combo_mode_multilayer\n");

  if(gpp == NULL) return;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &value);
  l_idx = value;

  if(gap_debug) printf("CB: on_mw__combo_mode_multilayer index: %d\n", (int)l_idx);

  gpp->val.multilayer = l_idx;

  p_update_wgt_sensitivity(gpp);

}  /* end on_mw__combo_mode_multilayer */


/* --------------------------------
 * on_mw__checkbutton_bluebox_toggled
 * --------------------------------
 */
static void
on_mw__checkbutton_bluebox_toggled
                                        (GtkToggleButton *togglebutton,
                                        GapVexMainGlobalParams *gpp)
{
 GtkWidget *wgt;

 if(gap_debug) printf("CB: on_mw__checkbutton_bluebox_toggled\n");
 if(gpp == NULL) return;

 wgt = gpp->mw__checkbutton_generate_alpha_via_bluebox;

 if (GTK_TOGGLE_BUTTON (wgt)->active)
 {
    gpp->val.generate_alpha_via_bluebox = TRUE;
 }
 else
 {
    gpp->val.generate_alpha_via_bluebox = FALSE;
 }
 p_update_wgt_sensitivity(gpp);

}

/* --------------------------------
 * on_mw__checkbutton_graymask_toggled
 * --------------------------------
 */
static void
on_mw__checkbutton_graymask_toggled
                                        (GtkToggleButton *togglebutton,
                                        GapVexMainGlobalParams *gpp)
{
 GtkWidget *wgt;

 if(gap_debug) printf("CB: on_mw__checkbutton_graymask_toggled\n");
 if(gpp == NULL) return;

 wgt = gpp->mw__checkbutton_extract_alpha_as_gray_frames;

 if (GTK_TOGGLE_BUTTON (wgt)->active)
 {
    gpp->val.extract_alpha_as_gray_frames = TRUE;
 }
 else
 {
    gpp->val.extract_alpha_as_gray_frames = FALSE;
 }
}

/* --------------------------------
 * on_mw__checkbutton_layermask_toggled
 * --------------------------------
 */
static void
on_mw__checkbutton_layermask_toggled
                                        (GtkToggleButton *togglebutton,
                                        GapVexMainGlobalParams *gpp)
{
 GtkWidget *wgt;

 if(gap_debug) printf("CB: on_mw__checkbutton_layermask_toggled\n");
 if(gpp == NULL) return;

 wgt = gpp->mw__checkbutton_extract_with_layermask;

 if (GTK_TOGGLE_BUTTON (wgt)->active)
 {
    gpp->val.extract_with_layermask = TRUE;
 }
 else
 {
    gpp->val.extract_with_layermask = FALSE;
 }
}

/* --------------------------------
 * on_mw__checkbutton_disable_mmx_toggled
 * --------------------------------
 */
static void
on_mw__checkbutton_disable_mmx_toggled
                                        (GtkToggleButton *togglebutton,
                                        GapVexMainGlobalParams *gpp)
{
 GtkWidget *wgt;

 if(gap_debug) printf("CB: on_mw__checkbutton_disable_mmx_toggled\n");
 if(gpp == NULL) return;

 wgt = gpp->mw__checkbutton_disable_mmx;

 if (GTK_TOGGLE_BUTTON (wgt)->active)
 {
    gpp->val.disable_mmx = TRUE;
 }
 else
 {
    gpp->val.disable_mmx = FALSE;
 }
}

/* --------------------------------
 * on_mw__spinbutton_begin_frame_changed
 * --------------------------------
 */
static void
on_mw__spinbutton_begin_frame_changed   (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

  if(gap_debug) printf("CB: on_mw__spinbutton_begin_frame_changed\n");
  if(gpp == NULL) return;

  adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_begin_frame_adj);

  if(gap_debug) printf("spin value: %f\n", (float)adj->value );

  if((gdouble)adj->value != gpp->val.begin_frame)
  {
    gpp->val.begin_frame = (gdouble)adj->value;
    p_update_time_labels(gpp);
  }
}


/* --------------------------------
 * on_mw__spinbutton_end_frame_changed
 * --------------------------------
 */
static void
on_mw__spinbutton_end_frame_changed   (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

  if(gap_debug) printf("CB: on_mw__spinbutton_end_frame_changed\n");
  if(gpp == NULL) return;

  adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_end_frame_adj);

  if(gap_debug) printf("spin value: %f\n", (float)adj->value );

  if((gdouble)adj->value != gpp->val.end_frame)
  {
    gpp->val.end_frame = (gdouble)adj->value;
    p_update_time_labels(gpp);
  }
}


/* ------------------------------------
 * on_mw__spinbutton_videotrack_changed
 * ------------------------------------
 */
static void
on_mw__spinbutton_videotrack_changed   (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

 if(gap_debug) printf("CB: on_mw__spinbutton_videotrack_changed\n");
 if(gpp == NULL) return;

 if(gpp->mw__spinbutton_videotrack_adj)
 {
   adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_videotrack_adj);

   if(gap_debug) printf("videotrack spin value: %f\n", (float)adj->value );

   if((gint)adj->value != gpp->val.videotrack)
   {
     gpp->val.videotrack = (gint)adj->value;
     if((gpp->val.chk_is_compatible_videofile == TRUE)
     &&(gpp->val.videotrack > gpp->val.chk_vtracks))
     {
       /* if we have a valid videofile constraint videotracks to available tracks */
       gtk_adjustment_set_value(adj, (gfloat)gpp->val.chk_vtracks );
     }
     p_update_wgt_sensitivity(gpp);
   }
 }
}


/* --------------------------------
 * on_mw__spinbutton_audiotrack_changed
 * --------------------------------
 */
static void
on_mw__spinbutton_audiotrack_changed   (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

 if(gap_debug) printf("CB: on_mw__spinbutton_audiotrack_changed\n");
 if(gpp == NULL) return;

 if(gpp->mw__spinbutton_audiotrack_adj)
 {
   adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_audiotrack_adj);

   if(gap_debug) printf("audiotrack spin value: %f\n", (float)adj->value );

   if((gint)adj->value != gpp->val.audiotrack)
   {
     gpp->val.audiotrack = (gint)adj->value;
     if((gpp->val.chk_is_compatible_videofile == TRUE)
     &&(gpp->val.audiotrack > gpp->val.chk_atracks))
     {
       /* if we have a valid videofile constraint audiotracks to available tracks */
       gtk_adjustment_set_value(adj, (gfloat)gpp->val.chk_atracks );
     }
     p_update_wgt_sensitivity(gpp);
   }
 }
}



/* --------------------------------
 * on_mw__entry_video_changed
 * --------------------------------
 */
static void
on_mw__entry_video_changed             (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_mw__entry_video_changed\n");
 if(gpp == NULL) return;

 if(gpp->mw__entry_video)
 {
    g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s"
              , gtk_entry_get_text(GTK_ENTRY(gpp->mw__entry_video)));
    p_check_videofile(gpp);
 }
}

/* --------------------------------
 * on_mw__button_video_clicked
 * --------------------------------
 */
static void
on_mw__button_video_clicked            (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_mw__button_video_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsv__fileselection == NULL)
 {
   gpp->fsv__fileselection = create_fsv__fileselection(gpp);
   gtk_file_selection_set_filename (GTK_FILE_SELECTION (gpp->fsv__fileselection),
                                    gpp->val.videoname);

   gtk_widget_show (gpp->fsv__fileselection);
 }
 else
 {
   gtk_window_present(GTK_WINDOW(gpp->fsv__fileselection));
 }
}


/* --------------------------------
 * on_mw__entry_basename_changed
 * --------------------------------
 */
static void
on_mw__entry_basename_changed          (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_mw__entry_basename_changed\n");
 if(gpp == NULL) return;

 if(gpp->mw__entry_basename)
 {
    g_snprintf(gpp->val.basename, sizeof(gpp->val.basename), "%s"
              , gtk_entry_get_text(GTK_ENTRY(gpp->mw__entry_basename)));
 }
}


/* --------------------------------
 * on_mw__button_basename_clicked
 * --------------------------------
 */
static void
on_mw__button_basename_clicked         (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_mw__button_basename_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsb__fileselection == NULL)
 {
   gpp->fsb__fileselection = create_fsb__fileselection(gpp);
   gtk_file_selection_set_filename (GTK_FILE_SELECTION (gpp->fsb__fileselection),
                                    gpp->val.basename);

   gtk_widget_show (gpp->fsb__fileselection);
 }
 else
 {
   gtk_window_present(GTK_WINDOW(gpp->fsb__fileselection));
 }
}


/* --------------------------------
 * on_mw__entry_extension_changed
 * --------------------------------
 */
static void
on_mw__entry_extension_changed         (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_mw__entry_extension_changed\n");
 if(gpp == NULL) return;

 if(gpp->mw__entry_extension)
 {
    g_snprintf(gpp->val.extension, sizeof(gpp->val.extension), "%s"
              , gtk_entry_get_text(GTK_ENTRY(gpp->mw__entry_extension)));
 }
}


/* --------------------------------
 * on_mw__spinbutton_basenum_changed
 * --------------------------------
 */
static void
on_mw__spinbutton_basenum_changed      (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

 if(gap_debug) printf("CB: on_mw__spinbutton_basenum_changed\n");
 if(gpp == NULL) return;

 if(gpp->mw__spinbutton_basenum_adj)
 {
   adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_basenum_adj);

   if(gap_debug) printf("basenum spin value: %f\n", (float)adj->value );

   if((gint)adj->value != gpp->val.basenum)
   {
     gpp->val.basenum = (gint)adj->value;
   }
 }
}

/* --------------------------------
 * on_mw__entry_audiofile_changed
 * --------------------------------
 */
static void
on_mw__entry_audiofile_changed         (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_mw__entry_audiofile_changed\n");
 if(gpp == NULL) return;

 if(gpp->mw__entry_audiofile)
 {
    g_snprintf(gpp->val.audiofile, sizeof(gpp->val.audiofile), "%s"
              , gtk_entry_get_text(GTK_ENTRY(gpp->mw__entry_audiofile)));
 }
}



/* --------------------------------
 * on_mw__button_audiofile_clicked
 * --------------------------------
 */
static void
on_mw__button_audiofile_clicked        (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_mw__button_audiofile_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsa__fileselection == NULL)
 {
   gpp->fsa__fileselection = create_fsa__fileselection(gpp);
   gtk_file_selection_set_filename (GTK_FILE_SELECTION (gpp->fsa__fileselection),
                                    gpp->val.audiofile);

   gtk_widget_show (gpp->fsa__fileselection);
 }
 else
 {
   gtk_window_present(GTK_WINDOW(gpp->fsa__fileselection));
 }
}


/* --------------------------------
 * on_mw__button_vrange_dialog_clicked
 * --------------------------------
 */
static void
on_mw__button_vrange_dialog_clicked             (GtkButton       *button,
                                                 GdkEventButton  *bevent,
                                                 GapVexMainGlobalParams *gpp)
{
 gdouble  delace;
 gboolean docked_mode;

 delace = gpp->val.deinterlace;
 if(delace == GAP_VEX_DELACE_ODD_X2)
 {
   delace = GAP_VEX_DELACE_ODD;
 }
 if(delace == GAP_VEX_DELACE_EVEN_X2)
 {
   delace = GAP_VEX_DELACE_EVEN;
 }
 delace += CLAMP(gpp->val.delace_threshold, 0.0, 0.9999);


 if(gap_debug) printf("CB: on_mw__button_vrange_dialog_clicked\n");
 if(gpp == NULL) return;

 docked_mode = TRUE;
 if(bevent)
 {
   if ((bevent->state & GDK_SHIFT_MASK))  /* SHIFT Click */
   {
     docked_mode = FALSE;
   }
 }

 p_call_player_widget(gpp
                   ,gpp->val.videoname
                   ,gpp->video_width
                   ,gpp->video_height
                   , gpp->val.begin_frame
                   , gpp->val.end_frame
                   , gpp->val.videotrack
                   , delace
                   , docked_mode
                   );
}


/* --------------------------------
 * on_mw__button_vrange_docked_clicked
 * --------------------------------
 */
static void
on_mw__button_vrange_docked_clicked             (GtkButton       *button,
                                                 GapVexMainGlobalParams *gpp)
{
 gdouble delace;

 delace = gpp->val.deinterlace;
 if(delace == GAP_VEX_DELACE_ODD_X2)
 {
   delace = GAP_VEX_DELACE_ODD;
 }
 if(delace == GAP_VEX_DELACE_EVEN_X2)
 {
   delace = GAP_VEX_DELACE_EVEN;
 }
 delace += CLAMP(gpp->val.delace_threshold, 0.0, 0.9999);


 if(gap_debug) printf("CB: on_mw__button_vrange_docked_clicked\n");
 if(gpp == NULL) return;

 p_call_player_widget(gpp
                   ,gpp->val.videoname
                   ,gpp->video_width
                   ,gpp->video_height
                   , gpp->val.begin_frame
                   , gpp->val.end_frame
                   , gpp->val.videotrack
                   , delace
                   , TRUE                      /* docked_mode */
                   );
}

/* --------------------------------
 * on_mw__entry_preferred_decoder_changed
 * --------------------------------
 */
static void
on_mw__entry_preferred_decoder_changed (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
  const char *preferred_decoder;
  GtkEntry *entry;

 if(gap_debug) printf("CB: on_mw__entry_preferred_decoder_changed\n");
 if(gpp == NULL) return;

 entry = GTK_ENTRY(gpp->mw__entry_preferred_decoder);
 if(entry)
 {
    preferred_decoder = gtk_entry_get_text(entry);
    g_snprintf(gpp->val.preferred_decoder, sizeof(gpp->val.preferred_decoder), "%s"
              , preferred_decoder);
    p_check_videofile(gpp);
 }

}


/* --------------------------------
 * on_mw__checkbutton_exact_seek_toggled
 * --------------------------------
 */
static void
on_mw__checkbutton_exact_seek_toggled  (GtkToggleButton *togglebutton,
                                        GapVexMainGlobalParams *gpp)
{
 GtkWidget *wgt;

 if(gap_debug) printf("CB: on_mw__checkbutton_exact_seek_toggled\n");
 if(gpp == NULL) return;

 wgt = gpp->mw__checkbutton_exact_seek;

 if (GTK_TOGGLE_BUTTON (wgt)->active)
 {
    gpp->val.exact_seek = TRUE;
 }
 else
 {
    gpp->val.exact_seek = FALSE;
 }
}

/* --------------------------------
 * on_mw__spinbutton_delace_threshold_changed
 * --------------------------------
 */
static void
on_mw__spinbutton_delace_threshold_changed
                                        (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

 if(gap_debug) printf("CB: on_mw__spinbutton_delace_threshold_changed\n");
 if(gpp == NULL) return;

 if(gpp->mw__spinbutton_delace_threshold_adj)
 {
   adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_delace_threshold_adj);

   if(gap_debug) printf("spin value: %f\n", (float)adj->value );

   if((gdouble)adj->value != gpp->val.delace_threshold)
   {
     gpp->val.delace_threshold = (gdouble)adj->value;
   }
 }

}



/* --------------------------------
 * on_mw__spinbutton_fn_digits_changed
 * --------------------------------
 */
static void
on_mw__spinbutton_fn_digits_changed    (GtkEditable     *editable,
                                        GapVexMainGlobalParams *gpp)
{
  GtkAdjustment *adj;

 if(gap_debug) printf("CB: on_mw__spinbutton_fn_digits_changed\n");
 if(gpp == NULL) return;

 if(gpp->mw__spinbutton_fn_digits_adj)
 {
   adj = GTK_ADJUSTMENT(gpp->mw__spinbutton_fn_digits_adj);

   if(gap_debug) printf("spin value: %f\n", (float)adj->value );

   if((gint32)adj->value != gpp->val.fn_digits)
   {
     gpp->val.fn_digits = (gint32)adj->value;
   }
 }
}



/* XXXXXXXXXXXXXXXXXXXXXXXXX  Videofile Select XXXXXXXXXXXXXXXXXXXXXXX FSV */


/* --------------------------------
 * on_fsv__fileselection_destroy
 * --------------------------------
 */
static void
on_fsv__fileselection_destroy          (GtkObject       *object,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_fsv__fileselection_destroy\n");
 if(gpp == NULL) return;

 gpp->fsv__fileselection = NULL;
}

/* --------------------------------
 * on_fsv__button_OK_clicked
 * --------------------------------
 */
static void
on_fsv__button_OK_clicked              (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp)
{
  const gchar *filename;
  GtkEntry *entry;

 if(gap_debug) printf("CB: on_fsv__button_OK_clicked\n");
 if(gpp == NULL) return;


 if(gpp->fsv__fileselection)
 {
   filename =  gtk_file_selection_get_filename (GTK_FILE_SELECTION (gpp->fsv__fileselection));
   g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s"
             ,filename);
   entry = GTK_ENTRY(gpp->mw__entry_video);
   if(entry)
   {
      gtk_entry_set_text(entry, filename);
      p_update_wgt_sensitivity(gpp);
   }
   on_fsv__button_cancel_clicked(NULL, (gpointer)gpp);
 }
}

/* --------------------------------
 * on_fsv__button_cancel_clicked
 * --------------------------------
 */
static void
on_fsv__button_cancel_clicked          (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_fsv__button_cancel_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsv__fileselection)
 {
   gtk_widget_destroy(gpp->fsv__fileselection);
   gpp->fsv__fileselection = NULL;
 }
}

/* XXXXXXXXXXXXXXXXXXXXXXXXX  Basename Fileselect  XXXXXXXXXXXXXXXXXXXXXXX FSB */

/* --------------------------------
 * on_fsb__fileselection_destroy
 * --------------------------------
 */
static void
on_fsb__fileselection_destroy          (GtkObject       *object,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_fsb__fileselection_destroy\n");
 if(gpp == NULL) return;

 gpp->fsb__fileselection = NULL;
}


/* --------------------------------
 * on_fsb__button_OK_clicked
 * --------------------------------
 */
static void
on_fsb__button_OK_clicked              (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp)
{
  GtkEntry *entry;
  const gchar *filename;

 if(gap_debug) printf("CB: on_fsb__button_OK_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsb__fileselection)
 {
   filename =  gtk_file_selection_get_filename (GTK_FILE_SELECTION (gpp->fsb__fileselection));
   g_snprintf(gpp->val.basename, sizeof(gpp->val.basename), "%s"
             , filename);
   entry = GTK_ENTRY(gpp->mw__entry_basename);
   if(entry)
   {
      gtk_entry_set_text(entry, filename);
   }
   on_fsb__button_cancel_clicked(NULL, (gpointer)gpp);
 }
}


/* --------------------------------
 * on_fsb__button_cancel_clicked
 * --------------------------------
 */
static void
on_fsb__button_cancel_clicked          (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_fsb__button_cancel_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsb__fileselection)
 {
   gtk_widget_destroy(gpp->fsb__fileselection);
   gpp->fsb__fileselection = NULL;
 }
}

/* XXXXXXXXXXXXXXXXXXXXXXXXX  Audio Fileselect  XXXXXXXXXXXXXXXXXXXXXXX FSA */

/* --------------------------------
 * on_fsa__fileselection_destroy
 * --------------------------------
 */
static void
on_fsa__fileselection_destroy          (GtkObject       *object,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_fsa__fileselection_destroy\n");
 if(gpp == NULL) return;

 gpp->fsa__fileselection = NULL;
}


/* --------------------------------
 * on_fsa__button_OK_clicked
 * --------------------------------
 */
static void
on_fsa__button_OK_clicked              (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp)
{
  const gchar *filename;
  GtkEntry *entry;

 if(gap_debug) printf("CB: on_fsa__button_OK_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsa__fileselection)
 {
   filename =  gtk_file_selection_get_filename (GTK_FILE_SELECTION (gpp->fsa__fileselection));
   g_snprintf(gpp->val.audiofile, sizeof(gpp->val.audiofile), "%s"
             , filename);
   entry = GTK_ENTRY(gpp->mw__entry_audiofile);
   if(entry)
   {
      gtk_entry_set_text(entry, filename);
   }
   on_fsa__button_cancel_clicked(NULL, (gpointer)gpp);
 }
}


/* --------------------------------
 * on_fsa__button_cancel_clicked
 * --------------------------------
 */
static void
on_fsa__button_cancel_clicked          (GtkButton       *button,
                                        GapVexMainGlobalParams *gpp)
{
 if(gap_debug) printf("CB: on_fsa__button_cancel_clicked\n");
 if(gpp == NULL) return;

 if(gpp->fsa__fileselection)
 {
   gtk_widget_destroy(gpp->fsa__fileselection);
   gpp->fsa__fileselection = NULL;
 }
}






/* ----------------------------------
 * create_fsv__fileselection
 * ----------------------------------
 * videofile selection dialog
 */
static GtkWidget*
create_fsv__fileselection (GapVexMainGlobalParams *gpp)
{
  GtkWidget *fsv__fileselection;
  GtkWidget *fsv__button_OK;
  GtkWidget *fsv__button_cancel;

  fsv__fileselection = gtk_file_selection_new (_("Select input videofile"));
  gtk_container_set_border_width (GTK_CONTAINER (fsv__fileselection), 10);

  fsv__button_OK = GTK_FILE_SELECTION (fsv__fileselection)->ok_button;
  gtk_widget_show (fsv__button_OK);
  GTK_WIDGET_SET_FLAGS (fsv__button_OK, GTK_CAN_DEFAULT);

  fsv__button_cancel = GTK_FILE_SELECTION (fsv__fileselection)->cancel_button;
  gtk_widget_show (fsv__button_cancel);
  GTK_WIDGET_SET_FLAGS (fsv__button_cancel, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (fsv__fileselection), "destroy",
                      G_CALLBACK (on_fsv__fileselection_destroy),
                      gpp);
  g_signal_connect (G_OBJECT (fsv__button_OK), "clicked",
                      G_CALLBACK (on_fsv__button_OK_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (fsv__button_cancel), "clicked",
                      G_CALLBACK (on_fsv__button_cancel_clicked),
                      gpp);

  gtk_widget_grab_default (fsv__button_cancel);
  return fsv__fileselection;
}  /* end create_fsv__fileselection */


/* ----------------------------------
 * create_fsb__fileselection
 * ----------------------------------
 * basename (for extracted frames) selection dialog
 */
static GtkWidget*
create_fsb__fileselection (GapVexMainGlobalParams *gpp)
{
  GtkWidget *fsb__fileselection;
  GtkWidget *fsb__button_OK;
  GtkWidget *fsb__button_cancel;

  fsb__fileselection = gtk_file_selection_new (_("Select basename for frame(s)"));
  gtk_container_set_border_width (GTK_CONTAINER (fsb__fileselection), 10);

  fsb__button_OK = GTK_FILE_SELECTION (fsb__fileselection)->ok_button;
  gtk_widget_show (fsb__button_OK);
  GTK_WIDGET_SET_FLAGS (fsb__button_OK, GTK_CAN_DEFAULT);

  fsb__button_cancel = GTK_FILE_SELECTION (fsb__fileselection)->cancel_button;
  gtk_widget_show (fsb__button_cancel);
  GTK_WIDGET_SET_FLAGS (fsb__button_cancel, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (fsb__fileselection), "destroy",
                      G_CALLBACK (on_fsb__fileselection_destroy),
                      gpp);
  g_signal_connect (G_OBJECT (fsb__button_OK), "clicked",
                      G_CALLBACK (on_fsb__button_OK_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (fsb__button_cancel), "clicked",
                      G_CALLBACK (on_fsb__button_cancel_clicked),
                      gpp);

  gtk_widget_grab_default (fsb__button_cancel);
  return fsb__fileselection;
}  /* end create_fsb__fileselection */


/* ----------------------------------
 * create_fsa__fileselection
 * ----------------------------------
 * audiofile selection dialog
 */
static GtkWidget*
create_fsa__fileselection (GapVexMainGlobalParams *gpp)
{
  GtkWidget *fsa__fileselection;
  GtkWidget *fsa__button_OK;
  GtkWidget *fsa__button_cancel;

  fsa__fileselection = gtk_file_selection_new (_("Select Audiofilename"));
  gtk_container_set_border_width (GTK_CONTAINER (fsa__fileselection), 10);

  fsa__button_OK = GTK_FILE_SELECTION (fsa__fileselection)->ok_button;
  gtk_widget_show (fsa__button_OK);
  GTK_WIDGET_SET_FLAGS (fsa__button_OK, GTK_CAN_DEFAULT);

  fsa__button_cancel = GTK_FILE_SELECTION (fsa__fileselection)->cancel_button;
  gtk_widget_show (fsa__button_cancel);
  GTK_WIDGET_SET_FLAGS (fsa__button_cancel, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (fsa__fileselection), "destroy",
                      G_CALLBACK (on_fsa__fileselection_destroy),
                      gpp);
  g_signal_connect (G_OBJECT (fsa__button_OK), "clicked",
                      G_CALLBACK (on_fsa__button_OK_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (fsa__button_cancel), "clicked",
                      G_CALLBACK (on_fsa__button_cancel_clicked),
                      gpp);

  gtk_widget_grab_default (fsa__button_cancel);
  return fsa__fileselection;
}  /* end create_fsa__fileselection */

/* endif GAP_ENABLE_VIDEOAPI_SUPPORT (1) */
#endif

/* ----------------------------------
 * p_align_widget_columns
 * ----------------------------------
 * IN: array of pointers to widgets
 *    (the array contains widgets that are arranged in one column
 *     but do not belong to the same table)
 * this procedure does check for the max width of all those widgets,
 * and forces them all to use the same width.
 * (to get same alignment in the different tables)
 */
static void
p_align_widget_columns(GtkWidget **wgt_array, gint max_elements)
{
  gint       max_label_width;
  gint       ii;

  max_label_width = 0;
  for(ii=0; ii < max_elements; ii++)
  {
    GtkRequisition  requisition;
    gtk_widget_size_request(wgt_array[ii], &requisition);
    if(gap_debug)
    {
      printf("WIDGET[%02d].width: %d\n"
          ,(int)ii
          ,(int)requisition.width
          );
    }
    if(requisition.width > max_label_width)
    {
      max_label_width = requisition.width;
    }
  }

  /* force all labels to use the max width
   * (to reach same alignment in both frames)
   */
  for(ii=0; ii < max_elements; ii++)
  {
    gtk_widget_set_size_request (wgt_array[ii], max_label_width, -1);
  }

}  /* end p_align_widget_columns */

/* ----------------------------------
 * gap_vex_dlg_create_mw__main_window
 * ----------------------------------
 * create the main window for video extract plug-in
 */
GtkWidget*
gap_vex_dlg_create_mw__main_window (GapVexMainGlobalParams *gpp)
{
  GtkWidget *mw__main_window;
  GtkWidget *mw__dialog_vbox1;
  GtkWidget *mw__vbox1;
  GtkWidget *mw__frame1;
  GtkWidget *mw__vbox2;
  GtkWidget *mw__table_in;
  GtkWidget *mw__table_out;
  GtkWidget *mw__label_video;
  GtkWidget *mw__checkbutton_disable_mmx;
  GtkWidget *mw__entry_video;
  GtkWidget *mw__button_vrange_dialog;
  GtkWidget *mw__button_vrange_docked;
  GtkWidget *mw__button_video;
  GtkWidget *mw__combo_preferred_decoder;
  GtkWidget *mw__label_active_decoder;
  GtkWidget *mw__label_aspect_ratio;
  GtkWidget *mw__entry_preferred_decoder;
  GtkObject *mw__spinbutton_audiotrack_adj;
  GtkWidget *mw__spinbutton_audiotrack;
  GtkObject *mw__spinbutton_videotrack_adj;
  GtkWidget *mw__spinbutton_videotrack;
  GtkObject *mw__spinbutton_end_frame_adj;
  GtkWidget *mw__spinbutton_end_frame;
  GtkObject *mw__spinbutton_begin_frame_adj;
  GtkWidget *mw__spinbutton_begin_frame;
  GtkWidget *mw__checkbutton_exact_seek;
  GtkWidget *mw__frame2;
  GtkWidget *mw__vbox10;
  GtkWidget *mw__label_basename;
  GtkWidget *mw__entry_basename;
  GtkWidget *mw__button_basename;
  GtkWidget *mw__label_extension;
  GtkObject *mw__spinbutton_basenum_adj;
  GtkWidget *mw__spinbutton_basenum;
  GtkWidget *mw__label_audifile;
  GtkWidget *mw__entry_audiofile;
  GtkWidget *mw__button_audiofile;
  GtkWidget *mw__combo_mode_multilayer;
  GtkWidget *mw__combo_deinterlace;
  GtkObject *mw__spinbutton_delace_threshold_adj;
  GtkWidget *mw__spinbutton_delace_threshold;
  GtkWidget *mw__entry_extension;
  GtkObject *mw__spinbutton_fn_digits_adj;
  GtkWidget *mw__spinbutton_fn_digits;
  GtkWidget *mw__button_OK;
  GtkWidget *mw__player_frame;
  GtkWidget *mw__main_hbox;
  GtkWidget *checkbutton;
  GtkWidget *label;
  GtkWidget *hbox2;
  GtkWidget *wgt_array[50];
  GtkWidget *lbl_array[50];
  gint       wgt_idx;
  gint       lbl_idx;
  gint       out_row;
  gint       in_row;


  mw__main_window = NULL;
  wgt_idx = 0;
  lbl_idx = 0;

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

  mw__main_window = gimp_dialog_new (_("Extract Videorange"),
                         GAP_VEX_PLUG_IN_NAME,
                         NULL, 0,
                         gimp_standard_help_func, GAP_VEX_PLUG_IN_HELP_ID,

                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK,     GTK_RESPONSE_OK,
                         NULL);
  gtk_window_set_type_hint (mw__main_window, GDK_WINDOW_TYPE_HINT_NORMAL);

  g_signal_connect (G_OBJECT (mw__main_window), "response",
                    G_CALLBACK (on_mw_response),
                    gpp);


  mw__dialog_vbox1 = GTK_DIALOG (mw__main_window)->vbox;
  gtk_widget_show (mw__dialog_vbox1);

  mw__main_hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (mw__main_hbox);
  gtk_box_pack_start (GTK_BOX (mw__dialog_vbox1), mw__main_hbox, TRUE, TRUE, 5);


  mw__vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (mw__vbox1);
  gtk_box_pack_start (GTK_BOX (mw__main_hbox), mw__vbox1, TRUE, TRUE, 10);
  /*gtk_container_set_border_width (GTK_CONTAINER (mw__vbox1), 5);*/


  /* XXXXXXXXXXX Player Frame  XXXXXXXXXXXX */
  /* the player_frame */
  mw__player_frame = gimp_frame_new ( _("Select Videorange") );
  gtk_frame_set_shadow_type (GTK_FRAME (mw__player_frame) ,GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (mw__main_hbox), mw__player_frame, TRUE, TRUE, 0);

  /* the mw__player_frame widget is hidden at startup
   * and becomes visible, when the user wants to select
   * the videorange via player in docked mode
   */
  /* gtk_widget_show (mw__player_frame); */  /* not yet, show the widget later */





  mw__frame1 = gimp_frame_new (_("Input Video selection"));
  gtk_widget_show (mw__frame1);
  gtk_box_pack_start (GTK_BOX (mw__vbox1), mw__frame1, TRUE, TRUE, 0);

  mw__vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (mw__vbox2);
  gtk_container_add (GTK_CONTAINER (mw__frame1), mw__vbox2);
  gtk_container_set_border_width (GTK_CONTAINER (mw__vbox2), 4);


  mw__table_in = gtk_table_new (7, 3, FALSE);
  gtk_widget_show (mw__table_in);
  gtk_box_pack_start (GTK_BOX (mw__vbox2), mw__table_in, TRUE, TRUE, 2);
  gtk_table_set_row_spacings (GTK_TABLE (mw__table_in), 1);
  gtk_table_set_col_spacings (GTK_TABLE (mw__table_in), 1);

  in_row = 0;

  /* the videofile label */
  mw__label_video = gtk_label_new (_("Videofilename:"));
  lbl_array[lbl_idx] = mw__label_video;
  lbl_idx++;

  gtk_widget_show (mw__label_video);
  gtk_misc_set_alignment (GTK_MISC (mw__label_video), 0.0, 0.0);
  gtk_table_attach (GTK_TABLE (mw__table_in), mw__label_video, 0, 1, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* the videofile entry */
  mw__entry_video = gtk_entry_new ();
  gtk_widget_show (mw__entry_video);
  gtk_widget_set_size_request (mw__entry_video, ENTRY_WIDTH_LARGE, -1);
  gtk_table_attach (GTK_TABLE (mw__table_in), mw__entry_video, 1, 2, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (mw__entry_video, _("Name of videofile (used as inputfile)"), NULL);


  /* the videofile button (that invokes fileselection dialog) */
  mw__button_video = gtk_button_new_with_label (_("..."));
  gtk_widget_show (mw__button_video);
  wgt_array[wgt_idx] = mw__button_video;
  wgt_idx++;
  gtk_table_attach (GTK_TABLE (mw__table_in), mw__button_video, 2, 3, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (mw__button_video, _("Select video using file browser"), NULL);



  /* MMX sometimes gives unusable results, and therefore is always OFF
   * checkbox is not needed any more..
   */
  mw__checkbutton_disable_mmx = gtk_check_button_new_with_label (_("Disable MMX"));
  gtk_widget_hide (mw__checkbutton_disable_mmx);

  in_row++;

  /* the videoextract range from label */
  label = gtk_label_new (_("From Frame:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  lbl_array[lbl_idx] = label;
  lbl_idx++;
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (mw__table_in), label, 0, 1, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* from spinbutton */
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_table_attach (GTK_TABLE (mw__table_in), hbox2, 1, 2, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  mw__spinbutton_begin_frame_adj = gtk_adjustment_new (1, 1, 999999, 1, 10, 0);
  mw__spinbutton_begin_frame = gtk_spin_button_new (GTK_ADJUSTMENT (mw__spinbutton_begin_frame_adj), 1, 0);
  gtk_widget_show (mw__spinbutton_begin_frame);
  gtk_widget_set_size_request (mw__spinbutton_begin_frame, SPIN_WIDTH_LARGE, -1);
  gtk_box_pack_start (GTK_BOX (hbox2), mw__spinbutton_begin_frame, FALSE, FALSE, 0);
  gimp_help_set_help_data (mw__spinbutton_begin_frame, _("Frame number of 1.st frame to extract"), NULL);


  /* from timecode label */
  label = gtk_label_new(TIME_UNDEF_STRING);
  gpp->mw__begin_time_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 12);
  gtk_widget_show(label);


  /* dummy label to fill up the hbox2 */
  label = gtk_label_new (" ");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);


  /* the videorange button (that invokes the player for video range selection) */
  mw__button_vrange_dialog = gtk_button_new_with_label (_("Video Range"));
  gtk_widget_show (mw__button_vrange_dialog);
  wgt_array[wgt_idx] = mw__button_vrange_dialog;
  wgt_idx++;
  gtk_table_attach (GTK_TABLE (mw__table_in), mw__button_vrange_dialog, 2, 3, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (mw__button_vrange_dialog
                         , _("Visual video range selection via videoplayer\n"
                             "SHIFT: Open a separate player window")
                         , NULL);


  in_row++;

  /* the videoextract range to label */
  label = gtk_label_new (_("To Frame:"));
  gtk_widget_show (label);
  lbl_array[lbl_idx] = label;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_in), label, 0, 1, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* to spinbutton */
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_table_attach (GTK_TABLE (mw__table_in), hbox2, 1, 2, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  mw__spinbutton_end_frame_adj = gtk_adjustment_new (1, 1, 999999, 1, 10, 0);
  mw__spinbutton_end_frame = gtk_spin_button_new (GTK_ADJUSTMENT (mw__spinbutton_end_frame_adj), 1, 0);
  gtk_widget_show (mw__spinbutton_end_frame);
  gtk_box_pack_start (GTK_BOX (hbox2), mw__spinbutton_end_frame, FALSE, FALSE, 0);
  gtk_widget_set_size_request (mw__spinbutton_end_frame, SPIN_WIDTH_LARGE, -1);
  gimp_help_set_help_data (mw__spinbutton_end_frame
                          , _("Frame number of last frame to extract. "
                              "To extract all frames use a range from 1 to 999999. "
                              "(Extract stops at the last available frame)")
                          , NULL);


  /* to timecode label */
  label = gtk_label_new(TIME_UNDEF_STRING);
  gpp->mw__end_time_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 12);
  gtk_widget_show(label);

  /* dummy label to fill up the hbox2 */
  label = gtk_label_new (" ");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);



  /* the videorange button (that invokes the player for video range selection in docked mode) */
  mw__button_vrange_docked = gtk_button_new_with_label (_("VideoRange"));
//  gtk_widget_show (mw__button_vrange_docked);
//  wgt_array[wgt_idx] = mw__button_vrange_docked;
//  wgt_idx++;
//  gtk_table_attach (GTK_TABLE (mw__table_in), mw__button_vrange_docked, 2, 3, in_row, in_row+1,
//                    (GtkAttachOptions) (GTK_FILL),
//                    (GtkAttachOptions) (0), 0, 0);
//  gimp_help_set_help_data (mw__button_vrange_docked, _("visual video range selection (docked mode)"), NULL);


  in_row++;

  /* the videotrack to label */
  label = gtk_label_new (_("Videotrack:"));
  gtk_widget_show (label);
  lbl_array[lbl_idx] = label;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_in), label, 0, 1, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);



  /* videotrack spinbutton */

  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_table_attach (GTK_TABLE (mw__table_in), hbox2, 1, 2, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  mw__spinbutton_videotrack_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 0);
  mw__spinbutton_videotrack = gtk_spin_button_new (GTK_ADJUSTMENT (mw__spinbutton_videotrack_adj), 1, 0);
  gtk_widget_show (mw__spinbutton_videotrack);
  gtk_widget_set_size_request (mw__spinbutton_videotrack, SPIN_WIDTH_SMALL, -1);
  gtk_box_pack_start (GTK_BOX (hbox2), mw__spinbutton_videotrack, FALSE, FALSE, 0);
  gimp_help_set_help_data (mw__spinbutton_videotrack, _("Videotrack number (0 == extract no frames)"), NULL);

  /* dummy label to fill up the hbox2 */
  label = gtk_label_new (" ");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);

  in_row++;

  /* the audiotrack to label */
  label = gtk_label_new (_("Audiotrack:"));
  gtk_widget_show (label);
  lbl_array[lbl_idx] = label;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_in), label, 0, 1, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);



  /* audiotrack spinbutton */

  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_table_attach (GTK_TABLE (mw__table_in), hbox2, 1, 2, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  mw__spinbutton_audiotrack_adj = gtk_adjustment_new (0, 0, 100, 1, 10, 0);
  mw__spinbutton_audiotrack = gtk_spin_button_new (GTK_ADJUSTMENT (mw__spinbutton_audiotrack_adj), 1, 0);
  gtk_widget_show (mw__spinbutton_audiotrack);
  gtk_widget_set_size_request (mw__spinbutton_audiotrack, SPIN_WIDTH_SMALL, -1);
  gtk_box_pack_start (GTK_BOX (hbox2), mw__spinbutton_audiotrack, FALSE, FALSE, 0);
  gimp_help_set_help_data (mw__spinbutton_audiotrack, _("Audiotrack number (0 == extract no audio)"), NULL);

  /* dummy label to fill up the hbox2 */
  label = gtk_label_new (" ");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);


  in_row++;

  /* the (preferred) Decoder label */

  label = gtk_label_new (_("Decoder:"));
  gtk_widget_show (label);
  lbl_array[lbl_idx] = label;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
  gtk_table_attach (GTK_TABLE (mw__table_in), label, 0, 1, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);



  mw__entry_preferred_decoder = gtk_entry_new ();
  gtk_widget_show (mw__entry_preferred_decoder);
  gtk_widget_set_size_request (mw__entry_preferred_decoder, ENTRY_WIDTH_LARGE, -1);
  gtk_table_attach (GTK_TABLE (mw__table_in), mw__entry_preferred_decoder, 1, 2, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (mw__entry_preferred_decoder, _("leave empty or select your preferred decoder (libmpeg3, libavformat, quicktime4linux)"), NULL);



  /* the menu to select the preferred decoder */
  mw__combo_preferred_decoder
    = gimp_int_combo_box_new (_("(none, automatic)"),    GAP_VEX_DECODER_NONE ,
                                "libmpeg3",              GAP_VEX_DECODER_LIBMPEG3,
                                "libavformat",           GAP_VEX_DECODER_LIBAVFORMAT,
                                "quicktime4linux",       GAP_VEX_DECODER_QUICKTIME,
                              NULL);
  gpp->mw__combo_preferred_decoder = mw__combo_preferred_decoder;

  gtk_widget_show (mw__combo_preferred_decoder);
  wgt_array[wgt_idx] = mw__combo_preferred_decoder;
  wgt_idx++;
  gtk_table_attach (GTK_TABLE (mw__table_in), mw__combo_preferred_decoder, 2, 3, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);



  in_row++;

  /* the (Active) Decoder Label(s) */

  label = gtk_label_new (_("Active Decoder:"));
  gtk_widget_show (label);
  lbl_array[lbl_idx] = label;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_in), label, 0, 1, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  mw__label_active_decoder = gtk_label_new (_("****"));
  gtk_widget_show (mw__label_active_decoder);
  gtk_table_attach (GTK_TABLE (mw__table_in), mw__label_active_decoder, 1, 2, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_padding (GTK_MISC (mw__label_active_decoder), 4, 0);
  gtk_misc_set_alignment (GTK_MISC (mw__label_active_decoder), 0.0, 0.0);


  /* exact_seek option to disable fast videoindex based positioning.
   * (the videoapi delivers exact frame positions on most videos
   *  but sometimes is not exact when libmepg3 is used)
   */
  mw__checkbutton_exact_seek = gtk_check_button_new_with_label (_("Exact Seek"));
  gtk_widget_show (mw__checkbutton_exact_seek);
  gimp_help_set_help_data (mw__checkbutton_exact_seek
                          , _("ON: emulate seek operations by sequential reads, "
                              "even when videoindex is available")
                          , NULL);
  gtk_table_attach (GTK_TABLE (mw__table_in), mw__checkbutton_exact_seek, 2, 3, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  in_row++;

  /* the Aspect Ratio Label(s) */

  label = gtk_label_new (_("Aspect Ratio:"));
  gtk_widget_show (label);
  lbl_array[lbl_idx] = label;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_in), label, 0, 1, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  mw__label_aspect_ratio = gtk_label_new (_("****"));
  gtk_widget_show (mw__label_aspect_ratio);
  gtk_table_attach (GTK_TABLE (mw__table_in), mw__label_aspect_ratio, 1, 2, in_row, in_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_padding (GTK_MISC (mw__label_aspect_ratio), 4, 0);
  gtk_misc_set_alignment (GTK_MISC (mw__label_aspect_ratio), 0.0, 0.0);







  mw__frame2 = gimp_frame_new (_("Output"));
  gtk_widget_show (mw__frame2);
  gtk_box_pack_start (GTK_BOX (mw__vbox1), mw__frame2, TRUE, TRUE, 10);

  mw__vbox10 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (mw__vbox10);
  gtk_container_add (GTK_CONTAINER (mw__frame2), mw__vbox10);
  gtk_container_set_border_width (GTK_CONTAINER (mw__vbox10), 4);

  mw__table_out = gtk_table_new (4, 3, FALSE);
  gtk_widget_show (mw__table_out);
  gtk_box_pack_start (GTK_BOX (mw__vbox10), mw__table_out, TRUE, TRUE, 0);

  out_row = 0;

  /* the operating Mode label */
  label = gtk_label_new (_("Mode:"));
  gtk_widget_show (label);
  lbl_array[lbl_idx] = label;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_out), label, 0, 1, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the multilayer combo box (decide if extract writes to frames on disc or to one image or storyboard) */
  mw__combo_mode_multilayer =
    gimp_int_combo_box_new (_("extracted frames are written to frame files on disc"),  0,
                            _("extracted frames are stored in one multilayer image"),  1,
                            _("create a storyboard from selected video clip"),         2,
                           NULL);
    gpp->mw__combo_mode_multilayer = mw__combo_mode_multilayer;



  gtk_widget_show (mw__combo_mode_multilayer);
  gtk_table_attach (GTK_TABLE (mw__table_out), mw__combo_mode_multilayer, 1, 2, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  out_row++;

  /* the basename label */
  mw__label_basename = gtk_label_new (_("Basename:"));
  gtk_widget_show (mw__label_basename);
  lbl_array[lbl_idx] = mw__label_basename;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (mw__label_basename), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_out), mw__label_basename, 0, 1, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the basename entry */
  mw__entry_basename = gtk_entry_new ();
  gtk_widget_show (mw__entry_basename);
  gtk_widget_set_size_request (mw__entry_basename, ENTRY_WIDTH_LARGE, -1);
  gtk_table_attach (GTK_TABLE (mw__table_out), mw__entry_basename, 1, 2, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (mw__entry_basename, _("Basename for extracted frames (framenr and extension is added)"), NULL);
  gtk_entry_set_text (GTK_ENTRY (mw__entry_basename), _("frame_"));

  /* the basename button (that invokes the fileselection dialog) */
  mw__button_basename = gtk_button_new_with_label (_("..."));
  gtk_widget_show (mw__button_basename);
  wgt_array[wgt_idx] = mw__button_basename;
  wgt_idx++;
  gtk_table_attach (GTK_TABLE (mw__table_out), mw__button_basename, 2, 3, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (mw__button_basename, _("Use filebrowser to select basename for extracted frames"), NULL);


  out_row++;

  /* the framenumber digits label */
  label = gtk_label_new (_("Digits:"));
  gtk_widget_show (label);
  lbl_array[lbl_idx] = label;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_out), label, 0, 1, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);



  /* the framenumber digits spinbutton */

  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_table_attach (GTK_TABLE (mw__table_out), hbox2, 1, 2, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  mw__spinbutton_fn_digits_adj = gtk_adjustment_new (1, 1, GAP_LIB_MAX_DIGITS, 1, 10, 0);
  mw__spinbutton_fn_digits = gtk_spin_button_new (GTK_ADJUSTMENT (mw__spinbutton_fn_digits_adj), 1, 0);
  gtk_widget_show (mw__spinbutton_fn_digits);
  gtk_widget_set_size_request (mw__spinbutton_fn_digits, SPIN_WIDTH_SMALL, -1);
  gtk_box_pack_start (GTK_BOX (hbox2), mw__spinbutton_fn_digits, FALSE, FALSE, 0);
  gimp_help_set_help_data (mw__spinbutton_fn_digits, _("Digits to use for framenumber part in filenames (use 1 if you dont want leading zeroes)"), NULL);


  /* dummy label to fill up the hbox2 */
  label = gtk_label_new (" ");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);



  /* the graymask checkbutton */
  checkbutton = gtk_check_button_new_with_label (_("graymask"));
  gpp->mw__checkbutton_extract_alpha_as_gray_frames = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_box_pack_start (GTK_BOX (hbox2), checkbutton, TRUE, TRUE, 0);
  gimp_help_set_help_data (checkbutton
                          , _("On: extract grayscale mask (generated by bluebox)\n"
                              "Off: extract color frames 1:1")
                          , NULL);
  g_signal_connect (G_OBJECT (checkbutton), "toggled",
                      G_CALLBACK (on_mw__checkbutton_graymask_toggled),
                      gpp);

  /* the layermask checkbutton */
  checkbutton = gtk_check_button_new_with_label (_("layermask"));
  gpp->mw__checkbutton_extract_with_layermask = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_box_pack_start (GTK_BOX (hbox2), checkbutton, TRUE, TRUE, 0);
  gimp_help_set_help_data (checkbutton
                          , _("On: bluebox shall generate transparency as layermask\n"
                              "Off: bluebox shall generate transparency as alpha channel")
                          , NULL);
  g_signal_connect (G_OBJECT (checkbutton), "toggled",
                      G_CALLBACK (on_mw__checkbutton_layermask_toggled),
                      gpp);

  /* the bluebox checkbutton */
  checkbutton = gtk_check_button_new_with_label (_("bluebox"));
  gpp->mw__checkbutton_generate_alpha_via_bluebox = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (mw__table_out), checkbutton, 2, 3, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (checkbutton
                          , _("On: add trasparency for extracted frames via blubox filter"
                              " (using values of last run in this session)\n"
                              "Off: extract frames 1.1")
                          , NULL);
  g_signal_connect (G_OBJECT (checkbutton), "toggled",
                      G_CALLBACK (on_mw__checkbutton_bluebox_toggled),
                      gpp);


  out_row++;

  /* the extension label */
  mw__label_extension = gtk_label_new (_("Extension:"));
  gtk_widget_show (mw__label_extension);
  lbl_array[lbl_idx] = mw__label_extension;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (mw__label_extension), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_out), mw__label_extension, 0, 1, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);


  /* the extension entry (fileextension for output frames) */
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_table_attach (GTK_TABLE (mw__table_out), hbox2, 1, 2, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  mw__entry_extension = gtk_entry_new ();
  gtk_widget_show (mw__entry_extension);
  gtk_widget_set_size_request (mw__entry_extension, SPIN_WIDTH_LARGE, -1);
  gtk_box_pack_start (GTK_BOX (hbox2), mw__entry_extension, FALSE, FALSE, 0);
  gimp_help_set_help_data (mw__entry_extension, _("Extension of extracted frames (.xcf, .jpg, .ppm)"), NULL);
  gtk_entry_set_text (GTK_ENTRY (mw__entry_extension), _(".xcf"));

  /* dummy label to fill up the hbox2 */
  label = gtk_label_new (" ");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);


  out_row++;

  /* the framenumber for 1st frame label */
  label = gtk_label_new (_("Framenr 1:"));
  gtk_widget_show (label);
  lbl_array[lbl_idx] = label;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_out), label, 0, 1, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the basenum spinbutton (set framenumber for  the 1st extracted frame) */
  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_table_attach (GTK_TABLE (mw__table_out), hbox2, 1, 2, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  mw__spinbutton_basenum_adj = gtk_adjustment_new (0, 0, 10000, 1, 10, 0);
  mw__spinbutton_basenum = gtk_spin_button_new (GTK_ADJUSTMENT (mw__spinbutton_basenum_adj), 1, 0);
  gtk_widget_show (mw__spinbutton_basenum);
  gtk_widget_set_size_request (mw__spinbutton_basenum, SPIN_WIDTH_LARGE, -1);
  gtk_box_pack_start (GTK_BOX (hbox2), mw__spinbutton_basenum, FALSE, FALSE, 0);
  gimp_help_set_help_data (mw__spinbutton_basenum, _("Framenumber for 1st extracted frame (use 0 for keeping original framenumbers)"), NULL);

  /* dummy label to fill up the hbox2 */
  label = gtk_label_new (" ");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox2), label, TRUE, TRUE, 0);

  out_row++;

  /* the deinterlace Mode label */
  label = gtk_label_new (_("Deinterlace:"));
  gtk_widget_show (label);
  lbl_array[lbl_idx] = label;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_out), label, 0, 1, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_table_attach (GTK_TABLE (mw__table_out), hbox2, 1, 2, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the deinterlace threshold spinbutton */
  mw__spinbutton_delace_threshold_adj = gtk_adjustment_new (0, 0, 1, 0.01, 0.1, 0.0);
  mw__spinbutton_delace_threshold = gtk_spin_button_new (GTK_ADJUSTMENT (mw__spinbutton_delace_threshold_adj), 1, 2);
  gtk_widget_show (mw__spinbutton_delace_threshold);
  gtk_box_pack_start (GTK_BOX (hbox2), mw__spinbutton_delace_threshold, FALSE, FALSE, 0);
  gtk_widget_set_size_request (mw__spinbutton_delace_threshold, SPIN_WIDTH_LARGE, -1);
  gimp_help_set_help_data (mw__spinbutton_delace_threshold, _("0.0 .. no interpolation, 1.0 smooth interpolation at deinterlacing"), NULL);


  /* the deinterlace combo */
  mw__combo_deinterlace
    = gimp_int_combo_box_new (_("no deinterlace"),                    GAP_VEX_DELACE_NONE,
                              _("deinterlace (odd lines only)"),      GAP_VEX_DELACE_ODD,
                              _("deinterlace (even lines only)"),     GAP_VEX_DELACE_EVEN,
                              _("deinterlace frames x 2 (odd 1st)"),  GAP_VEX_DELACE_ODD_X2,
                              _("deinterlace frames x 2 (even 1st)"), GAP_VEX_DELACE_EVEN_X2,
                              NULL);
  gpp->mw__combo_deinterlace = mw__combo_deinterlace;


  gtk_widget_show (mw__combo_deinterlace);
  gtk_box_pack_start (GTK_BOX (hbox2), mw__combo_deinterlace, TRUE, TRUE, 0);
  gimp_help_set_help_data (mw__combo_deinterlace, _("Deinterlace splits each extracted frame in 2 frames"), NULL);

  out_row++;

  /* the output audiofile label */
  mw__label_audifile = gtk_label_new (_("Audiofile:"));
  gtk_widget_show (mw__label_audifile);
  lbl_array[lbl_idx] = mw__label_audifile;
  lbl_idx++;
  gtk_misc_set_alignment (GTK_MISC (mw__label_audifile), 0.0, 0);
  gtk_table_attach (GTK_TABLE (mw__table_out), mw__label_audifile, 0, 1, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  /* the output audiofile entry */
  mw__entry_audiofile = gtk_entry_new ();
  gtk_widget_show (mw__entry_audiofile);
  gtk_widget_set_size_request (mw__entry_audiofile, ENTRY_WIDTH_LARGE, -1);
  gtk_table_attach (GTK_TABLE (mw__table_out), mw__entry_audiofile, 1, 2, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (mw__entry_audiofile, _("Name for extracted audio (audio is written in RIFF WAV format)"), NULL);
  gtk_entry_set_text (GTK_ENTRY (mw__entry_audiofile), _("frame.wav"));

  /* the output audiofile button (that invokes the fileselection dialog) */
  mw__button_audiofile = gtk_button_new_with_label (_("..."));
  gtk_widget_show (mw__button_audiofile);
  wgt_array[wgt_idx] = mw__button_audiofile;
  wgt_idx++;
  gtk_table_attach (GTK_TABLE (mw__table_out), mw__button_audiofile, 2, 3, out_row, out_row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (mw__button_audiofile, _("Use filebrowser to select audiofilename"), NULL);


  mw__button_OK = NULL;
  if(mw__button_OK)
  {
    gimp_help_set_help_data (mw__button_OK, _("Start extraction"), NULL);
  }


  /* check for the max width of all labels in both frames
   * for "input Video selection" and "Output"
   */
  p_align_widget_columns(lbl_array, lbl_idx);
  p_align_widget_columns(wgt_array, wgt_idx);


  /* copy widget pointers to global parameter
   * (for use in callbacks outside of this procedure)
   */
  gpp->mw__checkbutton_disable_mmx             = mw__checkbutton_disable_mmx;
  gpp->mw__entry_video                         = mw__entry_video;
  gpp->mw__button_vrange_dialog                = mw__button_vrange_dialog;
  gpp->mw__button_vrange_docked                = mw__button_vrange_docked;
  gpp->mw__button_video                        = mw__button_video;
  gpp->mw__label_active_decoder                = mw__label_active_decoder;
  gpp->mw__label_aspect_ratio                  = mw__label_aspect_ratio;
  gpp->mw__entry_preferred_decoder             = mw__entry_preferred_decoder;
  gpp->mw__spinbutton_audiotrack_adj           = mw__spinbutton_audiotrack_adj;
  gpp->mw__spinbutton_audiotrack               = mw__spinbutton_audiotrack;
  gpp->mw__spinbutton_videotrack_adj           = mw__spinbutton_videotrack_adj;
  gpp->mw__spinbutton_videotrack               = mw__spinbutton_videotrack;
  gpp->mw__spinbutton_end_frame_adj            = mw__spinbutton_end_frame_adj;
  gpp->mw__spinbutton_end_frame                = mw__spinbutton_end_frame;
  gpp->mw__spinbutton_begin_frame_adj          = mw__spinbutton_begin_frame_adj;
  gpp->mw__spinbutton_begin_frame              = mw__spinbutton_begin_frame;
  gpp->mw__checkbutton_exact_seek              = mw__checkbutton_exact_seek;
  gpp->mw__entry_extension                     = mw__entry_extension;
  gpp->mw__entry_basename                      = mw__entry_basename;
  gpp->mw__button_basename                     = mw__button_basename;
  gpp->mw__spinbutton_basenum_adj              = mw__spinbutton_basenum_adj;
  gpp->mw__spinbutton_basenum                  = mw__spinbutton_basenum;
  gpp->mw__entry_audiofile                     = mw__entry_audiofile;
  gpp->mw__button_audiofile                    = mw__button_audiofile;
  gpp->mw__spinbutton_delace_threshold_adj     = mw__spinbutton_delace_threshold_adj;
  gpp->mw__spinbutton_delace_threshold         = mw__spinbutton_delace_threshold;
  gpp->mw__spinbutton_fn_digits_adj            = mw__spinbutton_fn_digits_adj;
  gpp->mw__spinbutton_fn_digits                = mw__spinbutton_fn_digits;
  gpp->mw__button_OK                           = mw__button_OK;
  gpp->mw__player_frame                        = mw__player_frame;


  g_signal_connect (G_OBJECT (mw__checkbutton_disable_mmx), "toggled",
                      G_CALLBACK (on_mw__checkbutton_disable_mmx_toggled),
                      gpp);
  g_signal_connect (G_OBJECT (mw__entry_video), "changed",
                      G_CALLBACK (on_mw__entry_video_changed),
                      gpp);
  g_signal_connect (G_OBJECT (mw__button_vrange_dialog), "button_press_event",
                      G_CALLBACK (on_mw__button_vrange_dialog_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (mw__button_vrange_docked), "clicked",
                      G_CALLBACK (on_mw__button_vrange_docked_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (mw__button_video), "clicked",
                      G_CALLBACK (on_mw__button_video_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (mw__entry_preferred_decoder), "changed",
                      G_CALLBACK (on_mw__entry_preferred_decoder_changed),
                      gpp);
  g_signal_connect (G_OBJECT (mw__spinbutton_audiotrack), "value_changed",
                      G_CALLBACK (on_mw__spinbutton_audiotrack_changed),
                      gpp);
  g_signal_connect (G_OBJECT (mw__spinbutton_videotrack), "value_changed",
                      G_CALLBACK (on_mw__spinbutton_videotrack_changed),
                      gpp);
  g_signal_connect (G_OBJECT (mw__spinbutton_end_frame), "value_changed",
                      G_CALLBACK (on_mw__spinbutton_end_frame_changed),
                      gpp);
  g_signal_connect (G_OBJECT (mw__spinbutton_begin_frame), "value_changed",
                      G_CALLBACK (on_mw__spinbutton_begin_frame_changed),
                      gpp);
  g_signal_connect (G_OBJECT (mw__checkbutton_exact_seek), "toggled",
                      G_CALLBACK (on_mw__checkbutton_exact_seek_toggled),
                      gpp);
  g_signal_connect (G_OBJECT (mw__entry_basename), "changed",
                      G_CALLBACK (on_mw__entry_basename_changed),
                      gpp);
  g_signal_connect (G_OBJECT (mw__button_basename), "clicked",
                      G_CALLBACK (on_mw__button_basename_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (mw__spinbutton_basenum), "value_changed",
                      G_CALLBACK (on_mw__spinbutton_basenum_changed),
                      gpp);
  g_signal_connect (G_OBJECT (mw__entry_audiofile), "changed",
                      G_CALLBACK (on_mw__entry_audiofile_changed),
                      gpp);
  g_signal_connect (G_OBJECT (mw__button_audiofile), "clicked",
                      G_CALLBACK (on_mw__button_audiofile_clicked),
                      gpp);
  g_signal_connect (G_OBJECT (mw__spinbutton_delace_threshold), "value_changed",
                      G_CALLBACK (on_mw__spinbutton_delace_threshold_changed),
                      gpp);
  g_signal_connect (G_OBJECT (mw__entry_extension), "changed",
                      G_CALLBACK (on_mw__entry_extension_changed),
                      gpp);
  g_signal_connect (G_OBJECT (mw__spinbutton_fn_digits), "value_changed",
                      G_CALLBACK (on_mw__spinbutton_fn_digits_changed),
                      gpp);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (mw__combo_preferred_decoder),
                             GAP_VEX_DECODER_NONE,   /* initial value */
                             G_CALLBACK (on_mw__combo_preferred_decoder),
                             gpp);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (mw__combo_deinterlace),
                             GAP_VEX_DELACE_NONE,   /* initial value */
                             G_CALLBACK (on_mw__combo_deinterlace),
                             gpp);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (mw__combo_mode_multilayer),
                               CLAMP(gpp->val.multilayer, 0, 2),   /* initial value */
                               G_CALLBACK (on_mw__combo_mode_multilayer),
                               gpp);


  p_init_mw__main_window_widgets(gpp);

/* endif GAP_ENABLE_VIDEOAPI_SUPPORT (2) */
#endif

  return mw__main_window;
}  /* end gap_vex_dlg_create_mw__main_window */



/* ----------------------------------
 * gap_vex_dlg_main_dialog
 * ----------------------------------
 */
void
gap_vex_dlg_main_dialog (GapVexMainGlobalParams *gpp)
{
  gimp_ui_init ("gap_video_extract", FALSE);
  gap_stock_init();

  gpp->fsv__fileselection = NULL;
  gpp->fsb__fileselection = NULL;
  gpp->fsa__fileselection = NULL;
  gpp->mw__entry_preferred_decoder = NULL;
  gpp->mw__spinbutton_delace_threshold = NULL;
  gpp->mw__main_window = gap_vex_dlg_create_mw__main_window (gpp);
  gtk_widget_show (gpp->mw__main_window);

  gtk_main ();

  if(gap_debug) printf("A F T E R gtk_main\n");
}  /* end gap_vex_dlg_main_dialog */
