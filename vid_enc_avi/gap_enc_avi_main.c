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
 * <http://www.gnu.org/licenses/>.
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





/* GAP includes */
#include "gap_gvetypes.h"


#include "gap_libgapbase.h"
#include "gap_libgimpgap.h"
#include "gap_audio_wav.h"
#include "gap_arr_dialog.h"    /* for the GUI provisorium */

#include "gap_gve_story.h"     /* for STORYBOARD support */

#include "gap_gve_jpeg.h"      /* for the builtin JPEG support */
#include "gap_gve_png.h"       /* for the builtin PNG support */
#include "gap_gve_raw.h"       /* for raw CODEC support */
#include "gap_gve_xvid.h"      /* for XVID CODEC support */
#include "gap_enc_avi_main.h"
#include "gap_enc_avi_gui.h"



static gint p_avi_encode(GapGveAviGlobalParams *gpp);


/* Includes for extra LIBS */
#include "avilib.h"


/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;
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
    {GIMP_PDB_INT32, "master_encoder_id", "id of the master encoder that called this plug-in (typically the pid)"},
  };
  static int nargs_avi_enc = sizeof(args_avi_enc) / sizeof(args_avi_enc[0]);

  /* video encoder specific parameters */
  static GimpParamDef args_avi_enc_par[] =
  {
    {GIMP_PDB_INT32, "run_mode", "interactive, non-interactive"},
    {GIMP_PDB_STRING, "key_stdpar", "key to get standard video encoder params via gimp_get_data"},
    {GIMP_PDB_STRING, "codec_name", "identifier of the codec. one of the strings \"JPEG\", \"RGB\", \"DIVX\" "},
    {GIMP_PDB_INT32, "jpeg_dont_recode_frames", "=1: store the frames _directly_ into the AVI where possible. "
                                 "(works only for codec_name MJPG or JPEG and input frames must be 4:2:2 JPEG !)"},
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
                                 "( The APP0 marker is evaluated by some Windows programs for AVIs)"},

    {GIMP_PDB_INT32, "raw_vflip", "=1: flip vertically (only for codec_name RAW and RGB )"},

    {GIMP_PDB_INT32, "png_dont_recode_frames", "=1: store the frames _directly_ into the AVI where possible. "
                                 "(works only for codec_name PNG )"},
    {GIMP_PDB_INT32, "png_interlaced", "=1: interlaced png frames, 0= no interlace"},
    {GIMP_PDB_INT32, "png_compression", "the compression of the coded pngs (0 - 9) where 9 is best and 0 is fast "},
    {GIMP_PDB_INT32, "raw_bgr", "=1: bgr else rgb colormodel (only for codec_name RAW and RGB )"}
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
                         _("This plugin handles video encoding for the AVI videoformat."
                         " the (optional) audiodata must be a raw datafile(s) or .wav (RIFF WAVEfmt ) file(s)"
                         " .wav files can be mono (1) or stereo (2channels) audiodata must be 16bit uncompressed."
                         " IMPORTANT:  you should first call "
                         "\"" GAP_PLUGIN_NAME_AVI_PARAMS  "\""
                         " to set encoder specific paramters, then set the use_rest parameter to 1 to use them."),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                      /* has no Menu entry, just a PDB interface */
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_avi_enc, nreturn_vals,
                         args_avi_enc, return_vals);



  gimp_install_procedure(GAP_PLUGIN_NAME_AVI_PARAMS,
                         _("Set parameters for GAP avi video encoder Plugins"),
                         _("This plugin sets avi specific video encoding parameters."),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                      /* has no Menu entry, just a PDB interface */
                         NULL,
                         GIMP_PLUGIN,
                         nargs_avi_enc_par, nreturn_vals,
                         args_avi_enc_par, return_vals);


  l_ecp_key = g_strdup_printf("%s%s", GAP_QUERY_PREFIX_VIDEO_ENCODERS, GAP_PLUGIN_NAME_AVI_ENCODE);
  gimp_install_procedure(l_ecp_key,
                         _("Get GUI parameters for GAP avi video encoder"),
                         _("This plugin returns avi encoder specific parameters."),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
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

  static GimpParam values[2];
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
           epp->jpeg_dont_recode_frames = param[l_ii++].data.d_int32;
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

           epp->raw_vflip                      = param[l_ii++].data.d_int32;

           epp->png_dont_recode_frames = param[l_ii++].data.d_int32;
           epp->png_interlaced    = param[l_ii++].data.d_int32;
           epp->png_compression   = param[l_ii++].data.d_int32;
           epp->raw_bgr           = param[l_ii++].data.d_int32;

        }
      }
      else
      {
        /* try to read encoder specific params */
        p_gimp_get_data(l_encoder_key, epp, sizeof(GapGveAviValues));

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
        
        gpp->val.master_encoder_id = param[16].data.d_int32;
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

  g_snprintf(epp->codec_name, sizeof(epp->codec_name), GAP_AVI_CODEC_MJPG);
  epp->jpeg_dont_recode_frames = 0;
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

  epp->raw_vflip    = 1;
  epp->raw_bgr    = 1;
}  /* end gap_enc_avi_main_init_default_params */


