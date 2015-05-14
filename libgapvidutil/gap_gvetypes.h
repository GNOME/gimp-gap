/* gap_gvetypes.h
 *    Common Types for GIMP/GAP Video Encoders
 */

/*
 * Changelog:
 * 2004/05/05 v2.1.0a:  created
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef GAP_GVETYPES_H
#define GAP_GVETYPES_H

#include <config.h>

#define GAP_WILDCARD_VIDEO_ENCODERS     "^plug.in.gap.enc.*"
#define GAP_QUERY_PREFIX_VIDEO_ENCODERS "extension-gap-ENCPAR-QUERY-"
#define GAP_VENC_PAR_MENU_NAME          "gapve-menu-name"
#define GAP_VENC_PAR_VID_EXTENSION      "gapve-vid-extension"
#define GAP_VENC_PAR_SHORT_DESCRIPTION  "gapve-vid-short-description"
#define GAP_VENC_PAR_GUI_PROC           "gapve-gui-proc"

#define GAP_PLUGIN_NAME_SINGLEFRAMES_ENCODE  "plug-in-gap-enc-singleframes"
#define GAP_PLUGIN_NAME_FFMPEG_ENCODE        "plug-in-gap-enc-ffmpeg"
#define GAP_PLUGIN_NAME_AVI_ENCODE           "plug-in-gap-enc-avi1"
#define GAP_PLUGIN_NAME_QT1_ENCODE           "plug-in-gap-enc-qt1"
#define GAP_PLUGIN_NAME_QT2_ENCODE           "plug-in-gap-enc-qt2"

/* the old encoders MPG1 and MPG2 are not ported
 * to GTK+2.2 (maybe i'll remove them completely)
 */
#define GAP_PLUGIN_NAME_MPG1_ENCODE          "plug-in-gap-enc-mpg1"
#define GAP_PLUGIN_NAME_MPG2_ENCODE          "plug-in-gap-enc-mpg2"



#define GAP_VENC_NUM_STANDARD_PARAM 17

/* SYTEM (UNIX) includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gap-intl.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <gap_gve_misc_util.h>
#include <gap_lib_common_defs.h>




#define VID_FMT_COMP   0
#define VID_FMT_PAL    1
#define VID_FMT_NTSC   2
#define VID_FMT_SECAM  3
#define VID_FMT_MAC    4
#define VID_FMT_UNDEF  5

#define GAP_GVE_MENU_ITEM_INDEX_KEY "gap_enc_menu_item_index"


typedef struct GapGveEncList {
  gchar       vid_enc_plugin[80];
  gchar       gui_proc[80];
  char        menu_name[50];
  char        video_extension[50];
  char        short_description[500];
  gint        menu_nr;
  void       *next;
} GapGveEncList;



typedef struct GapGveCommonValues {                     /* nick: cval */
 /* common encoder plugin params */
  gchar   videoname[256];
  gchar   audioname1[256];
  gchar   *audioname_ptr;

  gchar   basename[512];
  gint32  basenum;
  gchar   extension[50];
  GapGveEncList ecp_sel;

  GapLibTypeInputRange input_mode;
  gint32  range_from;
  gint32  range_to;
  gdouble framerate;
  gint32  samplerate;
  gint32  wav_samplerate1;
  gint32  wav_samplerate_tmp;
  gint32  vid_width;
  gint32  vid_height;
  gint    vid_format;     /* 2: video_format: 0=comp., 1=PAL, 2=NTSC, 3=SECAM, 4=MAC, 5=unspec. */

  /* current states of actual loaded (frame)image */
  gint32  image_ID;        /* -1 if there is no valid current image */
  gint32  layer_ID;        /* -1 if there is no valid current image */
  gint32  display_ID;      /* -1 if there is no valid display */

  GimpRunMode run_mode;
  gint        ow_mode;
  gint        run;
  GThread    *gui_proc_thread;   /* thread id of gui_procedure null if not running */
  gchar       tmp_audfile[256];  /* Workfile (output after conversion by sox) */
  gchar       util_sox[256];
  gchar       util_sox_options[256];

 /* optional extras */
  gchar   filtermacro_file[256];
  gchar   storyboard_file[256];
  gint32  storyboard_total_frames;

  gint32  master_encoder_id;  
} GapGveCommonValues;



extern int gap_debug;

#endif
