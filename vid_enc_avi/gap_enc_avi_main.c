/* gap_enc_main_avi.c
 *  by hof (Wolfgang Hofer & Gernot Ziegler (gz@lysator.liu.se))
 *
 * GAP ... Gimp Animation Plugins
 *
 * This is the MAIN Module for the GAP AVI video encoder
 *  This encoder writes frames (and optional audio) as Videofile in the AVI format.
 *
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* revision history:
 * version 2.1.0b;  2004.10.07   hof: bugfix init xvid_control->plugins[xvid_enc_create->num_plugins]
 *                                    must start at index 0 (not at 1)
 *                  2004.10.05   hof: relinked with xvid-1.0.2 (same crash)
 *                  2004.06.12   hof: update to xvid-1.0.0 (but does permanent crash)
 * version 1.2.2b;  2003.04.17   hof: conditional compile of DivX Stuff
 *                                    (DivX dos not work and is disabled in this release)
 * version 1.2.2b;  2002.12.08   hof: created (based on avi_main.c done by gz)
 */

#include <config.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>


/* names of the supported AVI Codecs */
#define GAP_AVI_CODEC_NONE "RGB"
#define GAP_AVI_CODEC_JPEG "JPEG"
/* ??? not sure what to use for the correct 4cc codec names for xvid divx MPEG 4 */
#define GAP_AVI_CODEC_XVID "XVID"
#define GAP_AVI_CODEC_DIVX "div5"



/* GAP includes */
#include "gap_gvetypes.h"


#include "gap_file_util.h"
#include "gap_libgimpgap.h"
#include "gap_audio_wav.h"
#include "gap_arr_dialog.h"    /* for the GUI provisorium */

#include "gap_gve_story.h"     /* for STORYBOARD support */

#include "gap_gve_jpeg.h"      /* for the builtin JPEG support */
#include "gap_gve_raw.h"       /* for raw CODEC support */
#include "gap_gve_xvid.h"      /* for XVID CODEC support */
#include "gap_enc_avi_main.h"
#include "gap_enc_avi_gui.h"



static gint p_avi_encode(GapGveAviGlobalParams *gpp);
static gint p_avi_encode_dialog(GapGveAviGlobalParams *gpp);


/* Includes for extra LIBS */
#include "avilib.h"


static char *gap_enc_avi_version = "2.1.0a; 2004/10/07";


/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 1;
GapGveAviGlobalParams global_params;
int global_nargs_avi_enc_par;

static void query(void);
static void  run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals);  /* out-parameters */
static void   p_gimp_get_data(const char *key, void *buffer, gint expected_size);

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  query, /* query_proc */
  run,   /* run_proc */
};





/* ------------------------
 * MAIN
 * ------------------------
 */

MAIN ()

/* --------------------------------
 * query
 * --------------------------------
 */