gint32
p_dimSizeOfRawFrame(GapGveAviGlobalParams *gpp)
{
  gint32 sizeOfRawFrame;

  /* size of uncompressed RGBA frame + safety of 1000 bytes should be
   * more than enough
   */
  sizeOfRawFrame = 1000 + (gpp->val.vid_width * gpp->val.vid_height * 4);
  
  return (sizeOfRawFrame);
}  /* end p_dimSizeOfRawFrame */


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
#define AUDIO_CHUNK_ADVANCE_FRAMES 16
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
  gint32 audio_size = 0;
  gint32 audio_stereo = 0;
  long  l_sample_rate = 22050;
  long  l_channels = 2;
  long  l_bytes_per_sample = 4;
  long  l_bits = 16;
  long  l_samples = 0;

  guchar *buffer; /* Holding misc. file contents */
  unsigned char *l_video_chunk_ptr;
  gint32         l_maxSizeOfRawFrame;
  gint32         l_max_master_frame_nr;
  gint32         l_cnt_encoded_frames;
  gint32         l_cnt_reused_frames;
  gint32         l_check_flags;
  gint32         l_out_frame_nr;
  
  gdouble        audio_samples_per_frame;
  gint32         audio_samples_per_frame_gint32;
  gint32         audio_bytes_per_frame_gint32;
  gint32         audio_bytes_done_in_advance = 0;
  gint32         l_frames_to_go;

  gint32 wavsize = 0; /* Data size of the wav file */
  long audio_margin = 8192; /* The audio chunk size */


  char databuffer[300000]; /* For transferring audio data */
  gint32   l_video_frame_chunk_size;
  gint32   l_video_frame_chunk_hdr_size;
  gboolean l_dont_recode_frames;
  GapGveMasterEncoderStatus encStatus;

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

     printf("  codec_name:%s:\n", epp->codec_name);
     printf("  master_encoder_id:%d:\n", gpp->val.master_encoder_id);
  }

  l_maxSizeOfRawFrame = p_dimSizeOfRawFrame(gpp);
  l_video_chunk_ptr = g_malloc0(l_maxSizeOfRawFrame);

  l_out_frame_nr = 0;
  l_rc = 0;
  l_layer_id = -1;
  l_cnt_encoded_frames = 0;
  l_cnt_reused_frames = 0;
  l_tmp_image_id = -1;
  l_check_flags = GAP_VID_CHCHK_FLAG_SIZE;
  l_video_frame_chunk_size = 0;
  l_video_frame_chunk_hdr_size = 0;
  l_dont_recode_frames = FALSE;


  /* make list of frameranges */
  {
    gint32 l_total_framecount;
    l_vidhand = gap_gve_story_open_vid_handle (gpp->val.input_mode
                                         ,gpp->val.image_ID
                                         ,gpp->val.storyboard_file
                                         ,gpp->ainfo.basename
                                         ,gpp->ainfo.extension
                                         ,gpp->val.range_from
                                         ,gpp->val.range_to
                                         ,&l_total_framecount
                                         );
    l_vidhand->do_gimp_progress = FALSE;
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
    gap_gve_misc_initGapGveMasterEncoderStatus(&encStatus
       , gpp->val.master_encoder_id
       , abs(gpp->val.range_to - gpp->val.range_from) + 1   /* total_frames */
       );
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
    
    if(gap_debug)
    {
      printf("Audiocheck results for file:%s\n", gpp->val.audioname1);
      printf("  wavsize:%d\n", (int)wavsize);
      printf("  l_samples:%d\n", (int)l_samples);
      printf("  l_bytes_per_sample:%d\n", (int)l_bytes_per_sample);
      printf("  l_channels:%d\n", (int)l_channels);
      printf("  l_sample_rate:%d\n", (int)l_sample_rate);
      printf("  audio_size (in bits):%d\n", (int)audio_size);
      printf("  audio_stereo:%d\n", (int)audio_stereo);
    }
  }


  /* Calculations for encoding the sound into the avi
   */
  audio_samples_per_frame = l_sample_rate / MAX(1.0, gpp->val.framerate);
  audio_samples_per_frame_gint32 = (audio_samples_per_frame + 0.5);
  audio_bytes_per_frame_gint32 = audio_samples_per_frame_gint32 * l_bytes_per_sample;

  if(gap_debug)
  {
    printf("  audio_samples_per_frame:%f\n", (float)audio_samples_per_frame);
    printf("  audio_samples_per_frame_gint32:%d\n", (int)audio_samples_per_frame_gint32);
    printf("  audio_bytes_per_frame_gint32:%d\n", (int)audio_bytes_per_frame_gint32);
  }


  /* build AVI 4 byte codec name
   * ("RAW" is converted to "RGB ", other codec names can be copied 1:1)
   */
  {
    gchar *codec_name;

    if(strcmp(epp->codec_name, GAP_AVI_CODEC_RAW) == 0)
    {
      codec_name = g_strdup(GAP_AVI_CODEC_RGB);
    }
    else
    {
      codec_name = g_strdup(epp->codec_name);
    }
    if(gap_debug)
    {
      printf("AVI 4byte code for codec_name: %s\n", codec_name);
    }
    AVI_set_video( l_avifile
                 , gpp->val.vid_width
                 , gpp->val.vid_height
                 , gpp->val.framerate
                 , codec_name           /* char *compressor  one of "RGB", "JPEG"  ... */
                 );

    g_free(codec_name);
  }
  if(l_fp_inwav)
  {
    AVI_set_audio(l_avifile
               , (int)l_channels
               , l_sample_rate
               , (int)l_bits
               , (int)WAVE_FORMAT_PCM    /* format 1 == pcm */
               , (long)((l_sample_rate * 8) / 1000)    /* mp3rate,
                                          * as seen seen in some AVI files)
                                          * note that some mplayer versions freezes on attempt
                                          * to playback AVI files with mp3rate 0
                                          */
               );

    //l_avifile->must_use_index = 1;
  }

  if(gap_debug)
  {
    printf("next is INIT Encoder Instance ?\n");
  }

