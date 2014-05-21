/* gap_onion_main.c
 * 2001.11.20 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module handles ONION Skin Layers in the GIMP Video Menu.
 * Onion Layer(s) usually do show previous (or next) frame(s)
 * of the video in the current frame.
 *
 *   Video/OnionSkin/Configuration        ... GUI to configure, create abd delete onionskin Layer(s) for framerange
 *   Video/OnionSkin/Create or Replace    ... create or replace onionskin Layer(s) and set visible.
 *   Video/OnionSkin/Delete               ... delete onionskin Layer(s)
 *   Video/OnionSkin/Toggle Visibility    ... show/hide onionskin layer(s)
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
 * version 2.1.0a;  2004/06/03   hof: added onionskin ref_mode parameter
 * version 1.3.17a; 2003.07.29   hof: param types GimpPlugInInfo.run procedure
 * version 1.3.16c; 2003.07.12   hof: Onionsettings scope changes from gimp-session
 *                                    to permanent per animation (stored in video_info file=.
 * version 1.3.16b; 2003.07.06   hof: new parameter asc_opacity (for cross-fading support)
 * version 1.3.14a; 2003.05.24   hof: integration into gimp-gap-1.3.14
 * version 1.3.12a; 2003.05.03   hof: started port to gimp-1.3 / gtk+2.2
 * version 1.2.2a;  2001.11.20   hof: created
 */

#include <gap_onion_main.h>
#include <gap_onion_base.h>
#include <gap_onion_worker.h>
#include <gap_onion_dialog.h>


/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;
GapOnionMainGlobalParams global_params;


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

  static GimpParamDef args_onion_cfg[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (the current videoframe)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "num_olayers", "Number of Onion Layers"},
    {GIMP_PDB_INT32, "ref_delta", "Reference Frame Delta:  +- 1 ... n  Default: -1 "},
    {GIMP_PDB_INT32, "ref_cycle", "Reference is Cycle   : TRUE/FALSE  (TRUE .. last frame has frame 0 as next frame)"},
    {GIMP_PDB_INT32, "stack_pos", "Place OnionLayer(s) on Stackposition 0..n Default: 1"},
    {GIMP_PDB_INT32, "stack_top", "TRUE Stack Position is relative from TOP, FALSE relative to Bottom"},
    {GIMP_PDB_FLOAT, "opacity", "OnionOpacity: 0.0..100.0%"},
    {GIMP_PDB_FLOAT, "opacity_delta", "OnionOpacityDelta: 0..100%  Default: 80"},
    {GIMP_PDB_INT32, "ignore_botlayers", "Ignore N Bottom Sourcelayers (0 .. use full picture, 1 ignor bg layer) "},
    {GIMP_PDB_INT32, "select_mode", "Mode how to identify a layer: 0-3 by layername 0=equal, 1=prefix, 2=suffix, 3=contains, 6=all visible (ignore select_string)"},
    {GIMP_PDB_INT32, "select_case", "0: ignore case 1: select_string is case sensitive"},
    {GIMP_PDB_INT32, "select_invert", "0: select normal 1: invert (select all unselected layers)"},
    {GIMP_PDB_STRING, "select_string", "string to match with layername (how to match is defined by select_mode)"},
    {GIMP_PDB_INT32, "range_from", "first affected frame (ignored if run is not 2 or 3)"},
    {GIMP_PDB_INT32, "range_to", "last affected frame (ignored if run is not 2 or 3)"},
    {GIMP_PDB_INT32, "run", "0 .. do nothing, 1..set params for this session, 2..set and create or replace onionlayers for selected framerange  3..delete onionlayers from selected famerange "},
    {GIMP_PDB_INT32, "asc_opacity", "TRUE..farest neighbour frame has highest opacity, FALSE: nearest has highest opacity"},
    {GIMP_PDB_INT32, "auto_create", "TRUE..automatic creation/replacing of onionskinlayers after GAP controlled load"},
    {GIMP_PDB_INT32, "auto_delete", "TRUE..automatic delete of onionskinlayers before GAP controlled save"},
    {GIMP_PDB_INT32, "ref_mode", "Reference Mode:  0:NORMAL, 1:BIDIRECTIONAL_SINGLE, 2:BIDIRECTIONAL_DOUBLE "},
  };
  static int nargs_onion_cfg = G_N_ELEMENTS(args_onion_cfg);

  static GimpParamDef args_onion_visi[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (the current videoframe)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_INT32, "visible_mode", "0: set invisible 1: set visible 2: toggle visibility"},
  };
  static int nargs_onion_visi = G_N_ELEMENTS(args_onion_visi);

  static GimpParamDef args_onion_std[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image (the current videoframe)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
  };
  static int nargs_onion_std = G_N_ELEMENTS(args_onion_std);

  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;


/* ------------------------
 * MAIN query and run
 * ------------------------
 */

MAIN ()

