/*  gap_player_main.c
 *  video (preview) playback of video frames based on thumbnails  by Wolfgang Hofer
 *  2003/06/11
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

/* Revision history
 * version 1.3.20d; 2003/10/06  hof: new gpp struct members for resize behaviour
 * version 1.3.19a; 2003/09/06  hof: audiosupport (based on wavplay, for UNIX only),
 * version 1.3.16a; 2003/06/26  hof: param types GimpPlugInInfo.run procedure
 * version 1.3.16a; 2003/06/26  hof: param types GimpPlugInInfo.run procedure
 * version 1.3.16a; 2003/06/26  hof: updated version
 * version 1.3.15a; 2003/06/21  hof: created
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_player_main.h"
#include "gap_player_cache.h"
#include "gap_player_dialog.h"
#include "gap_story_file.h"
#include "gap_pview_da.h"

#include "gap-intl.h"

/* Defines */
#define PLUG_IN_NAME        "plug_in_gap_videoframes_player"
#define PLUG_IN_HELP_ID     "plug-in-gap-videoframes-player"
#define PLUG_IN_PRINT_NAME  "Videopreview Player"
#define PLUG_IN_IMAGE_TYPES "RGB*, INDEXED*, GRAY*"
#define PLUG_IN_AUTHOR      "Wolfgang Hofer (hof@gimp.org)"
#define PLUG_IN_COPYRIGHT   "Wolfgang Hofer"


int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */ 

