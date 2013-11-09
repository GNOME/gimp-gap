/* gap_decode_mplayer.c
 * 2004.12.06 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 *
 *        GIMP/GAP-frontend interface for mplayer
 *         Calls mplayer to split any mplayer supported video into
 *         video frames (single images on disk)
 *         Audio can also be extracted.
 *
 *        mplayer  exporting edition is available at:
 *            Web:        http://www.mplayerhq.hu/homepage/design7/news.html
 *
 * Warning: This Module needs UNIX environment to run.
 *   It uses programs and commands that are NOT available
 *   on other Operating Systems (Win95, NT, XP ...)
 *
 *     - mplayer            MPlayer 1.0pre5 or MPlayer 1.0pre7
 *                          (the best mediaplayer for linux)
 *                          set environment GAP_MPLAYER_PROG to configure where to find mplayer
 *                          (default: search mplayer in your PATH)
 *     - cd                 (UNIX command)
 *     - rm                 (UNIX command is used to delete by wildcard (expanded by /bin/sh)
 *                                and to delete a directory with all files
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

/* revision history
 * gimp-gap    2.1;     2005/05/22  hof: support MPlayer1.0pre7 (has new calling options)
 * gimp-gap    2.1;     2004/11/29  hof: created
 */


/*
 *    Here is a list of the mplayer 1.0 extracting related options
 *    that were used in this frontend.
 *    Please Note: Options changed incompatible from 1.0pre5 to 1.0pre7 release,
 *                 the old MPlayer1.0pre5 options can be selected via dialog.
 *
 *    -ss <time> (see -sb option too)
 *                  Seek to given time position.
 *
 *                  EXAMPLE:
 *                        -ss 56
 *                                seeks to 56 seconds
 *                        -ss 01:10:00
 *                                seeks to 1 hour 10 min
 *
 *    -frames <number>
 *       Play/convert only first <number> frames, then quit.
 *
 *    -vo png
 *       set video output to png device (this device saves png frames to disc)
 *    -vo jpeg
 *       set video output to jpeg device (this device saves jpeg frames to disc)
 *
 *    -jpeg <option1:option2:...> (-vo jpeg only)  # old syntax for Mplayer1.0pre5
 *            Specify options for the JPEG output.
 *            Available options are:
 *
 *                  [no]progressive
 *                          Specify standard or progressive JPEG.
 *                  [no]baseline
 *                          Specify use of baseline or not.
 *                  optimize=<value>
 *                          Optimization factor [0-100]
 *                  smooth=<value>
 *                          Smooth factor [0-100]
 *                  quality=<value>
 *                          Quality factor [0-100]
 *                  outdir=<value>
 *                          Directory to save the JPEG files
 *    -z <0-9> (-vo png only)   # old syntax for Mplayer1.0pre5
 *                  Specifies compression level for PNG output.
 *                        0    no compression
 *                        9    max compression
 *
 *    -dvdangle <angle id> (DVD only)
 *               Some DVD discs contain scenes that  can  be  viewed
 *               from  multiple  angles.   Here you can tell MPlayer
 *               which angles to use (default: 1).  Examples can  be
 *               found below.
 *
 *
 *    -ao pcm -aofile <filename>   # syntax for MPlayer 1.0pre5
 *    -ao pcm:file=<filename>      # syntax for MPlayer 1.0pre7 or latest CVS
 *       set audiooutput to pcm device (writes PCM wavefiles to disc)
 *
 *
 *    -aid <id> (also see -alang option)
 *                  Select audio channel [MPEG: 0-31 AVI/OGM: 1-99 ASF/
 *                  RM:  0-127  VOB(AC3):  128-159  VOB(LPCM):  160-191
 *                  MPEG-TS 17-8190].  MPlayer prints the available IDs
 *                  when running in verbose (-v) mode.  When playing an
 *                  MPEG-TS stream, MPlayer/Mencoder will use the first
 *                  program (if present) with the chosen audio  stream.
 *    -nosound
 *                  Do not play/encode sound.  Useful for benchmarking.
 *
 *    -novideo
 *                  Do not play/encode video.
 *
 *
 *    examples (MPlayer 1.0pre5):
 *    -----------------------
 *      mplayer  -ss 00:00:14 -frames 200 -ao pcm -aofile extracted_audio.wav  videoinput.rm
 *
 *        ==> extracts wav file named audiodump.wav
 *
 *      mplayer -vo jpeg -ss 00:00:14 -frames 150 -jpeg quality=92:smooth=70 videoinput.rm
 *
 *        ==> extracts 150 jpeg frames starting at second 13
 *
 *         00000001.jpg
 *      ..
 *      mplayer -vo png -ss 00:00:14 -frames 25 -z 9 videoinput.rm
 *
 *        ==> extracts 25 png frames in best compression starting at second 13
 *
 *         00000001.jpg
 *      ..
 *    examples (MPlayer 1.0pre7  or latest CVS):
 *    -----------------------
 *      mplayer  -ss 00:00:14 -frames 200 -ao pcm:file=extracted_audio.wav  videoinput.rm
 *
 *        ==> extracts wav file named audiodump.wav
 *
 *      mplayer -vo jpeg:quality=92:smooth=70 -ss 00:00:14 -frames 150  videoinput.rm
 *
 *        ==> extracts 150 jpeg frames starting at second 13   WARNING does not work yet
 *
 *         00000001.jpg
 *      ..
 *
 *      mplayer -vo png:z=9 -ss 00:00:14 -frames 25  videoinput.mpg
 *        ==> extracts 25 png frames in best compression starting at second 13   WARNING does not work yet
 */



/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib/gstdio.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "config.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"

#ifdef G_OS_WIN32
#include <io.h>
#  ifndef S_ISDIR
#    define S_ISDIR(m) ((m) & _S_IFDIR)
#  endif
#  ifndef S_ISREG
#    define S_ISREG(m) ((m) & _S_IFREG)
#  endif
#endif


#define MPLAYER_PROG "mplayer"



/* GAP includes */
#include "gap_libgapbase.h"
#include "gap_lib.h"
#include "gap_arr_dialog.h"
#include "gap_decode_mplayer.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */


static gchar *global_errlist = NULL;

gint32  global_delete_number;
/* fileformats supported by gap_decode_mplayer */

/* ============================================================================
 * p_mplayer_info
 * ============================================================================
 * this dialog is shown when errors occurred after attempt
 * to call mplayer.
 * Provides some informations what is required,
 * and the information about error.
 */
