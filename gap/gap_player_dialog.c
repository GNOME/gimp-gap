/*  gap_player_dialog.c
 *
 *  video (preview) playback of video frames  by Wolfgang Hofer (hof)
 *     supports both (fast) thumbnail based playback
 *     and full image playback (slow)
 *  the current implementation has audio support for RIFF WAV audiofiles
 *  but requires the wavplay executable as external audioserver program.
 *  2003/09/07
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
 *  (2007/11/01)  v2.3.0     hof: - gimprc changed to "show-tooltips" with gimp-2.4
 *  (2004/11/12)  v2.1.0     hof: - added help button
 *  (2004/03/17)  v1.3.27a   hof: - go_timer does check if video api is busy and retries
 *                                  to display the wanted frame if video fetch (of previous frame)
 *                                  has finished.
 *                                - use go_timer to display final frame when play_timer stops.
 *                                - use go_timer to display current frame on_framenr_spinbutton_changed
 *  (2004/03/16)  v1.3.27a   hof: - merged in changes from stable branch:
 *                                  declare varibales for audiosupport only if GAP_ENABLE_AUDIO_SUPPORT defined
 *  (2004/03/07)  v1.3.26b   hof: - click on preview opens/replaces the mtrace image
 *  (2004/03/04)  v1.3.26b   hof: - bugfixes Videofile playback
 *  (2004/02/29)  v1.3.26b   hof: - Basic Videoapi support (p_fetch_videoframe)
 *  (2004/02/26)  v1.3.26b   hof: - From/To convert from Button to Label
 *  (2004/02/14)  v1.3.26b   hof: - if not standalone: Player can run in docked_mode (without having own shell window)
 *  (2004/02/01)  v1.3.26b   hof: - Player can run in standalone mode (as it did before)
 *                                  or act as Playback widget window that does not call gtk_main_quit
 *                                  as it is required for use in the Storyboard dialog module.
 *                                - Support for Storyboard level1 playback
 *                                - most callbacks now directly use GapPlayerMainGlobalParams *gpp
 *                                  rather than  gpointer  *user_data
 *  (2004/01/22)  v1.3.25a   hof: performance tuning: use gap_thumb_file_load_pixbuf_thumbnail
 *  (2004/01/19)  v1.3.24b   hof: bugfix: spinbutton callbacks must connect to "value_changed" signal
 *  (2004/01/16)  v1.3.22c   hof: use gap_thumb_file_load_thumbnail (for faster thumb loading)
 *  (2003/11/15)  v1.3.22c   hof: bugfix: SHIFT size button
 *  (2003/11/01)  v1.3.21d   hof: cleanup messages
 *  (2003/10/14)  v1.3.20d   hof: sourcecode cleanup
 *  (2003/10/06)  v1.3.20d   hof: bugfix: changed shell_window resize handling
 *  (2003/09/29)  v1.3.20c   hof: moved gap_arr_overwrite_file_dialog to module gap_arr_dialog.c
 *  (2003/09/23)  v1.3.20b   hof: use GAPLIBDIR to locate audioconvert_to_wav.sh
 *  (2003/09/14)  v1.3.20a   hof: bugfix: added p_create_wav_dialog
 *                                now can create and resample WAVFILE from other audiofiles (MP3 and others)
 *                                based on external shellscript (using SOX and LAME to do that job)
 *                                Replaced direct wavplay Procedure calls
 *                                by abstracted AudioPlayerClient (APCL_) Procedures
 *  (2003/09/09)  v1.3.19b   hof: bugfix: on_framenr_spinbutton_changed must resync audio to the new Position when playing
 *                                bugfix: Selection of a new audiofile did continue play the old one
 *  (2003/09/07)  v1.3.19a   hof: audiosupport (based on wavplay, for UNIX only),
 *                                audiosupport is on by default, and can be disabled by defining
 *                                   GAP_DISABLE_WAV_AUDIOSUPPORT
 *  (2003/08/27)  v1.3.18b   hof: added ctrl/alt modifiers on_go_button_clicked,
 *                                added p_printout_range (for STORYBOARD FILE Processing
 *                                in the still unpublished GAP Videoencoder Project)
 *  (2003/07/31)  v1.3.17b   hof: message text fixes for translators (# 118392)
 *  (2003/06/26)  v1.3.16a   hof: bugfix: make preview drawing_area fit into frame (use an aspect_frame)
 *                                query gimprc for "show-tool-tips"
 *  (2003/06/21)  v1.3.15a   hof: created
 */

/* undefining GAP_ENABLE_AUDIO_SUPPORT will disable all audio stuff
 *  at compiletime
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib/gstdio.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "gap_libgapbase.h"
#include "gap_player_main.h"
#include "gap_player_dialog.h"
#include "gap_pdb_calls.h"
#include "gap_pview_da.h"
#include "gap_stock.h"
#include "gap_lib.h"
#include "gap_image.h"
#include "gap_vin.h"
#include "gap_timeconv.h"
#include "gap_thumbnail.h"
#include "gap_arr_dialog.h"
#include "gap_story_file.h"
#include "gap_story_render_processor.h"
#include "gap_layer_copy.h"
#include "gap_onion_base.h"
#include "gap_audio_extract.h"
#include "gap_audio_extract.h"
#include "gap_drawable_vref_parasite.h"
#include "gap_detail_tracking_exec.h"
#include "gap_audio_wav.h"

#include "gap-intl.h"

extern int gap_debug;  /* 1 == print debug infos , 0 dont print debug infos */
int cmdopt_x = 0;                               /* Debug option flag for wavplay */

#include "gap_apcl_lib.h"   /* headerfile for libwavplayclient (preferred) */

#ifdef GAP_ENABLE_AUDIO_SUPPORT_WAVPLAY


char *env_WAVPLAYPATH = WAVPLAYPATH;            /* Default pathname of executable /usr/local/bin/wavplay */
char *env_AUDIODEV = AUDIODEV;                  /* Default compiled in audio device */
unsigned long env_AUDIOLCK = AUDIOLCK;          /* Default compiled in locking semaphore */

#else

char *env_WAVPLAYPATH = NULL;                   /* Default pathname of executable /usr/local/bin/wavplay */
char *env_AUDIODEV = NULL;                      /* Default compiled in audio device */
unsigned long env_AUDIOLCK = 0;                 /* Default compiled in locking semaphore */
#endif

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
#include "gap_vid_api.h"
#endif

#define GAP_PLAYER_MIN_SIZE 64
#define GAP_PLAYER_MAX_SIZE 800
#define GAP_STANDARD_PREVIEW_SIZE 256
#define GAP_SMALL_PREVIEW_SIZE 128

#define GAP_PLAYER_CHECK_SIZE 6
#define GAP_PLAY_MAX_GOBUTTONS 51
#define GAP_PLAY_MAX_GOBUTTONS_DOCKED 41
#define GAP_PLAYER_MIDDLE_GO_NUMBER ((GAP_PLAY_MAX_GOBUTTONS / 2))
#define GAP_PLAY_AUDIO_ENTRY_WIDTH 300
#define GAP_PLAY_AUDIO_ENTRY_WIDTH_DOCKED 130

#define GAP_PLAYER_VID_FRAMES_TO_KEEP_CACHED 50

#define KEY_FRAMENR_BUTTON_TYPE  "gap_player_framnr_button_type"
#define FRAMENR_BUTTON_BEGIN 0
#define FRAMENR_BUTTON_END   1


typedef struct t_gobutton
{
  GapPlayerMainGlobalParams *gpp;
  gint             go_number;
} t_gobutton;


static gint32 global_max_vid_frames_to_keep_cached = 0;


/* the callbacks */
static void   on_shell_window_destroy                (GtkObject       *object,
                                                      GapPlayerMainGlobalParams *gpp);
static void   on_help_button_clicked                 (GtkButton       *button,
                                                      GapPlayerMainGlobalParams *gpp);

static void   on_from_spinbutton_changed             (GtkEditable     *editable,
                                                      GapPlayerMainGlobalParams *gpp);

static void   on_to_spinbutton_changed               (GtkEditable     *editable,
                                                      GapPlayerMainGlobalParams *gpp);



static gboolean on_vid_preview_button_press_event    (GtkWidget       *widget,
                                                      GdkEventButton  *bevent,
                                                      GapPlayerMainGlobalParams *gpp);
static gboolean on_warp_frame_scroll_event      (GtkWidget       *widget,
                                                      GdkEventScroll  *sevent,
                                                      GapPlayerMainGlobalParams *gpp);
static gboolean on_vid_preview_expose_event          (GtkWidget       *widget,
                                                      GdkEventExpose  *eevent,
                                                      GapPlayerMainGlobalParams *gpp);
static void   on_vid_preview_size_allocate           (GtkWidget       *widget,
                                                      GtkAllocation   *allocation,
                                                      GapPlayerMainGlobalParams *gpp);
static void   on_shell_window_size_allocate           (GtkWidget       *widget,
                                                      GtkAllocation   *allocation,
                                                      GapPlayerMainGlobalParams *gpp);

static void   on_framenr_button_clicked             (GtkButton       *button,
                                                     GdkEventButton  *bevent,
                                                     GapPlayerMainGlobalParams *gpp);
static void   on_from_button_clicked                 (GtkButton       *button,
                                                      GapPlayerMainGlobalParams *gpp);
static void   on_to_button_clicked                   (GtkButton       *button,
                                                      GapPlayerMainGlobalParams *gpp);

static gboolean on_framenr_scale_clicked             (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      GapPlayerMainGlobalParams *gpp);

static void   on_framenr_spinbutton_changed          (GtkEditable     *editable,
                                                      GapPlayerMainGlobalParams *gpp);


static void   on_origspeed_button_clicked            (GtkButton       *button,
                                                      GapPlayerMainGlobalParams *gpp);
static void   on_speed_spinbutton_changed            (GtkEditable     *editable,
                                                      GapPlayerMainGlobalParams *gpp);
static void   p_fit_initial_shell_window             (GapPlayerMainGlobalParams *gpp);
static void   p_fit_shell_window                     (GapPlayerMainGlobalParams *gpp);


static gboolean   on_size_button_button_press_event  (GtkWidget       *widget,
                                                      GdkEventButton  *event,
                                                      GapPlayerMainGlobalParams *gpp);



static void   on_size_spinbutton_changed             (GtkEditable     *editable,
                                                      GapPlayerMainGlobalParams *gpp);
static gboolean   on_size_spinbutton_enter           (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      GapPlayerMainGlobalParams *gpp);
static gboolean   on_shell_window_leave              (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      GapPlayerMainGlobalParams *gpp);


static void   on_exact_timing_checkbutton_toggled    (GtkToggleButton *togglebutton,
                                                      GapPlayerMainGlobalParams *gpp);

static void   on_use_thumb_checkbutton_toggled       (GtkToggleButton *togglebutton,
                                                      GapPlayerMainGlobalParams *gpp);

static void   on_pinpong_checkbutton_toggled         (GtkToggleButton *togglebutton,
                                                      GapPlayerMainGlobalParams *gpp);

static void   on_selonly_checkbutton_toggled         (GtkToggleButton *togglebutton,
                                                      GapPlayerMainGlobalParams *gpp);

static void   on_loop_checkbutton_toggled            (GtkToggleButton *togglebutton,
                                                      GapPlayerMainGlobalParams *gpp);

static void   on_show_gobuttons_checkbutton_toggled  (GtkToggleButton *togglebutton,
                                                      GapPlayerMainGlobalParams *gpp);
static void   on_show_positionscale_checkbutton_toggled (GtkToggleButton *togglebutton,
                                                      GapPlayerMainGlobalParams *gpp);

static void   on_play_button_clicked                (GtkButton       *button,
                                                     GdkEventButton  *bevent,
                                                     GapPlayerMainGlobalParams *gpp);

static gboolean on_pause_button_press_event         (GtkButton       *button,
                                                     GdkEventButton  *bevent,
                                                     GapPlayerMainGlobalParams *gpp);

static void   on_back_button_clicked                (GtkButton       *button,
                                                     GdkEventButton  *bevent,
                                                     GapPlayerMainGlobalParams *gpp);

static void   on_close_button_clicked               (GtkButton       *button,
                                                     GapPlayerMainGlobalParams *gpp);


static void   on_timer_playback                      (GapPlayerMainGlobalParams *gpp);

static void   on_timer_go_job                        (GapPlayerMainGlobalParams *gpp);

static void   on_go_button_clicked                   (GtkButton       *button,
                                                      GdkEventButton  *bevent,
                                                      t_gobutton *gob);

static gboolean   on_go_button_enter                 (GtkButton       *button,
                                                      GdkEvent        *event,
                                                      t_gobutton *gob);

static void   on_gobutton_hbox_leave                 (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      GapPlayerMainGlobalParams *gpp);

static void   on_audio_enable_checkbutton_toggled (GtkToggleButton *togglebutton,
                                                   GapPlayerMainGlobalParams *gpp);
static void   on_audio_volume_spinbutton_changed  (GtkEditable     *editable,
                                                   GapPlayerMainGlobalParams *gpp);
static void   on_audio_frame_offset_spinbutton_changed (GtkEditable     *editable,
                                                      GapPlayerMainGlobalParams *gpp);
static void   on_audio_reset_button_clicked       (GtkButton       *button,
                                                   GapPlayerMainGlobalParams *gpp);
static void   on_audio_create_copy_button_clicked (GtkButton       *button,
                                                   GapPlayerMainGlobalParams *gpp);
static void   on_audio_filesel_button_clicked      (GtkButton       *button,
                                                   GapPlayerMainGlobalParams *gpp);
static void   on_audio_filename_entry_changed     (GtkWidget     *widget,
                                                   GapPlayerMainGlobalParams *gpp);

static void   on_cancel_vindex_button_clicked     (GtkObject       *object,
                                                   GapPlayerMainGlobalParams *gpp);

static void   on_audio_auto_offset_checkbutton_toggled    (GtkToggleButton *togglebutton,
                                                   GapPlayerMainGlobalParams *gpp);
static void   on_audio_otone_extract_button_clicked (GtkButton       *button,
                                                   GapPlayerMainGlobalParams *gpp);
static void   on_audio_otone_atrack_spinbutton_changed     (GtkEditable     *editable,
                                                   GapPlayerMainGlobalParams *gpp);

static void     p_msg_progress_bar_audio(GapPlayerMainGlobalParams *gpp, const char *msg);
static void     p_reset_progress_bar_audio(GapPlayerMainGlobalParams *gpp);

static void     p_step_frame(GapPlayerMainGlobalParams *gpp, gint stepsize);

static void     p_set_frame_with_name_label(GapPlayerMainGlobalParams *gpp);
static void     p_update_position_widgets(GapPlayerMainGlobalParams *gpp);
static void     p_stop_playback(GapPlayerMainGlobalParams *gpp);
static void     p_connect_resize_handler(GapPlayerMainGlobalParams *gpp);
static void     p_disconnect_resize_handler(GapPlayerMainGlobalParams *gpp);

static gboolean p_vid_progress_callback(gdouble progress
                       ,gpointer user_data
                       );
static void     p_close_videofile(GapPlayerMainGlobalParams *gpp);
static void     p_open_videofile(GapPlayerMainGlobalParams *gpp
                , char *filename
                , gint32 seltrack
                , gdouble delace
                , const char *preferred_decoder
                );
static guchar * p_fetch_videoframe(GapPlayerMainGlobalParams *gpp
                   , char *gva_videofile
                   , gint32 framenumber
                   , gint32 rangesize
                   , gint32 seltrack
                   , gdouble delace
                   , const char *preferred_decoder
                   , gint32 *th_bpp
                   , gint32 *th_width
                   , gint32 *th_height
                   , gboolean isBackwards
                   );
static void     p_init_tile_cache(GapPlayerMainGlobalParams *gpp);
static void     p_init_video_playback_cache(GapPlayerMainGlobalParams *gpp);
static void     p_init_layout_options(GapPlayerMainGlobalParams *gpp);
static guchar * p_fetch_frame_via_cache(GapPlayerMainGlobalParams *gpp
                   , const gchar *ckey
                   , gint32 *th_bpp_ptr
                   , gint32 *th_width_ptr
                   , gint32 *th_height_ptr
                   , gint32 *flip_status_ptr
                   );
static guchar * p_fetch_videoframe_via_cache(GapPlayerMainGlobalParams *gpp
                   , char *gva_videofile
                   , gint32 framenumber
                   , gint32 rangesize
                   , gint32 seltrack
                   , gdouble delace
                   , const char *preferred_decoder
                   , gint32 *th_bpp_ptr
                   , gint32 *th_width_ptr
                   , gint32 *th_height_ptr
                   , gint32 *flip_status_ptr
                   , const gchar *ckey
                   , gboolean isBackwards
                   );
static void     p_frame_chache_processing(GapPlayerMainGlobalParams *gpp
                   , const gchar *ckey);
static void     p_update_cache_status (GapPlayerMainGlobalParams *gpp);

static void     p_audio_startup_server(GapPlayerMainGlobalParams *gpp);
static gint32       p_get_audio_relevant_FrameNr(GapPlayerMainGlobalParams *gpp, gint32 framenr);
static const char*  p_get_default_tmp_otone_audiofilename();
static char *       p_build_otone_audiofilename(const char *videofilename, gint32 atrack, const char *suffix);
static gboolean     p_check_otone_workfile_up_to_date(const char *videofilename, const char *audiofilename,
                              gint32 l_extract_audiotrack);
static void         p_update_ainfo_for_videofile(GapPlayerMainGlobalParams *gpp);


/* -----------------------------
 * p_check_tooltips
 * -----------------------------
 */
static void
p_check_tooltips(void)
{
  gap_base_check_tooltips(NULL);
}  /* end p_check_tooltips */


/* -----------------------------
 * p_audio_errfunc
 * -----------------------------
 */
static void
p_audio_errfunc(const char *format,va_list ap)
{
  char buf[1024];

  vsnprintf(buf,sizeof(buf),format,ap); /* Format the message */
  g_message(_("Problem with audioplayback. The audiolib reported:\n%s"),buf);

}  /* end p_audio_errfunc */


/* -----------------------------
 * p_create_wav_dialog
 * -----------------------------
 * return TRUE : OK, caller can create the wavefile
 *        FALSE: user has cancelled, dont create wavefile
 */
static gboolean
p_create_wav_dialog(GapPlayerMainGlobalParams *gpp)
{
  static GapArrArg  argv[4];
  gint   l_ii;
  gint   l_ii_resample;
  gint   l_ii_samplerate;

  l_ii = 0;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_ii].label_txt = _("Audiosource:");
  argv[l_ii].text_buf_ret = gpp->audio_filename;

  g_snprintf(gpp->audio_wavfile_tmp
            ,sizeof(gpp->audio_wavfile_tmp)
            ,"%s.tmp.wav"
            ,gpp->audio_filename
            );

  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_FILESEL);
  argv[l_ii].label_txt = _("Wavefile:");
  argv[l_ii].entry_width = 400;
  argv[l_ii].help_txt  = _("Name of wavefile to create as copy in RIFF WAVE format");
  argv[l_ii].text_buf_len = sizeof(gpp->audio_wavfile_tmp);
  argv[l_ii].text_buf_ret = gpp->audio_wavfile_tmp;

  l_ii++;
  l_ii_resample = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TOGGLE);
  argv[l_ii].label_txt = _("Resample:");
  argv[l_ii].help_txt  = _("ON: Resample the copy at specified samplerate.\n"
                           "OFF: Use original samplerate");
  argv[l_ii].int_ret   = 1;

  l_ii++;
  l_ii_samplerate = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("Samplerate:");
  argv[l_ii].help_txt  = _("Target audio samplerate in samples/sec. Ignored if resample is off");
  argv[l_ii].int_min   = (gint)GAP_PLAYER_MAIN_MIN_SAMPLERATE;
  argv[l_ii].int_max   = (gint)GAP_PLAYER_MAIN_MAX_SAMPLERATE;
  argv[l_ii].int_ret   = (gint)22050;

  if(gpp->audio_samples > 0)
  {
    /* the original is a valid wavefile
     * in that case we know the samplerate of the original audiofile
     * and can limit the samplerate of the copy to this size
     * (resample with higher rates does not improve quality and is a waste of memory.
     *  Making a copy of the input wavfile should use a lower samplerate
     *  that makes it possible for the audioserver to follow fast videoplayback
     *  by switching from the original to the copy)
     */
    argv[l_ii].int_min   = (gint)GAP_PLAYER_MAIN_MIN_SAMPLERATE;
    argv[l_ii].int_max   = (gint)MIN(gpp->audio_samplerate, GAP_PLAYER_MAIN_MAX_SAMPLERATE);
    argv[l_ii].int_ret   = (gint)MIN((gpp->audio_samplerate / 2), GAP_PLAYER_MAIN_MAX_SAMPLERATE);
  }


  if(TRUE == gap_arr_ok_cancel_dialog( _("Copy Audiofile as Wavefile"),
                                 _("Settings"),
                                  G_N_ELEMENTS(argv), argv))
  {
     gpp->audio_tmp_resample   = (gboolean)(argv[l_ii_resample].int_ret);
     gpp->audio_tmp_samplerate = (gint32)(argv[l_ii_samplerate].int_ret);

     return (gap_arr_overwrite_file_dialog(gpp->audio_wavfile_tmp));
  }

  return (FALSE);
}  /* end p_create_wav_dialog */




/* -----------------------------
 * p_audio_shut_server
 * -----------------------------
 */
static void
p_audio_shut_server(GapPlayerMainGlobalParams *gpp)
{
#ifdef GAP_ENABLE_AUDIO_SUPPORT
  /* if (gap_debug) printf("p_audio_shut_server\n"); */
  if(gpp->audio_status > GAP_PLAYER_MAIN_AUSTAT_NONE)
  {
    apcl_bye(0, p_audio_errfunc);
  }
  gpp->audio_status = GAP_PLAYER_MAIN_AUSTAT_NONE;
#endif
  return;
}  /* end p_audio_shut_server */


/* -----------------------------
 * p_audio_resync
 * -----------------------------
 */
static void
p_audio_resync(GapPlayerMainGlobalParams *gpp)
{
#ifdef GAP_ENABLE_AUDIO_SUPPORT
  if(gpp->audio_resync < 1)
  {
    gpp->audio_resync = 1 + (gpp->speed / 5);
  }
  /* if (gap_debug) printf("p_audio_resync :%d\n", (int)gpp->audio_resync); */
#endif
  return;
}  /* end p_audio_resync */

/* -----------------------------
 * p_audio_stop
 * -----------------------------
 */
static void
p_audio_stop(GapPlayerMainGlobalParams *gpp)
{
#ifdef GAP_ENABLE_AUDIO_SUPPORT
  /* if (gap_debug) printf("p_audio_stop\n"); */
  if(gpp->audio_status > GAP_PLAYER_MAIN_AUSTAT_NONE)
  {
    apcl_stop(0,p_audio_errfunc);  /* Tell the server to stop */
    gpp->audio_status = MIN(gpp->audio_status, GAP_PLAYER_MAIN_AUSTAT_FILENAME_SET);
  }
#endif
  return;
}  /* end p_audio_stop */

/* -----------------------------
 * p_audio_init
 * -----------------------------
 */
static void
p_audio_init(GapPlayerMainGlobalParams *gpp)
{
#ifdef GAP_ENABLE_AUDIO_SUPPORT
  /* if (gap_debug) printf("p_audio_init\n"); */
  if(gpp->audio_samples > 0)       /* audiofile has samples (seems to be a valid audiofile) */
  {
    if(gpp->audio_status == GAP_PLAYER_MAIN_AUSTAT_UNCHECKED)
    {
      p_audio_startup_server(gpp);
      if(gpp->audio_enable != TRUE)
      {
        return;
      }
      gpp->audio_status = GAP_PLAYER_MAIN_AUSTAT_NONE;
    }
    if(gpp->audio_status <= GAP_PLAYER_MAIN_AUSTAT_NONE)
    {
      if ( apcl_start(p_audio_errfunc) < 0 )
      {
         g_message(_("Failure to start the wavplay server is fatal.\n"
                "Please check the executability of the 'wavplay' command.\n"
                "If you have installed the wavplay executeable somewhere\n"
                "you can set the Environmentvariable WAVPLAYPATH before gimp startup\n"));
      }
      else
      {
        gpp->audio_status = GAP_PLAYER_MAIN_AUSTAT_SERVER_STARTED;
      }
      /* apcl_semreset(0,p_audio_errfunc); */  /* Tell server to reset semaphores */
    }
    else
    {
      p_audio_stop(gpp);
    }

    if(gpp->audio_status < GAP_PLAYER_MAIN_AUSTAT_FILENAME_SET)
    {
      apcl_path(gpp->audio_filename,0,p_audio_errfunc);  /* Tell server the new path */
      gpp->audio_status = GAP_PLAYER_MAIN_AUSTAT_FILENAME_SET;
    }
  }
#endif
  return;
}  /* end p_audio_init */


/* -----------------------------
 * p_audio_print_labels
 * -----------------------------
 */
static void
p_audio_print_labels(GapPlayerMainGlobalParams *gpp)
{
  char  txt_buf[100];
  gint  len;
  gint32 l_samplerate;
  gint32 l_samples;
  gint32 l_videoframes;

  l_samples = gpp->audio_samples;
  l_samplerate = gpp->audio_samplerate;

  if (gap_debug)
  {
    printf("p_audio_print_labels\n");
    printf("audio_filename: %s\n", gpp->audio_filename);
    printf("audio_enable: %d\n", (int)gpp->audio_enable);
    printf("audio_frame_offset: %d\n", (int)gpp->audio_frame_offset);
    printf("audio_bits: %d\n", (int)gpp->audio_bits);
    printf("audio_channels: %d\n", (int)gpp->audio_channels);
    printf("audio_samplerate: %d\n", (int)gpp->audio_samplerate);
    printf("audio_samples: %d\n", (int)gpp->audio_samples);
    printf("audio_status: %d\n", (int)gpp->audio_status);
  }
  if(gpp->audio_frame_offset < 0)
  {
    gap_timeconv_framenr_to_timestr( (gint32)(0 - gpp->audio_frame_offset)
                           , (gdouble)gpp->original_speed
                           , txt_buf
                           , sizeof(txt_buf)
                           );
    len = strlen(txt_buf);
    g_snprintf(&txt_buf[len], sizeof(txt_buf)-len, " %s", _("Audio Delay"));
  }
  else
  {
    gap_timeconv_framenr_to_timestr( (gint32)(gpp->audio_frame_offset)
                           , (gdouble)gpp->original_speed
                           , txt_buf
                           , sizeof(txt_buf)
                           );
    len = strlen(txt_buf);
    if(gpp->audio_frame_offset == 0)
    {
      g_snprintf(&txt_buf[len], sizeof(txt_buf)-len, " %s", _("Syncron"));
    }
    else
    {
      g_snprintf(&txt_buf[len], sizeof(txt_buf)-len, " %s", _("Audio Skipped"));
    }
  }
  gtk_label_set_text ( GTK_LABEL(gpp->audio_offset_time_label), txt_buf);

  gap_timeconv_samples_to_timestr( l_samples
                           , (gdouble)l_samplerate
                           , txt_buf
                           , sizeof(txt_buf)
                           );
  gtk_label_set_text ( GTK_LABEL(gpp->audio_total_time_label), txt_buf);

  g_snprintf(txt_buf, sizeof(txt_buf), _("%d (at %.4f frames/sec)")
             , (int)gap_timeconv_samples_to_frames(l_samples
                                            ,(gdouble)l_samplerate
                                            ,(gdouble)gpp->original_speed    /* framerate */
                                            )
             , (float)gpp->original_speed
             );
  gtk_label_set_text ( GTK_LABEL(gpp->audio_total_frames_label), txt_buf);

  g_snprintf(txt_buf, sizeof(txt_buf), "%d", (int)l_samples );
  gtk_label_set_text ( GTK_LABEL(gpp->audio_samples_label), txt_buf);

  g_snprintf(txt_buf, sizeof(txt_buf), "%d", (int)l_samplerate );
  gtk_label_set_text ( GTK_LABEL(gpp->audio_samplerate_label), txt_buf);

  g_snprintf(txt_buf, sizeof(txt_buf), "%d", (int)gpp->audio_bits );
  gtk_label_set_text ( GTK_LABEL(gpp->audio_bits_label), txt_buf);

  g_snprintf(txt_buf, sizeof(txt_buf), "%d", (int)gpp->audio_channels );
  gtk_label_set_text ( GTK_LABEL(gpp->audio_channels_label), txt_buf);

  l_videoframes = 0;
  if(gpp->ainfo_ptr)
  {
    l_videoframes = 1+ (gpp->ainfo_ptr->last_frame_nr - gpp->ainfo_ptr->first_frame_nr);
  }
  gap_timeconv_framenr_to_timestr( l_videoframes
                         , gpp->original_speed
                         , txt_buf
                         , sizeof(txt_buf)
                         );
  gtk_label_set_text ( GTK_LABEL(gpp->video_total_time_label), txt_buf);

  g_snprintf(txt_buf, sizeof(txt_buf), "%d", (int)l_videoframes);
  gtk_label_set_text ( GTK_LABEL(gpp->video_total_frames_label), txt_buf);

}  /* end p_audio_print_labels */


/* -----------------------------
 * p_print_and_clear_audiolabels
 * -----------------------------
 */
static void
p_print_and_clear_audiolabels(GapPlayerMainGlobalParams *gpp)
{
  gpp->audio_samplerate = 0;
  gpp->audio_bits       = 0;
  gpp->audio_channels   = 0;
  gpp->audio_samples    = 0;
  p_audio_print_labels(gpp);
}  /* end p_print_and_clear_audiolabels */

/* -----------------------------
 * p_audio_filename_changed
 * -----------------------------
 * check if audiofile is a valid wavefile,
 * and tell audioserver to prepare if its OK
 * (but dont start to play, just keep audioserver stand by)
 */
static void
p_audio_filename_changed(GapPlayerMainGlobalParams *gpp)
{
#ifdef GAP_ENABLE_AUDIO_SUPPORT
  int rc;
  long samplerate;                   /* Sampling rate in Hz */
  long channels;                     /* Channels recorded in this wav file */
  long bytes_per_sample;             /* data bit size (8/12/16) */
  long sample_bits;                  /* data bit size of one sample value (8/12/16) */
  long samples;                      /* The number of samples in this file */

  if (gap_debug)
  {
    printf("p_audio_filename_changed to:%s:\n", gpp->audio_filename);
  }
  p_audio_stop(gpp);
  gpp->audio_status = MIN(gpp->audio_status, GAP_PLAYER_MAIN_AUSTAT_SERVER_STARTED);

  rc = gap_audio_wav_file_check(gpp->audio_filename, &samplerate, &channels
                           , &bytes_per_sample, &sample_bits, &samples);

  if(rc != 0)
  {
     g_message (_("Error at reading WAV header from file '%s'")
               ,  gpp->audio_filename);
     p_print_and_clear_audiolabels(gpp);
     return;
  }

  gpp->audio_samplerate = samplerate;
  gpp->audio_bits       = sample_bits;
  gpp->audio_channels   = channels;
  gpp->audio_samples    = samples;

  if (gap_debug)
  {
    printf("p_audio_filename_changed :%s:\n", gpp->audio_filename);
    printf("    audio_samplerate:%d:\n", gpp->audio_samplerate);
    printf("    audio_bits:%d:\n", gpp->audio_bits);
    printf("    audio_channels:%d:\n", gpp->audio_channels);
    printf("    audio_samples:%d:\n", gpp->audio_samples);
  }
  p_audio_print_labels(gpp);

  /* p_audio_init(gpp) must not be called here.
   *  becuase it calls p_audio_startup_server and  this will recursive call
   *  p_audio_filename_changed in an endless loop....
   * For now the audio_init is done later on 1st attempt to start playback.
   */

#endif
  return;
}  /* end p_audio_filename_changed */


