/* gap_enc_rawframes_main.c
 *  by hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This is the MAIN Module for a rawframes video extract encoder
 *  This encoder writes the videoframes 1:1 as single files to disc
 *  instead of encoding to one videofile. It is mainly intended for
 *  extracting I Frames 1:1 from MPEG or AVI as JPEG files
 *  where the decoding and reencoding process is skipped where possible
 *  This results in fast and lossless extract of keyframes frames.
 *  (example: AVI files recoded by the OLYMPUS SP560UZ digitalcamera
 *  ........
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * version 2.5.0;  2008.06.01   hof: created
 */

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>



#define GAP_PLUGIN_NAME_RAWFRAMES_PARAMS     "extension-gap-encpar-rawframes"

#include "gap_gvetypes.h"

#include "gap_libgapvidutil.h"
#include "gap_libgimpgap.h"


/* rawframe specific encoder params  */
typedef struct {
  gint32 dum_par;   /* unused */
} GapGveRawValues;

typedef struct GapGveRawGlobalParams {   /* nick: gpp */
  GapGveCommonValues   val;
  GapGveEncAInfo       ainfo;
  GapGveEncList        *ecp;

  GapGveRawValues   evl;

} GapGveRawGlobalParams;


void   p_rawframe_init_default_params(GapGveRawValues *epp);
gint   p_rawframe_encode(GapGveRawGlobalParams *gpp);
gint   p_rawframe_encode_dialog(GapGveRawGlobalParams *gpp);
gchar* p_build_format_from_framename(gchar *framename);


/* Includes for extra LIBS */


#define GAP_PLUGIN_NAME_rawframeS_ENCODE     "plug-in-gap-enc-rawframes"

/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;
GapGveRawGlobalParams global_params;
int global_nargs_raw_enc_par;