static int
p_mplayer_info(char *errlist)
{
  GapArrArg  argv[22];
  GapArrButtonArg  b_argv[2];

  int        l_idx;
  int        l_rc;


  l_idx = 0;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = _("Requirements to run the mplayer based video split");

  l_idx++;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = "";

  l_idx++;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = "1.)";

  l_idx++;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = _("mplayer 1.0 must be installed somewhere in your PATH\n"
                            "you can get mplayer exporting edition at:\n"
                           );

  l_idx++;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = "http://www.mplayerhq.hu";


  l_idx++;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = "";

  l_idx++;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = "2.)";

  l_idx++;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = _("if your mplayer is not in your PATH or is not named mplayer\n"
                            "you have to set environment variable GAP_MPLAYER_PROG\n"
                            "to your mplayer program and restart gimp"
                           );

  l_idx++;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = "";

  l_idx++;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = _("An error occurred while trying to call mplayer:");

  l_idx++;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = "--------------------------------------------";


  l_idx++;
  gap_arr_arg_init(&argv[l_idx], GAP_ARR_WGT_LABEL_LEFT);
  argv[l_idx].label_txt = errlist;


  l_idx++;

  /* the  Action Button */
    b_argv[0].but_txt  = GTK_STOCK_CANCEL;
    b_argv[0].but_val  = -1;
    b_argv[1].but_txt  = GTK_STOCK_OK;
    b_argv[1].but_val  = 0;

  l_rc = gap_arr_std_dialog(_("mplayer Information"),
                             "",
                              l_idx,   argv,      /* widget array */
                              1,       b_argv,    /* button array */
                              -1);

  return (l_rc);
}       /* end p_mplayer_info */


/* -----------------------
 * p_scann_start_time
 * -----------------------
 * scan hour, minute, second from a string
 * that has the format "HH:MM:SS"
 * where any other non-digit characters are treated same as ":"
 * strings with format "HHMMSS" are also accepted.
 *
 * This procedure does not check if MM and SS values are in the range
 * 00 upto 59
 */
static void
p_scann_start_time(char *buf, GapMPlayerParams *gpp)
{
  gint ii;
  gint tab_idx;
  gint num;
  gint factor;
  gint tab[3];
  gboolean num_ready;


  tab[0] = 0;
  tab[1] = 0;
  tab[2] = 0;

  tab_idx = 0;
  num = 0;
  factor = 1;
  num_ready = FALSE;

  ii = strlen(buf);
  while(TRUE)
  {
    ii--;

    if((buf[ii] >= '0') && (buf[ii] <= '9'))
    {
      num += ((buf[ii] - (gint)'0') * factor);
      factor = factor * 10;
      if((factor > 10)  /* 2 digits done ? */
      || (ii==0))       /* last character handled ? */
      {
        num_ready = TRUE;
      }
    }
    else
    {
      /* current char is no digit */
      if(factor > 1)
      {
        /* at least one digit was collected in num */
        num_ready = TRUE;
      }
    }

    if(num_ready)
    {
      if(tab_idx < 3)
      {
        tab[tab_idx] = num;
        tab_idx++;
      }
      num = 0;
      factor = 1;
      num_ready = FALSE;
    }

    if(ii <= 0)
    {
      break;
    }
  }

  gpp->start_hour   = tab[2];
  gpp->start_minute = tab[1];
  gpp->start_second = tab[0];

}  /* end p_scann_start_time */


/* -----------------------
 * p_mplayer_dialog
 * -----------------------
 * the main dialog for the mplayer based video extract settings
 */