/* ---------------------------------
 * p_get_audio_relevant_FrameNr
 * ---------------------------------
 * get relevant framenumber respecting offsets
 * local to the referenced video clip.
 * (if playback currently does not refer to a vide
 * return framenr unchanged)
 */
static gint32
p_get_audio_relevant_FrameNr(GapPlayerMainGlobalParams *gpp, gint32 framenr)
{
  GapStoryLocateRet *stb_ret;
  gint32 localFrameNr;
  
  /* prepare standard behaviour 
   *
   */
  localFrameNr = framenr - gpp->ainfo_ptr->first_frame_nr;

  if(gpp->stb_ptr)
  {
    stb_ret = gap_story_locate_framenr(gpp->stb_ptr
                                  , framenr
                                  , gpp->stb_in_track
                                  );
    if(stb_ret)
    {
      if((stb_ret->locate_ok)
      && (stb_ret->stb_elem))
      {
        /* playback of a videoclip via storyboard clip */
        if(stb_ret->stb_elem->record_type == GAP_STBREC_VID_MOVIE)
        {
          localFrameNr = stb_ret->ret_framenr;
        }
      }
    }
  }
  else
  {
    if(gpp->ainfo_ptr->ainfo_type == GAP_AINFO_MOVIE)
    {
      /* playback of a single videoclip */
      localFrameNr = framenr;
    }
  }

  if(gap_debug)
  {
    printf("p_get_audio_relevant_FrameNr: framenr:%d localFrameNr:%d\n"
      ,framenr
      ,localFrameNr
      );
  }

  return (localFrameNr);
  
}  /* end p_get_audio_relevant_FrameNr */

/* -----------------------------------------
 * p_get_default_tmp_otone_audiofilename
 * -----------------------------------------
 * returns a default name for the temporary otone audiofile.
 * (The caller must NOT g_free the returned string after usage).
 */
const char*
p_get_default_tmp_otone_audiofilename()
{
  static const char *name = "tmp_gap_extracted_original_audiotrack.wav";
  return (name);
}  /* end p_get_default_tmp_otone_audiofilename */


/* -----------------------------------------
 * p_build_otone_audiofilename
 * -----------------------------------------
 * on writeable directory use videofilename + audiotrack + ".wav" extension
 * otherwise use a fixed name for the audiofile.
 * (The caller shall g_free the returned string after usage).
 */
char *
p_build_otone_audiofilename(const char *videofilename, gint32 atrack, const char *suffix)
{
  char *l_audiofilename;
  FILE *l_fp;
  
  if (suffix == NULL)
  {
    l_audiofilename = g_strdup_printf("%s.%d.wav", videofilename, atrack);
  }
  else
  {
    l_audiofilename = g_strdup_printf("%s%s.%d.wav", videofilename, suffix, atrack);
  }
  
  if(g_file_test(l_audiofilename, G_FILE_TEST_EXISTS))
  {
    return(l_audiofilename);
  }
  
  
  /* test creation of l_audiofilename is possible */
  l_fp = fopen(l_audiofilename, "w");
  if (l_fp)
  {
     fclose(l_fp);
     /* file creation OK (we have write permission)
      * but immediate remove the file.
      */
     g_remove(l_audiofilename);
     return(l_audiofilename);
  }

  l_audiofilename = g_strdup(p_get_default_tmp_otone_audiofilename());
  return(l_audiofilename);  
  
}  /* end p_build_otone_audiofilename */



/* -----------------------------------------
 * p_check_otone_workfile_up_to_date
 * -----------------------------------------
 * TODO: additional check if audiofile is complete
 *       (in case extract was cancelled before the full length of the audiotrack could be written)
 */
gboolean
p_check_otone_workfile_up_to_date(const char *videofilename, const char *audiofilename,
  gint32 l_extract_audiotrack)
{
  char *l_audiofilename;

  if(!g_file_test(audiofilename, G_FILE_TEST_EXISTS))
  {
    return(FALSE);
  }
  if(!g_file_test(videofilename, G_FILE_TEST_EXISTS))
  {
    return(FALSE);
  }
  
  l_audiofilename = p_build_otone_audiofilename(videofilename
                               , l_extract_audiotrack
                               , NULL  /* no suffix used */
                               );
  if (strcmp(l_audiofilename, audiofilename) != 0)
  {
    return(FALSE);
  }

  if (strcmp(l_audiofilename, p_get_default_tmp_otone_audiofilename()) == 0)
  {
    return(FALSE);
  }

  if (gap_file_get_mtime(l_audiofilename) < gap_file_get_mtime(videofilename))
  {
    return(FALSE);
  }

  return (TRUE);

}  /* end p_check_otone_workfile_up_to_date */



/* -----------------------------
 * p_audio_start_play
 * -----------------------------
 * conditional start audioplayback
 * note: if already playing this procedure does noting.
 *       (first call p_audio_stop to stop the old playing audiofile
 *        before you start another one)
 */
static void
p_audio_start_play(GapPlayerMainGlobalParams *gpp)
{
#ifdef GAP_ENABLE_AUDIO_SUPPORT
  gdouble offset_start_sec;
  gint32  offset_start_samples;
  gdouble flt_samplerate;
  gint32 l_samplerate;
  gint32 l_samples;
  gdouble l_referedClipSpeed;
    

  /* if (gap_debug) printf("p_audio_start_play\n"); */

  /* Never play audio when disabled, or video play backwards,
   * (reverse audio is not supported by the wavplay server)
   */
  if(!gpp->audio_enable)                        { return; }
  if(gpp->audio_status >= GAP_PLAYER_MAIN_AUSTAT_PLAYING)       { return; }
  if(gpp->play_backward)                        { return; }
  if(gpp->ainfo_ptr == NULL)                    { return; }

  l_samples = gpp->audio_samples;
  l_samplerate = gpp->audio_samplerate;
  l_referedClipSpeed = gpp->original_speed;

  if (gpp->audio_auto_offset_by_framenr == TRUE)
  {
    gint32 localFramenr;
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
    if (gpp->gvahand)
    {
      l_referedClipSpeed = gpp->gvahand->framerate;
      l_samplerate = gpp->gvahand->samplerate;
    }
#endif
    
    localFramenr = p_get_audio_relevant_FrameNr(gpp, gpp->play_current_framenr);
    
    offset_start_sec = ( localFramenr
                     + gpp->audio_frame_offset
                     ) / MAX(1, l_referedClipSpeed);
  }
  else
  {
    offset_start_sec = ( gpp->play_current_framenr
                     - gpp->ainfo_ptr->first_frame_nr
                     + gpp->audio_frame_offset
                     ) / MAX(1, gpp->original_speed);
  }

  offset_start_samples = offset_start_sec * l_samplerate;
  
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  if(gap_debug)
  {
    printf("p_audio_start_play  original_speed:%.3f refSpeed:%.3f audio_frame_offset:%d l_samples:%d l_samplerate:%d (gvahand:%d) offset_start_samples:%d\n\n"
      ,gpp->original_speed
      ,l_referedClipSpeed
      ,gpp->audio_frame_offset
      ,l_samples
      ,l_samplerate
      ,gpp->gvahand->samplerate
      ,offset_start_samples
      );
  }
#endif
  
  
  if(l_referedClipSpeed > 0)
  {
    /* use moidfied samplerate if video is not played at original speed
     */
    flt_samplerate = l_samplerate * gpp->speed / l_referedClipSpeed;
  }
  else
  {
    flt_samplerate = l_samplerate;
  }

  /* check if offset and rate is within playable limits */
  if((offset_start_samples >= 0)
  && ((gdouble)offset_start_samples < ((gdouble)l_samples - 1024.0))
  && (flt_samplerate >= GAP_PLAYER_MAIN_MIN_SAMPLERATE)
  )
  {
    guint32  lu_samplerate;

    p_audio_init(gpp);  /* tell ausioserver to go standby for this audiofile */
    gpp->audio_required_samplerate = (guint32)flt_samplerate;
    if(flt_samplerate > GAP_PLAYER_MAIN_MAX_SAMPLERATE)
    {
      lu_samplerate = (guint32)GAP_PLAYER_MAIN_MAX_SAMPLERATE;
      /* required samplerate is faster than highest possible audioplayback speed
       * (the audioplayback will be played but runs out of sync and cant follow)
       */
    }
    else
    {
      lu_samplerate = (guint32)flt_samplerate;
    }
    apcl_sampling_rate(lu_samplerate,0,p_audio_errfunc);
    apcl_start_sample((guint32)offset_start_samples,0,p_audio_errfunc);
    apcl_play(0,p_audio_errfunc);  /* Tell server to play */
    apcl_volume(gpp->audio_volume, 0, p_audio_errfunc);

    gpp->audio_status = GAP_PLAYER_MAIN_AUSTAT_PLAYING;
    gpp->audio_resync = 0;
  }
#endif
  return;
}  /* end p_audio_start_play */


/* -----------------------------
 * p_audio_startup_server
 * -----------------------------
 * In case compiled with (older) wavplay client/server
 * implementation, this method checks if the wavplay server
 * is available.
 *
 * In case newer audiosupport  is availabel just enable the relevant GUI widgets
 *
 * In case audiosupport is NOT available 
 *
 */
static void
p_audio_startup_server(GapPlayerMainGlobalParams *gpp)
{
  gboolean l_audio_enable = FALSE;
  
    
#ifdef GAP_ENABLE_AUDIO_SUPPORT_WAVPLAY
  const char *cp;
  gboolean wavplay_server_found;

  if (gap_debug)
  {
    printf("p_audio_startup_server\n");
  }

  wavplay_server_found = FALSE;

  /*
   * Set environmental values for the wavplay audio server:
   */

  /* check gimprc for the wavplay audio server: */
  if ( (cp = gimp_gimprc_query("wavplay_prog")) != NULL )
  {
    env_WAVPLAYPATH = g_strdup(cp);             /* Environment overrides compiled in default for WAVPLAYPATH */
    if(g_file_test (env_WAVPLAYPATH, G_FILE_TEST_IS_EXECUTABLE) )
    {
      wavplay_server_found = TRUE;
    }
    else
    {
      g_message(_("WARNING: your gimprc file configuration for the wavplay audio server\n"
             "does not point to an executable program\n"
             "the configured value for %s is: %s\n")
             , "wavplay_prog"
             , env_WAVPLAYPATH
             );
    }
  }

  /* check environment variable for the wavplay audio server: */
  if(!wavplay_server_found)
  {
    if ( (cp = g_getenv("WAVPLAYPATH")) != NULL )
    {
      env_WAVPLAYPATH = g_strdup(cp);           /* Environment overrides compiled in default for WAVPLAYPATH */
      if(g_file_test (env_WAVPLAYPATH, G_FILE_TEST_IS_EXECUTABLE) )
      {
        wavplay_server_found = TRUE;
      }
      else
      {
        g_message(_("WARNING: the environment variable %s\n"
               "does not point to an executable program\n"
               "the current value is: %s\n")
               , "WAVPLAYPATH"
               , env_WAVPLAYPATH
               );
      }
    }
  }

  if(!wavplay_server_found)
  {
    if ( (cp = g_getenv("PATH")) != NULL )
    {
      env_WAVPLAYPATH = gap_lib_searchpath_for_exefile("wavplay", cp);
      if(env_WAVPLAYPATH)
      {
        wavplay_server_found = TRUE;
      }
    }
  }

  if ( (cp = g_getenv("AUDIODEV")) != NULL )
          env_AUDIODEV = g_strdup(cp);          /* Environment overrides compiled in default for AUDIODEV */

  if ( (cp = g_getenv("AUDIOLCK")) == NULL || sscanf(cp,"%lX",&env_AUDIOLCK) != 1 )
          env_AUDIOLCK = AUDIOLCK;      /* Use compiled in default, if no environment, or its bad */

  if(wavplay_server_found)
  {
    p_audio_filename_changed(gpp);
    l_audio_enable = TRUE;
  }
  else
  {
    l_audio_enable = FALSE;
    g_message(_("No audiosupport available\n"
                 "the audioserver executable file '%s' was not found.\n"
                 "If you have installed '%s'\n"
                 "you should add the installation dir to your PATH\n"
                 "or set environment variable %s to the name of the executable\n"
                 "before you start GIMP")
                 , "wavplay"
                 , "wavplay 1.4"
                 , "WAVPLAYPATH"
                 );
  }
#else
#ifdef GAP_ENABLE_AUDIO_SUPPORT
    p_audio_filename_changed(gpp);
    l_audio_enable = TRUE;
#endif

#endif

  gpp->audio_enable = l_audio_enable;
  return;
}  /* end p_audio_startup_server */


/* ------------------------------
 * p_mtrace_image_alive
 * ------------------------------
 * Check if the mtrace_image_id is valid (== belongs to an existing image)
 * if INVALID (there was no mlayer image or it was closed by the user) then
 *    create a new mtrace_image
 * if valid then deliver width and height
 *
 * OUT: *mtrace_width *mtrace_height  .. the size of the mtrace image
 */
static void
p_mtrace_image_alive(GapPlayerMainGlobalParams *gpp
                    , gint32 width
                    , gint32 height
                    , gint32 *mtrace_width
                    , gint32 *mtrace_height
                    )
{
  GimpImageBaseType  image_type;

  image_type = GIMP_RGB;
  if(gpp->image_id >= 0)
  {
    image_type = gimp_image_base_type(gpp->image_id);
  }

  if(gap_image_is_alive(gpp->mtrace_image_id))
  {
    *mtrace_width =  gimp_image_width(gpp->mtrace_image_id);
    *mtrace_height = gimp_image_height(gpp->mtrace_image_id);
  }
  else
  {
    if(gpp->mtrace_mode == GAP_PLAYER_MTRACE_IMG_SIZE)
    {
      *mtrace_width = width;
      *mtrace_height = height;
    }
    else
    {
      *mtrace_width = gpp->pv_width;
      *mtrace_height = gpp->pv_height;
    }

    gpp->mtrace_image_id = gimp_image_new( *mtrace_width
                                         , *mtrace_height
                                         , image_type
                                         );

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
    /* if there is an active videohandle
     * set image resolution according to
     * aspect_ratio of the videofile
     */
    if(gpp->gvahand)
    {
      GVA_image_set_aspect(gpp->gvahand, gpp->mtrace_image_id);
    }
#endif
    gimp_display_new(gpp->mtrace_image_id);
  }
}  /* end p_mtrace_image_alive */


/* ------------------------------
 * p_conditional_detail_tracking
 * ------------------------------
 * optional call detail  tracking.
 * that logs coordinates of a detail (typically marked via 2 path vector points)
 * as XML input file for the MovePath feature.
 */
static void
p_conditional_detail_tracking(GapPlayerMainGlobalParams *gpp, gint32 image_id)
{
  if(gpp->enableDetailTracking)
  {
    gap_track_detail_on_top_layers_lastvals(image_id);
    /* flush display to force rendering of newly added layers and vectors */
    gimp_displays_flush ();
  }
}  /* end p_conditional_detail_tracking */


/* ------------------------------
 * p_conditional_attach_dvref
 * ------------------------------
 * check if there is a valid current videoreference.
 * if yes attach the reference as (temporary) videoreference parasite
 * to the specified drawable_id 
 * (that is typically the layer in the mtrace image)
 */
static void
p_conditional_attach_dvref(GapPlayerMainGlobalParams *gpp
               ,gint32 drawable_id)
{
  if(gap_debug)
  {
    printf("MTRACE: mtrace_mode:%d (GAP_PLAYER_MTRACE_IMG_SIZE == %d) dvref\n"
        ,(int)gpp->mtrace_mode
        ,(int)GAP_PLAYER_MTRACE_IMG_SIZE
        );
    gap_dvref_debug_print_GapDrawableVideoRef(gpp->dvref_ptr);
  }

  if((gpp->mtrace_mode == GAP_PLAYER_MTRACE_IMG_SIZE)
  && (gpp->dvref_ptr != NULL))
  {
    if(gpp->dvref_ptr->videofile != NULL)
    {
      if (gpp->dvref_ptr->videofile == gpp->gva_videofile)
      {
        gap_dvref_assign_videoref_parasites(gpp->dvref_ptr, drawable_id);
      }
    }
  } 
}  /* end p_conditional_attach_dvref */

/* ------------------------------
 * p_mtrace_image
 * ------------------------------
 * add image as one composite layer
 * to the mtrace image.
 * IMPORTANT: all visible layers of the input image are merged
 */
static void
p_mtrace_image( GapPlayerMainGlobalParams *gpp
               ,gint32 image_id
              )
{
  gint32 width;
  gint32 height;
  gint32 mtrace_width;
  gint32 mtrace_height;
  gint32 src_layer_id;
  gint32 dst_layer_id;
  gint   l_src_offset_x, l_src_offset_y;    /* layeroffsets as they were in src_image */

  if(gap_debug)
  {
      printf("MTRACE:p_mtrace_image START mtrace_mode:%d (GAP_PLAYER_MTRACE_IMG_SIZE == %d)\n"
          ,(int)gpp->mtrace_mode
          ,(int)GAP_PLAYER_MTRACE_IMG_SIZE
          );
      gap_dvref_debug_print_GapDrawableVideoRef(gpp->dvref_ptr);
  }

  if(gpp->mtrace_mode != GAP_PLAYER_MTRACE_OFF)
  {
    width = MAX(gimp_image_width(image_id), 1);
    height = MAX(gimp_image_height(image_id), 1);

    p_mtrace_image_alive(gpp
                        ,width
                        ,height
                        , &mtrace_width
                        , &mtrace_height
                        );

    src_layer_id = gap_image_merge_visible_layers(image_id, GIMP_CLIP_TO_IMAGE);
    dst_layer_id = gap_layer_copy_to_dest_image(gpp->mtrace_image_id     /* the dest image */
                                   , src_layer_id        /* the layer to copy */
                                   , 100.0               /* opacity */
                                   , 0                   /* NORMAL paintmode */
                                   ,&l_src_offset_x
                                   ,&l_src_offset_y);

    gimp_image_insert_layer(gpp->mtrace_image_id
                        , dst_layer_id
                        , 0
                        , 0              /* top of layerstack */
                        );
    if((width != mtrace_width)
    || (height != mtrace_height))
    {
      gimp_layer_scale(dst_layer_id, mtrace_width, mtrace_height, 0);
      l_src_offset_x = (gint32)(((gdouble)mtrace_width / (gdouble)width) * (gdouble)l_src_offset_x);
      l_src_offset_y = (gint32)(((gdouble)mtrace_height / (gdouble)height) * (gdouble)l_src_offset_y);
    }
    gimp_layer_set_offsets(dst_layer_id, l_src_offset_x, l_src_offset_y);
    p_conditional_attach_dvref(gpp, dst_layer_id);


    {
      gchar *l_name;

      l_name = g_strdup_printf("frame_%06d (%dms)"
                          ,(int)gpp->play_current_framenr
                          ,(int)(1000 / gpp->speed)
                          );
      gimp_drawable_set_name(dst_layer_id, l_name);
      g_free(l_name);
    }

    p_conditional_detail_tracking(gpp, gpp->mtrace_image_id);

    gimp_displays_flush();
  }
}  /* end p_mtrace_image */


/* ------------------------------
 * p_mtrace_tmpbuf
 * ------------------------------
 * add buffer as one composite layer
 * to the mtrace image.
 */
static void
p_mtrace_tmpbuf( GapPlayerMainGlobalParams *gpp
               , guchar *th_data
               , gint32 th_width
               , gint32 th_height
               , gint32 th_bpp
              )
{
  gint32 mtrace_width;
  gint32 mtrace_height;
  gint32 dst_layer_id;

  if(gpp->mtrace_mode != GAP_PLAYER_MTRACE_OFF)
  {
    p_mtrace_image_alive(gpp
                        ,th_width
                        ,th_height
                        , &mtrace_width
                        , &mtrace_height
                        );
    dst_layer_id = gap_layer_new_from_buffer(gpp->mtrace_image_id
                                 , th_width
                                 , th_height
                                 , th_bpp
                                 , th_data
                                 );
    gimp_image_insert_layer(gpp->mtrace_image_id
                        , dst_layer_id
                        , 0
                        , 0              /* top of layerstack */
                        );
    if(th_bpp == 3)
    {
      gimp_layer_add_alpha(dst_layer_id);
    }

    if((th_width != mtrace_width)
    || (th_height != mtrace_height))
    {
      gimp_layer_scale(dst_layer_id, mtrace_width, mtrace_height, 0);
    }
    gimp_layer_set_offsets(dst_layer_id
                          , 0  /* offset_x */
                          , 0  /* offset_y */
                          );

    p_conditional_attach_dvref(gpp, dst_layer_id);

    {
      gchar *l_name;

      l_name = g_strdup_printf("thumb_%06d (%dms)"
                          ,(int)gpp->play_current_framenr
                          ,(int)(1000 / gpp->speed)
                          );
      gimp_drawable_set_name(dst_layer_id, l_name);
      g_free(l_name);
    }

    p_conditional_detail_tracking(gpp, gpp->mtrace_image_id);

    gimp_displays_flush();
  }
}  /* end p_mtrace_tmpbuf */


/* ------------------------------
 * p_mtrace_pixbuf
 * ------------------------------
 * add buffer as one composite layer
 * to the mtrace image.
 */
static void
p_mtrace_pixbuf( GapPlayerMainGlobalParams *gpp
               , GdkPixbuf *pixbuf
              )
{
  if(gpp->mtrace_mode != GAP_PLAYER_MTRACE_OFF)
  {
    if(pixbuf)
    {
      gint32 nchannels;
      gint32 rowstride;
      gint32 width;
      gint32 height;
      guchar *pix_data;

      width = gdk_pixbuf_get_width(pixbuf);
      height = gdk_pixbuf_get_height(pixbuf);
      nchannels = gdk_pixbuf_get_n_channels(pixbuf);
      pix_data = gdk_pixbuf_get_pixels(pixbuf);
      rowstride = gdk_pixbuf_get_rowstride(pixbuf);

      p_mtrace_tmpbuf(gpp
                   , pix_data
                   , width
                   , height
                   , nchannels
                   );

    }
  }
}  /* end p_mtrace_pixbuf */


/* -----------------------------
 * p_printout_range
 * -----------------------------
 * call fptr_set_range
 * if such a callback function for setting the framerange is available (!= NULL)
 * typically for the case where the player is called from storyboard dialog.
 *
 * Another optional functionality
 * prints the selected RANGE to stdout
 * (Line Formated for STORYBOARD_FILE processing)
 *
 * if the Player is in Storyboard mode
 * the printout is just a comment with range numbers
 */
static void
p_printout_range(GapPlayerMainGlobalParams *gpp, gboolean inverse, gboolean printflag)
{
  gint32 l_begin;
  gint32 l_end;

  l_begin = gpp->begin_frame;
  l_end   = gpp->end_frame;
  if(inverse)
  {
    l_end   = gpp->begin_frame;
    l_begin = gpp->end_frame;
  }

  if(gpp->ainfo_ptr == NULL) { return; }
  if(gpp->ainfo_ptr->basename == NULL) { return; }

  if(gpp->stb_ptr)
  {
    if(printflag)
    {
      printf("# storyboard  %06d=frame_from %06d=frame_to  normal  1=nloops\n"
                     , (int)l_begin
                     , (int)l_end
                     );
    }
    return;
  }

  if(printflag)
  {
    if(gpp->ainfo_ptr->ainfo_type == GAP_AINFO_MOVIE)
    {
      /* Storyboard command line for playback of GAP singleframe range */
      printf("VID_PLAY_MOVIE      1=track  \"%s\" %06d=frame_from %06d=frame_to  normal  1=nloops %d=seltrack\n"
                         , gpp->ainfo_ptr->old_filename
                         , (int)l_begin
                         , (int)l_end
                         , (int)gpp->ainfo_ptr->seltrack
                         );
    }
    else
    {
      /* Storyboard command line for playback of GAP singleframe range */
      printf("VID_PLAY_FRAMES     1=track  \"%s\" %s %06d=frame_from %06d=frame_to  normal  1=nloops\n"
                         , gpp->ainfo_ptr->basename
                         , &gpp->ainfo_ptr->extension[1]
                         , (int)l_begin
                         , (int)l_end
                         );
    }
  }

  /* callback function for adding range as cliplist */
  if(gpp->fptr_set_range)
  {
    GapPlayerAddClip *plac_ptr;

    plac_ptr = g_new(GapPlayerAddClip, 1);
    if(plac_ptr)
    {
      plac_ptr->user_data_ptr = gpp->user_data_ptr;
      plac_ptr->ainfo_ptr     = gpp->ainfo_ptr;
      plac_ptr->range_from    = l_begin;
      plac_ptr->range_to      = l_end;
      (*gpp->fptr_set_range)(plac_ptr);

      g_free(plac_ptr);
    }
  }

}  /* end p_printout_range */


/* --------------------------------
 * p_update_ainfo_for_videofile
 * --------------------------------
 * typical called after creation of a videoindex
 * to update the total_frames information
 * (that may have changed from the guess value to
 * an exact value when all frames were counted)
 */
static void
p_update_ainfo_for_videofile(GapPlayerMainGlobalParams *gpp)
{
  GapAnimInfo   *ainfo_ptr;

  if(gpp == NULL)
  {
    return;
  }

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  if(gap_debug)
  {
    printf("p_update_ainfo_for_videofile  gpp->ainfo_ptr:%d gpp->gvahand:%d\n"
      , (int)gpp->ainfo_ptr
      , (int)gpp->gvahand
      );
  }

  ainfo_ptr = gpp->ainfo_ptr;
  if ((ainfo_ptr == NULL) || (gpp->gvahand == NULL))
  {
    return;
  }


  if(ainfo_ptr->ainfo_type == GAP_AINFO_MOVIE)
  {
    if(gpp->gvahand->all_frames_counted)
    {
      if(gap_debug)
      {
        printf("p_update_ainfo_for_videofile  ainfo_ptr->frame_cnt:%d gpp->gvahand->total_frames:%d\n"
          , (int)ainfo_ptr->frame_cnt
          , (int)gpp->gvahand->total_frames
          );
      }
      ainfo_ptr->frame_cnt = gpp->gvahand->total_frames;
      ainfo_ptr->last_frame_nr = gpp->gvahand->total_frames;
    }
   }
#endif

}  /* end p_update_ainfo_for_videofile*/


/* --------------------------------
 * p_alloc_ainfo_for_videofile
 * --------------------------------
 * get anim information from filename for videofiles
 * return NULL if filename is not a supported videofile
 * if filename is a supported video
 *    return anim information
 *    and keep the used videohandle gpp->gvahand open
 *    for the expected read access.
 */
static GapAnimInfo *
p_alloc_ainfo_for_videofile(GapPlayerMainGlobalParams *gpp
                                , char *filename
                                , gint32 seltrack
                                , gdouble delace
                                , const char *preferred_decoder
                                )
{
  GapAnimInfo   *l_ainfo_ptr;

  l_ainfo_ptr = NULL;

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  if(filename == NULL)
  {
    return (NULL);
  }

  if(!g_file_test(filename, G_FILE_TEST_EXISTS))
  {
    if(gap_debug)
    {
      printf ("p_alloc_ainfo_for_videofile: file %s does not exist\n"
             , filename
              );
    }
    return (NULL);
  }

  if(!gap_story_filename_is_videofile_by_ext(filename))
  {
    if(gap_debug)
    {
      printf ("p_alloc_ainfo_for_videofile: file %s has no typical video extension\n"
             , filename
              );
    }
    if(!gpp->force_open_as_video)
    {
      return (NULL);
    }
  }

  gpp->gva_lock = TRUE;
  p_close_videofile(gpp);
  p_open_videofile(gpp, filename, seltrack, delace, preferred_decoder);
  gpp->gva_lock = FALSE;
  if(gpp->gvahand == NULL)
  {
    if(gap_debug)
    {
      printf ("p_alloc_ainfo_for_videofile: file %s NO compatible Decoder available\n"
             , filename
              );
    }
    return (NULL);
  }


  l_ainfo_ptr = (GapAnimInfo*)g_malloc(sizeof(GapAnimInfo));
  if(l_ainfo_ptr == NULL) return(NULL);

  l_ainfo_ptr->basename = g_strdup(filename);
  l_ainfo_ptr->extension = gap_lib_alloc_extension(filename);

  l_ainfo_ptr->new_filename = NULL;
  l_ainfo_ptr->image_id = -1;
  l_ainfo_ptr->old_filename = g_strdup(filename);
  l_ainfo_ptr->ainfo_type = GAP_AINFO_MOVIE;
  l_ainfo_ptr->seltrack = seltrack;
  l_ainfo_ptr->delace = delace;

  l_ainfo_ptr->curr_frame_nr = 1;
  l_ainfo_ptr->first_frame_nr = 1;
  l_ainfo_ptr->frame_cnt = gpp->gvahand->total_frames;
  if(!gpp->gvahand->all_frames_counted)
  {
    /* frames are not counted yet,
     * and the total_frames information is just a guess
     * in this case we assume 2 times more frames
     */
    l_ainfo_ptr->frame_cnt *= 2;
  }
  l_ainfo_ptr->last_frame_nr = l_ainfo_ptr->frame_cnt;
  l_ainfo_ptr->width  = gpp->gvahand->width;
  l_ainfo_ptr->height = gpp->gvahand->height;
  gpp->original_speed = gpp->gvahand->framerate;

#endif
  return(l_ainfo_ptr);

}  /* end p_alloc_ainfo_for_videofile */



/* -----------------------------
 * p_reload_ainfo_ptr
 * -----------------------------
 * get anim information from imagename or image_id
 * NO operation when Player is in storyboard mode
 * when called with a negative image_id
 * then assume that gpp->imagename refers to a videofile
 * and try to allocate anim info from videofile.
 */