#ifdef ENABLE_LIBXVIDCORE
  if ((strcmp(epp->codec_name, GAP_AVI_CODEC_RGB) != 0)
  && (strcmp(epp->codec_name, GAP_AVI_CODEC_RAW) != 0)
  && (strcmp(epp->codec_name, GAP_AVI_CODEC_PNG) != 0)
  && (strcmp(epp->codec_name, GAP_AVI_CODEC_MJPG) != 0)
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
    if(gap_debug)
    {
      printf("creation of XVID encoder instance DONE OK\n");
    }
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
  l_max_master_frame_nr = abs(l_end - l_begin) + 1;
  l_frames_to_go = l_max_master_frame_nr;


  l_frames_to_go = l_max_master_frame_nr;

  l_cur_frame_nr = l_begin;
  while(l_rc >= 0)
  {
    if(l_cur_frame_nr == l_begin) /* setup things first if this is the first frame */
    {
      /* jpeg/mjpeg codec specific check flags setup */
      if (epp->jpeg_dont_recode_frames)
      {
        if ((strcmp(epp->codec_name, GAP_AVI_CODEC_JPEG) == 0)
        ||  (strcmp(epp->codec_name, GAP_AVI_CODEC_MJPG) == 0))
        {
           l_check_flags |= (GAP_VID_CHCHK_FLAG_JPG | GAP_VID_CHCHK_FLAG_FULL_FRAME);
           l_dont_recode_frames = TRUE;
        }
      }
      /* png codec specific check flags setup */
      if (epp->png_dont_recode_frames)
      {
        if (strcmp(epp->codec_name, GAP_AVI_CODEC_PNG) == 0)
        {
           l_check_flags |= (GAP_VID_CHCHK_FLAG_PNG | GAP_VID_CHCHK_FLAG_FULL_FRAME);
           l_dont_recode_frames = TRUE;
        }
      }
      
      if(gap_debug)
      {
        printf("l_dont_recode_frames:%d\n", l_dont_recode_frames);
      }
    }     /* end setup of 1.st frame (l_cur_frame_nr == l_begin) */



    /* encode VIDEO PART, call the frame fetcher */
    {
      gboolean l_fetch_ok;
      gboolean l_force_keyframe;
    
      l_out_frame_nr++;


      l_fetch_ok = gap_story_render_fetch_composite_image_or_chunk(l_vidhand
                                           , l_cur_frame_nr
                                           , (gint32)  gpp->val.vid_width
                                           , (gint32)  gpp->val.vid_height
                                           , gpp->val.filtermacro_file
                                           , &l_layer_id           /* output */
                                           , &l_tmp_image_id       /* output */
                                           , l_dont_recode_frames                  /* dont_recode_flag */
                                           , NULL                  /* GapCodecNameElem *vcodec_list NULL == no checks */
                                           , &l_force_keyframe
                                           , l_video_chunk_ptr
                                           , &l_video_frame_chunk_size  /* actual chunk size (incl. header) */
                                           , l_maxSizeOfRawFrame        /* IN max size */
                                           , gpp->val.framerate
                                           , l_max_master_frame_nr
                                           , &l_video_frame_chunk_hdr_size
                                           , l_check_flags
                                           );
      if(l_fetch_ok != TRUE)
      {
         l_rc = -1;
      }
    }
    
    
    /* this block is done foreach handled video frame */
    if(l_rc == 0)
    {
      if (l_video_frame_chunk_size > 0)
      {
        /* 1:1 lossless copy one VIDEO FRAME */
        l_cnt_reused_frames++;

        if (gap_debug)
        {
          printf("DEBUG: 1:1 copy of frame %d (fetch as chunk OK) chunk_ptr:%d  chunk_size:%d chunk_hdr_size:%d\n"
              , (int)l_cur_frame_nr
              , (int)l_video_chunk_ptr
              , (int)l_video_frame_chunk_size
              , (int)l_video_frame_chunk_hdr_size
              );
        }
        l_FRAME_size = l_video_frame_chunk_size - l_video_frame_chunk_hdr_size;
        buffer = l_video_chunk_ptr + l_video_frame_chunk_hdr_size;

        AVI_write_frame(l_avifile, buffer, l_FRAME_size, TRUE /* all frames are keyframe for JPEG codec */);

      }
      else
      {
        /* encode one VIDEO FRAME */
        gint32 l_nn;
        int    l_keyframe;
        guchar *l_app0_buffer;
        gint32  l_app0_len;

        l_cnt_encoded_frames++;
        if (gap_debug)
        {
          printf("DEBUG: saving recoded frame %d (fetch as chunk FAILED)\n", (int)l_cur_frame_nr);
        }

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

        if ((strcmp(epp->codec_name, GAP_AVI_CODEC_JPEG) == 0)
        || (strcmp(epp->codec_name, GAP_AVI_CODEC_MJPG) == 0))
        {
          /* Compress the picture into a JPEG */
          buffer = gap_gve_jpeg_drawable_encode_jpeg(l_drawable, epp->jpeg_interlaced,
                                        &l_FRAME_size, epp->jpeg_quality, epp->jpeg_odd_even, FALSE, l_app0_buffer, l_app0_len);
        }
        else if (strcmp(epp->codec_name, GAP_AVI_CODEC_PNG) == 0)
        {
          /* Compress the picture into a PNG */
          buffer = gap_gve_png_drawable_encode_png(l_drawable, epp->png_interlaced,
                                        &l_FRAME_size, epp->png_compression, l_app0_buffer, l_app0_len);
        }
        else
        {
          if ((strcmp(epp->codec_name, GAP_AVI_CODEC_RAW) == 0)
          ||  (strcmp(epp->codec_name, GAP_AVI_CODEC_RGB) == 0))
          {
            gboolean l_vflip;
            gboolean l_convertToBGR;

            /* fill buffer with raw 24bit data, optional flipped or converted to BGR.
             * it seems that some AVI players (for instance the WinDVD and VLC player)
             *  require the inverse row order than gimp anf BGR colormodel
             *  other players (like gmplayer on unix) does not want vflipped images.
             */
            l_vflip = FALSE;
            if(epp->raw_vflip != 0)
            {
              l_vflip = TRUE;
            }
            l_convertToBGR = FALSE;
            if(epp->raw_bgr != 0)
            {
              l_convertToBGR = TRUE;
            }
            buffer = gap_gve_raw_RGB_or_BGR_drawable_encode(l_drawable
                     , &l_FRAME_size
                     , l_vflip
                     , l_app0_buffer
                     , l_app0_len
                     , l_convertToBGR
                     );
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
          if (gap_debug)
          {
            printf("GAP_AVI: Writing frame nr. %d, size %d  l_keyframe:%d\n"
                 , (int)l_cur_frame_nr
                 , (int)l_FRAME_size
                 , (int)l_keyframe
                 );
          }
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

        gimp_drawable_detach (l_drawable);
        /* destroy the tmp image */
        gimp_image_delete(l_tmp_image_id);
      }
      
      
    }     /* end if l_rc == 0 */






      /* encode AUDIO PART */
      /* As long as there is a video frame, write audio chunks.
       * set AUDIO_CHUNK_ADVANCE_FRAMES = 1 triggers writing of an audio chunk for each handled frame.
       * values > 1 do write one longer audio chunk in advance for duration of the defined number of frames,
       * in this case the next audio chunk is written after the number of video frames reached.
       * in case the audio input is shorter than video playtime write the rest
       * and stop writing audio for all further frame.
       * in case the audio input is longer than video playtime it is truncated.
       * (e.g. the remaining audio is not written to the resulting video file).
       */
      if ((l_fp_inwav) && (wavsize > 0))
      {
        gint32 datasize;
        gint32 l_frames_advance;
        
        
        l_frames_advance = MIN(AUDIO_CHUNK_ADVANCE_FRAMES, l_frames_to_go);
        audio_margin = MIN((audio_bytes_per_frame_gint32 * l_frames_advance), sizeof(databuffer) -1);
        
        if(gap_debug)
        {
          printf("audio_bytes_per_frame:%d  audio_margin: %d (to_go:%d advance:%d)\n"
             , (int)audio_bytes_per_frame_gint32
             , (int)audio_margin
             , (int)l_frames_to_go
             , (int)l_frames_advance
             );
        }

        datasize = 0;
        if (audio_bytes_done_in_advance <= 0)
        {
          if (wavsize >= audio_margin)
          {
              datasize = fread(databuffer, 1, audio_margin, l_fp_inwav);
              if (datasize != audio_margin)
              {
                printf("Warning: Read %d bytes from wav file failed. (got %d bytes)\n"
                      ,(int)audio_margin
                      ,(int)datasize
                      );
              }
              wavsize -= audio_margin;
          }
          else
          {
              datasize = fread(databuffer, 1, wavsize, l_fp_inwav);
              if (datasize != wavsize)
              {
                printf("Warning: Read rest of %d bytes from wav file failed. (got %d bytes)\n"
                      ,(int)wavsize
                      ,(int)datasize
                      );
              }
              wavsize = 0;
          }
        }
        if (datasize > 0)
        {
          if(gap_debug)
          {
            printf("Now saving audio frame datasize:%d\n", (int)datasize);
          }
          AVI_write_audio(l_avifile, databuffer, datasize);

          if(gap_debug)
          {
            printf("audio chunk written\n");
          }
          audio_bytes_done_in_advance = datasize;
        }
        audio_bytes_done_in_advance -= audio_bytes_per_frame_gint32;
      }




    l_percentage += l_percentage_step;
    if(gap_debug)
    {
      printf("PROGRESS: %f\n", (float) l_percentage);
    }
    if(gpp->val.run_mode == GIMP_RUN_INTERACTIVE)
    {
      encStatus.frames_processed++;
      encStatus.frames_encoded = l_cnt_encoded_frames;
      encStatus.frames_copied_lossless = l_cnt_reused_frames;

      gap_gve_misc_do_master_encoder_progress(&encStatus);
    }

    /* terminate on cancel reqeuset (CANCEL button was pressed in the master encoder dialog) */
    if(gap_gve_misc_is_master_encoder_cancel_request(&encStatus))
    {
       break;
    }

    /* advance to next frame */
    if((l_cur_frame_nr == l_end) || (l_rc < 0))
    {
       break;
    }
    l_cur_frame_nr += l_step;
    l_frames_to_go--;

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

  if(l_vidhand)
  {
    gap_gve_story_close_vid_handle(l_vidhand);
  }

  /* statistics */
  if(gap_debug)
  {
    printf("encoded       frames: %d\n", (int)l_cnt_encoded_frames);
    printf("1:1 copied    frames: %d\n", (int)l_cnt_reused_frames);
    printf("total handled frames: %d\n", (int)l_cnt_encoded_frames + l_cnt_reused_frames);
  }
  g_free(l_video_chunk_ptr);
  return l_rc;
}    /* end p_avi_encode */