static int
p_mplayer_dialog   (GapMPlayerParams *gpp)
{
#define MPDIALOG_SMALL_ENTRY_WIDTH 80
#define MPDIALOG_LARGE_ENTRY_WIDTH 250
#define MPDIALOG_NUM_ARGS 22
  static GapArrArg  argv[MPDIALOG_NUM_ARGS];
  static char *radio_args[3]  = { "XCF", "PNG", "JPEG" };

  char buf_start_time[10];
  char err_msg_buffer[1000];

  gint ii;
  gint ii_num_frames;
  gint ii_vtrack;
  gint ii_atrack;
  gint ii_img_format;
  gint ii_png_compression;
  gint ii_jpg_quality;
  gint ii_jpg_optimize;
  gint ii_jpg_smooth;
  gint ii_jpg_progressive;
  gint ii_jpg_baseline;
  gint ii_autoload;
  gint ii_silent;
  gint ii_async;
  gint ii_old_syntax;

  err_msg_buffer[0] = '\0';
  g_snprintf(buf_start_time, sizeof(buf_start_time), "%02d:%02d:%02d"
     ,(int)gpp->start_hour
     ,(int)gpp->start_minute
     ,(int)gpp->start_second
     );


  /* the loop shows the dialog again until
   * - parameters are OK (valid starttime, videofile exists)
   * - CANCEL button is pressed
   */
  while(TRUE)
  {

  ii=0;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FILESEL);
  argv[ii].label_txt = _("Input Video:");
  argv[ii].help_txt  = _("Name of a videofile to read by mplayer. "
                        "Frames are extracted from the videofile "
                        "and written to separate diskfiles. "
                        "mplayer 1.0 is required.");
  argv[ii].text_buf_len = sizeof(gpp->video_filename);
  argv[ii].text_buf_ret = gpp->video_filename;
  argv[ii].entry_width = MPDIALOG_LARGE_ENTRY_WIDTH;

  if(err_msg_buffer[0] != '\0')
  {
    ii++;
    gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_LABEL_LEFT);
    argv[ii].label_txt = err_msg_buffer;
  }

  ii++;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TEXT);
  argv[ii].label_txt = _("Start Time:");
  argv[ii].help_txt  = _("Extracting starts at the specified time offset HH:MM:SS in the video");
  argv[ii].text_buf_len = sizeof(buf_start_time);
  argv[ii].text_buf_ret = buf_start_time;
  argv[ii].entry_width = MPDIALOG_SMALL_ENTRY_WIDTH;



  ii++; ii_num_frames = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT);
  argv[ii].label_txt = _("Frames:");
  argv[ii].help_txt  = _("Number of frames to extract");
  argv[ii].constraint = TRUE;
  argv[ii].int_min    = 1;
  argv[ii].int_max    = 999999;
  argv[ii].int_ret    = gpp->number_of_frames;
  argv[ii].umin       = 1;
  argv[ii].umax       = 999999;
  argv[ii].entry_width = MPDIALOG_SMALL_ENTRY_WIDTH;

  ii++; ii_vtrack = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT);
  argv[ii].label_txt = _("Videotrack:");
  argv[ii].help_txt  = _("Number of videotrack to extract. (0 == ignore video)");
  argv[ii].constraint = TRUE;
  argv[ii].int_min    = 0;
  argv[ii].int_max    = 20;
  argv[ii].int_ret    = gpp->vtrack;
  argv[ii].umin       = 0;
  argv[ii].umax       = 20;
  argv[ii].entry_width = MPDIALOG_SMALL_ENTRY_WIDTH;

  ii++; ii_atrack = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT);
  argv[ii].label_txt = _("Audiotrack:");
  argv[ii].help_txt  = _("Number of audiotrack to extract. (0 == ignore audio)");
  argv[ii].constraint = TRUE;
  argv[ii].int_min    = 0;
  argv[ii].int_max    = 20;
  argv[ii].int_ret    = gpp->atrack;
  argv[ii].umin       = 0;
  argv[ii].umax       = 20;
  argv[ii].entry_width = MPDIALOG_SMALL_ENTRY_WIDTH;


  ii++;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FILESEL);
  argv[ii].label_txt = _("Output Audio:");
  argv[ii].help_txt  = _("Filename for the extracted audiodata. "
                        "Audiodata is written in RIFF WAV fileformat "
                        "(but only if audiotrack >= 1)");
  argv[ii].text_buf_len = sizeof(gpp->audio_filename);
  argv[ii].text_buf_ret = gpp->audio_filename;
  argv[ii].entry_width = MPDIALOG_LARGE_ENTRY_WIDTH;

  ii++;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_FILESEL);
  argv[ii].label_txt = _("Framenames:");
  argv[ii].help_txt  = _("Basename for the video frames to write on disk. "
                        "Framenumber and extension is added.");
  argv[ii].text_buf_len = sizeof(gpp->basename);
  argv[ii].text_buf_ret = gpp->basename;
  argv[ii].entry_width = MPDIALOG_LARGE_ENTRY_WIDTH;


  ii++; ii_img_format = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_OPTIONMENU);
  argv[ii].label_txt = _("Format:");
  argv[ii].help_txt  = _("Image fileformat for the extracted video frames. "
                       "(xcf is extracted as png and converted to xcf)");
  argv[ii].radio_argc  = 3;
  argv[ii].radio_argv = radio_args;
  argv[ii].radio_ret  = gpp->img_format;


  ii++; ii_png_compression = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT);
  argv[ii].label_txt = _("Png Compression:");
  argv[ii].help_txt  = _("Compression for resulting png frames where 0 is uncompressed (fast), 9 is max. compression "
                        "(this option is ignored when JPEG format is used)");
  argv[ii].constraint = TRUE;
  argv[ii].int_min    = 0;
  argv[ii].int_max    = 9;
  argv[ii].int_ret    = gpp->png_compression;

  ii++; ii_jpg_quality = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT);
  argv[ii].label_txt = _("Jpeg Quality:");
  argv[ii].help_txt  = _("Quality for resulting jpeg frames where 100 is best quality "
                        "(ignored when other formats are used)");
  argv[ii].constraint = TRUE;
  argv[ii].int_min    = 0;
  argv[ii].int_max    = 100;
  argv[ii].int_ret    = gpp->jpg_quality;


  ii++; ii_jpg_optimize = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT);
  argv[ii].label_txt = _("Jpeg Optimize:");
  argv[ii].help_txt  = _("optimization factor"
                        "(is ignored when other formats are used)");
  argv[ii].constraint = TRUE;
  argv[ii].int_min    = 0;
  argv[ii].int_max    = 100;
  argv[ii].int_ret    = gpp->jpg_optimize;


  ii++; ii_jpg_smooth = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_INT);
  argv[ii].label_txt = _("Jpeg Smooth:");
  argv[ii].help_txt  = _("Smooth factor"
                        "(is ignored when other formats are used)");
  argv[ii].constraint = TRUE;
  argv[ii].int_min    = 0;
  argv[ii].int_max    = 100;
  argv[ii].int_ret    = gpp->jpg_smooth;


  ii++; ii_jpg_progressive = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE);
  argv[ii].label_txt = _("Jpeg Progressive:");
  argv[ii].help_txt  = _("Enable progressive jpeg encoding"
                        "(is ignored when other formats are used)");
  argv[ii].int_ret   = gpp->jpg_progressive;

  ii++; ii_jpg_baseline = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE);
  argv[ii].label_txt = _("Jpeg Baseline:");
  argv[ii].help_txt  = _("Enable baseline jpeg encoding"
                        "(is ignored when other formats are used)");
  argv[ii].int_ret   = gpp->jpg_baseline;

  ii++;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_LABEL);
  argv[ii].label_txt = "";


  ii++; ii_silent = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE);
  argv[ii].label_txt = _("Silent");
  argv[ii].help_txt  = _("use -nosound (-novideo) in case audiotrack (videotrack) is 0.\n"
                         "mplayer will operate silent and faster.");
  argv[ii].int_ret   = gpp->silent;

  ii++; ii_autoload = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE);
  argv[ii].label_txt = _("Open");
  argv[ii].help_txt  = _("Open the 1st one of the extracted frames");
  argv[ii].int_ret   = gpp->autoload;

  ii++; ii_async = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE);
  argv[ii].label_txt = _("Asynchronous");
  argv[ii].help_txt  = _("Run the mplayer as asynchronous process");
  argv[ii].int_ret   = gpp->run_mplayer_asynchron;


  ii++; ii_old_syntax = ii;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_TOGGLE);
  argv[ii].label_txt = _("MPlayer 1.0pre5:");
  argv[ii].help_txt  = _("ON: use deprecated options for mplayer 1.0pre5\n"
                         "OFF: use options for newer mplayer\n"
                        " Dont turn on the deprecated options if you have mplayer 1.0pre7 or newer mplayer versions");
  argv[ii].int_ret   = gpp->use_old_mplayer1_syntax;

  ii++;
  gap_arr_arg_init(&argv[ii], GAP_ARR_WGT_HELP_BUTTON);
  argv[ii].help_id = GAP_MPLAYER_HELP_ID;

  ii++;

  if(TRUE == gap_arr_ok_cancel_dialog(_("MPlayer based extraction"),
                            _("Select Frame Range"), ii, argv))
  {
     gboolean params_ok;
     p_scann_start_time(buf_start_time, gpp);

     if(gap_debug)
     {
       printf("# gpp->start_hour:   %d\n", (int)gpp->start_hour );
       printf("# gpp->start_minute: %d\n", (int)gpp->start_minute );
       printf("# gpp->start_second: %d\n", (int)gpp->start_second );
     }

     gpp->number_of_frames  = (gint32)(argv[ii_num_frames].int_ret);
     gpp->vtrack            = (gint32)(argv[ii_vtrack].int_ret);
     gpp->atrack            = (gint32)(argv[ii_atrack].int_ret);
     gpp->png_compression   = (gint32)(argv[ii_png_compression].int_ret);
     gpp->jpg_quality       = (gint32)(argv[ii_jpg_quality].int_ret);
     gpp->jpg_optimize      = (gint32)(argv[ii_jpg_optimize].int_ret);
     gpp->jpg_smooth        = (gint32)(argv[ii_jpg_smooth].int_ret);
     gpp->jpg_progressive   = (gint32)(argv[ii_jpg_progressive].int_ret);
     gpp->jpg_baseline      = (gint32)(argv[ii_jpg_baseline].int_ret);

     gpp->img_format        = (GapMPlayerFormats)(argv[ii_img_format].radio_ret);
     gpp->silent            = (gboolean)(argv[ii_silent].int_ret);
     gpp->autoload          = (gboolean)(argv[ii_autoload].int_ret);
     gpp->run_mplayer_asynchron  = (gboolean)(argv[ii_async].int_ret);
     gpp->use_old_mplayer1_syntax  = (gboolean)(argv[ii_old_syntax].int_ret);

     params_ok = TRUE;
     if(!g_file_test(gpp->video_filename, G_FILE_TEST_EXISTS))
     {
       params_ok = FALSE;
       g_snprintf(err_msg_buffer, sizeof(err_msg_buffer)
                 , _("videofile %s not existent\n")
                 , gpp->video_filename
                 );
     }
     if((gpp->start_minute > 59)
     || (gpp->start_second > 59))
     {
       params_ok = FALSE;
       g_snprintf(err_msg_buffer, sizeof(err_msg_buffer)
                 ,_("Illegal starttime %s")
                 , buf_start_time
                 );
     }

     if(params_ok)
     {
       return (0);    /* OK */
     }
  }
  else
  {
     return -1;     /* Cancel */
  }

  }
}       /* end p_mplayer_dialog */