static void
p_reload_ainfo_ptr(GapPlayerMainGlobalParams *gpp, gint32 image_id)
{
  gboolean l_hasImagename;

  gpp->image_id = image_id;
  l_hasImagename = FALSE;
  
  if(gpp->stb_ptr)    { return; }
  if(gpp->ainfo_ptr)  { gap_lib_free_ainfo(&gpp->ainfo_ptr); }

  if((image_id < 0) && (gpp->imagename))
  {
    gpp->ainfo_ptr = p_alloc_ainfo_for_videofile(gpp
                                                ,gpp->imagename
                                                ,gpp->seltrack
                                                ,gpp->delace
                                                ,gpp->preferred_decoder
                                                );
    if(gpp->ainfo_ptr)
    {
      return;
    }
    gpp->ainfo_ptr = gap_lib_alloc_ainfo_from_name(gpp->imagename, gpp->run_mode);
    l_hasImagename = TRUE;
  }
  else
  {
    char  *imagename;

    imagename = gimp_image_get_filename(image_id);
    if (imagename)
    {
      l_hasImagename = TRUE;
      g_free(imagename);
    }
    /* normal mode using anim frames or single image */
    gpp->ainfo_ptr = gap_lib_alloc_ainfo(gpp->image_id, gpp->run_mode);
  }

  if(gpp->ainfo_ptr == NULL)
  {
    return;
  }

  if (0 == gap_lib_dir_ainfo(gpp->ainfo_ptr))
  {
    if(gpp->image_id >= 0)
    {
      gpp->ainfo_ptr->width  = gimp_image_width(gpp->image_id);
      gpp->ainfo_ptr->height = gimp_image_height(gpp->image_id);
    }
    else
    {
      gpp->ainfo_ptr->width  = gpp->imagewidth;
      gpp->ainfo_ptr->height = gpp->imageheight;
    }

    if((gpp->ainfo_ptr->frame_cnt != 0) && (l_hasImagename == TRUE))
    {
      GapVinVideoInfo *vin_ptr;

      vin_ptr = gap_vin_get_all(gpp->ainfo_ptr->basename);


      gpp->onion_delete = FALSE;
      gpp->original_speed = 24.0;
      if(vin_ptr)
      {
        if(vin_ptr->framerate > 0.0)
        {
          gpp->original_speed = vin_ptr->framerate;
        }

        /* check if automatic onionskin layer removal is turned on */
        if((vin_ptr->auto_delete_before_save)
        && (vin_ptr->onionskin_auto_enable))
        {
          /* yes, in this case the player must show the active image
           * without onionskin layers
           */
          gpp->onion_delete = TRUE;
        }
      }
      if(vin_ptr)
      {
        g_free(vin_ptr);
      }
    }
    else
    {
      /* player operates on a single image that is not a numbered frame
       * patch the ainfo structure
       */
      if(gap_debug)
      {
        printf("p_reload_ainfo_ptr: player operates on a single image that is not a numbered frame\n");
        printf("basename: %s\n", gpp->ainfo_ptr->basename );
        printf("extension: %s\n", gpp->ainfo_ptr->extension );
      }
      gpp->ainfo_ptr->curr_frame_nr = 1;
      gpp->ainfo_ptr->first_frame_nr = 1;
      gpp->ainfo_ptr->last_frame_nr = 1;
      gpp->original_speed = 24.0;
      
    }
  }

}  /* end p_reload_ainfo_ptr */


/* --------------------------------
 * p_update_ainfo_dependent_widgets
 * --------------------------------
 */
static void
p_update_ainfo_dependent_widgets(GapPlayerMainGlobalParams *gpp)
{
  gdouble l_lower;
  gdouble l_upper;
  gdouble l_value;

  if(gpp == NULL) { return; }
  if(gpp->ainfo_ptr == NULL) { return; }

  l_lower = (gdouble)gpp->ainfo_ptr->first_frame_nr;
  l_upper = (gdouble)gpp->ainfo_ptr->last_frame_nr;


  GTK_ADJUSTMENT(gpp->from_spinbutton_adj)->lower = l_lower;
  GTK_ADJUSTMENT(gpp->from_spinbutton_adj)->upper = l_upper;
  GTK_ADJUSTMENT(gpp->from_spinbutton_adj)->value = CLAMP(GTK_ADJUSTMENT(gpp->from_spinbutton_adj)->value
                                                         ,l_lower, l_upper);
  gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->from_spinbutton_adj)
                            , (gfloat)CLAMP(GTK_ADJUSTMENT(gpp->from_spinbutton_adj)->value, l_lower, l_upper)
                            );

  GTK_ADJUSTMENT(gpp->to_spinbutton_adj)->lower = l_lower;
  GTK_ADJUSTMENT(gpp->to_spinbutton_adj)->upper = l_upper;
  gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->to_spinbutton_adj)
                          , (gfloat) CLAMP(GTK_ADJUSTMENT(gpp->to_spinbutton_adj)->value, l_lower, l_upper)
                          );

  l_value = CLAMP(GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)->value, l_lower, l_upper);

  if(gap_debug)
  {
    printf("## tmp p_update_ainfo_dependent_widgets adj l_value:%.2f  l_lower:%.2f  l_upper:%.2f\n"
        ,(float)l_value
        ,(float)l_lower
        ,(float)l_upper
        );
  }

  GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)->lower = l_lower;
  GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)->upper = l_upper;
  gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj), l_value);

}  /* end p_update_ainfo_dependent_widgets */


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
p_find_master_image_id(GapPlayerMainGlobalParams *gpp)
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
 * Note: in storyboard mode there is no active master image
 *       just return without any action in that case.
 */
static void
p_keep_track_of_active_master_image(GapPlayerMainGlobalParams *gpp)
{
  p_stop_playback(gpp);

  if(gpp->stb_ptr)    { return; }
  if(gpp->imagename)  { return; }

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
 * on_help_button_clicked
 * -----------------------------
 */
static void
on_help_button_clicked (GtkButton       *button,
                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp)
  {
    if(gpp->help_id)
    {
      gimp_standard_help_func(gpp->help_id, gpp->shell_window);
    }
  }
}  /* end on_help_button_clicked */


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
p_check_for_active_image(GapPlayerMainGlobalParams *gpp, gint32 framenr)
{

  if (gap_image_is_alive(gpp->image_id))
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
p_update_pviewsize(GapPlayerMainGlobalParams *gpp)
{
  gint32 l_width;
  gint32 l_height;

  l_width = gpp->ainfo_ptr->width;
  l_height = gpp->ainfo_ptr->height;


  if(gpp->aspect_ratio > 0.0)
  {
    gdouble asp_height;

    /* force playback at specified aspect ratio */
    asp_height = (gdouble)(l_width) / gpp->aspect_ratio;
    l_height = (gint32)asp_height;
  }

  /*
   * Resize the greater one of dwidth and dheight to PREVIEW_SIZE
   */
  if ( l_width > l_height )
  {
    /* landscape */
    gpp->pv_height = l_height * gpp->pv_pixelsize / l_width;
    gpp->pv_height = MAX (1, gpp->pv_height);
    gpp->pv_width = gpp->pv_pixelsize;
  }
  else
  {
    /* portrait */
    gpp->pv_width = l_width * gpp->pv_pixelsize / l_height;
    gpp->pv_width = MAX(1, gpp->pv_width);
    gpp->pv_height = gpp->pv_pixelsize;
  }


  if(gpp->pv_pixelsize != (gint32)GTK_ADJUSTMENT(gpp->size_spinbutton_adj)->value)
  {
    gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->size_spinbutton_adj)
                            , (gfloat)gpp->pv_pixelsize
                            );
  }

  gpp->pv_pixelsize = (gint32)GTK_ADJUSTMENT(gpp->size_spinbutton_adj)->value;

  gap_pview_set_size(gpp->pv_ptr
                  , gpp->pv_width
                  , gpp->pv_height
                  , MAX(GAP_PLAYER_CHECK_SIZE, (gpp->pv_pixelsize / 16))
                  );

} /* end p_update_pviewsize */


/* -----------------------------
 * p_set_frame_with_name_label
 * -----------------------------
 * set labeltext as filename (or basename) with type indicator prefix
 */
static void
p_set_frame_with_name_label(GapPlayerMainGlobalParams *gpp)
{
#define MAX_CHARS 60
  char *frame_title;


  frame_title = NULL;
  if(gpp->stb_ptr)
  {
    gchar *l_prefix;

    l_prefix = NULL;
    if(gpp->stb_in_track > 0)
    {
      /* filename prefix shortcut for storyboard single track playback for specified track number */
      l_prefix = g_strdup_printf(_("STB:[%d]"), (int)gpp->stb_in_track);
    }
    else
    {
      /* filename prefix shortcut for storyboard in composite video playback mode */
      l_prefix = g_strdup(_("STB:"));
    }
    /* shortname prefix to indicate that displayed filename is from type storyboard file */
    frame_title = gap_base_shorten_filename(l_prefix    /* prefix short for storyboard */
                        ,gpp->stb_ptr->storyboardfile  /* filenamepart */
                        ,NULL                          /* suffix */
                        ,MAX_CHARS
                        );
    g_free(l_prefix);
  }
  else
  {
    if((gpp->imagename) && (gpp->ainfo_ptr))
    {
      if(gpp->ainfo_ptr->ainfo_type == GAP_AINFO_MOVIE)
      {
        /* shortname prefix to indicate that displayed filename is a single videofile */
        frame_title = gap_base_shorten_filename(_("VIDEO:")   /* prefix short for storyboard */
                        ,gpp->ainfo_ptr->basename            /* filenamepart */
                        ,NULL                                /* suffix */
                        ,MAX_CHARS
                        );
      }
    }
  }

  if(frame_title == NULL)
  {
      /* shortname prefix to indicate that displayed filename is basename of the frames */
      frame_title = gap_base_shorten_filename(_("FRAMES:")    /* prefix short for storyboard */
                        ,gpp->ainfo_ptr->basename            /* filenamepart */
                        ,NULL                                /* suffix */
                        ,MAX_CHARS
                        );
  }

  gtk_frame_set_label (GTK_FRAME (gpp->frame_with_name)
                      , frame_title);
  g_free(frame_title);
}  /* end p_set_frame_with_name_label */

/* -----------------------------
 * p_update_position_widgets
 * -----------------------------
 * set position spinbutton and time entry
 */
static void
p_update_position_widgets(GapPlayerMainGlobalParams *gpp)
{
  static gchar time_txt[12];

  if((gint32)GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)->value != gpp->play_current_framenr)
  {
    gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)
                            , (gfloat)gpp->play_current_framenr
                            );
  }
  gap_timeconv_framenr_to_timestr( (gpp->play_current_framenr - gpp->ainfo_ptr->first_frame_nr)
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
p_start_playback_timer(GapPlayerMainGlobalParams *gpp)
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
p_initial_start_playback_timer(GapPlayerMainGlobalParams *gpp)
{
  p_audio_stop(gpp);    /* stop old playback if there is any */
  gpp->audio_resync = 0;
  gpp->cycle_time_secs = 1.0 / MAX(gpp->speed, 1.0);
  gpp->rest_secs = 10.0 / 1000.0;   /* use minimal delay for the 1st call */
  gpp->delay_secs = 0.0;            /* absolute delay (for display) */
  gpp->framecnt = 0.0;
  gpp->go_job_framenr = -1;         /* pending timer_go_job gets useless, since we start playback now  */

  gtk_label_set_text ( GTK_LABEL(gpp->status_label), _("Playing"));

  g_timer_start(gpp->gtimer);  /* (re)start timer at start of playback (== reset to 0) */
  p_start_playback_timer(gpp);
  p_audio_start_play(gpp);
}  /* end p_initial_start_playback_timer */


/* -----------------------------
 * p_remove_play_timer
 * -----------------------------
 */
static void
p_remove_play_timer(GapPlayerMainGlobalParams *gpp)
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
p_stop_playback(GapPlayerMainGlobalParams *gpp)
{
  /* if(gap_debug) printf("p_stop_playback\n"); */
  p_remove_play_timer(gpp);
  gpp->mtrace_mode = GAP_PLAYER_MTRACE_OFF;
  gpp->request_cancel_video_api = TRUE;
  gpp->play_is_active = FALSE;
  gpp->pingpong_count = 0;

  gtk_label_set_text ( GTK_LABEL(gpp->status_label), _("Ready"));

  p_check_tooltips();
  p_audio_stop(gpp);
  gpp->audio_resync = 0;
}  /* end p_stop_playback */


/* ---------------------------------
 * on_cancel_vindex_button_clicked
 * ---------------------------------
 */
static void
on_cancel_vindex_button_clicked  (GtkObject       *object,
                          GapPlayerMainGlobalParams *gpp)
{
  if(gpp)
  {
    gpp->cancel_video_api = TRUE;
    gpp->request_cancel_video_api = FALSE;
  }
}  /* end on_cancel_vindex_button_clicked */


/* -----------------------------------------
 * on_audio_auto_offset_checkbutton_toggled
 * -----------------------------------------
 */
static void
on_audio_auto_offset_checkbutton_toggled    (GtkToggleButton *togglebutton,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  if (togglebutton->active)
  {
       gpp->audio_auto_offset_by_framenr = TRUE;
  }
  else
  {
       gpp->audio_auto_offset_by_framenr = FALSE;
       gpp->audio_resync = 0;
  }
  p_audio_resync(gpp);


}  /* end on_audio_auto_offset_checkbutton_toggled */

/* -----------------------------------------
 * p_msg_progress_bar_audio
 * -----------------------------------------
 */
static void
p_msg_progress_bar_audio(GapPlayerMainGlobalParams *gpp, const char *msg)
{
  if (gpp->progress_bar_audio)
  {
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(gpp->progress_bar_audio), msg);
  }
}  /* end p_msg_progress_bar_audio */

/* -----------------------------------------
 * p_reset_progress_bar_audio
 * -----------------------------------------
 */
static void
p_reset_progress_bar_audio(GapPlayerMainGlobalParams *gpp)
{
  if (gpp->progress_bar_audio)
  {
    /* reset audio extract progress when done */
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(gpp->progress_bar_audio)," ");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gpp->progress_bar_audio), 0.0);
  }
}  /* end p_reset_progress_bar_audio */

/* -----------------------------------------
 * on_audio_otone_extract_button_clicked
 * -----------------------------------------
 */
static void
on_audio_otone_extract_button_clicked (GtkButton       *button,
                               GapPlayerMainGlobalParams *gpp)
{
#ifdef GAP_ENABLE_AUDIO_SUPPORT
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  gboolean    l_otone_is_up_to_date;
  gint32      l_extract_audiotrack;
  gint32      l_begin_frame_nr;
  gint32      l_end_frame_nr;
  char       *l_audiofilename;
  char       *l_audiofilename_cancel;

  if(gpp == NULL)
  {
    return;
  }
  if(gpp->gva_lock)
  {
    /* do not attempt displaying frames while we are inside
     * of p_alloc_ainfo_for_videofile procedure
     */
    return;
  }

  if(gpp->gvahand == NULL)
  {
    if(gpp->ainfo_ptr->ainfo_type == GAP_AINFO_MOVIE)
    {
      p_open_videofile(gpp
                      , gpp->ainfo_ptr->old_filename
                      , gpp->ainfo_ptr->seltrack
                      , gpp->ainfo_ptr->delace
                      , gpp->preferred_decoder
                      );
    }
  }


  if((gpp->gvahand == NULL) || (gpp->gva_videofile == NULL))
  {
    /* no videofile or no videohandle available */
    return;
  }
  if(gpp->gvahand->audio_cannels <= 0)
  {
    /* video has no audiotracks */
    return;
  }

  p_reset_progress_bar_audio(gpp);
  p_msg_progress_bar_audio(gpp, _("cheking audiotrack"));

  l_extract_audiotrack = CLAMP(GTK_ADJUSTMENT(gpp->audio_otone_atrack_spinbutton_adj)->value
                              , 1
                              , gpp->gvahand->audio_cannels
                              );

  /* build otone workfile name (set gpp->audio_otone_wavfile ) */
  l_audiofilename = p_build_otone_audiofilename(gpp->gva_videofile
                               , l_extract_audiotrack
                               , NULL  /* no special suffix used here */
                               );
  l_audiofilename_cancel = p_build_otone_audiofilename(gpp->gva_videofile
                               , l_extract_audiotrack
                               , ".incomplete"
                               );

  l_otone_is_up_to_date = p_check_otone_workfile_up_to_date(gpp->gva_videofile, l_audiofilename, l_extract_audiotrack);
  if (l_otone_is_up_to_date == TRUE)
  {
    p_msg_progress_bar_audio(gpp, _("extracted audio is up to date"));
  }
  else
  {
    gdouble l_dbl_total_frames;
    l_begin_frame_nr = 1;
    l_end_frame_nr = gpp->gvahand->total_frames;
    l_dbl_total_frames = (gdouble)gpp->gvahand->total_frames;

    p_msg_progress_bar_audio(gpp, _("extracting audio"));
 
    if(g_file_test(l_audiofilename, G_FILE_TEST_EXISTS))
    {
      g_remove(l_audiofilename);
    }
    if(g_file_test(l_audiofilename_cancel, G_FILE_TEST_EXISTS))
    {
      g_remove(l_audiofilename_cancel);
    }

    gpp->cancel_video_api = FALSE;
    gpp->request_cancel_video_api = FALSE;
   
    /* extract audio segment (in full length) to otone workfile */
    gap_audio_extract_from_videofile(gpp->gva_videofile
                         , l_audiofilename
                         , l_extract_audiotrack
                         , gpp->preferred_decoder       /* preferred_decoder */
                         , TRUE                         /* exact_seek */
                         , GVA_UPOS_FRAMES              /* pos_unit */
                         , l_begin_frame_nr
                         , l_dbl_total_frames           /* extracted_frames */
                         , l_dbl_total_frames           /* expected_frames */
                         , TRUE                         /* do_progress */
                         , gpp->progress_bar_audio      /* GtkWidget *progressBar  using NULL for gimp_progress */
                         , p_vid_progress_callback      /* fptr_progress_callback */
                         , gpp                          /* user_data */
                         );
    if (gpp->cancel_video_api == TRUE)
    {
      p_msg_progress_bar_audio(gpp, _("Audio Extract CANCELLED"));
    }
    else
    {
       /* reset audio extract progress when successfully done */
       p_reset_progress_bar_audio(gpp);
    }

    if(!g_file_test(l_audiofilename, G_FILE_TEST_EXISTS))
    {
      p_msg_progress_bar_audio(gpp, _("Audio Extract FAILED"));
      g_message(_("Extract of audiotrack failed on videofile: %s"), gpp->gva_videofile);
      goto cleanup;
    }
    else
    {
      gint32 l_audiofile_size;
      
      l_audiofile_size = gap_file_get_filesize(l_audiofilename);
      if (l_audiofile_size < 1)
      {
        p_msg_progress_bar_audio(gpp, _("Audio Extract FAILED"));
        g_message(_("Extract of audiotrack failed on videofile: %s"), gpp->gva_videofile);
        goto cleanup;
      }
      
      if (gpp->cancel_video_api == TRUE)
      {
        /* due to cancel the extracted audiofile is not complete
         * Therefore rename with suffix "incomplete"
         */
        if (strcmp(l_audiofilename_cancel, l_audiofilename) != 0)
        {
          g_rename(l_audiofilename, l_audiofilename_cancel);
          g_free(l_audiofilename);
          l_audiofilename = l_audiofilename_cancel;
          l_audiofilename_cancel = NULL;
        }
      }
    }
  }
  

  gpp->audio_auto_offset_by_framenr = TRUE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->audio_auto_offset_by_framenr_checkbutton),
                                gpp->audio_auto_offset_by_framenr);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->audio_enable_checkbutton),
                                TRUE);

  /* before the gtk_entry_set_text call, already set the gpp->audio_filename
   * explicite to the new name l_audiofilename
   * (this fakes "name has not changed" and prevents the callback
   * on_audio_filename_entry_changed from resetting the progress_bar_audio message
   * that shall be kept until the user enters the audiofilename manually)
   *
   */
  g_snprintf(gpp->audio_filename, sizeof(gpp->audio_filename), "%s"
            , l_audiofilename
            );
  gtk_entry_set_text(GTK_ENTRY(gpp->audio_filename_entry), l_audiofilename);

  p_audio_filename_changed (gpp);

cleanup:
  gpp->cancel_video_api = FALSE;
  if(l_audiofilename)        { g_free(l_audiofilename); }
  if(l_audiofilename_cancel) { g_free(l_audiofilename_cancel);}

#endif  /* GAP_ENABLE_VIDEOAPI_SUPPORT */
#endif  /* GAP_ENABLE_VIDEOAPI_SUPPORT */
}  /* end on_audio_otone_extract_button_clicked */


/* ----------------------------------------
 * on_audio_otone_atrack_spinbutton_changed
 * ----------------------------------------
 */
static void
on_audio_otone_atrack_spinbutton_changed     (GtkEditable     *editable,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  if(gpp->audio_otone_atrack != (gdouble)GTK_ADJUSTMENT(gpp->audio_otone_atrack_spinbutton_adj)->value)
  {
    gpp->audio_otone_atrack = (gdouble)GTK_ADJUSTMENT(gpp->audio_otone_atrack_spinbutton_adj)->value;

#ifdef GAP_ENABLE_AUDIO_SUPPORT
    p_audio_stop(gpp);
#endif
  }

}  /* end on_audio_otone_atrack_spinbutton_changed */



/* --------------------------------
 * p_vid_progress_callback
 * --------------------------------
 * this callback is used while searching videoframes
 * return: TRUE: cancel videoapi immediate
 *         FALSE: continue
 */
static gboolean
p_vid_progress_callback(gdouble progress
                       ,gpointer user_data
                       )
{
  GapPlayerMainGlobalParams *gpp;

  gpp = (GapPlayerMainGlobalParams *)user_data;
  if(gpp == NULL)
  {
    if(gap_debug)
    {
      printf("PLAYER: gpp == NULL p_vid_progress_callback CANCEL == true\n");
    }
    return (TRUE);
  }
  if(gpp->progress_bar == NULL)
  {
    if(gap_debug)
    {
      printf("PLAYER: gpp->progress_bar == NULL p_vid_progress_callback CANCEL == trUE\n");
    }
    return (TRUE);
  }

  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gpp->progress_bar), progress);

  /* g_main_context_iteration makes sure that
   *  gtk does refresh widgets,  and react on events while the videoapi
   *  is busy with searching for the next frame.
   */
  while(g_main_context_iteration(NULL, FALSE));

  /* if vthe long running videoindex creation is running
   * cancel is done only explicite when the cancel_vindex_button is clicked.
   */
  if(!gpp->vindex_creation_is_running)
  {
    /* no videoindex creation is running
     * (in most cases we are in a seek to frame operation at this point)
     * in this case we do cancel immedate on request.
     * request_cancel_video_api is set on most user actions
     * on the player widgets when he wants positioning or start/stop playing
     */
    gpp->cancel_video_api = gpp->request_cancel_video_api;
  }

  if(gpp->cancel_video_api)
  {
    if(gap_debug)
    {
      printf("PLAYER: cancel_video_api is TRUE p_vid_progress_callback CANCEL == TRUE\n");
    }
    return (TRUE);   /* cancel video api if playback was stopped */
  }

  return(FALSE);   /* continue with video API  */

}  /* end p_vid_progress_callback */

/* --------------------------------
 * p_close_videofile
 * --------------------------------
 */
static void
p_close_videofile(GapPlayerMainGlobalParams *gpp)
{
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  if(gpp->gvahand)
  {
    if(gpp->gva_videofile) g_free(gpp->gva_videofile);
    GVA_close(gpp->gvahand);

    gpp->gvahand =  NULL;
    gpp->gva_videofile = NULL;
  }
#endif
}  /* end p_close_videofile */



/* --------------------------------
 * p_reopen_videofile
 * --------------------------------
 */
static void
p_reopen_videofile(GapPlayerMainGlobalParams *gpp
                , char *filename
                , gint32 seltrack
                , gdouble delace
                , const char *preferred_decoder
                )
{
  const char *l_preferred_decoder;

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

  p_close_videofile(gpp);

  /* use global preferred_decoder setting per default */
  l_preferred_decoder = gpp->preferred_decoder;
  if(preferred_decoder)
  {
    if(*preferred_decoder != '\0')
    {
      /* use element specific preferred_decoder if available */
      l_preferred_decoder = preferred_decoder;
    }
  }

  /*
   * printf("PLAYER: open START\n");
   * if(l_preferred_decoder)
   * {
   *   printf("PLAYER: (decoder:%s)\n", l_preferred_decoder);
   * }
   */
  gpp->gvahand =  GVA_open_read_pref(filename
                                  , seltrack
                                  , 1 /* aud_track */
                                  , l_preferred_decoder
                                  , FALSE  /* use MMX if available (disable_mmx == FALSE) */
                                  );

  /*printf("PLAYER: open gpp->gvahand:%d\n", (int)gpp->gvahand);*/

  if(gpp->gvahand)
  {
    gpp->gva_videofile = g_strdup(filename);
    /* gpp->gvahand->emulate_seek = TRUE; */
    gpp->gvahand->do_gimp_progress = FALSE;

    gpp->gvahand->progress_cb_user_data = gpp;
    gpp->gvahand->fptr_progress_callback = p_vid_progress_callback;

    if(gap_debug)
    {
      printf("## p_reopen_videofile gpp->gvahand->total_frames:%d\n"
          ,(int)gpp->gvahand->total_frames
          );
    }

    /* printf("PLAYER: open fptr_progress_callback FPTR:%d\n"
     *       , (int)gpp->gvahand->fptr_progress_callback);
     */

    /* set decoder name as progress idle text */
    {
      t_GVA_DecoderElem *dec_elem;
      if(gpp->progress_bar_idle_txt)
      {
        g_free(gpp->progress_bar_idle_txt);
      }

      dec_elem = (t_GVA_DecoderElem *)gpp->gvahand->dec_elem;
      if(dec_elem->decoder_name)
      {
        gpp->progress_bar_idle_txt = g_strdup(dec_elem->decoder_name);
      }
      else
      {
        gpp->progress_bar_idle_txt = g_strdup(" ");
      }
    }

  }
#endif
}  /* end p_reopen_videofile */



/* --------------------------------
 * p_open_videofile
 * --------------------------------
 * open videofile handle
 * with checks for video index creation.
 * in case where video index creation is recommanded
 * this is done after asking the user for permission.
 */
static void
p_open_videofile(GapPlayerMainGlobalParams *gpp
                , char *filename
                , gint32 seltrack
                , gdouble delace
                , const char *preferred_decoder
                )
{
  char *vindex_file;
  gboolean    l_have_valid_vindex;
  l_have_valid_vindex = FALSE;

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

  p_reopen_videofile(gpp, filename, seltrack, delace, preferred_decoder);

  vindex_file = NULL;

  if(gpp->gvahand)
  {
    /* set decoder name as progress idle text */
    {
      t_GVA_DecoderElem *dec_elem;

      dec_elem = (t_GVA_DecoderElem *)gpp->gvahand->dec_elem;
      if(dec_elem->decoder_name)
      {
        vindex_file = GVA_build_videoindex_filename(gpp->gva_videofile
                                             ,1  /* track */
                                             ,dec_elem->decoder_name
                                             );
      }
    }
    
    if(gpp->gvahand->vindex)
    {
      if(gpp->gvahand->vindex->total_frames > 0)
      {
        l_have_valid_vindex = TRUE;
      }
    }

    if((l_have_valid_vindex == FALSE)
    &&(gpp->startup == FALSE))
    {
      gboolean vindex_permission;
      t_GVA_SeekSupport seekSupp;

      gpp->cancel_video_api = FALSE;
      gpp->request_cancel_video_api = FALSE;
      if(gpp->progress_bar)
      {
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(gpp->progress_bar), _("seek-selftest"));
      }
      seekSupp = GVA_check_seek_support(gpp->gvahand);

      /* check for permission to create a videoindex file */
      vindex_permission = gap_arr_create_vindex_permission(gpp->gva_videofile
                            , vindex_file
                            , (gint32)seekSupp
                            );

      if(vindex_permission)
      {
        /* create video index
         * (dont do that while we are in startup
         * because the shell window and progress_bar widget and many other things
         * are not set up yet and this long running operation
         * would be done without any visible sign)
         */
        gpp->cancel_video_api = FALSE;
        gpp->request_cancel_video_api = FALSE;
        if(gpp->progress_bar)
        {
           gtk_progress_bar_set_text(GTK_PROGRESS_BAR(gpp->progress_bar), _("Creating Index"));
        }
        gpp->gvahand->create_vindex = TRUE;

        /* while videoindex creation show cancel button and hide play/stop buttons */
        gtk_widget_hide (gpp->play_n_stop_hbox);
        gtk_widget_show (gpp->cancel_vindex_button);

        /* printf("PLAYER: open before GVA_count_frames\n"); */

        gpp->vindex_creation_is_running = TRUE;
        GVA_count_frames(gpp->gvahand);
        gpp->vindex_creation_is_running = FALSE;

        /* printf("PLAYER: open after GVA_count_frames\n"); */

        gtk_widget_hide (gpp->cancel_vindex_button);
        gtk_widget_show (gpp->play_n_stop_hbox);

        if(!gpp->cancel_video_api)
        {
          if(gpp->gvahand->vindex != NULL)
          {
            p_update_ainfo_for_videofile(gpp);
            p_update_ainfo_dependent_widgets(gpp);

            p_reopen_videofile(gpp, filename, seltrack, delace, preferred_decoder);
          }
          else
          {
            g_message(_("No videoindex available. "
                        "Access is limited to (slow) sequential read "
                        "on file: %s")
                     , gpp->gvahand->filename
                     );
          }
        }
      }
    }

    /* GVA_debug_print_videoindex(gpp->gvahand); */

    if(vindex_file)
    {
      g_free(vindex_file);
    }

  }
/* printf("PLAYER: open END\n"); */
#endif
  
}  /* end p_open_videofile */




/* -----------------------------
 * p_fetch_videoframe
 * -----------------------------
 */