static void query(void);
static void
run (const gchar *name,           /* name of plugin */
     gint n_params,               /* number of in-paramters */
     const GimpParam * param,     /* in-parameters */
     gint *nreturn_vals,          /* number of out-parameters */
     GimpParam ** return_vals);   /* out-parameters */
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
  static GimpParamDef args_raw_enc[] =
  {
    {GIMP_PDB_INT32, "run_mode", "non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_STRING, "videofile", "framename of the 1.st sinle framefile to write"},
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
  static int nargs_raw_enc = sizeof(args_raw_enc) / sizeof(args_raw_enc[0]);

  /* video encoder specific parameters */
  static GimpParamDef args_raw_enc_par[] =
  {
    {GIMP_PDB_INT32, "run_mode", "interactive, non-interactive"},
    {GIMP_PDB_STRING, "key_stdpar", "key to get standard video encoder params via gimp_get_data"},
    {GIMP_PDB_INT32,  "dum_par", "Dummy parameter (unused)"},
  };
  static int nargs_raw_enc_par = sizeof(args_raw_enc_par) / sizeof(args_raw_enc_par[0]);

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

  global_nargs_raw_enc_par = nargs_raw_enc_par;

  gimp_install_procedure(GAP_PLUGIN_NAME_rawframeS_ENCODE,
                         _("rawframes video encoding for anim frames. Menu: @rawframeS@"),
                         _("This plugin has a video encoder API"
                         " but writes a series of single raw frames instead of one videofile."
                         " the filetype of the output frames is derived from the extension."
                         " the extension is the suffix part of the parameter \"videofile\"."
                         " the names of the output frame(s) are same as the parameter \"videofile\""
                         " but the framenumber part is replaced by the current framenumber"
                         " (or added automatic if videofile has no number part)"
                         " audiodata is ignored."
                         " WARNINGS: for proper operation, the handled frames shall refer to single"
                         " video file without any transitions. this allows fetching frames"
                         " as raw data chunks. The chunks are 1:1 written to disc as framefiles."
                         " The resulting raw data frames on disc may be unusable if the raw chunk data"
                         " is not compatible to any image fileformat."
                         " MPEG I frames, and MJPG files may be extractable to the JPEG fileformat."
                         " A  call of"
                         "\"" GAP_PLUGIN_NAME_RAWFRAMES_PARAMS  "\""
                         " to set encoder specific parameters, is not required because"
                         " this release of the rawframe encoder has NO encoder specific parameters"),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                      /* has no Menu entry, just a PDB interface */
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_raw_enc, nreturn_vals,
                         args_raw_enc, return_vals);



  gimp_install_procedure(GAP_PLUGIN_NAME_RAWFRAMES_PARAMS,
                         _("Set parameters for GAP rawframes video encoder Plugins"),
                         _("This plugin sets rawframes specific video encoding parameters."),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,                      /* has no Menu entry, just a PDB interface */
                         NULL,
                         GIMP_PLUGIN,
                         nargs_raw_enc_par, nreturn_vals,
                         args_raw_enc_par, return_vals);


  l_ecp_key = g_strdup_printf("%s%s", GAP_QUERY_PREFIX_VIDEO_ENCODERS, GAP_PLUGIN_NAME_rawframeS_ENCODE);
  gimp_install_procedure(l_ecp_key,
                         _("Get GUI parameters for GAP rawframes video encoder"),
                         _("This plugin returns rawframes encoder specific parameters."),
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
     gint n_params,              /* number of in-parameters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  GapGveRawValues *epp;
  GapGveRawGlobalParams *gpp;

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

  l_ecp_key1 = g_strdup_printf("%s%s", GAP_QUERY_PREFIX_VIDEO_ENCODERS, GAP_PLUGIN_NAME_rawframeS_ENCODE);
  l_encoder_key = g_strdup(GAP_PLUGIN_NAME_rawframeS_ENCODE);


  p_rawframe_init_default_params(epp);

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
        values[1].data.d_string = g_strdup("rawframeS");
      }
      else if (strcmp (param_name, GAP_VENC_PAR_VID_EXTENSION) == 0)
      {
        values[1].data.d_string = g_strdup(".jpg");
      }
      else if (strcmp (param_name, GAP_VENC_PAR_SHORT_DESCRIPTION) == 0)
      {
        values[1].data.d_string =
          g_strdup(_("The rawframes Encoder\n"
                     "writes single frames instead of one videofile\n"
                     "the fileformat of the frames is derived from the\n"
                     "extension of the video name, frames are named\n"
                     "video name plus 6-digit number + extension"
                     )
                  );
      }
      else if (strcmp (param_name, GAP_VENC_PAR_GUI_PROC) == 0)
      {
        //values[1].data.d_string = g_strdup(GAP_PLUGIN_NAME_RAWFRAMES_PARAMS);
        /* the rawframes encoder has no encoder specific parameters.
         * deliver empty string for the parameter GUI procedure
         * in this case the master video encoder disables the button for
         * invoking encoder specific parameter dialog.
         * The currently implemented dialog is just a dummy popup
         */
        values[1].data.d_string = g_strdup("\0");
      }
      else
      {
        values[1].data.d_string = g_strdup("\0");
      }
  }
  else if (strcmp (name, GAP_PLUGIN_NAME_RAWFRAMES_PARAMS) == 0)
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
        if (n_params != global_nargs_raw_enc_par)
        {
          values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
          l_set_it = FALSE;
        }
        else
        {
           epp->dum_par = param[2+1].data.d_int32;
        }
      }
      else
      {
        /* try to read encoder specific params */
        p_gimp_get_data(l_encoder_key, epp, sizeof(GapGveRawValues));

        if(0 != p_rawframe_encode_dialog(gpp))
        {
          l_set_it = FALSE;
        }
      }

      if(l_set_it)
      {
         if(gap_debug) printf("Setting Encoder specific Params\n");
         gimp_set_data(l_encoder_key, epp, sizeof(GapGveRawValues));
      }

  }
  else   if (strcmp (name, GAP_PLUGIN_NAME_rawframeS_ENCODE) == 0)
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

        /* use rawframes specific encoder parameters (0==run with default values) */
        if (param[12].data.d_int32 == 0)
        {
          if(gap_debug) printf("Running the Encoder %s with Default Values\n", name);
        }
        else
        {
          /* try to read encoder specific params */
          p_gimp_get_data(name, epp, sizeof(GapGveRawValues));
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
            l_rc = p_rawframe_encode(gpp);
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


/* --------------------------------
 * p_rawframe_init_default_params
 * --------------------------------
 */
void
p_rawframe_init_default_params(GapGveRawValues *epp)
{
  if(gap_debug) printf("p_rawframe_init_default_params\n");

  epp->dum_par = 0;
}  /* end p_rawframe_init_default_params */


/* --------------------------------
 * p_rawframe_encode_dialog
 * --------------------------------
 */
gint
p_rawframe_encode_dialog(GapGveRawGlobalParams *gpp)
{
  gap_arr_msg_popup(GIMP_RUN_INTERACTIVE, _("the rawframe encoder has no encoder specific parameters"));
  return 0;
}  /* end p_rawframe_encode_dialog */


/* ----------------------------------------------------
 * p_build_format_from_framename
 * ----------------------------------------------------
 *   IN:   framename_0001.jpg
 *   OUT:  framename_%06d.jpg
 */
gchar *
p_build_format_from_framename(gchar *framename)
{
  gchar *l_fmt;
  gchar *l_fmtnum;
  gchar *l_framename;
  gint   l_idx;
  gint   l_len;
  gchar *l_ext_ptr;
  gchar *l_num_ptr;
  gint   l_idx_numlen;

  l_framename = g_strdup(framename);
  l_ext_ptr = NULL;
  l_num_ptr = NULL;
  l_idx_numlen = 0;
  l_len = strlen(l_framename);

  /* findout the numberpart  and extension */
  for(l_idx=l_len-1; l_idx >= 0; l_idx--)
  {
    if (l_framename[l_idx] == '.')
    {
      l_ext_ptr = &l_framename[l_idx];
      l_idx_numlen = 0;
      l_num_ptr = NULL;
      while(l_idx >= 0)
      {
        l_idx--;
        if(g_ascii_isdigit(l_framename[l_idx]))
        {
           l_idx_numlen++;
        }
        else
        {
          l_num_ptr = &l_framename[l_idx];
          break;                      /* stop if ran out of number part */
        }
      }
      break;
    }

    if(g_ascii_isdigit(l_framename[l_idx]))
    {
      if(l_num_ptr == NULL)
      {
        l_idx_numlen++;
      }
    }
    else
    {
      if (g_ascii_isdigit(l_framename[l_idx +1]) && (l_num_ptr == NULL))
      {
         l_num_ptr = &l_framename[l_idx];
      }
    }
  }

  if(l_num_ptr)
  {
    l_num_ptr++;
    *l_num_ptr = '\0';  /* set end of string marker */
  }
  if(l_ext_ptr)
  {
    *l_ext_ptr = '\0';  /* set end of string marker */
    l_ext_ptr++;
  }

  /* if(l_idx_numlen > 0) l_fmtnum = g_strdup_printf("%%0%dd", (int)l_idx_numlen);
   * else                 l_fmtnum = g_strdup("_%06d");
   */
  l_fmtnum = g_strdup("_%06d");  /* always use 6digit framenumbers */

  if(l_ext_ptr)
  {
    l_fmt = g_strdup_printf("%s%s.%s", l_framename, l_fmtnum, l_ext_ptr);
  }
  else
  {
    l_fmt = g_strdup_printf("%s%s", l_framename, l_fmtnum);
  }

  g_free(l_fmtnum);
  g_free(l_framename);

  return(l_fmt);
}   /* end p_build_format_from_framename */


/* --------------------------------
 * p_save_chunk_as_frame
 * --------------------------------
 */
gboolean
p_save_chunk_as_frame(const char *filename,  unsigned char *video_chunk, gint32 video_frame_chunk_size, gint32 header_length)
{
  FILE   *fp;
  unsigned char   *dataPtr;
  gint32  dataSize;

  dataSize = video_frame_chunk_size - header_length;
  dataPtr = video_chunk + header_length;

  fp = fopen(filename, "w");
  if (fp)
  {
    fwrite(dataPtr, dataSize, 1, fp);
    fclose(fp);
    return (TRUE);
  }

  printf("ERROR cant write to file:%s\n", filename);
  return (FALSE);
}  /* end p_save_chunk_as_frame */


gint32
p_dimSizeOfRawFrame(GapGveRawGlobalParams *gpp)
{
  gint32 sizeOfRawFrame;

  /* size of uncompressed RGBA frame + safety of 1000 bytes should be
   * more than enough
   */
  sizeOfRawFrame = 1000 + (gpp->val.vid_width * gpp->val.vid_height * 4);

  return (sizeOfRawFrame);
}  /* end p_dimSizeOfRawFrame */


gboolean
p_is_videoname_jpeg(const char *videoname)
{
  const char *ext;
  gint len;
  gint idx;


  ext = videoname;
  if (ext == NULL)
  {
    return (FALSE);
  }

  len = strlen(videoname);
  for(idx = len-1; idx >=  0; idx--)
  {
    ext = &videoname[idx];
    if (*ext == '.')
    {
      ext++;
      break;
    }
  }

  if (strcmp(ext, "jpg") == 0)  { return (TRUE); }
  if (strcmp(ext, "JPG") == 0)  { return (TRUE); }
  if (strcmp(ext, "jpeg") == 0)  { return (TRUE); }
  if (strcmp(ext, "JPEG") == 0)  { return (TRUE); }

  return (FALSE);
}


/* --------------------------------
 * p_rawframe_encode
 * --------------------------------
 */
gint
p_rawframe_encode(GapGveRawGlobalParams *gpp)
{
  static GapGveStoryVidHandle *l_vidhand = NULL;
  gint32         l_tmp_image_id;
  gint32         l_layer_id;
  long           l_cur_frame_nr;
  long           l_step, l_begin, l_end;
  gdouble        l_percentage, l_percentage_step;
  int            l_rc;
  unsigned char *l_video_chunk_ptr;
  gint32         l_maxSizeOfRawFrame;
  gint32         l_max_master_frame_nr;
  gint32         l_cnt_encoded_frames;
  gint32         l_cnt_reused_frames;
  gint32         l_check_flags;
  gchar         *l_frame_fmt;      /* format string has one %d for the framenumber */
  gint32         l_out_frame_nr;
  GimpRunMode    l_save_runmode;
  GapGveMasterEncoderStatus encStatus;

  if(gap_debug)
  {
     printf("p_rawframe_encode: START\n");
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

  if(p_is_videoname_jpeg(gpp->val.videoname) == TRUE)
  {
    l_check_flags += GAP_VID_CHCHK_FLAG_JPG;
    if(gap_debug)
    {
      printf("check fetched chunks for JPEG frames activated\n");
    }
  }

  l_frame_fmt = p_build_format_from_framename(gpp->val.videoname);

  if(gap_debug) printf("rawframes will be saved with filename: %s\n", l_frame_fmt);


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

  /* here we could open the video file for write */
  /*   gpp->val.videoname */

  l_percentage = 0.0;
  if(gpp->val.run_mode == GIMP_RUN_INTERACTIVE)
  {
    gap_gve_misc_initGapGveMasterEncoderStatus(&encStatus
       , gpp->val.master_encoder_id
       , abs(gpp->val.range_to - gpp->val.range_from) + 1   /* total_frames */
       );
  }



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

  l_cur_frame_nr = l_begin;
  l_save_runmode  = GIMP_RUN_INTERACTIVE;
  while(l_rc >= 0)
  {
    gboolean l_fetch_ok;
    gboolean l_force_keyframe;
    gint32   l_video_frame_chunk_size;
    gint32   l_video_frame_chunk_hdr_size;

    l_out_frame_nr++;


    l_fetch_ok = gap_story_render_fetch_composite_image_or_chunk(l_vidhand
                                           , l_cur_frame_nr
                                           , (gint32)  gpp->val.vid_width
                                           , (gint32)  gpp->val.vid_height
                                           , gpp->val.filtermacro_file
                                           , &l_layer_id           /* output */
                                           , &l_tmp_image_id       /* output */
                                           , TRUE                  /* dont_recode_flag */
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
      printf("ERROR: fetching of frame: %d FAILED, terminating\n", (int)l_cur_frame_nr);
      return -1;
    }
    else
    {
      gchar    *l_sav_name;

      l_sav_name = g_strdup_printf(l_frame_fmt, (int)l_out_frame_nr);

      if (l_video_frame_chunk_size > 0)
      {
        gboolean l_saveOk;

        l_cnt_reused_frames++;
        if (gap_debug)
        {
          printf("DEBUG: 1:1 copy of frame %d (fetch as chunk OK) chunk_ptr:%ld  chunk_size:%d chunk_hdr_size:%d\n"
              , (int)l_cur_frame_nr
              , (long)l_video_chunk_ptr
              , (int)l_video_frame_chunk_size
              , (int)l_video_frame_chunk_hdr_size
              );
        }

        /* dont recode, just write video chunk to output frame file */
        l_saveOk = p_save_chunk_as_frame(l_sav_name, l_video_chunk_ptr, l_video_frame_chunk_size, l_video_frame_chunk_hdr_size);
        if (!l_saveOk)
        {
          return -1;
        }
      }
      else
      {
        l_cnt_encoded_frames++;
        if (gap_debug)
        {
          printf("DEBUG: saving recoded frame %d (fetch as chunk FAILED)\n", (int)l_cur_frame_nr);
        }

        if(gpp->val.run_mode == GIMP_RUN_INTERACTIVE)
        {
          char *l_msg;

          l_msg = g_strdup_printf(_("SAVING: %s\n"), l_sav_name);
          gimp_progress_init(l_msg);
          g_free(l_msg);
        }

        {
          gint32 l_sav_rc;

          if(gap_debug)
          {
            printf("rawframe mode:%d, image_id:%d save: %s l_sav_name\n"
               ,(int)l_save_runmode
               ,(int)l_tmp_image_id
               ,l_sav_name
               );
          }
          l_sav_rc = gap_lib_save_named_image(l_tmp_image_id, l_sav_name, l_save_runmode);
          if(l_sav_rc < 0)
          {
            g_message(_("** Save FAILED on file\n%s"), l_sav_name);
            l_rc = -1;
          }
        }
        l_save_runmode  = GIMP_RUN_WITH_LAST_VALS;
        if(l_tmp_image_id < 0)
        {
          return -1;
        }

      }
      g_free(l_sav_name);
    }


    if(l_tmp_image_id >= 0)
    {
      /* destroy the tmp image */
      gap_image_delete_immediate(l_tmp_image_id);
    }

    l_percentage += l_percentage_step;
    if(gap_debug) printf("PROGRESS: %f\n", (float) l_percentage);
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

  }

  g_free(l_frame_fmt);

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

  return l_rc;
}  /* end p_rawframe_encode */