/* ------------------------------
 * p_overwrite_dialog
 * ------------------------------
 */
static gint
p_overwrite_dialog(char *filename, gint overwrite_mode)
{
  static  GapArrButtonArg  l_argv[3];
  static  GapArrArg  argv[1];

  if(g_file_test(filename, G_FILE_TEST_EXISTS))
  {
    if (overwrite_mode < 1)
    {
       l_argv[0].but_txt  = _("Overwrite Frame");
       l_argv[0].but_val  = 0;
       l_argv[1].but_txt  = _("Overwrite All");
       l_argv[1].but_val  = 1;
       l_argv[2].but_txt  = GTK_STOCK_CANCEL;
       l_argv[2].but_val  = -1;

       gap_arr_arg_init(&argv[0], GAP_ARR_WGT_LABEL);
       argv[0].label_txt = filename;

       return(gap_arr_std_dialog ( _("GAP Question"),
                                   _("File already exists"),
                                   1, argv,
                                   3, l_argv, -1));
    }
  }
  return (overwrite_mode);
}  /* end p_overwrite_dialog */



/* ------------------------------
 * p_build_mplayer_framename
 * ------------------------------
 * MPlayer extracted frames are named with an 8digit number and extension
 * and created in the current directory.
 * (for the mplayer call the current directory is set to gpp->mplayer_working_dir)
 * example
 *   00000001.png
 *   00000002.png
 *   ...
 */
static void
p_build_mplayer_framename(GapMPlayerParams *gpp, char *framename, gint32 sizeof_framename, gint32 frame_nr, char *ext)
{
   g_snprintf(framename, sizeof_framename, "%s%s%08d.%s"
              , gpp->mplayer_working_dir
              , G_DIR_SEPARATOR_S
              , (int)frame_nr
              ,ext
              );
}

/* ------------------------------
 * p_build_gap_framename
 * ------------------------------
 */
static void
p_build_gap_framename(char *framename, gint32 sizeof_framename, gint32 frame_nr, char *basename, char *ext)
{
   g_snprintf(framename, sizeof_framename, "%s%06d.%s", basename, (int)frame_nr, ext);
}


/* ------------------------------
 * p_dirname
 * ------------------------------
 */
static void
p_dirname(char *fname)
{
  int l_idx;

  l_idx = strlen(fname) -1;
  while(l_idx > 0)
  {
     if(fname[l_idx] == G_DIR_SEPARATOR)
     {
        fname[l_idx] = '\0';
        return;
     }
     l_idx--;
  }
  *fname = '\0';
}  /* end p_dirname */


/* ------------------------------
 * p_init_mplayer_working_dir
 * ------------------------------
 * build the name for a temporary directory
 * where mplayer will run and writes the extracted frames.
 *
 * the first guess is making a subdirectory at same location
 * where the resulting frames will be stored.
 * (has the advantage that fast renaming is possible
 *  on the same device)
 *
 * the 2nd gues uses a subdirectory loacted at the users home directory.
 */
static void
p_init_mplayer_working_dir(GapMPlayerParams *gpp)
{
  gchar        tmp_dir[200];
  gchar       *absolute;
  pid_t        l_pid;


  /* include process id of the current process in the temp dir name */
  l_pid = gap_base_getpid();
  g_snprintf(tmp_dir, sizeof(tmp_dir), "tmp_mplayer_frames.%d", (int)l_pid);

  if((gpp->basename[0] != '\0')
  && (gpp->vtrack > 0))
  {
    if (! g_path_is_absolute (gpp->basename))
    {
        gchar *current;

        current = g_get_current_dir ();
        absolute = g_build_filename (current, gpp->basename, NULL);
        g_free (current);
    }
    else
    {
        absolute = g_strdup (gpp->basename);
    }

    p_dirname(absolute);

    /* if destination directorypart does not exist, try to create it */
    if (! g_file_test(absolute, G_FILE_TEST_IS_DIR))
    {
       gap_file_mkdir (absolute, GAP_FILE_MKDIR_MODE);
    }

    gpp->mplayer_working_dir = g_build_filename(absolute, tmp_dir, NULL);

    g_free (absolute);

    return;
  }

  /* use gimp_temp_name to find a writeable tempdir
   * (used as mplayer workingdir)
   */

  gpp->mplayer_working_dir = g_build_filename(g_get_home_dir(), tmp_dir, NULL);

}  /* end p_init_mplayer_working_dir */

