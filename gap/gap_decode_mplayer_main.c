/* gap_decode_mplayer_main.c
 * 2009.02.12 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Package
 *
 * This Module contains:
 * - MAIN of frontend for the external program mplayer
 *     that can be used to extract frames from videofiles
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/* revision history:
 * gimp    2.1.0;   2004/12/06  hof: added gap_decode_mplayer
 * gimp    1.1.29b; 2000/11/25  hof: use gap lock procedures, update e-mail adress + main version
 * gimp    1.1.11b; 1999/11/20  hof: added gap_decode_xanim, fixed typo in mpeg encoder menu path
 *                                   based on parts that were found in gap_main.c before.
 */
 
/* SYTEM (UNIX) includes */ 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* GIMP includes */
#include "gtk/gtk.h"
#include "config.h"
#include "gap-intl.h"
#include "libgimp/gimp.h"

/* GAP includes */
#include "gap_lib.h"
#include "gap_decode_mplayer.h"
#include "gap_arr_dialog.h"
#include "gap_lock.h"

/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;


static void query(void);
static void run(const gchar *name
              , gint n_params
              , const GimpParam *param
              , gint *nreturn_vals
              , GimpParam **return_vals);

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  query, /* query_proc */
  run,   /* run_proc */
};

MAIN ()

static void
query ()
{
  static GimpParamDef args_mplayer[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive"},
    {GIMP_PDB_IMAGE, "image", "(unused)"},
    {GIMP_PDB_DRAWABLE, "drawable", "(unused)"},
  };

  static GimpParamDef args_mplayer_ext[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive"},
  };


  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  gimp_install_procedure(GAP_MPLAYER_PLUGIN_NAME,
                         "This plugin calls mplayer to split any video to video frames. "
                         "MPlayer 1.0 must be installed on your system.",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("MPlayer based extraction..."),
                         NULL,
                         GIMP_PLUGIN,
                         G_N_ELEMENTS (args_mplayer), nreturn_vals,
                         args_mplayer, return_vals);

  gimp_install_procedure(GAP_MPLAYER_PLUGIN_NAME_TOOLBOX,
                         "This plugin calls mplayer to split any video to video frames. "
                         "MPlayer 1.0 must be installed on your system.",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("MPlayer based extraction..."),
                         NULL,
                         GIMP_PLUGIN,
                         G_N_ELEMENTS (args_mplayer_ext), nreturn_vals,
                         args_mplayer_ext, return_vals);


  {
    /* Menu names */
    const char *menupath_image_video_split = N_("<Image>/Video/Split Video into Frames/");
    const char *menupath_toolbox_video_split = N_("<Toolbox>/Xtns/Split Video into Frames/");

    //gimp_plugin_menu_branch_register("<Image>", "Video");
    //gimp_plugin_menu_branch_register("<Image>/Video", "Split Video into Frames");

    //gimp_plugin_menu_branch_register("<Toolbox>", "Video");
    //gimp_plugin_menu_branch_register("<Toolbox>/Video", "Split Video into Frames");

    gimp_plugin_menu_register (GAP_MPLAYER_PLUGIN_NAME, menupath_image_video_split);
    gimp_plugin_menu_register (GAP_MPLAYER_PLUGIN_NAME_TOOLBOX, menupath_toolbox_video_split);
  }

}       /* end query */



static void run(const gchar *name
              , gint n_params
              , const GimpParam *param
              , gint *nreturn_vals
              , GimpParam **return_vals)
{
  const char *l_env;
  
  static GimpParam values[2];
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
   
  gint32     l_rc;

  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  INIT_I18N ();

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  run_mode = param[0].data.d_int32;

  if(gap_debug) fprintf(stderr, "\n\ngap_main: debug name = %s\n", name);

  
  if ((strcmp (name, GAP_MPLAYER_PLUGIN_NAME) == 0)
  ||  (strcmp (name, GAP_MPLAYER_PLUGIN_NAME_TOOLBOX) == 0))
  {
      GapMPlayerParams mplayer_gpp;
      GapMPlayerParams *gpp;
      
      /* only the INTERACTIVE runmode is supported, 
       * extracting frames in batch mode can be done outside the gimp
       * (mplayer has excellent commandline support)
       */
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
          status = GIMP_PDB_CALLING_ERROR;
      }

      if (status == GIMP_PDB_SUCCESS)
      {
        gpp = &mplayer_gpp;
        
        /* setup default values (for the 1.st call per session) */
        gpp->video_filename[0] = '\0';
        gpp->audio_filename[0] = '\0';
        gpp->number_of_frames  = 100;
        gpp->vtrack            = 1;
        gpp->atrack            = 1;
        gpp->png_compression   = 0;
        gpp->jpg_quality       = 86;
        gpp->jpg_optimize      = 100;
        gpp->jpg_smooth        = 0;
        gpp->jpg_progressive   = FALSE;
        gpp->jpg_baseline      = TRUE;
        gpp->start_hour        = 0;
        gpp->start_minute      = 0;
        gpp->start_second      = 0;

        gpp->img_format        = MPENC_JPEG;
        gpp->silent            = FALSE;
        gpp->autoload          = TRUE;
        gpp->run_mplayer_asynchron = TRUE;
        
        g_snprintf(gpp->basename, sizeof(gpp->basename), "frame_");

        gimp_get_data(GAP_MPLAYER_PLUGIN_NAME, gpp);
        mplayer_gpp.run_mode = run_mode;
        
        l_rc = gap_mplayer_decode(gpp);
        
        gimp_set_data(GAP_MPLAYER_PLUGIN_NAME, gpp, sizeof(mplayer_gpp));
      }
  }

  /* ---------- return handling --------- */

  if(l_rc < 0)
  {
    status = GIMP_PDB_EXECUTION_ERROR;
  }
 
  
  if (run_mode != GIMP_RUN_NONINTERACTIVE)
  {
    gimp_displays_flush();
  }
  
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

}
