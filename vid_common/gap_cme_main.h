/* gap_cme_main.h
 *    global_params structure for GIMP/GAP Common Video Encoder
 */
/*
 * Changelog:
 * version 2.1.0a;  2004.05.06   hof: integration into gimp-gap project
 * 2002/10/20 v1.2.2b:  added filtermacro_file, storyboard_file
 * 2001/11/03 v1.2.1b:  added short_description GAP_VENC_PAR_SHORT_DESCRIPTION
 * 2001/04/08 v1.2.1a:  created
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
 * <http://www.gnu.org/licenses/>.
 */

#ifndef GAP_CME_MAIN_H
#define GAP_CME_MAIN_H

#include <config.h>
#include "gap_gvetypes.h"

#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define GAP_CME_PLUGIN_NAME_VID_ENCODE_MASTER    "plug-in-gap-vid-encode-master"
#define GAP_CME_PLUGIN_HELP_ID_VID_ENCODE_MASTER "plug-in-gap-vid-encode-master"


#define GAP_CME_STANDARD_SIZE_KEEP -1
#define GAP_CME_STANDARD_SIZE_IMAGE   0
#define GAP_CME_STANDARD_SIZE_320x240 1
#define GAP_CME_STANDARD_SIZE_320x288 2
#define GAP_CME_STANDARD_SIZE_640x480 3
#define GAP_CME_STANDARD_SIZE_720x480 4
#define GAP_CME_STANDARD_SIZE_720x576 5
#define GAP_CME_STANDARD_SIZE_1280x720 6
#define GAP_CME_STANDARD_SIZE_1920x1080 7
#define GAP_CME_STANDARD_SIZE_1920x1088 8
#define GAP_CME_STANDARD_SIZE_MAX_ELEMENTS  9

#define GAP_CME_STANDARD_FRAMERATE_KEEP -1
#define GAP_CME_STANDARD_FRAMERATE_00_UNCHANGED  0
#define GAP_CME_STANDARD_FRAMERATE_01_23_98 1
#define GAP_CME_STANDARD_FRAMERATE_02_24    2
#define GAP_CME_STANDARD_FRAMERATE_03_25    3
#define GAP_CME_STANDARD_FRAMERATE_04_29_97 4
#define GAP_CME_STANDARD_FRAMERATE_05_30    5
#define GAP_CME_STANDARD_FRAMERATE_06_50    6
#define GAP_CME_STANDARD_FRAMERATE_07_59_94 7
#define GAP_CME_STANDARD_FRAMERATE_08_60    8
#define GAP_CME_STANDARD_FRAMERATE_09_1     9
#define GAP_CME_STANDARD_FRAMERATE_10_5     10
#define GAP_CME_STANDARD_FRAMERATE_11_10    11
#define GAP_CME_STANDARD_FRAMERATE_12_12    12
#define GAP_CME_STANDARD_FRAMERATE_13_15    13
#define GAP_CME_STANDARD_FRAMERATE_MAX_ELEMENTS  14

#define GAP_CME_STANDARD_VIDNORM_00_NTSC    0
#define GAP_CME_STANDARD_VIDNORM_01_PAL     1
#define GAP_CME_STANDARD_VIDNORM_02_SECAM   2
#define GAP_CME_STANDARD_VIDNORM_03_MAC     3
#define GAP_CME_STANDARD_VIDNORM_04_COMP    4
#define GAP_CME_STANDARD_VIDNORM_05_UNDEF   5
#define GAP_CME_STANDARD_VIDNORM_MAX_ELEMENTS  6

#define GAP_CME_STANDARD_SAMPLERATE_00_8000    0
#define GAP_CME_STANDARD_SAMPLERATE_01_11025   1
#define GAP_CME_STANDARD_SAMPLERATE_02_12000   2
#define GAP_CME_STANDARD_SAMPLERATE_03_16000   3
#define GAP_CME_STANDARD_SAMPLERATE_04_22050   4
#define GAP_CME_STANDARD_SAMPLERATE_05_24000   5
#define GAP_CME_STANDARD_SAMPLERATE_05_32000   6
#define GAP_CME_STANDARD_SAMPLERATE_05_44100   7
#define GAP_CME_STANDARD_SAMPLERATE_05_48000   8
#define GAP_CME_STANDARD_SAMPLERATE_MAX_ELEMENTS  9