static void
query ()
{
  gchar      *l_ecp_key;

  /* video encoder standard parameters (same for each encoder)  */
  static GimpParamDef args_avi_enc[] =
  {
    {GIMP_PDB_INT32, "run_mode", "non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (must be one of the input frames, or is ignored if storyboard_file is used"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_STRING, "videofile", "filename of AVI video (to write)"},
    {GIMP_PDB_INT32, "range_from", "number of first frame"},
    {GIMP_PDB_INT32, "range_to", "number of last frame"},
    {GIMP_PDB_INT32, "vid_width", "Width of resulting Video Frames (all Frames are scaled to this width)"},
    {GIMP_PDB_INT32, "vid_height", "Height of resulting Video Frames (all Frames are scaled to this height)"},
    {GIMP_PDB_INT32, "vid_format", "videoformat:  0=comp., 1=PAL, 2=NTSC, 3=SECAM, 4=MAC, 5=unspec"},
    {GIMP_PDB_FLOAT, "framerate", "framerate in frames per seconds"},
    {GIMP_PDB_INT32, "samplerate", "audio samplerate in samples per seconds (is ignored .wav files are used)"},
    {GIMP_PDB_STRING, "audfile", "optional audiodata file .wav must contain uncompressed 16 bit samples. pass empty string if no audiodata should be included"},
    {GIMP_PDB_INT32, "use_rest", "0 == use default values for encoder specific params, 1 == use encoder specific params"},
    {GIMP_PDB_STRING, "filtermacro_file", "macro to apply on each handled frame. (textfile with filter plugin names and LASTVALUE bufferdump"},
    {GIMP_PDB_STRING, "storyboard_file", "textfile with list of one or more framesequences"},
    {GIMP_PDB_INT32,  "input_mode", "0 ... image is one of the frames to encode, range_from/to params refere to numberpart of the other frameimages on disc. \n"
                                    "1 ... image is multilayer, range_from/to params refere to layer index. \n"
				    "2 ... image is ignored, input is specified by storyboard_file parameter."},
  };
  static int nargs_avi_enc = sizeof(args_avi_enc) / sizeof(args_avi_enc[0]);

  /* video encoder specific parameters */
  static GimpParamDef args_avi_enc_par[] =
  {
    {GIMP_PDB_INT32, "run_mode", "interactive, non-interactive"},
    {GIMP_PDB_STRING, "key_stdpar", "key to get standard video encoder params via gimp_get_data"},
    {GIMP_PDB_STRING, "codec_name", "identifier of the codec. one of the strings \"JPEG\", \"RGB\", \"DIVX\" "},
    {GIMP_PDB_INT32, "dont_recode_frames", "=1: store the frames _directly_ into the AVI. "
                                 "(works only for codec_name JPEG and input frames must be 4:2:2 JPEG !)"},
    {GIMP_PDB_INT32, "jpeg_interlaced", "=1: store two JPEG frames, for the odd/even lines"},
    {GIMP_PDB_INT32, "jpeg_quality", "the quality of the coded jpegs (0 - 100%)"},
    {GIMP_PDB_INT32, "jpeg_odd_even", "if jpeg_interlaced: odd frames first ?"},


    {GIMP_PDB_INT32, "xvid_rc_bitrate", "bitrate"},
    {GIMP_PDB_INT32, "xvid_rc_reaction_delay_factor", "use -1 to enforce default"},
    {GIMP_PDB_INT32, "xvid_rc_averaging_period", "use -1 to enforce default"},
    {GIMP_PDB_INT32, "xvid_rc_buffer", "use -1 to enforce default"},
    {GIMP_PDB_INT32, "xvid_max_quantizer", "upper limit for quantize Range 1==BEST Quality 31==Best Compression"},
    {GIMP_PDB_INT32, "xvid_min_quantizer", "lower limit for quantize Range 1==BEST Quality 31==Best Compression"},
    {GIMP_PDB_INT32, "xvid_max_key_interval", "max distance for keyframes (I-frames) "},
    {GIMP_PDB_INT32, "xvid_quality_preset", "-1 == no preset, 0..6 set general and motion algorithm flags according to presets, where 0==low quality(fast) 6==best(slow)"},
    {GIMP_PDB_INT32, "xvid_general", "ignored if preset ==-1, see xvid docs for description which bit turns which algorithm on"},
    {GIMP_PDB_INT32, "xvid_motion", "ignored if preset ==-1, see xvid docs for description which bit turns which algorithm on"},

    {GIMP_PDB_INT32, "APP0_marker", "=1: write APP0 marker for each frame into the AVI. "
                                 "( The APP0 marker is evaluated by some Windows programs for AVIs)"}

  };
  static int nargs_avi_enc_par = sizeof(args_avi_enc_par) / sizeof(args_avi_enc_par[0]);

  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  /* video encoder standard query (same for each encoder) */
  static GimpParamDef args_in_ecp[] =
  {
    {GIMP_PDB_STRING, "param_name", "name of the parameter, supported: menu_name, video_extension"},
  };

  static GimpParamDef args_out_ecp[] =
  {
    {GIMP_PDB_STRING, "param_value", "parmeter value"},
  };

  static int nargs_in_enp = sizeof(args_in_ecp) / sizeof(args_in_ecp[0]);
  static int nargs_out_enp = (sizeof(args_out_ecp) / sizeof(args_out_ecp[0]));

  INIT_I18N();

  global_nargs_avi_enc_par = nargs_avi_enc_par;

  gimp_install_procedure(GAP_PLUGIN_NAME_AVI_ENCODE,
                         _("avi video encoding for anim frames. Menu: @AVI@"),
                         _("This plugin does fake video encoding of animframes."
                         " the (optional) audiodata must be a raw datafile(s) or .wav (RIFF WAVEfmt ) file(s)"
                         " .wav files can be mono (1) or stereo (2channels) audiodata must be 16bit uncompressed."
                         " IMPORTANT:  you should first call "
                         "\"" GAP_PLUGIN_NAME_AVI_PARAMS  "\""
                         " to set encoder specific paramters, then set the use_rest parameter to 1 to use them."),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         gap_enc_avi_version,
                         NULL,                      /* has no Menu entry, just a PDB interface */
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_avi_enc, nreturn_vals,
                         args_avi_enc, return_vals);



  gimp_install_procedure(GAP_PLUGIN_NAME_AVI_PARAMS,
                         _("Set Parameters for GAP avi video encoder Plugins"),
                         _("This plugin sets avi specific video encoding parameters."),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         gap_enc_avi_version,
                         NULL,                      /* has no Menu entry, just a PDB interface */
                         NULL,
                         GIMP_PLUGIN,
                         nargs_avi_enc_par, nreturn_vals,
                         args_avi_enc_par, return_vals);


  l_ecp_key = g_strdup_printf("%s%s", GAP_QUERY_PREFIX_VIDEO_ENCODERS, GAP_PLUGIN_NAME_AVI_ENCODE);
  gimp_install_procedure(l_ecp_key,
                         _("Get GUI Parameters for GAP avi video encoder"),
                         _("This plugin returns avi encoder specific parameters."),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         gap_enc_avi_version,
                         NULL,                      /* has no Menu entry, just a PDB interface */
                         NULL,
                         GIMP_PLUGIN,
                         nargs_in_enp , nargs_out_enp,
                         args_in_ecp, args_out_ecp);


  g_free(l_ecp_key);
}       /* end query */


/* --------------------------------
 * run
 * --------------------------------
 */
