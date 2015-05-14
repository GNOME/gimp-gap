/* gap_enc_ffmpeg_main.h
 *    global_params structure for GIMP/GAP FFMPEG Video Encoder
 */
/*
 * Changelog:
 * version 2.1.0a;  2005/03/27  hof: support for ffmpeg 0.4.9 and later CVS versions (LIBAVCODEC_BUILD  >= 4744)
 * version 2.1.0a;  2004/11/05  hof: replaced deprecated option menu by gimp_int_combo_box
 * version 2.1.0a;  2004.06.05   hof: update params from ffmpeg 0.4.6 to 0.4.8
 * version 2.1.0a;  2004.05.07   created
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

#ifndef GAP_ENC_FFMPEG_MAIN_H
#define GAP_ENC_FFMPEG_MAIN_H

#include <config.h>
#include "gap_gvetypes.h"


/* Includes for encoder specific extra LIBS */
#include "avformat.h"
#include "avcodec.h"

/// start ffmpeg 0.5 / 0.6 / 0.7 support
#if LIBAVCODEC_VERSION_MAJOR < 52
#define GAP_USES_OLD_FFMPEG_0_5
#endif

#if LIBAVCODEC_VERSION_MAJOR == 52
#if LIBAVCODEC_VERSION_MINOR <= 20
#define GAP_USES_OLD_FFMPEG_0_5
#endif
#if LIBAVCODEC_VERSION_MINOR <= 72
#define GAP_USES_OLD_FFMPEG_0_6
#endif
#endif



#ifdef GAP_USES_OLD_FFMPEG_0_5
/* defines to use older ffmpeg-0.5 compatible types */
#define AVMEDIA_TYPE_UNKNOWN  CODEC_TYPE_UNKNOWN
#define AVMEDIA_TYPE_VIDEO    CODEC_TYPE_VIDEO
#define AVMEDIA_TYPE_AUDIO    CODEC_TYPE_AUDIO
#define AV_PKT_FLAG_KEY       PKT_FLAG_KEY
#endif

/// end ffmpeg 0.5 / 0.6 support


#define GAP_HELP_ID_FFMPEG_PARAMS         "plug-in-gap-encpar-ffmpeg"
#define GAP_PLUGIN_NAME_FFMPEG_PARAMS     "plug-in-gap-encpar-ffmpeg"
#define GAP_PLUGIN_NAME_FFMPEG_ENCODE     "plug-in-gap-enc-ffmpeg"

#define GAP_FFMPEG_CURRENT_VID_EXTENSION  "plug-in-gap-enc-ffmpeg-CURRENT-VIDEO-EXTENSION"

#define GAP_GVE_FFMPEG_PRESET_00_NONE           0
// #define GAP_GVE_FFMPEG_PRESET_01_DIVX_DEFAULT   1
// #define GAP_GVE_FFMPEG_PRESET_02_DIVX_BEST      2
// #define GAP_GVE_FFMPEG_PRESET_03_DIVX_LOW       3
// #define GAP_GVE_FFMPEG_PRESET_04_DIVX_MS        4
// #define GAP_GVE_FFMPEG_PRESET_05_MPEG1_VCD      5
// #define GAP_GVE_FFMPEG_PRESET_06_MPEG1_BEST     6
// #define GAP_GVE_FFMPEG_PRESET_07_MPEG2_SVCD     7
// #define GAP_GVE_FFMPEG_PRESET_08_MPEG2_DVD      8
// #define GAP_GVE_FFMPEG_PRESET_09_REAL           9
// #define GAP_GVE_FFMPEG_PRESET_MAX_ELEMENTS      10

/* NOTE:
 * since 2011.10.29 gimp-gap is shiped with preset files that are installed
 * automatically and checked for valid codecs at runtime.
 * therefore all the hardcoded presets are disabled.
 */
#define GAP_GVE_FFMPEG_PRESET_MAX_ELEMENTS      1

