/* gap_cme_main.c
 * 2001.05.26 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This is the Master GUI Module for all GIMP-GAP compatible video encoder PlugIns
 * The Master handles
 *  - Parameter input for Video+Audio encoding,
 *  - Audio Preprocessing (convert audio input file(s) to 16bit RIFF Wave format)
 *  - selection and execution of the desired videoencoder Plug-In.
 *
 * videoencoder plug-ins are registered in the gimp-PDB like any other plug-in
 * but have special names (starting with the prefix: plug-in-gap-enc- )
 * and must provide an API (PDB-procedures) for query their name and other properties.
 * (the Master uses this API to identify all available videoencoders dynamically at runtime)
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
 * version 2.1.0b;  2004.08.08   hof: new encoder standard parameter: input_mode 
 *                                    without this param it was not clear how to encode the passed image (FRAMES or multilayer image)
 * version 2.1.0a;  2004.05.06   hof: integration into gimp-gap project
 * version 1.2.2c;  2003.01.09   hof: allow video encoding of multilayer images
 *                                    (image must be saved before the encoder call)
 * version 1.2.2b;  2002.10.28   hof: added filtermacro_file, storyboard_file
 * version 1.2.1a;  2001.05.26   hof: created
 */

#include <config.h>
#include <stdlib.h>

#include <glib/gstdio.h>

#include "gap-intl.h"

#include "gap_libgapbase.h"
#include "gap_cme_main.h"
#include "gap_cme_gui.h"

#include "gap_gve_sox.h"
#include "gap_gve_story.h"
#include "gap_libgimpgap.h"


/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;
GapCmeGlobalParams global_params;

static void query(void);
static void  run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals);  /* out-parameters */

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  query, /* query_proc */
  run,   /* run_proc */
};



/* ------------------------
 * MAIN query and run
 * ------------------------
 */

MAIN ()

/* ----------------------------------------
 * query
 * ----------------------------------------
 */
static void
query ()
{
  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  static GimpParamDef args_qt_enc[] =
  {
    {GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE,    "image", "Input image"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_STRING,   "vidfile", "filename of the output video (to write)"},
    {GIMP_PDB_INT32,    "range_from", "number of first frame"},
    {GIMP_PDB_INT32,    "range_to", "number of last frame"},
    {GIMP_PDB_INT32,    "vid_width", "Width of resulting Video Frames (all Frames are scaled to this width)"},
    {GIMP_PDB_INT32,    "vid_height", "Height of resulting Video Frames (all Frames are scaled to this height)"},
    {GIMP_PDB_INT32,    "vid_format", "videoformat:  0=comp., 1=PAL, 2=NTSC, 3=SECAM, 4=MAC, 5=unspec"},
    {GIMP_PDB_FLOAT,    "framerate", "framerate in frames per seconds"},
    {GIMP_PDB_INT32,    "samplerate", "audio samplerate in samples per seconds (.wav files are resampled using sox, if needed)"},
    {GIMP_PDB_STRING,   "audfile", "optional audiodata file .wav or any audiodata compatible to sox (see manpage of sox for more info)"},
    {GIMP_PDB_STRING,   "vid_enc_plugin", "name of a gap_video_encoder plugin choose one of these strings: \n"
                                "\"" GAP_PLUGIN_NAME_SINGLEFRAMES_ENCODE "\"\n"
                                "\"" GAP_PLUGIN_NAME_FFMPEG_ENCODE "\"\n"
                                "\"" GAP_PLUGIN_NAME_AVI_ENCODE    "\"\n"
                           /*   "\"" GAP_PLUGIN_NAME_QT1_ENCODE    "\"\n" */
                           /*   "\"" GAP_PLUGIN_NAME_QT2_ENCODE    "\"\n" */
                           /*   "\"" GAP_PLUGIN_NAME_MPG1_ENCODE   "\"\n" */
                           /*   "\"" GAP_PLUGIN_NAME_MPG2_ENCODE   "\"\n" */
                               },
    {GIMP_PDB_STRING,   "filtermacro_file", "macro to apply on each handled frame."
                                            " filtermacro_files are textfiles with filter plugin names and LASTVALUE bufferdump,"
                                            " usually created by gimp-gap filermacro dialog "
                                            " (menu: Filters->Filtermacro)"},
    {GIMP_PDB_STRING,   "storyboard_file", "textfile with list of one or more images, framesequences, videoclips or audioclips (see storyboard docs for more information)"},
    {GIMP_PDB_INT32,    "input_mode", "0 ... image is one of the frames to encode, range_from/to params refere to numberpart of the other frameimages on disc. \n"
                                      "1 ... image is multilayer, range_from/to params refere to layer index. \n"
                                      "2 ... image is ignored, input is specified by storyboard_file parameter."},
  };
  static int nargs_qt_enc = sizeof(args_qt_enc) / sizeof(args_qt_enc[0]);

  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  INIT_I18N();

  gimp_install_procedure(GAP_CME_PLUGIN_NAME_VID_ENCODE_MASTER,
                         _("This plugin is the master dialog for video + audio encoding"),
                         _("This plugin is a common GUI for all available video + audio encoding plugins"
                         " it operates on a selected range of animframes or storyboard files."
                         " The (optional) audio inputdata (param: audfile) is transformed to RIFF WAVE format (16Bit PCM)"
                         " and passed to the selected videoencoder plug-in as temporary file."
                         " (or direct if format and samplerate already matches the desired target samplerate)."
                         " The videoformat is defined with vid_enc_plugin parameter. The specified plugin "
                         " is called with the parameters specified in the dialog. for noninteractive calls"
                         " default values will be used. (you may call the desired plugin directly if you"
                         " want to specify non-interacive parameters"),
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Master Videoencoder..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_qt_enc, nreturn_vals,
                         args_qt_enc, return_vals);

  //gimp_plugin_menu_branch_register("<Image>", "Video");

  gimp_plugin_menu_register (GAP_CME_PLUGIN_NAME_VID_ENCODE_MASTER, N_("<Image>/Video/"));

}       /* end query */