static guchar *
p_fetch_videoframe(GapPlayerMainGlobalParams *gpp
                   , char *gva_videofile
                   , gint32 framenumber
                   , gint32 rangesize
                   , gint32 seltrack
                   , gdouble delace
                   , const char *preferred_decoder
                   , gint32 *th_bpp
                   , gint32 *th_width
                   , gint32 *th_height
                   , gboolean isBackwards
                   )
{
  guchar *th_data;
  static gint32 funcId = -1;
  
  GAP_TIMM_GET_FUNCTION_ID(funcId, "playerDialog.p_fetch_videoframe");


  th_data = NULL;
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

  if(gpp->gva_lock)
  {
    return(NULL);
  }
  gpp->gva_lock = TRUE;

  GAP_TIMM_START_FUNCTION(funcId);

  if((gpp->gva_videofile) && (gpp->gvahand))
  {
    if((strcmp(gpp->gva_videofile, gva_videofile) != 0)
    ||(seltrack != (gpp->gvahand->vid_track+1)))
    {
//printf(" VIDFETCH (..) CALL p_close_videofile\n");
//printf(" VIDFETCH (..) gpp->gva_videofile: %s\n", gpp->gva_videofile);
//printf(" VIDFETCH (..) gpp->gvahand->vid_track: %d\n", (int)gpp->gvahand->vid_track);
//printf(" VIDFETCH (..) gva_videofile: %s\n", gva_videofile);
//printf(" VIDFETCH (..) seltrack: %d\n", (int)seltrack);
      p_close_videofile(gpp);
    }
  }

//printf(" VIDFETCH (0) gpp->gvahand: %d  framenumber:%06d\n", (int)gpp->gvahand, (int)framenumber);
  if(gpp->gvahand == NULL)
  {
    p_open_videofile(gpp, gva_videofile, seltrack, delace, preferred_decoder);
  }

  if(gpp->gvahand)
  {
     gint32 l_deinterlace;
     gdouble l_threshold;
     gboolean do_scale;
     gint32 fcache_size;

     if(gpp->progress_bar)
     {
       gtk_progress_bar_set_text(GTK_PROGRESS_BAR(gpp->progress_bar), _("Videoseek"));
     }

//printf(" VIDFETCH (3) gimprc: FCACHE SIZE: %d\n", (int)gpp->gvahand->fcache.frame_cache_size);

     if(global_max_vid_frames_to_keep_cached < 1)
     {
        fcache_size = gap_base_get_gimprc_int_value("video-max-frames-keep-cached"
                                                     , GAP_PLAYER_VID_FRAMES_TO_KEEP_CACHED  /* default */
                                                     , 2   /* min */
                                                     , 250 /* max */
                                                   );
     }

     // fcache_size = CLAMP(rangesize, 1, global_max_vid_frames_to_keep_cached);
     if(fcache_size > GVA_get_fcache_size_in_elements(gpp->gvahand))
     {
//printf(" VIDFETCH (5) gimprc: FCACHE_MAX:%d fcache_size:%d rangesize:%d\n"
//         , (int)global_max_vid_frames_to_keep_cached
//          , (int)fcache_size
//        , (int)rangesize
//         );

       GVA_set_fcache_size(gpp->gvahand, fcache_size);
     }



     do_scale = TRUE;
     if(*th_width < 1)
     {
       *th_bpp = gpp->gvahand->frame_bpp;
       *th_width = gpp->gvahand->width;
       *th_height = gpp->gvahand->height;
       do_scale = FALSE;
     }

     /* split delace value: integer part is deinterlace mode, rest is threshold */
     l_deinterlace = delace;
     l_threshold = delace - (gdouble)l_deinterlace;

     gpp->cancel_video_api = FALSE;
     gpp->request_cancel_video_api = FALSE;

//printf(" VIDFETCH (6) current_seek_nr:%d current_frame_nr:%d\n", (int)gpp->gvahand->current_seek_nr  ,(int)gpp->gvahand->current_frame_nr );
     /* fetch the wanted framenr  */
     th_data = GVA_fetch_frame_to_buffer(gpp->gvahand
                , do_scale
                , isBackwards
                , framenumber
                , l_deinterlace
                , l_threshold
                , th_bpp
                , th_width
                , th_height
                );
//printf(" VIDFETCH (7) current_seek_nr:%d current_frame_nr:%d\n", (int)gpp->gvahand->current_seek_nr  ,(int)gpp->gvahand->current_frame_nr );
     if(gpp->progress_bar)
     {
       gtk_progress_bar_set_text(GTK_PROGRESS_BAR(gpp->progress_bar)
                                ,gpp->progress_bar_idle_txt
                                );
       gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gpp->progress_bar), 0.0);
     }
  }

  if(th_data)
  {
    /* throw away frame data because the fetch was cancelled
     * (an empty display is better than trash or the wrong frame)
     */
    if(gpp->cancel_video_api)
    {
      g_free(th_data);
      th_data = NULL;
    }
    else if (gpp->dvref_ptr != NULL)
    {
      /* refresh current videofile reference 
       * Note: the  dvref_ptr->videofile gets no COPY but only a rference to the current videofilename
       *       this pointer can be reset to NULL any time (at p_display_frame)
       *       but MUST NOT free up the refered name.
       */
      gpp->dvref_ptr->videofile = gpp->gva_videofile;
      gpp->dvref_ptr->para.framenr = framenumber;
      gpp->dvref_ptr->para.seltrack = seltrack;
      g_snprintf(&gpp->dvref_ptr->para.preferred_decoder[0], sizeof(gpp->dvref_ptr->para.preferred_decoder), "%s"
                , preferred_decoder
                );
      if(gap_debug)
      {
        printf("p_fetch_videoframe: dvref\n");
        gap_dvref_debug_print_GapDrawableVideoRef(gpp->dvref_ptr);
      }

    }
  }
  else
  {
    /* one possible reason for a videframe fetch failure
     * could be an EOF in the video
     * (in this moment the GVA API might know the exactly
     *  number of total_frames and we can set the limits in the player widget
     *  if we are running on a single videofile -- but NOT in storyboard mode)
     */
    if(!gpp->stb_ptr)
    {
      if((gpp->imagename) && (gpp->gvahand->all_frames_counted))
      {
        if(gap_debug)
        {
          printf("p_fetch_videoframe  CALL p_update_ainfo_dependent_widgets ainfo_ptr->frame_cnt:%d gpp->gvahand->total_frames:%d\n"
            , (int)gpp->ainfo_ptr->frame_cnt
            , (int)gpp->gvahand->total_frames
            );
        }
        gpp->ainfo_ptr->frame_cnt = gpp->gvahand->total_frames;
        gpp->ainfo_ptr->last_frame_nr = gpp->gvahand->total_frames;
        p_update_ainfo_dependent_widgets(gpp);
      }
    }
  }
  gpp->gva_lock = FALSE;

  GAP_TIMM_STOP_FUNCTION(funcId);

#endif
  return (th_data);
}  /* end p_fetch_videoframe */


/* -----------------------------
 * p_init_tile_cache
 * -----------------------------
 */
static void
p_init_tile_cache(GapPlayerMainGlobalParams *gpp)
{
  gchar *value_string;

  gpp->cache_ntiles = GAP_PLAYER_MAIN_DEFAULT_CACHE_NTILES;

  value_string = gimp_gimprc_query("video_player_cache_ntiles");
  if(value_string)
  {
    gpp->cache_ntiles = MAX(0, atol(value_string));
    
    g_free(value_string);
  }

  gimp_tile_cache_ntiles(gpp->cache_ntiles);
  
}  /* end p_init_tile_cache */

/* -----------------------------
 * p_init_video_playback_cache
 * -----------------------------
 */
static void
p_init_video_playback_cache(GapPlayerMainGlobalParams *gpp)
{
  gpp->max_player_cache = gap_player_cache_get_gimprc_bytesize();
  gap_player_cache_set_max_bytesize(gpp->max_player_cache);
}  /* end p_init_video_playback_cache */

/* -----------------------------
 * p_init_layout_options
 * -----------------------------
 */
static void
p_init_layout_options(GapPlayerMainGlobalParams *gpp)
{
  gchar *value_string;

  gpp->show_go_buttons = TRUE;
  gpp->show_position_scale = TRUE;

  value_string = gimp_gimprc_query("video_player_show_go_buttons");
  if(value_string)
  {
    if ((*value_string == 'Y') || (*value_string == 'y'))
    {
      gpp->show_go_buttons = TRUE;
    }
    else
    {
      gpp->show_go_buttons = FALSE;
    }

    g_free(value_string);
  }

  value_string = gimp_gimprc_query("video_player_show_position_scale");
  if(value_string)
  {
    if ((*value_string == 'Y') || (*value_string == 'y'))
    {
      gpp->show_position_scale = TRUE;
    }
    else
    {
      gpp->show_position_scale = FALSE;
    }

    g_free(value_string);
  }


  value_string = gimp_gimprc_query("video_player_enable_detail_tracking");
  if(value_string)
  {
    if ((*value_string == 'Y') || (*value_string == 'y'))
    {
      gpp->enableDetailTracking = TRUE;
    }
    else
    {
      gpp->enableDetailTracking = FALSE;
    }

    g_free(value_string);
  }
}  /* end p_init_layout_options */


/* ------------------------------
 * p_fetch_frame_via_cache
 * ------------------------------
 * fetch any type of frame from the players cache.
 * returns NULL
 *    - if no frame for the specified ckey was found in the cache,
 *    - or if the player runs in MTRACE mode,
 *    the parameters th_bpp, th_width, th_height are not changed
 *    in case when NULL is the return value
 * If cached frame is returned,
 *    the parameters th_bpp, th_width, th_height are set
 *    according to the size of the cached frame
 */
static guchar *
p_fetch_frame_via_cache(GapPlayerMainGlobalParams *gpp
    , const gchar *ckey
    , gint32 *th_bpp_ptr             /* IN/OUT */
    , gint32 *th_width_ptr           /* IN/OUT */
    , gint32 *th_height_ptr          /* IN/OUT */
    , gint32 *flip_status_ptr        /* IN/OUT */
    )
{
  GapPlayerCacheData* cdata;
  guchar *th_data;

  cdata = NULL;
  th_data = NULL;

  if (gpp->mtrace_mode == GAP_PLAYER_MTRACE_OFF)
  {
    cdata = gap_player_cache_lookup(ckey);
  }
  if(cdata != NULL)
  {
    th_data = gap_player_cache_decompress(cdata);
    *th_bpp_ptr     = cdata->th_bpp;
    *th_width_ptr   = cdata->th_width;
    *th_height_ptr  = cdata->th_height;
    *flip_status_ptr = cdata->flip_status;
  }

  return (th_data);

}  /* end p_fetch_frame_via_cache */


/* ------------------------------
 * p_fetch_videoframe_via_cache
 * ------------------------------
 * fetch frame from the players cache
 * or alternatively from the videofile.
 *
 * No cache lookup is done if the player
 * runs in MTRACE mode, because the cache
 * holds a scaled copy, but MTRACE shall
 * deliver the original image quality and size.
 */
static guchar *
p_fetch_videoframe_via_cache(GapPlayerMainGlobalParams *gpp
                   , char *gva_videofile
                   , gint32 framenumber
                   , gint32 rangesize
                   , gint32 seltrack
                   , gdouble delace
                   , const char *preferred_decoder
                   , gint32 *th_bpp_ptr
                   , gint32 *th_width_ptr
                   , gint32 *th_height_ptr
                   , gint32 *flip_status_ptr
                   , const gchar *ckey
                   , gboolean isBackwards
                   )
{
  guchar *th_data;

  th_data = p_fetch_frame_via_cache(gpp
              , ckey
              , th_bpp_ptr
              , th_width_ptr
              , th_height_ptr
              , flip_status_ptr
              );
  if(th_data == NULL)
  {
    th_data = p_fetch_videoframe(gpp
                , gva_videofile
                , framenumber
                , rangesize
                , seltrack
                , delace
                , preferred_decoder
                , th_bpp_ptr
                , th_width_ptr
                , th_height_ptr
                , isBackwards
                );
  }

  return (th_data);

}  /* end p_fetch_videoframe_via_cache */




/* ------------------------------
 * p_frame_chache_processing
 * ------------------------------
 */
static void
p_frame_chache_processing(GapPlayerMainGlobalParams *gpp
    , const gchar *ckey)
{
  GapPlayerCacheData *cdata;
  guchar             *th_data;
  gint32              th_size;
  gint32              th_width;
  gint32              th_height;
  gint32              th_bpp;
  gboolean            th_has_alpha;


  if (gpp->max_player_cache <= 0)
  {
    /* chaching is turned OFF */
    return;
  }
  if(gap_player_cache_lookup(ckey) != NULL)
  {
    /* frame with ckey is already cached */
    return;
  }

  th_data = gap_pview_get_repaint_thdata(gpp->pv_ptr
                            , &th_size
                            , &th_width
                            , &th_height
                            , &th_bpp
                            , &th_has_alpha
                            );
  if (th_data != NULL)
  {
    cdata = gap_player_cache_new_data(th_data
             , th_size
             , th_width
             , th_height
             , th_bpp
             , gpp->cache_compression
             , gpp->pv_ptr->flip_status
             );
    if(cdata != NULL)
    {
      /* insert frame into cache
       */
      gap_player_cache_insert(ckey, cdata);
      p_update_cache_status(gpp);
    }
  }

}  /* end p_frame_chache_processing */




/* --------------------------------
 * p_close_composite_storyboard
 * --------------------------------
 */
static void
p_close_composite_storyboard(GapPlayerMainGlobalParams *gpp)
{
  if(gpp->stb_ptr == NULL)
  {
    return;
  }
  gpp->stb_parttype = -1;
  gpp->stb_unique_id = -1;

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  if(gpp->stb_comp_vidhand)
  {
    gap_story_render_close_vid_handle(gpp->stb_comp_vidhand);;
    gpp->stb_comp_vidhand = NULL;
  }
#endif
}  /* end p_close_composite_storyboard */


/* --------------------------------
 * p_open_composite_storyboard
 * --------------------------------
 */
static void
p_open_composite_storyboard(GapPlayerMainGlobalParams *gpp)
{
  gint32 l_total_framecount;

  if(gpp->stb_ptr == NULL)
  {
    return;
  }
#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
  p_close_composite_storyboard(gpp);

  gpp->stb_comp_vidhand = gap_story_render_open_vid_handle_from_stb (gpp->stb_ptr
                                       ,&l_total_framecount
                                       );

  if(gap_debug)
  {
    printf("p_open_composite_storyboard: %s comp_vidhand:%d type:%d(%d) id:%d(%d)\n"
        ,gpp->stb_ptr->storyboardfile
        ,(int)gpp->stb_comp_vidhand
        ,(int)gpp->stb_ptr->stb_parttype
        ,(int)gpp->stb_parttype
        ,(int)gpp->stb_ptr->stb_unique_id
        ,(int)gpp->stb_unique_id
        );
    gap_story_render_debug_print_framerange_list(gpp->stb_comp_vidhand->frn_list, -1);
  }

  if(gpp->stb_comp_vidhand)
  {
    /* store identifiers of the opened storyboard */
    gpp->stb_parttype = gpp->stb_ptr->stb_parttype;
    gpp->stb_unique_id = gpp->stb_ptr->stb_unique_id;
  }

#endif
}  /* end p_open_composite_storyboard */


/* -----------------------------
 * p_fetch_composite_image
 * -----------------------------
 */
static gint32
p_fetch_composite_image(GapPlayerMainGlobalParams *gpp
                   , gint32 framenumber
                   , gint32 width
                   , gint32 height
                   )
{
  gint32 composite_image_id;

  composite_image_id = -1;
  if(gpp->stb_ptr == NULL)
  {
    return (composite_image_id);
  }

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

  /* (re)open if nothing already open or ident data has changed */
  if ((gpp->stb_comp_vidhand == NULL)
  ||  (gpp->stb_parttype != gpp->stb_ptr->stb_parttype)
  ||  (gpp->stb_unique_id != gpp->stb_ptr->stb_unique_id))
  {
    p_open_composite_storyboard(gpp);
  }

  if(gpp->stb_comp_vidhand)
  {
      gint32 l_layer_id;
      l_layer_id = -1;

      /* The storyboard render processor is used to fetch
       * the frame as rendered gimp image of desired size.
       *
       * NOTE:flip_request transformations are already rendered by
       * the storyboard render processor
       * therefore no changes to propagate to the players
       * pview widget
       */

      composite_image_id = gap_story_render_fetch_composite_image(
                               gpp->stb_comp_vidhand
                               , framenumber
                               , width
                               , height
                               , NULL   /*  filtermacro_file */
                               , &l_layer_id
                               );
       if(gap_debug)
       {
         printf("p_fetch_composite_image: comp_vidhand:%d  composite_image_id:%d\n"
             ,(int)gpp->stb_comp_vidhand
             ,(int)composite_image_id
             );
       }
  }

#endif
  return (composite_image_id);
}  /* end p_fetch_composite_image */


/* ---------------------------------------
 * p_fetch_display_th_data_from_storyboard
 * ---------------------------------------
 * fetch the requested framnr as th_data
 * by interpreting the relevant storyboard clip element.
 *
 * return th_data if the clip is from type MOVIE
 * return NULL in case of other clip types (of error)
 *             for other clip types (IMAGE or FRAME range)
 *             the filename of the relevant image is set into the
 *             output parameter filename_pptr.
 * OUT: ckey_pptr       the string for caching the frame fetched from clip type MOVIE
 *                      (no changes for other clip types)
 * OUT: filename_pptr   filename for fetching frame in case clip type is no MOVIE
 *                      is reset to NULL for clip type MOVIE
 */
guchar *
p_fetch_display_th_data_from_storyboard(GapPlayerMainGlobalParams *gpp
  , gint32   framenr
  , gchar  **ckey_pptr
  , gchar  **filename_pptr
  , gint32  *th_width_ptr
  , gint32  *th_height_ptr
  , gint32  *th_bpp_ptr
  , gint32  *flip_request_ptr
  , gint32  *flip_status_ptr
  )
{
  GapStoryLocateRet *stb_ret;
  guchar *l_th_data;
  static gint32 funcId = -1;
  
  GAP_TIMM_GET_FUNCTION_ID(funcId, "playerDialog.p_fetch_display_th_data_from_storyboard");
  GAP_TIMM_START_FUNCTION(funcId);

  l_th_data = NULL;
  stb_ret = gap_story_locate_framenr(gpp->stb_ptr
                                  , framenr
                                  , gpp->stb_in_track
                                  );
  if(stb_ret)
  {
    if((stb_ret->locate_ok)
    && (stb_ret->stb_elem))
    {
      *filename_pptr = gap_story_get_filename_from_elem_nr(stb_ret->stb_elem
                                              , stb_ret->ret_framenr
                                             );
      *flip_request_ptr = stb_ret->stb_elem->flip_request;
      if(stb_ret->stb_elem->record_type == GAP_STBREC_VID_MOVIE)
      {
         if(*filename_pptr)
         {
           gboolean isBackwards;
           
           if(gpp->use_thumbnails)
           {
             /* fetch does already scale down to current preview size */
             *th_width_ptr = gpp->pv_ptr->pv_width;
             *th_height_ptr = gpp->pv_ptr->pv_height;
           }
           else
           {
             /* negative width/height does force fetch at original video size */
             *th_width_ptr = -1;
             *th_height_ptr = -1;
           }

           if (gpp->max_player_cache > 0)
           {
             *ckey_pptr = gap_player_cache_new_movie_key(*filename_pptr
                         , stb_ret->ret_framenr
                         , stb_ret->stb_elem->seltrack
                         , stb_ret->stb_elem->delace
                         );
           }
           isBackwards = (stb_ret->stb_elem->to_frame < stb_ret->stb_elem->from_frame) || gpp->play_backward;
           l_th_data  = p_fetch_videoframe_via_cache(gpp
                         , *filename_pptr
                         , stb_ret->ret_framenr
                         , 1 + (abs(stb_ret->stb_elem->to_frame) - abs(stb_ret->stb_elem->from_frame))
                         , stb_ret->stb_elem->seltrack
                         , stb_ret->stb_elem->delace
                         , stb_ret->stb_elem->preferred_decoder
                         , th_bpp_ptr       /* IN/OUT */
                         , th_width_ptr     /* IN/OUT */
                         , th_height_ptr    /* IN/OUT */
                         , flip_status_ptr  /* IN/OUT */
                         , *ckey_pptr       /* IN */
                         , isBackwards
                         );
           if(gpp->cancel_video_api)
           {
             if(l_th_data)
             {
               g_free(l_th_data); /* throw away undefined data in case of cancel */
               l_th_data = NULL;
             }
             if(gpp->progress_bar)
             {
                gtk_progress_bar_set_text(GTK_PROGRESS_BAR(gpp->progress_bar)
                             , _("Canceled"));
             }
           }
           g_free(*filename_pptr);
           *filename_pptr = NULL;
         }
      }
    }
    g_free(stb_ret);
  }

  GAP_TIMM_STOP_FUNCTION(funcId);

  return (l_th_data);
}  /* end p_fetch_display_th_data_from_storyboard */



/* -----------------------------------------------
 * p_fetch_display_composite_image_from_storyboard
 * -----------------------------------------------
 * fetch the requested framnr as th_data (if already cached)
 * or as gimp image id (if fetched as composite video frame
 * via storyboard render processor)
 *
 * return th_data if the frame was in the cache. (NULL if not)
 * OUT: ckey_pptr               the string for caching the frame
 * OUT: composite_image_id_ptr  image id if fetched via processing.
 *                               (-1 is output if no image was fetched)
 */
guchar *
p_fetch_display_composite_image_from_storyboard(GapPlayerMainGlobalParams *gpp
  , gint32   framenr
  , gchar  **ckey_pptr
  , gint32  *composite_image_id_ptr
  , gint32  *th_width_ptr
  , gint32  *th_height_ptr
  , gint32  *th_bpp_ptr
  , gint32  *flip_request_ptr
  , gint32  *flip_status_ptr
  )
{
  gint32 l_width;
  gint32 l_height;
  guchar *l_th_data;

  l_th_data = NULL;
  *composite_image_id_ptr = -1;

  if(gpp->stb_ptr == NULL)
  {
    return (NULL);
  }

  l_width = gpp->stb_ptr->master_width;
  l_height = gpp->stb_ptr->master_height;
  if(gpp->use_thumbnails)
  {
    /* fetch does already scale down to current preview size */
    l_width = gpp->pv_ptr->pv_width;
    l_height = gpp->pv_ptr->pv_height;
  }

  if(gpp->stb_comp_vidhand == NULL)
  {
     p_open_composite_storyboard(gpp);
  }

  if(gpp->stb_comp_vidhand)
  {
    *ckey_pptr = gap_player_cache_new_composite_video_key(
                           gpp->stb_ptr->storyboardfile
                         , framenr
                         , gpp->stb_ptr->stb_parttype
                         , gpp->stb_ptr->stb_unique_id
                         );


    l_th_data = p_fetch_frame_via_cache(gpp
              , *ckey_pptr
              , th_bpp_ptr
              , th_width_ptr
              , th_height_ptr
              , flip_status_ptr
              );
    if(gap_debug)
    {
      printf("COMPOSITE thdata:%d ckey: %s\n"
             ,(int)l_th_data
             ,*ckey_pptr
             );
    }

    if(l_th_data == NULL)
    {
      /* The storyboard render processor is used to fetch
       * the frame as rendered gimp image of desired size.
       * in this case th_data is returned as NULL.
       *
       * NOTE:flip_request transformations are already rendered by
       * the storyboard render processor
       * therefore no changes to propagate to the players
       * pview widget
       */

      *composite_image_id_ptr = p_fetch_composite_image(gpp
                                             , framenr
                                             , l_width
                                             , l_height
                                             );
      *flip_status_ptr = *flip_request_ptr;

    }
  }

  return (l_th_data);
}  /* end p_fetch_display_composite_image_from_storyboard */




/* ------------------------------
 * p_render_display_free_pixbuf
 * ------------------------------
 * render form pixbuf
 * free the pixbuf after rendering (unref)
 */
static void
p_render_display_free_pixbuf(GapPlayerMainGlobalParams *gpp
   , GdkPixbuf *pixbuf
   , gint32  flip_request
   , gint32  flip_status
   )
{
    /* copy pixbuf as layer into the mtrace_image (only of mtrace_mode not OFF) */
    p_mtrace_pixbuf(gpp, pixbuf);

    gap_pview_render_f_from_pixbuf (gpp->pv_ptr
                                   , pixbuf
                                   , flip_request
                                   , flip_status
                                   );
    g_object_unref(pixbuf);
}  /* end p_render_display_free_pixbuf */



/* ------------------------------
 * p_render_display_free_thdata
 * ------------------------------
 * render form th_data buffer
 * free the th_data buffer after rendering
 *  (but only if data was not grabbed for refresh)
 */
static void
p_render_display_free_thdata(GapPlayerMainGlobalParams *gpp
   , guchar *th_data
   , gint32  th_width
   , gint32  th_height
   , gint32  th_bpp
   , gint32  flip_request
   , gint32  flip_status
   )
{
  gboolean th_data_was_grabbed;

  p_mtrace_tmpbuf(gpp
               , th_data
               , th_width
               , th_height
               , th_bpp
               );

  th_data_was_grabbed = gap_pview_render_f_from_buf (gpp->pv_ptr
               , th_data
               , th_width
               , th_height
               , th_bpp
               , TRUE         /* allow_grab_src_data */
               , flip_request
               , flip_status
               );
  if(th_data_was_grabbed)
  {
    /* the gap_pview_render_f_from_buf procedure can grab the th_data
     * instead of making a ptivate copy for later use on repaint demands.
     * if such a grab happened it returns TRUE.
     * (this is done for optimal performance reasons)
     * in such a case the caller must NOT free the src_data (th_data) !!!
     */
    th_data = NULL;
  }

  if(th_data)  g_free(th_data);

}  /* end p_render_display_free_thdata */




/* ------------------------------
 * p_render_display_free_image_id
 * ------------------------------
 * render from the specified image_id
 * and delete this image after rendering
 */
static void
p_render_display_free_image_id(GapPlayerMainGlobalParams *gpp
   , gint32 image_id
   , gint32  flip_request
   , gint32  flip_status
  )
{
  /* there is no need for undo on this scratch image
   * so we turn undo off for performance reasons
   */
  gimp_image_undo_disable (image_id);

  /* copy image as layer into the mtrace_image (only if mtrace_mode not OFF) */
  p_mtrace_image(gpp, image_id);

  gap_pview_render_f_from_image (gpp->pv_ptr
                               , image_id
                               , flip_request
                               , flip_status
                               );
  gimp_image_delete(image_id);

}  /* end p_render_display_free_image_id */



/* ------------------------------------------
 * p_render_display_from_active_image_or_file
 * ------------------------------------------
 */
static void
p_render_display_from_active_image_or_file(GapPlayerMainGlobalParams *gpp
   , char *img_filename
   , gboolean framenr_is_the_active_image
   , gint32  flip_request
   , gint32  flip_status
  )
{
  gint32  l_image_id;

  /* got no thumbnail data, must use the full image */
  if(framenr_is_the_active_image)
  {
    /* check if automatic onionskin layer removal is turned on
     * (in this case the player must display the image without
     * onionskin layers)
     */
    if(gpp->onion_delete)
    {
      l_image_id = gap_onion_base_image_duplicate(gpp->image_id);
      gap_onion_base_onionskin_delete(l_image_id);
    }
    else
    {
      l_image_id = gimp_image_duplicate(gpp->image_id);
    }
  }
  else
  {
    l_image_id = gap_lib_load_image(img_filename);

    if (l_image_id < 0)
    {
      /* could not read the image
       * one reason could be, that frames were deleted while this plugin is active
       * so we stop playback,
       * and try to reload informations about all frames
       */
      if(gap_debug) printf("LOAD IMAGE_ID: %s failed\n", img_filename);
      if(gpp->image_id < 0)
      {
        /* give up, because we could not get any imagedata
         * (this case may happen if images are deleted while playback
         *  or if videofiles with unknown extensions were passed
         *  to the player and were tried to load as gimp image
         *  because they are not recognized as video)
         */
        return;
      }
      p_keep_track_of_active_master_image(gpp);
    }
  }

  if(gpp->use_thumbnails)
  {
    gap_thumb_cond_gimp_file_save_thumbnail(l_image_id, img_filename);
  }


  p_render_display_free_image_id(gpp
     , l_image_id
     , flip_request
     , flip_status
     );

}  /* end p_render_display_from_active_image_or_file */


/* ------------------------------
 * p_display_frame
 * ------------------------------
 * display framnr from thumbnail or from full image
 * the active image (the one from where we were invoked)
 * is not read from discfile to reflect actual changes.
 *
 * Alternative sources for fetching the frame are
 * A) storyboard files (gpp->stb_ptr != NULL)
 *    in this case the fetch for a single track is done by
 *    interpreting the relevant videoclip,
 *
 *    or via calling the fetcher from the storyboard render processor
 *    to get the composite image (if stb_in_track -1)
 *
 * B) fetching from a single movie file
 *    when invoked from the video extract plug-in.
 *    (gpp->ainfo_ptr->ainfo_type == GAP_AINFO_MOVIE)
 *
 * Note:
 *   there is no active image if the player was invoked
 *   for storyboard playback (gpp->stb_ptr != NULL)
 *   or for filename based playback (gpp->imagename != NULL)
 *   in those cases we always have to fetch from file (thumb or full image)
 */
static void
p_display_frame(GapPlayerMainGlobalParams *gpp, gint32 framenr)
{
  char   *l_filename;
  gint32  l_th_width;
  gint32  l_th_height;
  gint32  l_th_data_count;
  gint32  l_th_bpp;
  gint32  l_flip_request;
  gint32  l_flip_status;
  gint32  l_composite_image_id;
  guchar *l_th_data;
  gboolean framenr_is_the_active_image;
  GdkPixbuf *pixbuf;
  gchar *ckey;
  static gint32 funcId = -1;
  
  GAP_TIMM_GET_FUNCTION_ID(funcId, "playerDialog.p_display_frame");

  /*if(gap_debug) printf("p_display_frame START: framenr:%d\n", (int)framenr);*/
  if(gpp->gva_lock)
  {
    /* do not attempt displaying frames while we are inside
     * of p_alloc_ainfo_for_videofile procedure
     */
    return;
  }

  GAP_TIMM_START_FUNCTION(funcId);

  ckey = NULL;
  l_th_data = NULL;
  pixbuf = NULL;
  l_th_bpp = 3;
  l_filename = NULL;
  framenr_is_the_active_image = FALSE;
  l_composite_image_id = -1;

  if(gpp->dvref_ptr != NULL)
  {
    gpp->dvref_ptr->videofile = NULL;
  }

  if(gpp->stb_ptr)
  {
    l_flip_request = GAP_STB_FLIP_NONE;
    l_flip_status = GAP_STB_FLIP_NONE;

    if(gpp->stb_in_track >= 0)
    {
      l_th_data = p_fetch_display_th_data_from_storyboard(gpp
                , framenr
                , &ckey
                , &l_filename
                , &l_th_width
                , &l_th_height
                , &l_th_bpp
                , &l_flip_request
                , &l_flip_status
                );

      if(gpp->cancel_video_api)
      {
        GAP_TIMM_STOP_FUNCTION(funcId);
        return;
      }
    }
    else
    {
      gint32 mapped_framenr =
        gap_story_get_mapped_master_frame_number(gpp->stb_ptr->mapping, framenr);

      if(gap_debug)
      {
        printf("  >> PLAY composite framenr:%d mapped_framenr:%d\n"
          ,(int)framenr
          ,(int)mapped_framenr
          );
      }
      l_th_data = p_fetch_display_composite_image_from_storyboard(gpp
                , mapped_framenr
                , &ckey
                , &l_composite_image_id
                , &l_th_width
                , &l_th_height
                , &l_th_bpp
                , &l_flip_request
                , &l_flip_status
                );
    }
  }
  else
  {
    l_flip_request = gpp->flip_request;
    l_flip_status = gpp->flip_status;

    if(gpp->ainfo_ptr->ainfo_type == GAP_AINFO_MOVIE)
    {
      gboolean isBackwards;
      
      /* playback of a single videoclip */
      if(gpp->use_thumbnails)
      {
        /* fetch does alread scale down to current preview size */
        l_th_width = gpp->pv_ptr->pv_width;
        l_th_height = gpp->pv_ptr->pv_height;
      }
      else
      {
        /* negative width/height does force fetch at original video size */
        l_th_width = -1;
        l_th_height = -1;
      }

      if (gpp->max_player_cache > 0)
      {
        ckey = gap_player_cache_new_movie_key(gpp->ainfo_ptr->old_filename
                         , framenr
                         , gpp->ainfo_ptr->seltrack
                         , gpp->ainfo_ptr->delace
                         );
      }

      isBackwards = (gpp->ainfo_ptr->last_frame_nr < gpp->ainfo_ptr->first_frame_nr) || gpp->play_backward;
      l_th_data  = p_fetch_videoframe_via_cache(gpp
                                     , gpp->ainfo_ptr->old_filename
                                     , framenr
                                     , 1 + (abs(gpp->ainfo_ptr->last_frame_nr) - abs(gpp->ainfo_ptr->first_frame_nr))
                                     , gpp->ainfo_ptr->seltrack
                                     , gpp->ainfo_ptr->delace
                                     , gpp->preferred_decoder
                                     , &l_th_bpp       /* IN/OUT */
                                     , &l_th_width     /* IN/OUT */
                                     , &l_th_height    /* IN/OUT */
                                     , &l_flip_status  /* OUT */
                                     , ckey            /* IN */
                                     , isBackwards
                                     );
      if(gpp->cancel_video_api)
      {
        if(l_th_data)
        {
          g_free(l_th_data); /* throw away undefined data in case of cancel */
          l_th_data = NULL;
        }
        if(gpp->progress_bar)
        {
           gtk_progress_bar_set_text(GTK_PROGRESS_BAR(gpp->progress_bar)
                        , _("Canceled"));
        }

        GAP_TIMM_STOP_FUNCTION(funcId);
        return;
      }

    }
    else
    {
      if(gpp->ainfo_ptr->frame_cnt > 0)
      {
        l_filename = gap_lib_alloc_fname(gpp->ainfo_ptr->basename, framenr, gpp->ainfo_ptr->extension);
      }
      else
      {
        /* if player operates on a single image we have to build
         * the filename just of basename or basename+extension part
         * (this rare case can occure when the player is invoked
         *  from a storyboard to playback a clip that is made up of a single image)
         */
        if(gpp->ainfo_ptr->extension)
        {
          l_filename = g_strdup_printf("%s%s"
                                      ,gpp->ainfo_ptr->basename
                                      ,gpp->ainfo_ptr->extension
                                      );
        }
        else
        {
          l_filename = g_strdup(gpp->ainfo_ptr->basename);
        }
      }
      if(gpp->imagename == NULL)
      {
        framenr_is_the_active_image = p_check_for_active_image(gpp, framenr);
      }
    }
  }


  /* --------------------- */


  if((l_filename == NULL)
  && (l_th_data == NULL)
  && (l_composite_image_id < 0))
  {
    /* frame not available, create black fully transparent buffer
     * ( renders as empty checkerboard)
     */
    l_th_bpp = 4;
    l_th_width = gpp->pv_width;
    l_th_height = gpp->pv_height;
    l_th_data_count = l_th_width * l_th_height * l_th_bpp;
    l_th_data = g_malloc0(l_th_data_count);
  }
  else
  {
    if(gpp->use_thumbnails)
    {
      if(framenr_is_the_active_image)
      {
         gint32 l_tmp_image_id;

         l_tmp_image_id = -1;

         /* check if automatic onionskin layer removal is turned on */
         if(gpp->onion_delete)
         {
           /* check if image has visible onionskin layers */
           if(gap_onion_image_has_oinonlayers(gpp->image_id, TRUE /* only visible*/))
           {
             l_tmp_image_id = gap_onion_base_image_duplicate(gpp->image_id);
           }
         }

         if(l_tmp_image_id == -1)
         {
           gap_pdb_gimp_image_thumbnail(gpp->image_id
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
           gap_onion_base_onionskin_delete(l_tmp_image_id);
           gap_pdb_gimp_image_thumbnail(l_tmp_image_id
                          , gpp->pv_width
                          , gpp->pv_height
                          , &l_th_width
                          , &l_th_height
                          , &l_th_bpp
                          , &l_th_data_count
                          , &l_th_data
                          );
           gimp_image_delete(l_tmp_image_id);
         }

      }
      else
      {
        /* init preferred width and height
         * (as hint for the thumbnail loader to decide
         *  if thumbnail is to fetch from normal or large thumbnail directory
         *  just for the case when both sizes are available)
         */
        l_th_width = gpp->pv_width;
        l_th_height = gpp->pv_height;

        pixbuf = gap_thumb_file_load_pixbuf_thumbnail(l_filename
                                      , &l_th_width, &l_th_height
                                      , &l_th_bpp
                                      );

      }
    }
  }

  if(l_composite_image_id >= 0)
  {
    p_render_display_free_image_id(gpp
         , l_composite_image_id
         , l_flip_request
         , l_flip_status
         );
  }
  else
  {
    if(pixbuf)
    {
      p_render_display_free_pixbuf(gpp, pixbuf, l_flip_request, l_flip_status);
    }
    else
    {
      if((l_th_data == NULL)
      && (!framenr_is_the_active_image)
      && (gpp->max_player_cache > 0))
      {
        ckey = gap_player_cache_new_image_key(l_filename);
        l_th_data = p_fetch_frame_via_cache(gpp
                       , ckey
                       , &l_th_bpp
                       , &l_th_width
                       , &l_th_height
                       , &l_flip_status
                       );
      }

      if (l_th_data)
      {
         p_render_display_free_thdata(gpp
            , l_th_data
            , l_th_width
            , l_th_height
            , l_th_bpp
            , l_flip_request
            , l_flip_status
            );
         l_th_data = NULL;
      }
      else
      {
        /* got no thumbnail data, must use the full image */
        p_render_display_from_active_image_or_file(gpp
            , l_filename
            , framenr_is_the_active_image
            , l_flip_request
            , l_flip_status
            );
      }

    }
  }


  if (ckey != NULL)
  {
    p_frame_chache_processing(gpp, ckey);
    g_free(ckey);
  }


  gpp->play_current_framenr = framenr;
  p_update_position_widgets(gpp);

  gdk_flush();

  if(l_th_data)  g_free(l_th_data);

  if(l_filename) g_free(l_filename);

  GAP_TIMM_STOP_FUNCTION(funcId);

}  /* end p_display_frame */

/* ------------------------------
 * p_get_next_framenr_in_sequence /2
 * ------------------------------
 */
gint32
p_get_next_framenr_in_sequence2(GapPlayerMainGlobalParams *gpp)
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
      p_audio_resync(gpp);
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
      p_audio_resync(gpp);
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
}  /* end p_get_next_framenr_in_sequence2 */