typedef enum
{
   GAP_CME_ENC_RUN_STATE_READY       = 0
 , GAP_CME_ENC_RUN_STATE_RUNNING     = 1
 , GAP_CME_ENC_RUN_STATE_FINISHED    = 2 
} GapCmeEncoderRunState;

typedef struct GapCmeGlobalParams {                    /* nick: gpp */
  GapGveCommonValues   val;
  GapGveEncAInfo       ainfo;
  GapGveEncList        *ecp;
  
  gboolean   storyboard_create_composite_audio;

  GtkWidget *shell_window;
  GtkWidget *fsv__fileselection;
  GtkWidget *fsb__fileselection;
  GtkWidget *fsa__fileselection;
  GtkWidget *fss__fileselection;
  GtkWidget *ow__dialog_window;


  GtkWidget *ow__filename;
  GtkWidget *cme__button_gen_tmp_audfile;
  GtkWidget *cme__button_params;
  GtkWidget *cme__button_stb_audio;
  GtkWidget *cme__entry_audio1;
  GtkWidget *cme__entry_debug_flat;
  GtkWidget *cme__entry_debug_multi;
  GtkWidget *cme__entry_mac;
  GtkWidget *cme__entry_sox;
  GtkWidget *cme__entry_sox_options;
  GtkWidget *cme__entry_stb;
  GtkWidget *cme__entry_video;
  GtkWidget *cme__button_video_filesel;
  GtkWidget *cme__label_aud0_time;
  GtkWidget *cme__label_aud1_info;
  GtkWidget *cme__label_aud1_time;
  GtkWidget *cme__label_aud_tmp_info;
  GtkWidget *cme__label_from;
  GtkWidget *cme__label_fromtime;
  GtkWidget *cme__label_status;
  GtkWidget *cme__label_storyboard_helptext;
  GtkWidget *cme__label_to;
  GtkWidget *cme__label_totaltime;
  GtkWidget *cme__label_totime;
  GtkWidget *cme__combo_encodername;
  GtkWidget *cme__combo_framerate;
  GtkWidget *cme__combo_outsamplerate;
  GtkWidget *cme__combo_scale;
  GtkWidget *cme__progressbar_status;
  GtkWidget *cme__short_description;
  GtkWidget *cme__spinbutton_framerate;
  GtkWidget *cme__spinbutton_from;
  GtkWidget *cme__spinbutton_height;
  GtkWidget *cme__spinbutton_samplerate;
  GtkWidget *cme__spinbutton_to;
  GtkWidget *cme__spinbutton_width;
  GtkWidget *cme__label_aud_tmp_time;
  GtkWidget *cme__label_tmp_audfile;
  GtkWidget *cme__radio_button_frame;
  GtkWidget *cme__radio_button_layer;
  GtkWidget *cme__radio_button_storyboard;

  GtkObject *cme__spinbutton_width_adj;
  GtkObject *cme__spinbutton_height_adj;
  GtkObject *cme__spinbutton_from_adj;
  GtkObject *cme__spinbutton_to_adj;
  GtkObject *cme__spinbutton_framerate_adj;
  GtkObject *cme__spinbutton_samplerate_adj;


  GtkWidget *cme__notebook;
  GtkWidget *cme__encoder_status_frame;
  GtkWidget *cme__vbox_main;
  GtkWidget *cme__label_enc_stat_frames_total;
  GtkWidget *cme__label_enc_stat_frames_done;
  GtkWidget *cme__label_enc_stat_frames_encoded;
  GtkWidget *cme__label_enc_stat_frames_copied_lossless;
  GtkWidget *cme__label_active_encoder_name;
  GtkWidget *cme__label_enc_time_elapsed;

  GapCmeEncoderRunState  video_encoder_run_state;
  gint32 productive_encoder_timertag;
  gint32 encoder_status_poll_timertag;
  GThread    *productive_encoder_thread; 
  GapGveMasterEncoderStatus encStatus;
  time_t     encoder_started_on_utc_seconds;
  
} GapCmeGlobalParams;

GapCmeGlobalParams * gap_cme_main_get_global_params(void);

#endif