static GapPlayerMainGlobalParams global_params =
{
  TRUE        /* standalone_mode */
, GIMP_RUN_INTERACTIVE
, -1          /* gint32  image_id */
, NULL        /* gchar        *imagename */
, -1          /* gint32  imagewidth */
, -1          /* gint32  imageheight */
, 0.0         /* gdouble aspect_ratio */
, NULL        /* GapAnimInfo *ainfo_ptr */
, NULL        /* GapAnimInfo *stb_ptr */
, NULL        /* t_GVA_Handle *gvahand */
, NULL        /* gchar *gva_videofile */
, -1                       /* gint32  mtrace_image_id */
, GAP_PLAYER_MTRACE_OFF    /* mtrace_mode */
, NULL                     /* GapPlayerSetRangeFptr fptr_set_range */      
, NULL                     /* gpointer user_data_ptr */      

, FALSE       /*  gboolean   autostart */
, FALSE       /*  gboolean   caller_range_linked */
, TRUE        /*  gboolean   use_thumbnails */
, TRUE        /*  gboolean   exact_timing */
, FALSE       /*  gboolean   play_is_active */
, TRUE        /*  gboolean   play_selection_only */
, TRUE        /*  gboolean   play_loop */
, FALSE       /*  gboolean   play_pingpong */
, FALSE       /*  gboolean   play_backward */
, FALSE       /*  gboolean   request_cancel_video_api */
, FALSE       /*  gboolean   cancel_video_api */
, FALSE       /*  gboolean   gva_lock */

, -1          /*  gint32     play_timertag */

, 0           /*  gint32   begin_frame */
, 0           /*  gint32   end_frame */
, 0           /*  gint32   play_current_framenr */
, 0           /*  gint32   pb_stepsize */
, 24.0        /*  gdouble  speed;     playback speed fps */
, 24.0        /*  gdouble  original_speed;    playback speed fps */
, 24.0        /*  gdouble  prev_speed;    previous playback speed fps */
, 256         /*  gint32   pv_pixelsize  32 upto 512 */
, 256         /*  gint32   pv_width  32 upto 512 */
, 256         /*  gint32   pv_height  32 upto 512 */
  
, FALSE       /*  gboolean in_feedback */
, FALSE       /*  gboolean in_timer_playback */
, FALSE       /*  gboolean in_resize */

, 0           /*  gint32  go_job_framenr */
, -1          /*  gint32  go_timertag */
, -1          /*  gint32  go_base_framenr */
, -1          /*  gint32  go_base */
, 0           /*  gint32  pingpong_count */
  
, NULL        /*  GapPView   *pv_ptr */
, NULL        /*  GtkWidget *shell_window */
, NULL        /*  GtkWidget *docking_container */
, NULL        /*  GtkWidget *frame_with_name */
, NULL        /*  GtkObject *from_spinbutton_adj */
, NULL        /*  GtkObject *to_spinbutton_adj */
, NULL        /*  GtkWidget *framenr_scale */
, NULL        /*  GtkObject *framenr_spinbutton_adj */
, NULL        /*  GtkObject *speed_spinbutton_adj */
, NULL        /*  GtkObject *size_spinbutton_adj */
   
, NULL        /*  GtkWidget *progress_bar */
, NULL        /*  GtkWidget *status_label */
, NULL        /*  GtkWidget *timepos_label */
, NULL        /*  GtkWidget *resize_box */
, NULL        /*  GtkWidget *size_spinbutton */
, NULL        /*  GtkWidget *from_button */
, NULL        /*  GtkWidget *to_button */

, NULL        /*  GtkWidget *use_thumb_checkbutton */
, NULL        /*  GtkWidget *exact_timing_checkbutton */
, NULL        /*  GtkWidget *pinpong_checkbutton */
, NULL        /*  GtkWidget *selonly_checkbutton */
, NULL        /*  GtkWidget *loop_checkbutton */

, NULL        /*  Gtimer    *gtimer */
, 0.0         /*  gdouble   cycle_time_secs  */
, 0.0         /*  gdouble   rest_secs  */
, 0.0         /*  gdouble   delay_secs  */
, 0.0         /*  gdouble   framecnt  */
, 1           /*  gint32    seltrack  */
, 0.0         /*  gdouble   delace  */
, NULL        /*  gchar    *preferred_decoder */
, FALSE       /*  gboolean  force_open_as_video */
, FALSE       /*  gboolean  have_progress_bar */
, NULL        /*  gchar    *progress_bar_idle_txt */
, 0           /*  gint32    resize_handler_id */
, 0           /*  gint32    old_resize_width */
, 0           /*  gint32    old_resize_height */
, TRUE        /*  gboolean  startup */
, -1          /*  gint32    shell_initial_width */
, -1          /*  gint32    shell_initial_height */

, FALSE       /* audio_enable */
, 0           /* audio_resync */
, "\0"        /* audiofile */
, "\0"        /* audio_wavfile_tmp */
, 0           /* audio_frame_offset */
, 0           /* audio_samplerate */
, 0           /* audio_required_samplerate */
, 0           /* audio_bits */
, 0           /* audio_channels */
, 0           /* audio_samples */
, 0           /* audio_status */
, 1.0         /* audio_volume */
, 0           /* audio_tmp_samplerate */
, 0           /* audio_tmp_samples */
, FALSE       /* audio_tmp_resample */
, FALSE       /* audio_tmp_dialog_is_open */
, NULL        /* audio_filename_entry */
, NULL        /* audio_offset_time_label */
, NULL        /* audio_total_time_label */
, NULL        /* audio_total_frames_label */
, NULL        /* audio_samples_label */
, NULL        /* audio_samplerate_label */
, NULL        /* audio_bits_label */
, NULL        /* audio_channels_label */
, NULL        /* audio_volume_spinbutton_adj */
, NULL        /* audio_frame_offset_spinbutton_adj */
, NULL        /* audio_filesel */
, NULL        /* audio_table */
, NULL        /* audio_status_label */
, NULL        /* video_total_time_label */
, NULL        /* video_total_frames_label */
, FALSE       /* vindex_creation_is_running */
, NULL        /* play_n_stop_hbox */
, NULL        /* cancel_vindex_button */
, NULL        /* help_id */
, FALSE       /* onion_delete */

, NULL        /* cache_size_spinbutton_adj */
, NULL        /* label_current_cache_values */
, NULL        /* progress_bar_cache_usage */
, GAP_PLAYER_CACHE_DEFAULT_MAX_BYTESIZE  /* max_player_cache */
, GAP_PLAYER_CACHE_COMPRESSION_NONE      /* cache_compression */
, 0.86                                   /* cache_jpeg_quality */

, TRUE        /* show_go_buttons */
, TRUE        /* show_position_scale */
, NULL        /* show_go_buttons_checkbutton */
, NULL        /* show_position_scale_checkbutton */
, NULL        /* gobutton_hbox */
, NULL        /* frame_scale_hbox */
, GAP_STB_FLIP_NONE  /* flip_request */
, GAP_STB_FLIP_NONE  /* flip_status */
, 1                  /* stb_in_track */
,-1                  /* stb_parttype */
,-1                  /* stb_unique_id */
, NULL               /* stb_comp_vidhand */

, FALSE               /* audio_auto_offset_by_framenr */
, 1                   /* audio_otone_atrack */
, NULL                /* GtkWidget *audio_auto_offset_by_framenr_checkbutton */
, NULL                /* GtkWidget *audio_otone_extract_button */
, NULL                /* GtkWidget *audio_otone_atrack_spinbutton */
, NULL                /* GtkObject *audio_otone_atrack_spinbutton_adj */
, NULL                /* GtkWidget *progress_bar_audio */
, NULL                /* GtkWidget *audio_enable_checkbutton */

, NULL                /* GapDrawableVideoRef  *dvref_ptr */
, FALSE               /* gboolean     enableDetailTracking */
, GAP_PLAYER_MAIN_DEFAULT_CACHE_NTILES  /* gulong cache_ntiles */
, NULL                /* GtkObject *cache_ntiles_spinbutton_adj */
, NULL                /* GtkWidget   *detail_tracking_checkbutton */
, TRUE                /* gboolean     enableAutoSkipMissingFrames */
, NULL                /* GtkWidget   *autoSkipMissingFrames_checkbutton */
};