gint32
p_get_next_framenr_in_sequence(GapPlayerMainGlobalParams *gpp)
{
  gint32 framenr;
  framenr = p_get_next_framenr_in_sequence2(gpp);

  if((framenr < 0)
  || (gpp->play_backward)
  || (gpp->audio_resync > 0))
  {
    /* stop audio at end (-1) or when video plays revers or resync needed */
    p_audio_stop(gpp);

    /* RESYNC: break for N frames, then restart audio at sync position
     * (NOTE: ideally we should restart immediate without any break
     *  but restarting the playback often and very quickly leads to
     *  deadlock (in the audioserver or clent/server communication)
     */
    if(gpp->audio_resync > 0)
    {
      gpp->audio_resync--;
    }
  }
  else
  {
    /* conditional audio start
     * (can happen on any frame if audio offsets are used)
     */

#ifdef TRY_AUTO_SYNC_AT_FAST_PLAYBACKSPEED
    /* this codespart tries to keep audio playing sync at fast speed
     * but sounds not good, and does not work reliable
     * therefore it should not be compiled in public release version.
     * (asynchron audio that will not be able to follow up the videospeed
     *  will be the result in that case)
     */
    if (gpp->audio_required_samplerate > GAP_PLAYER_MAIN_MAX_SAMPLERATE)
    {
      gint32 nframes;
      /* required samplerate is faster than highest possible audioplayback speed
       * stop and restart at new offset at every n.th frame to follow the faster video
       * (this will result in glitches)
       */
      nframes = 8 * (1 + (gpp->speed / 5));
      if((framenr % nframes)  == 0)
      {
        printf("WARNING: Audiospeed cant follow up video speed nframes:%d\n", (int)nframes);
        p_audio_resync(gpp);  /* audioserver does not like tto much stop&go and hangs sometimes */
      }
    }
#endif
    p_audio_start_play(gpp);
  }
  return(framenr);
}  /*end p_get_next_framenr_in_sequence */



/* -----------------------------
 * p_step_frame
 * -----------------------------
 */
static void
p_step_frame(GapPlayerMainGlobalParams *gpp, gint stepsize)
{
  gint32 framenr;

  framenr = gpp->play_current_framenr + stepsize;
  if(gpp->ainfo_ptr)
  {
    framenr = CLAMP(framenr
                   , gpp->ainfo_ptr->first_frame_nr
                   , gpp->ainfo_ptr->last_frame_nr
                   );
    if(gpp->play_current_framenr != framenr)
    {
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

}  /* end p_step_frame */

/* ------------------------------
 * p_framenr_from_go_number
 * ------------------------------
 */
gint32
p_framenr_from_go_number(GapPlayerMainGlobalParams *gpp, gint32 go_number)
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
on_timer_playback(GapPlayerMainGlobalParams *gpp)
{
  gulong elapsed_microsecs;
  gdouble elapsed_secs;
  static char  status_txt[30];

  /*if(gap_debug) printf("\non_timer_playback: START\n");*/

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
      if(gap_debug) printf("\n\n\n  on_timer_playback interrupted by next TIMERCALL \n\n");
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
              * in this case we display that frame,
              * with minimal delay of 8 millisecs via the one-shot go_timer.
              */
             gpp->go_job_framenr = l_prev_framenr;
             if(gpp->go_timertag >= 0)
             {
               g_source_remove(gpp->go_timertag);
             }
             gpp->go_timertag = (gint32) g_timeout_add(8, (GtkFunction)on_timer_go_job, gpp);
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
             /* printf("DROP (SKIP) frame\n"); */
             gtk_label_set_text ( GTK_LABEL(gpp->status_label), _("Skip"));
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
on_timer_go_job(GapPlayerMainGlobalParams *gpp)
{
  /*if(gap_debug) printf("\non_timer_go_job: START\n");*/

  if(gpp)
  {

    if(gpp->go_timertag >= 0)
    {
      g_source_remove(gpp->go_timertag);
    }
    gpp->go_timertag = -1;

    if(gpp->go_job_framenr >= 0)
    {
      if(gpp->gva_lock)
      {
        /* the video api is still busy with fetching the previous frame
         * (do not disturb, but setup the go_timer for a next try
         * after 96 milliseconds)
         */
        gpp->go_timertag = (gint32) g_timeout_add(96, (GtkFunction)on_timer_go_job, gpp);

        /*if(gap_debug) printf("on_timer_go_job: TRY LATER (96msec) %06d\n", (int)gpp->go_job_framenr);*/
      }
      else
      {
        /*if(gap_debug) printf("on_timer_go_job: DISPLAY RENDER %06d\n", (int)gpp->go_job_framenr); */
        p_display_frame(gpp, gpp->go_job_framenr);
      }
    }

  }
}  /* end on_timer_go_job */

/* -----------------------------
 * on_play_button_clicked
 * -----------------------------
 */
static void
on_play_button_clicked(GtkButton *button
                     , GdkEventButton  *bevent
                     , GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  gpp->play_backward = FALSE;
  gpp->request_cancel_video_api = TRUE;

  if(bevent)
  {
    if(bevent->state & GDK_SHIFT_MASK)
    {
      gpp->mtrace_mode = GAP_PLAYER_MTRACE_IMG_SIZE;
    }
    if (bevent->state & GDK_CONTROL_MASK)
    {
      gpp->mtrace_mode = GAP_PLAYER_MTRACE_PV_SIZE;
    }
    if(bevent->state & GDK_MOD1_MASK)
    {
      gpp->mtrace_image_id = -1;  /* force creation of new image */
    }
  }

  if(gpp->play_is_active)
  {
    if (gpp->audio_required_samplerate > GAP_PLAYER_MAIN_MAX_SAMPLERATE)
    {
      p_audio_resync(gpp);
    }
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
                                    GapPlayerMainGlobalParams *gpp)
{
  gboolean         play_was_active;

  if(gpp == NULL)
  {
    return FALSE;
  }

  GAP_TIMM_PRINT_FUNCTION_STATISTICS();

  if(gpp->progress_bar)
  {
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(gpp->progress_bar)
                             ,gpp->progress_bar_idle_txt
                             );
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
on_back_button_clicked(GtkButton       *button
                     , GdkEventButton  *bevent
                     , GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  gpp->play_backward = TRUE;
  gpp->request_cancel_video_api = TRUE;

  if(bevent)
  {
    if(bevent->state & GDK_SHIFT_MASK)
    {
      gpp->mtrace_mode = GAP_PLAYER_MTRACE_IMG_SIZE;
    }
    if (bevent->state & GDK_CONTROL_MASK)
    {
      gpp->mtrace_mode = GAP_PLAYER_MTRACE_PV_SIZE;
    }
    if(bevent->state & GDK_MOD1_MASK)
    {
      gpp->mtrace_image_id = -1;  /* force creation of new image */
    }

  }

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
                                        GapPlayerMainGlobalParams *gpp)
{
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
  else
  {
    if(gpp->caller_range_linked)
    {
      p_printout_range(gpp, FALSE, FALSE);
    }
  }

}  /* end on_from_spinbutton_changed */



/* -----------------------------
 * on_to_spinbutton_changed
 * -----------------------------
 */
static void
on_to_spinbutton_changed               (GtkEditable     *editable,
                                        GapPlayerMainGlobalParams *gpp)
{
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
  else
  {
    if(gpp->caller_range_linked)
    {
      p_printout_range(gpp, FALSE, FALSE);
    }
  }

}  /* end on_to_spinbutton_changed */


/* -----------------------------
 * on_vid_preview_button_press_event
 * -----------------------------
 */
gboolean
on_vid_preview_button_press_event      (GtkWidget       *widget,
                                        GdkEventButton  *bevent,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gap_debug)
  {
    printf("on_vid_preview_button_press_event: START\n");
  }

  if(gpp->mtrace_mode == GAP_PLAYER_MTRACE_OFF)
  {
    gpp->mtrace_mode = GAP_PLAYER_MTRACE_IMG_SIZE;
    if (bevent->state & GDK_CONTROL_MASK)
    {
      gpp->mtrace_mode = GAP_PLAYER_MTRACE_PV_SIZE;
    }
    if(bevent->state & GDK_MOD1_MASK)
    {
      gpp->mtrace_image_id = -1;  /* force creation of new image */
    }
    p_display_frame(gpp, gpp->play_current_framenr);
    gpp->mtrace_mode = GAP_PLAYER_MTRACE_OFF;
  }

  return FALSE;

}  /* end on_vid_preview_button_press_event */


/* -----------------------------
 * on_warp_frame_scroll_event
 * -----------------------------
 * handle scroll event (inludes wheel mose)
 */
static gboolean
on_warp_frame_scroll_event      (GtkWidget       *widget,
                                 GdkEventScroll  *sevent,
                                 GapPlayerMainGlobalParams *gpp)
{
  /*if(gap_debug) printf("on_warp_frame_scroll_event: START\n"); */


  p_stop_playback(gpp);

  if((sevent->direction == GDK_SCROLL_UP)
  || (sevent->direction == GDK_SCROLL_RIGHT))
  {
    p_step_frame(gpp, -1);
  }
  else
  {
    p_step_frame(gpp, 1);
  }

  return FALSE;

}  /* end on_warp_frame_scroll_event */


/* -----------------------------
 * on_vid_preview_expose_event
 * -----------------------------
 */
gboolean
on_vid_preview_expose_event      (GtkWidget       *widget,
                                  GdkEventExpose  *eevent,
                                  GapPlayerMainGlobalParams *gpp)
{
  /*if(gap_debug) printf(" xxxxxxxx on_vid_preview_expose_event: START\n");*/

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
  gap_pview_repaint(gpp->pv_ptr);

  return FALSE;

}  /* end on_vid_preview_expose_event */

/* -----------------------------
 * on_shell_window_size_allocate
 * -----------------------------
 */
static void
on_shell_window_size_allocate            (GtkWidget       *widget,
                                         GtkAllocation   *allocation,
                                         GapPlayerMainGlobalParams *gpp)
{
   if((gpp == NULL) || (allocation == NULL))
   {
     return;
   }

   if(gap_debug) printf("on_shell_window_size_allocate: START  shell_alloc: w:%d h:%d \n"
                           , (int)allocation->width
                           , (int)allocation->height
                           );

   if(gpp->shell_initial_width < 0)
   {
     gpp->shell_initial_width = allocation->width;
     gpp->shell_initial_height = allocation->height;
     p_fit_initial_shell_window(gpp);  /* for setting window default size */
   }
   else
   {
     if((allocation->width < gpp->shell_initial_width)
     || (allocation->height < gpp->shell_initial_height))
     {
       /* dont allow shrink below initial size */
       p_fit_initial_shell_window(gpp);
     }
   }
}  /* end on_shell_window_size_allocate */

/* -----------------------------
 * on_vid_preview_size_allocate
 * -----------------------------
 * this procedure handles automatic
 * resize of the preview when the user resizes the window.
 * While sizechanges via the Size spinbutton this handler is usually
 * disconnected, and will be reconnected when the mouse leaves the shell window.
 * This handlerprocedure acts on the drawing_area widget of the preview.
 * Size changes of drawing_area are propagated to
 * the preview (aspect_frame and drawing_area) by calls to p_update_pviewsize.
 */
static void
on_vid_preview_size_allocate            (GtkWidget       *widget,
                                         GtkAllocation   *allocation,
                                         GapPlayerMainGlobalParams *gpp)
{
   gint32 allo_width;
   gint32 allo_height;

#define PV_BORDER_X 10
#define PV_BORDER_Y 10

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

   if(gap_debug) printf("  on_vid_preview_size_allocate: ORIGINAL pv_w:%d pv_h:%d handler_id:%d\n"
                           , (int)gpp->pv_ptr->pv_width
                           , (int)gpp->pv_ptr->pv_height
                           , (int)gpp->resize_handler_id
                           );


   allo_width = allocation->width;
   allo_height = allocation->height;

   /* react on significant changes (min 6 pixels) only) */
   if(((gpp->old_resize_width / 6) != (allo_width / 6))
   || ((gpp->old_resize_height / 6) != (allo_height / 6)))
   {
     gint32  pv_pixelsize;
     gboolean blocked;
     gdouble  img_ratio;
     gdouble  alloc_ratio;


     img_ratio = 1.0;
     blocked = FALSE;
     alloc_ratio = (gdouble)allo_width / (gdouble)allo_height;
     if(gpp->ainfo_ptr == NULL)
     {
       blocked = FALSE;
       pv_pixelsize = gpp->pv_pixelsize;
     }
     else
     {
        img_ratio   = (gdouble)gpp->ainfo_ptr->width / (gdouble)gpp->ainfo_ptr->height;
        if(img_ratio >= 1.0)
        {
           /* imageorientation is landscape */
           if(alloc_ratio <= img_ratio)
           {
             pv_pixelsize = CLAMP( (allo_width - PV_BORDER_X)
                       , GAP_PLAYER_MIN_SIZE
                       , GAP_PLAYER_MAX_SIZE);
           }
           else
           {
             pv_pixelsize = CLAMP( (((allo_height - PV_BORDER_Y) * gpp->ainfo_ptr->width) / gpp->ainfo_ptr->height)
                       , GAP_PLAYER_MIN_SIZE
                       , GAP_PLAYER_MAX_SIZE);
           }
        }
        else
        {
           /* imageorientation is portrait */
           if(alloc_ratio <= img_ratio)
           {
             pv_pixelsize = CLAMP( (((allo_width - PV_BORDER_X) * gpp->ainfo_ptr->height) / gpp->ainfo_ptr->width)
                       , GAP_PLAYER_MIN_SIZE
                       , GAP_PLAYER_MAX_SIZE);
           }
           else
           {
             pv_pixelsize = CLAMP( (allo_height - PV_BORDER_Y)
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
       && (allo_width < gpp->old_resize_width))
       {
         if(gap_debug) printf(" BLOCK preview grow on  width shrink\n");
         blocked = TRUE;
       }
       if ((alloc_ratio <= 1.0) /* portrait */
       && (allo_height < gpp->old_resize_height))
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
           p_display_frame(gpp, gpp->play_current_framenr);
         }
     }

     if(gap_debug) printf("  on_vid_preview_size_allocate: AFTER resize pv_w:%d pv_h:%d\n"
                           , (int)gpp->pv_ptr->pv_width
                           , (int)gpp->pv_ptr->pv_height
                           );
   }

   gpp->old_resize_width = allo_width;
   gpp->old_resize_height = allo_height;

   if(gap_debug) printf("  on_vid_preview_size_allocate: END\n");

}  /* end on_vid_preview_size_allocate */


/* -----------------------------
 * on_framenr_button_clicked
 * -----------------------------
 * SHIFT-click: goto frame
 * else:        (normal click or other modifier keys ...)
 *              set END   of range if inoked from the framenr_2_button widget
 *              set BEGIN of range if NOT inoked from the framenr_2_button widget
 */
static void
on_framenr_button_clicked             (GtkButton       *button,
                                       GdkEventButton  *bevent,
                                       GapPlayerMainGlobalParams *gpp)
{
   GimpParam       *return_vals;
   int              nreturn_vals;
   gint             button_type;
   gint32           dummy_layer_id;

  /*if(gap_debug) printf("on_framenr_button_clicked: START\n"); */

  if(gpp == NULL)
  {
    return;
  }

  p_stop_playback(gpp);

  if(button)
  {
    button_type = (gint) g_object_get_data (G_OBJECT (button), KEY_FRAMENR_BUTTON_TYPE);
  }
  else
  {
    button_type = FRAMENR_BUTTON_BEGIN;
  }

  if(bevent)
  {
    if (!(bevent->state & GDK_SHIFT_MASK))  /* normal Click */
    {
      /* for normal click and other modifiers other than SHIFT (GDK_CONTROL_MASK) */
      if(button_type == FRAMENR_BUTTON_END)
      {
        /* set END of the range */
        gpp->end_frame = gpp->play_current_framenr;
        gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->to_spinbutton_adj)
                                  , (gfloat)gpp->play_current_framenr
                                  );
        if(gpp->end_frame < gpp->begin_frame)
        {
          gpp->begin_frame = gpp->end_frame;
          gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->from_spinbutton_adj)
                                  , (gfloat)gpp->begin_frame
                                  );
        }
        return;
      }

      /* set BEGIN of the range */
      gpp->begin_frame = gpp->play_current_framenr;
      gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->from_spinbutton_adj)
                              , (gfloat)gpp->play_current_framenr
                              );
      if(gpp->begin_frame > gpp->end_frame)
      {
        gpp->end_frame = gpp->begin_frame;
        gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->to_spinbutton_adj)
                                , (gfloat)gpp->end_frame
                                );
      }
      return;

    }
  }

  if((gpp->stb_ptr)
  || (gpp->image_id < 0))
  {
    /* NOP in storyboard mode */
    return;
  }


  dummy_layer_id = gap_image_get_any_layer(gpp->image_id);
  return_vals = gimp_run_procedure ("plug_in_gap_goto",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32,    GIMP_RUN_NONINTERACTIVE,
                                    GIMP_PDB_IMAGE,    gpp->image_id,
                                    GIMP_PDB_DRAWABLE, dummy_layer_id,
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

    gimp_destroy_params(return_vals, nreturn_vals);
  }


}  /* end on_framenr_button_clicked */




/* -----------------------------
 * on_from_button_clicked
 * -----------------------------
 */
static void
on_from_button_clicked            (GtkButton       *button,
                                   GapPlayerMainGlobalParams *gpp)
{
  if(gpp)
  {
    gboolean printflag;
    printflag = TRUE;
    if(gpp->fptr_set_range)
    {
      printflag = FALSE;
    }

    p_printout_range(gpp, FALSE, printflag);
  }
}  /* end on_from_button_clicked */


/* -----------------------------
 * on_to_button_clicked
 * -----------------------------
 */
static void
on_to_button_clicked            (GtkButton       *button,
                                 GapPlayerMainGlobalParams *gpp)
{
  if(gpp)
  {
    gboolean printflag;
    printflag = TRUE;
    if(gpp->fptr_set_range)
    {
      printflag = FALSE;
    }
    p_printout_range(gpp, TRUE, printflag);
  }
}  /* end on_to_button_clicked */

/* -----------------------------
 * on_framenr_scale_clicked
 * -----------------------------
 */
static gboolean
on_framenr_scale_clicked        (GtkWidget        *widget,
                                 GdkEvent         *event,
                                 GapPlayerMainGlobalParams *gpp)
{
  if(gpp)
  {
    if(gap_debug) printf("SCALE CLICKED\n");
    if(gpp->play_is_active)
    {
      p_stop_playback(gpp);
    }
  }
  return(FALSE);  /* event not completely handled, continue calling other handlers */
}  /* end on_framenr_scale_clicked */

/* -----------------------------
 * on_framenr_spinbutton_changed
 * -----------------------------
 */
static void
on_framenr_spinbutton_changed          (GtkEditable     *editable,
                                        GapPlayerMainGlobalParams *gpp)
{
  gint32           framenr;

  if(gpp == NULL)
  {
    return;
  }
  framenr = (gint32)GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)->value;

  /* force redraw of the framenr_scale widget
   * that refers to the framenr_spinbutton_adj adjustment
   */
  gtk_widget_queue_draw(gpp->framenr_scale);

  if(gpp->play_current_framenr != framenr)
  {
    /* force adjustment to the integer value */
    GTK_ADJUSTMENT (gpp->framenr_spinbutton_adj)->value = framenr;
    gpp->play_current_framenr = framenr;
    if(gpp->play_is_active)
    {
      if(gpp->rest_secs < 0.2)
      {
        /* currently playing and next update is near,
         * just set the framenr and let the play_timer do the display render job
         */
        gpp->play_current_framenr = framenr;
      }
      else
      {
        /* currently playing, but next update not near
         * render display immediate
         */
        p_display_frame(gpp, framenr);
      }
    }
    else
    {
      /* currently not playing, setup a go_timer job to display framenr */
      if(gpp->go_timertag >= 0)
      {
        g_source_remove(gpp->go_timertag);
      }
      gpp->go_job_framenr = framenr;
      gpp->go_timertag = (gint32) g_timeout_add(8, (GtkFunction)on_timer_go_job, gpp);
    }
    p_audio_resync(gpp);       /* force audio resync */
  }

}  /* end on_framenr_spinbutton_changed */


/* -----------------------------
 * on_origspeed_button_clicked
 * -----------------------------
 */
static void
on_origspeed_button_clicked            (GtkButton       *button,
                                        GapPlayerMainGlobalParams *gpp)
{
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
  p_audio_resync(gpp);

}  /* end on_origspeed_button_clicked */


/* -----------------------------
 * on_speed_spinbutton_changed
 * -----------------------------
 */
static void
on_speed_spinbutton_changed            (GtkEditable     *editable,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  gpp->speed = GTK_ADJUSTMENT(gpp->speed_spinbutton_adj)->value;

  /* stop audio at speed changes
   * (audio will restart automatically at next frame with samplerate matching new speed)
   */
  p_audio_resync(gpp);

}  /* end on_speed_spinbutton_changed */


/* --------------------------
 * p_fit_initial_shell_window
 * --------------------------
 */
static void
p_fit_initial_shell_window(GapPlayerMainGlobalParams *gpp)
{
  gint width;
  gint height;

  if(gpp == NULL)                   { return; }
  if(gpp->shell_initial_width < 0)  { return; }
  if(gpp->shell_window == NULL)     { return; }

  width = gpp->shell_initial_width;
  height = gpp->shell_initial_height;

  gtk_widget_set_size_request (gpp->shell_window, width, height);  /* shrink shell window */
  gtk_window_set_default_size(GTK_WINDOW(gpp->shell_window), width, height);  /* shrink shell window */
  gtk_window_resize (GTK_WINDOW(gpp->shell_window), width, height);  /* shrink shell window */
}  /* end p_fit_initial_shell_window */

/* ---------------------
 * p_fit_shell_window
 * ---------------------
 */
static void
p_fit_shell_window(GapPlayerMainGlobalParams *gpp)
{
  gint width;
  gint height;

  if(gpp->shell_window == NULL)     { return; }
  if((gpp->pv_ptr->pv_width <= GAP_STANDARD_PREVIEW_SIZE)
  && (gpp->pv_ptr->pv_height <= GAP_STANDARD_PREVIEW_SIZE))
  {
    p_fit_initial_shell_window(gpp);
    return;
  }

  /* FIXME: use preview size plus fix offsets (the offsets are just a guess
   * and may be too small for other languages and/or fonts
   */
  width =  MAX(gpp->pv_ptr->pv_width, GAP_STANDARD_PREVIEW_SIZE) + 272;
#ifdef GAP_ENABLE_AUDIO_SUPPORT
  height = MAX(gpp->pv_ptr->pv_height, GAP_STANDARD_PREVIEW_SIZE) + 178;
#else
  height = MAX(gpp->pv_ptr->pv_height, GAP_STANDARD_PREVIEW_SIZE) + 128;
#endif

  gtk_window_set_default_size(GTK_WINDOW(gpp->shell_window), width, height);  /* shrink shell window */
  gtk_window_resize (GTK_WINDOW(gpp->shell_window), width, height);  /* resize shell window */
}  /* end p_fit_shell_window */

/* -----------------------------
 * on_size_button_button_press_event
 * -----------------------------
 */
