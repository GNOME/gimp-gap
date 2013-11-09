/*
 * gap_vex_main.h
 * MAIN Header stucture of Video extract specific global parameters
 */

/*
 * Changelog:
 * 2004/04/11 v2.1.0:   integrated sourcecode into gimp-gap project
 * 2003/04/14 v1.2.1a:  created
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

#ifndef GAP_VEX_MAIN
#define GAP_VEX_MAIN

#include <config.h>


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "gap-intl.h"

#include <gtk/gtk.h>
#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

#include "gap_libgimpgap.h"
#include "gap_player_dialog.h"

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* Includes for GAP Video API */
#include <gap_vid_api.h>
#endif


#define GAP_VEX_PLUG_IN_NAME        "plug_in_gap_extract_video"
#define GAP_VEX_PLUG_IN_NAME_XTNS   "plug_in_gap_extract_video_toolbox"
#define GAP_VEX_PLUG_IN_HELP_ID     "plug-in-gap-extract-video"


#define GAP_VEX_DELACE_NONE     0
#define GAP_VEX_DELACE_ODD      1
#define GAP_VEX_DELACE_EVEN     2
#define GAP_VEX_DELACE_ODD_X2   3
#define GAP_VEX_DELACE_EVEN_X2  4


#define GAP_VEX_DECODER_NONE         0
#define GAP_VEX_DECODER_LIBMPEG3     1
#define GAP_VEX_DECODER_QUICKTIME    2
#define GAP_VEX_DECODER_LIBAVFORMAT  3

typedef struct {
  gchar   videoname[512];
  gdouble begin_percent;
  gdouble end_percent;
  gdouble begin_frame;
  gdouble end_frame;
  gint    pos_unit;       /* 0 .. frames, 2 percent */
  gint    multilayer;     /* 0 .. extract to single frames, 1 ..extract to multilayer image, 2.. create Stroyboard, 3 create + edit Storyboard */
  gint    disable_mmx;    /* 0 .. use MMX if available, 1 disable MMX  */
  gint    exact_seek;     /* 0 .. NO, 1 .. YES */
  gint    deinterlace;    /* 0 .. NO, 1 .. odd rows only, 2 even rows only, 3 .. odd first, 4 .. even first */
  gdouble delace_threshold;      /* 0.0 .. no interpolation, 1.0 smooth interpolation at deinterlacing */

  gchar   basename[512];
  gint32  basenum;
  gint32  fn_digits;      /* number of digits in framenames 1 upto 6, 1 == no leading zeroes */
  gchar   extension[50];
  gchar   audiofile[512];
  gint    videotrack;     /* starting at 1 */
  gint    audiotrack;     /* starting at 1 */

  gchar preferred_decoder[100];

  /* last checked videoname */
  gboolean chk_is_compatible_videofile;
  gint32  chk_total_frames;
  gint32  chk_vtracks;
  gint32  chk_atracks;


  gint    run;            /* TRUE on OK button, FALSE on Cancel button */
  gint    ow_mode;        /* */

  /* current states of actual loaded (frame)image */
  gint32  image_ID;        /* -1 if there is no valid current image */
  GimpRunMode run_mode;

  gboolean generate_alpha_via_bluebox;
  gboolean extract_alpha_as_gray_frames;
  gboolean extract_with_layermask;
} GapVexMainVal;


typedef struct {  /* nick: gpp */
  GapVexMainVal   val;

  GapPlayerMainGlobalParams *plp;      /* player widget parameters */
  gboolean   in_player_call;
  gint32     video_width;
  gint32     video_height;
  gdouble    video_speed;    /* original playback speed in frames per sec */

  GtkWidget *mw__main_window;
  GtkWidget *fsv__fileselection;
  GtkWidget *fsb__fileselection;
  GtkWidget *fsa__fileselection;

  GtkWidget *mw__player_frame;
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
  GtkWidget *mw__entry_basename;
  GtkWidget *mw__entry_extension;
  GtkWidget *mw__button_basename;
  GtkObject *mw__spinbutton_basenum_adj;
  GtkWidget *mw__spinbutton_basenum;
  GtkWidget *mw__entry_audiofile;
  GtkWidget *mw__button_audiofile;
  GtkWidget *mw__combo_mode_multilayer;
  GtkWidget *mw__combo_deinterlace;
  GtkObject *mw__spinbutton_delace_threshold_adj;
  GtkWidget *mw__spinbutton_delace_threshold;
  GtkObject *mw__spinbutton_fn_digits_adj;
  GtkWidget *mw__spinbutton_fn_digits;
  GtkWidget *mw__button_OK;
  GtkWidget *mw__begin_time_label;
  GtkWidget *mw__end_time_label;

  GtkWidget *mw__checkbutton_generate_alpha_via_bluebox;
  GtkWidget *mw__checkbutton_extract_alpha_as_gray_frames;
  GtkWidget *mw__checkbutton_extract_with_layermask;

} GapVexMainGlobalParams;

extern int gap_debug;

/* -------------------
 * p_check_aspect
 * -------------------
 */
static gboolean
p_check_aspect(gdouble aspect_ratio, gint width, gint height)
{
  gdouble w_div_h;

  if(height)
  {
    w_div_h = (gdouble)width / (gdouble)height;

    if ((aspect_ratio <=  w_div_h + 0.001)
    &&  (aspect_ratio >=  w_div_h - 0.001))
    {
      return(TRUE);
    }
  }

  return (FALSE);
}  /* end p_check_aspect */

#endif