#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_00_32   0
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_01_40   1
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_02_48   2
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_03_56   3
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_04_64   4
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_05_80   5
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_06_96   6
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_07_112  7
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_08_128  8
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_09_160  9
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_10_192  10
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_11_224  11
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_12_256  12
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_13_320  13
#define GAP_GVE_FFMPEG_AUDIO_KBIT_RATE_MAX_ELEMENTS   14

#define GAP_GVE_FFMPEG_MOTION_ESTIMATION_01_ZERO   ME_ZERO
#define GAP_GVE_FFMPEG_MOTION_ESTIMATION_02_FULL   ME_FULL
#define GAP_GVE_FFMPEG_MOTION_ESTIMATION_03_LOG    ME_LOG
#define GAP_GVE_FFMPEG_MOTION_ESTIMATION_04_PHODS  ME_PHODS
#define GAP_GVE_FFMPEG_MOTION_ESTIMATION_05_EPZS   ME_EPZS
#define GAP_GVE_FFMPEG_MOTION_ESTIMATION_06_X1     ME_X1
/* motion estimation 
 *   7,8,10 are x264 specific
 *   9 is snow specific
 */
#define GAP_GVE_FFMPEG_MOTION_ESTIMATION_07_HEX   ME_HEX
#define GAP_GVE_FFMPEG_MOTION_ESTIMATION_08_UMH   ME_UMH
#define GAP_GVE_FFMPEG_MOTION_ESTIMATION_09_ITER  ME_ITER
#define GAP_GVE_FFMPEG_MOTION_ESTIMATION_10_TESA  ME_TESA
#define GAP_GVE_FFMPEG_MOTION_ESTIMATION_MAX_ELEMENTS   10



#define GAP_GVE_FFMPEG_DCT_ALGO_00_AUTO        0
#define GAP_GVE_FFMPEG_DCT_ALGO_01_FASTINT     1
#define GAP_GVE_FFMPEG_DCT_ALGO_02_INT         2
#define GAP_GVE_FFMPEG_DCT_ALGO_03_MMX         3
#define GAP_GVE_FFMPEG_DCT_ALGO_04_MLIB        4
#define GAP_GVE_FFMPEG_DCT_ALGO_05_ALTIVEC     5
#define GAP_GVE_FFMPEG_DCT_ALGO_06_FAAN        6
#define GAP_GVE_FFMPEG_DCT_ALGO_MAX_ELEMENTS   7