static void
run (const gchar *name,          /* name of plugin */
     gint n_params,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  GapGveAviValues *epp;
  GapGveAviGlobalParams *gpp;

  static GimpParam values[1];
  gint32     l_rc;
  const char *l_env;
  char       *l_ecp_key1;
  char       *l_encoder_key;

  gpp = &global_params;
  epp = &gpp->evl;
  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  INIT_I18N();


  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_SUCCESS;

  l_env = g_getenv("GAP_DEBUG_ENC");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  if(gap_debug) printf("\n\nSTART of PlugIn: %s\n", name);

  l_ecp_key1 = g_strdup_printf("%s%s", GAP_QUERY_PREFIX_VIDEO_ENCODERS, GAP_PLUGIN_NAME_AVI_ENCODE);
  l_encoder_key = g_strdup(GAP_PLUGIN_NAME_AVI_ENCODE);


  gap_enc_avi_main_init_default_params(epp);

  if (strcmp (name, l_ecp_key1) == 0)
  {
      /* this interface replies to the queries of the common encoder gui */
      gchar *param_name;

      param_name = param[0].data.d_string;
      if(gap_debug) printf("query for param_name: %s\n", param_name);
      *nreturn_vals = 2;

      values[1].type = GIMP_PDB_STRING;
      if(strcmp (param_name, GAP_VENC_PAR_MENU_NAME) == 0)
      {
        values[1].data.d_string = g_strdup(GAP_MENUNAME);
      }
      else if (strcmp (param_name, GAP_VENC_PAR_VID_EXTENSION) == 0)
      {
        values[1].data.d_string = g_strdup(".avi");
      }
      else if (strcmp (param_name, GAP_VENC_PAR_SHORT_DESCRIPTION) == 0)
      {
        values[1].data.d_string =
          g_strdup(_("AVI Encoder\n"
                     "writes RIFF AVI encoded videos\n"
                     "and supports MPEG4 (XVID), JPEG or RAW (uncompressed)\n"
                     )
                  );
      }
      else if (strcmp (param_name, GAP_VENC_PAR_GUI_PROC) == 0)
      {
        values[1].data.d_string = g_strdup(GAP_PLUGIN_NAME_AVI_PARAMS);
      }
      else
      {
        values[1].data.d_string = g_strdup("\0");
      }
  }
  else if (strcmp (name, GAP_PLUGIN_NAME_AVI_PARAMS) == 0)
  {
      /* this interface sets the encoder specific parameters */
      gint l_set_it;
      gchar  *l_key_stdpar;

      gpp->val.run_mode = param[0].data.d_int32;
      l_key_stdpar = param[1].data.d_string;
      gpp->val.vid_width = 320;
      gpp->val.vid_height = 200;
      p_gimp_get_data(l_key_stdpar, &gpp->val, sizeof(GapGveCommonValues));

      if(gap_debug)  printf("rate: %f  w:%d h:%d\n", (float)gpp->val.framerate, (int)gpp->val.vid_width, (int)gpp->val.vid_height);

      l_set_it = TRUE;
      if (gpp->val.run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        /* set video encoder specific params */
        if (n_params != global_nargs_avi_enc_par)
        {
          values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
          l_set_it = FALSE;
        }
        else
        {
           gint l_ii;

           if(param[2+1].data.d_string)
           {
             g_snprintf(epp->codec_name, sizeof(epp->codec_name), "%s", param[2+1].data.d_string);
           }
           l_ii = 2+2;
           epp->dont_recode_frames = param[l_ii++].data.d_int32;
           epp->jpeg_interlaced    = param[l_ii++].data.d_int32;
           epp->jpeg_quality       = param[l_ii++].data.d_int32;
           epp->jpeg_odd_even      = param[l_ii++].data.d_int32;

           epp->xvid.rc_bitrate                 = param[l_ii++].data.d_int32;
           epp->xvid.rc_reaction_delay_factor   = param[l_ii++].data.d_int32;
           epp->xvid.rc_averaging_period        = param[l_ii++].data.d_int32;
           epp->xvid.rc_buffer                  = param[l_ii++].data.d_int32;
           epp->xvid.max_quantizer              = param[l_ii++].data.d_int32;
           epp->xvid.min_quantizer              = param[l_ii++].data.d_int32;
           epp->xvid.max_key_interval           = param[l_ii++].data.d_int32;
           epp->xvid.quality_preset             = param[l_ii++].data.d_int32;
           epp->xvid.general                    = param[l_ii++].data.d_int32;
           epp->xvid.motion                     = param[l_ii++].data.d_int32;

           epp->APP0_marker                    = param[l_ii++].data.d_int32;

        }
      }
      else
      {
        /* try to read encoder specific params */
        p_gimp_get_data(l_encoder_key, epp, sizeof(GapGveAviValues));

        ///////  if(0 != p_avi_encode_dialog(gpp))
        if(0 != gap_enc_avi_gui_dialog(gpp))
        {
          l_set_it = FALSE;
        }
      }

      if(l_set_it)
      {
         if(gap_debug) printf("Setting Encoder specific Params\n");
         gimp_set_data(l_encoder_key, epp, sizeof(GapGveAviValues));
      }

  }
  else   if (strcmp (name, GAP_PLUGIN_NAME_AVI_ENCODE) == 0)
  {
      char *l_base;
      int   l_l;

      /* run the video encoder procedure */

      gpp->val.run_mode = param[0].data.d_int32;

      /* get image_ID and animinfo */
      gpp->val.image_ID    = param[1].data.d_image;
      gap_gve_misc_get_ainfo(gpp->val.image_ID, &gpp->ainfo);

      /* set initial (default) values */
      l_base = g_strdup(gpp->ainfo.basename);
      l_l = strlen(l_base);

      if (l_l > 0)
      {
         if(l_base[l_l -1] == '_')
         {
           l_base[l_l -1] = '\0';
         }
      }
      if(gap_debug) printf("Init Default parameters for %s base: %s\n", name, l_base);
      g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s.mpg", l_base);

      gpp->val.audioname1[0] = '\0';
      gpp->val.filtermacro_file[0] = '\0';
      gpp->val.storyboard_file[0] = '\0';
      gpp->val.framerate = gpp->ainfo.framerate;
      gpp->val.range_from = gpp->ainfo.curr_frame_nr;
      gpp->val.range_to   = gpp->ainfo.last_frame_nr;
      gpp->val.samplerate = 0;
      gpp->val.vid_width  = gimp_image_width(gpp->val.image_ID) - (gimp_image_width(gpp->val.image_ID) % 16);
      gpp->val.vid_height = gimp_image_height(gpp->val.image_ID) - (gimp_image_height(gpp->val.image_ID) % 16);
      gpp->val.vid_format = VID_FMT_NTSC;
      gpp->val.input_mode = GAP_RNGTYPE_FRAMES;

      g_free(l_base);

      if (n_params != GAP_VENC_NUM_STANDARD_PARAM)
      {
        values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
      }
      else
      {
        if(gap_debug) printf("Reading Standard parameters for %s\n", name);

        if (param[3].data.d_string[0] != '\0') { g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s", param[3].data.d_string); }
        if (param[4].data.d_int32 >= 0) { gpp->val.range_from =    param[4].data.d_int32; }
        if (param[5].data.d_int32 >= 0) { gpp->val.range_to   =    param[5].data.d_int32; }
        if (param[6].data.d_int32 > 0)  { gpp->val.vid_width  =    param[6].data.d_int32; }
        if (param[7].data.d_int32 > 0)  { gpp->val.vid_height  =   param[7].data.d_int32; }
        if (param[8].data.d_int32 > 0)  { gpp->val.vid_format  =   param[8].data.d_int32; }
        gpp->val.framerate   = param[9].data.d_float;
        gpp->val.samplerate  = param[10].data.d_int32;
        g_snprintf(gpp->val.audioname1, sizeof(gpp->val.audioname1), "%s", param[11].data.d_string);

        /* use avi specific encoder parameters (0==run with default values) */
        if (param[12].data.d_int32 == 0)
        {
          if(gap_debug) printf("Running the Encoder %s with Default Values\n", name);
        }
        else
        {
          /* try to read encoder specific params */
          p_gimp_get_data(name, epp, sizeof(GapGveAviValues));
        }
        if (param[13].data.d_string[0] != '\0') { g_snprintf(gpp->val.filtermacro_file, sizeof(gpp->val.filtermacro_file), "%s", param[13].data.d_string); }
        if (param[14].data.d_string[0] != '\0') { g_snprintf(gpp->val.storyboard_file, sizeof(gpp->val.storyboard_file), "%s", param[14].data.d_string); }
        if (param[15].data.d_int32 >= 0) { gpp->val.input_mode   =    param[15].data.d_int32; }
      }

      if (values[0].data.d_status == GIMP_PDB_SUCCESS)
      {
         if (l_rc >= 0 )
         {
            l_rc = p_avi_encode(gpp);
            /* delete images in the cache
             * (the cache may have been filled while parsing
             * and processing a storyboard file)
             */
             gap_gve_story_drop_image_cache();
         }
      }
  }
  else
  {
      values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
  }

 if(l_rc < 0)
 {
    values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
 }

}       /* end run */


/* --------------------------------
 * p_gimp_get_data
 * --------------------------------
 */
static void
p_gimp_get_data(const char *key, void *buffer, gint expected_size)
{
  if(gimp_get_data_size(key) == expected_size)
  {
      if(gap_debug) printf("p_gimp_get_data: key:%s\n", key);
      gimp_get_data(key, buffer);
  }
  else
  {
     if(gap_debug)
     {
       printf("ERROR: p_gimp_get_data key:%s failed\n", key);
       printf("ERROR: gimp_get_data_size:%d  expected size:%d\n"
             , (int)gimp_get_data_size(key)
             , (int)expected_size);
     }
  }
}  /* end p_gimp_get_data */


/* ------------------------------------
 * gap_enc_avi_main_init_default_params
 * ------------------------------------
 */
void
gap_enc_avi_main_init_default_params(GapGveAviValues *epp)
{
  if(gap_debug) printf("gap_enc_avi_main_init_default_params\n");

  g_snprintf(epp->codec_name, sizeof(epp->codec_name), GAP_AVI_CODEC_JPEG);
  epp->dont_recode_frames = 0;
  epp->jpeg_interlaced    = 0;
  epp->jpeg_quality       = 84;
  epp->jpeg_odd_even      = 0;

  epp->xvid.rc_bitrate               = 900 * 1000;
  epp->xvid.rc_reaction_delay_factor = 16;
  epp->xvid.rc_averaging_period      = 100;
  epp->xvid.rc_buffer                = 10;
  epp->xvid.max_quantizer            = 31;
  epp->xvid.min_quantizer            = 1;
  epp->xvid.max_key_interval         = 120;     /* recomanded framerate * 10 */
  epp->xvid.quality_preset           = 6;       /* best */

#ifdef ENABLE_LIBXVIDCORE
  gap_gve_xvid_algorithm_preset(&epp->xvid);
#endif

  epp->APP0_marker = TRUE;
}  /* end gap_enc_avi_main_init_default_params */


/* ============================================================================
 * p_avi_encode_dialog
 *
 *   return  0 .. OK
 *          -1 .. in case of Error or cancel
 * ============================================================================
 */
static gint
p_avi_encode_dialog(GapGveAviGlobalParams *gpp)
{
#define ARGC_AVI_DIALOG_PARAMS_MAX 28
  gint argc_avi_dialog_params;
  static GapArrArg  argv[ARGC_AVI_DIALOG_PARAMS_MAX];
  GapGveAviValues *epp;
  gint  l_ii;

  gint idx_app_mark;
  gint xvid_bitrate;
  gint xvid_rcdelay;
  gint xvid_rcperiod;
  gint xvid_rcbuffer;
  gint xvid_maxq;
  gint xvid_minq;
  gint xvid_keyint;
  gint xvid_qpreset;


  epp = &gpp->evl;


  l_ii = 0;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL);
  argv[l_ii].label_txt = _("AVI encoding options");

  l_ii = 1;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TEXT);
  argv[l_ii].label_txt = _("CODEC Name:");
  argv[l_ii].help_txt =_("4 char codec code name. One of: RGB (raw uncompressed) JPEG,  XVID");
  argv[l_ii].text_buf_len = sizeof(epp->codec_name);
  argv[l_ii].text_buf_ret = epp->codec_name;

  /* --------------- JPEG options ----------------------- */

  l_ii = 2;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TOGGLE);
  argv[l_ii].label_txt =_("Don't recode");
  argv[l_ii].help_txt =_("Don't recode the input JPEG frames (works for 4:2:2 JPEG only, may result in unreadable Video)");
  argv[l_ii].int_ret = epp->dont_recode_frames;

  l_ii = 3;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TOGGLE);
  argv[l_ii].label_txt = _("Interlaced Frames)");
  argv[l_ii].help_txt = _("Generate interlaced JPEGs (two frames for odd/even lines)");
  argv[l_ii].int_ret = epp->jpeg_interlaced;

  l_ii = 4;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TOGGLE);
  argv[l_ii].label_txt = _("Odd Frames first");
  argv[l_ii].help_txt = _("Check if you want the odd frames to be coded first (only for interlaced JPEGs)");
  argv[l_ii].int_ret = epp->jpeg_odd_even;

  l_ii = 5;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("JPEG quality:");
  argv[l_ii].help_txt  = _("The quality setting of the generated JPEG frames (100=best)");
  argv[l_ii].int_min   = 0;
  argv[l_ii].int_max   = 100;
  argv[l_ii].int_ret   = epp->jpeg_quality;

  /* --------------- XVID options ----------------------- */

  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL);
  argv[l_ii].label_txt = _("options for the XVID CODEC");

  l_ii++;
  xvid_bitrate = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("KBitrate:");
  argv[l_ii].help_txt  = _("Kilobitrate for XVID Codec (1 = 1000 Bit/sec) -1 for default");
  argv[l_ii].int_min   = -1;
  argv[l_ii].int_max   = 9999;
  argv[l_ii].int_ret   = epp->xvid.rc_bitrate / 1000;

  l_ii++;
  xvid_rcdelay = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("rc_reaction_delay:");
  argv[l_ii].help_txt  = _("for XVID Codec  -1 for default");
  argv[l_ii].int_min   = -1;
  argv[l_ii].int_max   = 100;
  argv[l_ii].int_ret   = epp->xvid.rc_reaction_delay_factor;

  l_ii++;
  xvid_rcperiod = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("rc_avg_period:");
  argv[l_ii].help_txt  = _("for XVID Codec  -1 for default");
  argv[l_ii].int_min   = -1;
  argv[l_ii].int_max   = 1000;
  argv[l_ii].int_ret   = epp->xvid.rc_averaging_period;

  l_ii++;
  xvid_rcbuffer = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("rc_buffer:");
  argv[l_ii].help_txt  = _("for XVID Codec  -1 for default");
  argv[l_ii].int_min   = -1;
  argv[l_ii].int_max   = 100;
  argv[l_ii].int_ret   = epp->xvid.rc_buffer;

  l_ii++;
  xvid_maxq = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("max_quantizer:");
  argv[l_ii].help_txt  = _("for XVID Codec  upper limit for quantize Range 1 == BEST Quality ");
  argv[l_ii].int_min   = 1;
  argv[l_ii].int_max   = 31;
  argv[l_ii].int_ret   = epp->xvid.max_quantizer;

  l_ii++;
  xvid_minq = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("min_quantizer:");
  argv[l_ii].help_txt  = _("for XVID Codec  lower limit for quantize Range 1 == BEST Quality ");
  argv[l_ii].int_min   = 1;
  argv[l_ii].int_max   = 31;
  argv[l_ii].int_ret   = epp->xvid.min_quantizer;

  l_ii++;
  xvid_keyint = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("key_interval:");
  argv[l_ii].help_txt  = _("for XVID Codec  max distance for keyframes (I-frames) ");
  argv[l_ii].int_min   = 1;
  argv[l_ii].int_max   = 500;
  argv[l_ii].int_ret   = epp->xvid.max_key_interval;


  l_ii++;
  xvid_qpreset = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_INT_PAIR);
  argv[l_ii].constraint = TRUE;
  argv[l_ii].label_txt = _("quality:");
  argv[l_ii].help_txt  = _("for XVID Codec select algoritm presets where 0==low quality(fast) 6==best(slow) ");
  argv[l_ii].int_min   = 0;
  argv[l_ii].int_max   = 6;
  argv[l_ii].int_ret   = epp->xvid.quality_preset;


  /* --------------- APP0 Marker ----------------------- */
  l_ii++;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_LABEL);
  argv[l_ii].label_txt = _("APP0 Marker for each frame");

  l_ii++;
  idx_app_mark = l_ii;
  gap_arr_arg_init(&argv[l_ii], GAP_ARR_WGT_TOGGLE);
  argv[l_ii].label_txt =_("APP0 Marker:");
  argv[l_ii].help_txt =_("Write APP0 Marker for each encoded frame."
                         " (The APP0 marker is evaluated by some Windows programs for AVIs");
  argv[l_ii].int_ret = epp->APP0_marker;

  argc_avi_dialog_params = l_ii + 1;

  if(TRUE == gap_arr_ok_cancel_dialog("GAP AVI Encode",
                                 "Settings :",
                                  argc_avi_dialog_params, argv))
  {
      epp->dont_recode_frames       = (long)(argv[2].int_ret);
      epp->jpeg_interlaced          = (long)(argv[3].int_ret);
      epp->jpeg_odd_even            = (long)(argv[4].int_ret);
      epp->jpeg_quality             = (long)(argv[5].int_ret);

      epp->xvid.rc_bitrate               = (long)(argv[xvid_bitrate].int_ret)  * 1000;
      epp->xvid.rc_reaction_delay_factor = (long)(argv[xvid_rcdelay].int_ret);
      epp->xvid.rc_averaging_period      = (long)(argv[xvid_rcperiod].int_ret);
      epp->xvid.rc_buffer                = (long)(argv[xvid_rcbuffer].int_ret);
      epp->xvid.max_quantizer            = (long)(argv[xvid_maxq].int_ret);
      epp->xvid.min_quantizer            = (long)(argv[xvid_minq].int_ret);
      epp->xvid.max_key_interval         = (long)(argv[xvid_keyint].int_ret);
      epp->xvid.quality_preset           = (long)(argv[xvid_qpreset].int_ret);
      epp->APP0_marker                   = (long)(argv[idx_app_mark].int_ret);

      return 0;
  }
  else
  {
      return -1;
  }
}    /* end p_avi_encode_dialog */