static void
query ()
{
  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  gimp_install_procedure(GAP_PLUGIN_NAME_ONION_CFG,
                         "This plugin sets Configuration for Onion Layers in Videofames",
                         "This plugin is the configuration GUI for Onion layers."
                         " Onion Layer(s) usually do show previous and/ or next frame(s)"
                         " of the video in the current frame, depending on ref_mode parameter"
                         " Onion Layers are not created automatically. You have to create or delete them manually"
                         " using the menu Video/OnionSkin/make or Video/OnionSkin/delete or call the Procedures "
                         GAP_PLUGIN_NAME_ONION_APPLY " "
                         GAP_PLUGIN_NAME_ONION_DEL " "
                         " The configuration can be saved in the gimprc parameter file.",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Configuration..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_onion_cfg, nreturn_vals,
                         args_onion_cfg, return_vals);

  gimp_install_procedure(GAP_PLUGIN_NAME_ONION_APPLY,
                         "This plugin creates or replaces Onionskin Layer(s)",
                         "This plugin creates or updates Onionskin Layers in the current Videoframe."
                         " Onion Layer(s) usually do show previous (or next) frame(s)"
                         " of the video. At 1.st call  in the current frame."
                         " This Plugin runs NONINTERACTIVE only. It depends on the configuration settings"
                         " made by Video/Onionskin/Config or call of the plugin: "
                         GAP_PLUGIN_NAME_ONION_CFG " "
                         " if no configuration is found, default settings are used",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Create or Replace"),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_onion_std, nreturn_vals,
                         args_onion_std, return_vals);


  gimp_install_procedure(GAP_PLUGIN_NAME_ONION_DEL,
                         "This plugin removes OnionSkin Layer(s)",
                         "This plugin removes Onion Skin Layers from the current Videoframe."
                         " Onion Layer(s) usually do show previous (or next) frame(s)"
                         " of the video.",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Delete"),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_onion_std, nreturn_vals,
                         args_onion_std, return_vals);


  gimp_install_procedure(GAP_PLUGIN_NAME_ONION_VISI,
                         "This plugin toggles visibility of OnionSkin Layer(s)",
                         "This plugin sets visibility for all onionskin Layers in the current Videoframe.",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Toggle Visibility"),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         nargs_onion_visi, nreturn_vals,
                         args_onion_visi, return_vals);
  {
     /* Menu names */
     const char *menupath_image_video_onionskin = N_("<Image>/Video/Onionskin/");
 
     gimp_plugin_menu_register (GAP_PLUGIN_NAME_ONION_CFG, menupath_image_video_onionskin);
     gimp_plugin_menu_register (GAP_PLUGIN_NAME_ONION_APPLY, menupath_image_video_onionskin);
     gimp_plugin_menu_register (GAP_PLUGIN_NAME_ONION_DEL, menupath_image_video_onionskin);
     gimp_plugin_menu_register (GAP_PLUGIN_NAME_ONION_VISI, menupath_image_video_onionskin);
  }
}       /* end query */


/* optional: include files with GUI and some worker procedures here
 * (so we can compile and install using gimptool without any makefile)
 */
#ifdef INCLUDE_ALL_C_FILES
#include "gap_match.c"
#include "gap_lib.c"
#include "gap_onion_gui.c"
#include "gap_onion_worker.c"
#endif

