/*
 * gap_vex_main.c
 *   A GIMP / GAP Plugin to extract frames and/or audio from Videofiles
 *
 *   This is a special kind of File Load Plugin,
 *     based on gap_vid_api (GVA)  (an API to read various Videoformats/CODECS)
 *     if compiled without GAP_ENABLE_VIDEOAPI_SUPPORT this module
 *     just displays a message
 */

/*
 * Changelog:
 * 2004/04/10 v2.1.0:   integrated sourcecode into gimp-gap project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

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
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include <locale.h>

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT
/* conditional Includes for video API */
#include <gap_vid_api.h>
#endif


/* Include of sub modules */
#include "gap_vex_main.h"
#include "gap_vex_dialog.h"
#include "gap_vex_exec.h"


int gap_debug = 0;  /* 1 == print debug infos , 0 dont print debug infos */


static void   query      (void);
static void  run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals);  /* out-parameters */

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,    /* init_proc */
  NULL,    /* quit_proc */
  query,   /* query_proc */
  run,     /* run_proc */
};



MAIN ()

static void
query ()
{
  static GimpParamDef load_args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE, "image", "(unused)"},
    { GIMP_PDB_DRAWABLE, "drawable", "(unused)"},
    { GIMP_PDB_STRING, "videoename", "The name of the Videofile to read (load)" },

    { GIMP_PDB_INT32, "pos_unit", "2: begin_pos, and end_pos are percent (0.0 upto 100.0) 0: begin_pos, and end_pos are framenumbers" },


    { GIMP_PDB_FLOAT, "begin_pos", "start of the affected range (percent or framenumber)" },
    { GIMP_PDB_FLOAT, "end_pos", "end of the affected range  (percent or framenumber)" },
    { GIMP_PDB_STRING, "audiofile", "name for extracted audio output file (RIFF PCM Wave Format, should end with .wav)" },
    { GIMP_PDB_STRING, "basename", "The name for extracted ouput frames _0001.<extension> is added" },
    { GIMP_PDB_STRING, "extension", "select image save format by extension (.xcf, .ppm, .jpg, ...)" },
    { GIMP_PDB_INT32, "basenum", "number for the 1.st extracted output frame, 0: use original framenr" },
    { GIMP_PDB_INT32, "multilayer", "0: save each frame to one file, 1: load all frames into one multilayer image, 2: create storyboard with selected clip" },
    { GIMP_PDB_INT32, "extract_videotrack", "0:ignore video frames, 1 upto n: extract frames from videotrack" },
    { GIMP_PDB_INT32, "extract_audiotrack", "0:ignore audio, 1 upto n: extract audiotrack to .wav file" },
    { GIMP_PDB_INT32, "overwrite_mode", "1: overwrite all existing files, other values: cancel if files already exists" },
    { GIMP_PDB_INT32, "disable_mmx", "0: use (faster) MMX if available, 1: do not USE MMX (MMX is lossy on lowbitrate streams)" },
    { GIMP_PDB_STRING, "preferred_decoder", "NULL, or one of: libmpeg3, quicktime4linux, libavformat" },
    { GIMP_PDB_INT32, "exact_seek", "0: NO (enable faster seek ops if available), 1: YES use only sequential read ops, will find exact framenumbers" },
    { GIMP_PDB_INT32, "deinterlace", "0: NO, 1: deinterlace odd rows only, 2: deinterlace even rows only, 3: deinterlace split into 2 frames where odd rows-frame is 1st, 4: deinterlace split into 2 frames where even rows-frame is 1st)" },
    { GIMP_PDB_FLOAT, "delace_threshold", "0.0 .. no interpolation, 1.0 smooth interpolation at deinterlacing" },
    { GIMP_PDB_INT32, "fn_digits", "1 <= fn_digits <= 8, number of digits to use in framenames (use 1 if you dont want leading zeroes) " },
    { GIMP_PDB_INT32, "bluebox", "0: NO, 1: YES generate transparency by applying bluebox filter with last values" },
    { GIMP_PDB_INT32, "graymask", "0: NO (extract frames 1:1), 1: YES extract generated transparency as gray mask" },
    { GIMP_PDB_INT32, "layermask", "0: NO (bluebox shall generate aplha channel), 1: YES (bluebox shall generate layermask)" },
  };
  static GimpParamDef load_return_vals[] =
  {
    { GIMP_PDB_IMAGE, "image", "Output image" },
  };
  static int nload_args = sizeof (load_args) / sizeof (load_args[0]);
  static int nload_return_vals = sizeof (load_return_vals) / sizeof (load_return_vals[0]);

  static GimpParamDef ext_args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_STRING, "videoename", "The name of the Videofile to read (load)" },

    { GIMP_PDB_INT32, "pos_unit", "2: begin_pos, and end_pos are percent (0.0 upto 100.0) 0: begin_pos, and end_pos are framenumbers" },


    { GIMP_PDB_FLOAT, "begin_pos", "start of the affected range (percent or framenumber)" },
    { GIMP_PDB_FLOAT, "end_pos", "end of the affected range  (percent or framenumber)" },
    { GIMP_PDB_STRING, "audiofile", "name for extracted audio output file (RIFF PCM Wave Format, should end with .wav)" },
    { GIMP_PDB_STRING, "basename", "The name for extracted ouput frames _0001.<extension> is added" },
    { GIMP_PDB_STRING, "extension", "select image save format by extension (.xcf, .ppm, .jpg, ...)" },
    { GIMP_PDB_INT32, "basenum", "number for the 1.st extracted output frame, 0: use original framenr" },
    { GIMP_PDB_INT32, "multilayer", "0: save each frame to one file, 1: load all frames into one multilayer image" },
    { GIMP_PDB_INT32, "extract_videotrack", "0:ignore video frames, 1 upto n: extract frames from videotrack" },
    { GIMP_PDB_INT32, "extract_audiotrack", "0:ignore audio, 1 upto n: extract audiotrack to .wav file" },
    { GIMP_PDB_INT32, "overwrite_mode", "1: overwrite all existing files, other values: cancel if files already exists" },
    { GIMP_PDB_INT32, "disable_mmx", "0: use (faster) MMX if available, 1: do not USE MMX (MMX is lossy on lowbitrate streams)" },
    { GIMP_PDB_STRING, "preferred_decoder", "NULL, or one of: libmpeg3, quicktime4linux, libavformat" },
    { GIMP_PDB_INT32, "exact_seek", "0: NO (enable faster seek ops if available), 1: YES use only sequential read ops, will find exact framenumbers" },
    { GIMP_PDB_INT32, "deinterlace", "0: NO, 1: deinterlace odd rows only, 2: deinterlace even rows only, 3: deinterlace split into 2 frames where odd rows-frame is 1st, 4: deinterlace split into 2 frames where even rows-frame is 1st)" },
    { GIMP_PDB_FLOAT, "delace_threshold", "0.0 .. no interpolation, 1.0 smooth interpolation at deinterlacing" },
    { GIMP_PDB_INT32, "fn_digits", "1 <= fn_digits <= 8, number of digits to use in framenames (use 1 if you dont want leading zeroes) " },
    { GIMP_PDB_INT32, "bluebox", "0: NO, 1: YES generate transparency by applying bluebox filter with last values" },
    { GIMP_PDB_INT32, "graymask", "0: NO (extract frames 1:1), 1: YES extract generated transparency as gray mask" },
    { GIMP_PDB_INT32, "layermask", "0: NO (bluebox shall generate aplha channel), 1: YES (bluebox shall generate layermask)" },
  };
  static int next_args = sizeof (ext_args) / sizeof (ext_args[0]);

  /* INIT_I18N(); */

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  gimp_install_procedure (GAP_VEX_PLUG_IN_NAME,
                          "Extract frames from videofiles into animframes or multilayer image",
                          "The specified range of frames (params: begin_pos upto end_pos) is extracted from"
                          " the videofile (param: videoname) and stored as sequence of frame imagefiles"
                          " on disk and load the 1st of the extracted frames as gimp image."
                          " optional you may extract the specified range of frames into one multilayer gimp image"
                          " instead of writing the frames as diskfiles. (par: multilayer == 1)"
                          " Additional you can extract (one) stereo or mono audiotrack"
                          " to uncompressed audiofile (RIFF Wave format .WAV)"
                          " The specified range is used both for video and audio"
                          " (see gap_vid_api docs for supported Videoformats and CODECS)",
                          "Wolfgang Hofer (hof@gimp.org)",
                          "Wolfgang Hofer",
                          GAP_VERSION_WITH_DATE,
                          N_("Extract Videorange"),
                          NULL,
                          GIMP_PLUGIN,
                          nload_args, nload_return_vals,
                          load_args, load_return_vals);

  gimp_install_procedure (GAP_VEX_PLUG_IN_NAME_XTNS,
                          "Extract frames from videofiles into animframes or multilayer image",
                          "The specified range of frames (params: begin_pos upto end_pos) is extracted from"
                          " the videofile (param: videoname) and stored as sequence of frame imagefiles"
                          " on disk and load the 1st of the extracted frames as gimp image."
                          " optional you may extract the specified range of frames into one multilayer gimp image"
                          " instead of writing the frames as diskfiles. (par: multilayer == 1)"
                          " Additional you can extract (one) stereo or mono audiotrack"
                          " to uncompressed audiofile (RIFF Wave format .WAV)"
                          " The specified range is used both for video and audio"
                          " (see gap_vid_api docs for supported Videoformats and CODECS)",
                          "Wolfgang Hofer (hof@gimp.org)",
                          "Wolfgang Hofer",
                          GAP_VERSION_WITH_DATE,
                          N_("Extract Videorange"),
                          NULL,
                          GIMP_PLUGIN,
                          next_args, nload_return_vals,
                          ext_args, load_return_vals);
  {
    /* Menu names */
    const char *menupath_image_video_split = N_("<Image>/Video/Split Video into Frames/");
    const char *menupath_toolbox_video_split = N_("<Toolbox>/Xtns/Split Video into Frames/");

    //gimp_plugin_menu_branch_register("<Image>", "Video");
    //gimp_plugin_menu_branch_register("<Image>/Video", "Split Video into Frames");

    //gimp_plugin_menu_branch_register("<Toolbox>", "Video");
    //gimp_plugin_menu_branch_register("<Toolbox>/Video", "Split Video into Frames");

    gimp_plugin_menu_register (GAP_VEX_PLUG_IN_NAME, menupath_image_video_split);

    gimp_plugin_menu_register (GAP_VEX_PLUG_IN_NAME_XTNS, menupath_toolbox_video_split);
  }
}  /* end query */