/* ----------------------------------------
 * gap_cme_main_get_global_params
 * ----------------------------------------
 */
GapCmeGlobalParams *
gap_cme_main_get_global_params(void)
{
  return (&global_params);  
} /* end gap_cme_main_get_global_params */

/* ----------------------------------------
 * run
 * ----------------------------------------
 */
static void
run (const gchar *name,          /* name of plugin */
     gint n_params,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  GapCmeGlobalParams *gpp;

  static GimpParam values[1];
  gint32     l_rc;
  const char *l_env;

  gpp = gap_cme_main_get_global_params();
  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_SUCCESS;

  l_env = g_getenv("GAP_DEBUG_ENC");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  if(gap_debug) fprintf(stderr, "\n\ngap_qt_main: debug name = %s\n", name);

  gpp->val.gui_proc_thread = NULL;
  gpp->val.run_mode = param[0].data.d_int32;

  INIT_I18N();

  gap_gve_sox_init_config(&gpp->val);

  if (strcmp (name, GAP_CME_PLUGIN_NAME_VID_ENCODE_MASTER) == 0)
  {
      char *l_base;
      int   l_l;
      gboolean l_copy_parameters;

      /* get image_ID and animinfo */
      gpp->val.image_ID    = param[1].data.d_image;
      gap_gve_misc_get_ainfo(gpp->val.image_ID, &gpp->ainfo);

      /* set initial values */
      l_base = g_strdup(gpp->ainfo.basename);
      l_l = strlen(l_base);

      if (l_l > 0)
      {
         if(l_base[l_l -1] == '_')
         {
           l_base[l_l -1] = '\0';
         }
      }
      g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s.mov", l_base);
      g_free(l_base);

      gpp->val.audioname1[0] = '\0';
      gpp->val.filtermacro_file[0] = '\0';
      gpp->val.storyboard_file[0] = '\0';
      gpp->val.framerate = gpp->ainfo.framerate;
      gpp->val.range_from = gpp->ainfo.curr_frame_nr;
      gpp->val.range_to   = gpp->ainfo.last_frame_nr;
      gpp->val.samplerate = 44100;
      gpp->val.vid_width  = gimp_image_width(gpp->val.image_ID);
      gpp->val.vid_height = gimp_image_height(gpp->val.image_ID);
      gpp->val.vid_format = VID_FMT_NTSC;
      gpp->val.input_mode = GAP_RNGTYPE_FRAMES;

      l_copy_parameters = FALSE;

      /* g_snprintf(gpp->val.ecp_sel.vid_enc_plugin, sizeof(gpp->val.ecp_sel.vid_enc_plugin), "%s", GAP_PLUGIN_NAME_MPG1_ENCODE); */
      gpp->val.ecp_sel.vid_enc_plugin[0] = '\0';

      if (gpp->val.run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        /* the masterencoder is called without the master_encoder_id parameter
         * (therefore its number of arguments is GAP_VENC_NUM_STANDARD_PARAM -1)
         */
        if (n_params >= GAP_VENC_NUM_STANDARD_PARAM -1)
        {
          l_copy_parameters = TRUE;
        }
        else
        {
          values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
        }
      }
      else 
      {
        if(gpp->val.run_mode == GIMP_RUN_INTERACTIVE)
        {
          if (n_params >= GAP_VENC_NUM_STANDARD_PARAM -1)
          {
            l_copy_parameters = TRUE;
          }
          /* dont complain on less parameters at INTERCATIVE calls */ 
        }
        else
        {
           values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
        }
      }
      
      if(l_copy_parameters)
      {
           if(param[3].data.d_string)
           {
             g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s", param[3].data.d_string);
           }
           if (param[4].data.d_int32 >= 0) { gpp->val.range_from =    param[4].data.d_int32; }
           if (param[5].data.d_int32 >= 0) { gpp->val.range_to   =    param[5].data.d_int32; }
           if (param[6].data.d_int32 > 0)  { gpp->val.vid_width  =    param[6].data.d_int32; }
           if (param[7].data.d_int32 > 0)  { gpp->val.vid_height  =   param[7].data.d_int32; }
           if (param[8].data.d_int32 >= 0) { gpp->val.vid_format  =   param[8].data.d_int32; }
           gpp->val.framerate   = MAX(param[9].data.d_float, 1.0);
           gpp->val.samplerate  = param[10].data.d_int32;
           if(param[11].data.d_string) 
           { 
             g_snprintf(gpp->val.audioname1, sizeof(gpp->val.audioname1), "%s", param[11].data.d_string);
           }
           if(param[12].data.d_string)
           {
             g_snprintf(gpp->val.ecp_sel.vid_enc_plugin, sizeof(gpp->val.ecp_sel.vid_enc_plugin), "%s", param[12].data.d_string);
           }
           if(param[13].data.d_string)
           {
             g_snprintf(gpp->val.filtermacro_file, sizeof(gpp->val.filtermacro_file), "%s", param[13].data.d_string);
           }
           if(param[14].data.d_string)
           {
             g_snprintf(gpp->val.storyboard_file, sizeof(gpp->val.storyboard_file), "%s", param[14].data.d_string);
           }
           
           if (param[15].data.d_int32 >= 0) { gpp->val.input_mode  =   param[15].data.d_int32; }
      }

      if (values[0].data.d_status == GIMP_PDB_SUCCESS)
      {
         if(gpp->val.run_mode == GIMP_RUN_INTERACTIVE)
         {
            if(gap_debug)
            {
               printf("MAIN before gap_cme_gui_master_encoder_dialog ------------------\n");
            }
            
            /* note that the dialog alrady performs the encoding (as worker thread) */
            l_rc = gap_cme_gui_master_encoder_dialog (gpp);
            
            
            if(gap_debug)
            {
              printf("MAIN after gap_cme_gui_master_encoder_dialog ------------------\n");
            }
            if(l_rc < 0)
            {
              values[0].data.d_status = GIMP_PDB_CANCEL;
            }
            
         }
         else
         {
            /* gap_gve_sox_chk_and_resample was already called
             * in the interactive dialog ( at the OK button checks.)
             * but in noninteractive modes we have to resample now.
             */
            l_rc =   gap_gve_sox_chk_and_resample(&gpp->val);
            if (l_rc >= 0 )
            {
               l_rc = gap_cme_gui_start_video_encoder(gpp);
            }
         }

         if(gpp->val.tmp_audfile[0] != '\0')
         {
           g_remove(gpp->val.tmp_audfile);
         }

      }

      /* wait 8 seconds to give the encoder process a chance to exit 
       * (and/or accept a possible cancel request)
       */
      usleep(800000);

      if(gap_debug)
      {
        printf("MAIN cleanup ------------------\n");
      }

      /* clean up removes the communication file and the cancel request file (if there is any)
       * note that this shall happen after the encoder process has exited.
       * (otherwise the files used for communication may be created again after the
       * following cleanup
       */
      gap_gve_misc_cleanup_GapGveMasterEncoder(gpp->encStatus.master_encoder_id);
  }
  else
  {
      values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
  }


  if((l_rc < 0) && (values[0].data.d_status == GIMP_PDB_SUCCESS))
  {
    values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
  }

}  /* end run */