static gboolean
on_size_button_button_press_event  (GtkWidget       *widget,
                                    GdkEventButton  *bevent,
                                    GapPlayerMainGlobalParams *gpp)
{
  gboolean fit_initial_flag;


  if(gap_debug) printf("\nON_SIZE_BUTTON_BUTTON_PRESS_EVENT START\n");

  if(gpp == NULL)
  {
    return FALSE;
  }

  p_disconnect_resize_handler(gpp);
  fit_initial_flag = TRUE;

  if ((bevent->state & GDK_SHIFT_MASK)
  &&  (bevent->type == GDK_BUTTON_PRESS)
  &&  (gpp->ainfo_ptr))
  {
    if(gap_debug) printf("GDK_SHIFT_MASK !!\n\n");
    gpp->pv_pixelsize = CLAMP(MAX(gpp->ainfo_ptr->width, gpp->ainfo_ptr->height)
                       , GAP_PLAYER_MIN_SIZE
                       , GAP_PLAYER_MAX_SIZE);
    fit_initial_flag = FALSE;
  }
  else
  {
    /* toggle between normal and large thumbnail size */
    if(gpp->pv_pixelsize == GAP_STANDARD_PREVIEW_SIZE)
    {
      gpp->pv_pixelsize = GAP_SMALL_PREVIEW_SIZE;
    }
    else
    {
      gpp->pv_pixelsize = GAP_STANDARD_PREVIEW_SIZE;
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


  if(fit_initial_flag)
  {
    p_fit_initial_shell_window(gpp);
  }
  else
  {
    p_fit_shell_window(gpp);
  }

  p_connect_resize_handler(gpp);

  return FALSE;
}  /* end on_size_button_button_press_event */


/* -----------------------------
 * on_size_spinbutton_changed
 * -----------------------------
 */
static void
on_size_spinbutton_changed             (GtkEditable     *editable,
                                       GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  if(gap_debug)
  {
     printf("on_size_spinbutton_changed: value: %d  pv_pixelsize: %d\n"
           , (int)GTK_ADJUSTMENT(gpp->size_spinbutton_adj)->value
           ,(int)gpp->pv_pixelsize
           );
  }

  if(gpp->pv_pixelsize != (gint32)GTK_ADJUSTMENT(gpp->size_spinbutton_adj)->value)
  {
    gpp->pv_pixelsize = (gint32)GTK_ADJUSTMENT(gpp->size_spinbutton_adj)->value;

    p_update_pviewsize(gpp);

    if(!gpp->play_is_active)
    {
      p_display_frame(gpp, gpp->play_current_framenr);
    }

    p_fit_shell_window(gpp);
  }
}  /* end on_size_spinbutton_changed */


/* -----------------------------
 * on_size_spinbutton_enter
 * -----------------------------
 */
static gboolean   on_size_spinbutton_enter               (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      GapPlayerMainGlobalParams *gpp)
{
  /*if(gap_debug) printf("\n on_size_spinbutton_enter START\n");*/

  if(gpp)
  {
    p_disconnect_resize_handler(gpp);
  }
  return FALSE;
}  /* end on_size_spinbutton_enter */


/* -----------------------------
 * on_shell_window_leave
 * -----------------------------
 */
static gboolean   on_shell_window_leave               (GtkWidget        *widget,
                                                      GdkEvent         *event,
                                                      GapPlayerMainGlobalParams *gpp)
{
  /*if(gap_debug) printf("\n on_shell_window_leave START\n");*/

  if(gpp)
  {
    p_connect_resize_handler(gpp);
  }
  return FALSE;
}  /* end on_shell_window_leave */




/* -----------------------------
 * on_exact_timing_checkbutton_toggled
 * -----------------------------
 */
static void
on_exact_timing_checkbutton_toggled    (GtkToggleButton *togglebutton,
                                        GapPlayerMainGlobalParams *gpp)
{
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
                                        GapPlayerMainGlobalParams *gpp)
{
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
                                        GapPlayerMainGlobalParams *gpp)
{
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
                                        GapPlayerMainGlobalParams *gpp)
{
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
                                        GapPlayerMainGlobalParams *gpp)
{
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


/* -------------------------------------
 * on_show_gobuttons_checkbutton_toggled
 * -------------------------------------
 */
static void
on_show_gobuttons_checkbutton_toggled (GtkToggleButton *togglebutton,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  if (togglebutton->active)
  {
       gpp->show_go_buttons = TRUE;
       gtk_widget_show(gpp->gobutton_hbox);
  }
  else
  {
       gpp->show_go_buttons = FALSE;
       gtk_widget_hide(gpp->gobutton_hbox);
  }

}  /* end on_show_gobuttons_checkbutton_toggled */


/* -----------------------------------------
 * on_show_positionscale_checkbutton_toggled
 * -----------------------------------------
 */
static void
on_show_positionscale_checkbutton_toggled(GtkToggleButton *togglebutton,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  if (togglebutton->active)
  {
       gpp->show_position_scale = TRUE;
       gtk_widget_show(gpp->frame_scale_hbox);
  }
  else
  {
       gpp->show_position_scale = FALSE;
       gtk_widget_hide(gpp->frame_scale_hbox);
  }

}  /* end on_show_positionscale_checkbutton_toggled */


/* ---------------------------------------------
 * on_enable_detail_tracking_checkbutton_toggled
 * ---------------------------------------------
 */
static void
on_enable_detail_tracking_checkbutton_toggled(GtkToggleButton *togglebutton,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  if (togglebutton->active)
  {
       gpp->enableDetailTracking = TRUE;
  }
  else
  {
       gpp->enableDetailTracking = FALSE;
  }

}  /* end on_enable_detail_tracking_checkbutton_toggled */

/* ---------------------------------
 * on_detail_tracking_button_clicked
 * ---------------------------------
 */
static void
on_detail_tracking_button_clicked (GtkButton       *button,
                                   GapPlayerMainGlobalParams *gpp)
{
  gboolean dtailTrackingOk;
  if(gpp == NULL)
  {
    return;
  }
  p_stop_playback(gpp);
  if(gpp->gva_lock)
  {
    gpp->request_cancel_video_api = TRUE;
    return;
  }

  dtailTrackingOk =
    gap_detail_tracking_dialog_cfg_set_vals(gpp->mtrace_image_id);
  
  if(dtailTrackingOk)
  {
    gpp->enableDetailTracking = TRUE;
    if(gpp->detail_tracking_checkbutton)
    {
      gtk_toggle_button_set_active (
         GTK_TOGGLE_BUTTON (gpp->detail_tracking_checkbutton), TRUE);
    }
  }

}  /* end on_detail_tracking_button_clicked */




/* -----------------------------
 * on_close_button_clicked
 * -----------------------------
 */
static void
on_close_button_clicked                (GtkButton       *button,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }
  p_stop_playback(gpp);
  if(gpp->gva_lock)
  {
    gpp->request_cancel_video_api = TRUE;
    return;
  }

  if(gpp->audio_tmp_dialog_is_open)
  {
    /* ignore close as long as sub dialog is open */
    return;
  }

  gtk_widget_destroy (GTK_WIDGET (gpp->shell_window));  /* close & destroy dialog window */
  if(gpp->standalone_mode)
  {
    gtk_main_quit ();
  }

}  /* end on_close_button_clicked */


/* -----------------------------
 * on_shell_window_destroy
 * -----------------------------
 */
static void
on_shell_window_destroy                (GtkObject       *object,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp)
  {
    p_stop_playback(gpp);
  }

  gpp->shell_window = NULL;
  p_close_videofile(gpp);
  p_close_composite_storyboard(gpp);

  if(gpp->standalone_mode)
  {
    gtk_main_quit ();
  }

}  /* end on_shell_window_destroy */



/* -----------------------------
 * on_go_button_clicked
 * -----------------------------
 */
static void
on_go_button_clicked                   (GtkButton       *button,
                                        GdkEventButton  *bevent,
                                        t_gobutton *gob)
{
   GapPlayerMainGlobalParams *gpp;
   gint32  framenr;

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

   if(bevent->type == GDK_BUTTON_PRESS)
   {
     if(bevent->state & GDK_CONTROL_MASK)
     {
       gpp->begin_frame = framenr;
       gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->from_spinbutton_adj)
                               , (gfloat)gpp->begin_frame
                               );
       if(gpp->begin_frame > gpp->end_frame)
       {
         gpp->end_frame = gpp->begin_frame;
         gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->to_spinbutton_adj)
                                 , (gfloat)gpp->end_frame
                                 );
       }
       return;
     }
     if(bevent->state & GDK_MOD1_MASK)  /* ALT modifier Key */
     {
       gpp->end_frame = framenr;
       gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->to_spinbutton_adj)
                                 , (gfloat)gpp->end_frame
                                 );
       if(gpp->end_frame < gpp->begin_frame)
       {
         gpp->begin_frame = gpp->end_frame;
         gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->from_spinbutton_adj)
                                 , (gfloat)gpp->begin_frame
                                 );
       }
       return;
     }
   }

   if(framenr != gpp->ainfo_ptr->curr_frame_nr)
   {
     on_framenr_button_clicked(NULL, NULL, gpp);
   }
}  /* end on_go_button_clicked */


/* -----------------------------
 * on_go_button_enter
 * -----------------------------
 */
static gboolean
on_go_button_enter                   (GtkButton       *button,
                                        GdkEvent      *event,
                                        t_gobutton *gob)
{
   GapPlayerMainGlobalParams *gpp;
   gint32  framenr;

   if(gob == NULL) { return FALSE; }

   gpp = gob->gpp;
   if(gpp == NULL) { return FALSE; }

   /*if (gap_debug) printf("ON_GO_BUTTON_ENTER: go_number:%d\n", (int)gob->go_number ); */

   p_stop_playback(gpp);

   /* skip display on full sized image mode
    * (gui cant follow that fast on many machines, and would react slow)
    */
   /*if(gpp->use_thumbnails) */
   {
      framenr = p_framenr_from_go_number(gpp, gob->go_number);
      if(gpp->play_current_framenr != framenr)
      {
          if(gap_debug)
          {
             if(gpp->go_timertag >= 0)
             {
               if(gap_debug) printf("on_go_button_enter: DROP GO_FRAMENR: %d\n", (int)gpp->go_job_framenr);
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
                                        GapPlayerMainGlobalParams *gpp)
{
  /*if (gap_debug) printf("ON_GOBUTTON_HBOX_LEAVE\n");*/

  if(gpp == NULL)
  {
    return;
  }

  /* reset go_base */
  gpp->go_base_framenr = -1;
  gpp->go_base = -1;

  p_check_tooltips();

}  /* end on_gobutton_hbox_leave */


/* -----------------------------------
 * on_audio_enable_checkbutton_toggled
 * -----------------------------------
 */
static void
on_audio_enable_checkbutton_toggled    (GtkToggleButton *togglebutton,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  if (togglebutton->active)
  {
       gpp->audio_enable = TRUE;
       /* audio will start automatic at next frame when playback is running */
  }
  else
  {
       gpp->audio_enable = FALSE;
       gpp->audio_resync = 0;
       p_audio_stop(gpp);
  }

}  /* end on_audio_enable_checkbutton_toggled */


/* -----------------------------
 * on_audio_volume_spinbutton_changed
 * -----------------------------
 */
static void
on_audio_volume_spinbutton_changed     (GtkEditable     *editable,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  if(gpp->audio_volume != (gdouble)GTK_ADJUSTMENT(gpp->audio_volume_spinbutton_adj)->value)
  {
    gpp->audio_volume = (gdouble)GTK_ADJUSTMENT(gpp->audio_volume_spinbutton_adj)->value;

#ifdef GAP_ENABLE_AUDIO_SUPPORT
    if(gpp->audio_status >= GAP_PLAYER_MAIN_AUSTAT_PLAYING)
    {
      apcl_volume(gpp->audio_volume, 0, p_audio_errfunc);
    }
#endif
  }

}  /* end on_audio_volume_spinbutton_changed */


/* -----------------------------------------
 * on_audio_frame_offset_spinbutton_changed
 * -----------------------------------------
 */
static void
on_audio_frame_offset_spinbutton_changed (GtkEditable     *editable,
                                        GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  if(gpp->audio_frame_offset != (gint32)GTK_ADJUSTMENT(gpp->audio_frame_offset_spinbutton_adj)->value)
  {
    gpp->audio_frame_offset = (gint32)GTK_ADJUSTMENT(gpp->audio_frame_offset_spinbutton_adj)->value;
    /* resync audio will cause an automatic restart respecting te new audio_offset value
     */
    p_audio_resync(gpp);
    p_audio_print_labels(gpp);
  }

}  /* end on_audio_frame_offset_spinbutton_changed */

/* -----------------------------------------
 * on_audio_reset_button_clicked
 * -----------------------------------------
 */
static void
on_audio_reset_button_clicked (GtkButton       *button,
                               GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }

  gpp->audio_frame_offset = 0;
  gpp->audio_volume = 1.0;
  gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->audio_frame_offset_spinbutton_adj)
                            , (gfloat)gpp->audio_frame_offset
                            );
  gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->audio_volume_spinbutton_adj)
                            , (gfloat)gpp->audio_volume
                            );

  /* resync audio (for the case its already playing this
   * will cause an automatic restart respecting te new audio_offset value
   */
  p_audio_resync(gpp);
  p_audio_print_labels(gpp);
}  /* end on_audio_reset_button_clicked */


/* -----------------------------------------
 * on_audio_create_copy_button_clicked
 * -----------------------------------------
 */
static void
on_audio_create_copy_button_clicked (GtkButton       *button,
                               GapPlayerMainGlobalParams *gpp)
{
#ifdef GAP_ENABLE_AUDIO_SUPPORT
  const char *cp;
  char  *envAUDIOCONVERT_TO_WAV;
  gboolean script_found;
  gboolean use_newly_created_wavfile;

  if(gpp == NULL)
  {
    return;
  }
  if(gpp->audio_tmp_dialog_is_open)
  {
    return;
  }

  script_found = FALSE;
  use_newly_created_wavfile = FALSE;
  envAUDIOCONVERT_TO_WAV = g_strdup(" ");
  /* check gimprc for the audioconvert_program */
  if ( (cp = gimp_gimprc_query("audioconvert_program")) != NULL )
  {
    g_free(envAUDIOCONVERT_TO_WAV);
    envAUDIOCONVERT_TO_WAV = g_strdup(cp);
    if(g_file_test (envAUDIOCONVERT_TO_WAV, G_FILE_TEST_IS_EXECUTABLE) )
    {
      script_found = TRUE;
    }
    else
    {
      g_message(_("WARNING: Your gimprc file configuration for the audioconverter script\n"
             "does not point to an executable program\n"
             "the configured value for %s is: %s\n")
             , "audioconvert_program"
             , envAUDIOCONVERT_TO_WAV
             );
    }
  }

  /* check environment variable for the audioconvert_program */
  if(!script_found)
  {
    if ( (cp = g_getenv("AUDIOCONVERT_TO_WAV")) != NULL )
    {
      g_free(envAUDIOCONVERT_TO_WAV);
      envAUDIOCONVERT_TO_WAV = g_strdup(cp);            /* Environment overrides compiled in default for WAVPLAYPATH */
      if(g_file_test (env_WAVPLAYPATH, G_FILE_TEST_IS_EXECUTABLE) )
      {
        script_found = TRUE;
      }
      else
      {
        g_message(_("WARNING: The environment variable %s\n"
               "does not point to an executable program\n"
               "the current value is: %s\n")
               , "AUDIOCONVERT_TO_WAV"
               , envAUDIOCONVERT_TO_WAV
               );
      }
    }
  }

  if(!script_found)
  {
      g_free(envAUDIOCONVERT_TO_WAV);
      envAUDIOCONVERT_TO_WAV = g_build_filename(GAPLIBDIR
                                              , "audioconvert_to_wav.sh"
                                              , NULL
                                              );
      if(!g_file_test(envAUDIOCONVERT_TO_WAV, G_FILE_TEST_IS_EXECUTABLE))
      {
        g_message(_("ERROR: The external program for audioconversion is not executable.\n"
                    "Filename: '%s'\n")
                 , envAUDIOCONVERT_TO_WAV
                 );
        return;
      }
      script_found = TRUE;
  }

  gpp->audio_tmp_dialog_is_open  = TRUE;
  if(p_create_wav_dialog(gpp))
  {
      gint    l_rc;
      gchar  *l_cmd;
      gchar  *l_resample_params;

      printf("CREATE WAVFILE as %s\n"
           "  in progress ***\n"
           ,gpp->audio_wavfile_tmp );

      gtk_label_set_text ( GTK_LABEL(gpp->audio_status_label), _("Creating audiofile - please wait"));
      gtk_widget_hide(gpp->audio_table);
      gtk_widget_show(gpp->audio_status_label);
      while (gtk_events_pending ())
      {
        gtk_main_iteration ();
      }

      if(gpp->audio_tmp_resample)
      {
        l_resample_params = g_strdup_printf("--resample %d"
                                           , (int)gpp->audio_tmp_samplerate
                                           );
      }
      else
      {
        l_resample_params = g_strdup(" ");  /* do not resample */
      }
      if(g_file_test(gpp->audio_wavfile_tmp, G_FILE_TEST_EXISTS))
      {
        g_remove(gpp->audio_wavfile_tmp);
      }

      l_cmd = g_strdup_printf("%s --in \"%s\" --out \"%s\" %s"
                             , envAUDIOCONVERT_TO_WAV
                             , gpp->audio_filename
                             , gpp->audio_wavfile_tmp
                             , l_resample_params
                             );


      /* CALL the external audioconverter Program */
      l_rc =  system(l_cmd);
      if (l_rc != 0)
      {
        printf("extern audioconverter FAILED. cmd:%s\n", l_cmd);
        g_message(_("external audioconverter FAILED."));
        p_msg_progress_bar_audio(gpp, _("extern audioconverter FAILED"));
      }

      g_free(l_cmd);
      g_free(l_resample_params);
      

      /* if the external converter created the wavfile
       * then use the newly created wavfile
       */
      if(g_file_test(gpp->audio_wavfile_tmp, G_FILE_TEST_EXISTS))
      {
        use_newly_created_wavfile = TRUE;
      }
  }

  g_free(envAUDIOCONVERT_TO_WAV);
  gpp->audio_tmp_dialog_is_open = FALSE;
  gtk_label_set_text ( GTK_LABEL(gpp->audio_status_label), " ");
  gtk_widget_hide(gpp->audio_status_label);
  gtk_widget_show(gpp->audio_table);

  if(use_newly_created_wavfile)
  {
    gtk_entry_set_text(GTK_ENTRY(gpp->audio_filename_entry), gpp->audio_wavfile_tmp);
  }

#endif
return;
}  /* end on_audio_create_copy_button_clicked */

/* -----------------------------------------
 * on_audio_filename_entry_changed
 * -----------------------------------------
 */
static void
on_audio_filename_entry_changed (GtkWidget     *widget,
                                 GapPlayerMainGlobalParams *gpp)
{
  gboolean has_changed;
  
  if(gpp == NULL)
  {
    return;
  }
  has_changed = TRUE;
  if (gpp->audio_filename)
  {
     if(gap_debug)
     {
       printf("\non_audio_filename_entry_changed\n");
       printf(" ()) gpp->audio_filename:%s\n"
            , gpp->audio_filename);
       printf(" ()) gtk_entry_get_text:%s\n"
            , gtk_entry_get_text(GTK_ENTRY(gpp->audio_filename_entry)));
     }
     
    if (gtk_entry_get_text(GTK_ENTRY(gpp->audio_filename_entry)) != NULL)
    {
      if (strcmp(gpp->audio_filename, gtk_entry_get_text(GTK_ENTRY(gpp->audio_filename_entry))) == 0)
      {
        has_changed = FALSE;
      }
    }
  }
  
  g_snprintf(gpp->audio_filename, sizeof(gpp->audio_filename), "%s"
            , gtk_entry_get_text(GTK_ENTRY(gpp->audio_filename_entry))
             );

  p_audio_filename_changed(gpp);

  if(has_changed == TRUE)
  {
    p_reset_progress_bar_audio(gpp);
  }
}  /* end on_audio_filename_entry_changed */


/* --------------------------
 * AUDIO_FILESEL
 * --------------------------
 */
static void
on_audio_filesel_close_cb(GtkWidget *widget, GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }
  if(gpp->audio_filesel == NULL)
  {
    gtk_window_present(GTK_WINDOW(gpp->audio_filesel));
    return;  /* filesel is already closed */
  }

  gtk_widget_destroy(GTK_WIDGET(gpp->audio_filesel));
  gpp->audio_filesel = NULL;
}  /* end on_audio_filesel_close_cb */

static void
on_audio_filesel_ok_cb(GtkWidget *widget, GapPlayerMainGlobalParams *gpp)
{
  const gchar *filename;

  if(gpp == NULL)
  {
    return;
  }

  if(gpp->audio_filesel == NULL)
  {
    return;  /* filesel is already closed */
  }

  filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (gpp->audio_filesel));
  if(filename)
  {
    if(*filename != '\0')
    {
      gtk_entry_set_text(GTK_ENTRY(gpp->audio_filename_entry), filename);
    }
  }

  on_audio_filesel_close_cb(widget, gpp);
}  /* end on_audio_filesel_ok_cb */


/* -----------------------------------------
 * on_audio_filesel_button_clicked
 * -----------------------------------------
 */
static void
on_audio_filesel_button_clicked (GtkButton       *button,
                               GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }
  if(gpp->audio_filesel)
  {
    gtk_window_present(GTK_WINDOW(gpp->audio_filesel));
    return;  /* filesection dialog is already open */
  }

  gpp->audio_filesel = gtk_file_selection_new (_("Select Audiofile"));

  gtk_window_set_position (GTK_WINDOW (gpp->audio_filesel), GTK_WIN_POS_MOUSE);

  gtk_file_selection_set_filename (GTK_FILE_SELECTION (gpp->audio_filesel),
                                   gpp->audio_filename);
  gtk_widget_show (gpp->audio_filesel);

  g_signal_connect (G_OBJECT (gpp->audio_filesel), "destroy",
                    G_CALLBACK (on_audio_filesel_close_cb),
                    gpp);

  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (gpp->audio_filesel)->ok_button),
                   "clicked",
                    G_CALLBACK (on_audio_filesel_ok_cb),
                    gpp);
  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (gpp->audio_filesel)->cancel_button),
                   "clicked",
                    G_CALLBACK (on_audio_filesel_close_cb),
                    gpp);

}  /* end on_audio_filesel_button_clicked */



/* -----------------------------
 * p_new_audioframe
 * -----------------------------
 * create widgets for the audio options
 */
