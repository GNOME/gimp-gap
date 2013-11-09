/* gap_filter_main.c
 * 1997.12.18 hof (Wolfgang Hofer)
 *
 * GAP ... Gimp Animation Plugins
 *
 * This Module contains:
 * - MAIN of GAP_filter  foreach: call any Filter (==Plugin Proc)
 *                                with varying settings for all
 *                                layers within one Image.
 * - query   registration of gap_foreach Procedure
 *                        and for the COMMON Iterator Procedures
 *                        and for all Iterator_ALT Procedures
 * - run     invoke the gap_foreach procedure by its PDB name
 * 
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
#include "gap_lastvaldesc.h"
#include "gap_filter.h"
#include "gap_filter_iterators.h"
#include "gap_dbbrowser_utils.h"
#include "gap_accel_char.h"

/* revision history:
 * gimp   1.3.20b;  2003/09/20  hof: update version, minor cleanup
 * gimp   1.3.12a;  2003/05/02  hof: merge into CVS-gimp-gap project
 * gimp   1.3.4b;   2002/03/24  hof: support COMMON_ITERATOR, removed support of iter_ALT Procedures
 * 2000/11/30 v1.1.29b:  hof: new e-mail adress
 * version 0.92.00              hof: set gap_debug from environment 
 * version 0.91.01; Tue Dec 23  hof: 1.st (pre) release
 */

/* ------------------------
 * global gap DEBUG switch
 * ------------------------
 */

/* int gap_debug = 1; */    /* print debug infos */
/* int gap_debug = 0; */    /* 0: dont print debug infos */

int gap_debug = 0;

#define PLUG_IN_NAME_ANIMFILTER "plug_in_gap_layers_run_animfilter"

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
  static GimpParamDef args_foreach[] =
  {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable (unused)"},
    {GIMP_PDB_STRING, "proc_name", "name of plugin procedure to run for each layer"},
    {GIMP_PDB_INT32, "acceleration", "0 .. apply constant, 1..apply varying constant speed, positive accelerate, nagative decelerate"},
  };

  static GimpParamDef *return_vals = NULL;
  static int nreturn_vals = 0;

  static GimpParamDef args_com_iter[] =
  {
    {GIMP_PDB_INT32, "run_mode", "non-interactive"},
    {GIMP_PDB_INT32, "total_steps", "total number of steps (# of layers-1 to apply the related plug-in)"},
    {GIMP_PDB_FLOAT, "current_step", "current (for linear iterations this is the layerstack position, otherwise some value inbetween)"},
    {GIMP_PDB_INT32, "len_struct", "length of stored data structure with id is equal to the plug_in  proc_name"},
    {GIMP_PDB_STRING, "plugin_name", "name of the plugin (used as keyname to access LAST_VALUES buffer)"},
  };


  gimp_plugin_domain_register (GETTEXT_PACKAGE, LOCALEDIR);

  gimp_install_procedure(PLUG_IN_NAME_ANIMFILTER,
                         "This plugin calls another plugin for each layer of an image, "
                         "optional varying its settings (to produce animated effects). "
                         "The called plugin must work on a single drawable and must be "
                         "able to run in runmode GIMP_RUN_WITH_LAST_VALS and using gimp_set_data "
                         "to store its parameters for this session with its own name as access key. "
                         "plug_in_gap_layers_run_animfilter runs as wizzard (using more dialog steps). "
                         "In Interactive runmode it starts with with a browser dialog where the name of the "
                         "other plug-in (that is to execute) can be selected."
                         "In non-interactive run mode this first browser dialog step is skiped. "
                         "But the selceted plug-in (in this case via parameter plugin_name) is called in "
                         "interactive runmode one time or two times if varying parameter is not 0. "
                         "Those interactive calls are done regardless what runmode is specified here.",
                         "",
                         "Wolfgang Hofer (hof@gimp.org)",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         N_("Filter all Layers..."),
                         "RGB*, INDEXED*, GRAY*",
                         GIMP_PLUGIN,
                         G_N_ELEMENTS (args_foreach), nreturn_vals,
                         args_foreach, return_vals);

  /* ------------------ Common Iterator ------------------------------ */

  gimp_install_procedure(GIMP_PLUGIN_GAP_COMMON_ITER,
                         "This procedure calculates the modified values in the LAST_VALUES buffer named by plugin_name for one iterationstep",
                         "",
                         "Wolfgang Hofer",
                         "Wolfgang Hofer",
                         GAP_VERSION_WITH_DATE,
                         NULL,    /* do not appear in menus */
                         NULL,
                         GIMP_PLUGIN,
                         G_N_ELEMENTS (args_com_iter), nreturn_vals,
                         args_com_iter, return_vals);

  /* ------------------ ALTernative Iterators ------------------------------ */

  gimp_plugin_menu_register (PLUG_IN_NAME_ANIMFILTER, N_("<Image>/Filters/"));

  gap_query_iterators_ALT();
                         
}       /* end query */