static void  query (void);
static void  run (const gchar *name,
                  gint nparams,              /* number of parameters passed in */
                  const GimpParam * param,   /* parameters passed in */
                  gint *nreturn_vals,        /* number of parameters returned */
                  GimpParam ** return_vals); /* parameters to be returned */


/* Global Variables */
GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,   /* init_proc  */
  NULL,   /* quit_proc  */
  query,  /* query_proc */
  run     /* run_proc   */
};

static GimpParamDef in_args[] = {
                  { GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive"},
                  { GIMP_PDB_IMAGE,    "image", "Input image" },
                  { GIMP_PDB_DRAWABLE, "drawable", "UNUSED"},
                  { GIMP_PDB_INT32,    "range_from", "framenr begin of selected range"},
                  { GIMP_PDB_INT32,    "range_to", "framenr end of selected range"},
                  { GIMP_PDB_INT32,    "autostart", "(0) FALSE: no autostart, (1) TRUE: automatc playback start"},
                  { GIMP_PDB_INT32,    "use_thumbnails", "(0) FALSE: playback original size frame images, (1) TRUE playback using thumbnails where available (much faster)"},
                  { GIMP_PDB_INT32,    "exact_timing", "(0) FALSE: disable frame dropping, (1) TRUE: allow dropping frames to keep exact timing"},
                  { GIMP_PDB_INT32,    "play_selection_only", "(0) FALSE: play all frames, (1) TRUE: play selected range only"},
                  { GIMP_PDB_INT32,    "play_loop", "(0) FALSE: play once, (1) TRUE: play in loop"},
                  { GIMP_PDB_INT32,    "play_pingpong", "(0) FALSE: normal play, (1) TRUE: bidirectional play"},
                  { GIMP_PDB_FLOAT,    "speed", "-1 use original speed  framerate in frames/sec (valid range is 1.0 upto 250.0)"},
                  { GIMP_PDB_INT32,    "size_width", "-1 use standard size"},
                  { GIMP_PDB_INT32,    "size_height", "-1 use standard size"},
                  { GIMP_PDB_INT32,    "audio_enable", "(0) FALSE: NO Audio, (1) TRUE: play audio too (requires UNIX, and intalled wavplay as audio server)"},
                  { GIMP_PDB_STRING,   "audio_filename", "Name of audiofile (must be RIFF WAV fileformat, PCM encoded)"},
                  { GIMP_PDB_INT32,    "audio_frame_offset", "0: audio starts at 1.st videoframe +n: start audio after n silent frames, -n: audio starts 10 frames before the video (this part of the audio is clipped and not played)"},
                  { GIMP_PDB_FLOAT,    "audio_volume", "range is 0.0 upto 1.0"},
  };

/* Functions */

MAIN ()

static void query (void)
{
  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  /* the actual installation of the plugin */
  gimp_install_procedure (PLUG_IN_NAME,
                          "Video Preview Playback",
                          "This plug-in does videoplayback, "
                          "based on thumbnail preview or full sized video frames.",
                          PLUG_IN_AUTHOR,
                          PLUG_IN_COPYRIGHT,
                          GAP_VERSION_WITH_DATE,
                          N_("Playback..."),
                          PLUG_IN_IMAGE_TYPES,
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (in_args),
                          0,        /* G_N_ELEMENTS (out_args) */
                          in_args,
                          NULL      /* out_args */
                          );

  // gimp_plugin_menu_branch_register("<Image>", "Video");
  gimp_plugin_menu_register (PLUG_IN_NAME, N_("<Image>/Video/"));
}

static void
run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  const gchar *l_env;
  gint32       image_id = -1;
  GapPlayerMainGlobalParams  *gpp = &global_params;

  /* Get the runmode from the in-parameters */
  GimpRunMode run_mode = param[0].data.d_int32;

  /* status variable, use it to check for errors in invocation usualy only
   * during non-interactive calling 
   */
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  /* always return at least the status to the caller. */
  static GimpParam values[1];

  INIT_I18N();


  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  if(gap_debug) fprintf(stderr, "\n\nDEBUG: run %s\n", name);

  /* initialize the return of the status */
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
  *nreturn_vals = 1;
  *return_vals = values;


  /* get image and drawable */
  image_id = param[1].data.d_int32;


  switch (run_mode)
  {
   case GIMP_RUN_INTERACTIVE:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (PLUG_IN_NAME, gpp);
      gpp->autostart = FALSE;
      gpp->pv_pixelsize = 256;
      gpp->pv_width = 256;
      gpp->pv_height = 256;
      break;

    case GIMP_RUN_NONINTERACTIVE:
      /* check to see if invoked with the correct number of parameters */
      if (nparams == G_N_ELEMENTS (in_args))
      {
          gpp->begin_frame          = (gint) param[3].data.d_int32;
          gpp->end_frame            = (gint) param[4].data.d_int32;
          gpp->autostart            = (gint) param[5].data.d_int32;
          gpp->use_thumbnails       = (gint) param[6].data.d_int32;
          gpp->exact_timing         = (gint) param[7].data.d_int32;
          gpp->play_selection_only  = (gint) param[8].data.d_int32;
          gpp->play_loop            = (gint) param[9].data.d_int32;
          gpp->play_pingpong        = (gint) param[10].data.d_int32;
          gpp->speed                = (gdouble) param[11].data.d_float;
          gpp->pv_width             = (gint) param[12].data.d_int32;
          gpp->pv_height            = (gint) param[13].data.d_int32;

          gpp->pv_pixelsize = MAX(gpp->pv_width, gpp->pv_height);
          
          gpp->audio_enable         = (gboolean) param[14].data.d_int32;
          if(param[15].data.d_string)
          {
            g_snprintf(gpp->audio_filename, sizeof(gpp->audio_filename), "%s"
                      , param[15].data.d_string);
          }
          else
          {
            gpp->audio_filename[0] = '\0';
            gpp->audio_enable = FALSE;
          }
          gpp->audio_frame_offset   = (gint) param[16].data.d_int32;
          gpp->audio_volume         = (gdouble) param[17].data.d_float;
      }
      else
      {
        printf("%s: noninteractive call wrong nuber %d of params were passed. (%d params are required)\n"
              , PLUG_IN_NAME
              , (int)nparams
              , (int)G_N_ELEMENTS (in_args)
              );
        status = GIMP_PDB_CALLING_ERROR;
      }
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /* Possibly retrieve data from a previous run */
      gimp_get_data (PLUG_IN_NAME, gpp);
      gpp->autostart = FALSE;

      break;

    default:
      break;
  }

  if (status == GIMP_PDB_SUCCESS)
  {
    
    gpp->image_id = image_id;
    gpp->run_mode = run_mode;
    gpp->stb_ptr  = NULL;
    gpp->help_id  = PLUG_IN_HELP_ID;
    gap_player_dlg_playback_dialog(gpp);
  
    /* Store variable states for next run */
    if (run_mode == GIMP_RUN_INTERACTIVE)
      gimp_set_data (PLUG_IN_NAME, gpp, sizeof (GapPlayerMainGlobalParams));
  }
  values[0].data.d_status = status;
}       /* end run */