/* -----------------------
 * p_convert_frames
 * -----------------------
 * this step converts the frames that were extracted by mplayer (from png)
 * to another fileformat (to xcf)
 * the destination frames also get the desired naming.
 */
static int
p_convert_frames(GapMPlayerParams *gpp, gint32 frame_from, gint32 frame_to, char *basename, char *ext, char *ext2)
{
   GimpParam          *return_vals;
   int              nreturn_vals;
   gint32           l_tmp_image_id;
   char             l_first_mp_frame[500];
   int              l_rc;

   l_rc = -1;

  /* load 1st one of those frames generated by mplayer  */
   p_build_mplayer_framename(gpp, l_first_mp_frame, sizeof(l_first_mp_frame), frame_from, ext);
   l_tmp_image_id = gap_lib_load_image(l_first_mp_frame);

   /* convert the mplayer frames (from png) to xcf fileformat
    * (the gap module for range convert is not linked to the frontends
    *  main program, therefore i call the convert procedure via PDB-interface)
    */
   return_vals = gimp_run_procedure ("plug_in_gap_range_convert2",
                                    &nreturn_vals,
                                    GIMP_PDB_INT32, GIMP_RUN_NONINTERACTIVE,     /* runmode  */
                                    GIMP_PDB_IMAGE, l_tmp_image_id,
                                    GIMP_PDB_DRAWABLE, 0,         /* (unused)  */
                                    GIMP_PDB_INT32, frame_from,
                                    GIMP_PDB_INT32, frame_to,
                                    GIMP_PDB_INT32, 0,            /* dont flatten */
                                    GIMP_PDB_INT32, 4444,         /* dest type (keep type) */
                                    GIMP_PDB_INT32, 256,          /* colors (unused)  */
                                    GIMP_PDB_INT32, 0,            /* no dither (unused)  */
                                    GIMP_PDB_STRING, ext2,        /* extension for dest. filetype  */
                                    GIMP_PDB_STRING, basename,    /* basename for dest. filetype  */
                                    GIMP_PDB_INT32, 0,            /* (unused)  */
                                    GIMP_PDB_INT32, 0,            /* (unused)  */
                                    GIMP_PDB_INT32, 0,            /* (unused)  */
                                    GIMP_PDB_STRING, "none",      /* (unused) palettename */
                                    GIMP_PDB_END);

   /* destroy the tmp image */
   gimp_image_delete(l_tmp_image_id);

   if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
   {
      l_rc = 0;   /* OK */
   }
   gimp_destroy_params (return_vals, nreturn_vals);

   return(l_rc);
}

/* ---------------------------
 * p_find_max_mplayer_frame
 * ---------------------------
 * check the names of the extracted frames
 * and search for the max framenumber.
 */
static gint32
p_find_max_mplayer_frame(GapMPlayerParams *gpp, gint32 from_nr, char *ext)
{
  gint32 l_high;
  gint32 l_max_found;
  gint32 l_nr;
  gint32 l_delta;
  char   l_frame[500];

  l_nr = from_nr;
  l_max_found = 0;
  l_high = 99999999;

  while(1 == 1)
  {
     p_build_mplayer_framename(gpp, l_frame, sizeof(l_frame), l_nr, ext);

     if(gap_debug) printf("DEBUG find_MAX :%s\n", l_frame);

     if(g_file_test(l_frame, G_FILE_TEST_EXISTS))
     {
        l_max_found = l_nr;
        l_delta = (l_high - l_nr) / 2;
        if(l_delta == 0)
        {
           l_delta = 1;
        }
        l_nr = l_max_found + l_delta;
     }
     else
     {
        if(l_nr == from_nr) { return (-1); }  /* no frames found */

        if(l_nr < l_high)
        {
           l_high = l_nr;
           l_nr = l_max_found + 1;
        }
        else
        {
           return(l_max_found);
        }
     }
  }
}       /* end p_find_max_mplayer_frame */

/* ---------------------
 * p_rename_frames
 * ---------------------
 * rename the extracted frames (they are locatred in a temp directory)
 * to the required name.
 * If the destination is on the same device, this will be done
 * fast via renaming.
 * If renaming is not possible (ondifferent devices)
 * the frames will be moved via  copy / remove sequence.
 */
static int
p_rename_frames(GapMPlayerParams *gpp, gint32 frame_from, gint32 frame_to, char *basename, char *ext)
{
  gint32 l_use_mv;
  gint32 l_frame_nr;
  gint32 l_max_found;
  char   l_src_frame[500];
  char   l_dst_frame[500];
  gint    l_overwrite_mode;


  if(gap_debug) printf("p_rename_frames:\n");
  l_use_mv = TRUE;
  l_overwrite_mode = 0;


  l_max_found = p_find_max_mplayer_frame (gpp, frame_from, ext);
  if(l_max_found < 0)
  {
       global_errlist = g_strdup_printf(
               _("can't find any extracted frames,\n%s\nmaybe mplayer has failed or was cancelled"),
               l_src_frame);
       return(-1);
  }


  l_frame_nr = frame_from;

  while (l_frame_nr <= frame_to)
  {
     p_build_mplayer_framename(gpp, l_src_frame, sizeof(l_src_frame), l_frame_nr, ext);
     p_build_gap_framename(l_dst_frame, sizeof(l_dst_frame), l_frame_nr, basename, ext);

     if(!g_file_test(l_src_frame, G_FILE_TEST_EXISTS))
     {
        break;  /* srcfile not found, stop */
     }

     if (strcmp(l_src_frame, l_dst_frame) != 0)
     {
        /* check overwrite if Destination frame already exsts */
        l_overwrite_mode = p_overwrite_dialog(l_dst_frame, l_overwrite_mode);
        if (l_overwrite_mode < 0)
        {
             global_errlist = g_strdup_printf(
                     _("frames are not extracted, because overwrite of %s was cancelled"),
                     l_dst_frame);
             return(-1);
        }
        else
        {
           g_remove(l_dst_frame);
           if(g_file_test(l_dst_frame, G_FILE_TEST_EXISTS))
           {
             global_errlist = g_strdup_printf(
                     _("failed to overwrite %s (check permissions ?)"),
                     l_dst_frame);
             return(-1);
           }
        }

        if (l_use_mv)
        {
           g_rename(l_src_frame, l_dst_frame);
        }

        if(!g_file_test(l_dst_frame, G_FILE_TEST_EXISTS))
        {
           gap_lib_file_copy(l_src_frame, l_dst_frame);
           if(g_file_test(l_dst_frame, G_FILE_TEST_EXISTS))
           {
              l_use_mv = FALSE; /* if destination is on another device use copy-remove strategy */
              g_remove(l_src_frame);
           }
           else
           {
              global_errlist = g_strdup_printf(
                     _("failed to write %s (check permissions ?)"),
                     l_dst_frame);
              return(-1);
           }
        }
     }
     l_frame_nr++;
     if(l_max_found > 0) gimp_progress_update ((gdouble)l_frame_nr / (gdouble)l_max_found);
  }
  return(0);
}       /* end p_rename_frames */