static GtkWidget *
p_new_audioframe(GapPlayerMainGlobalParams *gpp)
{
  GtkWidget *frame0a;
  GtkWidget *hseparator;
  GtkWidget *label;
  GtkWidget *vbox1;
  GtkWidget *table1;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *check_button;
  GtkWidget *spinbutton;
  GtkWidget *progress_bar;
  GtkObject *adj;
  gint       row;

  if (gap_debug) printf("p_new_audioframe\n");

  frame0a = gimp_frame_new (_("Audio Playback Settings"));

  /* the vbox */
  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (frame0a), vbox1);

  /* table */
  table1 = gtk_table_new (14, 3, FALSE);
  gpp->audio_table = table1;
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 4);

  /* status label */
  label = gtk_label_new(" ");
  gpp->audio_status_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox1), label, TRUE, TRUE, 0);
  gtk_widget_hide(label);

  row = 0;

  /* audiofile label */
  label = gtk_label_new (_("Audiofile:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1,  row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  /*gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);*/     /* right alligned */
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);        /* left alligned */

  /* audiofile entry */
  entry = gtk_entry_new();
  gpp->audio_filename_entry = entry;
  gtk_widget_show (entry);
  gimp_help_set_help_data(entry, _("Enter an audiofile. The file must be in RIFF WAVE fileformat."),NULL);
  if(gpp->docking_container)
  {
    gtk_widget_set_size_request(entry, GAP_PLAY_AUDIO_ENTRY_WIDTH_DOCKED, -1);
  }
  else
  {
    gtk_widget_set_size_request(entry, GAP_PLAY_AUDIO_ENTRY_WIDTH, -1);
  }
  gtk_entry_set_text(GTK_ENTRY(entry), gpp->audio_filename);
  gtk_table_attach(GTK_TABLE(table1), entry, 1, 2, row, row + 1,
                    (GtkAttachOptions) GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                    (GtkAttachOptions) GTK_FILL, 4, 0);
  g_signal_connect(G_OBJECT(entry), "changed",
                     G_CALLBACK (on_audio_filename_entry_changed),
                     gpp);


  /* audiofile button (fileselect invoker) */
  button = gtk_button_new_with_label ( "...");
  gtk_widget_show (button);
  gimp_help_set_help_data(button, _("Open audiofile selection browser dialog window"),NULL);
  gtk_table_attach(GTK_TABLE(table1), button, 2, 3, row, row + 1,
                    (GtkAttachOptions) GTK_FILL,
                    (GtkAttachOptions) GTK_FILL, 4, 0);
  g_signal_connect (G_OBJECT (button), "pressed",
                      G_CALLBACK (on_audio_filesel_button_clicked),
                      gpp);

  row++;


  /* Volume */
  label = gtk_label_new(_("Volume:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* volume spinutton */
  spinbutton = gimp_spin_button_new (&adj,  /* return value */
                      gpp->audio_volume,     /*   initial_val */
                      0.0,   /* umin */
                      1.0,   /* umax */
                      0.01,  /* sstep */
                      0.1,   /* pagestep */
                      0.0,                 /* page_size */
                      0.1,                 /* climb_rate */
                      2                    /* digits */
                      );
  gtk_widget_show (spinbutton);
  /*gtk_widget_set_sensitive(spinbutton, FALSE);*/  /* VOLUME CONTROL NOT IMPLEMENTED YET !!! */
  gpp->audio_volume_spinbutton_adj = adj;
  gtk_table_attach(GTK_TABLE(table1), spinbutton, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 4, 0);
  gimp_help_set_help_data(spinbutton, _("Audio Volume"),NULL);
  g_signal_connect (G_OBJECT (gpp->audio_volume_spinbutton_adj), "value_changed",
                      G_CALLBACK (on_audio_volume_spinbutton_changed),
                      gpp);

  /* check button */
  check_button = gtk_check_button_new_with_label (_("Enable"));
  gpp->audio_enable_checkbutton = check_button;
  gtk_table_attach ( GTK_TABLE (table1), check_button, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                gpp->audio_enable);
  gimp_help_set_help_data(check_button, _("ON: Play button plays video + audio.\n"
                                          "OFF: Play video silently"),NULL);
  gtk_widget_show (check_button);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                      G_CALLBACK (on_audio_enable_checkbutton_toggled),
                      gpp);

  row++;

  /* Sample Offset */
  label = gtk_label_new(_("Offset:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* offset spinutton */
  spinbutton = gimp_spin_button_new (&adj,  /* return value */
                      gpp->audio_frame_offset,    /*   initial_val */
                     -500000,   /* umin */
                      500000,   /* umax */
                      1.0,  /* sstep */
                      100,   /* pagestep */
                      0,                   /* page_size */
                      1,                   /* climb_rate */
                      0                    /* digits */
                      );
  gtk_widget_show (spinbutton);
  gpp->audio_frame_offset_spinbutton_adj = adj;
  gtk_table_attach(GTK_TABLE(table1), spinbutton, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 4, 0);
  gimp_help_set_help_data(spinbutton
                         , _("Audio offset in frames at original video playback speed. "
                             "A value of 0 starts audio and video at synchron time. "
                             "A value of -10 will play frame 1 up to frame 9 silently "
                             "and start audio at frame 10. "
                             "A value of 10 starts audio at frame 1, "
                             "but skips the audio begin part in a length that is "
                             "equal to the duration of 10 frames "
                             "at original video playback speed.")
                         ,NULL);
  g_signal_connect (G_OBJECT (gpp->audio_frame_offset_spinbutton_adj), "value_changed",
                      G_CALLBACK (on_audio_frame_offset_spinbutton_changed),
                      gpp);


  /* check button */
  check_button = gtk_check_button_new_with_label (_("original audio"));
  gpp->audio_auto_offset_by_framenr_checkbutton = check_button;
  gtk_table_attach ( GTK_TABLE (table1), check_button, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                gpp->audio_auto_offset_by_framenr);
  gimp_help_set_help_data(check_button, _("ON: for video clip playback sync audio startposition according"
                                          " to original position in the referenced video."
                                          " Use this option if the audiofile is an extracted audiotrack"
                                          " of the full referenced videofile\n"
                                          "OFF: do not sync audio with original position in the referenced videos."
                                          " Use this for independent audiofile playback.")
                                          ,NULL);
  gtk_widget_show (check_button);
  g_signal_connect (G_OBJECT (check_button), "toggled",
                      G_CALLBACK (on_audio_auto_offset_checkbutton_toggled),
                      gpp);


  row++;

  /* Sample Offset */
  label = gtk_label_new(_("Audiotrack:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* create the audio_otone_atrack_spinbutton spinutton */
  spinbutton = gimp_spin_button_new (&adj,  /* return value */
                      1,     /*   initial_val */
                      0.0,   /* umin */
                      10.0,  /* umax */
                      1.0,   /* sstep */
                      1.0,   /* pagestep */
                      0.0,                 /* page_size */
                      1.0,                 /* climb_rate */
                      0                    /* digits */
                      );
  gtk_widget_show (spinbutton);
  gpp->audio_otone_atrack_spinbutton_adj = adj;
  gpp->audio_otone_atrack_spinbutton = spinbutton;
  gtk_table_attach(GTK_TABLE(table1), spinbutton, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 4, 0);
  gimp_help_set_help_data(spinbutton, _("Audio Track"),NULL);
  g_signal_connect (G_OBJECT (gpp->audio_volume_spinbutton_adj), "value_changed",
                      G_CALLBACK (on_audio_otone_atrack_spinbutton_changed),
                      gpp);


  /* create extract audio otone track button */
  button = gtk_button_new_with_label(_("Extract Audio"));
  gpp->audio_otone_extract_button = button;
  gtk_widget_show (button);
  gimp_help_set_help_data(button, _("Extract Audio Track from videofile "
                                    "for the current videofile and use it for origial audiotrack playback")
                                    ,NULL);
  gtk_table_attach(GTK_TABLE(table1), button, 2, 3, row, row + 1,
                    (GtkAttachOptions) GTK_FILL,
                    (GtkAttachOptions) GTK_FILL, 4, 0);
  g_signal_connect (G_OBJECT (button), "pressed",
                      G_CALLBACK (on_audio_otone_extract_button_clicked),
                      gpp);


  row++;

  /* create wavfile button */
  button = gtk_button_new_with_label(_("Copy As Wavfile"));
  gtk_widget_show (button);
  gimp_help_set_help_data(button, _("Create a copy from audiofile as RIFF WAVE audiofile "
                                    "and use the copy for audio playback"),NULL);
  gtk_table_attach(GTK_TABLE(table1), button, 1, 2, row, row + 1,
                    (GtkAttachOptions) GTK_FILL,
                    (GtkAttachOptions) GTK_FILL, 4, 0);
  g_signal_connect (G_OBJECT (button), "pressed",
                      G_CALLBACK (on_audio_create_copy_button_clicked),
                      gpp);

  /* reset button */
  button = gtk_button_new_from_stock (GIMP_STOCK_RESET);
  gtk_widget_show (button);
  gimp_help_set_help_data(button, _("Reset offset and volume"),NULL);
  gtk_table_attach(GTK_TABLE(table1), button, 2, 3, row, row + 1,
                    (GtkAttachOptions) GTK_FILL,
                    (GtkAttachOptions) GTK_FILL, 4, 0);
  g_signal_connect (G_OBJECT (button), "pressed",
                      G_CALLBACK (on_audio_reset_button_clicked),
                      gpp);
  row++;


  /* create the audio extract progressbar */
  progress_bar = gtk_progress_bar_new ();
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), " ");
  gtk_widget_show (progress_bar);
  gtk_table_attach (GTK_TABLE (table1), progress_bar, 1, 3, row, row+1,
                      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
                      (GtkAttachOptions) (GTK_FILL), 4, 0);
  gpp->progress_bar_audio = progress_bar;


  row++;

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_table_attach(GTK_TABLE(table1), hseparator, 0, 3, row, row + 1,
                    (GtkAttachOptions) GTK_FILL,
                    (GtkAttachOptions) GTK_FILL,
                     0, 0);

  row++;

  /* Audio Offset Length (mm:ss:msec) */
  label = gtk_label_new(_("Offsettime:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("mm:ss:msec");
  gpp->audio_offset_time_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  row++;

  /* Total Audio Length (mm:ss:msec) */
  label = gtk_label_new(_("Audiotime:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("mm:ss:msec");
  gpp->audio_total_time_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);


  row++;

  /* Length (frames) */
  label = gtk_label_new(_("Audioframes:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("####");
  gpp->audio_total_frames_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);


  row++;

  /* Audiolength (Samples) */
  label = gtk_label_new(_("Samples:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("######");
  gpp->audio_samples_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);


  row++;

  /* Audio Samplerate */
  label = gtk_label_new(_("Samplerate:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("######");
  gpp->audio_samplerate_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  row++;

  /* Audio Channels */
  label = gtk_label_new(_("Channels:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("#");
  gpp->audio_channels_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  row++;

  /* Bits per Audio Sample */
  label = gtk_label_new(_("Bits/Sample:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("##");
  gpp->audio_bits_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);


  row++;

  /* Total Video Length (mm:ss:msec) */
  label = gtk_label_new(_("Videotime:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("mm:ss:msec");
  gpp->video_total_time_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);


  row++;

  /* Video Length (frames) */
  label = gtk_label_new(_("Videoframes:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("000000");
  gpp->video_total_frames_label = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  return(frame0a);
}  /* end p_new_audioframe */


/* -----------------------------------------
 * p_update_cache_status
 * -----------------------------------------
 */
static void
p_update_cache_status (GapPlayerMainGlobalParams *gpp)
{
  static char  status_txt[50];
  gint32 elem_counter;
  gint32 bytes_used;
  gint32 max_bytes;


  elem_counter = gap_player_cache_get_current_frames_cached();
  bytes_used = gap_player_cache_get_current_bytes_used();
  max_bytes = gap_player_cache_get_max_bytesize();

  g_snprintf(status_txt, sizeof(status_txt), "%d", (int)elem_counter);
  gtk_label_set_text ( GTK_LABEL(gpp->label_current_cache_values)
                     , status_txt);


  if(gpp->progress_bar_cache_usage)
  {
    float  mb_used;
    gdouble progress;

    mb_used = (float)bytes_used / (1024.0 * 1024.0);
    progress = (gdouble)bytes_used / MAX((gdouble)max_bytes, 1.0);
    g_snprintf(status_txt, sizeof(status_txt), "%.4f MB", mb_used);

    if(gap_debug)
    {
      printf("p_update_cache_status: bytes_used:%d max_bytes:%d, max_player_cache:%d progress:%f\n"
          , (int)bytes_used
          , (int)max_bytes
          , (int)gpp->max_player_cache
          , (float)progress
          );
    }

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gpp->progress_bar_cache_usage)
        , CLAMP(progress, 0.0, 1.0));
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(gpp->progress_bar_cache_usage)
        , status_txt);
  }

}  /* end p_update_cache_status */


/* -----------------------------------------
 * on_cache_size_spinbutton_changed
 * -----------------------------------------
 */
static void
on_cache_size_spinbutton_changed (GtkEditable     *editable,
                                  GapPlayerMainGlobalParams *gpp)
{
  gdouble  mb_chachesize;
  gdouble  bytesize;

  if(gpp == NULL)
  {
    return;
  }
  mb_chachesize = GTK_ADJUSTMENT(gpp->cache_size_spinbutton_adj)->value;
  bytesize = mb_chachesize * (1024.0 * 1024.0);

  if(gpp->max_player_cache != (gint32)bytesize)
  {
    gpp->max_player_cache = (gint32)bytesize;

    if(gap_debug)
    {
      printf("on_cache_size_spinbutton_changed: max_player_cache:%d\n"
              , (int) gpp->max_player_cache
              );
    }
    gap_player_cache_set_max_bytesize(gpp->max_player_cache);
    p_update_cache_status(gpp);
  }

}  /* end on_cache_size_spinbutton_changed */


/* -----------------------------------------
 * on_tile_cache_size_spinbutton_changed
 * -----------------------------------------
 */
static void
on_tile_cache_size_spinbutton_changed (GtkEditable     *editable,
                                  GapPlayerMainGlobalParams *gpp)
{
  gdouble  nsize;

  if(gpp == NULL)
  {
    return;
  }
  nsize = GTK_ADJUSTMENT(gpp->cache_ntiles_spinbutton_adj)->value;

  if(gap_debug)
  {
      printf("on_tile_cache_size_spinbutton_changed: cache_ntiles:%d newvalue:%d\n"
              , (int) gpp->cache_ntiles
              , (int) nsize
              );
  }

  if(gpp->cache_ntiles != (gulong)nsize)
  {
    gpp->cache_ntiles = (gulong)nsize;

    gimp_tile_cache_ntiles(gpp->cache_ntiles);
  }

}  /* end on_tile_cache_size_spinbutton_changed */


/* -----------------------------------------
 * on_cache_clear_button_clicked
 * -----------------------------------------
 */
static void
on_cache_clear_button_clicked (GtkButton       *button,
                               GapPlayerMainGlobalParams *gpp)
{
  if(gpp == NULL)
  {
    return;
  }
  gap_player_cache_free_all();
  p_update_cache_status(gpp);
}  /* end on_cache_clear_button_clicked */


/* -----------------------------------------
 * p_gimprc_save_option
 * -----------------------------------------
 */
static void
p_gimprc_save_boolean_option (const char *option_name, gboolean value)
{
  if(value)
  {
    gimp_gimprc_set(option_name, "yes");
  }
  else
  {
    gimp_gimprc_set(option_name, "no");
  }
}  /* end p_gimprc_save_boolean_option */

/* -----------------------------------------
 * on_prefs_save_gimprc_button_clicked
 * -----------------------------------------
 */
static void
on_prefs_save_gimprc_button_clicked (GtkButton       *button,
                               GapPlayerMainGlobalParams *gpp)
{
  gchar *valueAsString;
  
  if(gpp == NULL)
  {
    return;
  }
  gap_player_cache_set_gimprc_bytesize(gpp->max_player_cache);

  p_gimprc_save_boolean_option("video_player_show_go_buttons"
                             ,gpp->show_go_buttons);
  p_gimprc_save_boolean_option("video_player_show_position_scale"
                             ,gpp->show_position_scale);
  p_gimprc_save_boolean_option("video_player_enable_detail_tracking"
                             ,gpp->enableDetailTracking);

  valueAsString = g_strdup_printf("%ld", (long)gpp->cache_ntiles);
  gimp_gimprc_set("video_player_cache_ntiles", valueAsString);
  g_free(valueAsString);
  

}  /* end on_prefs_save_gimprc_button_clicked */


/* -----------------------------
 * p_new_configframe
 * -----------------------------
 * create widgets for the audio options
 */
static GtkWidget *
p_new_configframe(GapPlayerMainGlobalParams *gpp)
{
  GtkWidget *frame0c;
  GtkWidget *label;
  GtkWidget *vbox1;
  GtkWidget *table1;
  GtkWidget *button;
  GtkWidget *progress_bar;
  GtkWidget *spinbutton;
  GtkWidget *checkbutton;
  GtkObject *adj;
  gint       row;

  if (gap_debug) printf("p_new_configframe\n");

  frame0c = gimp_frame_new (_("Playback Preferences"));

  /* the vbox */
  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (frame0c), vbox1);

  /* table */
  table1 = gtk_table_new (14, 3, FALSE);
  gpp->audio_table = table1;
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 4);

  row = 0;

  /* Cache size label */
  label = gtk_label_new (_("Cache Size (MB):"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1,  row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);        /* left alligned */

  {
    gdouble mb_cachesize;

    mb_cachesize = (gdouble)gpp->max_player_cache / (1024.0 * 1024.0);

    /* frame cache size spinutton */
    spinbutton = gimp_spin_button_new (&adj,  /* return value */
                        mb_cachesize,         /*   initial_val */
                        0.0,   /* umin */
                     9000.0,   /* umax */
                        1.0,  /* sstep */
                       10.0,   /* pagestep */
                       0.0,                  /* page_size */
                       10.0,                 /* climb_rate */
                        4                    /* digits */
                        );
    gtk_widget_show (spinbutton);
    gpp->cache_size_spinbutton_adj = adj;
    gtk_table_attach(GTK_TABLE(table1), spinbutton, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 4, 0);
    gimp_help_set_help_data(spinbutton, _("Player frame cache maximum size in MB. Value 0 turns the cache off."),NULL);
    g_signal_connect (G_OBJECT (gpp->cache_size_spinbutton_adj), "value_changed",
                        G_CALLBACK (on_cache_size_spinbutton_changed),
                        gpp);
  }

  /* clear player frame cache button */
  button = gtk_button_new_from_stock (GIMP_STOCK_RESET);
  gtk_widget_show (button);
  gimp_help_set_help_data(button, _("Clear the frame cache"),NULL);
  gtk_table_attach(GTK_TABLE(table1), button, 2, 3, row, row + 1,
                    (GtkAttachOptions) GTK_FILL,
                    (GtkAttachOptions) GTK_FILL, 4, 0);
  g_signal_connect (G_OBJECT (button), "pressed",
                      G_CALLBACK (on_cache_clear_button_clicked),
                      gpp);

  row++;

  /* Chache Status (number of frames currently in the cache) */
  label = gtk_label_new(_("Cached Frames:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);

  /* current number of frames in the player cache */
  label = gtk_label_new("0");
  gpp->label_current_cache_values = label;
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show(label);

  /* cache usage percentage progressbar */
  progress_bar = gtk_progress_bar_new ();
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), " ");
  gtk_widget_show (progress_bar);
  gtk_table_attach (GTK_TABLE (table1), progress_bar, 2, 3, row, row+1,
                      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
                      (GtkAttachOptions) (GTK_FILL), 4, 0);
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "0.0 MB");
  gpp->progress_bar_cache_usage = progress_bar;


  row++;

  /* tile Chache  */
  label = gtk_label_new(_("Tile Cache:"));
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table1), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show(label);


  /* tile cache size spinutton */
  spinbutton = gimp_spin_button_new (&adj,  /* return value */
                        gpp->cache_ntiles,         /*   initial_val */
                        0.0,   /* umin */
                     9000.0,   /* umax */
                        1.0,   /* sstep */
                       10.0,   /* pagestep */
                       0.0,                  /* page_size */
                       10.0,                 /* climb_rate */
                        0                    /* digits */
                        );
  gtk_widget_show (spinbutton);
  gpp->cache_ntiles_spinbutton_adj = adj;
  gtk_table_attach(GTK_TABLE(table1), spinbutton, 1, 2, row, row + 1, GTK_FILL, GTK_FILL, 4, 0);
  gimp_help_set_help_data(spinbutton, _("gimp tile cache for the player process. (in tiles 64x64 pixel)"),NULL);
  g_signal_connect (G_OBJECT (gpp->cache_ntiles_spinbutton_adj), "value_changed",
                        G_CALLBACK (on_tile_cache_size_spinbutton_changed),
                        gpp);


  row++;

  /* Layout Options label */
  label = gtk_label_new (_("Layout Options:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1,  row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);        /* left alligned */

  row++;

  /* Show Go button array (configure to show/hide this optional positioning tool) */
  checkbutton = gtk_check_button_new_with_label (_("Show Button Array"));
  gpp->show_go_buttons_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table1), checkbutton, 1, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (checkbutton, _("ON: Show the go button array positioning tool.\n"
                                          "OFF: Hide the go button array."), NULL);
  if(gpp->show_go_buttons)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (checkbutton), "toggled",
                      G_CALLBACK (on_show_gobuttons_checkbutton_toggled),
                      gpp);

  row++;

  /* Show Position Scale (configure to show/hide this optional positioning tool) */
  checkbutton = gtk_check_button_new_with_label (_("Show Position Scale"));
  gpp->show_position_scale_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table1), checkbutton, 1, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (checkbutton, _("ON: Show the position scale.\n"
                                          "OFF: Hide the position scale."), NULL);
  if(gpp->show_position_scale)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (checkbutton), "toggled",
                      G_CALLBACK (on_show_positionscale_checkbutton_toggled),
                      gpp);

  row++;





  /* configure Detail Tracking button */
  button = gtk_button_new_with_label(_("Configure Tracking:"));

  gtk_widget_show (button);
  gimp_help_set_help_data(button, _("Configure detail tracking options"),NULL);
  gtk_table_attach(GTK_TABLE(table1), button, 0, 1, row, row + 1,
                    (GtkAttachOptions) GTK_FILL,
                    (GtkAttachOptions) GTK_FILL, 4, 0);
  g_signal_connect (G_OBJECT (button), "pressed",
                      G_CALLBACK (on_detail_tracking_button_clicked),
                      gpp);

  checkbutton = gtk_check_button_new_with_label (_("Enable Detail Tracking"));
  gpp->detail_tracking_checkbutton = checkbutton;
  gtk_widget_show (checkbutton);
  gtk_table_attach (GTK_TABLE (table1), checkbutton, 1, 3, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (checkbutton, _("ON: Enable detail tracking in snapshot image."
                                          " Mark coordinates of one (or 2) significant details in the snapshot image"
                                          " using the current path with one or 2 points."
                                          " Each further snapshot automatically moves the marked points"
                                          " to the coordinates of the corresponding details "
                                          " and logs the movement as XML parameters for the MovePath feature."
                                          " In case 2 points are marked, the rotation is calculated too.\n"
                                          "OFF: Disable detail tracking."), NULL);
  if(gpp->enableDetailTracking)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (checkbutton), "toggled",
                      G_CALLBACK (on_enable_detail_tracking_checkbutton_toggled),
                      gpp);

  row++;

  /* Save Player Preferences label */
  label = gtk_label_new (_("Save Preferences:"));
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table1), label, 0, 1,  row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);        /* left alligned */



  /* save player frame cache settings to gimprc */
  button = gtk_button_new_from_stock (GTK_STOCK_SAVE);
  gtk_widget_show (button);
  gimp_help_set_help_data(button, _("Save player cache and layout settings (as gimprc parameters)"),NULL);
  gtk_table_attach(GTK_TABLE(table1), button, 1, 2, row, row + 1,
                    (GtkAttachOptions) GTK_FILL,
                    (GtkAttachOptions) GTK_FILL, 4, 0);
  g_signal_connect (G_OBJECT (button), "pressed",
                      G_CALLBACK (on_prefs_save_gimprc_button_clicked),
                      gpp);


  return(frame0c);
}  /* end p_new_configframe */



/* -----------------------------
 * p_create_player_window
 * -----------------------------
 */
GtkWidget*
p_create_player_window (GapPlayerMainGlobalParams *gpp)
{
  GtkWidget *shell_window;
  GtkWidget *event_box;

  GtkWidget *frame0;
  GtkWidget *aspect_frame;
  GtkWidget *frame2;

  GtkWidget *vbox0;
  GtkWidget *vbox1;

  GtkWidget *hbox1;
  GtkWidget *play_n_stop_hbox;
  GtkWidget *gobutton_hbox;

  GtkWidget *table1;
  GtkWidget *table11;
  GtkWidget *table2;

  GtkWidget *vid_preview;

  GtkWidget *status_label;
  GtkWidget *timepos_label;
  GtkWidget *label;

  GtkObject *from_spinbutton_adj;
  GtkWidget *from_spinbutton;
  GtkObject *to_spinbutton_adj;
  GtkWidget *to_spinbutton;
  GtkObject *framenr_spinbutton_adj;
  GtkWidget *framenr_spinbutton;
  GtkWidget *framenr_scale;
  GtkObject *speed_spinbutton_adj;
  GtkWidget *speed_spinbutton;
  GtkObject *size_spinbutton_adj;
  GtkWidget *size_spinbutton;


  GtkWidget *cancel_vindex_button;
  GtkWidget *play_button;
  GtkWidget *pause_button;
  GtkWidget *back_button;
  GtkWidget *close_button;
  GtkWidget *origspeed_button;
  GtkWidget *size_button;
  GtkWidget *framenr_1_button;
  GtkWidget *framenr_2_button;
  GtkWidget *from_button;
  GtkWidget *to_button;
  GtkWidget *progress_bar;

  GtkWidget *use_thumb_checkbutton;
  GtkWidget *exact_timing_checkbutton;
  GtkWidget *pinpong_checkbutton;
  GtkWidget *selonly_checkbutton;
  GtkWidget *loop_checkbutton;

  GtkWidget *spc_hbox0;
  GtkWidget *spc_label;

  GtkWidget *notebook;
  GtkWidget *label_vid;
  GtkWidget *label_cfg;
  GtkWidget *frame0c;
  GtkWidget *spc_hbox0c;
#ifdef GAP_ENABLE_AUDIO_SUPPORT
  GtkWidget *frame0a;
  GtkWidget *spc_hbox0a;
  GtkWidget *label_aud;
#endif
  GtkWidget *frame_scale_hbox;

#define ROW_EXTRA_SPACING 8

  gint row;
  gint notebook_idx;

  /* columns for the spinbutton and buttons */
  gint colspin;
  gint colbutton;

  colspin =   0;
  colbutton = 1;


  shell_window = NULL;

  if(gpp->docking_container)
  {
    shell_window = gpp->docking_container;
  }
  else
  {
    shell_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gpp->shell_window = shell_window;
    gtk_window_set_title (GTK_WINDOW (shell_window), _("Videoframe Playback"));
    gtk_window_set_resizable(GTK_WINDOW (shell_window), TRUE);
  }
  g_signal_connect (G_OBJECT (shell_window), "destroy",
                      G_CALLBACK (on_shell_window_destroy),
                      gpp);

  vbox0 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox0);
  gtk_container_add (GTK_CONTAINER (shell_window), vbox0);




  /* vid options TAB frame */
  label_vid = gtk_label_new (_("Video Options"));
  gtk_widget_show (label_vid);

  if(gpp->stb_ptr)
  {
    frame0 = gimp_frame_new (gpp->stb_ptr->storyboardfile);
  }
  else
  {
    frame0 = gimp_frame_new (gpp->ainfo_ptr->basename);
  }
  gtk_widget_show (frame0);
  gpp->frame_with_name = frame0;

  spc_label = gtk_label_new(" ");
  gtk_widget_show (spc_label);

  spc_hbox0 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (spc_hbox0);
  gtk_box_pack_start (GTK_BOX (spc_hbox0), spc_label, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (spc_hbox0), frame0, TRUE, TRUE, 4);


  /* configure options TAB frame */
  label_cfg = gtk_label_new (_("Preferences"));
  gtk_widget_show (label_cfg);

  spc_label = gtk_label_new(" ");
  gtk_widget_show (spc_label);

  frame0c = p_new_configframe (gpp);
  gtk_widget_show (frame0c);

  spc_hbox0c = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (spc_hbox0c);
  gtk_box_pack_start (GTK_BOX (spc_hbox0c), spc_label, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (spc_hbox0c), frame0c, TRUE, TRUE, 4);

#ifdef GAP_ENABLE_AUDIO_SUPPORT
  /* audio options TAB frame */
  label_aud = gtk_label_new (_("Audio Options"));
  gtk_widget_show (label_aud);

  frame0a = p_new_audioframe (gpp);
  gtk_widget_show (frame0a);

  spc_label = gtk_label_new(" ");
  gtk_widget_show (spc_label);

  spc_hbox0a = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (spc_hbox0a);
  gtk_box_pack_start (GTK_BOX (spc_hbox0a), spc_label, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (spc_hbox0a), frame0a, TRUE, TRUE, 4);
#endif


  notebook = gtk_notebook_new();
  gtk_widget_show (notebook);

  notebook_idx = 0;
  gtk_container_add (GTK_CONTAINER (notebook), spc_hbox0);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), notebook_idx)
                             , label_vid
                             );
  notebook_idx++;

#ifdef GAP_ENABLE_AUDIO_SUPPORT
  gtk_container_add (GTK_CONTAINER (notebook), spc_hbox0a);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), notebook_idx)
                             , label_aud
                             );
  notebook_idx++;