static void
run(const gchar *name
   , gint n_params
   , const GimpParam *param
   , gint *nreturn_vals
   , GimpParam **return_vals)
{
  GapOnionMainGlobalParams *gpp;

  static GimpParam values[1];
  gint32     l_rc;
  gint32     l_lock_image_id;
  char       *l_env;

  gpp = &global_params;
  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_SUCCESS;

  l_env = (char*) g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }

  if(gap_debug) fprintf(stderr, "\n\ngap_onion_main: debug name = %s\n", name);

  gpp->run_mode = param[0].data.d_int32;

  INIT_I18N();


  gap_onion_dlg_init_default_values(gpp); /* init with default values */


  /* get image_ID */
  gpp->image_ID    = param[1].data.d_image;
  l_lock_image_id  = gpp->image_ID;

  /* ---------------------------
   * check for LOCKS
   * ---------------------------
   */
  if(gap_lock_check_for_lock(l_lock_image_id, gpp->run_mode))
  {
       values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
       return ;
  }

  /* set LOCK on current image (for all gap_plugins) */
  gap_lock_set_lock(l_lock_image_id);


  /* get animinfo */
  gap_onion_worker_plug_in_gap_get_animinfo(gpp->image_ID, &gpp->ainfo);
  gap_onion_worker_get_data_onion_cfg(gpp);  /* get current params (if there are any) */

  gpp->vin.onionskin_auto_enable = TRUE;
  gpp->cache.count = 0;    /* start with empty image cache */
  gpp->image_ID    = param[1].data.d_image;

  gpp->range_from            = gpp->ainfo.curr_frame_nr;
  gpp->range_to              = gpp->ainfo.last_frame_nr;

  if (strcmp (name, GAP_PLUGIN_NAME_ONION_CFG) == 0)
  {
    /* -----------------------
     * CONFIG
     * -----------------------
     */
      if (gpp->run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != nargs_onion_cfg)
        {
          values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
        }
        else
        {
          gpp->vin.num_olayers        = param[3].data.d_int32;
          gpp->vin.ref_delta          = param[4].data.d_int32;
          gpp->vin.ref_cycle          = param[5].data.d_int32;
          gpp->vin.stack_pos          = param[6].data.d_int32;
          gpp->vin.stack_top          = param[7].data.d_int32;
          gpp->vin.opacity            = param[8].data.d_float;
          gpp->vin.opacity_delta      = param[9].data.d_float;
          gpp->vin.ignore_botlayers   = param[10].data.d_int32;
          gpp->vin.select_mode        = param[11].data.d_int32;
          gpp->vin.select_case        = param[12].data.d_int32;
          gpp->vin.select_invert      = param[13].data.d_int32;
          if (param[14].data.d_string != NULL)
          {
            g_snprintf(&gpp->vin.select_string[0]
                      , sizeof(gpp->vin.select_string)
                      , "%s", param[14].data.d_string
                      );
          }
          gpp->range_from            = param[15].data.d_int32;
          gpp->range_to              = param[16].data.d_int32;
          gpp->run                   = param[17].data.d_int32;
          gpp->vin.asc_opacity       = param[18].data.d_int32;
          gpp->vin.auto_replace_after_load = param[19].data.d_int32;
          gpp->vin.auto_delete_before_save = param[20].data.d_int32;
          gpp->vin.onionskin_auto_enable   = TRUE;
          gpp->vin.ref_mode          = param[21].data.d_int32;
        }
      }
      else if(gpp->run_mode != GIMP_RUN_INTERACTIVE)
      {
         values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
      }

      if (values[0].data.d_status == GIMP_PDB_SUCCESS)
      {
         if(gpp->run_mode == GIMP_RUN_INTERACTIVE)
         {
            l_rc = gap_onion_dlg_onion_cfg_dialog (gpp);
            if(gap_debug) printf("MAIN after gap_onion_dlg_onion_cfg_dialog ------------------\n");
         }
         if (l_rc >= 0)
         {
           if(gpp->run != GAP_ONION_RUN_CANCEL)
           {
             /* disable both automatic onionskin triggers by disabling 
              * the master switch
              * while applying onionskin to a range of frames.
              * (this prevents from creating onionskins twice per image.
              *  automatic delete is also not done in that case because it makes no
              *  sense when the user explicitly wants to create onionskin layers
              *  in the processed range)
              */
             gpp->vin.onionskin_auto_enable   = FALSE;
             l_rc = gap_onion_worker_set_data_onion_cfg(gpp, GAP_PLUGIN_NAME_ONION_CFG);
           }
           if((gpp->run == GAP_ONION_RUN_APPLY)
           || (gpp->run == GAP_ONION_RUN_DELETE))
           {
             /* do ONIONSKIN processing for all the frames in selected Range */
             l_rc = gap_onion_worker_onion_range(gpp);
             
             gpp->vin.onionskin_auto_enable   = TRUE;
             l_rc = gap_onion_worker_set_data_onion_cfg(gpp, GAP_PLUGIN_NAME_ONION_CFG);
           }
         }
      }
  }
  else if (strcmp (name, GAP_PLUGIN_NAME_ONION_APPLY) == 0)
  {
    /* -----------------------
     * MAKE
     * -----------------------
     * store params also with name GAP_PLUGIN_NAME_ONION_APPLY
     * This makes it possible, to call this plugin as filter
     * for a selected range of frames.
     * (using plug_in_gap_modify  and selecting plug_in_onionskin_make as filter)
     */
     gap_onion_worker_set_data_onion_cfg(gpp, GAP_PLUGIN_NAME_ONION_APPLY);
     l_rc = gap_onion_worker_onion_apply(gpp, FALSE /* do not use_cache */ );
  }
  else if (strcmp (name, GAP_PLUGIN_NAME_ONION_DEL) == 0)
  {
    /* -----------------------
     * DEL
     * -----------------------
     */
     gap_onion_worker_set_data_onion_cfg(gpp, GAP_PLUGIN_NAME_ONION_DEL);
     l_rc = gap_onion_worker_onion_delete(gpp);
  }
  else if (strcmp (name, GAP_PLUGIN_NAME_ONION_VISI) == 0)
  {
    /* -----------------------
     * VISI
     * -----------------------
     */
     gint32  l_visi_mode;

     l_visi_mode = GAP_ONION_VISI_TOGGLE;  /* interactive mode always uses toggle */

     if (gpp->run_mode == GIMP_RUN_NONINTERACTIVE)
     {
        if (n_params != nargs_onion_visi)
        {
          values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
        }
        else
        {
          l_visi_mode  = param[3].data.d_int32;
        }
     }

     if (values[0].data.d_status == GIMP_PDB_SUCCESS)
     {
        gap_onion_worker_set_data_onion_cfg(gpp, GAP_PLUGIN_NAME_ONION_VISI);
        l_rc = gap_onion_worker_onion_visibility(gpp, l_visi_mode);
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

  if (gpp->run_mode != GIMP_RUN_NONINTERACTIVE)
  {
    gimp_displays_flush();
  }

  /* remove LOCK on this image for all gap_plugins */
  gap_lock_remove_lock(l_lock_image_id);
}       /* end run */