#define GAP_GVE_FFMPEG_IDCT_ALGO_00_AUTO            FF_IDCT_AUTO
#define GAP_GVE_FFMPEG_IDCT_ALGO_01_INT             FF_IDCT_INT
#define GAP_GVE_FFMPEG_IDCT_ALGO_02_SIMPLE          FF_IDCT_SIMPLE
#define GAP_GVE_FFMPEG_IDCT_ALGO_03_SIMPLEMMX       FF_IDCT_SIMPLEMMX
#define GAP_GVE_FFMPEG_IDCT_ALGO_04_LIBMPEG2MMX     FF_IDCT_LIBMPEG2MMX
#define GAP_GVE_FFMPEG_IDCT_ALGO_05_PS2             FF_IDCT_PS2
#define GAP_GVE_FFMPEG_IDCT_ALGO_06_MLIB            FF_IDCT_MLIB
#define GAP_GVE_FFMPEG_IDCT_ALGO_07_ARM             FF_IDCT_ARM
#define GAP_GVE_FFMPEG_IDCT_ALGO_08_ALTIVEC         FF_IDCT_ALTIVEC
#define GAP_GVE_FFMPEG_IDCT_ALGO_09_SH4             FF_IDCT_SH4
#define GAP_GVE_FFMPEG_IDCT_ALGO_10_SIMPLEARM       FF_IDCT_SIMPLEARM
#define GAP_GVE_FFMPEG_IDCT_ALGO_11_H264            FF_IDCT_H264
#define GAP_GVE_FFMPEG_IDCT_ALGO_12_VP3             FF_IDCT_VP3
#define GAP_GVE_FFMPEG_IDCT_ALGO_13_IPP             FF_IDCT_IPP
#define GAP_GVE_FFMPEG_IDCT_ALGO_14_XVIDMMX         FF_IDCT_XVIDMMX
#define GAP_GVE_FFMPEG_IDCT_ALGO_15_CAVS            FF_IDCT_CAVS
#define GAP_GVE_FFMPEG_IDCT_ALGO_16_SIMPLEARMV5TE   FF_IDCT_SIMPLEARMV5TE   
#define GAP_GVE_FFMPEG_IDCT_ALGO_17_SIMPLEARMV6     FF_IDCT_SIMPLEARMV6
#define GAP_GVE_FFMPEG_IDCT_ALGO_18_SIMPLEVIS       FF_IDCT_SIMPLEVIS
#define GAP_GVE_FFMPEG_IDCT_ALGO_19_WMV2            FF_IDCT_WMV2
#define GAP_GVE_FFMPEG_IDCT_ALGO_20_FAAN            FF_IDCT_FAAN
#define GAP_GVE_FFMPEG_IDCT_ALGO_21_EA              FF_IDCT_EA
#define GAP_GVE_FFMPEG_IDCT_ALGO_22_SIMPLENEON      FF_IDCT_SIMPLENEON
#define GAP_GVE_FFMPEG_IDCT_ALGO_23_SIMPLEALPHA     FF_IDCT_SIMPLEALPHA
#define GAP_GVE_FFMPEG_IDCT_ALGO_MAX_ELEMENTS  24



#define GAP_GVE_FFMPEG_MB_DECISION_00_SIMPLE    0
#define GAP_GVE_FFMPEG_MB_DECISION_01_BITS      1
#define GAP_GVE_FFMPEG_MB_DECISION_02_RD        2
#define GAP_GVE_FFMPEG_MB_DECISION_MAX_ELEMENTS    3

#define GAP_GVE_FFMPEG_ASPECT_00_AUTO    0
#define GAP_GVE_FFMPEG_ASPECT_01_3_2     1
#define GAP_GVE_FFMPEG_ASPECT_02_4_3     2
#define GAP_GVE_FFMPEG_ASPECT_03_16_9    3
#define GAP_GVE_FFMPEG_ASPECT_MAX_ELEMENTS    4


#define GAP_GVE_FFMPEG_CMP_00_SAD     FF_CMP_SAD
#define GAP_GVE_FFMPEG_CMP_01_SSE     FF_CMP_SSE
#define GAP_GVE_FFMPEG_CMP_02_SATD    FF_CMP_SATD
#define GAP_GVE_FFMPEG_CMP_03_DCT     FF_CMP_DCT
#define GAP_GVE_FFMPEG_CMP_04_PSNR    FF_CMP_PSNR
#define GAP_GVE_FFMPEG_CMP_05_BIT     FF_CMP_BIT
#define GAP_GVE_FFMPEG_CMP_06_RD      FF_CMP_RD
#define GAP_GVE_FFMPEG_CMP_07_ZERO    FF_CMP_ZERO
#define GAP_GVE_FFMPEG_CMP_08_VSAD    FF_CMP_VSAD
#define GAP_GVE_FFMPEG_CMP_09_VSSE    FF_CMP_VSSE
#define GAP_GVE_FFMPEG_CMP_10_NSSE    FF_CMP_NSSE
#define GAP_GVE_FFMPEG_CMP_11_W53     FF_CMP_W53
#define GAP_GVE_FFMPEG_CMP_12_W97     FF_CMP_W97
#define GAP_GVE_FFMPEG_CMP_13_DCTMAX  FF_CMP_DCTMAX 
#define GAP_GVE_FFMPEG_CMP_14_CHROMA  FF_CMP_CHROMA
#define GAP_GVE_FFMPEG_CMP_MAX_ELEMENTS   15