#endif
  gtk_container_add (GTK_CONTAINER (notebook), spc_hbox0c);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook)
                             , gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), notebook_idx)
                             , label_cfg
                             );
  notebook_idx++;

  gtk_box_pack_start (GTK_BOX (vbox0), notebook, TRUE, TRUE, 0);





  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (frame0), vbox1);

  /* the hbox for the go button array */
  gobutton_hbox = gtk_hbox_new (TRUE, 0);
  gpp->gobutton_hbox = gobutton_hbox;
  if(gpp->show_go_buttons)
  {
    gtk_widget_show (gobutton_hbox);
  }
  else
  {
    gtk_widget_hide (gobutton_hbox);
  }

  if(gobutton_hbox)
  {
    gint go_number;
    gint max_go_buttons;
    GtkWidget *go_button;
    t_gobutton *gob;

    /* the gobutton array is be filled with [n] gobuttons */
    max_go_buttons = GAP_PLAY_MAX_GOBUTTONS;
    if(gpp->docking_container)
    {
       max_go_buttons = GAP_PLAY_MAX_GOBUTTONS_DOCKED;
    }

    for(go_number=0; go_number < max_go_buttons; go_number++)
    {
       /* the go_button[s] */
       gob = g_malloc0(sizeof(t_gobutton));
       gob->gpp = gpp;
       gob->go_number = go_number;

       go_button = gtk_button_new ();
       gtk_widget_show (go_button);

       gtk_widget_set_events(go_button, GDK_ENTER_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK);
       gtk_box_pack_start (GTK_BOX (gobutton_hbox), go_button, FALSE, TRUE, 0);
       gtk_widget_set_size_request (go_button, -1, 40);
       if(go_number == 0)
       {
         gimp_help_set_help_data (go_button, _("Click: go to frame, Ctrl-Click: set 'From Frame', Alt-Click: set 'To Frame'"), NULL);
       }
       g_signal_connect (go_button, "enter_notify_event",
                      G_CALLBACK (on_go_button_enter)
                      ,gob);
       g_signal_connect (G_OBJECT (go_button), "button_press_event",
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



  /* the FRAMENR adjustment (for spinbutton and scale)  */
  {
    gdouble l_value;
    gdouble l_lower;
    gdouble l_upper;

    l_lower = MIN(gpp->ainfo_ptr->first_frame_nr, gpp->ainfo_ptr->last_frame_nr);
    l_upper = MAX(gpp->ainfo_ptr->first_frame_nr, gpp->ainfo_ptr->last_frame_nr);
    l_value = CLAMP(gpp->play_current_framenr, l_lower, l_upper);

    if(gap_debug) printf("CREATE framenr_spinbutton_adj: value: %d, lower:%d, upper:%d\n"
          ,(int)l_value
          ,(int)l_lower
          ,(int)l_upper
          );

    framenr_spinbutton_adj = gtk_adjustment_new (l_value
                                           , l_lower
                                           , l_upper
                                           , 1.0     /* step_increment */
                                           , 10.0    /* page_increment */
                                           , 0.0     /* page_size  must be 0 for simple scalar value */
                                           );
  }


  /* hbox for scale and time position label mm:ss:msec */
  frame_scale_hbox = gtk_hbox_new (FALSE, 0);
  gpp->frame_scale_hbox = frame_scale_hbox;


  /* the framenr_scale (tool for positioning) */
  framenr_scale = gtk_hscale_new (GTK_ADJUSTMENT (framenr_spinbutton_adj));
  gpp->framenr_scale = framenr_scale;
  gtk_scale_set_digits (GTK_SCALE (framenr_scale), 0 /* digits */);

  /* gtk_scale_set_draw_value (GTK_SCALE (framenr_scale), FALSE); */
  /* Dont know why, but without displaying the value
   * the scale and spinbutton value sometimes seems to differ by 1
   *
   * Movinge the Scale sometimes displays frame nnnnn but spinbutton shows nnnnn +1
   * (could not reproduce such differences with draw_value = TRUE)
   */
  gtk_scale_set_draw_value (GTK_SCALE (framenr_scale), FALSE);

  gtk_range_set_update_policy (GTK_RANGE (framenr_scale), GTK_UPDATE_DELAYED);
  gimp_help_set_help_data (framenr_scale,
      _("The currently displayed frame number"), NULL);

  gtk_widget_show(framenr_scale);


  gtk_box_pack_start (GTK_BOX (frame_scale_hbox), framenr_scale, TRUE, TRUE, 0);
  g_signal_connect (framenr_scale, "button_press_event",
                      G_CALLBACK (on_framenr_scale_clicked)
                      ,gpp);


  /* the timepos label */
  /* (had used an entry here before but had update performance problems
   *  beginning at playback speed of 17 frames/sec on PII 300 Mhz)
   */
  timepos_label = gtk_label_new ("00:00:000");
  gpp->timepos_label = timepos_label;
  gtk_widget_show (timepos_label);
  gtk_box_pack_start (GTK_BOX (frame_scale_hbox), timepos_label, FALSE, FALSE, 4);





  if(gpp->show_position_scale)
  {
    gtk_widget_show(frame_scale_hbox);
  }
  else
  {
    gtk_widget_show(frame_scale_hbox);
  }

  gtk_box_pack_start (GTK_BOX (vbox1), frame_scale_hbox, FALSE, FALSE, 0);


  table1 = gtk_table_new (2, 2, FALSE);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);


  /* the frame2 for range and playback mode control widgets */
  frame2 = gimp_frame_new (NULL);
  gtk_widget_show (frame2);
  gtk_table_attach (GTK_TABLE (table1), frame2, 1, 2, 0, 1
                   , (GtkAttachOptions) (0)
                   , (GtkAttachOptions) (0)
                   , 0, 0);

  /* table2 for range and playback mode control widgets */
  table2 = gtk_table_new (18, 2, FALSE);
  gtk_widget_show (table2);
  gtk_table_set_col_spacings (GTK_TABLE (table2), 4);
  gtk_container_add (GTK_CONTAINER (frame2), table2);

  row = 0;

  /* the framenr buttons */
  {

    GtkWidget *fnr_hbox;

    fnr_hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (fnr_hbox);
    gtk_table_attach (GTK_TABLE (table2), fnr_hbox, colbutton, colbutton+1, row, row+1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);

    /* the framenr 1 button (does set Begin of range) */
    framenr_1_button = gap_stock_button_new (GAP_STOCK_SET_RANGE_START);
    gtk_widget_show (framenr_1_button);
    gtk_box_pack_start (GTK_BOX (fnr_hbox), framenr_1_button, FALSE, FALSE, 0);
    g_object_set_data (G_OBJECT (framenr_1_button), KEY_FRAMENR_BUTTON_TYPE, (gpointer)FRAMENR_BUTTON_BEGIN);
    g_signal_connect (G_OBJECT (framenr_1_button), "button_press_event",
                        G_CALLBACK (on_framenr_button_clicked),
                        gpp);
    if((gpp->image_id >= 0)
    && (gpp->docking_container == NULL))
    {
      gimp_help_set_help_data (framenr_1_button
                              , _("Click: Set current framenr as selection range start 'From Frame',\n"
                                  "SHIFT-Click: load this frame into the calling image")
                              , NULL);
    }
    else
    {
      /* there is no "calling image" if we are invoked from storyboard
       * or from the video extract plug-in
       * in this case there is no special SHIFT-click function available
       */
      gimp_help_set_help_data (framenr_1_button
                              , _("Set current framenr as selection range start 'From Frame'")
                              , NULL);
    }

    /* the framenr 2 button (does set End of range) */
    framenr_2_button = gap_stock_button_new (GAP_STOCK_SET_RANGE_END);
    gtk_widget_show (framenr_2_button);
    gtk_box_pack_start (GTK_BOX (fnr_hbox), framenr_2_button, TRUE, TRUE, 0);
    g_object_set_data (G_OBJECT (framenr_2_button), KEY_FRAMENR_BUTTON_TYPE, (gpointer)FRAMENR_BUTTON_END);
    g_signal_connect (G_OBJECT (framenr_2_button), "button_press_event",
                        G_CALLBACK (on_framenr_button_clicked),
                        gpp);
    if((gpp->image_id >= 0)
    && (gpp->docking_container == NULL))
    {
      gimp_help_set_help_data (framenr_2_button
                              , _("Click: Set current framenr as selection range end 'To Frame',\n"
                                  "SHIFT-Click: load this frame into the calling image")
                              , NULL);
    }
    else
    {
      /* there is no "calling image" if we are invoked from storyboard
       * or from the video extract plug-in
       * in this case there is no special SHIFT-click function available
       */
      gimp_help_set_help_data (framenr_2_button
                              , _("Set current framenr as selection range end 'To Frame'")
                              , NULL);
    }
  }

  /* the FRAMENR spinbutton (current displayed frame)  */
  framenr_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (framenr_spinbutton_adj)
                       , 1   /* climb_rate  */
                       , 0   /* digits */
                       );
  gtk_widget_show (framenr_spinbutton);
  gtk_table_attach (GTK_TABLE (table2), framenr_spinbutton, colspin, colspin+1, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (framenr_spinbutton, 80, -1);
  gimp_help_set_help_data (framenr_spinbutton, _("The currently displayed frame number"), NULL);
  g_signal_connect (G_OBJECT (framenr_spinbutton_adj), "value_changed",
                      G_CALLBACK (on_framenr_spinbutton_changed),
                      gpp);


  gtk_table_set_row_spacing (GTK_TABLE (table2), row, ROW_EXTRA_SPACING);


  row++;

  /* the from button */
  from_button = gap_stock_button_new (GAP_STOCK_RANGE_START);
  gpp->from_button = from_button;
  /* the from button */
  gtk_widget_show (from_button);
  gtk_table_attach (GTK_TABLE (table2), from_button, colbutton, colbutton+1, row, row+1
                     , (GtkAttachOptions) (GTK_FILL)
                     , (GtkAttachOptions) (0)
                     , 0, 0);
  if(gpp->caller_range_linked)
  {
    gtk_widget_set_sensitive (from_button, FALSE);
  }
  else
  {
    if(gpp->fptr_set_range)
    {
      gimp_help_set_help_data (from_button, _("Add range to cliplist"), NULL);
    }
    else
    {
      gimp_help_set_help_data (from_button, _("Print range to stdout"), NULL);
    }
    g_signal_connect (G_OBJECT (from_button), "clicked",
                        G_CALLBACK (on_from_button_clicked),
                        gpp);
  }

  /* the FROM spinbutton (start of rangeselection)  */
  from_spinbutton_adj = gtk_adjustment_new ( gpp->begin_frame
                                           , gpp->ainfo_ptr->first_frame_nr
                                           , gpp->ainfo_ptr->last_frame_nr
                                           , 1.0     /* step_increment */
                                           , 10.0    /* page_increment */
                                           , 0.0     /* page_size  must be 0 for simple scalar value */
                                           );
  from_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (from_spinbutton_adj), 1, 0);
  gtk_widget_show (from_spinbutton);
  gtk_table_attach (GTK_TABLE (table2), from_spinbutton, colspin, colspin+1, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                     0, 0);
  gtk_widget_set_size_request (from_spinbutton, 80, -1);
  gimp_help_set_help_data (from_spinbutton, _("Start framenumber of selection range"), NULL);
  g_signal_connect (G_OBJECT (from_spinbutton_adj), "value_changed",
                      G_CALLBACK (on_from_spinbutton_changed),
                      gpp);

  row++;

  /* the to button */
  to_button = gap_stock_button_new (GAP_STOCK_RANGE_END);
  gpp->to_button = to_button;
  gtk_widget_show (to_button);
  gtk_table_attach (GTK_TABLE (table2), to_button, colbutton, colbutton+1, row, row+1
                     , (GtkAttachOptions) (GTK_FILL)
                     , (GtkAttachOptions) (0)
                     , 0, 0);
  if(gpp->caller_range_linked)
  {
    gtk_widget_set_sensitive (to_button, FALSE);
  }
  else
  {
    if(gpp->fptr_set_range)
    {
      gimp_help_set_help_data (to_button, _("Add inverse range to cliplist"), NULL);
    }
    else
    {
      gimp_help_set_help_data (to_button, _("Print inverse range to stdout"), NULL);
    }
    g_signal_connect (G_OBJECT (to_button), "clicked",
                        G_CALLBACK (on_to_button_clicked),
                        gpp);
  }



  /* the TO spinbutton (end of rangeselection)  */
  to_spinbutton_adj = gtk_adjustment_new ( gpp->end_frame
                                           , gpp->ainfo_ptr->first_frame_nr
                                           , gpp->ainfo_ptr->last_frame_nr
                                           , 1.0    /* step_increment */
                                           , 10.0   /* page_increment */
                                           , 0.0    /* page_size  must be 0 for simple scalar value */
                                           );
  to_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (to_spinbutton_adj), 1, 0);
  gtk_widget_show (to_spinbutton);
  gtk_table_attach (GTK_TABLE (table2), to_spinbutton, colspin, colspin+1, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                     0, 0);
  gtk_widget_set_size_request (to_spinbutton, 80, -1);
  gimp_help_set_help_data (to_spinbutton, _("End framenumber of selection range"), NULL);
  g_signal_connect (G_OBJECT (to_spinbutton_adj), "value_changed",
                      G_CALLBACK (on_to_spinbutton_changed),
                      gpp);


  gtk_table_set_row_spacing (GTK_TABLE (table2), row, ROW_EXTRA_SPACING);

  row++;

  /* the origspeed_button */
  origspeed_button = gap_stock_button_new (GAP_STOCK_SPEED);
  gtk_widget_show (origspeed_button);
  gtk_table_attach (GTK_TABLE (table2), origspeed_button, colbutton, colbutton+1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (origspeed_button, _("Reset playback speed to original (or previous) value"), NULL);
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
                                           , 1.0      /* step_increment */
                                           , 10.0     /* page_increment */
                                           , 0.0      /* page_size  must be 0 for simple scalar value */
                                           );
  speed_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (speed_spinbutton_adj), 1, 4);
  gtk_widget_show (speed_spinbutton);
  gtk_table_attach (GTK_TABLE (table2), speed_spinbutton, colspin, colspin+1, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (speed_spinbutton, 80, -1);
  gimp_help_set_help_data (speed_spinbutton, _("Current playback speed (frames/sec)"), NULL);
  g_signal_connect (G_OBJECT (speed_spinbutton_adj), "value_changed",
                      G_CALLBACK (on_speed_spinbutton_changed),
                      gpp);


  row++;

  /* the size button */
  {
      /* we use the same icon as the gimp scaling tool */
      GtkWidget *image;
      image = gtk_image_new_from_stock (GIMP_STOCK_TOOL_SCALE,
                                        GTK_ICON_SIZE_BUTTON);

      gtk_widget_show (image);
      size_button = gtk_button_new();
      gtk_container_add (GTK_CONTAINER (size_button), image);

  }
  gtk_widget_show (size_button);
  gtk_widget_set_events(size_button, GDK_BUTTON_PRESS_MASK);
  gtk_table_attach (GTK_TABLE (table2), size_button, colbutton, colbutton+1, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0),
                    0, 0);
  gimp_help_set_help_data (size_button, _("Toggle size 128/256. <Shift> Set 1:1 full image size"), NULL);
  g_signal_connect (G_OBJECT (size_button), "button_press_event",
                      G_CALLBACK (on_size_button_button_press_event),
                      gpp);

  /* the SIZE spinbutton */
  size_spinbutton_adj = gtk_adjustment_new (gpp->pv_pixelsize
                                           , GAP_PLAYER_MIN_SIZE
                                           , GAP_PLAYER_MAX_SIZE
                                           , 1.0     /* step_increment */
                                           , 10.0    /* page_increment */
                                           , 0.0     /* page_size  must be 0 for simple scalar value */
                                           );

  size_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (size_spinbutton_adj), 1, 0);
  gpp->size_spinbutton = size_spinbutton;
  gtk_widget_show (size_spinbutton);
  gtk_table_attach (GTK_TABLE (table2), size_spinbutton, colspin, colspin+1, row, row+1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0),
                    0, 0);
  gtk_widget_set_size_request (size_spinbutton, 80, -1);
  gimp_help_set_help_data (size_spinbutton, _("Video preview size (pixels)"), NULL);

  g_signal_connect (G_OBJECT (size_spinbutton_adj), "value_changed",
                      G_CALLBACK (on_size_spinbutton_changed),
                      gpp);

  if(gpp->docking_container == NULL)
  {
    gtk_widget_set_events(size_spinbutton
                         , GDK_ENTER_NOTIFY_MASK
                         );
    g_signal_connect (G_OBJECT (size_spinbutton), "enter_notify_event",
                        G_CALLBACK (on_size_spinbutton_enter),
                        gpp);
  }

  gtk_table_set_row_spacing (GTK_TABLE (table2), row, ROW_EXTRA_SPACING);

  row++;


  /* the playback mode checkbuttons */

  /* Loop Toggle */
  loop_checkbutton = gtk_check_button_new_with_label (_("Loop"));
  gpp->loop_checkbutton = loop_checkbutton;
  gtk_widget_show (loop_checkbutton);
  gtk_table_attach (GTK_TABLE (table2), loop_checkbutton, 0, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (loop_checkbutton, _("ON: Play in endless loop.\n"
                                               "OFF: Play only once"), NULL);
  if(gpp->play_loop)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (loop_checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (loop_checkbutton), "toggled",
                      G_CALLBACK (on_loop_checkbutton_toggled),
                      gpp);


  row++;

  /* SelOnly Toggle (keep text short) */
  selonly_checkbutton = gtk_check_button_new_with_label (_("Selection only"));
  gpp->selonly_checkbutton = selonly_checkbutton;
  gtk_widget_show (selonly_checkbutton);
  gtk_table_attach (GTK_TABLE (table2), selonly_checkbutton, 0, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (selonly_checkbutton, _("ON: Play only frames within the selected range.\n"
                                                  "OFF: Play all frames"), NULL);
  if(gpp->play_selection_only)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (selonly_checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (selonly_checkbutton), "toggled",
                      G_CALLBACK (on_selonly_checkbutton_toggled),
                      gpp);

  row++;

  /* PingPong Toggle (keep text short) */
  pinpong_checkbutton = gtk_check_button_new_with_label (_("Ping pong"));
  gpp->pinpong_checkbutton = pinpong_checkbutton;
  gtk_widget_show (pinpong_checkbutton);
  gtk_table_attach (GTK_TABLE (table2), pinpong_checkbutton, 0, 2, row, row+1,
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


  row++;

  /* UseThumbnails Toggle (keep text short) */
  use_thumb_checkbutton = gtk_check_button_new_with_label (_("Thumbnails"));
  gpp->use_thumb_checkbutton = use_thumb_checkbutton;
  gtk_widget_show (use_thumb_checkbutton);
  gtk_table_attach (GTK_TABLE (table2), use_thumb_checkbutton, 0, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (use_thumb_checkbutton, _("ON: Use thumbnails when available.\n"
                                                    "OFF: Read full sized frames"), NULL);
  if(gpp->use_thumbnails)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_thumb_checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (use_thumb_checkbutton), "toggled",
                      G_CALLBACK (on_use_thumb_checkbutton_toggled),
                      gpp);



  row++;

  /* ExactTiming Toggle (keep text short) */
  exact_timing_checkbutton = gtk_check_button_new_with_label (_("Exact timing"));
  gpp->exact_timing_checkbutton = exact_timing_checkbutton;
  gtk_widget_show (exact_timing_checkbutton);
  gtk_table_attach (GTK_TABLE (table2), exact_timing_checkbutton, 0, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gimp_help_set_help_data (exact_timing_checkbutton, _("ON: Skip frames to hold exact timing.\n"
                                                       "OFF: Disable frame skipping"), NULL);
  if(gpp->exact_timing)
  {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (exact_timing_checkbutton), TRUE);
  }
  g_signal_connect (G_OBJECT (exact_timing_checkbutton), "toggled",
                      G_CALLBACK (on_exact_timing_checkbutton_toggled),
                      gpp);

  row++;


  /* the status value label */
  status_label = gtk_label_new (_("Ready"));
  gtk_widget_show (status_label);
  gtk_table_attach (GTK_TABLE (table2), status_label, 0, 2, row, row+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (status_label), 0.0, 0.5);

  row++;

  /* the progress bar */
  progress_bar = NULL;
  if(gpp->have_progress_bar)
  {
    progress_bar = gtk_progress_bar_new ();
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), " ");
    gtk_widget_show (progress_bar);
    gtk_table_attach (GTK_TABLE (table2), progress_bar, 0, 2, row, row+1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
  }
  gpp->progress_bar = progress_bar;

  /* a dummy label to fill up table1 until bottom */
  label = gtk_label_new (" ");
  gtk_widget_show (label);
//  gtk_table_attach (GTK_TABLE (table1), label, 1, 2, 1, 2,
//                    (GtkAttachOptions) (0),
//                    (GtkAttachOptions) (GTK_FILL | GTK_SHRINK | GTK_EXPAND), 0, 0);



  /* the hbox */
  hbox1 = gtk_hbox_new (TRUE, 0);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox0), hbox1, FALSE, FALSE, 0);


  /* the playback/pause button box (hidden while videoindex creation is running) */
  play_n_stop_hbox = gtk_hbox_new (TRUE, 0);
  gpp->play_n_stop_hbox = play_n_stop_hbox;
  gtk_widget_show (play_n_stop_hbox);
  gtk_box_pack_start (GTK_BOX (hbox1), play_n_stop_hbox, FALSE, TRUE, 0);

  /* the Cancel Videoindex Creation button (only visible while creating vindex) */
  cancel_vindex_button = gtk_button_new_with_label ( _("Cancel Videoindex creation"));
  gpp->cancel_vindex_button = cancel_vindex_button;
  gtk_widget_hide (cancel_vindex_button);
  gtk_box_pack_start (GTK_BOX (hbox1), cancel_vindex_button, FALSE, TRUE, 0);
  gimp_help_set_help_data (cancel_vindex_button, _("Cancel videoindex creation. "
                                          "Videoindex creation requires full scanning of the video "
                                          "but allows fast random access to frames afterwards. "
                                          "Without a videoindex, access is done by a very slow sequential read"), NULL);
  g_signal_connect (G_OBJECT (cancel_vindex_button), "clicked",
                    G_CALLBACK (on_cancel_vindex_button_clicked),
                    gpp);

  if(gpp->help_id && gimp_show_help_button ())
  {
    GtkWidget *help_button;

    /* the HELP button */
    help_button = gtk_button_new_from_stock (GTK_STOCK_HELP);
    gtk_widget_show (help_button);
    gtk_box_pack_start (GTK_BOX (play_n_stop_hbox), help_button, FALSE, TRUE, 0);
    gimp_help_set_help_data (help_button, _("Show help page"), NULL);
    g_signal_connect (G_OBJECT (help_button), "clicked",
                        G_CALLBACK (on_help_button_clicked),
                        gpp);
  }


  /* the PLAY button */
  play_button = gap_stock_button_new_with_label (GAP_STOCK_PLAY, _("Play"));
  gtk_widget_show (play_button);
  gtk_box_pack_start (GTK_BOX (play_n_stop_hbox), play_button, FALSE, TRUE, 0);
  gimp_help_set_help_data (play_button, _("Start playback. "
                                          "SHIFT: snapshot frames in a multilayer image at original size "
                                          "CTRL: snapshot at preview size "
                                          "ALT: force creation of new snapshot image"), NULL);
  g_signal_connect (G_OBJECT (play_button), "button_press_event",
                      G_CALLBACK (on_play_button_clicked),
                      gpp);

  /* the PAUSE button */
  pause_button = gap_stock_button_new_with_label (GAP_STOCK_PAUSE, _("Pause"));
  gtk_widget_show (pause_button);
  gtk_widget_set_events(pause_button, GDK_BUTTON_PRESS_MASK);
  gtk_box_pack_start (GTK_BOX (play_n_stop_hbox), pause_button, FALSE, TRUE, 0);
  gimp_help_set_help_data (pause_button, _("Pause if playing (any mousebutton). "
                                           "Go to selection start/active/end (left/middle/right mousebutton) if not playing"), NULL);
  g_signal_connect (G_OBJECT (pause_button), "button_press_event",
                      G_CALLBACK (on_pause_button_press_event),
                      gpp);

  /* the PLAY_REVERSE button */
  back_button = gap_stock_button_new_with_label (GAP_STOCK_PLAY_REVERSE, _("Reverse"));
  gtk_widget_show (back_button);
  gtk_box_pack_start (GTK_BOX (play_n_stop_hbox), back_button, FALSE, TRUE, 0);
  gimp_help_set_help_data (back_button, _("Start reverse playback. "
                                          "SHIFT: snapshot frames in a multilayer image at original size "
                                          "CTRL: snapshot at preview size "
                                          "ALT: force creation of new snapshot image"), NULL);
  g_signal_connect (G_OBJECT (back_button), "button_press_event",
                      G_CALLBACK (on_back_button_clicked),
                      gpp);

  if(gpp->docking_container == NULL)
  {
    /* the CLOSE button */
    close_button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
    gtk_widget_show (close_button);
    gtk_box_pack_start (GTK_BOX (play_n_stop_hbox), close_button, FALSE, TRUE, 0);
    gimp_help_set_help_data (close_button, _("Close window"), NULL);
    g_signal_connect (G_OBJECT (close_button), "clicked",
                        G_CALLBACK (on_close_button_clicked),
                        gpp);
  }


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
  {
    gint ix;
    gint iy;
    GtkWidget *dummy;

    for(ix = 0; ix < 3; ix++)
    {
      for(iy = 0; iy < 3; iy++)
      {
        if((ix == 1) && (iy == 1))
        {
           gtk_table_attach (GTK_TABLE (table11), aspect_frame, ix, ix+1, iy, iy+1,
                             (GtkAttachOptions) (0),
                             (GtkAttachOptions) (0), 0, 0);
        }
        else
        {
          /* dummy widgets to fill up table11  */
          dummy = gtk_vbox_new (FALSE,3);
          gtk_widget_show (dummy);
          gtk_table_attach (GTK_TABLE (table11), dummy, ix, ix+1, iy, iy+1,
                            (GtkAttachOptions) (GTK_FILL | GTK_SHRINK | GTK_EXPAND),
                            (GtkAttachOptions) (GTK_FILL | GTK_SHRINK | GTK_EXPAND), 0, 0);
        }
      }
    }

  }

  {
    GtkWidget *wrap_frame;
    GtkWidget *wrap_event_box;
    wrap_event_box = gtk_event_box_new ();
    gtk_widget_show (wrap_event_box);

    wrap_frame= gimp_frame_new(NULL);
    gtk_container_set_border_width (GTK_CONTAINER (wrap_frame), 0);
    gpp->resize_box = wrap_frame;
    gtk_widget_show(wrap_frame);
    gtk_container_add (GTK_CONTAINER (wrap_frame), table11);
    gtk_container_add (GTK_CONTAINER (wrap_event_box), wrap_frame);
    gtk_table_attach (GTK_TABLE (table1), wrap_event_box, 0, 1, 0, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), 0, 0);

    g_signal_connect (G_OBJECT (wrap_event_box), "scroll_event",
                      G_CALLBACK (on_warp_frame_scroll_event),
                      gpp);
  }

  gtk_widget_realize (shell_window);

  /* the preview drawing_area_widget */
  /* ############################### */
  gpp->pv_ptr = gap_pview_new(GAP_SMALL_PREVIEW_SIZE, GAP_SMALL_PREVIEW_SIZE, GAP_PLAYER_CHECK_SIZE, aspect_frame);
  vid_preview = gpp->pv_ptr->da_widget;
  gtk_container_add (GTK_CONTAINER (aspect_frame), vid_preview);
  gtk_widget_show (vid_preview);

  /* gpp copies of objects used outside this procedure  */
  gpp->from_spinbutton_adj = from_spinbutton_adj;
  gpp->to_spinbutton_adj = to_spinbutton_adj;
  gpp->framenr_spinbutton_adj = framenr_spinbutton_adj;
  gpp->speed_spinbutton_adj = speed_spinbutton_adj;
  gpp->size_spinbutton_adj = size_spinbutton_adj;


  gtk_widget_realize (gpp->pv_ptr->da_widget);
  p_update_pviewsize(gpp);

  /* the call to gtk_widget_set_events results in WARNING:
   * (gimp-1.3:13789): Gtk-CRITICAL **: file gtkwidget.c: line 5257 (gtk_widget_set_events): assertion `!GTK_WIDGET_REALIZED (widget)' failed
   *
   * must use gtk_widget_add_events because the widget is already realized at this time
   */
  gtk_widget_add_events (vid_preview, GDK_BUTTON_PRESS_MASK | GDK_EXPOSURE_MASK);

  g_signal_connect (G_OBJECT (vid_preview), "button_press_event",
                      G_CALLBACK (on_vid_preview_button_press_event),
                      gpp);
  g_signal_connect (G_OBJECT (vid_preview), "expose_event",
                      G_CALLBACK (on_vid_preview_expose_event),
                      gpp);


  gpp->status_label = status_label;

  return shell_window;
}  /* end p_create_player_window */


/* -----------------------------
 * p_connect_resize_handler
 * -----------------------------
 */
static void
p_connect_resize_handler(GapPlayerMainGlobalParams *gpp)
{
  if(gpp->docking_container)
  {
    /* never connect resize handler when player is docked to a foreign container */
    return;
  }
  if(gpp->resize_handler_id < 0)
  {
    gpp->resize_handler_id = g_signal_connect (G_OBJECT (gpp->resize_box), "size_allocate",
                      G_CALLBACK (on_vid_preview_size_allocate),
                      gpp);
  }
}  /* end p_connect_resize_handler */


/* -----------------------------
 * p_disconnect_resize_handler
 * -----------------------------
 */
static void
p_disconnect_resize_handler(GapPlayerMainGlobalParams *gpp)
{
  if(gpp->resize_handler_id >= 0)
  {
    g_signal_handler_disconnect(gpp->resize_box, gpp->resize_handler_id);
    gpp->resize_handler_id = -1;
  }
}  /* end p_disconnect_resize_handler */


/* ---------------------------------
 * gap_player_dlg_restart
 * ---------------------------------
 * This procedure re-initializes an
 * already running player dialog for playback
 * of a new storyboard or normal image frame playback.
 * IN: autostart     if TRUE automatically start playback.
 * IN: image_id      a gimp image_id of a GIMP_GAP typical animation frame
 *                   specify -1 if you want play videofiles or storyboard files.
 * IN: imagename     name of an image or videofile.
 * IN: stb           Ponter to a storyboard structure that should be played,
 *                   specify NULL for playback of simple frame ranges
 *                   or single videofiles.
 * IN: flip_request  flag to specify simple transformation requests (0 none, 1 hor, 2 ver 3 rotate 180)
 *                   ignored for storyboard playback (stb_ptr != NULL)
 * IN: flip status   same bits as flip_requst, specifies the transformation status of the
 *                   original input frames.
 *                   The rendering calculates the transformation by XOR
 *                   operation  flip_request XOR flip_status.
 *                   (in other words: if the status is the same as the request
 *                    no transformation is performed.)
 *                   ignored for storyboard playback (stb_ptr != NULL)
 * IN: stb_in_track  the storyboard track that should be played
 *                   a value of -1 selects the composite video fro playback
 *                   stb_in_track is ignored if stb_ptr == NULL
 */
void
gap_player_dlg_restart(GapPlayerMainGlobalParams *gpp
                      , gboolean autostart
                      , gint32   image_id
                      , char *imagename
                      , gint32 imagewidth
                      , gint32 imageheight
                      , GapStoryBoard *stb_ptr
                      , gint32 begin_frame
                      , gint32 end_frame
                      , gboolean play_selection_only
                      , gint32 seltrack
                      , gdouble delace
                      , const char *preferred_decoder
                      , gboolean  force_open_as_video
                      , gint32 flip_request
                      , gint32 flip_status
                      , gint32 stb_in_track
                      )
{
  gboolean  reverse_play;
  gint32 l_begin_frame;
  gint32 l_end_frame;
  gint32 l_curr_frame;

  if(gap_debug) printf("gap_player_dlg_restart: START begin_frame:%d end_frame:%d\n", (int)begin_frame, (int)end_frame);

  reverse_play = FALSE;
  if(begin_frame > end_frame)
  {
    gint32 val;

    val = end_frame;
    end_frame = begin_frame;
    begin_frame = val;
    reverse_play = TRUE;
  }

  if(gpp == NULL)
  {
    return;
  }

  p_stop_playback(gpp);

  if(gpp->audio_tmp_dialog_is_open)
  {
    /* ignore close as long as sub dialog is open */
    return;
  }

  if(gpp->preferred_decoder)
  {
    g_free(gpp->preferred_decoder);
    gpp->preferred_decoder = NULL;
  }
  if(preferred_decoder)
  {
    gpp->preferred_decoder = g_strdup(preferred_decoder);
  }
  gpp->force_open_as_video = force_open_as_video;
  gpp->delace = delace;
  gpp->seltrack = seltrack;
  gpp->stb_ptr = stb_ptr;
  gpp->image_id = image_id;
  gpp->imagewidth = imagewidth;
  gpp->imageheight = imageheight;
  gpp->play_selection_only = play_selection_only;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gpp->selonly_checkbutton), play_selection_only);

  if(gpp->imagename)
  {
    g_free(gpp->imagename);
    gpp->imagename = NULL;
  }

  if(gpp->stb_ptr)
  {
    GapStoryBoard *stb;

    stb = gpp->stb_ptr;

    if(gpp->ainfo_ptr)
    {
      gap_lib_free_ainfo(&gpp->ainfo_ptr);
    }

    gpp->stb_in_track = stb_in_track;
    gpp->ainfo_ptr = gap_story_fake_ainfo_from_stb(stb, stb_in_track);
    if(stb->master_framerate > 0.0)
    {
      gpp->original_speed = stb->master_framerate;
      gpp->prev_speed = stb->master_framerate;
    }
  }
  else
  {
    if(imagename)
    {
      gpp->imagename = g_strdup(imagename);
      p_reload_ainfo_ptr(gpp, -1);
      if(gpp->ainfo_ptr == NULL)
      {
        return;
      }
    }
    else
    {
      p_reload_ainfo_ptr(gpp, image_id);
      if(gpp->ainfo_ptr == NULL)
      {
        return;
      }

      if(0 != gap_lib_chk_framerange(gpp->ainfo_ptr))
      {
        return;
      }
    }
  }

  l_curr_frame = gpp->ainfo_ptr->curr_frame_nr;
  l_begin_frame = gpp->ainfo_ptr->curr_frame_nr;
  l_end_frame = gpp->ainfo_ptr->last_frame_nr;
  if(begin_frame >= 0)
  {
    l_begin_frame = CLAMP(begin_frame
                            , gpp->ainfo_ptr->first_frame_nr
                            , gpp->ainfo_ptr->last_frame_nr
                            );
  }
  if(end_frame >= 0)
  {
    l_end_frame = CLAMP(end_frame
                            , gpp->ainfo_ptr->first_frame_nr
                            , gpp->ainfo_ptr->last_frame_nr
                            );
  }
  l_curr_frame = CLAMP(l_curr_frame
                      ,l_begin_frame
                      ,l_end_frame
                      );
  gpp->begin_frame = l_begin_frame;
  gpp->end_frame = l_end_frame;
  gpp->play_current_framenr = l_curr_frame;

  /* modify player widgets */
  p_update_pviewsize(gpp);
  p_update_ainfo_dependent_widgets(gpp);

  if(gap_debug)
  {
    printf(" -- -- RESTART l_begin_frame:%d  l_end_frame:%d l_curr_frame:%d\n"
       ,(int)l_begin_frame
       ,(int)l_end_frame
       ,(int)l_curr_frame
        );
  }

  gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->from_spinbutton_adj)
                            , (gfloat)l_begin_frame
                            );
  gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->to_spinbutton_adj)
                            , (gfloat)l_end_frame
                            );
  gtk_adjustment_set_value( GTK_ADJUSTMENT(gpp->framenr_spinbutton_adj)
                            , (gfloat)l_curr_frame
                            );

  /* workaround: gtk_adjustment_set_value sometimes does not immediate
   * show the new value (especially in the  to_spinbutton widget)
   * to force an update we emit a "value_changed" signal
   */
  g_signal_emit_by_name (gpp->to_spinbutton_adj, "value_changed", 0);
  g_signal_emit_by_name (gpp->from_spinbutton_adj, "value_changed", 0);
  g_signal_emit_by_name (gpp->framenr_spinbutton_adj, "value_changed", 0);

  p_update_position_widgets(gpp);
  p_set_frame_with_name_label(gpp);

  gpp->flip_request = flip_request;
  gpp->flip_status = flip_status;

  /* now display current frame */
  p_display_frame(gpp, gpp->play_current_framenr);
  gap_pview_repaint(gpp->pv_ptr);
  gdk_flush();

  if((gpp->autostart)
  && ((gpp->play_current_framenr != gpp->begin_frame) || (gpp->play_current_framenr != gpp->end_frame)))
  {
     if(reverse_play)
     {
       on_back_button_clicked(NULL, NULL, gpp);
     }
     else
     {
       on_play_button_clicked(NULL, NULL, gpp);
     }
  }

}  /* end gap_player_dlg_restart */



/* -----------------------------
 * gap_player_dlg_create
 * -----------------------------
 * create the player widget (gpp->shell_window)
 */
void
gap_player_dlg_create(GapPlayerMainGlobalParams *gpp)
{
  gboolean  reverse_play;

  reverse_play = FALSE;
  if(gpp->begin_frame > gpp->end_frame)
  {
    gint32 val;

    val = gpp->end_frame;
    gpp->end_frame = gpp->begin_frame;
    gpp->begin_frame = val;
    reverse_play = TRUE;
  }

  gpp->startup = TRUE;
  gpp->onion_delete = FALSE;
  gpp->shell_initial_width = -1;
  gpp->shell_initial_height = -1;

  gimp_tile_cache_ntiles(gpp->cache_ntiles);
  p_init_tile_cache(gpp);
  p_init_video_playback_cache(gpp);
  p_init_layout_options(gpp);

  gpp->mtrace_image_id = -1;
  gpp->mtrace_mode = GAP_PLAYER_MTRACE_OFF;
  gpp->detail_tracking_checkbutton = NULL;

  gpp->vindex_creation_is_running = FALSE;
  gpp->request_cancel_video_api = FALSE;
  gpp->cancel_video_api = FALSE;
  gpp->gvahand = NULL;
  gpp->gva_videofile = NULL;
  gpp->seltrack = 1;
  gpp->delace = 0.0;
  gpp->ainfo_ptr = NULL;
  gpp->original_speed = 24.0;   /* default if framerate is unknown */
  gpp->prev_speed = 24.0;       /* default if framerate is unknown */

  gpp->stb_comp_vidhand = NULL;
  gpp->stb_parttype = -1;
  gpp->stb_unique_id = -1;

  if(gpp->stb_ptr)
  {
    GapStoryBoard *stb;

    stb = gpp->stb_ptr;
    gpp->ainfo_ptr = gap_story_fake_ainfo_from_stb(stb, gpp->stb_in_track);
    if(stb->master_framerate > 0.0)
    {
      gpp->original_speed = stb->master_framerate;
      gpp->prev_speed = stb->master_framerate;
    }
  }
  else
  {

    if(gpp->imagename)
    {
      p_reload_ainfo_ptr(gpp, -1);
      if(gpp->ainfo_ptr == NULL)
      {
        return;
      }
    }
    else
    {
      p_reload_ainfo_ptr(gpp, gpp->image_id);
      if(gpp->ainfo_ptr == NULL)
      {
        return;
      }

      if (gpp->standalone_mode == TRUE)
      {
        if(0 != gap_lib_chk_framerange(gpp->ainfo_ptr))
        {
          return;
        }
      }
    }
  }

  gpp->resize_handler_id = -1;
  gpp->play_is_active = FALSE;
  gpp->play_timertag = -1;
  gpp->go_timertag = -1;
  gpp->go_base_framenr = -1;
  gpp->go_base = -1;
  gpp->pingpong_count = 0;
  gpp->gtimer = g_timer_new();
  gpp->cycle_time_secs = 0.3;
  gpp->rest_secs = 0.0;
  gpp->framecnt = 0.0;
  gpp->audio_volume = 1.0;
  gpp->audio_resync = 0;
  gpp->audio_frame_offset = 0;
  gpp->audio_filesel = NULL;
  gpp->audio_tmp_dialog_is_open = FALSE;
  
  gpp->audio_otone_atrack = 1;
  gpp->audio_enable_checkbutton = NULL;
  gpp->audio_auto_offset_by_framenr_checkbutton = NULL;
  gpp->audio_otone_extract_button = NULL;
  gpp->audio_otone_atrack_spinbutton = NULL;
  gpp->audio_otone_atrack_spinbutton_adj = NULL;
  gpp->progress_bar_audio = NULL;

  if((gpp->autostart) || (gpp->imagename))
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

  /* always startup at original speed */
  gpp->speed = gpp->original_speed;
  gpp->prev_speed = gpp->original_speed;

  if((gpp->pv_pixelsize < GAP_PLAYER_MIN_SIZE) || (gpp->pv_pixelsize > GAP_PLAYER_MAX_SIZE))
  {
    gpp->pv_pixelsize = GAP_STANDARD_PREVIEW_SIZE;
  }
  if ((gpp->pv_width < GAP_PLAYER_MIN_SIZE) || (gpp->pv_width > GAP_PLAYER_MAX_SIZE))
  {
    gpp->pv_width = GAP_STANDARD_PREVIEW_SIZE;
  }
  if ((gpp->pv_height < GAP_PLAYER_MIN_SIZE) || (gpp->pv_height > GAP_PLAYER_MAX_SIZE))
  {
    gpp->pv_height = GAP_STANDARD_PREVIEW_SIZE;
  }

  gpp->in_feedback = FALSE;
  gpp->in_timer_playback = FALSE;
  gpp->old_resize_width = 0;
  gpp->old_resize_height = 0;

  gpp->dvref_ptr = g_new(GapDrawableVideoRef, 1);
  gpp->dvref_ptr->videofile = NULL;

  p_create_player_window(gpp);
  p_set_frame_with_name_label(gpp);

  p_display_frame(gpp, gpp->play_current_framenr);
  gap_pview_repaint(gpp->pv_ptr);
  gdk_flush();

  p_check_tooltips();

  /* set audio status to unchecked state
   * and do p_audio_startup_server(gpp); deferred on 1st attempt to enter audiofile
   */
  gpp->audio_status = GAP_PLAYER_MAIN_AUSTAT_UNCHECKED;

  if(gpp->autostart)
  {
     if(reverse_play)
     {
       on_back_button_clicked(NULL, NULL, gpp);
     }
     else
     {
       on_play_button_clicked(NULL, NULL, gpp);
     }
  }
  if((gpp->docking_container == NULL)
  && (gpp->shell_window))
  {
    gtk_widget_show (gpp->shell_window);

    g_signal_connect (G_OBJECT (gpp->shell_window), "size_allocate",
                      G_CALLBACK (on_shell_window_size_allocate),
                      gpp);

    g_signal_connect (G_OBJECT (gpp->shell_window), "leave_notify_event",
                      G_CALLBACK (on_shell_window_leave),
                      gpp);
  }
  gpp->startup = FALSE;
  /* close videofile after startup.
   * this is done to enable videoindex creation
   * at next p_open_videofile call
   */
  p_close_videofile(gpp);

  p_connect_resize_handler(gpp);

}  /* end gap_player_dlg_create */


/* -----------------------------
 * gap_player_dlg_cleanup
 * -----------------------------
 */
void
gap_player_dlg_cleanup(GapPlayerMainGlobalParams *gpp)
{
    p_audio_shut_server(gpp);

    if(gpp->gtimer)
    {
      g_timer_destroy(gpp->gtimer);
      gpp->gtimer = NULL;
    }

    if(gpp->ainfo_ptr)
    {
      gap_lib_free_ainfo(&gpp->ainfo_ptr);
    }

    gap_player_cache_free_all();

}  /* end gap_player_dlg_cleanup */


/* ------------------------------
 * gap_player_dlg_playback_dialog
 * ------------------------------
 * This procedure does Playback
 * in standalone mode.
 * (when invoked from an anim frame image via the Video/Playback Menu)
 */
void
gap_player_dlg_playback_dialog(GapPlayerMainGlobalParams *gpp)
{
  gimp_ui_init ("gap_player_dialog", FALSE);
  gap_stock_init();
  p_check_tooltips();

  gpp->fptr_set_range = NULL;
  gpp->user_data_ptr = NULL;
  gpp->imagename = NULL;
  gpp->docking_container = NULL;
  gpp->preferred_decoder = NULL;
  gpp->have_progress_bar = FALSE;
  gpp->progress_bar_idle_txt = g_strdup(" ");
  gpp->exact_timing = TRUE;
  gpp->caller_range_linked = FALSE;
  gpp->aspect_ratio = GAP_PLAYER_DONT_FORCE_ASPECT;

  gap_player_dlg_create(gpp);
  if(gpp->shell_window)
  {
    gpp->standalone_mode = TRUE;
    gtk_main ();
    gap_player_dlg_cleanup(gpp);
  }

  gpp->ainfo_ptr = NULL;
  gpp->stb_ptr = NULL;
  gpp->gvahand = NULL;
  gpp->gva_videofile = NULL;


}  /* end gap_player_dlg_playback_dialog */