static void
run(const gchar *name
   , gint n_params
   , const GimpParam *param
   , gint *nreturn_vals
   , GimpParam **return_vals)
{
#define  MAX_PLUGIN_NAME_LEN  256

  char l_plugin_name[MAX_PLUGIN_NAME_LEN];
  static GimpParam values[1];
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  gint32     image_id;
  gint32  len_struct;
  gint32  total_steps;
  gdouble current_step;
  
  gint32      l_rc;
  const char *l_env;

  *nreturn_vals = 1;
  *return_vals = values;
  l_rc = 0;

  l_env = g_getenv("GAP_DEBUG");
  if(l_env != NULL)
  {
    if((*l_env != 'n') && (*l_env != 'N')) gap_debug = 1;
  }


  run_mode = param[0].data.d_int32;

  INIT_I18N ();


  if(gap_debug) fprintf(stderr, "\n\ngap_filter_main: debug name = %s\n", name);
  
  if (strcmp (name, PLUG_IN_NAME_ANIMFILTER) == 0)
  {
      gint32 accelCharacteristic;

      accelCharacteristic = GAP_ACCEL_CHAR_NONE;
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
      {
        if (n_params != 5)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
        else
        {
          strncpy(l_plugin_name, param[3].data.d_string, MAX_PLUGIN_NAME_LEN -1);
          l_plugin_name[MAX_PLUGIN_NAME_LEN -1] = '\0';
        }
        if( param[4].data.d_int32 != 0)
        {
          accelCharacteristic = param[4].data.d_int32;
        }
      }
      else if(run_mode == GIMP_RUN_WITH_LAST_VALS)
      {
        /* probably get last values (name of last plugin) */
        gimp_get_data(PLUG_IN_NAME_ANIMFILTER, l_plugin_name);
      }

      if (status == GIMP_PDB_SUCCESS)
      {

        image_id    = param[1].data.d_image;

        l_rc = gap_proc_anim_apply(run_mode, image_id, l_plugin_name, accelCharacteristic);
        gimp_set_data(PLUG_IN_NAME_ANIMFILTER,
                      l_plugin_name, sizeof(l_plugin_name));
      }
  }
  else if(strcmp (name, GIMP_PLUGIN_GAP_COMMON_ITER) == 0)
  {
      if ((run_mode == GIMP_RUN_NONINTERACTIVE) && (n_params == 5))
      {
        total_steps  =  param[1].data.d_int32;
        current_step =  param[2].data.d_float;
        len_struct   =  param[3].data.d_int32;
        l_rc = gap_common_iterator(param[4].data.d_string, run_mode, total_steps, current_step, len_struct);
      }
      else status = GIMP_PDB_CALLING_ERROR;
  }
  else
  {
      if ((run_mode == GIMP_RUN_NONINTERACTIVE) && (n_params == 4))
      {
        total_steps  =  param[1].data.d_int32;
        current_step =  param[2].data.d_float;
        len_struct   =  param[3].data.d_int32;
        l_rc =  gap_run_iterators_ALT(name,
                                      run_mode,
                                      total_steps, current_step, len_struct);
      }
      else
      {
        status = GIMP_PDB_CALLING_ERROR;
      }
  }

 if(l_rc < 0)
 {
    status = GIMP_PDB_EXECUTION_ERROR;
 }
 
  
 if (run_mode != GIMP_RUN_NONINTERACTIVE)
    gimp_displays_flush();

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

}