/* -----------------------------
 * p_init_and_check_mplayer
 * -----------------------------
 */
static gint
p_init_and_check_mplayer(GapMPlayerParams *gpp)
{
  gint l_rc;
  const char *cp;
  gboolean mplayer_prog_found;

  if (gap_debug) printf("p_init_and_check_mplayer\n");

  mplayer_prog_found = FALSE;

  l_rc = 0;  /* SUCCESS */

  /*
   * Set environmental values for the mplayer program:
   */

  /* check gimprc for the mplayer executable name (including path) */
  if ( (cp = gimp_gimprc_query("mplayer_prog")) != NULL )
  {
    gpp->mplayer_prog = g_strdup(cp);           /* Environment overrides compiled in default for WAVPLAYPATH */
    if(g_file_test (gpp->mplayer_prog, G_FILE_TEST_IS_EXECUTABLE) )
    {
      mplayer_prog_found = TRUE;
    }
    else
    {
      g_message(_("WARNING: your gimprc file configuration for the mediaplayer\n"
             "does not point to an executable program;\n"
             "the configured value for %s is: %s\n")
             , "mplayer_prog"
             , gpp->mplayer_prog
             );
    }
  }

  /* check environment variable for the mplayer program */
  if(!mplayer_prog_found)
  {
    if ( (cp = g_getenv("GAP_MPLAYER_PROG")) != NULL )
    {
      gpp->mplayer_prog = g_strdup(cp);         /* Environment overrides compiled in default for WAVPLAYPATH */
      if(g_file_test (gpp->mplayer_prog, G_FILE_TEST_IS_EXECUTABLE) )
      {
        mplayer_prog_found = TRUE;
      }
      else
      {
        g_message(_("WARNING: the environment variable %s\n"
               "does not point to an executable program;\n"
               "the current value is: %s\n")
               , "GAP_MPLAYER_PROG"
               , gpp->mplayer_prog
               );
      }
    }
  }

  if(!mplayer_prog_found)
  {
    if ( (cp = g_getenv("PATH")) != NULL )
    {
      gpp->mplayer_prog = gap_lib_searchpath_for_exefile(MPLAYER_PROG, cp);
      if(gpp->mplayer_prog)
      {
        mplayer_prog_found = TRUE;
      }
    }
  }


  if(!mplayer_prog_found)
  {
    l_rc = 10;  /* ERROR */
    global_errlist = g_strdup_printf(_("The mediaplayer executable file '%s' was not found.")
                 , MPLAYER_PROG
                 );
  }

  return (l_rc);
}  /* end p_init_and_check_mplayer */


/* -----------------------
 * p_poll
 * -----------------------
 * poll until asynchron mplayer process exits
 * and do checks for already extracted frames to show corresponding progress
 * (this assumes that the video contains all the request frames
 * otherwise the progress will be not correct)
 */
static void
p_poll(GapMPlayerParams *gpp, pid_t mplayer_pid, char *ext)
{
  gint32 l_max_frame;
  /* loop as long as the mplayer Process is alive */

  if(gap_debug) printf("poll started on mplayer pid: %d\n", (int)mplayer_pid);


  while (gap_base_is_pid_alive(mplayer_pid))
  {
    usleep(100000);  /* sleep 1 second, and let mplayer write some frames */

    /* check for the max_frame number out of the already extracted frames */
    l_max_frame = p_find_max_mplayer_frame (gpp, 1, ext);

    if(gap_debug)
    {
      printf("POLL: frames found: %d\n", (int)l_max_frame);
    }

    if(l_max_frame > 0)
    {
      gimp_progress_update ((gdouble)l_max_frame / (gdouble)gpp->number_of_frames);
    }
  }

  if(gap_debug) printf("poll ended on mplayer pid: %d\n", (int)mplayer_pid);
}       /* end p_poll */


/* -----------------------
 * p_start_mplayer_process
 * -----------------------
 *
 * build a cmd string
 *   "cd <dir>; mplayer <options> video_filename"
 * and run this string SYNCHRON as system command.
 * or asynchron (using a generated shellscript)
 */