/* ============================================================================
 * p_avi_encode
 *    The main "productive" routine
 *    avi encoding of anim frames, based on avilib (by Rainer Johanni)
 *    Optional you can provide audio, too
 *    (wav_audiofile must be provided in that case)
 *
 * returns   value >= 0 if all went ok
 *           (or -1 on error)
 * ============================================================================
 */
static gint
p_avi_encode(GapGveAviGlobalParams *gpp)
{
  GapGveAviValues   *epp;
  avi_t               *l_avifile;
  static GapGveStoryVidHandle *l_vidhand = NULL;
  gint32        l_tmp_image_id = -1;
  gint32        l_layer_id = -1;
  GimpDrawable *l_drawable = NULL;
  long          l_cur_frame_nr;
  long          l_step, l_begin, l_end;
  gdouble       l_percentage, l_percentage_step;
  int           l_rc;

  FILE *l_fp_inwav = NULL;
  gint32 l_FRAME_size;
  gint32 datasize;
  gint32 audio_size = 0;
  gint32 audio_stereo = 0;
  long  l_sample_rate = 22050;
  long  l_channels = 2;
  long  l_bytes_per_sample = 4;
  long  l_bits = 16;
  long  l_samples = 0;

  guchar *buffer; /* Holding misc. file contents */

  gint32 wavsize = 0; /* Data size of the wav file */
  long audio_margin = 8192; /* The audio chunk size */
  long audio_filled_100 = 0;
  long audio_per_frame_x100 = -1;
  long frames_per_second_x100 = 2500;
  gdouble frames_per_second_x100f = 2500;
  char databuffer[300000]; /* For transferring audio data */

#ifdef ENABLE_LIBXVIDCORE
  GapGveXvidControl      *xvid_control = NULL;
#endif

  epp = &gpp->evl;

  if(gap_debug)
  {
     printf("p_avi_encode: START\n");
     printf("  videoname: %s\n", gpp->val.videoname);
     printf("  audioname1: %s\n", gpp->val.audioname1);
     printf("  basename: %s\n", gpp->ainfo.basename);
     printf("  extension: %s\n", gpp->ainfo.extension);
     printf("  range_from: %d\n", (int)gpp->val.range_from);
     printf("  range_to: %d\n", (int)gpp->val.range_to);
     printf("  framerate: %f\n", (float)gpp->val.framerate);
     printf("  samplerate: %d\n", (int)gpp->val.samplerate);
     printf("  vid_width: %d\n", (int)gpp->val.vid_width);
     printf("  vid_height: %d\n", (int)gpp->val.vid_height);
     printf("  image_ID: %d\n", (int)gpp->val.image_ID);
     printf("  storyboard_file: %s\n", gpp->val.storyboard_file);
     printf("  input_mode: %d\n", gpp->val.input_mode);

     printf("  codec_name: %s\n", epp->codec_name);
  }

  l_rc = 0;
  l_layer_id = -1;

  /* make list of frameranges */
  { gint32 l_total_framecount;
  l_vidhand = gap_gve_story_open_vid_handle (gpp->val.input_mode
                                       ,gpp->val.image_ID
				       ,gpp->val.storyboard_file
                                       ,gpp->ainfo.basename
                                       ,gpp->ainfo.extension
                                       ,gpp->val.range_from
                                       ,gpp->val.range_to
                                       ,&l_total_framecount
                                       );
  }

  /* TODO check for overwrite */

  if (gap_debug) printf("Creating avi file.\n");
  /* open the AVI video file for write (create) */
  l_avifile = AVI_open_output_file(gpp->val.videoname);
  if(l_avifile == NULL)
  {
    l_rc = -1;
  }

  l_percentage = 0.0;
  if(gpp->val.run_mode == GIMP_RUN_INTERACTIVE)
  {
    gimp_progress_init(_("AVI Video Encoding .."));
  }


  l_fp_inwav = NULL;
  if(0 == gap_audio_wav_file_check( gpp->val.audioname1
                                , &l_sample_rate
                                , &l_channels
                                , &l_bytes_per_sample
                                , &l_bits
                                , &l_samples
                                ))
  {
    /* l_bytes_per_sample: 8bitmono: 1, 16bit_mono: 2, 8bitstereo: 2, 16bitstereo: 4 */
    wavsize = l_samples * l_bytes_per_sample;
    audio_size = l_bits;                          /*  16 or 8 */
    audio_stereo = (l_channels ==2) ? TRUE : FALSE;

    /* open WAVE file and set position to the 1.st audio databyte */
    l_fp_inwav = gap_audio_wav_open_seek_data(gpp->val.audioname1);
  }


  /* Calculations for encoding the sound into the avi
   */

  /* ORIGINAL CODE:
   *  frames_per_second_x100 = (norm == 0) ? 2500 : 2997;
   *  audio_per_frame_x100 = (100 * 100 * l_sample_rate * audio_size / 8 *
   *                        ((audio_stereo == TRUE) ? 2 : 1)) / frames_per_second_x100;
   */

  frames_per_second_x100f = (100.0 * gpp->val.framerate) + 0.5;
  frames_per_second_x100 = frames_per_second_x100f;
  audio_per_frame_x100 = (100 * 100 * l_sample_rate * l_bytes_per_sample) / MAX(1,frames_per_second_x100);

  AVI_set_video( l_avifile
               , gpp->val.vid_width
               , gpp->val.vid_height
               , gpp->val.framerate
               , epp->codec_name           /* char *compressor  one of "RGB", "JPEG"  ... */
               );
  if(l_fp_inwav)
  {
    AVI_set_audio(l_avifile
               , (int)l_channels
               , l_sample_rate
               , (int)l_bits
               , (int)WAVE_FORMAT_PCM    /* format 1 == pcm */
               , (long)0                 /* mp3rate (WHAT else shall i use if NO MP3 is supplied ???) */
               );
  }

  if(gap_debug) printf("next is INIT Encoder Instance ?\n");

#ifdef ENABLE_LIBXVIDCORE
  if ((strcmp(epp->codec_name, GAP_AVI_CODEC_NONE) != 0)
  && (strcmp(epp->codec_name, GAP_AVI_CODEC_JPEG) != 0))
  {
    if(gap_debug) printf("INIT Encoder Instance (HANDLE) for XVID (OpenDivX)\n");
    xvid_control = gap_gve_xvid_init( gpp->val.vid_width
                               , gpp->val.vid_height
                               , gpp->val.framerate
                               , &epp->xvid
                               );
    if (xvid_control == NULL)
    {
      printf("ERROR creation of XVID encoder instance FAILED\n");
      l_rc = -1;
    }
    if(gap_debug)  printf("creation of XVID encoder instance DONE OK\n");
  }
#endif

  /* special setup (makes it possible to code sequences backwards)
   * (NOTE: Audio is NEVER played backwards)
   */
  if(gpp->val.range_from > gpp->val.range_to)
  {
    l_step  = -1;     /* operate in descending (reverse) order */
    l_percentage_step = 1.0 / ((1.0 + gpp->val.range_from) - gpp->val.range_to);

  }
  else
  {
    l_step  = 1;      /* operate in ascending order */
    l_percentage_step = 1.0 / ((1.0 + gpp->val.range_to) - gpp->val.range_from);
  }
  l_begin = gpp->val.range_from;
  l_end   = gpp->val.range_to;

  l_cur_frame_nr = l_begin;
  while(l_rc >= 0)
  {
    if(l_cur_frame_nr == l_begin) /* setup things first if this is the first frame */
    {
      /* (light) check if dont_recode_frames is possible */
      if (epp->dont_recode_frames)
      {
        /* DONT_RECODE works only if:
         *  - input is a series of JPEG frames all encoded with YUV 4:2:2
         *  - framesize is same as videosize
         *  - codec_name is "JPEG"
         */
        if(gpp->val.storyboard_file)
        {
          if(*gpp->val.storyboard_file != '\0')
          {
            /* storyboard returns image_id of a composite image. We MUST recode here !
             */
            epp->dont_recode_frames = FALSE;
          }
        }

        if (strcmp(epp->codec_name, GAP_AVI_CODEC_JPEG) != 0)
        {
           /* must recode the frame other codecs than JPEG */
           epp->dont_recode_frames = FALSE;
        }

        if ((strcmp(gpp->ainfo.extension, ".jpg")  != 0)
        &&  (strcmp(gpp->ainfo.extension, ".jpeg") != 0)
        &&  (strcmp(gpp->ainfo.extension, ".JPG")  != 0)
        &&  (strcmp(gpp->ainfo.extension, ".JPEG") != 0))
        {
             /* we MUST recode if frame is no JPG,
              * (if image does not fit in size or is not JPG or is not YUV 4:2:2
              *  there wold be the need of recoding too
              *  but this code does just a 'light' check for extensions)
              */
              epp->dont_recode_frames = FALSE;
        }

      }
    }     /* end setup of 1.st frame (l_cur_frame_nr == l_begin) */


    /* must fetch the frame into gimp_image if we have to recode */
    if (!epp->dont_recode_frames)
    {
      /* load the current frame image, and transform (flatten, convert to RGB, scale, macro, etc..) */
      l_tmp_image_id = gap_gve_story_fetch_composite_image(l_vidhand
                                             , l_cur_frame_nr
                                             , (gint32)  gpp->val.vid_width
                                             , (gint32)  gpp->val.vid_height
                                             , gpp->val.filtermacro_file
                                             , &l_layer_id           /* output */
                                             );
      if(l_tmp_image_id < 0)
      {
         l_rc = -1;
      }
    }

    /* this block is done foreach handled video frame */
    if(l_rc == 0)
    {
      /* encode one VIDEO FRAME */
      if (epp->dont_recode_frames)
      {
        char *l_frame_filename;


        if(gap_debug) printf("DONT_RECODE_FRAMES packing input frame 1:1 into AVI\n");

        /* the DONT_RECODE_FRAMES option is fast,
         * - but works only if input is JPEG with size is same as videosize
         * - of course there is no support for filtermacros and storyboard stuff
         *   because the processing of gap_gve_story_fetch_composite_image is passed by in that case!
         */
        /* build the frame name */
        /* Use the gap functions to generate the frame filename */
        l_frame_filename = gap_lib_alloc_fname(gpp->ainfo.basename,
                                        l_cur_frame_nr,
                                        gpp->ainfo.extension);
        /* can't find the frame ? */
        if(l_frame_filename == NULL)
        {
          l_rc = -1;
        }
        else
        {
          l_FRAME_size = gap_file_get_filesize(l_frame_filename);
          buffer = gap_file_load_file(l_frame_filename);
          if (buffer == NULL)
          {
             printf("gap_avi: Failed opening encoded input frame %s.",
                                       l_frame_filename);
             l_rc = -1;
          }
          else
          {
            if (gap_debug) printf("gap_avi: Writing frame nr. %ld, size %d\n", l_cur_frame_nr, l_FRAME_size);

            AVI_write_frame(l_avifile, buffer, l_FRAME_size, TRUE /* all frames are keyframe for JPEG codec */);
          }
        }
        if(l_frame_filename)
        {
          g_free(l_frame_filename);
        }
      }
      else
      {
        gint32 l_nn;
        int    l_keyframe;
        guchar *l_app0_buffer;
        gint32  l_app0_len;

        l_keyframe = TRUE;  /* TRUE: keyframe is independent image (I frame or uncompressed)
                             * FALSE: for dependent frames (P and B frames)
                             */
        l_drawable = gimp_drawable_get (l_layer_id);
        if (gap_debug) printf("DEBUG: %s encoding frame %d\n", epp->codec_name, (int)l_cur_frame_nr);

        l_app0_buffer = NULL;
        l_app0_len = 0;

        if(epp->APP0_marker)
        {
          l_app0_len = 14;
          l_app0_buffer = databuffer;  /* abusing databuffer for this */
          /* ) */
          for (l_nn = 0; l_nn < 14; l_nn++) l_app0_buffer[l_nn] = 0; /* abusing databuffer for this */
          l_app0_buffer[0] = 'A';
          l_app0_buffer[1] = 'V';
          l_app0_buffer[2] = 'I';
          l_app0_buffer[3] = '1';

          if (epp->jpeg_interlaced)
          {
            l_app0_buffer[4] = (epp->jpeg_odd_even) ? 2 : 1;
          }
        }

        if (strcmp(epp->codec_name, GAP_AVI_CODEC_JPEG) == 0)
        {
          /* Compress the picture into a JPEG */
          buffer = gap_gve_jpeg_drawable_encode_jpeg(l_drawable, epp->jpeg_interlaced,
                                        &l_FRAME_size, epp->jpeg_quality, epp->jpeg_odd_even, FALSE, l_app0_buffer, l_app0_len);
        }
        else
        {
          if (strcmp(epp->codec_name, GAP_AVI_CODEC_NONE) == 0)
          {
            gboolean l_vflip;

            /* fill buffer with raw 24bit data
             * AVI uses opposite vertical row order than gimp, vertical flip
             */
            l_vflip = TRUE;
            buffer = gap_gve_raw_BGR_drawable_encode(l_drawable, &l_FRAME_size, l_vflip, l_app0_buffer, l_app0_len);
          }
#ifdef ENABLE_LIBXVIDCORE
          else
          {
            /* Compress the picture into MPEG4 (XVID)  */
            buffer = gap_gve_xvid_drawable_encode(l_drawable, &l_FRAME_size, xvid_control, &l_keyframe, l_app0_buffer, l_app0_len);
          }
#endif
        }


        if(buffer)
        {
          /* store the compressed video frame */
          if (gap_debug) printf("GAP_AVI: Writing frame nr. %d, size %d\n",
                        	(int)l_cur_frame_nr, (int)l_FRAME_size);
          AVI_write_frame(l_avifile, buffer, l_FRAME_size, l_keyframe);
          /* free the (un)compressed Frame data buffer */
          g_free(buffer);
        }
	else
	{
	  /* the CODEC delivered a NULL buffer
	   * there is something essential wrong (TERMINATE)
	   */
	  g_message(_("ERROR: GAP AVI encoder CODEC %s delivered empty buffer at frame %d")
	           , epp->codec_name
		   , (int)l_cur_frame_nr
		   );
	  l_rc = -1;
	}

        /* destroy the tmp image */
        gimp_image_delete(l_tmp_image_id);
      }

      /* encode AUDIO PART */
      /* As long as there is a video frame, "fill" the fake audio buffer */
      if (l_fp_inwav)
      {
        audio_filled_100 += audio_per_frame_x100;
      }

      /* Now "empty" the fake audio buffer, until it goes under the margin again */
      while ((audio_filled_100 >= (audio_margin * 100)) && (wavsize > 0))
      {
        if (gap_debug) printf("Audio processing: Audio buffer at %ld bytes.\n", audio_filled_100);
        if (wavsize >= audio_margin)
        {
            datasize = fread(databuffer, 1, audio_margin, l_fp_inwav);
            if (datasize != audio_margin)
            {
              printf("Warning: Read from wav file failed. (non-critical)\n");
            }
            wavsize -= audio_margin;
        }
        else
        {
            datasize = fread(databuffer, 1, wavsize, l_fp_inwav);
            if (datasize != wavsize)
            {
              printf("Warning: Read from wav file failed. (non-critical)\n");
            }
            wavsize = 0;
        }

        if (gap_debug) printf("Now saving audio frame datasize:%d\n", (int)datasize);

        /*XXX ?? - TODO check if write in portions or all at once */
        /* XX     - check if we should use write or append Procedure ? */

        AVI_write_audio(l_avifile, databuffer, datasize);
        /*AVI_append_audio(l_avifile, databuffer, datasize);*/

        audio_filled_100 -= audio_margin * 100;
      }
    }     /* end if l_rc == 0 */


    l_percentage += l_percentage_step;
    if(gap_debug) printf("PROGRESS: %f\n", (float) l_percentage);
    if(gpp->val.run_mode == GIMP_RUN_INTERACTIVE)
    {
      gimp_progress_update (l_percentage);
    }

    /* advance to next frame */
    if((l_cur_frame_nr == l_end) || (l_rc < 0))
    {
       break;
    }
    l_cur_frame_nr += l_step;

  }  /* end loop foreach frame */

  if(l_avifile != NULL)
  {
    AVI_close(l_avifile);
  }

  if(l_fp_inwav)
  {
    fclose(l_fp_inwav);
  }

#ifdef ENABLE_LIBXVIDCORE
  if(xvid_control)
  {
    gap_gve_xvid_cleanup(xvid_control);
    g_free(xvid_control);
  }
#endif

  return l_rc;
}    /* end p_avi_encode */