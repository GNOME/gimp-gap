/* gap_enc_avi_main.h
 *    global_params structure for GIMP/GAP AVU Video Encoder
 */
/*
 * Changelog:
 * version 2.1.0a;  2004.06.12   created
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

#ifndef GAP_ENC_AVI_MAIN_H
#define GAP_ENC_AVI_MAIN_H

#include <config.h>
#include "gap_gvetypes.h"



#define GAP_PLUGIN_NAME_AVI_PARAMS     "plug-in-gap-encpar-avi1"
#define GAP_HELP_ID_AVI_PARAMS         "plug-in-gap-encpar-avi1"
#define GAP_MENUNAME                   "AVI1"


#define GAP_AVI_VIDCODEC_00_JPEG   0
#define GAP_AVI_VIDCODEC_01_MJPG   1
#define GAP_AVI_VIDCODEC_02_PNG    2
#define GAP_AVI_VIDCODEC_03_RAW    3
#define GAP_AVI_VIDCODEC_04_XVID   4
#define GAP_AVI_VIDCODEC_MAX_ELEMENTS   5

/* names of the supported AVI Codecs */
#define GAP_AVI_CODEC_RAW  "RAW "   /* refers to 4 byte code "RGB " */
#define GAP_AVI_CODEC_RGB  "RGB "
#define GAP_AVI_CODEC_JPEG "JPEG"
#define GAP_AVI_CODEC_MJPG "MJPG"
#define GAP_AVI_CODEC_PNG  "PNG "
/* ??? not sure what to use for the correct 4cc codec names for xvid divx MPEG 4 */
#define GAP_AVI_CODEC_XVID "XVID"
#define GAP_AVI_CODEC_DIVX "div5"


/* avi specific encoder params  */
typedef struct {
  char   codec_name[50];
  gint32 APP0_marker;

  /* for the builtin "JPEG" CODEC */
  gint32 jpeg_dont_recode_frames;
  gint32 jpeg_interlaced;
  gint32 jpeg_quality;         /* 0..100% */
  gint32 jpeg_odd_even;

  /* for the "xvid" (Open DivX) CODEC */
  GapGveXvidValues xvid;

  /* for the "RGB " (== raw) CODEC */
  gint32 raw_vflip;
  gint32 raw_bgr;    /* TRUE: BGR (default) FALSE: RGB */

  /* for the "PNG " CODEC */
  gint32 png_dont_recode_frames;
  gint32 png_interlaced;
  gint32 png_compression;         /* 0..9 */
} GapGveAviValues;

typedef struct GapGveAviGlobalParams {   /* nick: gpp */
  GapGveCommonValues   val;
  GapGveEncAInfo       ainfo;
  GapGveEncList        *ecp;

  GapGveAviValues   evl;

  GtkWidget *shell_window;
  GtkWidget *notebook_main;

  GtkWidget *combo_codec;
  GtkWidget *app0_checkbutton;
  GtkWidget *jpg_dont_recode_checkbutton;
  GtkWidget *jpg_interlace_checkbutton;
  GtkWidget *jpg_odd_first_checkbutton;
  GtkWidget *jpg_quality_spinbutton;
  GtkObject *jpg_quality_spinbutton_adj;

  GtkWidget *xvid_rc_kbitrate_spinbutton;
  GtkObject *xvid_rc_kbitrate_spinbutton_adj;
  gint32     xvid_kbitrate;
  GtkWidget *xvid_rc_reaction_delay_spinbutton;
  GtkObject *xvid_rc_reaction_delay_spinbutton_adj;
  GtkWidget *xvid_rc_avg_period_spinbutton;
  GtkObject *xvid_rc_avg_period_spinbutton_adj;
  GtkWidget *xvid_rc_buffer_spinbutton;
  GtkObject *xvid_rc_buffer_spinbutton_adj;
  GtkWidget *xvid_max_quantizer_spinbutton;
  GtkObject *xvid_max_quantizer_spinbutton_adj;
  GtkWidget *xvid_min_quantizer_spinbutton;
  GtkObject *xvid_min_quantizer_spinbutton_adj;
  GtkWidget *xvid_max_key_interval_spinbutton;
  GtkObject *xvid_max_key_interval_spinbutton_adj;
  GtkWidget *xvid_quality_spinbutton;
  GtkObject *xvid_quality_spinbutton_adj;
  
  GtkWidget *raw_vflip_checkbutton;
  GtkWidget *raw_bgr_checkbutton;

  GtkWidget *png_dont_recode_checkbutton;
  GtkWidget *png_interlace_checkbutton;
  GtkWidget *png_compression_spinbutton;
  GtkObject *png_compression_spinbutton_adj;

} GapGveAviGlobalParams;


void gap_enc_avi_main_init_default_params(GapGveAviValues *epp);

#endif