static pid_t
p_start_mplayer_process(GapMPlayerParams *gpp)
{
   gchar  l_cmd[2500];
   gchar  l_buf[500];
   int    l_rc;
   pid_t  l_mplayer_pid;

   l_mplayer_pid = -1;

   if((gpp->vtrack > 0)
   && (!gpp->run_mplayer_asynchron))
   {
     /* use the working dir in case where frames should be extracted
      * (for asynchron process the cd is done later in a generated shellscript)
      */
     g_snprintf(l_buf, sizeof(l_buf), "cd \"%s\"; "
             , gpp->mplayer_working_dir
             );
   }
   else
   {
     l_buf[0] = '\0';
   }

   /* prepare args for the mplayer call */
   g_snprintf(l_cmd, sizeof(l_cmd), "%s%s -ss %02d:%02d:%02d -frames %d "
             , l_buf                            /* optional cd   gpp->mplayer_working_dir */
             , gpp->mplayer_prog                /* programname */
             , (int)gpp->start_hour
             , (int)gpp->start_minute
             , (int)gpp->start_second
             , (int)gpp->number_of_frames
             );

   if (gpp->atrack > 0)
   {
     if(gpp->atrack > 1)
     {
       /* select a non-default audiotrack (such as multilingual dvd stuff) */
       g_snprintf(l_buf, sizeof(l_buf), "-aid %d "
                    ,(int)gpp->atrack
                    );
       strcat(l_cmd, l_buf);
     }

     /* the -ao option selects the audio output device.
      * device pcm writes pcm encoded audiodata
      * to RIFF wave formated audiofiles on disc.
      * the -aofile option defines the filename for the wav file.
      */
     if(gpp->use_old_mplayer1_syntax)
     {
       strcat(l_cmd, "-ao pcm -aofile '");  // deprecated syntax
     }
     else
     {
       strcat(l_cmd, "-ao pcm:file='");
     }

     if(gpp->audio_filename[0] == '\0')
     {
       strcat(l_cmd, gpp->video_filename);
       strcat(l_cmd, ".wav");
     }
     else
     {
       strcat(l_cmd, gpp->audio_filename);
     }
     strcat(l_cmd, "' ");
   }
   else
   {
     if(gpp->silent)
     {
       strcat(l_cmd, "-nosound ");
     }
   }

   if (gpp->vtrack)
   {
     if(gpp->vtrack > 1)
     {
       /* select a non-default videotrack (usually another DVD angle) */
       g_snprintf(l_buf, sizeof(l_buf), "-dvdangle %d "
                    ,(int)gpp->vtrack
                    );
       strcat(l_cmd, l_buf);
     }

     /* the -vo option selects the video output device
      * devices jpeg and png are used to write frames to disc
      * in the corresponding image fileformat.
      */
     strcat(l_cmd, "-vo ");

     switch(gpp->img_format)
     {
       case MPENC_JPEG:
          if(gpp->use_old_mplayer1_syntax)
          {
            g_snprintf(l_buf, sizeof(l_buf), "jpeg -jpeg quality=%d:optimize=%d:smooth=%d"
                    ,(int)gpp->jpg_quality
                    ,(int)gpp->jpg_optimize
                    ,(int)gpp->jpg_smooth
                    );
          }
          else
          {
            g_snprintf(l_buf, sizeof(l_buf), "jpeg:quality=%d:optimize=%d:smooth=%d"
                    ,(int)gpp->jpg_quality
                    ,(int)gpp->jpg_optimize
                    ,(int)gpp->jpg_smooth
                    );
          }

          strcat(l_cmd, l_buf);

          if(gpp->jpg_progressive)
          {
            strcat(l_cmd, ":progressive");
          }
          else
          {
            strcat(l_cmd, ":noprogressive");
          }
          if(gpp->jpg_baseline)
          {
            strcat(l_cmd, ":baseline");
          }
          else
          {
            strcat(l_cmd, ":nobaseline");
          }
          strcat(l_cmd, " ");
          break;
       case MPENC_PNG:
       default:
          if(gpp->use_old_mplayer1_syntax)
          {
            g_snprintf(l_buf, sizeof(l_buf), "png -z %d"
                    ,(int)gpp->png_compression
                    );
          }
          else
          {
            g_snprintf(l_buf, sizeof(l_buf), "png:z=%d"
                    ,(int)gpp->png_compression
                    );
          }
          strcat(l_cmd, l_buf);   /* other formats extract as png,
                                   * and may need further processing for convert
                                   */
          break;
      }

   }
   else
   {
     if(gpp->silent)
     {
       strcat(l_cmd, "-novideo ");
     }
   }

   /* add the videofilename (in quotes to protect blanks) as last parameter */
   strcat(l_cmd, " \"");
   strcat(l_cmd, gpp->video_filename);
   strcat(l_cmd, "\"");



   /* ============= START ================= */
   if (gpp->run_mplayer_asynchron)
   {
     gchar *l_mplayer_startscript;
     gchar *l_mplayer_pidfile;
     FILE  *l_fp;

     l_mplayer_startscript = gimp_temp_name(".mplayer_start.sh");
     l_mplayer_pidfile = gimp_temp_name(".mplayer_pidfile.txt");

     /* asynchron start */
     g_remove(l_mplayer_pidfile);

     /* generate a shellscript */
     l_fp = g_fopen(l_mplayer_startscript, "w+");
     if (l_fp != NULL)
     {
         fprintf(l_fp, "#!/bin/sh\n");
         fprintf(l_fp, "CURR_DIR=`pwd`\n");
         if(gpp->vtrack > 0)
         {
           fprintf(l_fp, "cd \"%s\"\n", gpp->mplayer_working_dir);
         }
         fprintf(l_fp, "%s &\n"
                       , l_cmd                 /* start mplayer as background process */
                 );
         fprintf(l_fp, "MPLAYER_PID=$!\n");
         fprintf(l_fp, "cd \"$CURR_DIR\"\n");
         fprintf(l_fp, "echo \"$MPLAYER_PID # MPLAYER_PID\"\n");
         fprintf(l_fp, "echo \"$MPLAYER_PID # MPLAYER_PID\" > \"%s\"\n", l_mplayer_pidfile);

         /* we pass the mplayer pid in a file,
          * exitcodes are truncated to 8 bit
          * by the system call
          */
         /* fprintf(l_fp, "exit $MPLAYER_PID\n"); */
         fclose(l_fp);

         /* set execute permissions for the generated shellscript */
         gap_file_chmod(l_mplayer_startscript, GAP_FILE_MKDIR_MODE);
     }

     /* START the generated shellscrit */
     l_rc = system(l_mplayer_startscript);

     l_fp = g_fopen(l_mplayer_pidfile, "r");
     if (l_fp != NULL)
     {
        fscanf(l_fp, "%d", &l_rc);
        fclose(l_fp);
        l_mplayer_pid = (pid_t)l_rc;
     }

     g_remove(l_mplayer_startscript);
     g_remove(l_mplayer_pidfile);
     g_free(l_mplayer_startscript);
     g_free(l_mplayer_pidfile);

     if(gap_debug)
     {
       printf("ASYNCHRON CALL: %s\nl_mplayer_pid:%d\n", l_cmd, (int)l_mplayer_pid);
     }
   }
   else
   {
     /* synchron start (blocks until mplayer process has finished */
     l_rc = system(l_cmd);
     if ((l_rc & 0xff) == 0) l_mplayer_pid = 0;    /* OK */
     else                    l_mplayer_pid = -1;   /* Synchron call failed */

     if(gap_debug)
     {
       printf("SYNCHRON CALL: %s\nretcode:%d (%d)\n", l_cmd, (int)l_rc, (int)l_mplayer_pid);
     }
   }

   return(l_mplayer_pid);

}       /* end p_start_mplayer_process */


/* ============================================================================
 * gap_mplayer_decode
 * ============================================================================
 */