static void
run (const gchar *name,          /* name of plugin */
     gint nparams,               /* number of in-paramters */
     const GimpParam * param,    /* in-parameters */
     gint *nreturn_vals,         /* number of out-parameters */
     GimpParam ** return_vals)   /* out-parameters */
{
  gint argc = 1;
  gchar **argv = g_new (gchar *, 1);
  GapVexMainGlobalParams global_pars;
  GapVexMainGlobalParams *gpp;

  static GimpParam values[2];
  int    l_par;
  int    expected_params;
  const char   *l_env;

  gpp = &global_pars;

  INIT_I18N();

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
  *nreturn_vals = 1;
  *return_vals = values;

  gpp->val.run_mode = param[0].data.d_int32;
  if(gap_debug) printf("\nRUN PROCEDURE %s RUN_MODE %d nparams:%d\n"
                      , name
                      , (int)gpp->val.run_mode
                      , (int)nparams );


  gpp->val.generate_alpha_via_bluebox = FALSE;
  gpp->val.extract_alpha_as_gray_frames = FALSE;
  gpp->val.extract_with_layermask = FALSE;

  if(strcmp(name, GAP_VEX_PLUG_IN_NAME) == 0)
  {
    l_par = 2;
  }
  else
  {
    l_par = 0;
    if(strcmp (name, GAP_VEX_PLUG_IN_NAME_XTNS) != 0)
    {
       return;  /* calling error */
    }
  }

  argv[0] = g_strdup (_("MAIN_TST"));
  gtk_set_locale ();
  setlocale (LC_NUMERIC, "C");  /* is needed when after gtk_set_locale ()
                                 * to make sure PASSING FLOAT PDB_PARAMETERS works
                                 * (thanks to sven for the tip)
                                 */
  gtk_init (&argc, &argv);


  /* init return status
   * (if this plug-in is compiled without GAP_ENABLE_VIDEOAPI_SUPPORT
   *  it will always fail)
   */
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

#ifdef GAP_ENABLE_VIDEOAPI_SUPPORT

  gap_vex_dlg_init_gpp(gpp);


  if(gpp->val.run_mode == GIMP_RUN_NONINTERACTIVE)
  {
    /* ---------- get batch parameters  ----------*/
    expected_params = l_par + 21;
    if(nparams != expected_params)
    {
       printf("Calling Error wrong number of params %d (expected: %d)\n"
              ,(int) nparams
              ,(int) expected_params);

       return;  /* calling error */
    }

    if(param[l_par + 1].data.d_string != NULL)
    {
      g_snprintf(gpp->val.videoname, sizeof(gpp->val.videoname), "%s", param[l_par + 1].data.d_string);
    }

    gpp->val.pos_unit    = param[l_par + 2].data.d_int32;

    if(gpp->val.pos_unit == 0)
    {
      gpp->val.begin_percent  = -1.0;
      gpp->val.end_percent    = -1.0;
      gpp->val.begin_frame    = param[l_par + 3].data.d_float;
      gpp->val.end_frame      = param[l_par + 4].data.d_float;
    }
    else
    {
      gpp->val.begin_percent  = param[l_par + 3].data.d_float;
      gpp->val.end_percent    = param[l_par + 4].data.d_float;
      gpp->val.begin_frame    = -1;
      gpp->val.end_frame      = -1;
    }


    if(param[l_par + 5].data.d_string != NULL)
    {
      g_snprintf(gpp->val.audiofile, sizeof(gpp->val.audiofile), "%s", param[l_par + 4].data.d_string);
    }
    if(param[l_par + 6].data.d_string != NULL)
    {
      g_snprintf(gpp->val.basename, sizeof(gpp->val.basename), "%s", param[l_par + 5].data.d_string);
    }
    if(param[l_par + 7].data.d_string != NULL)
    {
      g_snprintf(gpp->val.extension, sizeof(gpp->val.extension), "%s", param[l_par + 6].data.d_string);
    }
    gpp->val.basenum    = param[l_par + 8].data.d_int32;
    gpp->val.multilayer = param[l_par + 9].data.d_int32;
    gpp->val.videotrack = param[l_par + 10].data.d_int32;
    gpp->val.audiotrack = param[l_par + 11].data.d_int32;
    gpp->val.ow_mode    = param[l_par + 12].data.d_int32;
    if(gpp->val.ow_mode != 1)
    {
      gpp->val.ow_mode = -1;   /* cancel at 1.st existing file (dont overwrite) */
    }
    gpp->val.disable_mmx = param[l_par + 13].data.d_int32;

    if(param[l_par + 14].data.d_string != NULL)
    {
      g_snprintf(gpp->val.preferred_decoder, sizeof(gpp->val.preferred_decoder), "%s", param[l_par + 14].data.d_string);
    }

    gpp->val.exact_seek    = param[l_par + 15].data.d_int32;
    gpp->val.deinterlace   = param[l_par + 16].data.d_int32;
    gpp->val.delace_threshold   = param[l_par + 17].data.d_float;
    gpp->val.fn_digits     = param[l_par + 18].data.d_int32;

    gpp->val.generate_alpha_via_bluebox     = param[l_par + 19].data.d_int32;
    gpp->val.extract_alpha_as_gray_frames   = param[l_par + 20].data.d_int32;
    gpp->val.extract_with_layermask         = param[l_par + 21].data.d_int32;

    gpp->val.run = TRUE;
  }
  else
  {
    /* ---------- dialog ----------*/
    gap_vex_dlg_main_dialog (gpp);
  }

  /* ---------- extract  ----------*/

  if(gpp->val.run)
  {
    gap_vex_exe_extract_videorange(gpp);

    if (gpp->val.image_ID >= 0)
    {
        *nreturn_vals = 2;
        values[0].data.d_status = GIMP_PDB_SUCCESS;
        values[1].type = GIMP_PDB_IMAGE;
        values[1].data.d_image = gpp->val.image_ID;
    }
    else
    {
        values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
    }
  }

  return;

/* endif GAP_ENABLE_VIDEOAPI_SUPPORT */
#endif
  g_message(_("Videoextract is not available because "
              "GIMP-GAP was configured and compiled with\n "
              "--disable-videoapi-support")
           );
  return;

}  /* end run */