#define GAP_GVE_FFMPEG_CODER_TYPE_00_VLC       FF_CODER_TYPE_VLC      
#define GAP_GVE_FFMPEG_CODER_TYPE_01_AC        FF_CODER_TYPE_AC
#define GAP_GVE_FFMPEG_CODER_TYPE_MAX_ELEMENTS 2        

#define GAP_GVE_FFMPEG_PREDICTOR_00_LEFT        FF_PRED_LEFT
#define GAP_GVE_FFMPEG_PREDICTOR_01_PLANE       FF_PRED_PLANE
#define GAP_GVE_FFMPEG_PREDICTOR_02_MEDIAN      FF_PRED_MEDIAN
#define GAP_GVE_FFMPEG_PREDICTOR_MAX_ELEMENTS   3


#define GAP_GVE_FF_QP2LAMBDA   FF_QP2LAMBDA
#define GAP_ENCODER_PRESET_NAME_MAX_LENGTH 60
#define GAP_ENCODER_PRESET_FILENAME_MAX_LENGTH 1024

#define GAP_GVE_FFMPEG_SHOW_EXPERT_SETTINGS "video-enoder-ffmpeg-show-expert-settings"



/* GapGveFFMpegValues ffmpeg specific encoder params */
typedef struct {
  gint32  presetId;
  char    presetName[GAP_ENCODER_PRESET_NAME_MAX_LENGTH];
  char    presetFileName[GAP_ENCODER_PRESET_FILENAME_MAX_LENGTH];
  char    current_vid_extension[80];

  /* ffmpeg options */
  char    format_name[20];
  char    vcodec_name[20];
  char    acodec_name[20];
  char    title[40];
  char    author[40];
  char    copyright[40];
  char    comment[80];
  char    passlogfile[512];
  gint32  pass_nr;
  gint32  twoPassFlag;

  gint32  audio_bitrate;
  gint32  video_bitrate;
  gint32  gop_size;
  gint32  intra;
  gdouble qscale;       /* changed from int to float with ffmpeg 0.4.9pre1 */
  gint32  qmin;
  gint32  qmax;
  gint32  qdiff;
  gdouble qblur;
  gdouble qcomp;
  gdouble rc_init_cplx;
  gdouble b_qfactor;
  gdouble i_qfactor;
  gdouble b_qoffset;
  gdouble i_qoffset;
  gint32  bitrate_tol;
  gint32  maxrate_tol;
  gint32  minrate_tol;
  gint32  bufsize;
  gint32  motion_estimation;
  gint32  dct_algo;
  gint32  idct_algo;
  gint32  strict;
  gint32  mb_qmin;
  gint32  mb_qmax;
  gint32  mb_decision;
  gint32  aic;
  gint32  umv;
  gint32  b_frames;
  gint32  mv4;
  gint32  partitioning;
  gint32  packet_size;
 
  /* ?? debug options */
  gint32  benchmark;
  gint32  hex_dump;
  gint32  psnr;
  gint32  vstats;
  gint32  bitexact;
  gint32  set_aspect_ratio;
  gdouble factor_aspect_ratio;  /* 0.0 == auto detect from pixelsizes */
  /* extras */
  gboolean dont_recode_flag;


  /* new params (introduced with ffmpeg 0.4.9pre1) */
  gint32   thread_count;
  gint32   mb_cmp;
  gint32   ildct_cmp;
  gint32   sub_cmp;
  gint32   cmp;
  gint32   pre_cmp;
  gint32   pre_me;
  gdouble  lumi_mask;
  gdouble  dark_mask;
  gdouble  scplx_mask;
  gdouble  tcplx_mask;
  gdouble  p_mask;
  gint32   qns;

  gint32 use_ss;
  gint32 use_aiv;
  gint32 use_obmc;
  gint32 use_loop;
  gint32 use_alt_scan;
  gint32 use_mv0;
  gint32 do_normalize_aqp;
  gint32 use_scan_offset;
  gint32 closed_gop;
  gint32 use_qpel;
  gint32 use_qprd;
  gint32 use_cbprd;
  gint32 do_interlace_dct;
  gint32 do_interlace_me;
  gint32 video_lmin;
  gint32 video_lmax;
  gint32 video_lelim;
  gint32 video_celim;
  gint32 video_intra_quant_bias;
  gint32 video_inter_quant_bias;
  gint32 me_threshold;
  gint32 mb_threshold;
  gint32 intra_dc_precision;
  gint32 error_rate;
  gint32 noise_reduction;
  gint32 sc_threshold;
  gint32 me_range;
  gint32 coder;
  gint32 context;
  gint32 predictor;
  gint32 nsse_weight;
  gint32 subpel_quality;


  /* new parms (found CVS version 2005.03.02 == LIBAVCODEC_BUILD 4744 ) */
  gint32 strict_gop;
  gint32 no_output;
  gint32 video_mb_lmin;
  gint32 video_mb_lmax;

  gint32 video_profile;
  gint32 video_level;
  gint32 frame_skip_threshold;
  gint32 frame_skip_factor;
  gint32 frame_skip_exp;
  gint32 frame_skip_cmp;

  gint32  mux_rate;
  gint32  mux_packet_size;
  gdouble mux_preload;
  gdouble mux_max_delay;


  gint32  ntsc_width;
  gint32  ntsc_height;
  gint32  pal_width;
  gint32  pal_height;

  /* new parms 2009.01.31 */
  
  gint32  b_frame_strategy;
  gint32  workaround_bugs;
  gint32  error_recognition;
  gint32  mpeg_quant;
  gdouble rc_qsquish;
  gdouble rc_qmod_amp;
  gint32  rc_qmod_freq;
  char    rc_eq[60];
  gdouble rc_buffer_aggressivity;
  gint32  dia_size;
  gint32  last_predictor_count;
  gint32  pre_dia_size;
  gint32  inter_threshold;
  gdouble border_masking;
  gint32  me_penalty_compensation;
  gint32  bidir_refine;
  gint32  brd_scale;
  gdouble crf;
  gint32  cqp;
  gint32  keyint_min;
  gint32  refs;
  gint32  chromaoffset;        
  gint32  bframebias;
  gint32  trellis;
  gdouble complexityblur;
  gint32  deblockalpha;
  gint32  deblockbeta;
  gint32  partitions;
  gint32  directpred;
  gint32  cutoff;
  gint32  scenechange_factor;
  gint32  mv0_threshold;
  gint32  b_sensitivity;
  gint32  compression_level;
  gint32  use_lpc;
  gint32  lpc_coeff_precision;
  gint32  min_prediction_order;
  gint32  max_prediction_order;
  gint32  prediction_order_method;
  gint32  min_partition_order;
  gint64  timecode_frame_start;
  gint64  bits_per_raw_sample;
  gint64  channel_layout;
  gdouble rc_max_available_vbv_use;
  gdouble rc_min_vbv_overflow_use;

  gint32  color_primaries;        // enum AVColorPrimaries color_primaries;
  gint32  color_trc;              // enum AVColorTransferCharacteristic color_trc;
  gint32  colorspace;             // enum AVColorSpace colorspace;
  gint32  color_range;            // enum AVColorRange color_range;
  gint32  chroma_sample_location; // enum AVChromaLocation chroma_sample_location;
  gint32  weighted_p_pred;   // int weighted_p_pred;
  gint32  aq_mode;           // int aq_mode;
  gdouble aq_strength;       // float aq_strength;
  gdouble psy_rd;            // float psy_rd;
  gdouble psy_trellis;       // float psy_trellis;
  gint32  rc_lookahead;      // int rc_lookahead;

  /* new params ffmpeg-0.7.11 */
  gdouble crf_max;               // float crf_max;
  gint32  log_level_offset;      // int log_level_offset;
  gint32  slices;                // int slices;
  gint32  thread_type;           // int thread_type;
  gint32  active_thread_type;    // int active_thread_type;
  gint32  thread_safe_callbacks; // int thread_safe_callbacks;
  gint64  vbv_delay;             // uint64_t vbv_delay;
  gint32  audio_service_type;    // enum AVAudioServiceType  audio_service_type;


  gint32 codec_FLAG_GMC;
  gint32 codec_FLAG_INPUT_PRESERVED;
  gint32 codec_FLAG_GRAY;
  gint32 codec_FLAG_EMU_EDGE;
  gint32 codec_FLAG_TRUNCATED;

  gint32 codec_FLAG2_FAST;
  gint32 codec_FLAG2_LOCAL_HEADER;
  gint32 codec_FLAG2_BPYRAMID;
  gint32 codec_FLAG2_WPRED;
  gint32 codec_FLAG2_MIXED_REFS;
  gint32 codec_FLAG2_8X8DCT;
  gint32 codec_FLAG2_FASTPSKIP;
  gint32 codec_FLAG2_AUD;
  gint32 codec_FLAG2_BRDO;
  gint32 codec_FLAG2_INTRA_VLC;
  gint32 codec_FLAG2_MEMC_ONLY;
  gint32 codec_FLAG2_DROP_FRAME_TIMECODE;
  gint32 codec_FLAG2_SKIP_RD;
  gint32 codec_FLAG2_CHUNKS;
  gint32 codec_FLAG2_NON_LINEAR_QUANT;
  gint32 codec_FLAG2_BIT_RESERVOIR;
  gint32 codec_FLAG2_MBTREE;
  gint32 codec_FLAG2_PSY;
  gint32 codec_FLAG2_SSIM;
  gint32 codec_FLAG2_INTRA_REFRESH;

  gint32 partition_X264_PART_I4X4;
  gint32 partition_X264_PART_I8X8;
  gint32 partition_X264_PART_P8X8;
  gint32 partition_X264_PART_P4X4;
  gint32 partition_X264_PART_B8X8;


  void *next;

} GapGveFFMpegValues;