gint32
gap_mplayer_decode(GapMPlayerParams *gpp)
{
  gint32 l_rc;
  char   extension[20];
  char   extension2[20];
  char   l_cmd[300];
  char   l_first_to_laod[200];
  char  *l_dst_dir;
  int    l_input_dir_created_by_myself;
  pid_t  l_mplayer_pid;

  l_rc = 0;
  l_input_dir_created_by_myself = FALSE;
  global_errlist = NULL;


  /* init mplayer_prog name
   * (including check if mplayer is installed)
   */
  l_rc = p_init_and_check_mplayer(gpp);
  if(l_rc != 0)
  {
    if(global_errlist)
    {
      p_mplayer_info(global_errlist);
    }
    else
    {
      p_mplayer_info("ERROR: could not execute mplayer");
    }

    return(l_rc);
  }


  l_rc = p_mplayer_dialog (gpp);


  if(l_rc != 0)
  {
    return(l_rc);
  }

  if((gpp->vtrack <= 0)
  && (gpp->atrack <= 0))
  {
    g_message(_("Exit, neither video nor audio track was selected"));
    return(0);
  }


  p_init_mplayer_working_dir(gpp);

  if(!g_file_test(gpp->video_filename, G_FILE_TEST_EXISTS))
  {
     global_errlist = g_strdup_printf(
            _("videofile %s not existent\n"),
            gpp->video_filename);
            l_rc = 10;
  }


  if (l_rc == 0)
  {
    switch(gpp->img_format)
    {
      case MPENC_PNG:
         strcpy(extension,  "png");
         strcpy(extension2, ".png");
         break;
      case MPENC_JPEG:
         strcpy(extension,  "jpg");
         strcpy(extension2, ".jpg");
         break;
      default:
         strcpy(extension,  "png");
         strcpy(extension2, ".xcf");
         break;

     }

     if (gpp->vtrack > 0)
     {
         /* for the frames we create a new directory */
         if (g_file_test(gpp->mplayer_working_dir, G_FILE_TEST_IS_DIR))
         {
           /* the input directory already exists,
            * remove frames.
            */
           g_snprintf(l_cmd, sizeof(l_cmd), "rm -f %s%s*.%s", gpp->mplayer_working_dir, G_DIR_SEPARATOR_S, extension);
           system(l_cmd);
         }
         else
         {
            /* create input directory (needed by mplayer to store the frames) */
            gap_file_mkdir(gpp->mplayer_working_dir, GAP_FILE_MKDIR_MODE);

            if (g_file_test(gpp->mplayer_working_dir, G_FILE_TEST_IS_DIR))
            {
              l_input_dir_created_by_myself = TRUE;
            }
            else
            {
               global_errlist = g_strdup_printf(
                      _("could not create %s directory\n"
                       "(that is required for mplayer frame export)"),
                       gpp->mplayer_working_dir);
               l_rc = 10;
            }
         }
     }
  }

  if(l_rc == 0)
  {
     if (gpp->vtrack > 0)
     {
       gimp_progress_init (_("Extracting frames..."));
     }
     else
     {
       gimp_progress_init (_("Extracting audio..."));
     }
     gimp_progress_update (0.02);  /* fake some progress */
     /* note:
      *  we can't show realistic progress for the extracting process
      *  because we know nothing about videofileformat and how much frames
      *  are realy stored in the videofile.
      *
      *  note: the mplayer process was already called synchron
      *  now rename and/or convert the extracted frames
      */

     l_mplayer_pid = p_start_mplayer_process(gpp);

     /* negative pid is used as error indicator */
     if (l_mplayer_pid < 0)
     {
        global_errlist = g_strdup_printf(
           _("could not start mplayer process\n(program=%s)")
           , gpp->mplayer_prog
           );
        l_rc = -1;
     }
  }

  if((l_rc == 0)
  && (gpp->vtrack > 0))
  {
     gint32  l_max_frame;


     if(gpp->run_mplayer_asynchron)
     {
       /* if mplayer was started as asynchron process
        * we use a polling procedure to show progress
        * the polling is done until the asynchron process is finished
        */
       p_poll(gpp, l_mplayer_pid, extension);
     }

     gimp_progress_update (1.0);

     l_max_frame = p_find_max_mplayer_frame (gpp, 1, extension);
     if (l_max_frame < 1)
     {
        global_errlist = g_strdup_printf(
               _("can't find any extracted frames,\n"
                 "mplayer has failed or was cancelled"));
        l_rc = -1;
     }
     else
     {
       /* if destination directorypart does not exist, try to create it */
       l_dst_dir = g_strdup(gpp->basename);
       p_dirname(l_dst_dir);
       if (*l_dst_dir != '\0')
       {
         if (! g_file_test(l_dst_dir, G_FILE_TEST_IS_DIR))
         {
            gap_file_mkdir (l_dst_dir, GAP_FILE_MKDIR_MODE);
         }
       }

       if(gap_debug)
       {
         printf("## img_format: %d ", gpp->img_format);
         printf("extension:%s:", extension);
         printf("extension2:%s:\n", extension2);
       }

       if(strcmp(extension, &extension2[1]) == 0)
       {
          gimp_progress_init (_("Renaming frames..."));
          l_rc = p_rename_frames(gpp
                                ,1                     /* first */
                                ,gpp->number_of_frames /* last_frame */
                                ,gpp->basename
                                ,extension
                                );
       }
       else
       {
          gimp_progress_init (_("Converting frames..."));
          l_rc = p_convert_frames(gpp
                                 ,1                      /* first_frame */
                                 ,gpp->number_of_frames  /* last_frame */
                                 ,gpp->basename
                                 ,extension
                                 ,extension2
                                 );
       }

       if (l_input_dir_created_by_myself)
       {
         if (strcmp(l_dst_dir, gpp->mplayer_working_dir) != 0)
         {
            /* remove input dir with all files */
            g_snprintf(l_cmd, sizeof(l_cmd), "rm -rf \"%s\"", gpp->mplayer_working_dir);
            system(l_cmd);
         }
       }
       g_free(l_dst_dir);
       gimp_progress_update (1.0);
     }
   }



   if(l_rc != 0)
   {
      if(global_errlist == NULL)
      {
         p_mplayer_info("ERROR: could not execute mplayer");
      }
      else
      {
         p_mplayer_info(global_errlist);
      }
      l_rc = -1;
   }
   else
   {
     if((gpp->autoload)
     && (gpp->vtrack > 0))
     {
        /* load first frame and add a display */
        p_build_gap_framename(l_first_to_laod
                             ,sizeof(l_first_to_laod)
                             ,1 /* mplayer starts extracted frames with nr 00000001 */
                             ,gpp->basename
                             ,&extension2[1]
                             );
        l_rc = gap_lib_load_image(l_first_to_laod);

        if(l_rc >= 0) gimp_display_new(l_rc);
     }
   }

   return(l_rc);
}       /* end  gap_mplayer_decode */