typedef struct GapGveFFMpegGlobalParams {   /* nick: gpp */
  GapGveCommonValues   val;
  GapGveEncAInfo       ainfo;
  GapGveEncList        *ecp;

  GapGveFFMpegValues   evl;

  GtkWidget *shell_window;
  gboolean   startup;
  gboolean   ffpar_save_flag;
  gboolean   show_expert_settings;
  char       ffpar_filename[1024];
  GtkWidget *ffpar_fileselection;   /* ffmpeg video encoder parameter file */
  GtkWidget *fsb__fileselection;    /* passlog file */

  GtkWidget *main_notebook;
  GtkWidget *show_expert_settings_checkbutton;
  
  GtkWidget *ff_aspect_combo;
  GtkWidget *ff_aud_bitrate_combo;
  GtkWidget *ff_aud_bitrate_spinbutton;
  GtkWidget *ff_aud_codec_combo;
  GtkWidget *ff_author_entry;
  GtkWidget *ff_b_frames_spinbutton;
  GtkWidget *ff_b_qfactor_spinbutton;
  GtkWidget *ff_b_qoffset_spinbutton;
  GtkWidget *ff_basic_info_label;
  GtkWidget *ff_bitrate_tol_spinbutton;
  GtkWidget *ff_bufsize_spinbutton;
  GtkWidget *ff_comment_entry;
  GtkWidget *ff_copyright_entry;
  GtkWidget *ff_dct_algo_combo;
  GtkWidget *ff_dont_recode_checkbutton;
  GtkWidget *ff_fileformat_combo;
  GtkWidget *ff_gop_size_spinbutton;
  GtkWidget *ff_bitexact_checkbutton;
  GtkWidget *ff_aspect_checkbutton;
  GtkWidget *ff_aic_checkbutton;
  GtkWidget *ff_umv_checkbutton;
  GtkWidget *ff_i_qfactor_spinbutton;
  GtkWidget *ff_i_qoffset_spinbutton;
  GtkWidget *ff_idct_algo_combo;
  GtkWidget *ff_intra_checkbutton;
  GtkWidget *ff_maxrate_tol_spinbutton;
  GtkWidget *ff_mb_qmax_spinbutton;
  GtkWidget *ff_mb_qmin_spinbutton;
  GtkWidget *ff_mb_decision_combo;
  GtkWidget *ff_minrate_tol_spinbutton;
  GtkWidget *ff_motion_estimation_combo;
  GtkWidget *ff_mv4_checkbutton;
  GtkWidget *ff_partitioning_checkbutton;
  GtkWidget *ff_pass_checkbutton;
  GtkWidget *ff_passlogfile_entry;
  GtkWidget *ff_presets_combo;
  GtkWidget *ff_qblur_spinbutton;
  GtkWidget *ff_qcomp_spinbutton;
  GtkWidget *ff_qdiff_spinbutton;
  GtkWidget *ff_qmax_spinbutton;
  GtkWidget *ff_qmin_spinbutton;
  GtkWidget *ff_qscale_spinbutton;
  GtkWidget *ff_rc_init_cplx_spinbutton;
  GtkWidget *ff_strict_spinbutton;
  GtkWidget *ff_title_entry;
  GtkWidget *ff_vid_bitrate_spinbutton;
  GtkWidget *ff_vid_codec_combo;

  GtkWidget *ff_mb_cmp_combo;
  GtkWidget *ff_ildct_cmp_combo;
  GtkWidget *ff_sub_cmp_combo;
  GtkWidget *ff_cmp_combo;
  GtkWidget *ff_pre_cmp_combo;
  GtkWidget *ff_frame_skip_cmp_combo;
  GtkWidget *ff_coder_combo;
  GtkWidget *ff_predictor_combo;
  
  GtkWidget *ff_use_ss_checkbutton;
  GtkWidget *ff_use_aiv_checkbutton;
  GtkWidget *ff_use_obmc_checkbutton;
  GtkWidget *ff_use_loop_checkbutton;
  GtkWidget *ff_use_alt_scan_checkbutton;
  GtkWidget *ff_use_mv0_checkbutton;
  GtkWidget *ff_do_normalize_aqp_checkbutton;
  GtkWidget *ff_use_scan_offset_checkbutton;
  GtkWidget *ff_closed_gop_checkbutton;
  GtkWidget *ff_use_qpel_checkbutton;
  GtkWidget *ff_use_qprd_checkbutton;
  GtkWidget *ff_use_cbprd_checkbutton;
  GtkWidget *ff_do_interlace_dct_checkbutton;
  GtkWidget *ff_do_interlace_me_checkbutton;
  GtkWidget *ff_strict_gop_checkbutton;
  GtkWidget *ff_no_output_checkbutton;


  GtkWidget *ff_codec_FLAG_GMC_checkbutton;
  GtkWidget *ff_codec_FLAG_INPUT_PRESERVED_checkbutton;
  GtkWidget *ff_codec_FLAG_GRAY_checkbutton;
  GtkWidget *ff_codec_FLAG_EMU_EDGE_checkbutton;
  GtkWidget *ff_codec_FLAG_TRUNCATED_checkbutton;

  GtkWidget *ff_codec_FLAG2_FAST_checkbutton;
  GtkWidget *ff_codec_FLAG2_LOCAL_HEADER_checkbutton;
  GtkWidget *ff_codec_FLAG2_BPYRAMID_checkbutton;
  GtkWidget *ff_codec_FLAG2_WPRED_checkbutton;
  GtkWidget *ff_codec_FLAG2_MIXED_REFS_checkbutton;
  GtkWidget *ff_codec_FLAG2_8X8DCT_checkbutton;
  GtkWidget *ff_codec_FLAG2_FASTPSKIP_checkbutton;
  GtkWidget *ff_codec_FLAG2_AUD_checkbutton;
  GtkWidget *ff_codec_FLAG2_BRDO_checkbutton;
  GtkWidget *ff_codec_FLAG2_INTRA_VLC_checkbutton;
  GtkWidget *ff_codec_FLAG2_MEMC_ONLY_checkbutton;
  GtkWidget *ff_codec_FLAG2_DROP_FRAME_TIMECODE_checkbutton;
  GtkWidget *ff_codec_FLAG2_SKIP_RD_checkbutton;
  GtkWidget *ff_codec_FLAG2_CHUNKS_checkbutton;
  GtkWidget *ff_codec_FLAG2_NON_LINEAR_QUANT_checkbutton;
  GtkWidget *ff_codec_FLAG2_BIT_RESERVOIR_checkbutton;

  GtkWidget *ff_codec_FLAG2_MBTREE_checkbutton;
  GtkWidget *ff_codec_FLAG2_PSY_checkbutton;
  GtkWidget *ff_codec_FLAG2_SSIM_checkbutton;
  GtkWidget *ff_codec_FLAG2_INTRA_REFRESH_checkbutton;

  GtkWidget *ff_partition_X264_PART_I4X4_checkbutton;
  GtkWidget *ff_partition_X264_PART_I8X8_checkbutton;
  GtkWidget *ff_partition_X264_PART_P8X8_checkbutton;
  GtkWidget *ff_partition_X264_PART_P4X4_checkbutton;
  GtkWidget *ff_partition_X264_PART_B8X8_checkbutton;



  GtkWidget *ff_mux_rate_spinbutton;
  GtkWidget *ff_mux_packet_size_spinbutton;
  GtkWidget *ff_mux_preload_spinbutton;
  GtkWidget *ff_mux_max_delay_spinbutton;


  GtkObject *ff_aud_bitrate_spinbutton_adj;
  GtkObject *ff_vid_bitrate_spinbutton_adj;
  GtkObject *ff_qscale_spinbutton_adj;
  GtkObject *ff_qmin_spinbutton_adj;
  GtkObject *ff_qmax_spinbutton_adj;
  GtkObject *ff_qdiff_spinbutton_adj;
  GtkObject *ff_gop_size_spinbutton_adj;
  GtkObject *ff_qblur_spinbutton_adj;
  GtkObject *ff_qcomp_spinbutton_adj;
  GtkObject *ff_rc_init_cplx_spinbutton_adj;
  GtkObject *ff_b_qfactor_spinbutton_adj;
  GtkObject *ff_i_qfactor_spinbutton_adj;
  GtkObject *ff_b_qoffset_spinbutton_adj;
  GtkObject *ff_i_qoffset_spinbutton_adj;
  GtkObject *ff_bitrate_tol_spinbutton_adj;
  GtkObject *ff_maxrate_tol_spinbutton_adj;
  GtkObject *ff_minrate_tol_spinbutton_adj;
  GtkObject *ff_bufsize_spinbutton_adj;
  GtkObject *ff_strict_spinbutton_adj;
  GtkObject *ff_mb_qmin_spinbutton_adj;
  GtkObject *ff_mb_qmax_spinbutton_adj;
  GtkObject *ff_b_frames_spinbutton_adj;

  GtkObject *ff_mux_rate_adj;
  GtkObject *ff_mux_packet_size_adj;
  GtkObject *ff_mux_preload_adj;
  GtkObject *ff_mux_max_delay_adj;

} GapGveFFMpegGlobalParams;


void  gap_enc_ffmpeg_main_init_preset_params(GapGveFFMpegValues *epp, gint preset_idx);

#endif
